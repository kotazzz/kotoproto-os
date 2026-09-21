#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "koto/config.hpp"
#include "koto/hal/clock.hpp"
#include "koto/hal/fan.hpp"
#include "koto/hal/hid_host.hpp"
#include "koto/hal/led_ring.hpp"
#include "koto/hal/matrix.hpp"
#include "koto/hal/oled.hpp"
#include "koto/hal/sensors.hpp"
#include "koto/hal/store.hpp"

namespace koto {
namespace pio {

class Clock final : public hal::IClock {
 public:
  std::uint32_t millis() const override;
};

class Matrix final : public hal::IMatrix {
 public:
  bool begin();
  int width() const override;
  int height() const override;
  void present(const Color* pixels, int width, int height, int panel) override;
};

class Oled final : public hal::IOled {
 public:
  bool begin();
  int width() const override;
  int height() const override;
  void present(const std::uint8_t* packed_bits, int width, int height) override;
};

class LedRing final : public hal::ILedRing {
 public:
  bool begin();
  int size() const override;
  void present(const Color* leds, int count) override;
};

class HidHost final : public hal::IHidHost {
 public:
  void start() override;
  void set_report_handler(ReportHandler handler) override;
  bool connected() const override;
  void pump();

 private:
  ReportHandler handler_;
};

class Store final : public hal::IStore {
 public:
  bool load(std::uint8_t* data, std::size_t size) override;
  bool save(const std::uint8_t* data, std::size_t size) override;
  std::uint32_t free_heap() const override;
};

class Sensors final : public hal::ISensors {
 public:
  bool begin();
  void pump();
  float microphone() const override;
  int copy_microphone_pcm(float* out, int max) const override;
  hal::GyroSample gyro() const override;
  float proximity() const override;

 private:
  float pcm_[kMicPcmSize]{};
  int pcm_count_ = 0;
  float mic_ = 0;
  float prox_ = 0;
  hal::GyroSample gyro_{};
  bool mpu_ok_ = false;
  bool gyro_ready_ = false;
  std::uint32_t gyro_ms_ = 0;
};

class Fan final : public hal::IFan {
 public:
  bool begin();
  void set_speed(std::uint8_t duty) override;
  std::uint8_t speed() const override;

 private:
  std::uint8_t duty_ = 255;
};

}  // namespace pio
}  // namespace koto
