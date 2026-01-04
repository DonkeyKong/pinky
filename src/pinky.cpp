#include "Settings.hpp"
#include "Inky.hpp"
#include "ColorMapEffect.hpp"

#include <cpp/Button.hpp>
#include <cpp/LedStripWs2812b.hpp>
#include <cpp/CommandParser.hpp>
#include <cpp/Math.hpp>

#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <pico/bootrom.h>
#include <hardware/watchdog.h>

#include <iostream>
#include <functional>
#include <malloc.h>

#include "cam_spi_master.h"
#include <Arducam_Mega.h>
#include <magic_enum/magic_enum.hpp>
#include <picojpeg.h>

// Update the rest of the app on progress using a float, 0.0f - 1.0f
using ProgressUpdateCallback = std::function<void(float)>;
using McuCopyFunc = void(*)(unsigned char* r, unsigned char* g, unsigned char* b, RGBColor* dest, int stride);

uint32_t getTotalHeap() 
{
   extern char __StackLimit, __bss_end__;
   return &__StackLimit  - &__bss_end__;
}

uint32_t getUsedHeap() 
{
   struct mallinfo m = mallinfo();
   return m.uordblks;
}

void imageSizeFromArducamMode(CAM_IMAGE_MODE mode, int& width, int& height)
{
  switch(mode)
  {
    case CAM_IMAGE_MODE_96X96    :
      width = 96;
      height = 96;
      return;
    case CAM_IMAGE_MODE_128X128  :
      width = 128;
      height = 128;
      return;
    case CAM_IMAGE_MODE_QQVGA    :
      width = 160;
      height = 120;
      return;
    case CAM_IMAGE_MODE_QVGA     :
      width = 320;
      height = 240;
      return;
    case CAM_IMAGE_MODE_320X320  :
      width = 320;
      height = 320;
      return;
    case CAM_IMAGE_MODE_VGA      :
      width = 640;
      height = 480;
      return;
    case CAM_IMAGE_MODE_SVGA     :
      width = 800;
      height = 600;
      return;
    case CAM_IMAGE_MODE_1024X768 :
      width = 1024;
      height = 768;
      return;
    case CAM_IMAGE_MODE_HD       :
      width = 1280;
      height = 720;
      return;
    case CAM_IMAGE_MODE_1280X1024:
      width = 1280;
      height = 1024;
      return;
    case CAM_IMAGE_MODE_UXGA     :
      width = 1600;
      height = 1200;
      return;
    case CAM_IMAGE_MODE_FHD      :
      width = 1920;
      height = 1080;
      return;
    case CAM_IMAGE_MODE_QXGA     :
      width = 2048;
      height = 1536;
      return;
    case CAM_IMAGE_MODE_WQXGA2   :
      width = 2592;
      height = 1944;
      return;
    default:
      width = 0;
      height = 0;
      return;
  }
}

void rebootIntoProgMode()
{
  //multicore_reset_core1();
  reset_usb_boot(0,0);
}

void flushCamera(Arducam_Mega& cam)
{
  uint8_t buf[255];
  int bytesFlushed = 0;
  while (cam.getReceivedLength() > 0)
  {
    bytesFlushed += cam.readBuff(buf, 255);
  }
  DEBUG_LOG_IF(bytesFlushed > 0, "Flushed " << bytesFlushed << " bytes from camera send buffer.");
}

void snapAndFlushCamera(Arducam_Mega& cam, CAM_IMAGE_MODE mode, CAM_IMAGE_PIX_FMT format)
{
  cam.takePicture(mode, format);
  flushCamera(cam);
}

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
            (uint8_t)std::clamp((int)line0[x*2+1] + (int)line2[x*2+1] / 2, 0, 255),
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
            (uint8_t)std::clamp((int)line0[x*2+1] + (int)line2[x*2+1] / 2, 0, 255),
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

int main()
{
  // Configure stdio
  stdio_init_all();

  // Wait 1 second for remote terminals to connect
  // before doing anything.
  sleep_ms(1000);

  // // Init the settings object
  // FlashStorage<Settings> settingsMgr;
  // Settings& settings = settingsMgr.data;

  // // Read the current settings
  // std::cout << "Loading settings..." << std::endl;
  // if (!settingsMgr.readFromFlash())
  // {
  //   std::cout << "No valid settings found, loading defaults..." << std::endl;
  //   settings.wifiPassword[0] = '\0';
  //   settings.wifiSsid[0] = '\0';
  // }
  // std::cout << "Load complete!" << std::endl;

  GPIOButton shutterButton(9, true);
  shutterButton.holdActivationRepeatMs(-1); // Don't send more than one held event ever

  LedStripWs2812b leds(16);
  LEDBuffer buf(6);
  float lastT{-1.0f};
  RGBColor lastFg, lastBg;
  auto showProgressOnLeds = [&](float t, RGBColor fg, RGBColor bg = {0,0,0})
  {    
    if (t == lastT && fg == lastFg && bg == lastBg)
    {
      return;
    }

    lastT = t;
    lastFg = fg;
    lastBg = bg;
    int thresh = remap(t, 0.0f, 0.95f, 0, (int)buf.size());
    for (int i = 0; i < buf.size(); ++i)
    {
      if (i < thresh)
      {
        buf[i] = fg;
      }
      else
      {
        buf[i] = bg;
      }
    }

    leds.writeColors(buf, 0.1f);
  };

  Arducam_Mega cam(CAM_CSn_PIN);
  cam.reset();
  cam.begin();
  cam.setAutoExposure(0);
  cam.setAutoISOSensitive(0);
  cam.setAutoWhiteBalance(0);
  cam.setImageQuality(IMAGE_QUALITY::HIGH_QUALITY);

  std::unique_ptr<Inky> inky = Inky::Create();
  std::shared_ptr<IndexedColorMap> colorMap;

  CommandParser parser;

  parser.addCommand("mem", "", "Show memory usage stats",[&]()
  {
      std::cout << "Memory Usage: " << getUsedHeap() << " / " << getTotalHeap() << std::endl;
  });

  parser.addCommand("prog", "", "Reboot into programming mode",[&]()
  {
    // Reboot into programming mode
    std::cout << "rebooting into programming mode..." << std::endl << std::flush;
    if (inky) inky->~Inky();
    rebootIntoProgMode();
  });

  parser.addCommand("reboot", "", "Reboot",[&]()
  {
      // Reboot the system immediately
      std::cout << "rebooting..." << std::endl << std::flush;
      if (inky) inky->~Inky();
      watchdog_reboot(0,0,50);
  });

  bool ledStateDirty = true;
  float ditherAccuracy = 0.75f;
  int camMode = (int)CAM_IMAGE_MODE_VGA;
  int camFormat = (int)CAM_IMAGE_PIX_FMT_JPG;
  snapAndFlushCamera(cam, (CAM_IMAGE_MODE)camMode, (CAM_IMAGE_PIX_FMT)camFormat);

  if (inky)
  {
    parser.addProperty("dither", ditherAccuracy, false, "0.0 - 1.0 (default 0.75)");

    parser.addCommand("format", "[enum]", "JPG=1, RGB565=2, YUV=3", [&](int format){
      camFormat = format;
      snapAndFlushCamera(cam, (CAM_IMAGE_MODE)camMode, (CAM_IMAGE_PIX_FMT)camFormat);
    });

    parser.addCommand("mode", "[enum]", "4=320x320, 5=640x480, 12=2048x1536", [&](int mode){
      camMode = mode;
      snapAndFlushCamera(cam, (CAM_IMAGE_MODE)camMode, (CAM_IMAGE_PIX_FMT)camFormat);
    });

    parser.addCommand("quality", "[compression]", "0=high, 1=med, 2=low", [&](int quality){
      cam.setImageQuality((IMAGE_QUALITY)quality);
      snapAndFlushCamera(cam, (CAM_IMAGE_MODE)camMode, (CAM_IMAGE_PIX_FMT)camFormat);
    });

    parser.addCommand("whitebalance", "[enum]", "Auto white balance mode, 0=default", [&](int wb){
      cam.setAutoWhiteBalanceMode((CAM_WHITE_BALANCE)wb);
      snapAndFlushCamera(cam, (CAM_IMAGE_MODE)camMode, (CAM_IMAGE_PIX_FMT)camFormat);
    });

    parser.addCommand("saturation", "[level]", "-3 to +3", [&](int value){
      switch (value)
      {
        case -3:
          cam.setSaturation(CAM_STAURATION_LEVEL_MINUS_3);
          break;
        case -2:
          cam.setSaturation(CAM_STAURATION_LEVEL_MINUS_2);
          break;
        case -1:
          cam.setSaturation(CAM_STAURATION_LEVEL_MINUS_1);
          break;
        case 0:
          cam.setSaturation(CAM_STAURATION_LEVEL_DEFAULT);
          break;
        case 1:
          cam.setSaturation(CAM_STAURATION_LEVEL_1);
          break;
        case 2:
          cam.setSaturation(CAM_STAURATION_LEVEL_2);
          break;
        case 3:
          cam.setSaturation(CAM_STAURATION_LEVEL_3);
          break;
        default:
          return false;
      }
      snapAndFlushCamera(cam, (CAM_IMAGE_MODE)camMode, (CAM_IMAGE_PIX_FMT)camFormat);
      return true;
    });

    parser.addCommand("eeprom", "", "Print out eeprom data read from the display",[&]()
    {
      inky->eeprom().print();
    });

    parser.addCommand("effect", "[val]", "", [&](std::string name) {

      std::optional<ColorMapEffect> effect;
      colorMap.reset();

      if (!effect)
      {
        effect = magic_enum::enum_cast<ColorMapEffect>(name, magic_enum::case_insensitive);
      }

      if (!effect)
      {
        effect = magic_enum::enum_cast<ColorMapEffect>(atoi(name.c_str()));
      }

      if (effect)
      {
        colorMap = getColorMapWithEffect(*inky->getColorMap(), effect.value());
      }
      
      if (colorMap)
        std::cout << "Set custom effect" << std::endl;
      else
        std::cout << "Cleared custom effect" << std::endl;
    });

    parser.addCommand("snap", "", "Snap a photo and display it.", [&]()
    {
      CAM_IMAGE_MODE mode = (CAM_IMAGE_MODE)camMode;
      CAM_IMAGE_PIX_FMT format = (CAM_IMAGE_PIX_FMT)camFormat;
      DEBUG_LOG("Taking photo...");
      showProgressOnLeds(1.0f, {255,0,0});

      cam.takePicture(mode, format);
      showProgressOnLeds(1.0f, {0,255,0});
      ProgressUpdateCallback progressCb = [&](float progress)
      {
        if (progress > 0.17f)
        {
          showProgressOnLeds(progress, {0,128,255});
        }
      };

      LabDitherView buffer(inky->bufferIndexed(), colorMap);
      buffer.ditherAccuracy = ditherAccuracy;

      bool decodeOk = false;
      auto startTime = to_ms_since_boot(get_absolute_time());
      int width, height;
      imageSizeFromArducamMode(mode, width, height);
      if (format == CAM_IMAGE_PIX_FMT_RGB565)
      {
        decodeOk = decodeImageRGB565(width, height, cam, buffer, progressCb);
      }
      else if (format == CAM_IMAGE_PIX_FMT_YUV)
      {
        decodeOk = decodeImageYUV(width, height, cam, buffer, progressCb);
      }
      else if (format == CAM_IMAGE_PIX_FMT_JPG)
      {
        decodeOk = decodeImageJPG(width, height, cam, buffer, progressCb);
      }

      flushCamera(cam);

      if (decodeOk)
      {
        auto elapsedTimeMs = to_ms_since_boot(get_absolute_time()) - startTime;
        DEBUG_LOG("Picture converted in " << elapsedTimeMs << " ms");
        inky->show();
      }

      return decodeOk;
    });

    parser.addCommand("test", "pattern", "Show a color test pattern",[&]()
    {
      // Color bar pattern, written directly in indexed colors
      const auto& indexedColors = inky->getColorMap()->indexedColors();
      auto& bufIndexed = *inky->bufferIndexed().get();
      int colsPerColor = bufIndexed.width / indexedColors.size();
      for (int y = 0; y < bufIndexed.height; ++y)
      {
        for (int x = 0; x < bufIndexed.width; ++x)
        {
          bufIndexed.setPixel(x, y, indexedColors[std::clamp(x / colsPerColor, 0, (int)(indexedColors.size()-1))]);
        }
      }
      inky->show();
    });

    parser.addCommand("clear", "", "Clear the display",[&]()
    {
        inky->clear();
        inky->show();
    });

    parser.addCommand("show", "", "Push diplay buffer to display",[&]()
    {
        inky->show();
    });
  }

  absolute_time_t nextEvalTime = get_absolute_time();
  while (1)
  {
    // Regulate loop speed
    sleep_until(nextEvalTime);
    nextEvalTime = make_timeout_time_ms(50);

    // Check for USB I/O
    parser.processStdIo();

    // Check for the shutter button being pressed
    shutterButton.update();
    if (shutterButton.buttonUp())
    {
      parser.processCommand("snap");
    }
    else
    {
      showProgressOnLeds(0.0f, {0,0,0});
    }

    if (shutterButton.heldActivate())
    {
      parser.processCommand("test 0");
    }

  }
  return 0;
}
