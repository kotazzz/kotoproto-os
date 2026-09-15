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

// Full-sat HSV wheel, h = 0..255. Integer only: six sextants, no float.
inline Color hue_rgb(std::uint8_t h) {
  const int sextant = (static_cast<int>(h) * 6) >> 8;
  const std::uint8_t f = static_cast<std::uint8_t>((static_cast<int>(h) * 6) & 0xFF);
  const std::uint8_t q = static_cast<std::uint8_t>(255 - f);
  switch (sextant) {
    case 0:
      return Color{255, f, 0};
    case 1:
      return Color{q, 255, 0};
    case 2:
      return Color{0, 255, f};
    case 3:
      return Color{0, q, 255};
    case 4:
      return Color{f, 0, 255};
    default:
      return Color{255, 0, q};
  }
}

}  // namespace koto
