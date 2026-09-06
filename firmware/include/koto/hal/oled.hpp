#pragma once

#include <cstdint>

namespace koto {
namespace hal {

// Монохромный SSD1306-совместимый кадр: 1 бит/пиксель, MSB слева, по строкам.
class IOled {
 public:
  virtual ~IOled() = default;
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual void present(const std::uint8_t* packed_bits, int width, int height) = 0;
};

}  // namespace hal
}  // namespace koto
