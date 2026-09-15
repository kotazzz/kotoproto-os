#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

struct DvdSprite {
  const char* id;
  int width;
  int height;
  const std::uint8_t* pix;
  std::size_t size;
};

extern const DvdSprite kDvdSprites[];
extern const int kDvdSpriteCount;

const DvdSprite* find_dvd_sprite(std::string_view id);

}  // namespace assets
}  // namespace koto
