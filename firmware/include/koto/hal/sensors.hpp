#pragma once

namespace koto {
namespace hal {

struct GyroSample {
  float pitch_deg = 0;
  float roll_deg = 0;
  float yaw_deg = 0;
};

// Микрофон, IMU и датчик приближения (boop). На железе — ADC/I2C, в симе — слайдеры.
class ISensors {
 public:
  virtual ~ISensors() = default;
  virtual float microphone() const = 0;  // 0..1, амплитуда
  virtual GyroSample gyro() const = 0;
  virtual float proximity() const = 0;  // 0 = далеко, 1 = вплотную
};

}  // namespace hal
}  // namespace koto
