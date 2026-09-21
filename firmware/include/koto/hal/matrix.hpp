#pragma once

#include <cstdint>

#include "koto/color.hpp"

namespace koto {
namespace hal {

// P3 64×32 RGB framebuffer, two physical panels. panel 0 is left, 1 is right.
// PIO HAL drives HUB75 DMA. IDF stub logs only. Simulator paints the browser
// canvas from both presented buffers.
class IMatrix {
 public:
  virtual ~IMatrix() = default;
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual void present(const Color* pixels, int width, int height, int panel) = 0;
};

}  // namespace hal
}  // namespace koto
