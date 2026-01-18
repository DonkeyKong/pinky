#pragma once

#include "InkyBase.hpp"

class InkyE673 final : public InkyBase
{
  private:

  enum class InkyCommand : uint8_t
  {
    EL673_PSR = 0x00,
    EL673_PWR = 0x01,
    EL673_POF = 0x02,
    EL673_POFS = 0x03,
    EL673_PON = 0x04,
    EL673_BTST1 = 0x05,
    EL673_BTST2 = 0x06,
    EL673_DSLP = 0x07,
    EL673_BTST3 = 0x08,
    EL673_DTM1 = 0x10,
    EL673_DSP = 0x11,
    EL673_DRF = 0x12,
    EL673_PLL = 0x30,
    EL673_CDI = 0x50,
    EL673_TCON = 0x60,
    EL673_TRES = 0x61,
    EL673_REV = 0x70,
    EL673_VDCS = 0x82,
    EL673_INIT = 0xAA,
    EL673_PWS = 0xE3,
    NOP = 0xFF
  };

  static const uint32_t SPIDeviceSpeedHz = 1000000;
  static const uint32_t SPITransferSize = 4096;
  static const uint32_t SendCommandDelay = 1;

  void waitForBusy(int timeoutMs = 40000)
  {
    // If the busy_pin is *high* (pulled up by host)
    // then assume we're not getting a signal from inky
    // and wait the timeout period to be safe.
    if (busy_.get())
    {
      sleep_ms(timeoutMs);
      return;
    }

    int i = 0;
    while (!busy_.get())
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
    reset_.set(false);
    sleep_ms(30);
    reset_.set(true);
    sleep_ms(30);

    waitForBusy(300);

    sendCommand(InkyCommand::EL673_INIT, (uint8_t[]){0x49, 0x55, 0x20, 0x08, 0x09, 0x18});
    sendCommand(InkyCommand::EL673_PWR, (uint8_t)0x3F);
    sendCommand(InkyCommand::EL673_PSR, (uint8_t[]){0x5F, 0x69});

    sendCommand(InkyCommand::EL673_BTST1, (uint8_t[]){0x40, 0x1F, 0x1F, 0x2C});
    sendCommand(InkyCommand::EL673_BTST3, (uint8_t[]){0x6F, 0x1F, 0x1F, 0x22});
    sendCommand(InkyCommand::EL673_BTST2, (uint8_t[]){0x6F, 0x1F, 0x17, 0x17});

    sendCommand(InkyCommand::EL673_POFS, (uint8_t[]){0x00, 0x54, 0x00, 0x44});
    sendCommand(InkyCommand::EL673_TCON, (uint8_t[]){0x02, 0x00});
    sendCommand(InkyCommand::EL673_PLL, (uint8_t)0x08);
    sendCommand(InkyCommand::EL673_CDI, (uint8_t)0x3F);
    sendCommand(InkyCommand::EL673_TRES, (uint8_t[]){0x03, 0x20, 0x01, 0xE0});
    sendCommand(InkyCommand::EL673_PWS, (uint8_t)0x2F);
    sendCommand(InkyCommand::EL673_VDCS, (uint8_t)0x01);
  }
  
  std::shared_ptr<Packed4BitIndexedImage> buf_;

public:
  InkyE673(const InkyConfig& config, InkyEeprom info) : InkyBase(config, info, SPIDeviceSpeedHz, SPITransferSize, SendCommandDelay)
  {
    // Give a little warning if the display type is wrong
    DEBUG_LOG_IF(info.displayVariant != DisplayVariant::Spectra_6_7_3_800x480_E673, "Unsupported Inky display type!!");

    colorMap_ = std::make_shared<IndexedColorMap>(ColorMapArgList{

        // Emperically measured colors
        // {ColorName::Black, 0, {30, 25, 40}},
        // {ColorName::White, 1, {225, 215, 200}},
        // {ColorName::Yellow, 2, {250, 200, 100}},
        // {ColorName::Red, 3, {160, 28, 0}},
        // {ColorName::Blue, 5, {21, 62, 150}},
        // {ColorName::Green, 6, {70, 96, 70}}

        // Tweaked colors
        {ColorName::Black, 0, {0, 0, 0}},
        {ColorName::White, 1, {190, 190, 190}},
        {ColorName::Yellow, 2, {250, 200, 100}},
        {ColorName::Red, 3, {160, 28, 0}},
        {ColorName::Blue, 5, {20, 80, 150}},
        {ColorName::Green, 6, {50, 130, 60}}

    });
    border_ = colorMap_->toIndexedColor(ColorName::Black);

    buf_ = std::make_shared<Packed4BitIndexedImage>(eeprom_.width, eeprom_.height);

    // Setup the GPIO pins
    dc_.set(false);
    reset_.set(true);

    // Call reset method here? Reference implementation does.
  }

  virtual ImageView<IndexedColor>& bufferIndexed() override
  {
    return *buf_;
  }

  virtual void show() override
  {
    reset();

    sendCommand(InkyCommand::EL673_DTM1, buf_->getData());
    sendCommand(InkyCommand::EL673_PON);
    sleep_ms(300);

    // second setting of the BTST2 register
    sendCommand(InkyCommand::EL673_BTST2, (uint8_t[]){0x6F, 0x1F, 0x17, 0x49});

    sendCommand(InkyCommand::EL673_DRF, (uint8_t)0x00);
    waitForBusy(320000);

    sendCommand(InkyCommand::EL673_POF, (uint8_t)0x00);
    waitForBusy(300);
  }

  virtual void clear() override
  {
    auto& bufIndexed = *buf_;
    for (int y=0; y < eeprom_.height; ++y)
    {
      for (int x=0; x < eeprom_.width; ++x)
      {
        bufIndexed.setPixel(x,y, border_);
      }
    }
  }

  virtual void clean() override
  {
    auto cleanColor = colorMap_->toIndexedColor(ColorName::Clean);
    auto& bufIndexed = *buf_;
    for (int y=0; y < eeprom_.height; ++y)
    {
      for (int x=0; x < eeprom_.width; ++x)
      {
        bufIndexed.setPixel(x,y, cleanColor);
      }
    }
  }
};
