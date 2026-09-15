#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

struct CasinoSprite {
  const char* id;
  int width;
  int height;
  const std::uint8_t* rgb;
  std::size_t size;
};

extern const CasinoSprite kCasinoSprites[];
extern const int kCasinoSpriteCount;

const CasinoSprite* find_casino_sprite(std::string_view id);

}  // namespace assets
}  // namespace koto
