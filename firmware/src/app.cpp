#include "koto/app.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "koto/assets/bitmaps.hpp"
#include "koto/assets/emotions.hpp"
#include "koto/config.hpp"
#include "koto/gfx/font5x7.hpp"
#include "koto/settings.hpp"
#include "koto/version.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace koto {
namespace {

constexpr int kSnakeW = 30;
constexpr int kSnakeH = 6;
constexpr int kSnakeOx = 2;
constexpr int kSnakeOy = 18;
constexpr int kSnakeCell = 2;
constexpr std::uint32_t kSnakeStepMs = 200;
constexpr std::uint32_t kSnakeBlinkMs = 100;

const int kThumbPos[8][2] = {
    {0, 32}, {0, 16}, {43, 16}, {86, 16}, {86, 32}, {86, 48}, {43, 48}, {0, 48},
};

struct MenuItem {
  const char* category;
  const char* name;
  enum Kind { Percent, Toggle, Action } kind;
  int min_value;
  int max_value;
  int step;
};

const MenuItem kMenu[kSettingCount] = {
    {"Matrix", "Brightness", MenuItem::Percent, 0, 15, 1},
    {"Matrix", "Enable", MenuItem::Toggle, 0, 1, 1},
    {"LED Strip", "Enable", MenuItem::Toggle, 0, 1, 1},
    {"Auto blink", "Enable", MenuItem::Toggle, 0, 1, 1},
    {"Boop sensor", "Enable", MenuItem::Toggle, 0, 1, 1},
    {"Boop sensor", "Sensitivity", MenuItem::Percent, 0, 255, 17},
    {"Boop sensor", "Calibrate", MenuItem::Action, 0, 1, 1},
    {"Mouth anim.", "Enable", MenuItem::Toggle, 0, 1, 1},
    {"Mouth anim.", "Calibrate", MenuItem::Action, 0, 1, 1},
    {"Rare transit.", "Chance", MenuItem::Percent, 0, 255, 17},
    {"Fan speed", "", MenuItem::Percent, 0, 255, 51},
    {"Misc.", "Snake game", MenuItem::Action, 0, 1, 1},
    {"System", "Restart", MenuItem::Action, 0, 1, 1},
    {"System", "Save settings", MenuItem::Action, 0, 1, 1},
};

bool pressed(bool now, bool was) {
  return now && !was;
}

bool released(bool now, bool was) {
  return !now && was;
}

bool stick_active(const PadState& pad) {
  return std::abs(static_cast<int>(pad.x) - 128) >= kStickDeadzone ||
         std::abs(static_cast<int>(pad.y) - 128) >= kStickDeadzone;
}

int stick_octant(const PadState& pad) {
  const int dx = 128 - static_cast<int>(pad.x);
  const int dy = 128 - static_cast<int>(pad.y);
  if (std::abs(dx) < kStickDeadzone && std::abs(dy) < kStickDeadzone) {
    return -1;
  }
  double angle = std::atan2(static_cast<double>(dy), static_cast<double>(dx)) * (180.0 / M_PI);
  if (angle < 0) {
    angle += 360.0;
  }
  return static_cast<int>((angle + 22.5) / 45.0) % 8;
}

bool rgb_blank(const assets::EmotionFrame* frame) {
  if (frame == nullptr || frame->rgb == nullptr) {
    return true;
  }
  for (std::size_t i = 0; i < frame->size; ++i) {
    if (frame->rgb[i] != 0) {
      return false;
    }
  }
  return true;
}

bool rgb_hot(const assets::EmotionFrame* frame) {
  if (frame == nullptr || frame->rgb == nullptr || frame->size < 3) {
    return false;
  }
  const int pixels = static_cast<int>(frame->size / 3);
  int lit = 0;
  for (int i = 0; i < pixels; ++i) {
    const std::size_t o = static_cast<std::size_t>(i * 3);
    if ((frame->rgb[o] | frame->rgb[o + 1] | frame->rgb[o + 2]) != 0) {
      ++lit;
    }
  }
  return lit * 10 > pixels * 7;
}

const assets::Emotion* lookup_emotion(const char* id) {
  const assets::Emotion* found = assets::find_emotion(id);
  if (found != nullptr) {
    return found;
  }
  return assets::find_emotion("Neutral");
}

const assets::EmotionFrame* frame_at(const assets::Emotion* emotion, int index) {
  if (emotion == nullptr || emotion->frames == nullptr || emotion->frame_count <= 0) {
    return nullptr;
  }
  const int clamped = std::clamp(index, 0, emotion->frame_count - 1);
  return &emotion->frames[clamped];
}

std::string format_uptime(std::uint32_t ms) {
  const std::uint32_t total_sec = ms / 1000;
  const std::uint32_t sec = total_sec % 60;
  const std::uint32_t min = (total_sec / 60) % 60;
  const std::uint32_t hour = total_sec / 3600;
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u", hour, min, sec);
  return std::string(buf);
}

void snapshot(std::vector<Color>& out, const gfx::Framebuffer& fb) {
  const int n = fb.width() * fb.height();
  out.assign(fb.data(), fb.data() + n);
}

const char* hud_label(const assets::Emotion* emotion) {
  if (emotion == nullptr) {
    return "Neutral";
  }
  return emotion->short_label != nullptr ? emotion->short_label : emotion->id;
}

}  // namespace

App::App(hal::IMatrix& matrix,
         hal::IOled& oled,
         hal::ILedRing& ring,
         hal::IHidHost& hid,
         hal::IClock& clock,
         hal::ISensors& sensors,
         hal::IFan& fan,
         hal::IStore& store)
    : matrix_(matrix),
      oled_(oled),
      ring_(ring),
      hid_(hid),
      clock_(clock),
      sensors_(sensors),
      fan_(fan),
      store_(store),
      matrix_fb_(matrix.width(), matrix.height()),
      oled_fb_(oled.width(), oled.height()) {}

void App::sync_brightness() {
  state_.brightness = static_cast<std::uint8_t>(state_.brightness_level * 17);
}

bool App::load_settings() {
  SettingsBlob blob{};
  if (!store_.load(reinterpret_cast<std::uint8_t*>(&blob), sizeof(blob)) || !settings_valid(blob)) {
    return false;
  }
  state_.brightness_level = static_cast<std::uint8_t>(std::min<int>(blob.brightness, 15));
  state_.matrix_enabled = (blob.flags & kFlagMatrix) != 0;
  state_.led_enabled = (blob.flags & kFlagLed) != 0;
  state_.auto_blink = (blob.flags & kFlagBlink) != 0;
  state_.boop_enabled = (blob.flags & kFlagBoop) != 0;
  state_.mouth_enabled = (blob.flags & kFlagMouth) != 0;
  state_.boop_sensitivity = blob.boop_sensitivity;
  state_.rare_chance = blob.rare_chance;
  state_.fan_speed = blob.fan_speed;
  sync_brightness();
  fan_.set_speed(state_.fan_speed);
  return true;
}

bool App::save_settings() {
  SettingsBlob blob;
  blob.brightness = state_.brightness_level;
  blob.flags = 0;
  if (state_.matrix_enabled) {
    blob.flags = static_cast<std::uint8_t>(blob.flags | kFlagMatrix);
  }
  if (state_.led_enabled) {
    blob.flags = static_cast<std::uint8_t>(blob.flags | kFlagLed);
  }
  if (state_.auto_blink) {
    blob.flags = static_cast<std::uint8_t>(blob.flags | kFlagBlink);
  }
  if (state_.boop_enabled) {
    blob.flags = static_cast<std::uint8_t>(blob.flags | kFlagBoop);
  }
  if (state_.mouth_enabled) {
    blob.flags = static_cast<std::uint8_t>(blob.flags | kFlagMouth);
  }
  blob.boop_sensitivity = state_.boop_sensitivity;
  blob.rare_chance = state_.rare_chance;
  blob.fan_speed = state_.fan_speed;
  settings_seal(blob);
  return store_.save(reinterpret_cast<const std::uint8_t*>(&blob), sizeof(blob));
}

void App::init() {
  hid_.set_report_handler([this](const std::uint8_t* data, std::size_t len) {
    handle_report(data, len);
  });
  hid_.start();
  load_settings();
  fan_.set_speed(state_.fan_speed);
  auto_next_ = lookup_emotion("Joy");
  startup_started_ms_ = clock_.millis();
  set_sequence("Startup", false, false);
  state_.scene = "startup";
  tick();
}

void App::restart() {
  trans_kind_ = face::TransitionKind::None;
  blink_ = BlinkState::Idle;
  blink_held_ = false;
  joystick_centered_ = true;
  prev_boop_ = false;
  action_done_ = false;
  frame_index_ = 0;
  boop_triggers_ = 0;
  mouth_level_ = 0;
  snake_reset();
  load_settings();
  mode_ = Mode::Startup;
  startup_started_ms_ = clock_.millis();
  set_sequence("Startup", false, false);
  state_.scene = "startup";
  tick();
}

void App::handle_report(const std::uint8_t* data, std::size_t len) {
  apply_pad(parse_mocute_report(data, len));
}

void App::calibrate_boop() {
  boop_calibrate_left_ = 16;
  boop_cal_acc_ = 0;
  calib_percent_ = 0;
  action_done_ = false;
  std::snprintf(toast_, sizeof(toast_), "Boop cal");
  toast_until_ms_ = clock_.millis() + 1200;
}

void App::calibrate_mouth() {
  mouth_calibrate_left_ = 8;
  mouth_baseline_acc_ = 0;
  mouth_level_ = 0;
  mouth_peak_ = 0.35f;
  calib_percent_ = 0;
  action_done_ = false;
  std::snprintf(toast_, sizeof(toast_), "Mouth cal");
  toast_until_ms_ = clock_.millis() + 1200;
}

void App::set_fan_speed(std::uint8_t duty) {
  state_.fan_speed = duty;
  fan_.set_speed(duty);
}

std::uint32_t App::rng() {
  rng_state_ = rng_state_ * 1664525u + 1013904223u;
  return rng_state_;
}

void App::set_sequence(const char* name, bool loop, bool with_transition) {
  const assets::Emotion* next = lookup_emotion(name);
  if (with_transition && next != nullptr) {
    snapshot(trans_from_, matrix_fb_);
    trans_kind_ = face::pick_transition(next->transition, rng(), state_.rare_chance);
    trans_start_ms_ = clock_.millis();
    trans_duration_ms_ = face::transition_duration_ms(trans_kind_);
    state_.transition = face::transition_name(trans_kind_);
  } else {
    trans_kind_ = face::TransitionKind::None;
    state_.transition = "none";
  }
  emotion_ = next;
  sequence_loop_ = loop;
  frame_index_ = 0;
  frame_count_ = emotion_ != nullptr ? std::max(1, emotion_->frame_count) : 1;
  frame_started_ms_ = clock_.millis();
  wink_cycle_ms_ = clock_.millis();
  wink_burst_ = false;
  sparks_.clear();
  randomize_pick_ = nullptr;
  randomize_frame_ = 0;
  randomize_last_ms_ = 0;
  while (frame_index_ + 1 < frame_count_ && rgb_blank(frame_at(emotion_, frame_index_))) {
    ++frame_index_;
    frame_started_ms_ = clock_.millis();
  }
  state_.face = emotion_ != nullptr ? emotion_->id : "Neutral";
  state_.text = hud_label(emotion_);
}

bool App::set_face(const char* id, bool with_transition) {
  const assets::Emotion* next = assets::find_emotion(id);
  if (next == nullptr || mode_ == Mode::Snake) {
    return false;
  }
  if (mode_ == Mode::Startup) {
    mode_ = Mode::FaceSet;
    state_.faceset = 1;
    state_.scene = "faceset";
  }
  set_sequence(next->id, next->loop, with_transition);
  return true;
}

void App::preview_blink() {
  if (!blink_allowed() || blink_ != BlinkState::Idle) {
    return;
  }
  blink_ = BlinkState::Closing;
  blink_started_ms_ = clock_.millis();
  blink_closed_ms_ = 80;
}

void App::finish_startup() {
  set_sequence("Neutral", true, false);
  enter_faceset(state_.faceset);
}

void App::enter_faceset(int set) {
  mode_ = Mode::FaceSet;
  state_.faceset = set;
  state_.scene = "faceset";
  prev_octant_ = 255;
  joystick_centered_ = true;
}

void App::enter_auto() {
  mode_ = Mode::Auto;
  state_.scene = "auto";
  last_auto_skip_ms_ = clock_.millis();
  pick_next_auto_face();
}

void App::enter_settings() {
  mode_ = Mode::Settings;
  state_.scene = "settings";
  state_.setting_index = kSettingStatus1;
  action_done_ = false;
}

void App::enter_settings_list() {
  mode_ = Mode::Settings;
  state_.scene = "settings";
  state_.setting_index = 0;
  action_done_ = false;
}

void App::cycle_menu_pages() {
  if (mode_ != Mode::Settings) {
    enter_settings();
    return;
  }
  if (state_.setting_index == kSettingStatus1) {
    state_.setting_index = kSettingStatus2;
  } else if (state_.setting_index == kSettingStatus2) {
    state_.setting_index = 0;
  } else {
    state_.setting_index = kSettingStatus1;
  }
  action_done_ = false;
}

void App::poll_menu_hold() {
  if (mode_ == Mode::Startup || !state_.pad.select() || menu_long_fired_) {
    return;
  }
  if (menu_down_ms_ == 0) {
    menu_down_ms_ = clock_.millis();
    return;
  }
  if (clock_.millis() - menu_down_ms_ >= kMenuHoldMs) {
    menu_long_fired_ = true;
    enter_settings_list();
  }
}

void App::enter_snake() {
  mode_ = Mode::Snake;
  state_.scene = "snake";
  snake_reset();
}

void App::enter_safe_mode() {
  if (mode_ == Mode::Startup) {
    return;
  }
  if (emotion_ == nullptr ||
      (std::strcmp(emotion_->id, "Startup") != 0 && std::strcmp(emotion_->id, "Neutral") != 0)) {
    set_sequence("Neutral", true, false);
  }
  enter_settings();
}

void App::pick_next_auto_face() {
  const int count = assets::kAutoFaceCount;
  const assets::Emotion* current = auto_next_;
  for (int i = 0; i < 8; ++i) {
    auto_next_ = lookup_emotion(assets::kAutoFaces[rng() % static_cast<std::uint32_t>(count)]);
    if (auto_next_ != current || count == 1) {
      break;
    }
  }
  auto_started_ms_ = clock_.millis();
  auto_wait_ms_ = 5000 + (rng() % 15001u);
}

void App::apply_octant_face() {
  if (prev_octant_ < 0 || prev_octant_ > 7) {
    return;
  }
  const int set = std::max(1, std::min(3, state_.faceset)) - 1;
  const assets::Emotion* next = lookup_emotion(assets::kFaceSets[set][prev_octant_]);
  set_sequence(next->id, next->loop, true);
  state_.octant = prev_octant_;
}

void App::add_settings_cursor(int delta) {
  int cursor = state_.setting_index;
  if (cursor == kSettingStatus1 || cursor == kSettingStatus2) {
    return;
  }
  cursor += delta;
  if (cursor < 0) {
    cursor = kSettingCount - 1;
  } else if (cursor >= kSettingCount) {
    cursor = 0;
  }
  state_.setting_index = cursor;
  action_done_ = false;
}

void App::nudge_setting(int delta) {
  const int index = state_.setting_index;
  if (index < 0 || index >= kSettingCount) {
    return;
  }
  const MenuItem& item = kMenu[index];
  if (item.kind == MenuItem::Action) {
    if (delta > 0) {
      activate_setting();
    }
    return;
  }
  if (item.kind == MenuItem::Toggle) {
    activate_setting();
    return;
  }

  auto clamp_u8 = [item](int value) {
    return static_cast<std::uint8_t>(std::clamp(value, item.min_value, item.max_value));
  };

  if (index == 0) {
    state_.brightness_level = clamp_u8(static_cast<int>(state_.brightness_level) + delta * item.step);
    sync_brightness();
  } else if (index == 5) {
    state_.boop_sensitivity = clamp_u8(static_cast<int>(state_.boop_sensitivity) + delta * item.step);
    const float span = (255.0f - static_cast<float>(state_.boop_sensitivity)) / 255.0f;
    boop_threshold_ = std::clamp(boop_rest_ + 0.08f + span * 0.35f, 0.08f, 0.95f);
  } else if (index == 9) {
    state_.rare_chance = clamp_u8(static_cast<int>(state_.rare_chance) + delta * item.step);
  } else if (index == 10) {
    set_fan_speed(clamp_u8(static_cast<int>(state_.fan_speed) + delta * item.step));
  }
}

void App::activate_setting() {
  const int index = state_.setting_index;
  if (index < 0 || index >= kSettingCount) {
    return;
  }
  switch (index) {
    case 1:
      state_.matrix_enabled = !state_.matrix_enabled;
      break;
    case 2:
      state_.led_enabled = !state_.led_enabled;
      break;
    case 3:
      state_.auto_blink = !state_.auto_blink;
      break;
    case 4:
      state_.boop_enabled = !state_.boop_enabled;
      if (!state_.boop_enabled) {
        state_.boop = false;
        boop_triggers_ = 0;
      }
      break;
    case 6:
      calibrate_boop();
      break;
    case 7:
      state_.mouth_enabled = !state_.mouth_enabled;
      if (!state_.mouth_enabled) {
        mouth_level_ = 0;
      }
      break;
    case 8:
      calibrate_mouth();
      break;
    case 11:
      enter_snake();
      break;
    case 12:
      restart();
      break;
    case 13:
      action_done_ = save_settings();
      break;
    default:
      break;
  }
}

void App::apply_pad(const PadState& pad) {
  const PadState prev = prev_pad_;
  state_.pad = pad;
  prev_pad_ = pad;

  if (mode_ == Mode::Startup) {
    if ((pad.buttons != 0 && prev.buttons == 0) || stick_active(pad)) {
      finish_startup();
    }
    return;
  }

  if (mode_ == Mode::Snake) {
    handle_stick_snake(pad);
    if (pressed(pad.b(), prev.b()) || pressed(pad.esc(), prev.esc())) {
      enter_settings();
    }
  } else {
    if (pressed(pad.x_btn(), prev.x_btn())) {
      enter_faceset(1);
    }
    if (pressed(pad.a(), prev.a())) {
      enter_faceset(2);
    }
    if (pressed(pad.y_btn(), prev.y_btn())) {
      enter_faceset(3);
    }
    if (pressed(pad.b(), prev.b())) {
      enter_auto();
    }
    if (pressed(pad.esc(), prev.esc())) {
      if (mode_ == Mode::Settings) {
        enter_faceset(state_.faceset);
      }
    }

    if (mode_ == Mode::Settings) {
      if (pressed(pad.ok(), prev.ok())) {
        activate_setting();
      }
      blink_held_ = false;
    } else {
      const bool stick_out = stick_active(pad);
      blink_held_ = pad.ok() && !stick_out;
      if (pressed(pad.ok(), prev.ok()) && blink_ == BlinkState::Idle && blink_allowed() && !stick_out) {
        blink_ = BlinkState::Closing;
        blink_started_ms_ = clock_.millis();
        blink_closed_ms_ = 40;
      }
    }

    if (mode_ == Mode::FaceSet) {
      handle_stick_faceset(pad);
    } else if (mode_ == Mode::Auto) {
      if (stick_active(pad) && clock_.millis() - last_auto_skip_ms_ >= 250) {
        set_sequence(auto_next_ != nullptr ? auto_next_->id : "Joy", true, true);
        last_auto_skip_ms_ = clock_.millis();
        pick_next_auto_face();
      }
    } else if (mode_ == Mode::Settings) {
      handle_stick_settings(pad, prev);
    }
  }

  if (pressed(pad.select(), prev.select())) {
    menu_down_ms_ = clock_.millis();
    menu_long_fired_ = false;
  }
  if (released(pad.select(), prev.select())) {
    if (!menu_long_fired_) {
      cycle_menu_pages();
    }
    menu_down_ms_ = 0;
    menu_long_fired_ = false;
  }
}

void App::handle_stick_faceset(const PadState& pad) {
  const int octant = stick_octant(pad);
  if (octant < 0) {
    if (!joystick_centered_) {
      if (!pad.ok()) {
        apply_octant_face();
      }
    }
    joystick_centered_ = true;
    return;
  }
  joystick_centered_ = false;
  prev_octant_ = octant;
  state_.octant = octant;
}

void App::handle_stick_settings(const PadState& pad, const PadState& prev) {
  (void)prev;
  const bool up = pad.y < 40;
  const bool down = pad.y > 215;
  const bool left = pad.x < 40;
  const bool right = pad.x > 215;
  const bool y_center = pad.y >= 80 && pad.y <= 175;
  const bool x_center = pad.x >= 80 && pad.x <= 175;
  if (y_center) {
    settings_y_latched_ = false;
  }
  if (x_center) {
    settings_x_latched_ = false;
  }
  if (!settings_y_latched_ && (up || down)) {
    add_settings_cursor(down ? 1 : -1);
    settings_y_latched_ = true;
  }
  if (!settings_x_latched_ && (left || right)) {
    const int index = state_.setting_index;
    if (index >= 0 && index < kSettingCount && kMenu[index].kind == MenuItem::Percent) {
      nudge_setting(right ? 1 : -1);
    }
    settings_x_latched_ = true;
  }
}

void App::handle_stick_snake(const PadState& pad) {
  if (pad.y < 40 && snake_.last_dir != 2) {
    snake_.dir = 1;
  } else if (pad.y > 215 && snake_.last_dir != 1) {
    snake_.dir = 2;
  } else if (pad.x < 40 && snake_.last_dir != 4) {
    snake_.dir = 3;
  } else if (pad.x > 215 && snake_.last_dir != 3) {
    snake_.dir = 4;
  }
}

void App::snake_reset() {
  snake_.play = Snake::Play::Wait;
  snake_.dir = 0;
  snake_.last_dir = 0;
  snake_.score = 0;
  snake_.grow = 5;
  snake_.body.clear();
  snake_.body.push_back({kSnakeW / 2, kSnakeH / 2});
  snake_.fruit_x = 3 + static_cast<int>(rng() % static_cast<std::uint32_t>(kSnakeW - 4));
  snake_.fruit_y = 1 + static_cast<int>(rng() % static_cast<std::uint32_t>(kSnakeH - 2));
  snake_.last_step_ms = clock_.millis();
  snake_.last_blink_ms = clock_.millis();
  state_.snake_score = 0;
}

void App::snake_game_over() {
  snake_.play = Snake::Play::Stop;
  snake_.dir = 0;
  snake_.last_dir = 0;
}

void App::update_hid_link() {
  const bool connected = hid_.connected();
  state_.hid_connected = connected;
  state_.bt_status = connected ? "MOCUTE" : "----";
  if (hid_was_connected_ && !connected) {
    enter_safe_mode();
  }
  hid_was_connected_ = connected;
}

void App::update_sensors(std::uint32_t now_ms) {
  const hal::GyroSample gyro = sensors_.gyro();
  state_.mic = std::clamp(sensors_.microphone(), 0.0f, 1.0f);
  state_.proximity = std::clamp(sensors_.proximity(), 0.0f, 1.0f);
  state_.pitch = gyro.pitch_deg;
  state_.roll = gyro.roll_deg;
  state_.yaw = gyro.yaw_deg;
  state_.fan_speed = fan_.speed();
  state_.heap = store_.free_heap();
  (void)now_ms;

  const float dp = gyro.pitch_deg - prev_pitch_;
  const float dr = gyro.roll_deg - prev_roll_;
  prev_pitch_ = gyro.pitch_deg;
  prev_roll_ = gyro.roll_deg;
  const float motion = std::sqrt(dp * dp + dr * dr);
  const bool dizzy = std::fabs(gyro.pitch_deg - gyro_rest_pitch_) > kGyroDizzyDeg ||
                     std::fabs(gyro.roll_deg - gyro_rest_roll_) > kGyroDizzyDeg || motion > 12.0f;
  state_.dizzy = dizzy && !state_.boop;
  if (gyro_last_motion_ms_ == 0) {
    gyro_rest_pitch_ = gyro.pitch_deg;
    gyro_rest_roll_ = gyro.roll_deg;
    gyro_last_motion_ms_ = now_ms;
  }
  if (motion > 1.2f) {
    gyro_last_motion_ms_ = now_ms;
  }
  if (motion > kGyroKickDeg) {
    gyro_kick_x_ = std::clamp(static_cast<int>(dr / kGyroKickDeg), -3, 3);
    gyro_kick_y_ = std::clamp(static_cast<int>(-dp / kGyroKickDeg), -3, 3);
    gyro_kick_until_ = now_ms + 160;
  }
  if (now_ms - gyro_last_motion_ms_ > 700) {
    gyro_rest_pitch_ += (gyro.pitch_deg - gyro_rest_pitch_) * 0.16f;
    gyro_rest_roll_ += (gyro.roll_deg - gyro_rest_roll_) * 0.16f;
  }

  if (mouth_calibrate_left_ > 0) {
    mouth_baseline_acc_ += state_.mic;
    --mouth_calibrate_left_;
    calib_percent_ = (8 - mouth_calibrate_left_) * 100 / 8;
    if (mouth_calibrate_left_ == 0) {
      mouth_baseline_ = mouth_baseline_acc_ / 8.0f;
      mouth_peak_ = 0.35f;
      action_done_ = true;
      calib_percent_ = 100;
    }
  }

  if (boop_calibrate_left_ > 0) {
    boop_cal_acc_ += state_.proximity;
    --boop_calibrate_left_;
    calib_percent_ = (16 - boop_calibrate_left_) * 100 / 16;
    if (boop_calibrate_left_ == 0) {
      boop_rest_ = boop_cal_acc_ / 16.0f;
      const float span = (255.0f - static_cast<float>(state_.boop_sensitivity)) / 255.0f;
      boop_threshold_ = std::clamp(boop_rest_ + 0.08f + span * 0.35f, 0.08f, 0.95f);
      boop_triggers_ = 0;
      state_.boop = false;
      action_done_ = true;
      calib_percent_ = 100;
    }
  }

  if (boop_allowed()) {
    const bool high = state_.proximity >= boop_threshold_;
    if (high) {
      if (boop_triggers_ < kBoopTriggersMax) {
        ++boop_triggers_;
      }
    } else if (boop_triggers_ > 0) {
      --boop_triggers_;
    }
    const bool boop = boop_triggers_ >= kBoopTriggerCount;
    if (boop && !state_.boop) {
      ++state_.boop_count;
      boop_started_ms_ = now_ms;
    }
    state_.boop = boop;
  } else {
    boop_triggers_ = 0;
    state_.boop = false;
  }

  if (prev_boop_ && !state_.boop && mode_ != Mode::Snake && mode_ != Mode::Startup) {
    snapshot(trans_from_, matrix_fb_);
    trans_kind_ = face::TransitionKind::Glitch;
    trans_start_ms_ = clock_.millis();
    trans_duration_ms_ = face::transition_duration_ms(trans_kind_);
    state_.transition = face::transition_name(trans_kind_);
  }
  prev_boop_ = state_.boop;

  if (state_.mouth_enabled) {
    const float reading = std::max(0.0f, std::fabs(state_.mic - mouth_baseline_) - 0.02f);
    mouth_level_ = std::max(reading, mouth_level_ * 0.60f);
    if (mouth_level_ > mouth_peak_) {
      mouth_peak_ = mouth_level_;
    } else {
      mouth_peak_ = std::max(0.35f, mouth_peak_ - 0.02f);
    }
  } else {
    mouth_level_ = 0;
  }
}

void App::update_face_player(std::uint32_t now_ms) {
  if (mode_ == Mode::Startup) {
    if (now_ms - startup_started_ms_ >= kStartupMs) {
      finish_startup();
      return;
    }
    if (now_ms - frame_started_ms_ >= kStartupFaceMs) {
      set_sequence(assets::kBootFaces[rng() % static_cast<std::uint32_t>(assets::kBootFaceCount)], true, false);
    }
    return;
  }

  if (mode_ == Mode::Snake) {
    return;
  }

  const assets::EmotionFrame* frame = frame_at(emotion_, frame_index_);
  std::uint32_t duration = kDefaultFrameMs;
  if (frame != nullptr) {
    duration = frame->duration_ms > 0 ? static_cast<std::uint32_t>(frame->duration_ms)
                                      : kDefaultFrameMs;
  }
  if (now_ms - frame_started_ms_ < duration) {
    return;
  }
  if (sequence_loop_) {
    frame_index_ = (frame_index_ + 1) % frame_count_;
    frame_started_ms_ = now_ms;
  } else if (frame_index_ + 1 < frame_count_) {
    ++frame_index_;
    frame_started_ms_ = now_ms;
  }
}

void App::update_auto(std::uint32_t now_ms) {
  if (mode_ != Mode::Auto) {
    return;
  }
  if (now_ms - auto_started_ms_ < auto_wait_ms_) {
    return;
  }
  set_sequence(auto_next_ != nullptr ? auto_next_->id : "Joy", true, true);
  last_auto_skip_ms_ = now_ms;
  pick_next_auto_face();
}

void App::update_blink(std::uint32_t now_ms) {
  if (mode_ == Mode::Snake || mode_ == Mode::Startup || !blink_allowed()) {
    blink_ = BlinkState::Idle;
    state_.blinking = false;
    return;
  }
  if (!state_.auto_blink && !blink_held_ && blink_ == BlinkState::Idle) {
    state_.blinking = false;
    return;
  }

  switch (blink_) {
    case BlinkState::Idle:
      if (state_.auto_blink && now_ms - blink_started_ms_ >= blink_wait_ms_) {
        blink_ = BlinkState::Closing;
        blink_started_ms_ = now_ms;
      }
      break;
    case BlinkState::Closing:
      if (now_ms - blink_started_ms_ >= 80) {
        blink_ = BlinkState::Closed;
        blink_started_ms_ = now_ms;
        blink_closed_ms_ = blink_held_ ? 40 : (40 + (rng() % 161u));
      }
      break;
    case BlinkState::Closed:
      if (!blink_held_ && now_ms - blink_started_ms_ >= blink_closed_ms_) {
        blink_ = BlinkState::Opening;
        blink_started_ms_ = now_ms;
      }
      break;
    case BlinkState::Opening:
      if (now_ms - blink_started_ms_ >= 80) {
        blink_ = BlinkState::Idle;
        blink_started_ms_ = now_ms;
        blink_wait_ms_ = 5000 + (rng() % 15001u);
      }
      break;
  }
  state_.blinking = blink_ != BlinkState::Idle;
}

void App::update_snake(std::uint32_t now_ms) {
  if (mode_ != Mode::Snake) {
    return;
  }
  if (now_ms - snake_.last_blink_ms >= kSnakeBlinkMs) {
    snake_.last_blink_ms = now_ms;
    snake_.fruit_on = !snake_.fruit_on;
  }
  if (now_ms - snake_.last_step_ms < kSnakeStepMs) {
    return;
  }
  snake_.last_step_ms = now_ms;

  if (snake_.play == Snake::Play::Stop) {
    if (snake_.dir != 0) {
      snake_reset();
    }
    return;
  }
  if (snake_.play == Snake::Play::Wait) {
    if (snake_.dir != 0) {
      snake_.play = Snake::Play::Run;
    }
    return;
  }

  auto head = snake_.body.front();
  if (snake_.dir == 1) {
    --head.second;
  } else if (snake_.dir == 2) {
    ++head.second;
  } else if (snake_.dir == 3) {
    --head.first;
  } else if (snake_.dir == 4) {
    ++head.first;
  }
  snake_.body.insert(snake_.body.begin(), head);

  if (head.first < 0 || head.first >= kSnakeW || head.second < 0 || head.second >= kSnakeH) {
    snake_game_over();
    return;
  }
  for (std::size_t i = 1; i < snake_.body.size(); ++i) {
    if (snake_.body[i] == head) {
      snake_game_over();
      return;
    }
  }
  if (head.first == snake_.fruit_x && head.second == snake_.fruit_y) {
    ++snake_.score;
    state_.snake_score = snake_.score;
    snake_.grow += 2;
    snake_.fruit_x = static_cast<int>(rng() % static_cast<std::uint32_t>(kSnakeW));
    snake_.fruit_y = static_cast<int>(rng() % static_cast<std::uint32_t>(kSnakeH));
  }
  if (snake_.grow > 0) {
    --snake_.grow;
  } else if (snake_.body.size() > 1) {
    snake_.body.pop_back();
  }
  snake_.last_dir = snake_.dir;
}

void App::update_fps(std::uint32_t now_ms) {
  ++fps_ticks_;
  if (now_ms - fps_window_ms_ < 1000) {
    return;
  }
  const std::uint32_t dt = std::max(1u, now_ms - fps_window_ms_);
  state_.fps = static_cast<int>((fps_ticks_ * 1000u) / dt);
  fps_ticks_ = 0;
  fps_window_ms_ = now_ms;
}

Color App::accent() const {
  if (emotion_ != nullptr) {
    return emotion_->accent;
  }
  return Color{90, 220, 255};
}

void App::render_snake(std::uint32_t now_ms) {
  (void)now_ms;
  matrix_fb_.clear(Color::black());
  const Color border = Color{40, 80, 90};
  const Color body = accent();
  const Color fruit = Color{255, 80, 80};
  matrix_fb_.draw_rect(kSnakeOx - 1, kSnakeOy - 1, kSnakeW * kSnakeCell + 2, kSnakeH * kSnakeCell + 2, border);
  for (const auto& part : snake_.body) {
    matrix_fb_.fill_rect(kSnakeOx + part.first * kSnakeCell, kSnakeOy + part.second * kSnakeCell, kSnakeCell,
                         kSnakeCell, body);
  }
  if (snake_.fruit_on || snake_.play != Snake::Play::Run) {
    matrix_fb_.fill_rect(kSnakeOx + snake_.fruit_x * kSnakeCell, kSnakeOy + snake_.fruit_y * kSnakeCell,
                         kSnakeCell, kSnakeCell, fruit);
  }
  char score[12];
  std::snprintf(score, sizeof(score), "%u", static_cast<unsigned>(snake_.score));
  matrix_fb_.draw_text(2, 4, score, Color::white());
  if (snake_.play == Snake::Play::Wait) {
    matrix_fb_.draw_text(20, 4, "GO", scale(body, 200));
  } else if (snake_.play == Snake::Play::Stop) {
    matrix_fb_.draw_text(16, 4, "DEAD", Color{255, 70, 70});
  }
}

void App::render_face(std::uint32_t now_ms) {
  matrix_fb_.clear(Color::black());

  const assets::Emotion* draw = emotion_;
  int draw_frame = frame_index_;
  if (emotion_ != nullptr && emotion_->effect == assets::Effect::Randomize) {
    if (randomize_pick_ == nullptr || now_ms - randomize_last_ms_ >= kRandomizePeriodMs) {
      randomize_last_ms_ = now_ms;
      for (int attempt = 0; attempt < 16; ++attempt) {
        const assets::Emotion* pick =
            assets::emotion_at(static_cast<int>(rng() % static_cast<std::uint32_t>(assets::kEmotionCount)));
        if (pick == nullptr || pick->kind != assets::Kind::Classic || pick->effect == assets::Effect::Randomize) {
          continue;
        }
        const int idx = static_cast<int>(rng() % static_cast<std::uint32_t>(std::max(1, pick->frame_count)));
        if (rgb_hot(frame_at(pick, idx))) {
          continue;
        }
        randomize_pick_ = pick;
        randomize_frame_ = idx;
        break;
      }
    }
    if (randomize_pick_ != nullptr) {
      draw = randomize_pick_;
      draw_frame = randomize_frame_;
    }
  } else if (emotion_ != nullptr && emotion_->effect == assets::Effect::Dizzy && emotion_->frame_count > 0) {
    draw_frame = static_cast<int>((now_ms / 75) % static_cast<std::uint32_t>(emotion_->frame_count));
  }

  if (state_.boop && boop_allowed()) {
    const assets::Emotion* boop = assets::find_emotion("Boop");
    const assets::EmotionFrame* frame = frame_at(boop, 0);
    if (frame != nullptr) {
      matrix_fb_.blit_rgb(0, 0, kFaceW, kFaceH, frame->rgb, frame->size);
    }
    apply_boop_overlay(now_ms);
  } else {
    const assets::EmotionFrame* frame = frame_at(draw, draw_frame);
    if (frame == nullptr) {
      frame = frame_at(assets::find_emotion("Neutral"), 0);
    }
    if (frame != nullptr) {
      matrix_fb_.blit_rgb(0, 0, kFaceW, kFaceH, frame->rgb, frame->size);
    }
    apply_effect(now_ms);
    // Classic faces keep one static RGB sprite. Overlays then rewrite pixels in
    // the mouth band (mic / snarl) and the left-eye rect (blink). Special faces skip both.
    if (emotion_classic(emotion_)) {
      apply_mouth_effect();
      apply_blink_overlay();
    }
    draw_sparks();
  }

  if (trans_kind_ != face::TransitionKind::None && trans_duration_ms_ > 0 &&
      trans_from_.size() == static_cast<std::size_t>(matrix_fb_.width() * matrix_fb_.height())) {
    const float t = static_cast<float>(now_ms - trans_start_ms_) / static_cast<float>(trans_duration_ms_);
    if (t >= 1.0f) {
      trans_kind_ = face::TransitionKind::None;
      state_.transition = "none";
    } else {
      snapshot(trans_to_, matrix_fb_);
      face::apply_transition(matrix_fb_, trans_from_.data(), trans_to_.data(), matrix_fb_.width(),
                             matrix_fb_.height(), trans_kind_, t, rng_state_);
    }
  }
  apply_gyro_nudge();
}

bool App::boop_allowed() const {
  if (!state_.boop_enabled || mode_ == Mode::Snake || mode_ == Mode::Startup) {
    return false;
  }
  return emotion_ == nullptr || emotion_->allow_boop;
}

bool App::blink_allowed() const {
  if (state_.boop || emotion_ == nullptr) {
    return false;
  }
  if (emotion_->effect == assets::Effect::Wink) {
    return false;
  }
  return emotion_->allow_blink;
}

void App::apply_boop_overlay(std::uint32_t now_ms) {
  const std::uint32_t elapsed = now_ms - boop_started_ms_;
  if (elapsed >= kBoopGlitchMs) {
    return;
  }
  const float fall = 1.0f - static_cast<float>(elapsed) / static_cast<float>(kBoopGlitchMs);
  const int amp = std::max(1, static_cast<int>(fall * 5.0f));
  matrix_fb_.glitch_rows(0, matrix_fb_.height(), amp, rng_state_ + elapsed);
}

void App::apply_effect(std::uint32_t now_ms) {
  if (emotion_ == nullptr) {
    return;
  }
  switch (emotion_->effect) {
    case assets::Effect::Dizzy:
      apply_dizzy_motion(now_ms);
      break;
    case assets::Effect::Wink:
      apply_wink_motion(now_ms);
      break;
    case assets::Effect::Snarl:
      apply_angry_motion(now_ms);
      break;
    default:
      break;
  }
}

void App::apply_dizzy_motion(std::uint32_t now_ms) {
  const int phase = static_cast<int>((now_ms / 75) % 8);
  const int turns = phase / 2;
  matrix_fb_.rotate_square_cw(kDizzySpinX, kEyeY0, kDizzySpinSize, turns);
  const int dx = static_cast<int>(std::sin(static_cast<float>(now_ms) / 300.0f) * 2.0f);
  matrix_fb_.translate_rect(kEyeLX, kEyeY0, kEyeW, kEyeH, dx, 0);
}

void App::apply_gyro_nudge() {
  int dx = std::clamp(static_cast<int>((state_.roll - gyro_rest_roll_) / kGyroNudgeDiv), -3, 3);
  int dy = std::clamp(static_cast<int>(-(state_.pitch - gyro_rest_pitch_) / kGyroNudgeDiv), -3, 3);
  if (clock_.millis() < gyro_kick_until_) {
    dx = std::clamp(dx + gyro_kick_x_, -3, 3);
    dy = std::clamp(dy + gyro_kick_y_, -3, 3);
  }
  if (dx == 0 && dy == 0) {
    return;
  }
  matrix_fb_.translate_rect(0, 0, matrix_fb_.width(), matrix_fb_.height(), dx, dy);
}

void App::emit_wink_sparks() {
  for (int i = 0; i < 5; ++i) {
    Spark spark;
    spark.x = 10.0f + static_cast<float>(rng() % 12u);
    spark.y = 4.0f + static_cast<float>(rng() % 6u);
    spark.vx = -1.6f - static_cast<float>(rng() % 8u) / 10.0f;
    spark.vy = -1.2f - static_cast<float>(rng() % 10u) / 10.0f;
    spark.life = 10 + static_cast<int>(rng() % 8u);
    sparks_.push_back(spark);
  }
}

void App::draw_sparks() {
  std::vector<Spark> live;
  live.reserve(sparks_.size());
  const Color c = accent();
  for (Spark spark : sparks_) {
    spark.x += spark.vx;
    spark.y += spark.vy;
    spark.vy += 0.18f;
    --spark.life;
    if (spark.life <= 0) {
      continue;
    }
    matrix_fb_.set_pixel(static_cast<int>(spark.x), static_cast<int>(spark.y), c);
    live.push_back(spark);
  }
  sparks_.swap(live);
}

void App::apply_wink_motion(std::uint32_t now_ms) {
  const std::uint32_t t = (now_ms - wink_cycle_ms_) % 1600u;
  int cover = 0;
  if (t >= 180 && t < 430) {
    cover = static_cast<int>((t - 180) * kEyeH / 250);
  } else if (t >= 430 && t < 950) {
    cover = kEyeH;
  } else if (t >= 950 && t < 1200) {
    cover = kEyeH - static_cast<int>((t - 950) * kEyeH / 250);
  }
  if (t < 180) {
    wink_burst_ = false;
  }
  if (t >= 500 && t < 850 && !wink_burst_) {
    emit_wink_sparks();
    wink_burst_ = true;
  }
  if (cover <= 0) {
    return;
  }
  const int top = std::max(1, cover / 2);
  const int bottom = std::max(1, cover - top);
  matrix_fb_.fill_rect(kEyeLX, kEyeY0, kEyeW, top, Color::black());
  matrix_fb_.fill_rect(kEyeLX, kEyeY0 + kEyeH - bottom, kEyeW, bottom, Color::black());
  const int lid = kEyeY0 + top;
  matrix_fb_.draw_hline(kEyeLX + 2, lid, kEyeW - 4, accent());
}

void App::apply_angry_motion(std::uint32_t now_ms) {
  if (!state_.mouth_enabled) {
    return;
  }
  const float wave = 0.5f + 0.5f * std::sin(static_cast<float>(now_ms) / 140.0f);
  const float peak = wave * 5.0f;
  // Triangle open: tip near the middle, base on the right. Left side stays put.
  const int origin = kFaceW / 2 - 8;
  const int span = kFaceW - 1 - origin;
  for (int x = origin; x < kFaceW; ++x) {
    const float t = static_cast<float>(x - origin) / static_cast<float>(span);
    const int amount = static_cast<int>(peak * t + 0.5f);
    if (amount > 0) {
      matrix_fb_.expand_column_y(x, kMouthY0, kMouthH, amount);
    }
  }
}

void App::apply_mouth_effect() {
  // Stretch already-drawn mouth pixels in y=16..32. No second mouth sprite.
  if (!state_.mouth_enabled || mouth_level_ <= 0.02f) {
    return;
  }
  const float peak = std::max(0.2f, mouth_peak_);
  const float norm = std::min(1.0f, (mouth_level_ / peak) * 0.35f);
  for (int i = 0; i < 32; ++i) {
    const int amount = static_cast<int>(norm * static_cast<float>(i));
    if (amount <= 0) {
      continue;
    }
    matrix_fb_.expand_column_y(31 - i, kMouthY0, kMouthH, amount);
    matrix_fb_.expand_column_y(32 + i, kMouthY0, kMouthH, amount);
  }
}

void App::apply_blink_overlay() {
  // Cover the left 32×16 eye with black from the top; nose at x=48 is untouched.
  if (!state_.blinking) {
    return;
  }
  int cover = kEyeH;
  const std::uint32_t elapsed = clock_.millis() - blink_started_ms_;
  const int stepped = elapsed > 80 ? kEyeH : static_cast<int>(elapsed * kEyeH / 80);
  if (blink_ == BlinkState::Closing) {
    cover = stepped;
  } else if (blink_ == BlinkState::Opening) {
    cover = kEyeH - stepped;
  }
  matrix_fb_.fill_rect(kEyeLX, kEyeY0, kEyeW, cover, Color::black());
  if (blink_ == BlinkState::Closed || cover >= kEyeH / 2) {
    const int lid = kEyeY0 + std::min(cover, kEyeH) - 1;
    matrix_fb_.draw_hline(kEyeLX + 2, lid, kEyeW - 4, accent());
  }
}

void App::draw_face_thumb(int x, int y, const char* id, bool invert) {
  const assets::Emotion* emotion = lookup_emotion(id);
  if (emotion != nullptr && emotion->hud != nullptr) {
    oled_fb_.blit_bitmap_1bpp(x, y, kHudThumbW, kHudThumbH, emotion->hud, emotion->hud_size, kHudThumbW,
                              kHudThumbH);
  }
  if (invert) {
    oled_fb_.invert_rect(x, y, kHudThumbW, kHudThumbH);
  }
}

void App::draw_toggle(int x, int y, bool on) {
  oled_fb_.draw_rect(x, y, 36, 16, true);
  const int knob_x = on ? x + 22 : x + 2;
  oled_fb_.fill_rect(knob_x, y + 2, 12, 12, true);
  oled_fb_.draw_char(knob_x + 3, y + 4, on ? '1' : '0', false);
}

void App::draw_settings_pager() {
  if (state_.setting_index < 0 || state_.setting_index >= kSettingCount) {
    return;
  }
  const int x_dot = 126;
  const int y0 = 18;
  const int step = 3;
  for (int i = 0; i < kSettingCount; ++i) {
    oled_fb_.set_pixel(x_dot, y0 + i * step, true);
  }
  const int ax = 123;
  const int ay = y0 + state_.setting_index * step - 1;
  oled_fb_.set_pixel(ax, ay, true);
  oled_fb_.set_pixel(ax + 1, ay + 1, true);
  oled_fb_.set_pixel(ax, ay + 2, true);
}

void App::render_oled_header() {
  const int mw = matrix_fb_.width();
  const int mh = matrix_fb_.height();
  const int pw = 32;
  const int ph = 16;
  for (int y = 0; y < ph; ++y) {
    const int sy = y * mh / ph;
    for (int x = 0; x < pw; ++x) {
      const int sx = x * mw / pw;
      const Color c = matrix_fb_.get_pixel(sx, sy);
      oled_fb_.set_pixel(x, y, (c.r | c.g | c.b) > 20);
    }
  }
  const assets::Emotion* label = emotion_;
  if (mode_ == Mode::FaceSet && !joystick_centered_) {
    const int set = std::max(1, std::min(3, state_.faceset)) - 1;
    label = lookup_emotion(assets::kFaceSets[set][std::clamp(state_.octant, 0, 7)]);
  }
  oled_fb_.draw_text(34, 4, hud_label(label), true);
  oled_fb_.draw_rect(90, 4, 7, 7, true);
  if (state_.boop) {
    oled_fb_.fill_rect(92, 6, 3, 3, true);
  }
  oled_fb_.draw_rect(99, 4, 27, 7, true);
  const float peak = std::max(0.2f, mouth_peak_);
  const int fill = std::clamp(static_cast<int>((mouth_level_ / peak) * 23.0f), 0, 23);
  if (fill > 0) {
    oled_fb_.fill_rect(101, 6, fill, 3, true);
  }
}

void App::render_oled_settings(std::uint32_t now_ms) {
  char line[32];
  if (state_.setting_index == kSettingStatus1) {
    std::snprintf(line, sizeof(line), "BT: %s", state_.bt_status.c_str());
    oled_fb_.draw_text(0, 20, line, true);
    std::snprintf(line, sizeof(line), "Uptime: %s", format_uptime(now_ms).c_str());
    oled_fb_.draw_text(0, 28, line, true);
    std::snprintf(line, sizeof(line), "Boops: %d", state_.boop_count);
    oled_fb_.draw_text(0, 36, line, true);
    std::snprintf(line, sizeof(line), "FPS: %d", state_.fps);
    oled_fb_.draw_text(0, 44, line, true);
    return;
  }
  if (state_.setting_index == kSettingStatus2) {
    std::snprintf(line, sizeof(line), "Frame: %u", static_cast<unsigned>(tick_count_));
    oled_fb_.draw_text(0, 20, line, true);
    std::snprintf(line, sizeof(line), "FPS: %d", state_.fps);
    oled_fb_.draw_text(0, 28, line, true);
    std::snprintf(line, sizeof(line), "Heap: %u", static_cast<unsigned>(state_.heap));
    oled_fb_.draw_text(0, 36, line, true);
    std::snprintf(line, sizeof(line), "Version: %s", kVersion);
    oled_fb_.draw_text(0, 44, line, true);
    std::snprintf(line, sizeof(line), "Built: %s", kCompileTimestamp);
    oled_fb_.draw_text(0, 52, line, true);
    return;
  }

  const int index = state_.setting_index;
  const MenuItem& item = kMenu[index];
  oled_fb_.draw_text(0, 18, item.category, true);
  if (item.name[0] != '\0') {
    oled_fb_.draw_text(0, 28, item.name, true);
  }

  char value[20] = "";
  int raw = 0;
  int max_value = item.max_value;
  if (index == 0) {
    raw = state_.brightness_level;
  } else if (index == 1) {
    raw = state_.matrix_enabled ? 1 : 0;
  } else if (index == 2) {
    raw = state_.led_enabled ? 1 : 0;
  } else if (index == 3) {
    raw = state_.auto_blink ? 1 : 0;
  } else if (index == 4) {
    raw = state_.boop_enabled ? 1 : 0;
  } else if (index == 5) {
    raw = state_.boop_sensitivity;
  } else if (index == 7) {
    raw = state_.mouth_enabled ? 1 : 0;
  } else if (index == 9) {
    raw = state_.rare_chance;
  } else if (index == 10) {
    raw = state_.fan_speed;
  }

  if (item.kind == MenuItem::Toggle) {
    draw_toggle(0, 40, raw != 0);
    oled_fb_.draw_text(40, 44, item.name, true);
  } else if (item.kind == MenuItem::Action) {
    if ((index == 6 && boop_calibrate_left_ > 0) || (index == 8 && mouth_calibrate_left_ > 0)) {
      std::snprintf(value, sizeof(value), "%d%%", calib_percent_);
    } else {
      std::snprintf(value, sizeof(value), "%s", action_done_ ? "Done" : "OK");
    }
    oled_fb_.draw_text(0, 42, value, true, 2);
  } else {
    const int pct = max_value > 0 ? (raw * 100) / max_value : 0;
    std::snprintf(value, sizeof(value), "%d%%", pct);
    oled_fb_.draw_text(0, 40, value, true, 2);
    oled_fb_.draw_rect(0, 56, 86, 6, true);
    const int bar = std::clamp(raw * 82 / std::max(1, max_value), 0, 82);
    oled_fb_.fill_rect(2, 58, bar, 2, true);
  }
  draw_settings_pager();
}

void App::render_oled_faceset() {
  const int set = std::max(1, std::min(3, state_.faceset)) - 1;
  const char* current_id = emotion_ != nullptr ? emotion_->id : "";
  int current_i = -1;
  for (int i = 0; i < 8; ++i) {
    const char* id = assets::kFaceSets[set][i];
    const bool current = std::strcmp(current_id, id) == 0;
    if (current) {
      current_i = i;
    }
    draw_face_thumb(kThumbPos[i][0], kThumbPos[i][1], id, current);
  }
  if (current_i >= 0) {
    oled_fb_.draw_corners(kThumbPos[current_i][0], kThumbPos[current_i][1], kHudThumbW, kHudThumbH,
                          kHudCornerLen, false);
  }
  if (!joystick_centered_ && state_.octant >= 0 && state_.octant <= 7 && state_.octant != current_i) {
    oled_fb_.draw_corners(kThumbPos[state_.octant][0], kThumbPos[state_.octant][1], kHudThumbW,
                          kHudThumbH, kHudCornerLen, true);
  }
  const int line_x = state_.pad.x / 2;
  const int line_y = 16 + static_cast<int>(state_.pad.y * 48 / 255);
  oled_fb_.draw_line(64, 40, line_x, line_y, true);
  if (clock_.millis() < toast_until_ms_ && toast_[0] != '\0') {
    oled_fb_.fill_rect(0, 56, 128, 8, false);
    oled_fb_.draw_text(0, 56, toast_, true);
  }
}

void App::render_oled_auto(std::uint32_t now_ms) {
  const assets::Bitmap* visor = assets::find_system("visor");
  if (visor != nullptr) {
    oled_fb_.blit_bitmap_1bpp(0, 26, visor->width, visor->height, visor->data, visor->size, visor->width,
                              visor->height);
  }
  draw_face_thumb(8, 42, auto_next_ != nullptr ? auto_next_->id : "Joy", false);
  char line[24];
  std::snprintf(line, sizeof(line), "%s", hud_label(auto_next_));
  oled_fb_.draw_text(68, 41, line, true);
  const float left = static_cast<float>(auto_wait_ms_ - std::min(auto_wait_ms_, now_ms - auto_started_ms_)) /
                     1000.0f;
  std::snprintf(line, sizeof(line), "%.1f", static_cast<double>(left));
  oled_fb_.draw_text(80, 49, line, true, 2);
}

void App::render_oled_startup(std::uint32_t now_ms) {
  const std::uint32_t elapsed = now_ms - startup_started_ms_;
  if (elapsed < kStartupMs / 2) {
    oled_fb_.fill_rect(0, 0, oled_fb_.width(), oled_fb_.height(), true);
    const assets::Bitmap* splash1 = assets::find_system("splash1");
    const assets::Bitmap* logo = assets::find_system("logo");
    if (splash1 != nullptr) {
      oled_fb_.blit_bitmap_1bpp(kSplashVisorX, kSplashVisorY, splash1->width, splash1->height, splash1->data,
                                 splash1->size, splash1->width, splash1->height, false, true);
    }
    if (logo != nullptr) {
      oled_fb_.blit_bitmap_1bpp(kSplashLogoX, kSplashLogoY, logo->width, logo->height, logo->data, logo->size,
                                 logo->width, logo->height, false, true);
    }
  } else {
    oled_fb_.fill_rect(0, 0, oled_fb_.width(), oled_fb_.height(), true);
    oled_fb_.draw_text(8, 6, "KOTOPROTO", false);
    oled_fb_.draw_text(8, 17, "by Kotaz", false);
    oled_fb_.draw_text(8, 28, kVersion, false);
    const int bar = static_cast<int>(elapsed * 110 / kStartupMs);
    oled_fb_.fill_rect(8, 50, std::clamp(bar, 0, 110), 6, false);
  }
}

void App::render_oled(std::uint32_t now_ms) {
  oled_fb_.clear();

  if (mode_ == Mode::Startup) {
    render_oled_startup(now_ms);
    return;
  }

  render_oled_header();

  if (mode_ == Mode::Settings) {
    render_oled_settings(now_ms);
    return;
  }
  if (mode_ == Mode::Snake) {
    oled_fb_.draw_text(0, 20, "Snake!", true);
    char line[24];
    std::snprintf(line, sizeof(line), "score %u", static_cast<unsigned>(snake_.score));
    oled_fb_.draw_text(0, 32, line, true);
    oled_fb_.draw_text(0, 44, snake_.play == Snake::Play::Stop ? "stick=retry" : "stick=move", true);
    oled_fb_.draw_text(0, 54, "B exit", true);
    return;
  }
  if (mode_ == Mode::Auto) {
    render_oled_auto(now_ms);
    return;
  }
  render_oled_faceset();
}

void App::render_ring(std::uint32_t now_ms) {
  Color leds[hal::kLedRingCount];
  if (!state_.led_enabled) {
    for (int i = 0; i < hal::kLedRingCount; ++i) {
      leds[i] = Color::black();
    }
    ring_.present(leds, hal::kLedRingCount);
    return;
  }
  const int speed = state_.boop ? 20 : (state_.dizzy ? 18 : 50);
  const int head = static_cast<int>((now_ms / static_cast<std::uint32_t>(speed)) % hal::kLedRingCount);
  const Color base = accent();
  for (int i = 0; i < hal::kLedRingCount; ++i) {
    const int dist = (i - head + hal::kLedRingCount) % hal::kLedRingCount;
    std::uint8_t fade = 22;
    if (dist == 0) {
      fade = 255;
    } else if (dist == 1 || dist == 11) {
      fade = 150;
    } else if (dist == 2 || dist == 10) {
      fade = 70;
    }
    if (state_.mic > 0.15f && dist > 3) {
      fade = static_cast<std::uint8_t>(std::min(255, static_cast<int>(fade + state_.mic * 80)));
    }
    leds[i] = scale(scale(base, fade), state_.brightness);
  }
  ring_.present(leds, hal::kLedRingCount);
}

void App::tick() {
  const std::uint32_t now = clock_.millis();
  ++tick_count_;

  update_hid_link();
  poll_menu_hold();
  update_sensors(now);
  update_face_player(now);
  update_auto(now);
  update_blink(now);
  update_snake(now);
  update_fps(now);

  if (mode_ == Mode::Snake) {
    render_snake(now);
  } else if (!state_.matrix_enabled) {
    matrix_fb_.clear(Color::black());
  } else {
    render_face(now);
  }
  render_oled(now);
  render_ring(now);

  const Color* src = matrix_fb_.data();
  const int count = matrix_fb_.width() * matrix_fb_.height();
  std::vector<Color> dimmed(static_cast<std::size_t>(count));
  if (!state_.matrix_enabled) {
    for (int i = 0; i < count; ++i) {
      dimmed[static_cast<std::size_t>(i)] = Color::black();
    }
  } else {
    for (int i = 0; i < count; ++i) {
      dimmed[static_cast<std::size_t>(i)] = scale(src[i], state_.brightness);
    }
  }
  matrix_.present(dimmed.data(), matrix_fb_.width(), matrix_fb_.height());
  oled_.present(oled_fb_.packed(), oled_fb_.width(), oled_fb_.height());
}

}  // namespace koto
