#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "koto/color.hpp"
#include "koto/face/transition.hpp"

namespace koto {
namespace assets {

enum class Kind : std::uint8_t { Classic, Special };
enum class Effect : std::uint8_t { None, Snarl, Dizzy, Wink, Randomize };

struct EmotionFrame {
  const std::uint8_t* rgb;
  std::size_t size;
  int duration_ms;
};

struct Emotion {
  const char* id;
  const char* label;
  const char* short_label;
  Kind kind;
  Effect effect;
  face::TransitionKind transition;
  Color accent;
  bool loop;
  bool allow_blink;
  bool allow_boop;
  const EmotionFrame* frames;
  int frame_count;
  const std::uint8_t* hud;
  std::size_t hud_size;
};

extern const Emotion kEmotions[];
extern const int kEmotionCount;
extern const char* const kFaceSets[3][8];
extern const char* const kAutoFaces[];
extern const int kAutoFaceCount;
extern const char* const kBootFaces[];
extern const int kBootFaceCount;

const Emotion* find_emotion(std::string_view id);
const Emotion* emotion_at(int index);
const char* kind_name(Kind kind);
const char* effect_name(Effect effect);

inline bool emotion_classic(const Emotion* emotion) {
  return emotion != nullptr && emotion->kind == Kind::Classic;
}

}  // namespace assets
}  // namespace koto
