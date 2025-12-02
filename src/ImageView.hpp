#pragma once

#include <cpp/Color.hpp>
#include "IndexedColor.hpp"

#include <tuple>
#include <vector>
#include <string>
#include <memory>

class ImageView
{
public:
  virtual ~ImageView() = 0;
  int width;
  int height;
protected:
  ImageView(int width, int height)
        : width{width}
        , height{height} {}
};

ImageView::~ImageView() {}

class RGBImageView : public ImageView
{
public:
  virtual ~RGBImageView() = 0;
  virtual RGBColor getPixel(int x, int y) const = 0;
  virtual void setPixel(int x, int y, const RGBColor& color) = 0;
protected:
  RGBImageView(int width, int height)
    : ImageView{width, height} {}
};

RGBImageView::~RGBImageView() {}

class RGBImage : public RGBImageView
{
public: 
  RGBImage(int width, int height)
        : RGBImageView{width, height}
        , data_((size_t)(width*height)) {}

  virtual ~RGBImage() = default;
  virtual RGBColor getPixel(int x, int y) const override
  {
    return data_[x+y*width];
  }

  virtual void setPixel(int x, int y, const RGBColor& color) override
  {
    data_[x+y*width] = color;
  }

  RGBColor* getPixelData(int x, int y)
  {
    return &data_[x+y*width];
  }

  const RGBColor* getPixelData(int x, int y) const
  {
    return &data_[x+y*width];
  }
private:
    std::vector<RGBColor> data_;
};

class IndexedImageView : public ImageView
{
public:
  virtual ~IndexedImageView() = 0;
  virtual IndexedColor getPixel(int x, int y) const = 0;
  virtual void setPixel(int x, int y, const IndexedColor& color) = 0;
  const IndexedColorMap& colorMap() { return *(colorMap_.get()); }
protected:
  IndexedImageView(int width, int height, std::shared_ptr<const IndexedColorMap> colorMap)
    : ImageView{width, height}
    , colorMap_{std::move(colorMap)} {}
  std::shared_ptr<const IndexedColorMap> colorMap_;
};

IndexedImageView::~IndexedImageView() {}

class IndexedImage : public IndexedImageView
{
public:
  // Construct an indexed image with the specified size, allocating memory.
  IndexedImage(int width, int height, std::shared_ptr<const IndexedColorMap> colorMap)
    : IndexedImageView{width, height, std::move(colorMap)}
    , data_((size_t)(width*height)) { }

  virtual ~IndexedImage() = default;

  virtual IndexedColor getPixel(int x, int y) const override
  {
    return data_[x+y*width];
  }

  virtual void setPixel(int x, int y, const IndexedColor& color) override
  {
    data_[x+y*width] = color;
  }

  IndexedColor* getPixelData(int x, int y)
  {
    return &data_[x+y*width];
  }

  const IndexedColor* getPixelData(int x, int y) const
  {
    return &data_[x+y*width];
  }
private:
  std::vector<IndexedColor> data_;
};

// Used by 7 Color Inky displays
class Packed4BitIndexedImage : public IndexedImageView
{
public:
  // Construct an indexed image with the specified size, allocating memory.
  Packed4BitIndexedImage(int width, int height, std::shared_ptr<const IndexedColorMap> colorMap)
    : IndexedImageView{width, height, std::move(colorMap)}
    , data_ ((size_t)(width*height/2)) {}

  virtual ~Packed4BitIndexedImage() = default;
  virtual IndexedColor getPixel(int x, int y) const override
  {
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
    int index = (y*width+x) / 2;
    return &data_[index];
  }

  const uint8_t* getPixelData(int x, int y) const
  {
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

// Wrap an indexed image to access it as RGB
class RGBToIndexedImageView : public RGBImageView
{
public:
  RGBToIndexedImageView(std::shared_ptr<IndexedImageView> indexed, bool diffusion)
    : RGBImageView(indexed->width, indexed->height)
    , indexed_{std::move(indexed)}
    , diffusion_{diffusion} { }
  virtual ~RGBToIndexedImageView() = default;
  virtual RGBColor getPixel(int x, int y) const override
  {
    return indexed_->colorMap().toRGBColor(indexed_->getPixel(x,y));
  }

  virtual void setPixel(int x, int y, const RGBColor& color) override
  {
    if (diffusion_)
    {
      // TBD
    }
    else
    {
      // Use closest color, discard error
      indexed_->setPixel(x,y, indexed_->colorMap().toIndexedColor(color));
    }
  }

  // Reset the accumulated diffusion error to 0
  void resetDiffusionError()
  {
    // TBD
  }

private:
  std::shared_ptr<IndexedImageView> indexed_;
  bool diffusion_;
};

// Used by Black/White/Red and Black/White/Yellow Inky displays
class PackedTwoPlaneBinaryImage : public ImageView
{
public:
  enum class Plane
  {
    Black,
    Color
  };

  PackedTwoPlaneBinaryImage(int width, int height)
    : ImageView(width, height)
    , bPlane_((size_t)((width*height)/8 + (((width*height)%8 != 0) ? 1 : 0)))
    , cPlane_((size_t)((width*height)/8 + (((width*height)%8 != 0) ? 1 : 0)))
  { }
  
  virtual ~PackedTwoPlaneBinaryImage() = default;

  std::pair<bool, bool> getPixel(int x, int y) const
  {
    int index = (y*width+x) / 8;
    int offset = (y*width+x) % 8;
    return {
      (bPlane_[index] & (0b10000000 >> offset)) != 0,
      (cPlane_[index] & (0b10000000 >> offset)) != 0
    };
  }

  void setPixel(int x, int y, std::pair<bool, bool> value)
  {
    int index = (y*width+x) / 8;
    int offset = (y*width+x) % 8;

    if (value.first)
    {
      bPlane_[index] = (bPlane_[index] | (0b10000000 >> offset));
    }
    else
    {
      bPlane_[index] = (bPlane_[index] & ~(0b10000000 >> offset));
    }

    if (value.second)
    {
      cPlane_[index] = (cPlane_[index] | (0b10000000 >> offset));
    }
    else
    {
      cPlane_[index] = (cPlane_[index] & ~(0b10000000 >> offset));
    }
  }

  uint8_t* getPixelData(int x, int y, Plane p)
  {
    int index = (y*width+x) / 8;
    if (p == Plane::Black)
      return &bPlane_[index];
    else
      return &cPlane_[index];
  }

  const uint8_t* getPixelData(int x, int y, Plane p) const
  {
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
  std::vector<uint8_t> bPlane_;
  std::vector<uint8_t> cPlane_;
};

// Wrap an indexed image to access it as RGB
class IndexedToPackedTwoPlaneBinaryImageView : public IndexedImageView
{
public:
  IndexedToPackedTwoPlaneBinaryImageView(
      std::shared_ptr<PackedTwoPlaneBinaryImage> twoPlane, 
      std::shared_ptr<const IndexedColorMap> colorMap, 
      IndexedColor colorNone, 
      IndexedColor colorB,
      IndexedColor colorC,
      IndexedColor colorBoth
    )
    : IndexedImageView(twoPlane->width, twoPlane->height, std::move(colorMap))
    , twoPlane_{std::move(twoPlane)}
    , colorNone_{colorNone}
    , colorB_{colorB}
    , colorC_{colorC}
    , colorBoth_{colorBoth} { }
  
  virtual ~IndexedToPackedTwoPlaneBinaryImageView() = default;

  virtual IndexedColor getPixel(int x, int y) const override
  {
    auto px = twoPlane_->getPixel(x,y);
    if (px.first && px.second)
    {
      return colorBoth_;
    }
    else if (px.first && !px.second)
    {
      return colorB_;
    }
    else if (!px.first && px.second)
    {
      return colorC_;
    }
    return colorNone_;
  }

  virtual void setPixel(int x, int y, const IndexedColor& color) override
  {
    if (color == colorBoth_)
    {
      return twoPlane_->setPixel(x,y,{true, true});
    }
    else if (color == colorB_)
    {
      return twoPlane_->setPixel(x,y,{true, false});
    }
    else if (color == colorC_)
    {
      return twoPlane_->setPixel(x,y,{false, true});
    }
    return twoPlane_->setPixel(x,y,{false, false});
  }

private:
  std::shared_ptr<PackedTwoPlaneBinaryImage> twoPlane_;
  IndexedColor colorNone_, colorB_, colorC_, colorBoth_;
};
