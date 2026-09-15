#include "koto/gfx/oled_canvas.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

#include "koto/gfx/font5x7.hpp"

namespace koto {
namespace gfx {

OledCanvas::OledCanvas(int width, int height)
    : width_(width),
      height_(height),
      bytes_per_row_((width + 7) / 8),
      bits_(static_cast<std::size_t>(bytes_per_row_ * height), 0) {}

void OledCanvas::clear() {
  for (std::uint8_t& byte : bits_) {
    byte = 0;
  }
}

void OledCanvas::set_pixel(int x, int y, bool on) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    return;
  }
  const int index = y * bytes_per_row_ + (x / 8);
  const std::uint8_t mask = static_cast<std::uint8_t>(0x80 >> (x % 8));
  if (on) {
    bits_[static_cast<std::size_t>(index)] =
        static_cast<std::uint8_t>(bits_[static_cast<std::size_t>(index)] | mask);
  } else {
    bits_[static_cast<std::size_t>(index)] =
        static_cast<std::uint8_t>(bits_[static_cast<std::size_t>(index)] & static_cast<std::uint8_t>(~mask));
  }
}

bool OledCanvas::get_pixel(int x, int y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    return false;
  }
  const int index = y * bytes_per_row_ + (x / 8);
  const std::uint8_t mask = static_cast<std::uint8_t>(0x80 >> (x % 8));
  return (bits_[static_cast<std::size_t>(index)] & mask) != 0;
}

void OledCanvas::fill_rect(int x, int y, int w, int h, bool on) {
  for (int row = y; row < y + h; ++row) {
    for (int col = x; col < x + w; ++col) {
      set_pixel(col, row, on);
    }
  }
}

void OledCanvas::invert_rect(int x, int y, int w, int h) {
  for (int row = y; row < y + h; ++row) {
    for (int col = x; col < x + w; ++col) {
      set_pixel(col, row, !get_pixel(col, row));
    }
  }
}

void OledCanvas::draw_hline(int x, int y, int w, bool on) {
  for (int col = 0; col < w; ++col) {
    set_pixel(x + col, y, on);
  }
}

void OledCanvas::draw_vline(int x, int y, int h, bool on) {
  for (int row = 0; row < h; ++row) {
    set_pixel(x, y + row, on);
  }
}

void OledCanvas::draw_rect(int x, int y, int w, int h, bool on) {
  if (w <= 0 || h <= 0) {
    return;
  }
  draw_hline(x, y, w, on);
  draw_hline(x, y + h - 1, w, on);
  draw_vline(x, y, h, on);
  draw_vline(x + w - 1, y, h, on);
}

void OledCanvas::draw_corners(int x, int y, int w, int h, int len, bool on) {
  if (w <= 0 || h <= 0 || len <= 0) {
    return;
  }
  draw_hline(x, y, len, on);
  draw_vline(x, y, len, on);
  draw_hline(x + w - len, y, len, on);
  draw_vline(x + w - 1, y, len, on);
  draw_hline(x, y + h - 1, len, on);
  draw_vline(x, y + h - len, len, on);
  draw_hline(x + w - len, y + h - 1, len, on);
  draw_vline(x + w - 1, y + h - len, len, on);
}

void OledCanvas::draw_line(int x0, int y0, int x1, int y1, bool on) {
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    set_pixel(x0, y0, on);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void OledCanvas::blit_bitmap_1bpp(int x, int y, int bitmap_w, int bitmap_h, const std::uint8_t* data,
                                  std::size_t size, int dst_w, int dst_h, bool on, bool invert) {
  if (data == nullptr || bitmap_w <= 0 || bitmap_h <= 0 || dst_w <= 0 || dst_h <= 0) {
    return;
  }
  const int bytes_per_row = (bitmap_w + 7) / 8;
  for (int row = 0; row < dst_h; ++row) {
    const int src_y = row * bitmap_h / dst_h;
    for (int col = 0; col < dst_w; ++col) {
      const int src_x = col * bitmap_w / dst_w;
      const std::size_t index = static_cast<std::size_t>(src_y * bytes_per_row + (src_x / 8));
      if (index >= size) {
        return;
      }
      const std::uint8_t bit = static_cast<std::uint8_t>(0x80 >> (src_x & 7));
      const bool lit = (data[index] & bit) != 0;
      if (lit != invert) {
        set_pixel(x + col, y + row, on);
      }
    }
  }
}

void OledCanvas::blit_packed_shift(const std::uint8_t* src, int dx) {
  if (src == nullptr) {
    return;
  }
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      const int sx = x - dx;
      if (sx < 0 || sx >= width_) {
        continue;
      }
      const int index = y * bytes_per_row_ + (sx / 8);
      const std::uint8_t mask = static_cast<std::uint8_t>(0x80 >> (sx % 8));
      if ((src[static_cast<std::size_t>(index)] & mask) != 0) {
        set_pixel(x, y, true);
      }
    }
  }
}

int OledCanvas::draw_char(int x, int y, char ch, bool on, int scale) {
  const char* glyph = glyph5x7(ch);
  if (glyph == nullptr) {
    glyph = glyph5x7('?');
  }
  if (glyph == nullptr) {
    return (kFontWidth + kFontSpacing) * std::max(1, scale);
  }
  const int s = std::max(1, scale);
  for (int row = 0; row < kFontHeight; ++row) {
    for (int col = 0; col < kFontWidth; ++col) {
      if (glyph[row * kFontWidth + col] == 'X') {
        fill_rect(x + col * s, y + row * s, s, s, on);
      }
    }
  }
  return (kFontWidth + kFontSpacing) * s;
}

int OledCanvas::draw_text(int x, int y, std::string_view text, bool on, int scale) {
  int cursor = x;
  for (char ch : text) {
    cursor += draw_char(cursor, y, ch, on, scale);
  }
  return cursor - x;
}

}  // namespace gfx
}  // namespace koto
