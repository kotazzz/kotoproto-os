#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

struct TetrisSprite {
  const char* id;
  int width;
  int height;
  const std::uint8_t* pix;
  std::size_t size;
};

extern const TetrisSprite kTetrisSprites[];
extern const int kTetrisSpriteCount;

const TetrisSprite* find_tetris_sprite(std::string_view id);

}  // namespace assets
}  // namespace koto
