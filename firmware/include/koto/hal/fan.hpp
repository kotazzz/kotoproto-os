#pragma once

#include <cstdint>

namespace koto {
namespace hal {

// PWM-вентилятор 25 кГц, 8 бит. В симе — слайдер и вращающаяся крыльчатка.
class IFan {
 public:
  virtual ~IFan() = default;
  virtual void set_speed(std::uint8_t duty) = 0;
  virtual std::uint8_t speed() const = 0;
};

inline constexpr int kFanPwmHz = 25000;

}  // namespace hal
}  // namespace koto
