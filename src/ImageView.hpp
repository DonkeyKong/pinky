#pragma once

// An abstract class for interacting with images. Reading and writing
// pixel data is allowed, though writes may be cached.
template <typename PixelTypeT>
class ImageView
{
public:
  using PixelType = PixelTypeT;
  virtual ~ImageView()
  {
    flush();
  }
  const int width;
  const int height;

  // Get a pixel from the image.
  virtual PixelType getPixel(int x, int y) const = 0;

  // Write a pixel to the image.
  // A call to flush() is required to ensure pixel writes
  // are actually applied as images may cache writes to
  // perform operations more efficiently.
  virtual void setPixel(int x, int y, const PixelType& color) = 0;

  // Ensure all pixels set are flushed to underlying storage
  // and any internal memory buffers are cleared.
  virtual void flush() {}
protected:
  ImageView(int width, int height)
        : width{width}
        , height{height} {}
};
