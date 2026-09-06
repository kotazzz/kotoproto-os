#pragma once

namespace koto {
namespace gfx {

inline constexpr int kFontWidth = 5;
inline constexpr int kFontHeight = 7;
inline constexpr int kFontSpacing = 1;

// 35 символов '.'/'X', строки сверху вниз. nullptr — глифа нет.
const char* glyph5x7(char ch);

}  // namespace gfx
}  // namespace koto
