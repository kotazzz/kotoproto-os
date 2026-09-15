#include "koto/face/transition.hpp"

#include <algorithm>
#include <cmath>

#include "koto/config.hpp"

namespace koto {
namespace face {
namespace {

Color sample(const Color* src, int w, int h, int x, int y) {
  if (x < 0 || y < 0 || x >= w || y >= h) {
    return Color::black();
  }
  return src[y * w + x];
}

bool lit(Color c) {
  return (c.r | c.g | c.b) != 0;
}

Color mix(Color a, Color b, float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return Color{
      static_cast<std::uint8_t>(a.r + (b.r - a.r) * t),
      static_cast<std::uint8_t>(a.g + (b.g - a.g) * t),
      static_cast<std::uint8_t>(a.b + (b.b - a.b) * t),
  };
}

float ease_out_bounce(float t) {
  if (t < 1.0f / 2.75f) {
    return 7.5625f * t * t;
  }
  if (t < 2.0f / 2.75f) {
    t -= 1.5f / 2.75f;
    return 7.5625f * t * t + 0.75f;
  }
  if (t < 2.5f / 2.75f) {
    t -= 2.25f / 2.75f;
    return 7.5625f * t * t + 0.9375f;
  }
  t -= 2.625f / 2.75f;
  return 7.5625f * t * t + 0.984375f;
}

std::uint32_t hash(std::uint32_t x) {
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

void blit(gfx::Framebuffer& dst, const Color* src, int w, int h, int ox, int oy) {
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const Color c = src[y * w + x];
      if (lit(c)) {
        dst.set_pixel(x + ox, y + oy, c);
      }
    }
  }
}

void copy_rect(gfx::Framebuffer& dst, const Color* src, int w, int h, int x0, int y0, int rw, int rh) {
  for (int y = 0; y < rh; ++y) {
    for (int x = 0; x < rw; ++x) {
      dst.set_pixel(x0 + x, y0 + y, sample(src, w, h, x0 + x, y0 + y));
    }
  }
}

}  // namespace

const char* transition_name(TransitionKind kind) {
  switch (kind) {
    case TransitionKind::Blink:
      return "blink";
    case TransitionKind::Crossfade:
      return "crossfade";
    case TransitionKind::Drop:
      return "drop";
    case TransitionKind::Slide:
      return "slide";
    case TransitionKind::Glitch:
      return "glitch";
    case TransitionKind::Explode:
      return "explode";
    case TransitionKind::Fizz:
      return "fizz";
    case TransitionKind::DoomMelt:
      return "doomMelt";
    case TransitionKind::LosePower:
      return "losePower";
    case TransitionKind::Earthquake:
      return "earthquake";
    case TransitionKind::Shuffle:
      return "shuffle";
    default:
      return "none";
  }
}

std::uint32_t transition_duration_ms(TransitionKind kind) {
  switch (kind) {
    case TransitionKind::Blink:
      return 280;
    case TransitionKind::Crossfade:
      return 400;
    case TransitionKind::Drop:
      return 1000;
    case TransitionKind::Slide:
      return 900;
    case TransitionKind::Glitch:
      return 750;
    case TransitionKind::Explode:
      return 650;
    case TransitionKind::Fizz:
      return 400;
    case TransitionKind::DoomMelt:
      return 800;
    case TransitionKind::LosePower:
      return 500;
    case TransitionKind::Earthquake:
      return 500;
    case TransitionKind::Shuffle:
      return 1000;
    default:
      return 0;
  }
}

TransitionKind pick_transition(TransitionKind preferred, std::uint32_t rng, std::uint8_t rare_chance) {
  if (preferred != TransitionKind::Blink) {
    return preferred;
  }
  const TransitionKind rare[] = {
      TransitionKind::Drop,     TransitionKind::Slide,   TransitionKind::LosePower,
      TransitionKind::Glitch,   TransitionKind::Explode, TransitionKind::Fizz,
      TransitionKind::DoomMelt,
  };
  if (static_cast<std::uint8_t>(rng & 0xFFu) < rare_chance) {
    return rare[(rng >> 8) % 7];
  }
  return TransitionKind::Blink;
}

void apply_transition(gfx::Framebuffer& dst, const Color* from, const Color* to, int width, int height,
                      TransitionKind kind, float t, std::uint32_t rng_seed) {
  t = std::clamp(t, 0.0f, 1.0f);
  dst.clear(Color::black());

  if (kind == TransitionKind::None || t >= 1.0f) {
    blit(dst, to, width, height, 0, 0);
    return;
  }

  switch (kind) {
    case TransitionKind::Crossfade:
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          dst.set_pixel(x, y, mix(from[y * width + x], to[y * width + x], t));
        }
      }
      break;

    case TransitionKind::Blink: {
      // New face (eye, nose, mouth) is already on screen; then a lid wipe
      // plays over the left eye only. The eye is never kept from `from`.
      blit(dst, to, width, height, 0, 0);
      constexpr float kHold = 0.18f;
      if (t <= kHold) {
        break;
      }
      const float u = (t - kHold) / (1.0f - kHold);
      const float cover = u < 0.5f ? u * 2.0f : (1.0f - u) * 2.0f;
      const int rows = std::min(kEyeH, static_cast<int>(cover * static_cast<float>(kEyeH) + 0.5f));
      if (rows > 0) {
        dst.fill_rect(kEyeLX, kEyeY0, kEyeW, rows, Color::black());
      }
      break;
    }

    case TransitionKind::Drop: {
      if (t < 0.3f) {
        const int dy = static_cast<int>((t / 0.3f) * static_cast<float>(height));
        blit(dst, from, width, height, 0, dy);
      } else {
        const float u = ease_out_bounce((t - 0.3f) / 0.7f);
        const int dy = static_cast<int>((1.0f - u) * -static_cast<float>(height));
        blit(dst, to, width, height, 0, dy);
      }
      break;
    }

    case TransitionKind::Slide: {
      if (t < 0.4f) {
        const int dx = static_cast<int>((t / 0.4f) * static_cast<float>(width));
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width / 2; ++x) {
            dst.set_pixel(x - dx, y, sample(from, width, height, x, y));
          }
          for (int x = width / 2; x < width; ++x) {
            dst.set_pixel(x + dx, y, sample(from, width, height, x, y));
          }
        }
      } else {
        const float u = 1.0f - (t - 0.4f) / 0.6f;
        const int dx = static_cast<int>(u * u * static_cast<float>(width / 2));
        for (int y = 0; y < height; ++y) {
          for (int x = 0; x < width / 2; ++x) {
            dst.set_pixel(x - dx, y, sample(to, width, height, x, y));
          }
          for (int x = width / 2; x < width; ++x) {
            dst.set_pixel(x + dx, y, sample(to, width, height, x, y));
          }
        }
      }
      break;
    }

    case TransitionKind::Glitch: {
      blit(dst, to, width, height, 0, 0);
      const float amp = (1.0f - t) * 4.0f;
      const int chance = static_cast<int>((1.0f - t) * 80.0f);
      for (int y = 0; y < height; ++y) {
        if (static_cast<int>(hash(rng_seed + static_cast<std::uint32_t>(y * 31)) % 100u) >= chance) {
          continue;
        }
        const int shift = static_cast<int>(hash(rng_seed + static_cast<std::uint32_t>(y * 17)) %
                                           (static_cast<std::uint32_t>(amp) + 1u));
        const int dir = (hash(rng_seed + static_cast<std::uint32_t>(y * 9)) & 1u) != 0 ? shift : -shift;
        for (int x = 0; x < width; ++x) {
          dst.set_pixel(x, y, sample(to, width, height, x - dir, y));
        }
      }
      break;
    }

    case TransitionKind::Explode: {
      blit(dst, to, width, height, 0, 0);
      const float cx = static_cast<float>(width) * 0.5f;
      const float cy = static_cast<float>(height) * 0.5f;
      const float expand = 1.0f + t * 2.4f;
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const Color c = from[y * width + x];
          if (!lit(c)) {
            continue;
          }
          const int nx = static_cast<int>(cx + (static_cast<float>(x) - cx) * expand);
          const int ny = static_cast<int>(cy + (static_cast<float>(y) - cy) * expand);
          if ((hash(rng_seed + x * 13u + y * 7u) & 255u) > static_cast<unsigned>(t * 220)) {
            dst.set_pixel(nx, ny, scale(c, static_cast<std::uint8_t>((1.0f - t) * 255)));
          }
        }
      }
      break;
    }

    case TransitionKind::Fizz: {
      const int dy = static_cast<int>(-t * 10.0f);
      blit(dst, from, width, height, 0, dy);
      blit(dst, to, width, height, 0, static_cast<int>((1.0f - t) * 8.0f));
      for (int i = 0; i < width; ++i) {
        if ((hash(rng_seed + static_cast<std::uint32_t>(i) * 19u) % 20u) != 0) {
          continue;
        }
        const int y = height - 1 - static_cast<int>(t * 18.0f + (hash(rng_seed + i) & 7));
        dst.set_pixel(i, y, Color::white());
      }
      break;
    }

    case TransitionKind::DoomMelt: {
      blit(dst, to, width, height, 0, 0);
      for (int x = 0; x < width; ++x) {
        const int speed = 8 + static_cast<int>(hash(rng_seed + static_cast<std::uint32_t>(x) * 31u) % 22u);
        const int fall = static_cast<int>(t * static_cast<float>(speed + height));
        for (int y = 0; y < height; ++y) {
          const Color c = from[y * width + x];
          if (lit(c)) {
            dst.set_pixel(x, y + fall, c);
          }
        }
      }
      break;
    }

    case TransitionKind::LosePower: {
      const int visible = std::max(1, static_cast<int>(height * (1.0f - t)));
      const int y0 = (height - visible) / 2;
      const std::uint8_t dim = static_cast<std::uint8_t>((1.0f - t) * 255);
      for (int y = 0; y < visible; ++y) {
        if ((y & 1) != 0 && t > 0.2f) {
          continue;
        }
        const int src_y = y * height / visible;
        for (int x = 0; x < width; ++x) {
          dst.set_pixel(x, y0 + y, scale(sample(from, width, height, x, src_y), dim));
        }
      }
      break;
    }

    case TransitionKind::Earthquake: {
      const float amp = (1.0f - t);
      const int ox = static_cast<int>(std::sin(t * 38.0f) * amp * 6.0f);
      const int oy = static_cast<int>(std::cos(t * 27.0f) * amp * 3.0f);
      blit(dst, t < 0.45f ? from : to, width, height, ox, oy);
      break;
    }

    case TransitionKind::Shuffle: {
      const int tile = 4;
      for (int y = 0; y < height; y += tile) {
        for (int x = 0; x < width; x += tile) {
          const std::uint32_t h = hash(rng_seed + static_cast<std::uint32_t>(x * 13 + y * 7));
          const bool from_tile = (h & 255u) > static_cast<std::uint32_t>(t * 255.0f);
          const int sx = static_cast<int>((h >> 8) % static_cast<std::uint32_t>(width / tile)) * tile;
          const int sy = static_cast<int>((h >> 16) % static_cast<std::uint32_t>(std::max(1, height / tile))) * tile;
          const Color* src = from_tile ? from : to;
          const int src_x = from_tile ? sx : x;
          const int src_y = from_tile ? sy : y;
          for (int ty = 0; ty < tile && y + ty < height; ++ty) {
            for (int tx = 0; tx < tile && x + tx < width; ++tx) {
              dst.set_pixel(x + tx, y + ty, sample(src, width, height, src_x + tx, src_y + ty));
            }
          }
        }
      }
      break;
    }

    default:
      blit(dst, to, width, height, 0, 0);
      break;
  }
}

}  // namespace face
}  // namespace koto
