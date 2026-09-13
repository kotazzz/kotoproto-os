#include "koto/protocol/mocute.hpp"

#include <cstdio>
#include <sstream>

namespace koto {
namespace {

constexpr std::uint8_t kKeyEnter = 0x28;
constexpr std::uint8_t kKeyEsc = 0x29;
constexpr std::uint8_t kKeyA = 0x04;
constexpr std::uint8_t kKeyB = 0x05;
constexpr std::uint8_t kKeyX = 0x1B;
constexpr std::uint8_t kKeyY = 0x1C;
constexpr std::uint8_t kKeyRight = 0x4F;
constexpr std::uint8_t kKeyLeft = 0x50;
constexpr std::uint8_t kKeyDown = 0x51;
constexpr std::uint8_t kKeyUp = 0x52;
constexpr std::uint8_t kKeyMenu = 0x65;

std::uint8_t hat_from_axes(std::uint8_t x, std::uint8_t y) {
  const int dx = static_cast<int>(x) - 128;
  const int dy = static_cast<int>(y) - 128;
  const bool left = dx < -40;
  const bool right = dx > 40;
  const bool up = dy < -40;
  const bool down = dy > 40;
  if (up && right) {
    return 1;
  }
  if (down && right) {
    return 3;
  }
  if (down && left) {
    return 5;
  }
  if (up && left) {
    return 7;
  }
  if (up) {
    return 0;
  }
  if (right) {
    return 2;
  }
  if (down) {
    return 4;
  }
  if (left) {
    return 6;
  }
  return kHatCenter;
}

bool looks_like_keyboard(const std::uint8_t* data, std::size_t len) {
  return len == 8 && data[1] == 0;
}

PadState parse_keyboard(const std::uint8_t* data) {
  PadState pad;
  pad.mode = PadMode::Key;
  pad.x = 128;
  pad.y = 128;
  pad.hat = kHatCenter;
  pad.buttons = 0;
  for (int i = 2; i < 8; ++i) {
    switch (data[i]) {
      case kKeyUp:
        pad.y = 0;
        break;
      case kKeyDown:
        pad.y = 255;
        break;
      case kKeyLeft:
        pad.x = 0;
        break;
      case kKeyRight:
        pad.x = 255;
        break;
      case kKeyA:
        pad.buttons = static_cast<std::uint8_t>(pad.buttons | kBtnA);
        break;
      case kKeyB:
        pad.buttons = static_cast<std::uint8_t>(pad.buttons | kBtnB);
        break;
      case kKeyX:
        pad.buttons = static_cast<std::uint8_t>(pad.buttons | kBtnX);
        break;
      case kKeyY:
        pad.buttons = static_cast<std::uint8_t>(pad.buttons | kBtnY);
        break;
      case kKeyEnter:
        pad.buttons = static_cast<std::uint8_t>(pad.buttons | kBtnOk | kBtnA);
        break;
      case kKeyEsc:
        pad.buttons = static_cast<std::uint8_t>(pad.buttons | kBtnEsc);
        break;
      case kKeyMenu:
        pad.buttons = static_cast<std::uint8_t>(pad.buttons | kBtnSelect);
        break;
      default:
        break;
    }
  }
  pad.hat = hat_from_axes(pad.x, pad.y);
  return pad;
}

}  // namespace

std::array<std::uint8_t, kReportSize> encode_mocute_report(const PadState& pad) {
  std::array<std::uint8_t, kReportSize> out{};
  out[0] = pad.x;
  out[1] = pad.y;
  out[2] = pad.hat > 8 ? kHatCenter : pad.hat;
  out[3] = pad.buttons;
  out[4] = static_cast<std::uint8_t>(pad.mode);
  out[5] = 0;
  return out;
}

PadState parse_mocute_report(const std::uint8_t* data, std::size_t len) {
  if (data == nullptr || len == 0) {
    return PadState{};
  }
  if (looks_like_keyboard(data, len)) {
    return parse_keyboard(data);
  }

  PadState pad;
  pad.x = data[0];
  pad.y = len > 1 ? data[1] : 128;
  pad.hat = len > 2 ? data[2] : kHatCenter;
  pad.buttons = len > 3 ? data[3] : 0;
  pad.mode = (len > 4 && data[4] != 0) ? PadMode::Key : PadMode::Game;
  if (pad.hat > 8) {
    pad.hat = kHatCenter;
  }
  if (pad.hat == kHatCenter) {
    pad.hat = hat_from_axes(pad.x, pad.y);
  }
  return pad;
}

std::string format_pad(const PadState& pad) {
  std::ostringstream out;
  out << (pad.mode == PadMode::Game ? "GAME" : "KEY")
      << " x=" << static_cast<int>(pad.x)
      << " y=" << static_cast<int>(pad.y)
      << " hat=" << static_cast<int>(pad.hat);
  if (pad.a()) {
    out << " A";
  }
  if (pad.b()) {
    out << " B";
  }
  if (pad.x_btn()) {
    out << " X";
  }
  if (pad.y_btn()) {
    out << " Y";
  }
  if (pad.ok()) {
    out << " OK";
  }
  if (pad.esc()) {
    out << " ESC";
  }
  if (pad.select()) {
    out << " SEL";
  }
  return out.str();
}

std::string report_hex(const std::uint8_t* data, std::size_t len) {
  std::string out;
  char buf[4];
  for (std::size_t i = 0; i < len; ++i) {
    std::snprintf(buf, sizeof(buf), "%02X", data[i]);
    if (!out.empty()) {
      out.push_back(' ');
    }
    out += buf;
  }
  return out;
}

}  // namespace koto
