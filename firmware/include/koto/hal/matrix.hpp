#pragma once

#include <cstdint>

#include "koto/color.hpp"

namespace koto {
namespace hal {

// P3 HUB75 RGB-панель. На железе present() уйдёт в DMA/I2S драйвер.
class IMatrix {
 public:
  virtual ~IMatrix() = default;
  virtual int width() const = 0;
  virtual int height() const = 0;
  virtual void present(const Color* pixels, int width, int height) = 0;
};

}  // namespace hal
}  // namespace koto
