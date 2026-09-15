#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

struct DinoSprite {
  const char* id;
  int width;
  int height;
  const std::uint8_t* rgb;
  std::size_t size;
};

extern const DinoSprite kDinoSprites[];
extern const int kDinoSpriteCount;

const DinoSprite* find_dino_sprite(std::string_view id);

}  // namespace assets
}  // namespace koto
