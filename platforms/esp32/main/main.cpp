#include "hal_esp32.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "koto/app.hpp"

namespace {

constexpr const char* kTag = "koto";

void firmware_task(void*) {
  koto::esp::Matrix matrix;
  koto::esp::Oled oled;
  koto::esp::LedRing ring;
  koto::esp::HidHost hid;
  koto::esp::Clock clock;
  koto::esp::Sensors sensors;
  koto::esp::Fan fan;
  koto::esp::Store store;
  koto::App app(matrix, oled, ring, hid, clock, sensors, fan, store);
  app.init();

  for (;;) {
    app.tick();
    vTaskDelay(pdMS_TO_TICKS(33));
  }
}

}  // namespace

extern "C" void app_main() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
  ESP_LOGI(kTag, "kotoproto-os hello (HAL stubs)");
  xTaskCreate(firmware_task, "koto", 8192, nullptr, 5, nullptr);
}
