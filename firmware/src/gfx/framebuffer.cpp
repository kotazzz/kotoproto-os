#include "koto/gfx/framebuffer.hpp"

#include <algorithm>
#include <vector>

#include "koto/gfx/font5x7.hpp"
#include "koto/gfx/pix.hpp"

namespace koto {
namespace gfx {

Framebuffer::Framebuffer(int width, int height)
    : width_(width),
      height_(height),
      pixels_(static_cast<std::size_t>(width * height)),
      scratch_(static_cast<std::size_t>(width * height)) {}

void Framebuffer::clear(Color color) {
  for (Color& pixel : pixels_) {
    pixel = color;
  }
}

void Framebuffer::put_pixel(int x, int y, Color color) {
  pixels_[static_cast<std::size_t>(y * width_ + x)] = color;
}

void Framebuffer::set_pixel(int x, int y, Color color) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    return;
  }
  put_pixel(x, y, color);
}

Color Framebuffer::get_pixel(int x, int y) const {
  if (x < 0 || y < 0 || x >= width_ || y >= height_) {
    return Color::black();
  }
  return pixels_[static_cast<std::size_t>(y * width_ + x)];
}

void Framebuffer::fill_rect(int x, int y, int w, int h, Color color) {
  const int x0 = std::max(0, x);
  const int y0 = std::max(0, y);
  const int x1 = std::min(width_, x + w);
  const int y1 = std::min(height_, y + h);
  for (int row = y0; row < y1; ++row) {
    Color* line = pixels_.data() + row * width_ + x0;
    for (int col = x0; col < x1; ++col) {
      *line++ = color;
    }
  }
}

void Framebuffer::draw_rect(int x, int y, int w, int h, Color color) {
  draw_hline(x, y, w, color);
  draw_hline(x, y + h - 1, w, color);
  for (int row = y; row < y + h; ++row) {
    set_pixel(x, row, color);
    set_pixel(x + w - 1, row, color);
  }
}

void Framebuffer::draw_hline(int x, int y, int w, Color color) {
  if (y < 0 || y >= height_ || w <= 0) {
    return;
  }
  const int x0 = std::max(0, x);
  const int x1 = std::min(width_, x + w);
  Color* line = pixels_.data() + y * width_ + x0;
  for (int col = x0; col < x1; ++col) {
    *line++ = color;
  }
}

void Framebuffer::blit_rgb(int x, int y, int bitmap_w, int bitmap_h, const std::uint8_t* rgb,
                            std::size_t size, bool skip_black) {
  if (rgb == nullptr || bitmap_w <= 0 || bitmap_h <= 0) {
    return;
  }
  const std::size_t need = static_cast<std::size_t>(bitmap_w * bitmap_h * 3);
  if (size < need) {
    return;
  }
  const int x0 = std::max(0, x);
  const int y0 = std::max(0, y);
  const int x1 = std::min(width_, x + bitmap_w);
  const int y1 = std::min(height_, y + bitmap_h);
  for (int row = y0; row < y1; ++row) {
    const int src_y = row - y;
    Color* dst = pixels_.data() + row * width_ + x0;
    for (int col = x0; col < x1; ++col) {
      const int src_x = col - x;
      const std::size_t i = static_cast<std::size_t>((src_y * bitmap_w + src_x) * 3);
      if (skip_black && (rgb[i] | rgb[i + 1] | rgb[i + 2]) == 0) {
        ++dst;
        continue;
      }
      *dst++ = Color{rgb[i], rgb[i + 1], rgb[i + 2]};
    }
  }
}

void Framebuffer::blit_pix(int x, int y, int bitmap_w, int bitmap_h, const std::uint8_t* pix,
                           std::size_t size, bool skip_black, Color tint, int clip_y0, int clip_y1) {
  if (pix == nullptr || bitmap_w <= 0 || bitmap_h <= 0) {
    return;
  }
  const bool clip = clip_y1 > clip_y0;
  const bool tint_on = tint.r != 255 || tint.g != 255 || tint.b != 255;
  pix_each(pix, size, bitmap_w, bitmap_h, [&](int src_x, int src_y, Color c) {
    const int dx = x + src_x;
    const int dy = y + src_y;
    if (dx < 0 || dy < 0 || dx >= width_ || dy >= height_) {
      return true;
    }
    if (clip && (dy < clip_y0 || dy >= clip_y1)) {
      return true;
    }
    if (tint_on) {
      c.r = static_cast<std::uint8_t>((c.r * tint.r) / 255);
      c.g = static_cast<std::uint8_t>((c.g * tint.g) / 255);
      c.b = static_cast<std::uint8_t>((c.b * tint.b) / 255);
    }
    if (skip_black && (c.r | c.g | c.b) == 0) {
      return true;
    }
    put_pixel(dx, dy, c);
    return true;
  });
}

void Framebuffer::expand_column_y(int x, int y0, int h, int amount) {
  if (amount <= 0 || h <= 0 || x < 0 || x >= width_) {
    return;
  }
  if (static_cast<std::size_t>(h) > scratch_.size()) {
    return;
  }
  Color* col = scratch_.data();
  for (int i = 0; i < h; ++i) {
    col[i] = get_pixel(x, y0 + i);
  }
  const int above = amount / 2;
  const int below = amount - above;
  auto lit = [](Color c) { return (c.r | c.g | c.b) != 0; };
  for (int i = 0; i < h; ++i) {
    if (!lit(col[i])) {
      continue;
    }
    const Color c = col[i];
    for (int j = 1; j <= above; ++j) {
      if (i - j < 0) {
        break;
      }
      set_pixel(x, y0 + i - j, c);
    }
    for (int j = 1; j <= below; ++j) {
      if (i + j >= h) {
        break;
      }
      set_pixel(x, y0 + i + j, c);
    }
  }
}

void Framebuffer::rotate_square_cw(int x, int y, int size, int turns) {
  turns %= 4;
  if (turns < 0) {
    turns += 4;
  }
  if (turns == 0 || size <= 1) {
    return;
  }
  const std::size_t n = static_cast<std::size_t>(size * size);
  if (n > scratch_.size()) {
    return;
  }
  for (int row = 0; row < size; ++row) {
    for (int col = 0; col < size; ++col) {
      scratch_[static_cast<std::size_t>(row * size + col)] = get_pixel(x + col, y + row);
    }
  }
  auto src = [&](int col, int row) { return scratch_[static_cast<std::size_t>(row * size + col)]; };
  for (int row = 0; row < size; ++row) {
    for (int col = 0; col < size; ++col) {
      Color c = Color::black();
      if (turns == 1) {
        c = src(size - 1 - row, col);
      } else if (turns == 2) {
        c = src(size - 1 - col, size - 1 - row);
      } else {
        c = src(row, size - 1 - col);
      }
      set_pixel(x + col, y + row, c);
    }
  }
}

void Framebuffer::translate_rect(int x, int y, int w, int h, int dx, int dy) {
  if (dx == 0 && dy == 0) {
    return;
  }
  const std::size_t n = static_cast<std::size_t>(w * h);
  if (w <= 0 || h <= 0 || n > scratch_.size()) {
    return;
  }
  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      scratch_[static_cast<std::size_t>(row * w + col)] = get_pixel(x + col, y + row);
      set_pixel(x + col, y + row, Color::black());
    }
  }
  for (int row = 0; row < h; ++row) {
    for (int col = 0; col < w; ++col) {
      set_pixel(x + col + dx, y + row + dy, scratch_[static_cast<std::size_t>(row * w + col)]);
    }
  }
}

void Framebuffer::hue_cycle_lit(std::uint8_t phase, std::uint16_t mix) {
  if (mix == 0 || width_ <= 0 || height_ <= 0) {
    return;
  }
  if (mix > 256) {
    mix = 256;
  }
  Color* px = pixels_.data();
  const int w = width_;
  const int h = height_;
  const std::uint16_t inv = static_cast<std::uint16_t>(256 - mix);
  Color wheel[64];
  const int cols = w < 64 ? w : 64;
  for (int x = 0; x < cols; ++x) {
    wheel[x] = hue_rgb(static_cast<std::uint8_t>(static_cast<std::uint32_t>(x) * 256u /
                                                static_cast<std::uint32_t>(w) +
                                                phase));
  }
  for (int y = 0; y < h; ++y) {
    Color* row = px + y * w;
    for (int x = 0; x < w; ++x) {
      Color& c = row[x];
      const std::uint8_t v = c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b);
      if (v == 0) {
        continue;
      }
      const Color rain = x < cols ? wheel[x]
                                   : hue_rgb(static_cast<std::uint8_t>(
                                         static_cast<std::uint32_t>(x) * 256u /
                                             static_cast<std::uint32_t>(w) +
                                         phase));
      const std::uint8_t tr =
          static_cast<std::uint8_t>((static_cast<std::uint16_t>(rain.r) * v) / 255);
      const std::uint8_t tg =
          static_cast<std::uint8_t>((static_cast<std::uint16_t>(rain.g) * v) / 255);
      const std::uint8_t tb =
          static_cast<std::uint8_t>((static_cast<std::uint16_t>(rain.b) * v) / 255);
      c.r = static_cast<std::uint8_t>((static_cast<std::uint16_t>(c.r) * inv +
                                       static_cast<std::uint16_t>(tr) * mix) >>
                                      8);
      c.g = static_cast<std::uint8_t>((static_cast<std::uint16_t>(c.g) * inv +
                                       static_cast<std::uint16_t>(tg) * mix) >>
                                      8);
      c.b = static_cast<std::uint8_t>((static_cast<std::uint16_t>(c.b) * inv +
                                       static_cast<std::uint16_t>(tb) * mix) >>
                                      8);
    }
  }
}

void Framebuffer::glitch_rows(int y0, int h, int amplitude, std::uint32_t seed) {
  if (amplitude <= 0 || h <= 0 || scratch_.size() < static_cast<std::size_t>(width_)) {
    return;
  }
  auto hash = [](std::uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
  };
  Color* row = scratch_.data();
  for (int y = y0; y < y0 + h && y < height_; ++y) {
    const int shift =
        static_cast<int>(hash(seed + static_cast<std::uint32_t>(y * 17)) % (amplitude * 2 + 1)) -
        amplitude;
    for (int x = 0; x < width_; ++x) {
      row[x] = get_pixel(x, y);
    }
    for (int x = 0; x < width_; ++x) {
      const int sx = x - shift;
      Color c = Color::black();
      if (sx >= 0 && sx < width_) {
        c = row[sx];
      }
      put_pixel(x, y, c);
    }
  }
}

int Framebuffer::draw_char(int x, int y, char ch, Color color) {
  const std::uint8_t* glyph = glyph5x7(ch);
  if (glyph == nullptr) {
    return kFontWidth + kFontSpacing;
  }
  for (int row = 0; row < kFontHeight; ++row) {
    for (int col = 0; col < kFontWidth; ++col) {
      if (glyph5x7_dot(glyph, col, row)) {
        set_pixel(x + col, y + row, color);
      }
    }
  }
  return kFontWidth + kFontSpacing;
}

int Framebuffer::draw_text(int x, int y, std::string_view text, Color color) {
  int cursor = x;
  for (char ch : text) {
    cursor += draw_char(cursor, y, ch, color);
  }
  return cursor - x;
}

}  // namespace gfx
}  // namespace koto
