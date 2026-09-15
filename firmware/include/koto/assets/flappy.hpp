#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

struct FlappySprite {
  const char* id;
  int width;
  int height;
  const std::uint8_t* rgb;
  std::size_t size;
};

extern const FlappySprite kFlappySprites[];
extern const int kFlappySpriteCount;

const FlappySprite* find_flappy_sprite(std::string_view id);

}  // namespace assets
}  // namespace koto
