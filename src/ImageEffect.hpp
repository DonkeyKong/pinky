#pragma once

#include "ImageView.hpp"

template <typename ImageViewT>
// Centers a source image over a destination
class AlignCenterView : public ImageView<typename ImageViewT::PixelType>
{
public: 
  AlignCenterView(ImageViewT& destination, int srcWidth, int srcHeight)
        : ImageView<typename ImageViewT::PixelType>{srcWidth, srcHeight}
        , destination_{destination}
        , dx_{(destination_.width - srcWidth) / 2}
        , dy_{(destination_.height - srcHeight) / 2}
    {}

  virtual ~AlignCenterView() = default;

  virtual typename ImageViewT::PixelType getPixel(int x, int y) const override
  {
    return destination_.getPixel(x+dx_, y+dy_);
  }

  virtual void setPixel(int x, int y, const typename ImageViewT::PixelType& color) override
  {
    destination_.setPixel(x+dx_, y+dy_, color);
  }
private:
  ImageViewT& destination_;
  int dx_;
  int dy_;
};