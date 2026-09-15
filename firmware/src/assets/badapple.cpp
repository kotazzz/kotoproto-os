#include "koto/assets/ba_player.hpp"

namespace koto {
namespace assets {

#ifndef ESP_PLATFORM
extern "C" {
extern const std::uint8_t kBadAppleBlob[];
extern const std::uint8_t kBadAppleBlobEnd[];
}

const std::uint8_t* badapple_blob() {
  return kBadAppleBlob;
}

std::size_t badapple_blob_size() {
  return static_cast<std::size_t>(kBadAppleBlobEnd - kBadAppleBlob);
}

#else

extern "C" {
extern const std::uint8_t _binary_badapple_ba1p_start[];
extern const std::uint8_t _binary_badapple_ba1p_end[];
}

const std::uint8_t* badapple_blob() {
  return _binary_badapple_ba1p_start;
}

std::size_t badapple_blob_size() {
  return static_cast<std::size_t>(_binary_badapple_ba1p_end - _binary_badapple_ba1p_start);
}

#endif

}  // namespace assets
}  // namespace koto
