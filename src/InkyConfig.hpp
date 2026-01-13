#pragma once

#include <hardware/spi.h>
#include <hardware/i2c.h>

#include <stdint.h>

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
  Seven_Colour_800x480_AC073TC1A = 20,
  Spectra_6_13_3_1600x1200_EL133UF1 = 21,
  Spectra_6_7_3_800x480_E673 = 22,
  Red_Yellow_pHAT_JD79661 = 23,
  Red_Yellow_wHAT_JD79668 = 24,
  Spectra_6_4_0_400_x_600_E640 = 25,

  InvalidDisplayType = 255
};

enum class ColorCapability : uint8_t
{
  BlackWhite = 1,
  BlackWhiteRed = 2,
  BlackWhiteYellow = 3,
  SevenColor = 5,
  Spectra6 = 6,
  BlackWhiteRedYellow = 7
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