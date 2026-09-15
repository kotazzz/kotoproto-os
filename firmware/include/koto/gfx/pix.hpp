#pragma once

#include <cstddef>
#include <cstdint>

#include "koto/color.hpp"

namespace koto {
namespace gfx {

inline constexpr std::uint8_t kPixFlagRle = 1;
inline constexpr std::uint8_t kPixFlagRaw = 2;

inline int pix_stored_colors(std::uint8_t stored) {
  return stored == 0 ? 256 : static_cast<int>(stored);
}

inline int pix_bpp(int ncolors) {
  if (ncolors <= 2) {
    return 1;
  }
  if (ncolors <= 4) {
    return 2;
  }
  if (ncolors <= 16) {
    return 4;
  }
  return 8;
}

inline Color pix_palette_color(const std::uint8_t* palette, int index, int ncolors) {
  if (palette == nullptr || index < 0 || index >= ncolors) {
    return Color::black();
  }
  const std::size_t o = static_cast<std::size_t>(index) * 3u;
  return Color{palette[o], palette[o + 1], palette[o + 2]};
}

inline std::uint8_t pix_packed_index(const std::uint8_t* data, std::size_t size, int bpp,
                                     int pixel) {
  if (data == nullptr || bpp <= 0 || pixel < 0) {
    return 0;
  }
  const int bitpos = pixel * bpp;
  const std::size_t byte_i = static_cast<std::size_t>(bitpos / 8);
  if (byte_i >= size) {
    return 0;
  }
  const int shift = 8 - bpp - (bitpos % 8);
  if (shift < 0) {
    return 0;
  }
  const std::uint8_t mask = static_cast<std::uint8_t>((1 << bpp) - 1);
  return static_cast<std::uint8_t>((data[byte_i] >> shift) & mask);
}

template <typename Fn>
bool pix_each(const std::uint8_t* blob, std::size_t size, int w, int h, Fn&& fn) {
  if (blob == nullptr || size < 2 || w <= 0 || h <= 0) {
    return false;
  }
  const std::uint8_t flags = blob[1];
  const int pixels = w * h;
  if ((flags & kPixFlagRaw) != 0) {
    if (size < 2u + static_cast<std::size_t>(pixels) * 3u) {
      return false;
    }
    const std::uint8_t* rgb = blob + 2;
    for (int i = 0; i < pixels; ++i) {
      const std::size_t o = static_cast<std::size_t>(i) * 3u;
      if (!fn(i % w, i / w, Color{rgb[o], rgb[o + 1], rgb[o + 2]})) {
        return true;
      }
    }
    return true;
  }

  const int ncolors = pix_stored_colors(blob[0]);
  const std::size_t pal_bytes = static_cast<std::size_t>(ncolors) * 3u;
  if (size < 2u + pal_bytes) {
    return false;
  }
  const std::uint8_t* palette = blob + 2;
  const std::uint8_t* data = palette + pal_bytes;
  const std::size_t data_size = size - 2u - pal_bytes;

  if ((flags & kPixFlagRle) != 0) {
    int i = 0;
    std::size_t p = 0;
    while (i < pixels) {
      if (p + 1 >= data_size) {
        return false;
      }
      const int count = data[p];
      const int index = data[p + 1];
      p += 2;
      if (count <= 0) {
        return false;
      }
      const Color c = pix_palette_color(palette, index, ncolors);
      for (int n = 0; n < count && i < pixels; ++n, ++i) {
        if (!fn(i % w, i / w, c)) {
          return true;
        }
      }
    }
    return true;
  }

  const int bpp = pix_bpp(ncolors);
  for (int i = 0; i < pixels; ++i) {
    const int index = pix_packed_index(data, data_size, bpp, i);
    if (!fn(i % w, i / w, pix_palette_color(palette, index, ncolors))) {
      return true;
    }
  }
  return true;
}

inline Color pix_at(const std::uint8_t* blob, std::size_t size, int w, int h, int x, int y) {
  if (blob == nullptr || size < 2 || x < 0 || y < 0 || x >= w || y >= h) {
    return Color::black();
  }
  const int pixel = y * w + x;
  const std::uint8_t flags = blob[1];
  if ((flags & kPixFlagRaw) != 0) {
    const std::size_t o = 2u + static_cast<std::size_t>(pixel) * 3u;
    if (o + 2 >= size) {
      return Color::black();
    }
    return Color{blob[o], blob[o + 1], blob[o + 2]};
  }
  const int ncolors = pix_stored_colors(blob[0]);
  const std::size_t pal_bytes = static_cast<std::size_t>(ncolors) * 3u;
  if (size < 2u + pal_bytes) {
    return Color::black();
  }
  const std::uint8_t* palette = blob + 2;
  const std::uint8_t* data = palette + pal_bytes;
  const std::size_t data_size = size - 2u - pal_bytes;
  if ((flags & kPixFlagRle) != 0) {
    int i = 0;
    std::size_t p = 0;
    while (i <= pixel && p + 1 < data_size) {
      const int count = data[p];
      const int index = data[p + 1];
      p += 2;
      if (count <= 0) {
        break;
      }
      if (pixel < i + count) {
        return pix_palette_color(palette, index, ncolors);
      }
      i += count;
    }
    return Color::black();
  }
  const int bpp = pix_bpp(ncolors);
  const int index = pix_packed_index(data, data_size, bpp, pixel);
  return pix_palette_color(palette, index, ncolors);
}

inline bool pix_lit_at(const std::uint8_t* blob, std::size_t size, int w, int h, int x, int y) {
  const Color c = pix_at(blob, size, w, h, x, y);
  return (c.r | c.g | c.b) != 0;
}

inline int pix_lit_count(const std::uint8_t* blob, std::size_t size, int w, int h) {
  int lit = 0;
  pix_each(blob, size, w, h, [&](int, int, Color c) {
    if ((c.r | c.g | c.b) != 0) {
      ++lit;
    }
    return true;
  });
  return lit;
}

inline bool pix_any_lit(const std::uint8_t* blob, std::size_t size, int w, int h) {
  bool lit = false;
  pix_each(blob, size, w, h, [&](int, int, Color c) {
    if ((c.r | c.g | c.b) != 0) {
      lit = true;
      return false;
    }
    return true;
  });
  return lit;
}

}  // namespace gfx
}  // namespace koto
