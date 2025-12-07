#pragma once

#include "ImageView.hpp"

#include <cpp/Logging.hpp>
#include <cpp/I2CInterface.hpp>
#include <cpp/SPIDevice.hpp>
#include <cpp/DiscreteIn.hpp>
#include <cpp/DiscreteOut.hpp>

#include <vector>

struct InkyConfig
{
  uint8_t I2CDeviceId = 0x50;
  i2c_inst_t* I2CInstance = i2c0;
  spi_inst_t* SPIInstance = spi0;
  uint I2C_SDA_PIN = 0;  // I2C data
  uint I2C_SCL_PIN = 1;  // I2C clock
  uint SPI_CLOCK_PIN = 2;   // SPI clock
  uint SPI_MOSI_PIN = 3;  // SPI MOSI (host transmit)
  uint SPI_MISO_PIN = 4;  // SPI MISO (host receive) unused
  uint SPI_CSn_PIN = 5;   // SPI chip select
  uint BUSY_PIN = 6;  // Device busy (gpio)
  uint RESET_PIN = 7; // Device Reset (gpio)
  uint DC_PIN = 8;    // delay/command (gpio)
};

enum class ColorCapability : uint8_t
{
  BlackWhite = 1,
  BlackWhiteRed = 2,
  BlackWhiteYellow = 3,
  SevenColor = 4,
  SevenColorv2 = 5
};

enum class DisplayVariant : uint8_t
{
    Red_pHAT_High_Temp = 1,
    Yellow_wHAT = 2,
    Black_wHAT = 3,
    Black_pHAT = 4,
    Yellow_pHAT = 5,
    Red_wHAT = 6,
    Red_wHAT_High_Temp = 7,
    Red_wHAT_v2 = 8,
    Black_pHAT_SSD1608 = 10,
    Red_pHAT_SSD1608 = 11,
    Yellow_pHAT_SSD1608 = 12,
    Seven_Colour_UC8159 = 14,
    Seven_Colour_640x400_UC8159 = 15,
    Seven_Colour_640x400_UC8159_v2 = 16,
    Black_wHAT_SSD1683 = 17,
    Red_wHAT_SSD1683 = 18,
    Yellow_wHAT_SSD1683 = 19,
    InvalidDisplayType = 255
};

#pragma pack(push, 1)
struct InkyEeprom
{
  uint16_t width;
  uint16_t height;
  ColorCapability colorCapability;
  uint8_t pcbVariant;
  DisplayVariant displayVariant;
  char writeTime[22];
};
#pragma pack(pop)

class Inky
{
public:

  // Define this later after all screen subtypes have been declared
  static std::unique_ptr<Inky> Create(const InkyConfig& config = {});

  virtual ~Inky() = 0;

  // Get a buffer to draw to the display using indexed colors
  virtual std::shared_ptr<IndexedImageView> bufferIndexed() = 0;

  // Get a buffer to draw to the display using RGB colors
  // Supports advanced dithering (TBI)
  virtual std::shared_ptr<RGBImageView> bufferRGB() = 0;

  // Get the colormap used to convert RGB to the display's indexed colors
  virtual std::shared_ptr<const IndexedColorMap> getColorMap() const = 0;
  // Set the color of the bonder pixels
  virtual void setBorder(IndexedColor color) = 0;
  // Get the display eeprom info fetched from I2C before connection
  virtual const InkyEeprom& eeprom() const = 0;
  // Set the the buffer to all "clean" pixels
  virtual void clear() = 0;
  // Push the buffer contents to the display
  virtual void show() = 0;
};

// Constants for SSD1608 driver IC
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

  NOP = 0xFF,
};

Inky::~Inky() {}

class InkyBase : public Inky
{
protected:
  InkyEeprom eeprom_;
  SPIDevice spi_;
  DiscreteIn busy_;
  DiscreteOut reset_;
  DiscreteOut dc_;

  std::shared_ptr<const IndexedColorMap> colorMap_;
  IndexedColor border_;

  InkyBase(const InkyConfig& config, InkyEeprom InkyEeprom, uint32_t spiSpeedHz, uint32_t spiTransferSizeBytes) : 
    eeprom_{InkyEeprom},
    spi_
    { 
      config.SPIInstance,
      config.SPI_MISO_PIN, 
      config.SPI_MOSI_PIN,
      config.SPI_CLOCK_PIN,
      config.SPI_CSn_PIN,
      spiSpeedHz,
      spiTransferSizeBytes
    },
    busy_{config.BUSY_PIN},
    reset_{config.RESET_PIN},
    dc_{config.DC_PIN}
  {
    std::vector<std::tuple<ColorName,IndexedColor,RGBColor>> displayColors;
    switch (eeprom_.colorCapability)
    {
      case ColorCapability::BlackWhite:
        displayColors = 
        {
          {ColorName::White, 0, {255,255,255}},
          {ColorName::Black, 1, {0,0,0}}
        };
        break;
      case ColorCapability::BlackWhiteRed:
        displayColors = 
        {
          {ColorName::White, 0, {255,255,255}},
          {ColorName::Black, 1, {0,0,0}},
          {ColorName::Red, 2, {255,0,0}}
        };
        break;
      case ColorCapability::BlackWhiteYellow:
        displayColors = 
        {
          {ColorName::White, 0, {255,255,255}},
          {ColorName::Black, 1, {0,0,0}},
          {ColorName::Yellow, 2, {255,0,0}}
        };
        break;
      case ColorCapability::SevenColor:
      case ColorCapability::SevenColorv2:
        displayColors = 
          {
            {ColorName::Black, 0, {0, 0, 0}},
            {ColorName::White, 1, {255, 255, 255}},
            {ColorName::Green, 2, {81, 128, 44}},
            {ColorName::Blue, 3, {90, 78, 144}},
            {ColorName::Red, 4, {240, 96, 87}},
            {ColorName::Yellow, 5, {255, 255, 152}},
            {ColorName::Orange, 6, {255, 157, 125}}
          };
        break;
    }
    
    colorMap_ = std::make_shared<const IndexedColorMap>(displayColors);
    border_ = colorMap_->toIndexedColor(ColorName::White);
  }

  virtual void setBorder(IndexedColor color) override
  {
    border_ = color;
  }

  virtual const InkyEeprom& eeprom() const override
  {
    return eeprom_;
  }

  virtual std::shared_ptr<const IndexedColorMap> getColorMap() const override
  {
    return colorMap_;
  }

  void sendCommand(InkyCommand command)
  {
    dc_.set(false);
    #ifdef DEBUG_SPI
    std::cout << "Command " << (int)command << " ret: " << 
    #endif
    spi_.write((uint8_t*)(&command), 1);
    #ifdef DEBUG_SPI
    std::cout << std::endl;
    #endif
  }

  template <typename T>
  void sendCommand(InkyCommand command, const T& data)
  {
    sendCommand(command);

    size_t len = 0;
    const uint8_t* dataPtr = nullptr;

    if constexpr(std::is_same<T, std::initializer_list<uint8_t>>())
    {
      len = data.size();
      dataPtr = std::data(data);
    }
    else if constexpr(std::is_same<T, std::vector<uint8_t>>())
    {
      len = data.size();
      dataPtr = data.data();
    }
    else if constexpr(std::is_arithmetic<T>())
    {
      len = sizeof(T);
      dataPtr = (uint8_t*)(&data);
    }
    else if constexpr(std::is_trivial<T>() && std::is_standard_layout<T>())
    {
      len = sizeof(T);
      dataPtr = (uint8_t*)(&data);
    }
    else
    {
      static_assert(std::is_trivial<T>() && std::is_standard_layout<T>(), "Unsupported data type!");
    }

    dc_.set(true);
    #ifdef DEBUG_SPI
    std::cout << "Sent buffer len " << len << " ret: " << 
    #endif
    spi_.write(dataPtr, len);
    #ifdef DEBUG_SPI
    std::cout << std::endl;
    #endif
  }
};

class InkySSD1683 final : public InkyBase
{
  private: 
  static const int SPIDeviceSpeedHz = 10000000;
  static const uint32_t SPITransferSize = 4096;

  void waitForBusy(int timeoutMs = 5000)
  {
    int i = 0;
    while (busy_.get())
    {
      sleep_ms(10);
      ++i;
      if (i*10 > timeoutMs)
      {
        DEBUG_LOG("Timed out while wating for display to finish an operation.");
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
  std::shared_ptr<RGBToIndexedImageView> bufRgb_;


public:
  InkySSD1683(const InkyConfig& config, InkyEeprom info) : InkyBase(config, info, SPIDeviceSpeedHz, SPITransferSize)
  {
    if (info.displayVariant != DisplayVariant::Black_wHAT_SSD1683 &&
        info.displayVariant != DisplayVariant::Red_wHAT_SSD1683 &&
        info.displayVariant != DisplayVariant::Yellow_wHAT_SSD1683)
    {
      DEBUG_LOG("WARNING: Unsupported display type for InkySSD1683!");
    }

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
    bufRgb_ = std::make_shared<RGBToIndexedImageView>(bufIndexed_);
  }

  virtual std::shared_ptr<IndexedImageView> bufferIndexed() override
  {
    return bufIndexed_;
  }

  virtual std::shared_ptr<RGBImageView> bufferRGB() override
  {
    return bufRgb_;
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

class InkyUC8159 final : public InkyBase
{
  private:
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
  CorrectionData correctionData;
  void reset();
  void waitForBusy(int timeoutMs = 40000);

  std::shared_ptr<Packed4BitIndexedImage> buf_;
  std::shared_ptr<RGBToIndexedImageView> bufRgb_;

public:
  InkyUC8159(const InkyConfig& config, InkyEeprom info) : InkyBase(config, info, SPIDeviceSpeedHz, SPITransferSize)
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

    // Correct the eeprom and buffer sizes
    eeprom_.width = correctionData.cols;
    eeprom_.height = correctionData.rows;

    buf_ = std::make_shared<Packed4BitIndexedImage>(eeprom_.width, eeprom_.height, colorMap_);
    bufRgb_ = std::make_shared<RGBToIndexedImageView>(buf_);

    // Setup the GPIO pins
    dc_.set(false);
    reset_.set(true);
  }

  virtual std::shared_ptr<IndexedImageView> bufferIndexed() override
  {
    return buf_;
  }

  virtual std::shared_ptr<RGBImageView> bufferRGB() override
  {
    return bufRgb_;
  }

  virtual void show() override;

  virtual void clear() override
  {
    auto cleanColor = colorMap_->toIndexedColor(ColorName::Clean);
    auto& bufIndexed = *buf_.get();
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
      DEBUG_LOG("Timed out while wating for display to finish an operation.");
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

InkyEeprom readEeprom(const InkyConfig& config)
{
  InkyEeprom eeprom
  {
    .width = 0,
    .height = 0,
    .colorCapability = ColorCapability::BlackWhite,
    .pcbVariant = 0,
    .displayVariant = DisplayVariant::InvalidDisplayType,
  };
  strcpy(eeprom.writeTime, "invalid");
  
  I2CInterface i2c(config.I2CInstance, config.I2C_SDA_PIN, config.I2C_SCL_PIN, 100000);
  I2CRegister<InkyEeprom> eepromRegister(i2c, config.I2CDeviceId, 0);
  eepromRegister.get(eeprom);

  // Null terminate the string just in case
  eeprom.writeTime[21] = '\0';

  return eeprom;
}

std::unique_ptr<Inky> Inky::Create(const InkyConfig& config)
{
  auto InkyEeprom = readEeprom(config);
  switch (InkyEeprom.displayVariant)
  {
    case DisplayVariant::Black_wHAT_SSD1683:
    case DisplayVariant::Red_wHAT_SSD1683:
    case DisplayVariant::Yellow_wHAT_SSD1683:
      return std::make_unique<InkySSD1683>(config, InkyEeprom);
    case DisplayVariant::Seven_Colour_UC8159:
    case DisplayVariant::Seven_Colour_640x400_UC8159:
    case DisplayVariant::Seven_Colour_640x400_UC8159_v2:
      return std::make_unique<InkyUC8159>(config, InkyEeprom);
    default:
      DEBUG_LOG("Unknown display detected, assuming InkySSD1683");
      return std::make_unique<InkySSD1683>(config, InkyEeprom);
  }
}