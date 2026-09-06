#pragma once

#include <cstdint>

namespace koto {

// Display geometry. Hardware pinout is TBD and not yet tested on a real visor.

inline constexpr int kMatrixW = 64;
inline constexpr int kMatrixH = 32;
inline constexpr int kOledW = 128;
inline constexpr int kOledH = 64;

// P3 left half-face. The right panel is this bitmap mirrored on X.
inline constexpr int kFaceW = 64;
inline constexpr int kFaceH = 32;
inline constexpr int kEyeY0 = 0;
inline constexpr int kEyeH = 16;
inline constexpr int kEyeW = 32;
inline constexpr int kEyeLX = 0;
inline constexpr int kMouthY0 = 16;
inline constexpr int kMouthH = 16;

inline constexpr int kBoopTriggerCount = 4;
inline constexpr int kBoopTriggersMax = 6;
inline constexpr std::uint32_t kBoopGlitchMs = 1800;
inline constexpr std::uint32_t kStartupMs = 3000;
inline constexpr std::uint32_t kStartupFaceMs = 200;
inline constexpr std::uint32_t kDefaultFrameMs = 500;
inline constexpr std::uint32_t kTickMs = 33;

inline constexpr int kSettingCount = 14;
inline constexpr int kSettingStatus1 = 255;
inline constexpr int kSettingStatus2 = 254;

// Placeholder ESP32 wiring. Do not flash as-is: HAL is stubs until hardware lands.
namespace pins {
inline constexpr int kOledScl = -1;
inline constexpr int kOledSda = -1;
inline constexpr int kLedDin = -1;
inline constexpr int kFanPwm = -1;
inline constexpr int kMicAdc = -1;
inline constexpr int kBoopAdc = -1;
inline constexpr int kMatrixR1 = -1;
inline constexpr int kMatrixG1 = -1;
inline constexpr int kMatrixB1 = -1;
}  // namespace pins

}  // namespace koto
