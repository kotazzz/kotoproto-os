#include "hal_pio.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Preferences.h>
#include <Wire.h>
#include <driver/adc.h>
#include <driver/gpio.h>
#include <driver/rmt.h>

#include "koto/config.hpp"
#include "koto/hal/fan.hpp"
#include "koto/hal/led_ring.hpp"

namespace koto {
namespace pio {
namespace {

constexpr float kRadToDeg = 57.2957795f;
constexpr float kAccelLsb = 16384.0f;
constexpr float kGyroLsb = 131.0f;
constexpr float kGyroLpf = 0.12f;
constexpr int kFanLedcChannel = 2;
constexpr int kFanLedcBits = 8;
constexpr int kPanelPixels = kMatrixW * kMatrixH;
constexpr int kOledPacked = kOledW * kOledH / 8;
constexpr adc1_channel_t kMicAdc1 = ADC1_CHANNEL_7;
constexpr adc1_channel_t kBoopAdc1 = ADC1_CHANNEL_0;
constexpr adc_atten_t kAdcAtten = ADC_ATTEN_DB_12;
constexpr int kRingPixels = hal::kLedRingCount * hal::kLedRingCopies;
constexpr int kRingBytes = kRingPixels * 3;
constexpr rmt_channel_t kRingRmtCh = RMT_CHANNEL_0;
constexpr std::uint32_t kWsT0hNs = 400;
constexpr std::uint32_t kWsT0lNs = 850;
constexpr std::uint32_t kWsT1hNs = 800;
constexpr std::uint32_t kWsT1lNs = 450;

MatrixPanel_I2S_DMA* gPanel = nullptr;
Adafruit_SSD1306* gOled = nullptr;
bool gOledOk = false;
bool gRingRmt = false;
std::uint8_t gRingGrb[kRingBytes]{};
std::uint8_t gRingSent[kRingBytes]{};
bool gRingHaveSent = false;
std::uint32_t gRingLastMs = 0;
std::uint32_t gRingT0h = 8;
std::uint32_t gRingT0l = 17;
std::uint32_t gRingT1h = 16;
std::uint32_t gRingT1l = 9;
bool gWireOk = false;
bool gFanPwm = false;
PresentYield gPresentYield = nullptr;
Color gLastPanel[kMatrixPanels][kPanelPixels]{};
bool gLastPanelOk[kMatrixPanels] = {false, false};
std::uint8_t gLastOled[kOledPacked]{};
bool gLastOledOk = false;

void yield_present() {
  if (gPresentYield != nullptr) {
    gPresentYield();
  }
}

int read_adc1(adc1_channel_t ch, int pin) {
  const int raw = adc1_get_raw(ch);
  if (raw >= 0) {
    return raw;
  }
  return analogRead(pin);
}

void ensure_wire() {
  if (gWireOk) {
    return;
  }
  Wire.begin(pins::kOledSda, pins::kOledScl);
  Wire.setClock(400000);
  gWireOk = true;
}

bool mpu_write(std::uint8_t reg, std::uint8_t value) {
  Wire.beginTransmission(static_cast<std::uint8_t>(pins::kMpuAddr));
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool mpu_read(std::uint8_t reg, std::uint8_t* buf, int n) {
  if (buf == nullptr || n <= 0) {
    return false;
  }
  Wire.beginTransmission(static_cast<std::uint8_t>(pins::kMpuAddr));
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const int got = Wire.requestFrom(static_cast<int>(pins::kMpuAddr), n);
  if (got != n) {
    return false;
  }
  for (int i = 0; i < n; ++i) {
    buf[i] = static_cast<std::uint8_t>(Wire.read());
  }
  return true;
}

std::int16_t be16(const std::uint8_t* p) {
  return static_cast<std::int16_t>((static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

void write_fan_duty(std::uint8_t duty) {
  if (!gFanPwm) {
    return;
  }
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pins::kFanPwm, duty);
#else
  ledcWrite(kFanLedcChannel, duty);
#endif
}

void IRAM_ATTR ring_rmt_adapter(const void* src, rmt_item32_t* dest, size_t src_size, size_t wanted_num,
                                size_t* translated_size, size_t* item_num) {
  if (src == nullptr || dest == nullptr) {
    *translated_size = 0;
    *item_num = 0;
    return;
  }
  const rmt_item32_t bit0 = {{{gRingT0h, 1, gRingT0l, 0}}};
  const rmt_item32_t bit1 = {{{gRingT1h, 1, gRingT1l, 0}}};
  const auto* psrc = static_cast<const std::uint8_t*>(src);
  size_t size = 0;
  size_t num = 0;
  while (size < src_size && num < wanted_num) {
    const std::uint8_t byte = psrc[size];
    for (int i = 0; i < 8; ++i) {
      dest[num].val = ((byte & (1 << (7 - i))) != 0) ? bit1.val : bit0.val;
      ++num;
    }
    ++size;
  }
  *translated_size = size;
  *item_num = num;
}

bool ring_rmt_send() {
  if (!gRingRmt) {
    return false;
  }
  if (rmt_write_sample(kRingRmtCh, gRingGrb, static_cast<size_t>(kRingBytes), true) != ESP_OK) {
    return false;
  }
  return rmt_wait_tx_done(kRingRmtCh, pdMS_TO_TICKS(100)) == ESP_OK;
}

}  // namespace

void set_present_yield(PresentYield fn) {
  gPresentYield = fn;
}

std::uint32_t Clock::millis() const {
  return static_cast<std::uint32_t>(::millis());
}

bool Matrix::begin() {
  const HUB75_I2S_CFG::i2s_pins hub_pins = {
      pins::kMatrixR1, pins::kMatrixG1, pins::kMatrixB1, pins::kMatrixR2, pins::kMatrixG2,
      pins::kMatrixB2, pins::kMatrixA,  pins::kMatrixB,  pins::kMatrixC,  pins::kMatrixD,
      pins::kMatrixE,  pins::kMatrixLat, pins::kMatrixOe, pins::kMatrixClk};
  HUB75_I2S_CFG cfg(kMatrixW, kMatrixH, pins::kMatrixChain, hub_pins);
  cfg.clkphase = false;
  cfg.latch_blanking = 4;
  cfg.setPixelColorDepthBits(6);
  gPanel = new MatrixPanel_I2S_DMA(cfg);
  if (!gPanel->begin()) {
    Serial.println("HUB75 DMA init failed");
    return false;
  }
  gPanel->setBrightness8(static_cast<uint8_t>(pins::kMatrixBrightness));
  gPanel->clearScreen();
  Serial.println("HUB75 2x64x32");
  return true;
}

int Matrix::width() const { return kMatrixW; }
int Matrix::height() const { return kMatrixH; }

void Matrix::present(const Color* pixels, int width, int height, int panel) {
  if (gPanel == nullptr || pixels == nullptr || width <= 0 || height <= 0) {
    return;
  }
  if (panel < 0 || panel >= kMatrixPanels) {
    return;
  }
  const int count = width * height;
  if (width == kMatrixW && height == kMatrixH && count == kPanelPixels && gLastPanelOk[panel] &&
      std::memcmp(gLastPanel[panel], pixels, sizeof(Color) * static_cast<std::size_t>(count)) == 0) {
    return;
  }
  const int ox = panel * width;
  for (int y = 0; y < height; ++y) {
    if ((y & 7) == 0) {
      yield_present();
    }
    for (int x = 0; x < width; ++x) {
      const Color c = pixels[y * width + x];
      gPanel->drawPixelRGB888(ox + x, y, c.r, c.g, c.b);
    }
  }
  if (width == kMatrixW && height == kMatrixH && count == kPanelPixels) {
    std::memcpy(gLastPanel[panel], pixels, sizeof(Color) * static_cast<std::size_t>(count));
    gLastPanelOk[panel] = true;
  }
}

bool Oled::begin() {
  ensure_wire();
  gOled = new Adafruit_SSD1306(kOledW, kOledH, &Wire, -1);
  gOledOk = gOled->begin(SSD1306_SWITCHCAPVCC, static_cast<std::uint8_t>(pins::kOledAddr));
  if (!gOledOk) {
    Serial.println("OLED missing");
    return false;
  }
  gOled->clearDisplay();
  gOled->display();
  Serial.println("OLED 128x64");
  return true;
}

int Oled::width() const { return kOledW; }
int Oled::height() const { return kOledH; }

void Oled::present(const std::uint8_t* packed_bits, int width, int height) {
  if (!gOledOk || gOled == nullptr || packed_bits == nullptr || width <= 0 || height <= 0) {
    return;
  }
  const int bpr = (width + 7) / 8;
  const int packed = bpr * height;
  if (width == kOledW && height == kOledH && packed == kOledPacked && gLastOledOk &&
      std::memcmp(gLastOled, packed_bits, static_cast<std::size_t>(kOledPacked)) == 0) {
    return;
  }
  std::uint8_t* buf = gOled->getBuffer();
  if (buf == nullptr) {
    return;
  }
  const int max_x = width < kOledW ? width : kOledW;
  const int max_y = height < kOledH ? height : kOledH;
  std::memset(buf, 0, static_cast<std::size_t>(kOledPacked));
  for (int y = 0; y < max_y; ++y) {
    for (int x = 0; x < max_x; ++x) {
      if ((packed_bits[y * bpr + (x >> 3)] & static_cast<std::uint8_t>(0x80 >> (x & 7))) == 0) {
        continue;
      }
      buf[x + (y >> 3) * kOledW] |= static_cast<std::uint8_t>(1 << (y & 7));
    }
  }
  gOled->display();
  if (width == kOledW && height == kOledH && packed == kOledPacked) {
    std::memcpy(gLastOled, packed_bits, static_cast<std::size_t>(kOledPacked));
    gLastOledOk = true;
  }
}

bool LedRing::begin() {
  pinMode(pins::kLedDin, OUTPUT);
  digitalWrite(pins::kLedDin, LOW);
  gpio_pulldown_en(static_cast<gpio_num_t>(pins::kLedDin));
  gpio_set_drive_capability(static_cast<gpio_num_t>(pins::kLedDin), GPIO_DRIVE_CAP_3);

  rmt_config_t cfg = RMT_DEFAULT_CONFIG_TX(static_cast<gpio_num_t>(pins::kLedDin), kRingRmtCh);
  cfg.clk_div = 2;
  cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  cfg.tx_config.idle_output_en = true;
  if (rmt_config(&cfg) != ESP_OK || rmt_driver_install(kRingRmtCh, 0, 0) != ESP_OK) {
    Serial.println("WS2812 RMT init failed");
    return false;
  }
  std::uint32_t hz = 0;
  rmt_get_counter_clock(kRingRmtCh, &hz);
  if (hz == 0) {
    hz = 40000000;
  }
  gRingT0h = static_cast<std::uint32_t>((static_cast<std::uint64_t>(hz) * kWsT0hNs) / 1000000000ULL);
  gRingT0l = static_cast<std::uint32_t>((static_cast<std::uint64_t>(hz) * kWsT0lNs) / 1000000000ULL);
  gRingT1h = static_cast<std::uint32_t>((static_cast<std::uint64_t>(hz) * kWsT1hNs) / 1000000000ULL);
  gRingT1l = static_cast<std::uint32_t>((static_cast<std::uint64_t>(hz) * kWsT1lNs) / 1000000000ULL);
  if (gRingT0h == 0) {
    gRingT0h = 1;
  }
  if (gRingT0l == 0) {
    gRingT0l = 1;
  }
  if (gRingT1h == 0) {
    gRingT1h = 1;
  }
  if (gRingT1l == 0) {
    gRingT1l = 1;
  }
  rmt_translator_init(kRingRmtCh, ring_rmt_adapter);
  gpio_set_drive_capability(static_cast<gpio_num_t>(pins::kLedDin), GPIO_DRIVE_CAP_3);
  gRingRmt = true;
  std::memset(gRingGrb, 0, sizeof(gRingGrb));
  if (!ring_rmt_send()) {
    gRingRmt = false;
    Serial.println("WS2812 RMT write failed");
    return false;
  }
  std::memcpy(gRingSent, gRingGrb, sizeof(gRingGrb));
  gRingHaveSent = true;
  gRingLastMs = static_cast<std::uint32_t>(::millis());
  Serial.println("WS2812 RMT hold GPIO12 x12");
  return true;
}

int LedRing::size() const { return hal::kLedRingCount; }

void LedRing::present(const Color* leds, int count) {
  if (!gRingRmt || leds == nullptr || count <= 0) {
    return;
  }
  const int n = count < hal::kLedRingCount ? count : hal::kLedRingCount;
  std::memset(gRingGrb, 0, sizeof(gRingGrb));
  for (int copy = 0; copy < hal::kLedRingCopies; ++copy) {
    for (int i = 0; i < n; ++i) {
      const Color c = leds[i];
      std::uint8_t* p = &gRingGrb[(copy * hal::kLedRingCount + i) * 3];
      p[0] = c.g;
      p[1] = c.r;
      p[2] = c.b;
    }
  }
  const std::uint32_t now = static_cast<std::uint32_t>(::millis());
  const bool same = gRingHaveSent && std::memcmp(gRingGrb, gRingSent, sizeof(gRingGrb)) == 0;
  if (same && (now - gRingLastMs) < 250) {
    return;
  }
  if (!ring_rmt_send()) {
    return;
  }
  std::memcpy(gRingSent, gRingGrb, sizeof(gRingGrb));
  gRingHaveSent = true;
  gRingLastMs = now;
}

bool Store::load(std::uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0) {
    return false;
  }
  Preferences prefs;
  if (!prefs.begin("koto", true)) {
    return false;
  }
  const std::size_t got = prefs.getBytesLength("set");
  if (got != size) {
    prefs.end();
    return false;
  }
  prefs.getBytes("set", data, size);
  prefs.end();
  return true;
}

bool Store::save(const std::uint8_t* data, std::size_t size) {
  if (data == nullptr || size == 0) {
    return false;
  }
  Preferences prefs;
  if (!prefs.begin("koto", false)) {
    return false;
  }
  const std::size_t written = prefs.putBytes("set", data, size);
  prefs.end();
  return written == size;
}

std::uint32_t Store::free_heap() const {
  return static_cast<std::uint32_t>(ESP.getFreeHeap());
}

bool Sensors::begin() {
  ensure_wire();
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(kMicAdc1, kAdcAtten);
  adc1_config_channel_atten(kBoopAdc1, kAdcAtten);
  read_adc1(kMicAdc1, pins::kMicAdc);
  read_adc1(kBoopAdc1, pins::kBoopAdc);
  pinMode(pins::kFanRpm, INPUT);

  delay(40);
  mpu_ok_ = mpu_write(0x6B, 0x00);
  if (mpu_ok_) {
    mpu_write(0x1A, 0x03);
    mpu_write(0x1B, 0x00);
    mpu_write(0x1C, 0x00);
    Serial.println("MPU6050 0x68");
  } else {
    Serial.println("MPU6050 missing");
  }
  Serial.println("mic GPIO35 boop GPIO36");
  return true;
}

void Sensors::set_microphone_window(int n) {
  if (n < 1) {
    n = 1;
  }
  if (n > kMicPcmSize) {
    n = kMicPcmSize;
  }
  pcm_window_ = n;
}

void Sensors::pump() {
  int raw[kMicPcmSize];
  const int n = pcm_window_ < 1 ? 1 : (pcm_window_ > kMicPcmSize ? kMicPcmSize : pcm_window_);
  int sum = 0;
  for (int i = 0; i < n; ++i) {
    raw[i] = read_adc1(kMicAdc1, pins::kMicAdc);
    sum += raw[i];
  }
  const float dc = static_cast<float>(sum) / static_cast<float>(n);
  float acc = 0;
  for (int i = 0; i < n; ++i) {
    float s = (static_cast<float>(raw[i]) - dc) / 2048.0f;
    if (s > 1.0f) {
      s = 1.0f;
    } else if (s < -1.0f) {
      s = -1.0f;
    }
    pcm_[i] = s;
    acc += s * s;
  }
  pcm_count_ = n;
  mic_ = std::min(1.0f, std::sqrt(acc / static_cast<float>(n)) * 1.41421356f);

  // TCRT5000 AO drops when something is close (phototransistor to GND).
  const float far = static_cast<float>(read_adc1(kBoopAdc1, pins::kBoopAdc)) / 4095.0f;
  prox_ = std::clamp(1.0f - far, 0.0f, 1.0f);

  if (!mpu_ok_) {
    return;
  }
  std::uint8_t buf[14];
  if (!mpu_read(0x3B, buf, 14)) {
    return;
  }
  const float ax = static_cast<float>(be16(buf + 0)) / kAccelLsb;
  const float ay = static_cast<float>(be16(buf + 2)) / kAccelLsb;
  const float az = static_cast<float>(be16(buf + 4)) / kAccelLsb;
  const float gz = static_cast<float>(be16(buf + 12)) / kGyroLsb;
  const float pitch = std::atan2(-ax, std::sqrt(ay * ay + az * az)) * kRadToDeg;
  const float roll = std::atan2(ay, az) * kRadToDeg;
  if (!gyro_ready_) {
    gyro_.pitch_deg = pitch;
    gyro_.roll_deg = roll;
    gyro_ready_ = true;
  } else {
    gyro_.pitch_deg += (pitch - gyro_.pitch_deg) * kGyroLpf;
    gyro_.roll_deg += (roll - gyro_.roll_deg) * kGyroLpf;
  }
  const std::uint32_t now = static_cast<std::uint32_t>(::millis());
  if (gyro_ms_ != 0) {
    const float dt = static_cast<float>(now - gyro_ms_) / 1000.0f;
    if (dt > 0.0f && dt < 0.2f) {
      gyro_.yaw_deg += gz * dt;
    }
  }
  gyro_ms_ = now;
}

float Sensors::microphone() const { return mic_; }

int Sensors::copy_microphone_pcm(float* out, int max) const {
  if (out == nullptr || max <= 0 || pcm_count_ <= 0) {
    return 0;
  }
  const int n = max < pcm_count_ ? max : pcm_count_;
  std::memcpy(out, pcm_, static_cast<std::size_t>(n) * sizeof(float));
  return n;
}

hal::GyroSample Sensors::gyro() const { return gyro_; }
float Sensors::proximity() const { return prox_; }

bool Fan::begin() {
  // GPIO0 is a boot strap pin. A LOW at reset enters download mode; a sudden
  // HIGH turns an active-high fan on and can brown out USB 5 V. Keep the pin
  // as a pull-up until PWM is attached, then start at duty 0. App::init applies
  // the saved speed after the rest of the board is up.
  pinMode(pins::kFanPwm, INPUT_PULLUP);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pins::kFanPwm, hal::kFanPwmHz, kFanLedcBits);
#else
  ledcSetup(kFanLedcChannel, hal::kFanPwmHz, kFanLedcBits);
  ledcAttachPin(pins::kFanPwm, kFanLedcChannel);
#endif
  gFanPwm = true;
  duty_ = 0;
  write_fan_duty(0);
  Serial.println("fan PWM GPIO0 25kHz");
  return true;
}

void Fan::set_speed(std::uint8_t duty) {
  if (gFanPwm && duty_ == 0 && duty > 32) {
    for (int step = 32; step < static_cast<int>(duty); step += 32) {
      write_fan_duty(static_cast<std::uint8_t>(step));
      delay(15);
    }
  }
  duty_ = duty;
  write_fan_duty(duty_);
}

std::uint8_t Fan::speed() const { return duty_; }

}  // namespace pio
}  // namespace koto
