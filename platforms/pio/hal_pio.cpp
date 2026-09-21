#include "hal_pio.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <Adafruit_NeoPixel.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Preferences.h>
#include <Wire.h>

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

MatrixPanel_I2S_DMA* gPanel = nullptr;
Adafruit_SSD1306* gOled = nullptr;
bool gOledOk = false;
Adafruit_NeoPixel* gRing = nullptr;
bool gWireOk = false;
bool gFanPwm = false;

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

}  // namespace

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
  const int ox = panel * width;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const Color c = pixels[y * width + x];
      gPanel->drawPixel(ox + x, y, gPanel->color565(c.r, c.g, c.b));
    }
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
  std::uint8_t* buf = gOled->getBuffer();
  if (buf == nullptr) {
    return;
  }
  const int bpr = (width + 7) / 8;
  const int max_x = width < kOledW ? width : kOledW;
  const int max_y = height < kOledH ? height : kOledH;
  std::memset(buf, 0, static_cast<std::size_t>(kOledW * kOledH / 8));
  for (int y = 0; y < max_y; ++y) {
    for (int x = 0; x < max_x; ++x) {
      if ((packed_bits[y * bpr + (x >> 3)] & static_cast<std::uint8_t>(0x80 >> (x & 7))) == 0) {
        continue;
      }
      buf[x + (y >> 3) * kOledW] |= static_cast<std::uint8_t>(1 << (y & 7));
    }
  }
  gOled->display();
}

bool LedRing::begin() {
  pinMode(pins::kLedDin, OUTPUT);
  digitalWrite(pins::kLedDin, LOW);
  const int n = hal::kLedRingCount * hal::kLedRingCopies;
  gRing = new Adafruit_NeoPixel(n, pins::kLedDin, NEO_GRB + NEO_KHZ800);
  gRing->begin();
  gRing->clear();
  gRing->show();
  Serial.println("WS2812 GPIO12 x12");
  return true;
}

int LedRing::size() const { return hal::kLedRingCount; }

void LedRing::present(const Color* leds, int count) {
  if (gRing == nullptr || leds == nullptr || count <= 0) {
    return;
  }
  const int n = count < hal::kLedRingCount ? count : hal::kLedRingCount;
  for (int copy = 0; copy < hal::kLedRingCopies; ++copy) {
    for (int i = 0; i < n; ++i) {
      const Color c = leds[i];
      gRing->setPixelColor(copy * hal::kLedRingCount + i, gRing->Color(c.r, c.g, c.b));
    }
  }
  gRing->show();
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
  analogRead(pins::kMicAdc);
  analogRead(pins::kBoopAdc);
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

void Sensors::pump() {
  int sum = 0;
  int raw[kMicPcmSize];
  for (int i = 0; i < kMicPcmSize; ++i) {
    raw[i] = analogRead(pins::kMicAdc);
    sum += raw[i];
  }
  const float dc = static_cast<float>(sum) / static_cast<float>(kMicPcmSize);
  float acc = 0;
  for (int i = 0; i < kMicPcmSize; ++i) {
    float s = (static_cast<float>(raw[i]) - dc) / 2048.0f;
    if (s > 1.0f) {
      s = 1.0f;
    } else if (s < -1.0f) {
      s = -1.0f;
    }
    pcm_[i] = s;
    acc += s * s;
  }
  pcm_count_ = kMicPcmSize;
  mic_ = std::min(1.0f, std::sqrt(acc / static_cast<float>(kMicPcmSize)) * 1.41421356f);

  // TCRT5000 AO drops when something is close (phototransistor to GND).
  const float far = static_cast<float>(analogRead(pins::kBoopAdc)) / 4095.0f;
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
  pinMode(pins::kFanPwm, OUTPUT);
  digitalWrite(pins::kFanPwm, HIGH);
  delay(20);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pins::kFanPwm, hal::kFanPwmHz, kFanLedcBits);
#else
  ledcSetup(kFanLedcChannel, hal::kFanPwmHz, kFanLedcBits);
  ledcAttachPin(pins::kFanPwm, kFanLedcChannel);
#endif
  gFanPwm = true;
  write_fan_duty(duty_);
  Serial.println("fan PWM GPIO0 25kHz");
  return true;
}

void Fan::set_speed(std::uint8_t duty) {
  duty_ = duty;
  write_fan_duty(duty_);
}

std::uint8_t Fan::speed() const { return duty_; }

}  // namespace pio
}  // namespace koto
