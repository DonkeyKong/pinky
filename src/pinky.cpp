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

  auto inky = Inky::Create();

  CommandParser parser;

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

  parser.addCommand("test", "pattern", "Show a color test pattern",[&](int pattern)
  {
    if (pattern == 0)
    {
      inky->colorTest();
    }
    else if (pattern == 1)
    {
      auto& buffer = inky->bufferRGB();
      int centerX = buffer.width / 2;
      int centerY = buffer.height / 2;
      for (int y=0; y < buffer.height; ++y)
      {
        DEBUG_LOG_IF((y % 10 == 0), "Generation: " << (int)((float)y / (float) buffer.height * 100.0f) << "%");
        for (int x=0; x < buffer.width; ++x)
        {
          float dist = sqrt(powf(x-centerX, 2.0f) + powf(y-centerY, 2.0f));
          float hue = remap(dist, 0.0f, 240.0f, 0.0f, 360.0f);
          if ((int)dist % 20 < 3)
          {
            buffer.setPixel(x,y,{0,0,0});
          }
          else
          {
            buffer.setPixel(x,y, HSVColor{hue, 1.0f, 1.0f}.toRGB());
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

  parser.addCommand("mem", "", "Show memory usage stats",[&]()
  {
      std::cout << "Memory Usage: " << getUsedHeap() << " / " << getTotalHeap() << std::endl;
  });

  parser.addCommand("prog", "", "Reboot into programming mode",[&]()
  {
    // Reboot into programming mode
    std::cout << "rebooting into programming mode..." << std::endl << std::flush;
    rebootIntoProgMode();
  });

  parser.addCommand("reboot", "", "Reboot into programming mode",[&]()
  {
      // Reboot the system immediately
      std::cout << "rebooting..." << std::endl << std::flush;
      inky->~Inky();
      watchdog_reboot(0,0,50);
  });

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
