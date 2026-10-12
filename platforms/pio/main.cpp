#include <Arduino.h>

#include "hal_pio.hpp"
#include "koto/app.hpp"
#include "koto/config.hpp"

namespace {

koto::pio::Matrix matrix;
koto::pio::Oled oled;
koto::pio::LedRing ring;
koto::pio::HidHost hid;
koto::pio::Clock kclock;
koto::pio::Sensors sensors;
koto::pio::Fan fan;
koto::pio::Store store;
koto::App* app = nullptr;

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("kotoproto-os T8 visor");

  pinMode(koto::pins::kLedDin, OUTPUT);
  digitalWrite(koto::pins::kLedDin, LOW);

  koto::pio::set_present_yield([]() { hid.pump(); });

  if (!matrix.begin()) {
    while (true) {
      delay(1000);
    }
  }
  oled.begin();
  ring.begin();
  sensors.begin();
  fan.begin();

  app = new koto::App(matrix, oled, ring, hid, kclock, sensors, fan, store);
  app->init();
}

void loop() {
  const std::uint32_t start = static_cast<std::uint32_t>(millis());
  hid.pump();
  hid.pump_idle();
  const std::uint32_t after_hid = static_cast<std::uint32_t>(millis());
  sensors.pump();
  const std::uint32_t after_adc = static_cast<std::uint32_t>(millis());
  if (app != nullptr) {
    app->tick();
  }
  const std::uint32_t after_tick = static_cast<std::uint32_t>(millis());

  const std::uint32_t elapsed = after_tick - start;
  if (elapsed < koto::kTickMs) {
    delay(static_cast<int>(koto::kTickMs - elapsed));
  }

  static std::uint32_t window_ms = 0;
  static std::uint32_t acc_hid = 0;
  static std::uint32_t acc_adc = 0;
  static std::uint32_t acc_tick = 0;
  static std::uint32_t frames = 0;
  acc_hid += after_hid - start;
  acc_adc += after_adc - after_hid;
  acc_tick += after_tick - after_adc;
  ++frames;
  const std::uint32_t now = static_cast<std::uint32_t>(millis());
  if (now - window_ms >= 1000 && frames > 0) {
    Serial.printf("adc=%u tick=%u hid=%u fps=%u heap=%u\n",
                  static_cast<unsigned>(acc_adc / frames),
                  static_cast<unsigned>(acc_tick / frames),
                  static_cast<unsigned>(acc_hid / frames),
                  static_cast<unsigned>(frames),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    window_ms = now;
    acc_hid = 0;
    acc_adc = 0;
    acc_tick = 0;
    frames = 0;
  }
}
