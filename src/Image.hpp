#pragma once

#include "ImageView.hpp"
#include "IndexedColor.hpp"

#include <vector>
#include <tuple>

template <typename PixelTypeT>
// Image type that stores image data serially in a std::vector
class Image : public ImageView<PixelTypeT>
{
public: 
  Image(int width, int height)
        : ImageView<PixelTypeT>{width, height}
        , data_((size_t)(width*height)) {}

  virtual ~Image() = default;

  virtual PixelTypeT getPixel(int x, int y) const override
  {
    if (x < 0 || x >= this->width || y < 0 || y >= this->height) return PixelTypeT();
    return data_[x+y*this->width];
  }

  virtual void setPixel(int x, int y, const PixelTypeT& color) override
  {
    if (x < 0 || x >= this->width || y < 0 || y >= this->height) return;
    data_[x+y*this->width] = color;
  }

  PixelTypeT* getPixelData(int x, int y)
  {
    if (x < 0 || x >= this->width || y < 0 || y >= this->height) return nullptr;
    return &data_[x+y*this->width];
  }

  const PixelTypeT* getPixelData(int x, int y) const
  {
    if (x < 0 || x >= this->width || y < 0 || y >= this->height) return nullptr;
    return &data_[x+y*this->width];
  }
private:
    std::vector<PixelTypeT> data_;
};

// Image type that stores 4 bit IndexedColors, packed 2 pixels per byte
class Packed4BitIndexedImage : public ImageView<IndexedColor>
{
  // This class is used internally by inky impression devices
  // Both 7 color and Spectra 6 variants
public:
  // Construct an indexed image with the specified size, allocating memory.
  Packed4BitIndexedImage(int width, int height)
    : ImageView{width, height}
    , data_ ((size_t)(width*height/2)) {}

  virtual ~Packed4BitIndexedImage() = default;
  virtual IndexedColor getPixel(int x, int y) const override
  {
    if (x < 0 || x >= width || y < 0 || y >= height) return 0;

    int index = (y*width+x) / 2;
    int offset = (y*width+x) % 2;

    if (offset == 0)
    {
      return data_[index] >> 4;
    }
    else
    {
      return data_[index] & 0x0F;
    }
  }

  virtual void setPixel(int x, int y, const IndexedColor& color) override
  {
    if (x < 0 || x >= width || y < 0 || y >= height) return;

    int index = (y*width+x) / 2;
    int offset = (y*width+x) % 2;

    if (offset == 0)
    {
      data_[index] = (data_[index] & 0x0F) | (color << 4);
    }
    else
    {
      data_[index] = (data_[index] & 0xF0) | (color & 0x0F);
    }
  }

  uint8_t* getPixelData(int x, int y)
  {
    if (x < 0 || x >= width || y < 0 || y >= height) return nullptr;

    int index = (y*width+x) / 2;
    return &data_[index];
  }

  const uint8_t* getPixelData(int x, int y) const
  {
    if (x < 0 || x >= width || y < 0 || y >= height) return nullptr;

    int index = (y*width+x) / 2;
    return &data_[index];
  }

  std::vector<uint8_t>& getData()
  {
    return data_;
  }

private:
  std::vector<uint8_t> data_;
};

// Image type that stores 2 planes of binary image data, packed 8 pixels per byte
class PackedTwoPlaneBinaryImage : public ImageView<IndexedColor>
{
  // Used by Black/White/Red and Black/White/Yellow Inky displays
public:
  enum class Plane
  {
    Black,
    Color
  };

  PackedTwoPlaneBinaryImage(
      int width,
      int height,
      IndexedColor colorNone,
      IndexedColor colorB,
      IndexedColor colorC,
      IndexedColor colorBoth)
    : ImageView(width, height)
    , colorNone_{colorNone}
    , colorB_{colorB}
    , colorC_{colorC}
    , colorBoth_{colorBoth}
    , bPlane_((size_t)((width*height)/8 + (((width*height)%8 != 0) ? 1 : 0)))
    , cPlane_((size_t)((width*height)/8 + (((width*height)%8 != 0) ? 1 : 0)))
  { }
  
  virtual ~PackedTwoPlaneBinaryImage() = default;

  IndexedColor getPixel(int x, int y) const override
  {
    if (x < 0 || x >= width || y < 0 || y >= height) return colorNone_;

    int index = (y*width+x) / 8;
    int offset = (y*width+x) % 8;

    bool b = (bPlane_[index] & (0b10000000 >> offset)) != 0;
    bool c = (cPlane_[index] & (0b10000000 >> offset)) != 0;

    if (b && c)
    {
      return colorBoth_;
    }
    else if (b && !c)
    {
      return colorB_;
    }
    else if (!b && c)
    {
      return colorC_;
    }
    return colorNone_;
  }

  void setPixel(int x, int y, const IndexedColor& value) override
  {
    if (x < 0 || x >= width || y < 0 || y >= height) return;

    int index = (y*width+x) / 8;
    int offset = (y*width+x) % 8;

    if (value == colorBoth_)
    {
      bPlane_[index] = (bPlane_[index] | (0b10000000 >> offset));
      cPlane_[index] = (cPlane_[index] | (0b10000000 >> offset));
    }
    else if (value == colorB_)
    {
      bPlane_[index] = (bPlane_[index] | (0b10000000 >> offset));
      cPlane_[index] = (cPlane_[index] & ~(0b10000000 >> offset));
    }
    else if (value == colorC_)
    {
      bPlane_[index] = (bPlane_[index] & ~(0b10000000 >> offset));
      cPlane_[index] = (cPlane_[index] | (0b10000000 >> offset));
    }
    else //if (value == colorNone_)
    {
      bPlane_[index] = (bPlane_[index] & ~(0b10000000 >> offset));
      cPlane_[index] = (cPlane_[index] & ~(0b10000000 >> offset));
    }
  }

  uint8_t* getPixelData(int x, int y, Plane p)
  {
    if (x < 0 || x >= width || y < 0 || y >= height) return nullptr;

    int index = (y*width+x) / 8;
    if (p == Plane::Black)
      return &bPlane_[index];
    else
      return &cPlane_[index];
  }

  const uint8_t* getPixelData(int x, int y, Plane p) const
  {
    if (x < 0 || x >= width || y < 0 || y >= height) return nullptr;

    int index = (y*width+x) / 8;
    if (p == Plane::Black)
      return &bPlane_[index];
    else
      return &cPlane_[index];
  }

  std::vector<uint8_t>& getPlane(Plane p)
  {
    if (p == Plane::Black)
      return bPlane_;
    else
      return cPlane_;
  }

private:
  IndexedColor colorNone_;
  IndexedColor colorB_;
  IndexedColor colorC_;
  IndexedColor colorBoth_;
  std::vector<uint8_t> bPlane_;
  std::vector<uint8_t> cPlane_;
};
