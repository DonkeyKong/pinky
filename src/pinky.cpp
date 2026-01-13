#include "Settings.hpp"
#include "Inky.hpp"
#include "ColorMapEffect.hpp"
#include "ArducamDecode.hpp"

#include <cpp/Button.hpp>
#include <cpp/LedStripWs2812b.hpp>
#include <cpp/CommandParser.hpp>
#include <cpp/Math.hpp>
#include <cpp/Memory.hpp>

#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <pico/bootrom.h>
#include <hardware/watchdog.h>

#include <iostream>

#include "cam_spi_master.h"
#include <Arducam_Mega.h>
#include <magic_enum/magic_enum.hpp>

// Resolutions supported by the Arducam Mega
// 320×240    CAM_IMAGE_MODE_QVGA
// 640×480    CAM_IMAGE_MODE_VGA
// 1280×720   CAM_IMAGE_MODE_HD
// 1600×1200  CAM_IMAGE_MODE_UXGA
// 1920×1080  CAM_IMAGE_MODE_FHD
// 2048×1536  CAM_IMAGE_MODE_QXGA (3MP only)
// 2592×1944  CAM_IMAGE_MODE_WQXGA2 (5MP only)

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
  auto status = cam.takePicture(mode, format);
  DEBUG_LOG_IF(status != CamStatus::CAM_ERR_SUCCESS, "arducam takePicture returned error: " << (int)status);
  flushCamera(cam);
}

int main()
{
  // Configure stdio
  stdio_init_all();

  // Wait 1 second for remote terminals to connect
  // before doing anything.
  sleep_ms(1000);

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
  cam.begin();
  cam.reset();
  cam.setAutoExposure(0);
  cam.setAutoISOSensitive(0);
  cam.setAutoWhiteBalance(0);
  bool ledStateDirty = true;
  float ditherAccuracy = 0.95f;
  bool yuvDownsample = true;
  int camMode = (int)CAM_IMAGE_MODE_UXGA;
  int camFormat = (int)CAM_IMAGE_PIX_FMT_YUV;
  snapAndFlushCamera(cam, (CAM_IMAGE_MODE)camMode, (CAM_IMAGE_PIX_FMT)camFormat);

  std::unique_ptr<Inky> inky = InkyCreate();
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

  if (inky)
  {
    parser.addProperty("dither", ditherAccuracy, false, "0.0 - 1.0 (default 0.75)");

    parser.addProperty("yuvDownsample", yuvDownsample, false, "Cut YUV image res in half");

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
      std::cout << "Display EEPROM:" << std::endl;
      std::cout << "    Width: " << inky->eeprom().width << std::endl;
      std::cout << "    Height: " << inky->eeprom().height << std::endl;
      std::cout << "    Color Capability: " << (int)inky->eeprom().colorCapability << std::endl;
      std::cout << "    PCB Variant: " << (int)inky->eeprom().pcbVariant << std::endl;
      std::cout << "    Display Variant: " << (int)inky->eeprom().displayVariant << std::endl;
      std::cout << "    Write Time: " << inky->eeprom().writeTime << std::endl;
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

      auto status = cam.takePicture(mode, format);
      DEBUG_LOG_IF(status != CamStatus::CAM_ERR_SUCCESS, "arducam takePicture returned error: " << (int)status);
      DEBUG_LOG("Fetching photo...");

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
      else if (format == CAM_IMAGE_PIX_FMT_YUV && !yuvDownsample)
      {
        decodeOk = decodeImageYUYV(width, height, cam, buffer, progressCb);
      }
      else if (format == CAM_IMAGE_PIX_FMT_YUV && yuvDownsample)
      {
        decodeOk = decodeImageYUYVHalf(width, height, cam, buffer, progressCb);
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

    parser.addCommand("bars", "", "Show a color test pattern",[&]()
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

    parser.addCommand("gradient", "", "Show a color test pattern",[&]()
    {
      // Color bar pattern, written directly in indexed colors
      LabDitherView buffer(inky->bufferIndexed(), colorMap);
      buffer.ditherAccuracy = ditherAccuracy;

      for (int y = 0; y < buffer.height; ++y)
      {
        for (int x = 0; x < buffer.width; ++x)
        {
          RGBColor rowColor = HSVColor{
            remap(x, 0, buffer.width, 0.0f, 360.0f),
            remap(y, 0, buffer.height, 0.0f, 1.0f),
            1.0f
          }.toRGB();


          buffer.setPixel(x, y, rowColor);
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
      parser.processCommand("test");
    }

  }
  return 0;
}
