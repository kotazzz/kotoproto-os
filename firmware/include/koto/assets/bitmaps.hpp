#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

struct Bitmap {
  const char* id;
  int width;
  int height;
  int stride;
  const std::uint8_t* data;
  std::size_t size;
};

const Bitmap* find_system(std::string_view id);
const Bitmap* find_icon(std::string_view id);

extern const Bitmap kSystem[];
extern const int kSystemCount;
extern const Bitmap kIcons[];
extern const int kIconCount;

}  // namespace assets
}  // namespace koto
