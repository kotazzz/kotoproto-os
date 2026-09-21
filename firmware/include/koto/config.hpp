#pragma once

#include <cstdint>

namespace koto {

// Display geometry. GPIO is the LILYGO T8 V1.8 wiring from
// esp32-tools/docs/t8_visor_wiring.html. Do not invent extra pins.

inline constexpr int kMatrixW = 64;
inline constexpr int kMatrixH = 32;
inline constexpr int kMatrixPanels = 2;
inline constexpr int kOledW = 128;
inline constexpr int kOledH = 64;
// Dual-color SSD1306: rows 0..15 are physically yellow, 16..63 blue.
// The panel is still 1bpp; firmware cannot tint a region yellow or blue.
inline constexpr int kOledYellowH = 16;

// P3 left half-face. Firmware composes this 64×32, then presents two panels:
// faces X-mirror onto the right unless Emotion::mirror is false or the scene
// is an arcade game (copy, so text stays readable). One RGB sprite is the
// whole left half. Runtime overlays clip these regions after blit: blink
// paints the left eye only, microphone/snarl stretch the mouth. The default
// Blink face transition shows the new eye immediately, then wipes only that
// eye. Snarl follows mouth_enabled. Gyro nudge always translates the full
// framebuffer (yaw is stored, not used).
inline constexpr int kFaceW = 64;
inline constexpr int kFaceH = 32;
inline constexpr int kEyeLX = 0;
inline constexpr int kEyeY0 = 0;
inline constexpr int kEyeW = 32;
inline constexpr int kEyeH = 16;
inline constexpr int kNoseX = 48;
inline constexpr int kNoseY0 = 0;
inline constexpr int kNoseW = 16;
inline constexpr int kNoseH = 16;
inline constexpr int kMouthY0 = 16;
inline constexpr int kMouthH = 16;
inline constexpr int kHudThumbW = 42;
inline constexpr int kHudThumbH = 16;
inline constexpr int kHudCornerLen = 3;
inline constexpr int kAutoVisorX = 4;
inline constexpr int kAutoVisorY = 26;
inline constexpr int kAutoThumbX = 12;
inline constexpr int kAutoThumbY = 42;

inline constexpr int kSplashVisorX = 4;
inline constexpr int kSplashVisorY = 5;
inline constexpr int kSplashLogoX = 77;
inline constexpr int kSplashLogoY = 6;

inline constexpr int kBoopTriggerCount = 4;
inline constexpr int kBoopTriggersMax = 6;
inline constexpr std::uint32_t kBoopGlitchMs = 1800;
// After this hold, the Boop replacement face gets a hue cycle.
inline constexpr std::uint32_t kBoopHueHoldMs = 1800;
inline constexpr std::uint32_t kBoopHueRampMs = 2500;
inline constexpr std::uint32_t kBoopHuePeriodMs = 2048;
inline constexpr std::uint32_t kGlitchBurstMs = 180;
inline constexpr std::uint32_t kGlitchPauseMs = 1200;
inline constexpr std::uint32_t kStartupMs = 3000;
inline constexpr std::uint32_t kStartupFaceMs = 200;
inline constexpr std::uint32_t kDefaultFrameMs = 500;
inline constexpr std::uint32_t kTickMs = 33;
inline constexpr std::uint32_t kRandomizePeriodMs = 100;
inline constexpr int kStickDeadzone = 64;
inline constexpr int kDizzySpinX = 8;
inline constexpr int kDizzySpinSize = 16;
inline constexpr float kGyroDizzyDeg = 55.0f;
inline constexpr float kGyroKickDeg = 16.0f;
inline constexpr float kGyroNudgeDiv = 32.0f;
inline constexpr float kGyroNudgeDeadDeg = 12.0f;
inline constexpr float kGyroMotionDizzyDeg = 24.0f;
inline constexpr float kGyroRestMotionDeg = 3.5f;

inline constexpr int kSettingsPageCount = 5;
inline constexpr int kSettingStatus1 = 255;
inline constexpr int kSettingStatus2 = 254;
inline constexpr std::uint32_t kMenuHoldMs = 600;
inline constexpr std::uint32_t kEscDoubleMs = 500;
inline constexpr std::uint32_t kSettingsSlideMs = 220;
inline constexpr int kGameCount = 9;
inline constexpr int kSpectrumBands = 16;
inline constexpr int kSpectrumBarW = 4;
inline constexpr int kSpectrumPlotH = 29;
// Signed PCM window from the microphone HAL (-1..1). Mouth still uses RMS.
inline constexpr int kMicPcmSize = 256;

// Red 3×3 on the left panel while the Mocute pad is missing.
inline constexpr int kHidLostX = 1;
inline constexpr int kHidLostY = 1;
inline constexpr int kHidLostSize = 3;

// LILYGO T8 V1.8. HUB75 matches the tested env:hub75 firmware in esp32-tools.
// TF slot must stay empty. GPIO2 is strapping/SD-MISO — leave unused.
namespace pins {
inline constexpr int kOledScl = 22;
inline constexpr int kOledSda = 21;
inline constexpr int kOledAddr = 0x3C;
inline constexpr int kMpuAddr = 0x68;
inline constexpr int kLedDin = 12;
inline constexpr int kFanPwm = 0;
inline constexpr int kFanRpm = 34;
inline constexpr int kMicAdc = 35;
inline constexpr int kBoopAdc = 36;
inline constexpr int kMatrixR1 = 25;
inline constexpr int kMatrixG1 = 26;
inline constexpr int kMatrixB1 = 27;
inline constexpr int kMatrixR2 = 14;
inline constexpr int kMatrixG2 = 13;
inline constexpr int kMatrixB2 = 4;
inline constexpr int kMatrixA = 23;
inline constexpr int kMatrixB = 19;
inline constexpr int kMatrixC = 5;
inline constexpr int kMatrixD = 18;
inline constexpr int kMatrixE = -1;
inline constexpr int kMatrixLat = 15;
inline constexpr int kMatrixOe = 33;
inline constexpr int kMatrixClk = 32;
inline constexpr int kMatrixChain = 2;
inline constexpr int kMatrixBrightness = 77;
}  // namespace pins

}  // namespace koto
