#pragma once

#include <cstdint>

namespace koto {

struct Color {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;

  static Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return Color{r, g, b};
  }

  static Color black() { return Color{0, 0, 0}; }
  static Color white() { return Color{255, 255, 255}; }
};

inline Color scale(Color c, std::uint8_t brightness) {
  return Color{
      static_cast<std::uint8_t>((c.r * brightness) / 255),
      static_cast<std::uint8_t>((c.g * brightness) / 255),
      static_cast<std::uint8_t>((c.b * brightness) / 255),
  };
}

}  // namespace koto
