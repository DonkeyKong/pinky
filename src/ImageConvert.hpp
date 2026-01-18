#pragma once

#include "ImageView.hpp"
#include "Image.hpp"
#include "IndexedColor.hpp"

#include <cpp/Color.hpp>

class RGBToIndexedImageView : public ImageView<RGBColor>
{
public:
  RGBToIndexedImageView(ImageView<IndexedColor>& indexed, const IndexedColorMap& colorMap)
    : ImageView(indexed.width, indexed.height)
    , indexed_{indexed}
    , colorMap_{colorMap}
  { }

  virtual ~RGBToIndexedImageView() = default;

  virtual RGBColor getPixel(int x, int y) const override
  {
    return colorMap_.toRGBColor(indexed_.getPixel(x,y));
  }

  virtual void setPixel(int x, int y, const RGBColor& color) override
  {
    indexed_.setPixel(x,y, colorMap_.toIndexedColor(color));
  }

protected:
  ImageView<IndexedColor>& indexed_;
  const IndexedColorMap& colorMap_;
};

class LabDitherView : public ImageView<RGBColor>
{
public:
  // Determines how accurate we try to make colors when doing diffusion.
  // Lower values provide more clarity on more limited displays.
  // Sane values: 0.5 - 1.0
  float ditherAccuracy;

  LabDitherView(ImageView<IndexedColor>& indexed, const IndexedColorMap& colorMap)
    : ImageView(indexed.width, indexed.height)
    , indexed_{indexed}
    , colorMap_{colorMap}
    , ditherAccuracy{0.7f}
    , currentDiffusionRow_{-1}
    , thisRowError_((size_t)width)
    , nextRowError_((size_t)width)
  { }

  virtual RGBColor getPixel(int x, int y) const override
  {
    return colorMap_.toRGBColor(indexed_.getPixel(x,y));
  }

  virtual void setPixel(int x, int y, const RGBColor& color) override
  {
    // Because we cache error data here we should protect against bad
    // coordinates at this layer.
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    
    // If diffusion is currently off or y has jumped in a weird way
    // clear both error buffers and set the current y to be the diffusion error row
    if (currentDiffusionRow_ == -1 || y > (currentDiffusionRow_+1) || y < currentDiffusionRow_)
    {
      for (int i=0; i < width; ++i)
      {
        nextRowError_[i] = {0,0,0};
        thisRowError_[i] = {0,0,0};
      }
      currentDiffusionRow_ = y;
    }
    // If y has advanced by one, then swap next and current buffers 
    // and clear the next buffer.
    else if (currentDiffusionRow_+1 == y)
    {
      auto swap = std::move(nextRowError_);
      nextRowError_ = std::move(thisRowError_);
      thisRowError_ = std::move(swap);
      for (int i=0; i < width; ++i)
      {
        nextRowError_[i] = {0,0,0};
      }
      currentDiffusionRow_ = y;
    }

    // Convert the current color to LAB and add the current error,
    // attenuating error slightly as we do so (to ensure error doesn't grow unbounded)
    LabColor current = color.toLab() + (thisRowError_[x] * ditherAccuracy);

    // Convert to nearest indexed color, saving error
    LabColor error;
    IndexedColor nearestIndexed = colorMap_.toIndexedColor(current, error);
    indexed_.setPixel(x,y,nearestIndexed);

    // Diffuse the error into the error buffers
    if (x < width-1)
    {
      thisRowError_[x+1] += error *   (7.0f / 16.0f);
      nextRowError_[x+1] += error * (1.0f / 16.0f);
    }
    if (x > 0)
    {
      nextRowError_[x-1] += error * (3.0f / 16.0f);
    }
    if (y < height-1)
    {
      nextRowError_[x] += error *   (5.0f / 16.0f);
    }
  }

  // Reset the accumulated diffusion error to 0
  void resetDiffusion()
  {
    // This is sufficient to mark diffusion error data as invalid
    currentDiffusionRow_ = -1;
  }

private:
  ImageView<IndexedColor>& indexed_;
  const IndexedColorMap& colorMap_;
  int currentDiffusionRow_;
  std::vector<LabColor> thisRowError_;
  std::vector<LabColor> nextRowError_;
};
