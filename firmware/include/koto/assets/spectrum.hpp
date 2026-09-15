#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

struct SpectrumSprite {
  const char* id;
  int width;
  int height;
  const std::uint8_t* pix;
  std::size_t size;
};

extern const SpectrumSprite kSpectrumSprites[];
extern const int kSpectrumSpriteCount;

const SpectrumSprite* find_spectrum_sprite(std::string_view id);

}  // namespace assets
}  // namespace koto
