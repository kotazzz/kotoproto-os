#pragma once

#include <cstdint>
#include <utility>

#include "koto/hal/clock.hpp"
#include "koto/hal/fan.hpp"
#include "koto/hal/hid_host.hpp"
#include "koto/hal/led_ring.hpp"
#include "koto/hal/matrix.hpp"
#include "koto/hal/oled.hpp"
#include "koto/hal/sensors.hpp"
#include "koto/hal/store.hpp"

namespace koto {
namespace esp {

class Clock final : public hal::IClock {
 public:
  std::uint32_t millis() const override;
};

class Matrix final : public hal::IMatrix {
 public:
  int width() const override;
  int height() const override;
  void present(const Color* pixels, int width, int height, int panel) override;
};

class Oled final : public hal::IOled {
 public:
  int width() const override;
  int height() const override;
  void present(const std::uint8_t* packed_bits, int width, int height) override;
};

class LedRing final : public hal::ILedRing {
 public:
  int size() const override;
  void present(const Color* leds, int count) override;
};

class HidHost final : public hal::IHidHost {
 public:
  void start() override;
  void set_report_handler(ReportHandler handler) override;
  bool connected() const override;

 private:
  ReportHandler handler_;
};

class Store final : public hal::IStore {
 public:
  bool load(std::uint8_t* data, std::size_t size) override;
  bool save(const std::uint8_t* data, std::size_t size) override;
  std::uint32_t free_heap() const override;

 private:
  bool has_ = false;
  std::uint8_t blob_[16]{};
};

class Sensors final : public hal::ISensors {
 public:
  float microphone() const override;
  hal::GyroSample gyro() const override;
  float proximity() const override;
};

class Fan final : public hal::IFan {
 public:
  void set_speed(std::uint8_t duty) override;
  std::uint8_t speed() const override;

 private:
  std::uint8_t duty_ = 255;
};

}  // namespace esp
}  // namespace koto
