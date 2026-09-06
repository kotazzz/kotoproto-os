#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "koto/hal/clock.hpp"
#include "koto/hal/fan.hpp"
#include "koto/hal/hid_host.hpp"
#include "koto/hal/led_ring.hpp"
#include "koto/hal/matrix.hpp"
#include "koto/hal/oled.hpp"
#include "koto/hal/sensors.hpp"
#include "koto/hal/store.hpp"

namespace koto {
namespace sim {

class Clock final : public hal::IClock {
 public:
  std::uint32_t millis() const override { return millis_; }
  void advance(std::uint32_t dt_ms) { millis_ += dt_ms; }
  void reset() { millis_ = 0; }

 private:
  std::uint32_t millis_ = 0;
};

class Matrix final : public hal::IMatrix {
 public:
  Matrix(int width, int height);

  int width() const override { return width_; }
  int height() const override { return height_; }
  void present(const Color* pixels, int width, int height) override;

  void copy_rgb(std::vector<std::uint8_t>& out) const;

 private:
  int width_;
  int height_;
  std::vector<std::uint8_t> rgb_;
};

class Oled final : public hal::IOled {
 public:
  Oled(int width, int height);

  int width() const override { return width_; }
  int height() const override { return height_; }
  void present(const std::uint8_t* packed_bits, int width, int height) override;

  void copy_bits(std::vector<std::uint8_t>& out) const;

 private:
  int width_;
  int height_;
  std::vector<std::uint8_t> bits_;
};

class LedRing final : public hal::ILedRing {
 public:
  explicit LedRing(int count);

  int size() const override { return count_; }
  void present(const Color* leds, int count) override;
  void copy_rgb(std::vector<std::uint8_t>& out) const;

 private:
  int count_;
  std::vector<std::uint8_t> rgb_;
};

class HidHost final : public hal::IHidHost {
 public:
  void start() override;
  void set_report_handler(ReportHandler handler) override;
  bool connected() const override;
  void set_connected(bool value);
  void inject_report(const std::uint8_t* data, std::size_t len);
  void log_line(const std::string& line);
  std::vector<std::string> log_copy() const;

 private:
  ReportHandler handler_;
  std::vector<std::string> log_;
  bool connected_ = true;
};

class Store final : public hal::IStore {
 public:
  explicit Store(std::string path = "koto_settings.bin");
  bool load(std::uint8_t* data, std::size_t size) override;
  bool save(const std::uint8_t* data, std::size_t size) override;
  std::uint32_t free_heap() const override;

 private:
  std::string path_;
};

class Sensors final : public hal::ISensors {
 public:
  float microphone() const override { return mic_; }
  hal::GyroSample gyro() const override { return gyro_; }
  float proximity() const override { return proximity_; }

  void set_microphone(float value) { mic_ = value; }
  void set_gyro(float pitch, float roll, float yaw) {
    gyro_.pitch_deg = pitch;
    gyro_.roll_deg = roll;
    gyro_.yaw_deg = yaw;
  }
  void set_proximity(float value) { proximity_ = value; }

 private:
  float mic_ = 0;
  float proximity_ = 0;
  hal::GyroSample gyro_{};
};

class Fan final : public hal::IFan {
 public:
  void set_speed(std::uint8_t duty) override { duty_ = duty; }
  std::uint8_t speed() const override { return duty_; }

 private:
  std::uint8_t duty_ = 255;
};

}  // namespace sim
}  // namespace koto
