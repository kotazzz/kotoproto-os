#pragma once

#include <cstdint>

namespace koto {
namespace hal {

// Monochrome SSD1306-compatible frame: 1 bit/pixel, MSB left, row-major.
// PIO HAL writes I2C SSD1306. IDF stub logs only.
class IOled {
 public:
  virtual ~IOled() = default;
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual void present(const std::uint8_t* packed_bits, int width, int height) = 0;
};

}  // namespace hal
}  // namespace koto
