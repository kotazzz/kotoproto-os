#pragma once

#include <cstdint>

namespace koto {
namespace gfx {

inline constexpr int kFontWidth = 5;
inline constexpr int kFontHeight = 7;
inline constexpr int kFontSpacing = 1;

// 7 bytes, one row each. Columns are bits 7..3 (MSB = left).
const std::uint8_t* glyph5x7(char ch);

inline bool glyph5x7_dot(const std::uint8_t* glyph, int col, int row) {
  if (glyph == nullptr || col < 0 || col >= kFontWidth || row < 0 || row >= kFontHeight) {
    return false;
  }
  return (glyph[row] & static_cast<std::uint8_t>(0x80 >> col)) != 0;
}

}  // namespace gfx
}  // namespace koto
