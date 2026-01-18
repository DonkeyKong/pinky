#pragma once

#include "ImageView.hpp"
#include "IndexedColor.hpp"
#include "InkyConfig.hpp"

#include <cpp/DiscreteIn.hpp>
#include <cpp/DiscreteOut.hpp>
#include <cpp/Logging.hpp>
#include <cpp/SPIDevice.hpp>

#include <memory>

class Inky
{
public:
  virtual ~Inky() = 0;
  // Get a buffer to draw to the display using indexed colors
  virtual ImageView<IndexedColor>& bufferIndexed() = 0;
  // Get the colormap used to convert RGB to the display's indexed colors
  virtual const IndexedColorMap& colorMap() const = 0;
  // Set the color of the bonder pixels
  virtual void setBorder(IndexedColor color) = 0;
  // Get the display eeprom info fetched from I2C before connection
  virtual const InkyEeprom& eeprom() const = 0;
  // Set all pixels in the buffer to the same color
  // as the border
  virtual void clear() = 0;
  // Set the the buffer to all "clean" pixels
  // (or white if clean is not available)
  virtual void clean() = 0;
  // Push the buffer contents to the display
  virtual void show() = 0;
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

  std::shared_ptr<IndexedColorMap> colorMap_;
  IndexedColor border_;
  uint32_t sendCommandDelay_;

  InkyBase(const InkyConfig& config, InkyEeprom InkyEeprom, uint32_t spiSpeedHz, uint32_t spiTransferSizeBytes, uint32_t sendCommandDelay) : 
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
    dc_{config.DC_PIN},
    sendCommandDelay_{sendCommandDelay}
  { }

  virtual void setBorder(IndexedColor color) override
  {
    border_ = color;
  }

  virtual const InkyEeprom& eeprom() const override
  {
    return eeprom_;
  }

  virtual const IndexedColorMap& colorMap() const override
  {
    return *colorMap_;
  }

  template <typename C>
  void sendCommand(C command)
  {
    dc_.set(false);
    sleep_ms(sendCommandDelay_);
    #ifdef DEBUG_SPI
    std::cout << "Command " << (int)command << " ret: " << 
    #endif
    spi_.write((uint8_t*)(&command), 1);
    #ifdef DEBUG_SPI
    std::cout << std::endl;
    #endif
  }

  template <typename C, typename T>
  void sendCommand(C command, const T& data)
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
    sleep_ms(sendCommandDelay_);
    #ifdef DEBUG_SPI
    std::cout << "Sent buffer len " << len << " ret: " << 
    #endif
    spi_.write(dataPtr, len);
    #ifdef DEBUG_SPI
    std::cout << std::endl;
    #endif
  }
};