#pragma once

#include <cstdint>

#include "koto/color.hpp"

namespace koto {
namespace hal {

// P3 64×32 RGB framebuffer. ESP32 present() is a log stub until a panel driver
// lands. Simulator paints the browser canvas from the same pixels.
class IMatrix {
 public:
  virtual ~IMatrix() = default;
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual void present(const Color* pixels, int width, int height) = 0;
};

}  // namespace hal
}  // namespace koto
