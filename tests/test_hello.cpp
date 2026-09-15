#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "koto/app.hpp"
#include "koto/assets/ba_player.hpp"
#include "koto/assets/bitmaps.hpp"
#include "koto/assets/casino.hpp"
#include "koto/assets/dino.hpp"
#include "koto/assets/flappy.hpp"
#include "koto/assets/tetris.hpp"
#include "koto/assets/dvd.hpp"
#include "koto/assets/bsod.hpp"
#include "koto/assets/spectrum.hpp"
#include "koto/assets/emotions.hpp"
#include "koto/config.hpp"
#include "koto/face/transition.hpp"
#include "koto/gfx/font5x7.hpp"
#include "koto/gfx/framebuffer.hpp"
#include "koto/gfx/oled_canvas.hpp"
#include "koto/gfx/pix.hpp"
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
  void reset() { ms_ = 0; }

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
  int copy_microphone_pcm(float* out, int max) const override {
    if (out == nullptr || pcm_.empty() || max <= 0) {
      return 0;
    }
    const int n = std::min(max, static_cast<int>(pcm_.size()));
    std::memcpy(out, pcm_.data(), static_cast<std::size_t>(n) * sizeof(float));
    return n;
  }
  koto::hal::GyroSample gyro() const override { return gyro_; }
  float proximity() const override { return proximity_; }
  void set_microphone(float value) { mic_ = value; }
  void set_pcm_sine(float amp) {
    pcm_.assign(static_cast<std::size_t>(koto::kMicPcmSize), 0.0f);
    for (int i = 0; i < koto::kMicPcmSize; ++i) {
      pcm_[static_cast<std::size_t>(i)] =
          amp * std::sin(6.2831853f * 220.0f * static_cast<float>(i) / 48000.0f);
    }
  }
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
  std::vector<float> pcm_;
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

bool visor_has_hud(const koto::App& app, const char* text) {
  const int w = static_cast<int>(std::strlen(text)) * (koto::gfx::kFontWidth + koto::gfx::kFontSpacing);
  koto::gfx::Framebuffer expected(64, 32);
  expected.draw_text(64 - w - 1, 1, text, koto::Color::white());
  for (int y = 1; y < 8; ++y) {
    for (int x = 64 - w - 1; x < 64; ++x) {
      const koto::Color e = expected.get_pixel(x, y);
      if ((e.r | e.g | e.b) == 0) {
        continue;
      }
      const koto::Color g = app.matrix_buffer().get_pixel(x, y);
      if (g.r != e.r || g.g != e.g || g.b != e.b) {
        return false;
      }
    }
  }
  return true;
}

int oled_text_mismatch(const koto::App& app, int x, int y, const char* text) {
  koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
  expected.draw_text(x, y, text, true);
  const int w = static_cast<int>(std::strlen(text)) * (koto::gfx::kFontWidth + koto::gfx::kFontSpacing);
  int mismatch = 0;
  for (int row = y; row < y + koto::gfx::kFontHeight; ++row) {
    for (int col = x; col < x + w; ++col) {
      if (app.oled_buffer().get_pixel(col, row) != expected.get_pixel(col, row)) {
        ++mismatch;
      }
    }
  }
  return mismatch;
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
  {
    const koto::assets::Emotion* off = koto::assets::find_emotion("PowerOff");
    require(off != nullptr && off->frame_count == 1 && off->frames[0].pix == nullptr, "PowerOff has no sprite");
    const koto::assets::Emotion* nope = koto::assets::find_emotion("NOPE");
    require(nope != nullptr && nope->frame_count == 2 && nope->frames[1].pix == nullptr, "NOPE flash-off has no sprite");
    const koto::assets::Emotion* batt = koto::assets::find_emotion("BatteryCheck");
    require(batt != nullptr && batt->frame_count == 2 && batt->frames[1].pix != nullptr,
            "BatteryCheck has two authored frames");
    const koto::assets::Emotion* hello = koto::assets::find_emotion("Startup");
    require(hello != nullptr && hello->frame_count == 1 && hello->frames[0].pix != nullptr, "Startup is one visor frame");
    const koto::assets::Emotion* dead = koto::assets::find_emotion("Dead");
    require(dead != nullptr && dead->effect == koto::assets::Effect::Glitch, "Dead uses periodic glitch");
    require(dead->accent.r > dead->accent.g && dead->accent.r > dead->accent.b, "Dead accent is red");
  }

  {
    const koto::assets::Emotion* from_e = koto::assets::find_emotion("Neutral");
    const koto::assets::Emotion* to_e = koto::assets::find_emotion("Angry");
    require(from_e != nullptr && to_e != nullptr && from_e->frame_count > 0 && to_e->frame_count > 0,
            "Neutral and Angry exist for blink transition");
    koto::gfx::Framebuffer from_fb(koto::kFaceW, koto::kFaceH);
    koto::gfx::Framebuffer to_fb(koto::kFaceW, koto::kFaceH);
    koto::gfx::Framebuffer dst(koto::kFaceW, koto::kFaceH);
    from_fb.blit_pix(0, 0, koto::kFaceW, koto::kFaceH, from_e->frames[0].pix, from_e->frames[0].size);
    to_fb.blit_pix(0, 0, koto::kFaceW, koto::kFaceH, to_e->frames[0].pix, to_e->frames[0].size);

    auto count_eye = [](const koto::gfx::Framebuffer& fb, int& red, int& cyan) {
      red = 0;
      cyan = 0;
      for (int y = 0; y < koto::kEyeH; ++y) {
        for (int x = 0; x < koto::kEyeW; ++x) {
          const koto::Color c = fb.get_pixel(x, y);
          if (c.r > 40 && c.r > c.g && c.r > c.b) {
            ++red;
          }
          if (c.b > 40 && c.b > c.r && c.g > c.r) {
            ++cyan;
          }
        }
      }
    };

    int red = 0;
    int cyan = 0;
    koto::face::apply_transition(dst, from_fb.data(), to_fb.data(), koto::kFaceW, koto::kFaceH,
                                koto::face::TransitionKind::Blink, 0.05f, 0);
    count_eye(dst, red, cyan);
    require(red > 20, "blink transition shows new eye color first");
    require(cyan == 0, "blink transition does not keep old eye color");

    koto::face::apply_transition(dst, from_fb.data(), to_fb.data(), koto::kFaceW, koto::kFaceH,
                                koto::face::TransitionKind::Blink, 0.59f, 0);
    count_eye(dst, red, cyan);
    require(red == 0 && cyan == 0, "blink transition lid covers the new eye after recolor");
  }

  {
    koto::gfx::Framebuffer hue(koto::kFaceW, koto::kFaceH);
    hue.set_pixel(0, 4, koto::Color{80, 200, 255});
    hue.set_pixel(32, 4, koto::Color{80, 200, 255});
    hue.set_pixel(10, 4, koto::Color::black());
    hue.set_pixel(0, 8, koto::Color{40, 40, 40});
    hue.hue_cycle_lit(0, 256);
    const koto::Color blank = hue.get_pixel(10, 4);
    require((blank.r | blank.g | blank.b) == 0, "hue cycle skips black pixels");
    const koto::Color left = hue.get_pixel(0, 4);
    const koto::Color mid = hue.get_pixel(32, 4);
    require(left.r > left.g && left.r > left.b, "hue cycle left column is red");
    require(mid.g > 80 && mid.b > 80 && mid.r < 40, "hue cycle mid column is cyan");
    const koto::Color dim = hue.get_pixel(0, 8);
    require(dim.r <= 40 && dim.g == 0 && dim.b == 0, "hue cycle keeps original brightness");

    const koto::assets::Emotion* neu = koto::assets::find_emotion("Neutral");
    require(neu != nullptr && neu->frame_count > 0 && neu->frames[0].pix != nullptr, "Neutral sprite");
    koto::gfx::Framebuffer neu_fb(koto::kFaceW, koto::kFaceH);
    neu_fb.blit_pix(0, 0, koto::kFaceW, koto::kFaceH, neu->frames[0].pix, neu->frames[0].size);
    int neu_lit = 0;
    for (int i = 0; i < koto::kFaceW * koto::kFaceH; ++i) {
      const koto::Color c = neu_fb.data()[i];
      if ((c.r | c.g | c.b) != 0) {
        ++neu_lit;
      }
    }
    require(neu_lit > 20, "Neutral sprite has lit pixels");
    neu_fb.hue_cycle_lit(40, 256);
    int neu_hue_lit = 0;
    int neu_hue_black = 0;
    for (int i = 0; i < koto::kFaceW * koto::kFaceH; ++i) {
      const koto::Color c = neu_fb.data()[i];
      if ((c.r | c.g | c.b) == 0) {
        ++neu_hue_black;
      } else {
        ++neu_hue_lit;
      }
    }
    require(neu_hue_lit > 20 && neu_hue_black > 100, "hue cycle on Neutral keeps silhouette");
  }

  koto::PadState skip;
  skip.buttons = koto::kBtnB;
  const auto skip_report = koto::encode_mocute_report(skip);
  app.handle_report(skip_report.data(), skip_report.size());
  require(app.state().scene == "faceset", "skip splash opens faceset");
  require(app.state().face == "Neutral", "startup ends on Neutral");

  app.calibrate_mouth();
  clock.advance(koto::kTickMs);
  app.tick();
  require(oled_text_mismatch(app, 0, 56, "Mouth cal") > 0, "mouth cal does not toast on FaceSet");
  clock.reset();
  app.restart();
  app.handle_report(skip_report.data(), skip_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  require(app.state().scene == "faceset", "restart skip splash opens faceset");
  require(oled_text_mismatch(app, 0, 56, "Mouth cal") > 0, "mouth cal toast does not survive restart");

  sensors.set_proximity(1.0f);
  for (int i = 0; i < 6; ++i) {
    clock.advance(koto::kTickMs);
    app.tick();
  }
  require(app.state().boop, "proximity triggers boop");
  const int hue_ticks =
      static_cast<int>((koto::kBoopHueHoldMs + koto::kBoopHueRampMs) / koto::kTickMs) + 4;
  for (int i = 0; i < hue_ticks; ++i) {
    clock.advance(koto::kTickMs);
    app.tick();
  }
  require(app.state().boop, "long hold keeps boop");
  {
    const koto::Color* px = app.matrix_buffer().data();
    int black = 0;
    const koto::Color* left = nullptr;
    const koto::Color* far = nullptr;
    for (int y = 0; y < 32; ++y) {
      for (int x = 0; x < 64; ++x) {
        const koto::Color& c = px[y * 64 + x];
        if ((c.r | c.g | c.b) == 0) {
          ++black;
          continue;
        }
        if (left == nullptr && x < 16) {
          left = &c;
        }
        if (x >= 24) {
          far = &c;
        }
      }
    }
    require(black > 100, "hue cycle does not fill the whole face");
    require(left != nullptr && far != nullptr, "hue cycle keeps lit face pixels");
    const koto::assets::Emotion* boop_e = koto::assets::find_emotion("Boop");
    require(boop_e != nullptr && boop_e->frame_count > 0 && boop_e->frames[0].pix != nullptr,
            "Boop sprite exists");
    koto::gfx::Framebuffer boop_fb(koto::kFaceW, koto::kFaceH);
    boop_fb.blit_pix(0, 0, koto::kFaceW, koto::kFaceH, boop_e->frames[0].pix, boop_e->frames[0].size);
    int mismatch = 0;
    for (int i = 0; i < 64 * 32; ++i) {
      const bool src_lit = (boop_fb.data()[i].r | boop_fb.data()[i].g | boop_fb.data()[i].b) != 0;
      const bool dst_lit = (px[i].r | px[i].g | px[i].b) != 0;
      if (src_lit != dst_lit) {
        ++mismatch;
      }
    }
    require(mismatch < 8, "long-hold hue stays on the Boop face");
    const int delta =
        std::abs(static_cast<int>(left->r) - static_cast<int>(far->r)) +
        std::abs(static_cast<int>(left->g) - static_cast<int>(far->g)) +
        std::abs(static_cast<int>(left->b) - static_cast<int>(far->b));
    require(delta > 80, "long-hold boop hue shifts left to right");
  }
  sensors.set_proximity(0.0f);
  for (int i = 0; i < 8; ++i) {
    clock.advance(koto::kTickMs);
    app.tick();
  }
  require(!app.state().boop, "proximity release ends boop");

  require(app.set_face("Dead", false), "Dead can be selected");
  clock.advance(koto::kTickMs);
  app.tick();
  {
    const koto::Color* px = app.matrix_buffer().data();
    int red = 0;
    for (int i = 0; i < 64 * 32; ++i) {
      if (px[i].r > 40 && px[i].r > px[i].g && px[i].r > px[i].b) {
        ++red;
      }
    }
    require(red > 20, "Dead face pixels are red");
  }
  require(app.set_face("Neutral", false), "return to Neutral after Dead");

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
  require(app.state().mouth_sensitivity == 192, "mouth sensitivity defaults high enough for speech");
  sensors.set_pcm_sine(0.7f);
  clock.advance(koto::kTickMs);
  app.tick();
  require(app.state().mic > 0.4f, "pcm sine window becomes a microphone level");

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
  require(koto::assets::find_icon("screen_matrix") != nullptr, "MATRIX page icon packed");
  require(koto::assets::find_icon("param_headphones_led") != nullptr, "switch icons packed");
  require(koto::assets::find_icon("param_mouth") != nullptr, "mouth mic icon packed");
  require(koto::assets::find_icon("game_snake") != nullptr, "snake game icon packed");
  require(koto::assets::find_icon("game_casino") != nullptr, "casino game icon packed");
  require(koto::assets::find_icon("game_dino") != nullptr, "dino game icon packed");
  require(koto::assets::find_icon("game_badapple") != nullptr, "bad apple game icon packed");
  require(koto::assets::find_icon("game_flappy") != nullptr, "flappy game icon packed");
  require(koto::assets::find_icon("game_tetris") != nullptr, "tetris game icon packed");
  require(koto::assets::find_icon("game_dvd") != nullptr, "dvd game icon packed");
  require(koto::assets::find_icon("game_bsod") != nullptr, "bsod game icon packed");
  require(koto::assets::find_icon("game_spectrum") != nullptr, "spectrum game icon packed");
  require(sizeof(koto::assets::BaHeader) == 13, "BA1P header is packed 13 bytes");
  {
    koto::assets::BaPlayer player;
    require(koto::assets::ba_init(player, koto::assets::badapple_blob(),
                                   static_cast<std::uint32_t>(koto::assets::badapple_blob_size())),
            "bad apple blob inits");
    require(player.fps == 25, "bad apple is 25 fps");
    require(player.frame_count >= 5000, "full-length bad apple");
    require(koto::assets::badapple_blob_size() > 200u * 1024u, "packed blob is not empty");
    require(koto::assets::badapple_blob_size() <= 1024u * 1024u, "packed blob fits in 1 MiB");
    require(koto::assets::ba_next(player), "first bad apple frame decodes");
  }
  {
    const std::uint8_t packed2[] = {2, 0, 0, 0, 0, 255, 255, 255, 0x40};
    require(koto::gfx::pix_at(packed2, sizeof(packed2), 2, 1, 0, 0).r == 0, "kpix packed bit0 is black");
    const koto::Color w = koto::gfx::pix_at(packed2, sizeof(packed2), 2, 1, 1, 0);
    require(w.r == 255 && w.g == 255 && w.b == 255, "kpix packed bit1 is white");
    const std::uint8_t rle2[] = {2, koto::gfx::kPixFlagRle, 0, 0, 0, 255, 0, 0, 2, 0, 2, 1};
    koto::gfx::Framebuffer rle_fb(4, 1);
    rle_fb.blit_pix(0, 0, 4, 1, rle2, sizeof(rle2), false);
    require((rle_fb.get_pixel(0, 0).r | rle_fb.get_pixel(1, 0).r) == 0, "kpix rle run of black");
    require(rle_fb.get_pixel(2, 0).r == 255 && rle_fb.get_pixel(3, 0).r == 255, "kpix rle run of red");
    const std::uint8_t four[] = {4, 0, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 0x1B};
    require(koto::gfx::pix_at(four, sizeof(four), 2, 2, 0, 0).r == 0, "kpix 2bpp index0");
    require(koto::gfx::pix_at(four, sizeof(four), 2, 2, 1, 0).r == 255, "kpix 2bpp index1");
    require(koto::gfx::pix_at(four, sizeof(four), 2, 2, 0, 1).g == 255, "kpix 2bpp index2");
    require(koto::gfx::pix_at(four, sizeof(four), 2, 2, 1, 1).b == 255, "kpix 2bpp index3");
    std::uint8_t many[2 + 18 * 3 + 4];
    many[0] = 18;
    many[1] = 0;
    for (int i = 0; i < 18; ++i) {
      many[2 + i * 3] = static_cast<std::uint8_t>(i * 7);
      many[3 + i * 3] = 0;
      many[4 + i * 3] = 0;
    }
    many[2 + 18 * 3] = 17;
    many[3 + 18 * 3] = 0;
    many[4 + 18 * 3] = 0;
    many[5 + 18 * 3] = 0;
    require(koto::gfx::pix_at(many, sizeof(many), 2, 2, 0, 0).r == 17 * 7, "kpix 8bpp first pixel");
    require(koto::assets::find_emotion("Neutral")->frames[0].pix[0] == 2, "Neutral has two colors");
    const koto::assets::CasinoSprite* seven = koto::assets::find_casino_sprite("seven");
    require(seven != nullptr && seven->pix[0] >= 3, "casino seven keeps more than two colors");
    const std::uint8_t* glyph_a = koto::gfx::glyph5x7('A');
    require(glyph_a != nullptr && !koto::gfx::glyph5x7_dot(glyph_a, 0, 0) &&
                koto::gfx::glyph5x7_dot(glyph_a, 1, 0),
            "bit font A has a peaked top");
    require(koto::gfx::glyph5x7_dot(koto::gfx::glyph5x7('?'), 0, 0), "question mark glyph is not empty");
  }
  {
    std::uint8_t mini[13 + 1 + 32 + 1 + 1] = {
        'B', 'A', '1', 'P', 64, 0, 32, 0, 25, 2, 0, 0, 1,
    };
    mini[13] = koto::assets::kBaMask;
    mini[13 + 1] = 0x80;
    mini[13 + 1 + 32] = 0xA5;
    mini[13 + 1 + 32 + 1] = 0x00;
    koto::assets::BaPlayer masked;
    require(koto::assets::ba_init(masked, mini, static_cast<std::uint32_t>(sizeof(mini))),
            "tiny BA1P with mask inits");
    require(koto::assets::ba_next(masked), "mask frame decodes");
    require(masked.pixels[0] == 0xA5, "mask writes the first changed byte");
    require(masked.pixels[1] == 0, "mask leaves other bytes");
    require(koto::assets::ba_next(masked), "hold frame after mask");
    require(masked.pixels[0] == 0xA5, "hold keeps masked byte");
  }
  {
    const koto::assets::DinoSprite* trex = koto::assets::find_dino_sprite("trex_run_0");
    const koto::assets::DinoSprite* cactus = koto::assets::find_dino_sprite("cactus_l1");
    const koto::assets::DinoSprite* bird = koto::assets::find_dino_sprite("bird_0");
    const koto::assets::DinoSprite* cloud = koto::assets::find_dino_sprite("cloud");
    require(trex != nullptr && trex->width == 14 && trex->height == 15, "trex sprite from Chromium sheet");
    require(cactus != nullptr && cactus->width == 8 && cactus->height == 16, "cactus sprite from Chromium sheet");
    require(bird != nullptr && bird->width == 15 && bird->height == 13, "bird sprite from Chromium sheet");
    require(cloud != nullptr && cloud->width == 23 && cloud->height == 7, "cloud keeps a readable silhouette");
    const koto::assets::CasinoSprite* seven = koto::assets::find_casino_sprite("seven");
    require(seven != nullptr && seven->width == 16 && seven->height == 16, "casino seven sprite packed");
    const koto::assets::FlappySprite* flappy_bird = koto::assets::find_flappy_sprite("bird_1");
    const koto::assets::FlappySprite* pipe = koto::assets::find_flappy_sprite("pipe_body");
    const koto::assets::FlappySprite* cap = koto::assets::find_flappy_sprite("pipe_cap_bot");
    require(flappy_bird != nullptr && flappy_bird->width == 12 && flappy_bird->height == 8,
            "flappy bird sprite packed");
    require(pipe != nullptr && pipe->width == 10 && pipe->height == 4, "flappy pipe body packed");
    require(cap != nullptr && cap->width == 12 && cap->height == 5, "flappy pipe cap packed");
    const koto::assets::TetrisSprite* tet = koto::assets::find_tetris_sprite("cell_t");
    const koto::assets::TetrisSprite* wall = koto::assets::find_tetris_sprite("wall");
    require(tet != nullptr && tet->width == 8 && tet->height == 8, "tetris mino tile packed");
    require(wall != nullptr && wall->width == 2 && wall->height == 8, "tetris wall tile packed");
    const koto::assets::DvdSprite* dvd = koto::assets::find_dvd_sprite("logo");
    require(dvd != nullptr && dvd->width == 24 && dvd->height == 11, "dvd logo sprite packed");
    const koto::assets::BsodSprite* sad = koto::assets::find_bsod_sprite("sad");
    const koto::assets::BsodSprite* bar = koto::assets::find_bsod_sprite("bar");
    require(sad != nullptr && sad->width == 18 && sad->height == 12, "bsod sad header packed");
    require(bar != nullptr && bar->width == 64 && bar->height == 2, "bsod bar is 64x2");
    const koto::assets::SpectrumSprite* peak = koto::assets::find_spectrum_sprite("peak");
    const koto::assets::SpectrumSprite* baseline = koto::assets::find_spectrum_sprite("baseline");
    require(peak != nullptr && peak->width == 3 && peak->height == 2, "spectrum peak marker packed");
    require(baseline != nullptr && baseline->width == 64 && baseline->height == 2,
            "spectrum baseline is 64x2");
  }
  require(app.state().scene != "snake", "snake is not in the settings menu");

  koto::PadState set_down;
  set_down.x = 128;
  set_down.y = 255;
  app.handle_report(koto::encode_mocute_report(set_down).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  {
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.fill_rect(0, 0, koto::kOledW, 16, true);
    expected.draw_text(15, 4, "BRIGHT", false);
    int mismatch = 0;
    for (int y = 4; y < 11; ++y) {
      for (int x = 15; x < 15 + 6 * 6; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "settings header shows BRIGHT when slider is selected");
  }
  const std::uint8_t brightness_before = app.state().brightness_level;
  koto::PadState set_right;
  set_right.x = 255;
  set_right.y = 128;
  app.handle_report(koto::encode_mocute_report(set_right).data(), koto::kReportSize);
  require(app.state().brightness_level == static_cast<std::uint8_t>(brightness_before + 1),
          "slider right raises brightness");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(set_down).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  koto::PadState press_ok;
  press_ok.buttons = koto::kBtnOk;
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(!app.state().matrix_enabled, "blink toggles matrix on MATRIX page");
  app.handle_report(center_report.data(), center_report.size());

  koto::PadState set_up;
  set_up.x = 128;
  set_up.y = 0;
  app.handle_report(koto::encode_mocute_report(set_up).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(set_up).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(set_right).data(), koto::kReportSize);
  require(app.state().setting_index == 1, "header right opens SWITCHES");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(set_down).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  const bool led_before = app.state().led_enabled;
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(app.state().led_enabled != led_before, "blink toggles headphones LED");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(set_right).data(), koto::kReportSize);
  require(app.state().setting_index == 1, "grid right stays on SWITCHES");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(set_up).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(set_right).data(), koto::kReportSize);
  require(app.state().setting_index == 2, "header right opens CAL/SENSE");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(set_right).data(), koto::kReportSize);
  require(app.state().setting_index == 3, "header right opens OTHER");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(set_right).data(), koto::kReportSize);
  require(app.state().setting_index == 4, "header right opens SAVE/RST");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(set_right).data(), koto::kReportSize);
  require(app.state().setting_index == 4, "settings pages do not wrap");
  app.handle_report(center_report.data(), center_report.size());

  koto::PadState press_esc_settings;
  press_esc_settings.buttons = koto::kBtnEsc;
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "faceset", "short ESC leaves settings");

  koto::PadState hold_esc;
  hold_esc.buttons = koto::kBtnEsc;
  app.handle_report(koto::encode_mocute_report(hold_esc).data(), koto::kReportSize);
  clock.advance(koto::kEscHoldMs);
  app.tick();
  require(app.state().scene == "games", "ESC hold opens games list");
  require(app.state().game_index == 0, "games list starts on Snake");
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "games", "releasing held ESC stays in games");
  require(app.oled_buffer().get_pixel(64, 0), "games header is inverted");
  require(!app.oled_buffer().get_pixel(0, 63), "games does not invert the blue band");

  koto::PadState games_right;
  games_right.x = 255;
  games_right.y = 128;
  app.handle_report(koto::encode_mocute_report(games_right).data(), koto::kReportSize);
  require(app.state().game_index == 1, "stick right selects Casino");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(app.state().scene == "casino", "blink starts casino");
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(33);
  app.tick();
  {
    const koto::Color* px = app.matrix_buffer().data();
    int lit = 0;
    for (int i = 0; i < 64 * 32; ++i) {
      if ((px[i].r | px[i].g | px[i].b) != 0) {
        ++lit;
      }
    }
    require(lit > 40, "casino reels are drawn");
  }
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  clock.advance(80);
  app.tick();
  app.handle_report(center_report.data(), center_report.size());
  for (int i = 0; i < 40; ++i) {
    clock.advance(33);
    app.tick();
  }
  require(app.state().scene == "casino", "casino stays after spin");
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "games", "ESC from casino returns to games");

  app.handle_report(koto::encode_mocute_report(games_right).data(), koto::kReportSize);
  require(app.state().game_index == 2, "stick right selects Dino");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(app.state().scene == "dino", "blink starts dino");
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  {
    const koto::Color* px = app.matrix_buffer().data();
    int lit = 0;
    for (int i = 0; i < 64 * 32; ++i) {
      if ((px[i].r | px[i].g | px[i].b) != 0) {
        ++lit;
      }
    }
    require(lit > 20, "waiting dino and horizon are drawn");
    require(visor_has_hud(app, "GO"), "waiting dino visor shows GO");
  }
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  for (int i = 0; i < 160; ++i) {
    clock.advance(koto::kTickMs);
    app.tick();
  }
  {
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.draw_text((koto::kOledW - 48) / 2, 1, "DEAD", true, 2);
    int mismatch = 0;
    for (int y = 1; y < 15; ++y) {
      for (int x = (koto::kOledW - 48) / 2; x < (koto::kOledW - 48) / 2 + 48; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "dino header shows DEAD after crash");
    require(visor_has_hud(app, "DEAD"), "dino visor shows DEAD after crash");
  }
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "games", "ESC from dino returns to games");

  app.handle_report(koto::encode_mocute_report(games_right).data(), koto::kReportSize);
  require(app.state().game_index == 3, "stick right selects Bad Apple");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(app.state().scene == "badapple", "blink starts bad apple");
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  {
    koto::gfx::OledCanvas unexpected(koto::kOledW, koto::kOledH);
    unexpected.draw_text((koto::kOledW - 24) / 2, 42, "2.5X", true);
    int hits = 0;
    for (int y = 42; y < 49; ++y) {
      for (int x = (koto::kOledW - 24) / 2; x < (koto::kOledW - 24) / 2 + 24; ++x) {
        if (unexpected.get_pixel(x, y) && app.oled_buffer().get_pixel(x, y)) {
          ++hits;
        }
      }
    }
    require(hits == 0, "bad apple starts at 1x without 2.5X");
  }
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  {
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.draw_text((koto::kOledW - 24) / 2, 42, "2.5X", true);
    int mismatch = 0;
    for (int y = 42; y < 49; ++y) {
      for (int x = (koto::kOledW - 24) / 2; x < (koto::kOledW - 24) / 2 + 24; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "blink turns on 2.5X playback");
  }
  for (int i = 0; i < 20; ++i) {
    clock.advance(koto::kTickMs);
    app.tick();
  }
  {
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.draw_text((koto::kOledW - 24) / 2, 28, "0:01", true);
    int mismatch = 0;
    for (int y = 28; y < 35; ++y) {
      for (int x = (koto::kOledW - 24) / 2; x < (koto::kOledW - 24) / 2 + 24; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "2.5x bad apple reaches 0:01 after 20 ticks");
  }
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  {
    koto::gfx::OledCanvas unexpected(koto::kOledW, koto::kOledH);
    unexpected.draw_text((koto::kOledW - 24) / 2, 42, "2.5X", true);
    int hits = 0;
    for (int y = 42; y < 49; ++y) {
      for (int x = (koto::kOledW - 24) / 2; x < (koto::kOledW - 24) / 2 + 24; ++x) {
        if (unexpected.get_pixel(x, y) && app.oled_buffer().get_pixel(x, y)) {
          ++hits;
        }
      }
    }
    require(hits == 0, "second blink turns off 2.5X");
  }
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "games", "ESC from bad apple returns to games");

  app.handle_report(koto::encode_mocute_report(games_right).data(), koto::kReportSize);
  require(app.state().game_index == 4, "stick right selects Flappy");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(app.state().scene == "flappy", "blink starts flappy");
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  {
    const koto::Color* px = app.matrix_buffer().data();
    int lit = 0;
    for (int i = 0; i < 64 * 32; ++i) {
      if ((px[i].r | px[i].g | px[i].b) != 0) {
        ++lit;
      }
    }
    require(lit > 20, "waiting flappy bird and ground are drawn");
    require(visor_has_hud(app, "GO"), "waiting flappy visor shows GO");
  }
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  for (int i = 0; i < 80; ++i) {
    clock.advance(koto::kTickMs);
    app.tick();
  }
  {
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.draw_text((koto::kOledW - 48) / 2, 1, "DEAD", true, 2);
    int mismatch = 0;
    for (int y = 1; y < 15; ++y) {
      for (int x = (koto::kOledW - 48) / 2; x < (koto::kOledW - 48) / 2 + 48; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "flappy header shows DEAD after crash");
    require(visor_has_hud(app, "DEAD"), "flappy visor shows DEAD after crash");
  }
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "games", "ESC from flappy returns to games");

  app.handle_report(koto::encode_mocute_report(games_right).data(), koto::kReportSize);
  require(app.state().game_index == 5, "stick right selects Tetris");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(app.state().scene == "tetris", "blink starts tetris");
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  {
    const koto::Color* px = app.matrix_buffer().data();
    int lit = 0;
    for (int i = 0; i < 64 * 32; ++i) {
      if ((px[i].r | px[i].g | px[i].b) != 0) {
        ++lit;
      }
    }
    require(lit > 10, "waiting tetris well is drawn");
    require(visor_has_hud(app, "GO"), "waiting tetris visor shows GO");
  }
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  {
    koto::PadState drop;
    drop.x = 128;
    drop.y = 255;
    app.handle_report(koto::encode_mocute_report(drop).data(), koto::kReportSize);
  }
  for (int i = 0; i < 250; ++i) {
    clock.advance(koto::kTickMs);
    app.tick();
  }
  {
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.draw_text((koto::kOledW - 48) / 2, 1, "DEAD", true, 2);
    int mismatch = 0;
    for (int y = 1; y < 15; ++y) {
      for (int x = (koto::kOledW - 48) / 2; x < (koto::kOledW - 48) / 2 + 48; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "tetris header shows DEAD after crash");
    require(visor_has_hud(app, "DEAD"), "tetris visor shows DEAD after crash");
  }
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "games", "ESC from tetris returns to games");

  app.handle_report(koto::encode_mocute_report(games_right).data(), koto::kReportSize);
  require(app.state().game_index == 6, "stick right selects DVD");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(app.state().scene == "dvd", "blink starts dvd");
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  koto::Color dvd_color{};
  {
    const koto::Color* px = app.matrix_buffer().data();
    int lit = 0;
    for (int i = 0; i < 64 * 32; ++i) {
      if ((px[i].r | px[i].g | px[i].b) != 0) {
        if (lit == 0) {
          dvd_color = px[i];
        }
        ++lit;
      }
    }
    require(lit > 20, "waiting dvd logo is drawn");
  }
  require(app.state().dvd_hits == 0, "dvd starts with no bounces");
  for (int i = 0; i < 25; ++i) {
    clock.advance(koto::kTickMs);
    app.tick();
  }
  require(app.state().dvd_hits >= 1, "dvd counts a wall hit");
  {
    const koto::Color* px = app.matrix_buffer().data();
    koto::Color next{};
    bool found = false;
    for (int i = 0; i < 64 * 32; ++i) {
      if ((px[i].r | px[i].g | px[i].b) != 0) {
        next = px[i];
        found = true;
        break;
      }
    }
    require(found, "dvd logo still lit after bounce");
    require(next.r != dvd_color.r || next.g != dvd_color.g || next.b != dvd_color.b,
            "dvd logo changes color on collision");
  }
  {
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.fill_rect(0, 0, koto::kOledW, 16, true);
    expected.draw_text((koto::kOledW - 18) / 2, 4, "DVD", false);
    int mismatch = 0;
    for (int y = 4; y < 11; ++y) {
      for (int x = (koto::kOledW - 18) / 2; x < (koto::kOledW - 18) / 2 + 18; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "dvd header shows DVD");
  }
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "games", "ESC from dvd returns to games");

  app.handle_report(koto::encode_mocute_report(games_right).data(), koto::kReportSize);
  require(app.state().game_index == 7, "stick right selects BSOD");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(app.state().scene == "bsod", "blink starts bsod");
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  int white0 = 0;
  int blue = 0;
  {
    const koto::Color* px = app.matrix_buffer().data();
    for (int i = 0; i < 64 * 32; ++i) {
      if (px[i].r == 0 && px[i].g == 120 && px[i].b == 215) {
        ++blue;
      } else if (px[i].r == 255 && px[i].g == 255 && px[i].b == 255) {
        ++white0;
      }
    }
    require(blue > 100, "bsod fills the visor with Windows blue");
    require(white0 > 10, "bsod draws the Windows header in white");
  }
  require(app.state().bsod_bars == 0, "bsod starts with no bars");
  for (int i = 0; i < 20; ++i) {
    clock.advance(koto::kTickMs);
    app.tick();
  }
  require(app.state().bsod_bars >= 2, "bsod draws a sequence of bars");
  {
    const koto::Color* px = app.matrix_buffer().data();
    int white1 = 0;
    for (int i = 0; i < 64 * 32; ++i) {
      if (px[i].r == 255 && px[i].g == 255 && px[i].b == 255) {
        ++white1;
      }
    }
    require(white1 > white0, "bsod bars add white rows");
  }
  {
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.fill_rect(0, 0, koto::kOledW, 16, true);
    expected.draw_text((koto::kOledW - 24) / 2, 4, "BSOD", false);
    int mismatch = 0;
    for (int y = 4; y < 11; ++y) {
      for (int x = (koto::kOledW - 24) / 2; x < (koto::kOledW - 24) / 2 + 24; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "bsod header shows BSOD");
  }
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "games", "ESC from bsod returns to games");

  app.handle_report(koto::encode_mocute_report(games_right).data(), koto::kReportSize);
  require(app.state().game_index == 8, "stick right selects Spectrum");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(app.state().scene == "spectrum", "blink starts spectrum");
  app.handle_report(center_report.data(), center_report.size());
  sensors.set_pcm_sine(0.8f);
  for (int i = 0; i < 8; ++i) {
    clock.advance(koto::kTickMs);
    app.tick();
  }
  {
    const koto::Color* px = app.matrix_buffer().data();
    int low = 0;
    int high = 0;
    for (int y = 0; y < koto::kSpectrumPlotH; ++y) {
      for (int x = 0; x < 16; ++x) {
        const koto::Color c = px[y * 64 + x];
        if ((c.r | c.g | c.b) != 0) {
          ++low;
        }
      }
      for (int x = 48; x < 64; ++x) {
        const koto::Color c = px[y * 64 + x];
        if ((c.r | c.g | c.b) != 0) {
          ++high;
        }
      }
    }
    require(low > 20, "spectrum lights low-frequency bars");
    require(low > high, "one-cycle sine energy sits on the left");
  }
  {
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.fill_rect(0, 0, koto::kOledW, 16, true);
    expected.draw_text((koto::kOledW - 48) / 2, 4, "SPECTRUM", false);
    int mismatch = 0;
    for (int y = 4; y < 11; ++y) {
      for (int x = (koto::kOledW - 48) / 2; x < (koto::kOledW - 48) / 2 + 48; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "spectrum header shows SPECTRUM");
  }
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "games", "ESC from spectrum returns to games");

  koto::PadState games_left;
  games_left.x = 0;
  games_left.y = 128;
  app.handle_report(koto::encode_mocute_report(games_left).data(), koto::kReportSize);
  require(app.state().game_index == 7, "stick left from Spectrum selects BSOD");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(games_left).data(), koto::kReportSize);
  require(app.state().game_index == 6, "stick left from BSOD selects DVD");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(games_left).data(), koto::kReportSize);
  require(app.state().game_index == 5, "stick left from DVD selects Tetris");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(games_left).data(), koto::kReportSize);
  require(app.state().game_index == 4, "stick left from Tetris selects Flappy");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(games_left).data(), koto::kReportSize);
  require(app.state().game_index == 3, "stick left from Flappy selects Bad Apple");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(games_left).data(), koto::kReportSize);
  require(app.state().game_index == 2, "stick left from Bad Apple selects Dino");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(games_left).data(), koto::kReportSize);
  require(app.state().game_index == 1, "stick left from Dino selects Casino");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(games_left).data(), koto::kReportSize);
  require(app.state().game_index == 0, "stick left selects Snake");
  app.handle_report(center_report.data(), center_report.size());
  app.handle_report(koto::encode_mocute_report(press_ok).data(), koto::kReportSize);
  require(app.state().scene == "snake", "blink starts snake");
  app.handle_report(center_report.data(), center_report.size());
  clock.advance(koto::kTickMs);
  app.tick();
  require(app.oled_buffer().get_pixel(0, 16), "snake field top border");
  require(app.oled_buffer().get_pixel(0, 63), "snake field bottom border");
  require(app.oled_buffer().get_pixel(127, 16), "snake field top-right border");
  require(app.oled_buffer().get_pixel(127, 63), "snake field bottom-right border");
  {
    const koto::Color* mx = app.matrix_buffer().data();
    require((mx[0].r | mx[0].g | mx[0].b) != 0, "snake matrix has left border");
    const koto::Color a = mx[15 * 64 + 31];
    const koto::Color b = mx[16 * 64 + 32];
    require((a.r | a.g | a.b) != 0 && (b.r | b.g | b.b) != 0, "snake matrix cells are 2x2");
  }
  {
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.draw_text((koto::kOledW - 12) / 2, 1, "0", true, 2);
    int mismatch = 0;
    for (int y = 1; y < 15; ++y) {
      for (int x = (koto::kOledW - 12) / 2; x < (koto::kOledW - 12) / 2 + 12; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "snake header shows score instead of emotion name");
  }
  {
    koto::PadState crash;
    crash.x = 0;
    crash.y = 128;
    app.handle_report(koto::encode_mocute_report(crash).data(), koto::kReportSize);
    for (int i = 0; i < 20; ++i) {
      clock.advance(200);
      app.tick();
    }
    koto::gfx::OledCanvas expected(koto::kOledW, koto::kOledH);
    expected.draw_text((koto::kOledW - 48) / 2, 1, "DEAD", true, 2);
    int mismatch = 0;
    for (int y = 1; y < 15; ++y) {
      for (int x = (koto::kOledW - 48) / 2; x < (koto::kOledW - 48) / 2 + 48; ++x) {
        if (app.oled_buffer().get_pixel(x, y) != expected.get_pixel(x, y)) {
          ++mismatch;
        }
      }
    }
    require(mismatch == 0, "snake header shows DEAD after game over");
  }
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "games", "ESC from snake returns to games");
  app.handle_report(koto::encode_mocute_report(press_esc_settings).data(), koto::kReportSize);
  app.handle_report(center_report.data(), center_report.size());
  require(app.state().scene == "faceset", "short ESC leaves games list");

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
