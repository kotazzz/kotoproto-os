#pragma once

#include <cstdint>

#include "koto/color.hpp"

namespace koto {
namespace hal {

inline constexpr int kLedRingCount = 12;
inline constexpr int kLedRingCopies = 2;

// Одно логическое кольцо WS2812 на 12 светодиодов.
// На железе present() пишется дважды — второе кольцо дублирует первое.
class ILedRing {
 public:
  virtual ~ILedRing() = default;
  virtual int size() const = 0;
  virtual void present(const Color* leds, int count) = 0;
};

}  // namespace hal
}  // namespace koto
