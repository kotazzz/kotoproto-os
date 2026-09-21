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
  pinMode(koto::pins::kFanPwm, OUTPUT);
  digitalWrite(koto::pins::kFanPwm, HIGH);

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
  hid.pump();
  sensors.pump();
  if (app != nullptr) {
    app->tick();
  }
  delay(static_cast<int>(koto::kTickMs));
}
