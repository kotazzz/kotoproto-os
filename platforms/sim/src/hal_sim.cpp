#include "koto/sim/hal_sim.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <utility>
#include <vector>

#include "koto/protocol/mocute.hpp"

namespace koto {
namespace sim {

Matrix::Matrix(int width, int height)
    : width_(width),
      height_(height),
      rgb_(static_cast<std::size_t>(width * height * 3), 0) {}

void Matrix::present(const Color* pixels, int width, int height) {
  const int w = std::min(width, width_);
  const int h = std::min(height, height_);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const Color c = pixels[y * width + x];
      const std::size_t i = static_cast<std::size_t>((y * width_ + x) * 3);
      rgb_[i] = c.r;
      rgb_[i + 1] = c.g;
      rgb_[i + 2] = c.b;
    }
  }
}

void Matrix::copy_rgb(std::vector<std::uint8_t>& out) const {
  out = rgb_;
}

Oled::Oled(int width, int height)
    : width_(width),
      height_(height),
      bits_(static_cast<std::size_t>(((width + 7) / 8) * height), 0) {}

void Oled::present(const std::uint8_t* packed_bits, int width, int height) {
  const int bytes_per_row = (std::min(width, width_) + 7) / 8;
  const int rows = std::min(height, height_);
  const int dst_bpr = (width_ + 7) / 8;
  for (int y = 0; y < rows; ++y) {
    for (int b = 0; b < bytes_per_row && b < dst_bpr; ++b) {
      bits_[static_cast<std::size_t>(y * dst_bpr + b)] =
          packed_bits[y * ((width + 7) / 8) + b];
    }
  }
}

void Oled::copy_bits(std::vector<std::uint8_t>& out) const {
  out = bits_;
}

LedRing::LedRing(int count)
    : count_(count), rgb_(static_cast<std::size_t>(count * 3), 0) {}

void LedRing::present(const Color* leds, int count) {
  const int n = std::min(count, count_);
  for (int i = 0; i < n; ++i) {
    const std::size_t idx = static_cast<std::size_t>(i * 3);
    rgb_[idx] = leds[i].r;
    rgb_[idx + 1] = leds[i].g;
    rgb_[idx + 2] = leds[i].b;
  }
}

void LedRing::copy_rgb(std::vector<std::uint8_t>& out) const {
  out = rgb_;
}

void HidHost::start() {
  connected_ = true;
  log_.push_back("HID SCAN mocute");
}

void HidHost::set_report_handler(ReportHandler handler) {
  handler_ = std::move(handler);
}

bool HidHost::connected() const {
  return connected_;
}

void HidHost::set_connected(bool value) {
  connected_ = value;
  log_line(value ? "HID connected" : "HID disconnected");
}

void HidHost::inject_report(const std::uint8_t* data, std::size_t len) {
  const PadState pad = parse_mocute_report(data, len);
  log_line(std::string("HID ") + report_hex(data, len) + " | " + format_pad(pad));
  if (handler_) {
    handler_(data, len);
  }
}

void HidHost::log_line(const std::string& line) {
  log_.push_back(line);
  if (log_.size() > 80) {
    log_.erase(log_.begin(), log_.begin() + static_cast<std::ptrdiff_t>(log_.size() - 80));
  }
}

std::vector<std::string> HidHost::log_copy() const {
  return log_;
}

Store::Store(std::string path) : path_(std::move(path)) {}

bool Store::load(std::uint8_t* data, std::size_t size) {
  std::ifstream in(path_, std::ios::binary);
  if (!in) {
    return false;
  }
  in.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(size));
  return in.gcount() == static_cast<std::streamsize>(size);
}

bool Store::save(const std::uint8_t* data, std::size_t size) {
  std::ofstream out(path_, std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  return static_cast<bool>(out);
}

std::uint32_t Store::free_heap() const {
  return 8u * 1024u * 1024u;
}

}  // namespace sim
}  // namespace koto
