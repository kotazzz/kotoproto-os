#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

struct BsodSprite {
  const char* id;
  int width;
  int height;
  const std::uint8_t* pix;
  std::size_t size;
};

extern const BsodSprite kBsodSprites[];
extern const int kBsodSpriteCount;

const BsodSprite* find_bsod_sprite(std::string_view id);

}  // namespace assets
}  // namespace koto
