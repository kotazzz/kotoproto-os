#pragma once

#include <cstdint>
#include <string_view>

#include "koto/gfx/framebuffer.hpp"

namespace koto {
namespace face {

enum class TransitionKind : std::uint8_t {
  None = 0,
  Blink,
  Crossfade,
  Drop,
  Slide,
  Glitch,
  Explode,
  Fizz,
  DoomMelt,
  LosePower,
  Earthquake,
  Shuffle,
};

const char* transition_name(TransitionKind kind);
std::uint32_t transition_duration_ms(TransitionKind kind);
TransitionKind transition_for_sequence(std::string_view sequence);
TransitionKind pick_transition(TransitionKind preferred, std::uint32_t rng, std::uint8_t rare_chance);

void apply_transition(gfx::Framebuffer& dst, const Color* from, const Color* to, int width, int height,
                      TransitionKind kind, float t, std::uint32_t rng_seed);

}  // namespace face
}  // namespace koto
