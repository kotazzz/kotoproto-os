#pragma once

#include <cstdint>

#include "koto/color.hpp"

namespace koto {
namespace hal {

inline constexpr int kLedRingCount = 12;
inline constexpr int kLedRingCopies = 2;

// One logical WS2812 ring of 12 LEDs. On hardware present() should be written
// twice so the second ring copies the first. ESP32 present() is a log stub.
class ILedRing {
 public:
  virtual ~ILedRing() = default;
  virtual int size() const = 0;
  virtual void present(const Color* leds, int count) = 0;
};

}  // namespace hal
}  // namespace koto
