#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "koto/app.hpp"
#include "koto/config.hpp"
#include "koto/hal/clock.hpp"
#include "koto/hal/fan.hpp"
#include "koto/hal/hid_host.hpp"
#include "koto/hal/led_ring.hpp"
#include "koto/hal/matrix.hpp"
#include "koto/hal/oled.hpp"
#include "koto/hal/sensors.hpp"
#include "koto/hal/store.hpp"
#include "koto/protocol/mocute.hpp"
#include "koto/settings.hpp"

namespace {

class MockClock final : public koto::hal::IClock {
 public:
  std::uint32_t millis() const override { return ms_; }
  void advance(std::uint32_t dt) { ms_ += dt; }

 private:
  std::uint32_t ms_ = 0;
};

class MockMatrix final : public koto::hal::IMatrix {
 public:
  MockMatrix(int w, int h) : w_(w), h_(h), rgb_(static_cast<std::size_t>(w * h * 3), 0) {}
  int width() const override { return w_; }
  int height() const override { return h_; }
  void present(const koto::Color* pixels, int width, int height) override {
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        const koto::Color c = pixels[y * width + x];
        const std::size_t i = static_cast<std::size_t>((y * w_ + x) * 3);
        rgb_[i] = c.r;
        rgb_[i + 1] = c.g;
        rgb_[i + 2] = c.b;
      }
    }
  }
  bool any_lit() const {
    for (std::uint8_t v : rgb_) {
      if (v != 0) {
        return true;
      }
    }
    return false;
  }

 private:
  int w_;
  int h_;
  std::vector<std::uint8_t> rgb_;
};

class MockOled final : public koto::hal::IOled {
 public:
  MockOled(int w, int h) : w_(w), h_(h), bits_(static_cast<std::size_t>(((w + 7) / 8) * h), 0) {}
  int width() const override { return w_; }
  int height() const override { return h_; }
  void present(const std::uint8_t* packed, int, int height) override {
    const int bpr = (w_ + 7) / 8;
    for (int y = 0; y < height; ++y) {
      for (int b = 0; b < bpr; ++b) {
        bits_[static_cast<std::size_t>(y * bpr + b)] = packed[y * bpr + b];
      }
    }
  }
  bool any_lit() const {
    for (std::uint8_t v : bits_) {
      if (v != 0) {
        return true;
      }
    }
    return false;
  }

 private:
  int w_;
  int h_;
  std::vector<std::uint8_t> bits_;
};

class MockLedRing final : public koto::hal::ILedRing {
 public:
  int size() const override { return koto::hal::kLedRingCount; }
  void present(const koto::Color* leds, int count) override {
    rgb_.assign(static_cast<std::size_t>(count * 3), 0);
    for (int i = 0; i < count; ++i) {
      rgb_[static_cast<std::size_t>(i * 3)] = leds[i].r;
      rgb_[static_cast<std::size_t>(i * 3 + 1)] = leds[i].g;
      rgb_[static_cast<std::size_t>(i * 3 + 2)] = leds[i].b;
    }
  }
  bool any_lit() const {
    for (std::uint8_t v : rgb_) {
      if (v != 0) {
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<std::uint8_t> rgb_;
};

class MockHid final : public koto::hal::IHidHost {
 public:
  void start() override { started_ = true; }
  void set_report_handler(ReportHandler handler) override { handler_ = std::move(handler); }
  bool connected() const override { return connected_; }
  bool started_ = false;
  bool connected_ = true;
  ReportHandler handler_;
};

class MockStore final : public koto::hal::IStore {
 public:
  bool load(std::uint8_t* data, std::size_t size) override {
    if (!has_) {
      return false;
    }
    if (size > blob_.size()) {
      return false;
    }
    std::memcpy(data, blob_.data(), size);
    return true;
  }
  bool save(const std::uint8_t* data, std::size_t size) override {
    blob_.assign(data, data + size);
    has_ = true;
    return true;
  }

 private:
  bool has_ = false;
  std::vector<std::uint8_t> blob_;
};

class MockSensors final : public koto::hal::ISensors {
 public:
  float microphone() const override { return mic_; }
  koto::hal::GyroSample gyro() const override { return gyro_; }
  float proximity() const override { return proximity_; }
  void set_microphone(float value) { mic_ = value; }
  void set_gyro(float pitch, float roll, float yaw) {
    gyro_.pitch_deg = pitch;
    gyro_.roll_deg = roll;
    gyro_.yaw_deg = yaw;
  }
  void set_proximity(float value) { proximity_ = value; }

 private:
  float mic_ = 0;
  float proximity_ = 0;
  koto::hal::GyroSample gyro_{};
};

class MockFan final : public koto::hal::IFan {
 public:
  void set_speed(std::uint8_t duty) override { duty_ = duty; }
  std::uint8_t speed() const override { return duty_; }

 private:
  std::uint8_t duty_ = 255;
};

void require(bool cond, const char* what) {
  if (!cond) {
    std::cerr << "FAIL: " << what << "\n";
    std::exit(1);
  }
}

}  // namespace

int main() {
  require((koto::SettingsBlob{}.flags & koto::kFlagMouth) != 0, "default blob enables mouth");

  koto::PadState pad;
  pad.x = 200;
  pad.y = 10;
  pad.buttons = koto::kBtnA | koto::kBtnSelect;
  pad.mode = koto::PadMode::Game;
  const auto encoded = koto::encode_mocute_report(pad);
  const koto::PadState decoded = koto::parse_mocute_report(encoded.data(), encoded.size());
  require(decoded.x == 200, "encode x");
  require(decoded.a(), "encode A");
  require(decoded.select(), "encode SELECT");
  require(decoded.right(), "hat from stick right");

  std::uint8_t keyboard[8] = {0, 0, 0x52, 0x28, 0, 0, 0, 0};
  const koto::PadState key = koto::parse_mocute_report(keyboard, 8);
  require(key.mode == koto::PadMode::Key, "KEY mode");
  require(key.y == 0, "KEY up");
  require(key.ok() && key.a(), "KEY enter = OK/A");

  std::uint8_t keyboard_esc[8] = {0, 0, 0x29, 0, 0, 0, 0, 0};
  const koto::PadState key_esc = koto::parse_mocute_report(keyboard_esc, 8);
  require(key_esc.esc() && !key_esc.b(), "KEY Esc is Esc only");

  MockMatrix matrix(64, 32);
  MockOled oled(128, 64);
  MockLedRing ring;
  MockHid hid;
  MockClock clock;
  MockSensors sensors;
  MockFan fan;
  MockStore store;
  koto::App app(matrix, oled, ring, hid, clock, sensors, fan, store);
  app.init();
  require(hid.started_, "hid start");
  require(matrix.any_lit(), "matrix has pixels after init");
  require(oled.any_lit(), "oled has pixels after init");
  require(ring.any_lit(), "led ring has pixels after init");
  require(app.state().scene == "startup", "boot splash");

  koto::PadState skip;
  skip.buttons = koto::kBtnB;
  const auto skip_report = koto::encode_mocute_report(skip);
  app.handle_report(skip_report.data(), skip_report.size());
  require(app.state().scene == "faceset", "skip splash opens faceset");
  require(app.state().face == "Neutral", "startup ends on Neutral");

  koto::PadState press_x;
  press_x.buttons = koto::kBtnX;
  const auto x_report = koto::encode_mocute_report(press_x);
  app.handle_report(x_report.data(), x_report.size());
  require(app.state().faceset == 1, "X selects face set 1");

  koto::PadState stick_left;
  stick_left.x = 0;
  stick_left.y = 128;
  const auto left_report = koto::encode_mocute_report(stick_left);
  app.handle_report(left_report.data(), left_report.size());
  koto::PadState stick_center;
  const auto center_report = koto::encode_mocute_report(stick_center);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().face == "JoyBlush", "stick octant 0 maps to JoyBlush");
  require(app.state().mouth_enabled, "microphone on by default");

  koto::PadState hover_right;
  hover_right.x = 255;
  hover_right.y = 128;
  app.handle_report(koto::encode_mocute_report(hover_right).data(), koto::kReportSize);
  require(app.state().face == "JoyBlush", "hover does not apply face yet");

  koto::PadState hover_blink;
  hover_blink.x = 255;
  hover_blink.y = 128;
  hover_blink.buttons = koto::kBtnOk;
  app.handle_report(koto::encode_mocute_report(hover_blink).data(), koto::kReportSize);
  require(!app.state().blinking, "blink ignored while stick is off-center");

  koto::PadState center_blink;
  center_blink.buttons = koto::kBtnOk;
  app.handle_report(koto::encode_mocute_report(center_blink).data(), koto::kReportSize);
  require(app.state().face == "JoyBlush", "blink held cancels face change on stick return");
  require(!app.state().blinking, "blink does not start after stick cancel");

  koto::PadState angry_stick;
  angry_stick.x = 255;
  angry_stick.y = 255;
  app.handle_report(koto::encode_mocute_report(angry_stick).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().face == "Angry", "stick octant 5 maps to Angry");
  clock.advance(33);
  app.tick();
  {
    const koto::Color* px = app.matrix_buffer().data();
    int mouth_lit = 0;
    for (int y = 16; y < 32; ++y) {
      for (int x = 0; x < 64; ++x) {
        const koto::Color c = px[y * 64 + x];
        if ((c.r | c.g | c.b) != 0) {
          ++mouth_lit;
        }
      }
    }
    require(mouth_lit > 40, "Angry mouth is drawn");
  }

  koto::PadState esc_faceset;
  esc_faceset.buttons = koto::kBtnEsc;
  app.handle_report(koto::encode_mocute_report(esc_faceset).data(), koto::kReportSize);
  require(app.state().scene == "faceset", "ESC on main page does not leave faceset");

  koto::PadState press_y;
  press_y.buttons = koto::kBtnY;
  const auto y_report = koto::encode_mocute_report(press_y);
  app.handle_report(y_report.data(), y_report.size());
  require(app.state().faceset == 3, "Y selects face set 3");

  koto::PadState press_menu;
  press_menu.buttons = koto::kBtnSelect;
  app.handle_report(koto::encode_mocute_report(press_menu).data(), koto::kReportSize);
  koto::PadState release_menu;
  app.handle_report(koto::encode_mocute_report(release_menu).data(), koto::kReportSize);
  require(app.state().scene == "settings", "MENU opens BT status");
  require(app.state().setting_index == koto::kSettingStatus1, "first MENU is BT screen");

  app.handle_report(koto::encode_mocute_report(press_menu).data(), koto::kReportSize);
  app.handle_report(koto::encode_mocute_report(release_menu).data(), koto::kReportSize);
  require(app.state().setting_index == koto::kSettingStatus2, "second MENU is Frame screen");

  app.handle_report(koto::encode_mocute_report(press_menu).data(), koto::kReportSize);
  app.handle_report(koto::encode_mocute_report(release_menu).data(), koto::kReportSize);
  require(app.state().setting_index == 0, "third MENU is settings list");

  app.handle_report(koto::encode_mocute_report(press_menu).data(), koto::kReportSize);
  app.handle_report(koto::encode_mocute_report(release_menu).data(), koto::kReportSize);
  require(app.state().setting_index == koto::kSettingStatus1, "fourth MENU returns to BT");

  koto::PadState hold_menu;
  press_x.buttons = koto::kBtnX;
  app.handle_report(koto::encode_mocute_report(press_x).data(), koto::kReportSize);
  require(app.state().scene == "faceset", "X leaves status to faceset");
  hold_menu.buttons = koto::kBtnSelect;
  app.handle_report(koto::encode_mocute_report(hold_menu).data(), koto::kReportSize);
  clock.advance(koto::kMenuHoldMs);
  app.tick();
  require(app.state().scene == "settings", "MENU hold opens settings");
  require(app.state().setting_index == 0, "MENU hold skips BT/Frame");

  koto::PadState press_auto;
  press_auto.buttons = koto::kBtnB;
  const auto auto_report = koto::encode_mocute_report(press_auto);
  app.handle_report(auto_report.data(), auto_report.size());
  require(app.state().scene == "auto", "B starts automatic faces");

  sensors.set_proximity(1.0f);
  for (int i = 0; i < 6; ++i) {
    clock.advance(33);
    app.tick();
  }
  require(app.state().boop, "proximity triggers boop");

  sensors.set_proximity(0.0f);
  for (int i = 0; i < 8; ++i) {
    clock.advance(33);
    app.tick();
  }
  require(!app.state().boop, "proximity release ends boop");

  sensors.set_gyro(48.0f, -10.0f, 0.0f);
  clock.advance(33);
  app.tick();
  require(app.state().dizzy, "gyro tilt triggers dizzy");

  app.set_fan_speed(102);
  require(app.state().fan_speed == 102, "fan pwm duty");

  std::cout << "ok hello world core\n";
  return 0;
}
