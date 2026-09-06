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

const Bitmap* find_part(std::string_view kind, std::string_view id);
const Bitmap* find_hud(std::string_view id);
const Bitmap* find_system(std::string_view id);

extern const Bitmap kEyes[];
extern const Bitmap kMouths[];
extern const Bitmap kNoses[];
extern const Bitmap kOther[];
extern const Bitmap kHud[];
extern const Bitmap kSystem[];
extern const int kEyeCount;
extern const int kMouthCount;
extern const int kNoseCount;
extern const int kOtherCount;
extern const int kHudCount;
extern const int kSystemCount;

}  // namespace assets
}  // namespace koto
