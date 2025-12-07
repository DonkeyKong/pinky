#include "Settings.hpp"
#include "Inky.hpp"
#include "cpp/CommandParser.hpp"
#include "cpp/Math.hpp"

#include <pico/stdlib.h>
#include <pico/stdio.h>
#include <pico/bootrom.h>
#include <hardware/watchdog.h>

#include <iostream>
#include <malloc.h>

#include "cam_spi_master.h"
#include <Arducam_Mega.h>

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

void rebootIntoProgMode()
{
  //multicore_reset_core1();
  reset_usb_boot(0,0);
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

  Arducam_Mega cam(CAM_CSn_PIN);
  cam.begin();
  cam.setAutoExposure(0);
  cam.setAutoISOSensitive(0);
  cam.takePicture(CAM_IMAGE_MODE_VGA, CAM_IMAGE_PIX_FMT_RGB565);

  std::unique_ptr<Inky> inky = Inky::Create();
  std::shared_ptr<LabDitherView> labDitherBuf;
  std::shared_ptr<RGBDitherView> rgbDitherBuf;
  std::shared_ptr<RGBToIndexedImageView> rgbBuf;
  auto getDisplayBuffer = [&](int ditherMode)->RGBImageView&
  {
    switch(ditherMode)
    {
      case 0: 
        return *labDitherBuf.get();
      case 1: 
        return *rgbDitherBuf.get();
      default:
        return *rgbBuf.get();
    }
  };

  if (inky)
  {
    labDitherBuf = std::make_shared<LabDitherView>(inky->bufferIndexed());
    rgbDitherBuf = std::make_shared<RGBDitherView>(inky->bufferIndexed());
    rgbBuf = std::make_shared<RGBToIndexedImageView>(inky->bufferIndexed());
  }

  CommandParser parser;
  
  parser.addCommand("props", [&](){ parser.printPropertyValues(); });

  parser.addCommand("mem", "", "Show memory usage stats",[&]()
  {
      std::cout << "Memory Usage: " << getUsedHeap() << " / " << getTotalHeap() << std::endl;
  });

  parser.addCommand("prog", "", "Reboot into programming mode",[&]()
  {
    // Reboot into programming mode
    std::cout << "rebooting into programming mode..." << std::endl << std::flush;
    inky->~Inky();
    rebootIntoProgMode();
  });

  parser.addCommand("reboot", "", "Reboot into programming mode",[&]()
  {
      // Reboot the system immediately
      std::cout << "rebooting..." << std::endl << std::flush;
      if (inky) inky->~Inky();
      watchdog_reboot(0,0,50);
  });

  float testSaturation = 0.9f;
  float testValue = 0.9f;
  bool testRings = false;

  if (inky)
  {
    parser.addProperty("t1_s", testSaturation, false, "Test Pattern 1, Saturation");
    parser.addProperty("t1_v", testValue, false, "Test Pattern 1, Value");
    parser.addProperty("t1_rings", testRings, false, "Test Pattern 1, Rings enabled");
    parser.addProperty("dither_accuracy", labDitherBuf->ditherAccuracy, false, "Lab Dither error propigation rate. (default 0.95)");

    parser.addCommand("eeprom", "", "Print out eeprom data read from the display",[&]()
    {
      inky->eeprom().print();
    });

    parser.addCommand("snap", "", "Snap a photo and display it.", [&](int ditherMode){

      DEBUG_LOG("Taking photo...");
      int height = 480; int width = 640;
      cam.takePicture(CAM_IMAGE_MODE_VGA, CAM_IMAGE_PIX_FMT_RGB565);

      if (cam.getReceivedLength() != (width * height * 2))
      {
        std::cout << "Bad image size! Got " << cam.getReceivedLength() << " bytes, expected " << (width * height * 2) << " bytes" << std::endl;
        return;
      }

      auto startTime = to_ms_since_boot(get_absolute_time());

      auto& buffer = getDisplayBuffer(ditherMode);
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
            //rgb.applyGamma(0.8f);
            buffer.setPixel(x, y, rgb);
          }
          DEBUG_LOG_IF((y % 40 == 0), "Conversion: " << (int)((float)y / (float) buffer.height * 100.0f) << "%");
        }
      }

      auto elapsedTimeMs = to_ms_since_boot(get_absolute_time()) - startTime;
      DEBUG_LOG("Picture converted in " << elapsedTimeMs << " ms");
      inky->show();
    });

    parser.addCommand("test", "pattern", "Show a color test pattern",[&](int pattern)
    {
      if (pattern == 0)
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
      }
      else if (pattern == 1)
      {
        RGBImageView& buffer = *labDitherBuf.get();

        int centerX = buffer.width / 2;
        int centerY = buffer.height / 2;
        float radius = (float)buffer.width / 2.0f * 0.9f;

        for (int y=0; y < buffer.height; ++y)
        {
          DEBUG_LOG_IF((y % 10 == 0), "Generation: " << (int)((float)y / (float) buffer.height * 100.0f) << "%");
          for (int x=0; x < buffer.width; ++x)
          {
            float dist = sqrt(powf(x-centerX, 2.0f) + powf(y-centerY, 2.0f));
            float hue = remap(dist, 40.0f, radius, 0.0f, 360.0f);
            if (testRings && (int)dist % 20 < 3)
            {
              buffer.setPixel(x,y,{0,0,0});
            }
            else
            {
              buffer.setPixel(x,y, HSVColor{hue, testSaturation, testValue}.toRGB());
            }
          }
        }
      }
      DEBUG_LOG("Test pattern generated. Displaying...");
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

    // Do stuff
    parser.processStdIo();
  }
  return 0;
}
