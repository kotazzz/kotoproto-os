#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace koto {

enum class PadMode : std::uint8_t {
  Game = 0,
  Key = 1,
};

inline constexpr std::uint8_t kBtnA = 1 << 0;
inline constexpr std::uint8_t kBtnB = 1 << 1;
inline constexpr std::uint8_t kBtnX = 1 << 2;
inline constexpr std::uint8_t kBtnY = 1 << 3;
inline constexpr std::uint8_t kBtnOk = 1 << 4;
inline constexpr std::uint8_t kBtnEsc = 1 << 5;
inline constexpr std::uint8_t kBtnSelect = 1 << 6;

inline constexpr std::uint8_t kHatCenter = 8;
inline constexpr std::uint8_t kReportSize = 6;

struct PadState {
  PadMode mode = PadMode::Game;
  std::uint8_t x = 128;
  std::uint8_t y = 128;
  std::uint8_t hat = kHatCenter;
  std::uint8_t buttons = 0;

  bool a() const { return (buttons & kBtnA) != 0; }
  bool b() const { return (buttons & kBtnB) != 0; }
  bool x_btn() const { return (buttons & kBtnX) != 0; }
  bool y_btn() const { return (buttons & kBtnY) != 0; }
  bool ok() const { return (buttons & kBtnOk) != 0; }
  bool esc() const { return (buttons & kBtnEsc) != 0; }
  bool select() const { return (buttons & kBtnSelect) != 0; }
  bool up() const { return hat == 0 || hat == 1 || hat == 7; }
  bool right() const { return hat == 1 || hat == 2 || hat == 3; }
  bool down() const { return hat == 3 || hat == 4 || hat == 5; }
  bool left() const { return hat == 5 || hat == 6 || hat == 7; }
};

struct DeviceState {
  std::string text = "Neutral";
  std::uint8_t brightness = 136;
  std::uint8_t brightness_level = 8;
  std::string scene = "startup";
  PadState pad;
  std::string face = "Startup";
  int faceset = 1;
  int octant = 2;
  int setting_index = 255;
  bool blinking = false;
  bool boop = false;
  bool dizzy = false;
  bool auto_blink = true;
  bool mouth_enabled = true;
  bool boop_enabled = true;
  bool matrix_enabled = true;
  bool led_enabled = true;
  bool hid_connected = true;
  std::uint8_t boop_sensitivity = 127;
  std::uint8_t fan_speed = 255;
  std::uint8_t rare_chance = 25;
  int snake_score = 0;
  int boop_count = 0;
  int fps = 0;
  std::uint32_t heap = 0;
  std::string transition = "none";
  std::string bt_status = "----";
  float mic = 0;
  float proximity = 0;
  float pitch = 0;
  float roll = 0;
  float yaw = 0;
};

std::array<std::uint8_t, kReportSize> encode_mocute_report(const PadState& pad);
PadState parse_mocute_report(const std::uint8_t* data, std::size_t len);
std::string format_pad(const PadState& pad);
std::string report_hex(const std::uint8_t* data, std::size_t len);

}  // namespace koto
