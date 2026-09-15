#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace koto {
namespace assets {

inline constexpr std::uint32_t kBaMagic = 0x50314142u;  // 'BA1P' little-endian
inline constexpr int kBaWidth = 64;
inline constexpr int kBaHeight = 32;
inline constexpr int kBaBpf = 256;
inline constexpr std::uint8_t kBaRaw = 0xFFu;
inline constexpr std::uint8_t kBaMask = 0xFEu;
inline constexpr int kBaMaskBytes = 32;

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif
struct
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((packed))
#endif
    BaHeader {
  std::uint32_t magic;
  std::uint16_t width;
  std::uint16_t height;
  std::uint8_t fps;
  std::uint16_t frames;
  std::uint16_t bpf;
};
#if defined(_MSC_VER)
#pragma pack(pop)
#endif

struct BaPlayer {
  const std::uint8_t* base = nullptr;
  const std::uint8_t* p = nullptr;
  const std::uint8_t* end = nullptr;
  std::uint16_t frame_index = 0;
  std::uint16_t frame_count = 0;
  std::uint8_t fps = 0;
  std::uint8_t pixels[kBaBpf]{};
};

inline bool ba_init(BaPlayer& s, const std::uint8_t* data, std::uint32_t size) {
  if (data == nullptr || size < sizeof(BaHeader)) {
    return false;
  }
  const BaHeader* h = reinterpret_cast<const BaHeader*>(data);
  if (h->magic != kBaMagic || h->width != kBaWidth || h->height != kBaHeight || h->bpf != kBaBpf) {
    return false;
  }
  s.base = data;
  s.p = data + sizeof(BaHeader);
  s.end = data + size;
  s.frame_index = 0;
  s.frame_count = h->frames;
  s.fps = h->fps;
  std::memset(s.pixels, 0, kBaBpf);
  return true;
}

inline bool ba_next(BaPlayer& s) {
  if (s.p == nullptr || s.p >= s.end || s.frame_index >= s.frame_count) {
    return false;
  }
  const std::uint8_t n = *s.p++;
  if (n == kBaRaw) {
    if (s.p + kBaBpf > s.end) {
      return false;
    }
    std::memcpy(s.pixels, s.p, kBaBpf);
    s.p += kBaBpf;
  } else if (n == kBaMask) {
    if (s.p + kBaMaskBytes > s.end) {
      return false;
    }
    const std::uint8_t* mask = s.p;
    s.p += kBaMaskBytes;
    for (int i = 0; i < kBaBpf; ++i) {
      const std::uint8_t bit = static_cast<std::uint8_t>(0x80 >> (i & 7));
      if ((mask[i >> 3] & bit) == 0) {
        continue;
      }
      if (s.p >= s.end) {
        return false;
      }
      s.pixels[i] = *s.p++;
    }
  } else {
    if (s.p + static_cast<std::uint16_t>(n) * 2u > s.end) {
      return false;
    }
    for (std::uint8_t i = 0; i < n; ++i) {
      const std::uint8_t idx = *s.p++;
      s.pixels[idx] = *s.p++;
    }
  }
  ++s.frame_index;
  return true;
}

inline void ba_rewind(BaPlayer& s) {
  if (s.base == nullptr) {
    return;
  }
  s.p = s.base + sizeof(BaHeader);
  s.frame_index = 0;
  std::memset(s.pixels, 0, kBaBpf);
}

inline int ba_pixel(const BaPlayer& s, int x, int y) {
  if (x < 0 || y < 0 || x >= kBaWidth || y >= kBaHeight) {
    return 0;
  }
  const std::uint8_t b = s.pixels[static_cast<std::uint16_t>(y) * 8u + static_cast<std::uint8_t>(x >> 3)];
  return (b >> (7 - (x & 7))) & 1;
}

const std::uint8_t* badapple_blob();
std::size_t badapple_blob_size();

}  // namespace assets
}  // namespace koto
