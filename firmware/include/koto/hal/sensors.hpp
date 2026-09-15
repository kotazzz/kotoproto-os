#pragma once

namespace koto {
namespace hal {

struct GyroSample {
  float pitch_deg = 0;
  float roll_deg = 0;
  float yaw_deg = 0;
};

// Microphone, IMU, and proximity (boop). Simulator feeds a PCM window
// (slider sine or live mic); ESP32 stub returns zeros. Firmware uses
// pitch/roll for dizzy and gyro nudge; yaw is stored and forwarded to
// /api/state but does not move pixels.
class ISensors {
 public:
  virtual ~ISensors() = default;
  virtual float microphone() const = 0;  // 0..1, RMS/peak level
  // Optional signed PCM in -1..1. 0 means amplitude-only HAL.
  virtual int copy_microphone_pcm(float* out, int max) const {
    (void)out;
    (void)max;
    return 0;
  }
  virtual GyroSample gyro() const = 0;
  virtual float proximity() const = 0;  // 0 = far, 1 = against the snout
};

}  // namespace hal
}  // namespace koto
