#pragma once

#include "InkyBase.hpp"

class InkyUC8159 final : public InkyBase
{
  private:

  enum class InkyCommand : uint8_t
  {
    UC8159_PSR = 0x00,
    UC8159_PWR = 0x01,
    UC8159_POF = 0x02,
    UC8159_PFS = 0x03,
    UC8159_PON = 0x04,
    UC8159_BTST = 0x06,
    UC8159_DSLP = 0x07,
    UC8159_DTM1 = 0x10,
    UC8159_DSP = 0x11,
    UC8159_DRF = 0x12,
    UC8159_IPC = 0x13,
    UC8159_PLL = 0x30,
    UC8159_TSC = 0x40,
    UC8159_TSE = 0x41,
    UC8159_TSW = 0x42,
    UC8159_TSR = 0x43,
    UC8159_CDI = 0x50,
    UC8159_LPD = 0x51,
    UC8159_TCON = 0x60,
    UC8159_TRES = 0x61,
    UC8159_DAM = 0x65,
    UC8159_REV = 0x70,
    UC8159_FLG = 0x71,
    UC8159_AMV = 0x80,
    UC8159_VV = 0x81,
    UC8159_VDCS = 0x82,
    UC8159_PWS = 0xE3,
    UC8159_TSSET = 0xE5,
    NOP = 0xFF
  };

  struct CorrectionData
  {
    int cols = 0;
    int rows = 0;
    uint8_t rotation = 0;
    uint8_t offsetX = 0;
    uint8_t offsetY = 0;
    uint8_t resolutionSetting = 0;
  };
  static const uint32_t SPIDeviceSpeedHz = 3000000;
  static const uint32_t SPITransferSize = 4096;
  static const uint32_t SendCommandDelay = 1;

  CorrectionData correctionData;
  void reset();
  void waitForBusy(int timeoutMs = 40000);

  std::shared_ptr<Packed4BitIndexedImage> buf_;

public:
  InkyUC8159(const InkyConfig& config, InkyEeprom info) : InkyBase(config, info, SPIDeviceSpeedHz, SPITransferSize, SendCommandDelay)
  {
    // Detect the display type, make sure it's supported, and set some correction data
    switch (info.displayVariant)
    {
      case DisplayVariant::Seven_Colour_UC8159:
        correctionData = 
        { 
          .cols = 600,
          .rows = 448,
          .resolutionSetting = 0b11000000,
        };
      break;
      case DisplayVariant::Seven_Colour_640x400_UC8159:
      case DisplayVariant::Seven_Colour_640x400_UC8159_v2:
        correctionData = 
        { 
          .cols = 640,
          .rows = 400,
          .resolutionSetting = 0b10000000,
        };
      break;
      default:
        DEBUG_LOG("Unsupported Inky display type!!");
        break;
    }

    colorMap_ = std::make_shared<IndexedColorMap>(ColorMapArgList{
        // Emperically measured colors
        {ColorName::Black, 0, {36, 39, 63}},
        //{ColorName::Black, 0, {0, 0, 0}},
        //{ColorName::White, 1, {195, 185, 184}},
        {ColorName::White, 1, {240, 230, 230}},
        //{ColorName::White, 1, {255, 255, 255}},
        {ColorName::Green, 2, {56, 76, 46}},
        {ColorName::Blue, 3, {59, 54, 86}},
        {ColorName::Red, 4, {133, 55, 46}},
        {ColorName::Yellow, 5, {195, 158, 56}},
        {ColorName::Orange, 6, {159, 83, 57}}
    });
    border_ = colorMap_->toIndexedColor(ColorName::Black);

    // Correct the eeprom and buffer sizes
    eeprom_.width = correctionData.cols;
    eeprom_.height = correctionData.rows;

    buf_ = std::make_shared<Packed4BitIndexedImage>(eeprom_.width, eeprom_.height);

    // Setup the GPIO pins
    dc_.set(false);
    reset_.set(true);
  }

  virtual ImageView<IndexedColor>& bufferIndexed() override
  {
    return *buf_;
  }

  virtual void show() override;

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

void InkyUC8159::reset()
{
    reset_.set(false);
    sleep_ms(100);
    reset_.set(true);

    waitForBusy(1000);

    // Resolution Setting
    // 10bit horizontal followed by a 10bit vertical resolution
    sendCommand(InkyCommand::UC8159_TRES, (uint16_t[]){eeprom_.width, eeprom_.height} );

    // Panel Setting
    // 0b11000000 = Resolution select, 0b00 = 640x480, our panel is 0b11 = 600x448
    // 0b00100000 = LUT selection, 0 = ext flash, 1 = registers, we use ext flash
    // 0b00010000 = Ignore
    // 0b00001000 = Gate scan direction, 0 = down, 1 = up (default)
    // 0b00000100 = Source shift direction, 0 = left, 1 = right (default)
    // 0b00000010 = DC-DC converter, 0 = off, 1 = on
    // 0b00000001 = Soft reset, 0 = Reset, 1 = Normal (Default)
    // 0b11 = 600x448
    // 0b10 = 640x400
    sendCommand(InkyCommand::UC8159_PSR, (uint8_t[])
    {
        (uint8_t)(correctionData.resolutionSetting | 0b00101111),  // See above for more magic numbers
        0x08                                                       // display_colours == UC8159_7C
    });

    // Power Settings
    sendCommand(InkyCommand::UC8159_PWR, (uint8_t[])
    {
        (0x06 << 3) |  // ??? - not documented in UC8159 datasheet  # noqa: W504
        (0x01 << 2) |  // SOURCE_INTERNAL_DC_DC                     # noqa: W504
        (0x01 << 1) |  // GATE_INTERNAL_DC_DC                       # noqa: W504
        (0x01),        // LV_SOURCE_INTERNAL_DC_DC
        0x00,          // VGx_20V
        0x23,          // UC8159_7C
        0x23           // UC8159_7C
    });

    // Set the PLL clock frequency to 50Hz
    // 0b11000000 = Ignore
    // 0b00111000 = M
    // 0b00000111 = N
    // PLL = 2MHz * (M / N)
    // PLL = 2MHz * (7 / 4)
    // PLL = 2,800,000 ???
    sendCommand(InkyCommand::UC8159_PLL, (uint8_t)0x3C);  // 0b00111100

    // Send the TSE register to the display
    sendCommand(InkyCommand::UC8159_TSE, (uint8_t)0x00);  // Colour

    // VCOM and Data Interval setting
    // 0b11100000 = Vborder control (0b001 = LUTB voltage)
    // 0b00010000 = Data polarity
    // 0b00001111 = Vcom and data interval (0b0111 = 10, default)
    sendCommand(InkyCommand::UC8159_CDI, (uint8_t)((border_ << 5) | 0x17));  // 0b00110111

    // Gate/Source non-overlap period
    // 0b11110000 = Source to Gate (0b0010 = 12nS, default)
    // 0b00001111 = Gate to Source
    sendCommand(InkyCommand::UC8159_TCON, (uint8_t)0x22);  // 0b00100010

    // Disable external flash
    sendCommand(InkyCommand::UC8159_DAM, (uint8_t)0x00);

    // UC8159_7C
    sendCommand(InkyCommand::UC8159_PWS, (uint8_t)0xAA);

    // Power off sequence
    // 0b00110000 = power off sequence of VDH and VDL, 0b00 = 1 frame (default)
    // All other bits ignored?
    sendCommand(InkyCommand::UC8159_PFS, (uint8_t)0x00);  // PFS_1_FRAME
}

void InkyUC8159::waitForBusy(int timeoutMs)
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

void InkyUC8159::show()
{
  reset();

  sendCommand(InkyCommand::UC8159_DTM1, buf_->getData());

  sendCommand(InkyCommand::UC8159_PON);
  waitForBusy(200);

  sendCommand(InkyCommand::UC8159_DRF);
  waitForBusy(32000);

  sendCommand(InkyCommand::UC8159_POF);
  waitForBusy(200);
}


