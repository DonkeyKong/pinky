#pragma once

#include "ImageView.hpp"

#include <cpp/Color.hpp>
#include <cpp/Logging.hpp>

#include <Arducam_Mega.h>
#include <picojpeg.h>

#include <functional>
#include <iostream>

// Update the rest of the app on progress using a float, 0.0f - 1.0f
using ProgressUpdateCallback = std::function<void(float)>;
using McuCopyFunc = void(*)(unsigned char* r, unsigned char* g, unsigned char* b, RGBColor* dest, int stride);

bool decodeImageRGB565(int width, int height, Arducam_Mega& cam, RGBImageView& buffer, ProgressUpdateCallback progressCb = nullptr)
{
  if (cam.getReceivedLength() != (width * height * 2))
  {
    std::cout << "Bad image size! Got " << cam.getReceivedLength() << " bytes, expected " << (width * height * 2) << " bytes" << std::endl;
    return false;
  }

  uint16_t rgb565[width];
  for (int y=0; y < height; ++y)
  {
    // Collect one line of the image
    int readX = 0;
    while (readX < width)
    {
      uint8_t bytesRead = cam.readBuff((uint8_t*)(&rgb565[readX]), std::min(127, width-readX) * 2);
      readX += (bytesRead / 2);
    }

    // Write the line to the eInk display
    if (y < buffer.height)
    {
      RGBColor rgb;
      for (int x=0; x < std::min(width, buffer.width); ++x)
      {
        rgb = RGBColor::fromRGB565(rgb565[x]);
        buffer.setPixel(x, y, rgb);
      }
      if (progressCb)
      {
        progressCb((float)y / (float) buffer.height);
      }
    }
  }

  return true;
}

bool decodeImageYUYV(int width, int height, Arducam_Mega& cam, RGBImageView& buffer, ProgressUpdateCallback progressCb = nullptr)
{
  if (cam.getReceivedLength() != (width * height * 2))
  {
    std::cout << "Bad image size! Got " << cam.getReceivedLength() << " bytes, expected " << (width * height * 2) << " bytes" << std::endl;
    return false;
  }

  int widthBytes = width * 2;
  int writeWidth = std::min(width, buffer.width);
  uint8_t yuyv[widthBytes];
  
  for (int y=0; y < height; ++y)
  {
    // Collect one line of the image into line0
    int readX = 0;
    while (readX < widthBytes)
    {
      readX += cam.readBuff(yuyv + readX, std::min(255, widthBytes-readX));
    }

    if (y < buffer.height)
    {
      for (int x=0; x < writeWidth; ++x)
      {
        if (x*2+3 < widthBytes)
        {
          if (x%2==0)
          {
            buffer.setPixel(x, y, YUVColor {
              yuyv[x*2],
              yuyv[x*2+1],
              yuyv[x*2+3],
            }.toRGB());
          }
          else
          {
            buffer.setPixel(x, y, YUVColor {
              yuyv[x*2],
              yuyv[x*2+3],
              yuyv[x*2+1],
            }.toRGB());
          }
        }
      }
    }

    // Give a progress update
    if (progressCb)
    {
      progressCb((float)y / (float) buffer.height);
    }
  }

  return true;
}

bool decodeImageYUV(int width, int height, Arducam_Mega& cam, RGBImageView& buffer, ProgressUpdateCallback progressCb = nullptr)
{
  if (cam.getReceivedLength() != (width * height * 2))
  {
    std::cout << "Bad image size! Got " << cam.getReceivedLength() << " bytes, expected " << (width * height * 2) << " bytes" << std::endl;
    return false;
  }

  int widthBytes = width * 2;
  int writeWidth = std::min(width, buffer.width);
  uint8_t yuv16a[widthBytes];
  uint8_t yuv16b[widthBytes];
  uint8_t yuv16c[widthBytes];
  uint8_t* line2 = yuv16c;
  uint8_t* line1 = yuv16b;
  uint8_t* line0 = yuv16a;
  bool uLine = true;
  
  for (int y=0; y < height; ++y)
  {
    // Collect one line of the image into line0
    int readX = 0;
    while (readX < widthBytes)
    {
      readX += cam.readBuff(line0 + readX, std::min(255, widthBytes-readX));
    }

    // If this is the second line, write out line1,
    // interpolating only with line0
    if (y == 1 && (y-1) < buffer.height)
    {
      for (int x=0; x < writeWidth; ++x)
      {
        buffer.setPixel(x, y-1, YUVColor {
          line1[x*2],
          line1[x*2+1],
          line0[x*2+1]
        }.toRGB());
      }
    }
    // If this is the third or greater line, write out line1
    // interpolating it with lines 0 and 2
    else if (y > 1 && (y-1) < buffer.height)
    {
      if (uLine)
      {
        for (int x=0; x < writeWidth; ++x)
        {
          buffer.setPixel(x, y-1, YUVColor {
            line1[x*2],
            (uint8_t)std::clamp(((int)line0[x*2+1] + (int)line2[x*2+1]) / 2, 0, 255),
            line1[x*2+1]
          }.toRGB());
        }
      }
      else
      {
        for (int x=0; x < writeWidth; ++x)
        {
          buffer.setPixel(x, y-1, YUVColor {
            line1[x*2],
            line1[x*2+1],
            (uint8_t)std::clamp(((int)line0[x*2+1] + (int)line2[x*2+1]) / 2, 0, 255),
          }.toRGB());
        }
      }
    }
    
    // If this was the last line, write out line0,
    // interpolating only with line1
    if (y == height-1 && y < buffer.height)
    {
      if (uLine)
      {
        for (int x=0; x < writeWidth; ++x)
        {
          buffer.setPixel(x, y, YUVColor {
            line0[x*2],
            line0[x*2+1],
            line1[x*2+1]
          }.toRGB());
        }
      }
      else
      {
        for (int x=0; x < writeWidth; ++x)
        {
          buffer.setPixel(x, y, YUVColor {
            line0[x*2],
            line1[x*2+1],
            line0[x*2+1]
          }.toRGB());
        }
      }
    }

    // Give a progress update
    if (progressCb)
    {
      progressCb((float)y / (float) buffer.height);
    }

    // mark the next line as a V line
    uLine = !uLine;

    // shuffle the line buffers
    std::swap(line0, line2);
    std::swap(line1, line2);
  }

  return true;
}

void copyMcuDataGreyscale(unsigned char* r, unsigned char* /*g*/, unsigned char* /*b*/, RGBColor* dest, int stride)
{
  for (int j=0; j < 8; ++j)
  {
    for (int i=0; i < 8; ++i)
    {
      dest[i] = {*r, *r, *r};
      r+=1;
    }
    dest += stride;
  }
}

void copyMcuDataH1V1(unsigned char* r, unsigned char* g, unsigned char* b, RGBColor* dest, int stride)
{
  for (int j=0; j < 8; ++j)
  {
    for (int i=0; i < 8; ++i)
    {
      dest[i] = {*r, *g, *b};
      r+=1; g+=1; b+=1;
    }
    dest += stride;
  }
}

void copyMcuDataH2V1(unsigned char* r, unsigned char* g, unsigned char* b, RGBColor* dest, int stride)
{
  copyMcuDataH1V1(r, g, b, dest, stride);
  copyMcuDataH1V1(r+64, g+64, b+64, dest+8, stride);
}

void copyMcuDataH1V2(unsigned char* r, unsigned char* g, unsigned char* b, RGBColor* dest, int stride)
{
  copyMcuDataH1V1(r, g, b, dest, stride);
  copyMcuDataH1V1(r+128, g+128, b+128, dest+(8*stride), stride);
}

void copyMcuDataH2V2(unsigned char* r, unsigned char* g, unsigned char* b, RGBColor* dest, int stride)
{
  copyMcuDataH1V1(r, g, b, dest, stride);
  copyMcuDataH1V1(r+64, g+64, b+64, dest+8, stride);
  copyMcuDataH1V1(r+128, g+128, b+128, dest+(8*stride), stride);
  copyMcuDataH1V1(r+192, g+192, b+192, dest+(8*stride)+8, stride);
}

bool decodeImageJPG(int width, int height, Arducam_Mega& cam, RGBImageView& buffer, ProgressUpdateCallback progressCb = nullptr)
{
  DEBUG_LOG("Snapped JPG formatted iamge with size " << cam.getReceivedLength() << " bytes");
  pjpeg_image_info_t info;
  pjpeg_need_bytes_callback_t getCamBytes = [](unsigned char* pBuf, unsigned char buf_size, unsigned char *pBytes_actually_read, void* camPtr)
  {
    Arducam_Mega& cam = *(Arducam_Mega*)camPtr;
    uint8_t bytesRead = cam.readBuff(pBuf, buf_size);
    *pBytes_actually_read = bytesRead;
    return (unsigned char)(bytesRead > 0 ? 0 : 1);
  };
  pjpeg_decode_init(&info, getCamBytes, &cam, 0);

  McuCopyFunc copyFunc;
  switch (info.m_scanType)
  {
    case pjpeg_scan_type_t::PJPG_GRAYSCALE:
      copyFunc = copyMcuDataGreyscale;
      break;
    case pjpeg_scan_type_t::PJPG_YH1V1:
      copyFunc = copyMcuDataH1V1;
      break;
    case pjpeg_scan_type_t::PJPG_YH2V1:
      copyFunc = copyMcuDataH2V1;
      break;
    case pjpeg_scan_type_t::PJPG_YH1V2:
      copyFunc = copyMcuDataH1V2;
      break;
    case pjpeg_scan_type_t::PJPG_YH2V2:
      copyFunc = copyMcuDataH2V2;
      break;
    default:
      DEBUG_LOG("Bad scan type: " << (int)info.m_scanType);
      return false;
  }
  DEBUG_LOG("Scan type: " << (int)info.m_scanType);

  // We will need buffer [MCUHeight] lines of RGB pixels of decoded data
  // so that the dither code can operate line-wise, the way it likes
  int decodeWidth = info.m_MCUWidth*info.m_MCUSPerRow;
  RGBColor decodeBuffer[decodeWidth*info.m_MCUHeight];
  for (int mcuY = 0; mcuY < info.m_MCUSPerCol; ++mcuY)
  {
    for (int mcuX = 0; mcuX < info.m_MCUSPerRow; ++mcuX)
    {
      // If something has gone wrong in the loop where we are pretty sure we
      // should have data to decode, bail with a failure flag.
      unsigned char res = pjpeg_decode_mcu();
      if (res != 0)
      {
        DEBUG_LOG("JPEG decode error: " << res);
        return false;
      }

      // Copy the data to decodeBuffer
      copyFunc(info.m_pMCUBufR, info.m_pMCUBufG, info.m_pMCUBufB, decodeBuffer+(mcuX*info.m_MCUWidth), decodeWidth);
    }

    // Now that we have [MCUHeight] full lines, iterate over them
    int y = mcuY * info.m_MCUHeight;
    RGBColor* decodeLine = decodeBuffer;
    for (int i=0; i < info.m_MCUHeight; ++i)
    {
      if (y < buffer.height)
      {
        for (int x=0; x < std::min(decodeWidth, buffer.width); ++x)
        {
          buffer.setPixel(x, y, decodeLine[x]);
        }
      }
      y += 1;
      decodeLine += decodeWidth;
    }

    // Report progress
    if (progressCb)
    {
      progressCb((float)mcuY / (float)info.m_MCUSPerCol);
    }
  }

  return true;
}
