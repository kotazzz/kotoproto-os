#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

// One P3 64x32 left half-face. The right panel is this bitmap flipped on X.

struct FaceFrame {
  const char* sequence;
  int index;
  int duration_ms;
  const std::uint8_t* data;
  std::size_t size;
  bool flip_mouth = false;
};

extern const FaceFrame kFaceFrames[];
extern const int kFaceFrameCount;

const FaceFrame* find_face_frame(std::string_view sequence, int index);
int count_sequence_frames(std::string_view sequence);

}  // namespace assets
}  // namespace koto
