#pragma once

#include <cstdint>

namespace koto {
namespace hal {

// PWM fan 25 kHz, 8-bit duty. Settings menu writes it. Simulator stores duty
// only (no /api/fan). ESP32 HAL keeps RAM duty, no PWM yet.
class IFan {
 public:
  virtual ~IFan() = default;
  virtual void set_speed(std::uint8_t duty) = 0;
  virtual std::uint8_t speed() const = 0;
};

inline constexpr int kFanPwmHz = 25000;

}  // namespace hal
}  // namespace koto
