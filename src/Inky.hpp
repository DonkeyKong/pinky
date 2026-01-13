#pragma once

#include "InkyConfig.hpp"
#include "InkyBase.hpp"
#include "InkySSD1683.hpp"
#include "InkyUC8159.hpp"
#include "InkyE673.hpp"

#include <cpp/Logging.hpp>
#include <cpp/I2CInterface.hpp>

#include "hardware/spi.h"

#include <vector>


InkyEeprom readEeprom(const InkyConfig& config)
{
  static_assert(sizeof(InkyEeprom) == 29, "InkyEeprom must be 29 bytes");

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
  I2CRegister<uint8_t> eepromRegisterReset(i2c, config.I2CDeviceId, 0);
  I2CRegister<InkyEeprom> eepromRegister(i2c, config.I2CDeviceId, 0);
  eepromRegisterReset.set(0);
  eepromRegister.get(eeprom);

  // Null terminate the string just in case
  eeprom.writeTime[21] = '\0';

  return eeprom;
}

std::unique_ptr<Inky> InkyCreate(const InkyConfig& config = {})
{
  auto eeprom = readEeprom(config);
  switch (eeprom.displayVariant)
  {
    case DisplayVariant::Black_wHAT_SSD1683:
    case DisplayVariant::Red_wHAT_SSD1683:
    case DisplayVariant::Yellow_wHAT_SSD1683:
      return std::make_unique<InkySSD1683>(config, eeprom);
    case DisplayVariant::Seven_Colour_UC8159:
    case DisplayVariant::Seven_Colour_640x400_UC8159:
    case DisplayVariant::Seven_Colour_640x400_UC8159_v2:
      return std::make_unique<InkyUC8159>(config, eeprom);
    case DisplayVariant::Spectra_6_7_3_800x480_E673:
      return std::make_unique<InkyE673>(config, eeprom);
    default:
      DEBUG_LOG("Display not created (EEPROM error)");
      return nullptr;
  }
}