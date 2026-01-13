#pragma once

#include "InkyBase.hpp"

class InkySSD1683 final : public InkyBase
{
  private: 

  enum class InkyCommand : uint8_t
  {
    SSD1683_DRIVER_CONTROL = 0x01,
    SSD1683_GATE_VOLTAGE = 0x03,
    SSD1683_SOURCE_VOLTAGE = 0x04,
    SSD1683_DISPLAY_CONTROL = 0x07,
    SSD1683_NON_OVERLAP = 0x0B,
    SSD1683_BOOSTER_SOFT_START = 0x0C,
    SSD1683_GATE_SCAN_START = 0x0F,
    SSD1683_DEEP_SLEEP = 0x10,
    SSD1683_DATA_MODE = 0x11,
    SSD1683_SW_RESET = 0x12,
    SSD1683_TEMP_WRITE = 0x1A,
    SSD1683_TEMP_READ = 0x1B,
    SSD1683_TEMP_CONTROL = 0x18,
    SSD1683_TEMP_LOAD = 0x1A,
    SSD1683_MASTER_ACTIVATE = 0x20,
    SSD1683_DISP_CTRL1 = 0x21,
    SSD1683_DISP_CTRL2 = 0x22,
    SSD1683_WRITE_RAM = 0x24,
    SSD1683_WRITE_ALTRAM = 0x26,
    SSD1683_READ_RAM = 0x25,
    SSD1683_VCOM_SENSE = 0x2B,
    SSD1683_VCOM_DURATION = 0x2C,
    SSD1683_WRITE_VCOM = 0x2C,
    SSD1683_READ_OTP = 0x2D,
    SSD1683_WRITE_LUT = 0x32,
    SSD1683_WRITE_DUMMY = 0x3A,
    SSD1683_WRITE_GATELINE = 0x3B,
    SSD1683_WRITE_BORDER = 0x3C,
    SSD1683_SET_RAMXPOS = 0x44,
    SSD1683_SET_RAMYPOS = 0x45,
    SSD1683_SET_RAMXCOUNT = 0x4E,
    SSD1683_SET_RAMYCOUNT = 0x4F,
    NOP = 0xFF,
  };

  static const int SPIDeviceSpeedHz = 10000000;
  static const uint32_t SPITransferSize = 4096;
  static const uint32_t SendCommandDelay = 1;

  void waitForBusy(int timeoutMs = 5000)
  {
    int i = 0;
    while (busy_.get())
    {
      sleep_ms(10);
      ++i;
      if (i*10 > timeoutMs)
      {
        DEBUG_LOG("Display operation is running long.");
        i = 0;
      }
    }
  }

  void reset()
  {
    // Perform a hardware reset
    reset_.set(false);
    sleep_ms(500);
    reset_.set(true);
    sleep_ms(500);
    sendCommand(InkyCommand::SSD1683_SW_RESET);
    sleep_ms(1000);
    waitForBusy();
  }

  std::shared_ptr<PackedTwoPlaneBinaryImage> buf_;
  std::shared_ptr<IndexedToPackedTwoPlaneBinaryImageView> bufIndexed_;


public:
  InkySSD1683(const InkyConfig& config, InkyEeprom info) : InkyBase(config, info, SPIDeviceSpeedHz, SPITransferSize, SendCommandDelay)
  {
    if (info.displayVariant != DisplayVariant::Black_wHAT_SSD1683 &&
        info.displayVariant != DisplayVariant::Red_wHAT_SSD1683 &&
        info.displayVariant != DisplayVariant::Yellow_wHAT_SSD1683)
    {
      DEBUG_LOG("WARNING: Unsupported display type for InkySSD1683!");
    }

    if (info.colorCapability == ColorCapability::BlackWhite)
    {
      colorMap_ = std::make_shared<IndexedColorMap>(ColorMapArgList{
        {ColorName::White, 0, ColorNameToSaturatedRGBColor(ColorName::White)},
        {ColorName::Black, 1, ColorNameToSaturatedRGBColor(ColorName::Black)}
      });
    }
    else if (info.colorCapability == ColorCapability::BlackWhiteRed)
    {
      colorMap_ = std::make_shared<IndexedColorMap>(ColorMapArgList{
        {ColorName::White, 0, ColorNameToSaturatedRGBColor(ColorName::White)},
        {ColorName::Black, 1, ColorNameToSaturatedRGBColor(ColorName::Black)},
        {ColorName::Red, 2, ColorNameToSaturatedRGBColor(ColorName::Red)}
      });
    }
    else if (info.colorCapability == ColorCapability::BlackWhiteYellow)
    {
      colorMap_ = std::make_shared<IndexedColorMap>(ColorMapArgList{
        {ColorName::White, 0, ColorNameToSaturatedRGBColor(ColorName::White)},
        {ColorName::Black, 1, ColorNameToSaturatedRGBColor(ColorName::Black)},
        {ColorName::Yellow, 2, ColorNameToSaturatedRGBColor(ColorName::Yellow)}
      });
    }

    border_ = colorMap_->toIndexedColor(ColorName::Black);

    // Setup the GPIO pins
    dc_.set(false);
    reset_.set(true);

    buf_ = std::make_shared<PackedTwoPlaneBinaryImage>(eeprom_.width, eeprom_.height);

    IndexedColor color = colorMap_->toIndexedColor(ColorName::Red);
    if (eeprom_.colorCapability == ColorCapability::BlackWhiteYellow)
    {
      color = colorMap_->toIndexedColor(ColorName::Yellow);
    }
    bufIndexed_ = std::make_shared<IndexedToPackedTwoPlaneBinaryImageView>(
      buf_, 
      colorMap_, 
      colorMap_->toIndexedColor(ColorName::Black),
      colorMap_->toIndexedColor(ColorName::White), 
      color, 
      color
    );
  }

  virtual std::shared_ptr<IndexedImageView> bufferIndexed() override
  {
    return bufIndexed_;
  }

  virtual void show() override
  {
    reset();

    sendCommand(InkyCommand::SSD1683_DRIVER_CONTROL, (uint8_t[]){(uint8_t)(eeprom_.height - 1), (uint8_t)((eeprom_.height - 1) >> 8), 0x00});
    // Set dummy line period
    sendCommand(InkyCommand::SSD1683_WRITE_DUMMY, uint8_t{0x1B});
    // Set Line Width
    sendCommand(InkyCommand::SSD1683_WRITE_GATELINE, uint8_t{0x0B});
    // Data entry squence (scan direction leftward and downward)
    sendCommand(InkyCommand::SSD1683_DATA_MODE, uint8_t{0x03});
    // Set ram X start and end position
    sendCommand(InkyCommand::SSD1683_SET_RAMXPOS, (uint8_t[]){(uint8_t)0x00, (uint8_t)((eeprom_.width / 8) - 1)});
    // Set ram Y start and end position
    sendCommand(InkyCommand::SSD1683_SET_RAMYPOS, (uint8_t[]){0x00, 0x00, (uint8_t)(eeprom_.height - 1), (uint8_t)((eeprom_.height - 1) >> 8)});
    // VCOM Voltage
    sendCommand(InkyCommand::SSD1683_WRITE_VCOM, uint8_t{0x70});
    // Write LUT DATA
    // sendCommand(InkyCommand::WRITE_LUT, self._luts[self.lut])

    if (border_ == colorMap_->toIndexedColor(ColorName::Black))
    {
      sendCommand(InkyCommand::SSD1683_WRITE_BORDER, uint8_t{0b00000000});
      // GS Transition + Waveform 00 + GSA 0 + GSB 0
    }  
    else if (border_ == colorMap_->toIndexedColor(ColorName::Red))
    {
      sendCommand(InkyCommand::SSD1683_WRITE_BORDER, uint8_t{0b00000110});
      // GS Transition + Waveform 01 + GSA 1 + GSB 0
    }
    else if (border_ == colorMap_->toIndexedColor(ColorName::Yellow))
    {
      sendCommand(InkyCommand::SSD1683_WRITE_BORDER, uint8_t{0b00001111});
      // GS Transition + Waveform 11 + GSA 1 + GSB 1
    }
    else if (border_ == colorMap_->toIndexedColor(ColorName::White))
    {
      sendCommand(InkyCommand::SSD1683_WRITE_BORDER, uint8_t{0b00000001});
      // GS Transition + Waveform 00 + GSA 0 + GSB 1
    }

    // Set RAM address to 0, 0
    sendCommand(InkyCommand::SSD1683_SET_RAMXCOUNT, uint8_t{0x00});
    sendCommand(InkyCommand::SSD1683_SET_RAMYCOUNT, (uint8_t[2]){0x00, 0x00});
    
    sendCommand(InkyCommand::SSD1683_WRITE_RAM, buf_->getPlane(PackedTwoPlaneBinaryImage::Plane::Black));

    if (eeprom_.colorCapability != ColorCapability::BlackWhite)
    {
      sendCommand(InkyCommand::SSD1683_WRITE_ALTRAM, buf_->getPlane(PackedTwoPlaneBinaryImage::Plane::Color));
    }

    waitForBusy();
    sendCommand(InkyCommand::SSD1683_MASTER_ACTIVATE);
  }

  virtual void clear() override
  {
    auto whiteColor = colorMap_->toIndexedColor(ColorName::White);
    auto& bufIndexed = *bufIndexed_.get();
    for (int y=0; y < eeprom_.height; ++y)
    {
      for (int x=0; x < eeprom_.width; ++x)
      {
        bufIndexed.setPixel(x,y, whiteColor);
      }
    }
  }
};
