#include "hal_esp32.hpp"

#include <utility>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "koto/config.hpp"

namespace koto {
namespace esp {
namespace {

constexpr const char* kTag = "koto-hal";

}  // namespace

std::uint32_t Clock::millis() const {
  return static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
}

int Matrix::width() const { return kMatrixW; }
int Matrix::height() const { return kMatrixH; }

void Matrix::present(const Color*, int, int) {
  ESP_LOGD(kTag, "matrix present stub");
}

int Oled::width() const { return kOledW; }
int Oled::height() const { return kOledH; }

void Oled::present(const std::uint8_t*, int, int) {
  ESP_LOGD(kTag, "oled present stub");
}

int LedRing::size() const { return hal::kLedRingCount; }

void LedRing::present(const Color*, int) {
  // TODO: WS2812 ×12, затем повтор той же маски на второе кольцо.
  ESP_LOGD(kTag, "led ring present stub (x%d copies)", hal::kLedRingCopies);
}

void HidHost::start() {
  ESP_LOGI(kTag, "HID scan stub: MOCUTE GAME/KEY");
}

void HidHost::set_report_handler(ReportHandler handler) {
  handler_ = std::move(handler);
}

bool HidHost::connected() const {
  return true;
}

bool Store::load(std::uint8_t* data, std::size_t size) {
  if (!has_ || size > sizeof(blob_)) {
    return false;
  }
  for (std::size_t i = 0; i < size; ++i) {
    data[i] = blob_[i];
  }
  return true;
}

bool Store::save(const std::uint8_t* data, std::size_t size) {
  if (size > sizeof(blob_)) {
    return false;
  }
  for (std::size_t i = 0; i < size; ++i) {
    blob_[i] = data[i];
  }
  has_ = true;
  return true;
}

std::uint32_t Store::free_heap() const {
  return 0;
}

float Sensors::microphone() const { return 0; }
hal::GyroSample Sensors::gyro() const { return hal::GyroSample{}; }
float Sensors::proximity() const { return 0; }

void Fan::set_speed(std::uint8_t duty) { duty_ = duty; }
std::uint8_t Fan::speed() const { return duty_; }

}  // namespace esp
}  // namespace koto
