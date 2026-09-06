#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "koto/color.hpp"
#include "koto/face/transition.hpp"
#include "koto/gfx/framebuffer.hpp"
#include "koto/gfx/oled_canvas.hpp"
#include "koto/hal/clock.hpp"
#include "koto/hal/fan.hpp"
#include "koto/hal/hid_host.hpp"
#include "koto/hal/led_ring.hpp"
#include "koto/hal/matrix.hpp"
#include "koto/hal/oled.hpp"
#include "koto/hal/sensors.hpp"
#include "koto/hal/store.hpp"
#include "koto/protocol/mocute.hpp"

namespace koto {

class App {
 public:
  App(hal::IMatrix& matrix,
      hal::IOled& oled,
      hal::ILedRing& ring,
      hal::IHidHost& hid,
      hal::IClock& clock,
      hal::ISensors& sensors,
      hal::IFan& fan,
      hal::IStore& store);

  void init();
  void tick();
  void handle_report(const std::uint8_t* data, std::size_t len);
  void calibrate_boop();
  void calibrate_mouth();
  void set_fan_speed(std::uint8_t duty);
  void restart();
  bool save_settings();
  bool load_settings();

  std::uint32_t tick_count() const { return tick_count_; }
  const DeviceState& state() const { return state_; }
  const gfx::Framebuffer& matrix_buffer() const { return matrix_fb_; }
  const gfx::OledCanvas& oled_buffer() const { return oled_fb_; }

 private:
  enum class Mode { Startup, FaceSet, Auto, Settings, Snake };
  enum class BlinkState { Idle, Closing, Closed, Opening };

  void apply_pad(const PadState& pad);
  void update_sensors(std::uint32_t now_ms);
  void update_hid_link();
  void update_face_player(std::uint32_t now_ms);
  void update_auto(std::uint32_t now_ms);
  void update_blink(std::uint32_t now_ms);
  void update_snake(std::uint32_t now_ms);
  void update_fps(std::uint32_t now_ms);
  void render_face(std::uint32_t now_ms);
  void render_snake(std::uint32_t now_ms);
  void render_oled(std::uint32_t now_ms);
  void render_oled_settings(std::uint32_t now_ms);
  void render_oled_faceset();
  void render_oled_auto(std::uint32_t now_ms);
  void render_ring(std::uint32_t now_ms);
  void enter_faceset(int set);
  void enter_auto();
  void enter_settings();
  void enter_snake();
  void enter_safe_mode();
  void finish_startup();
  void set_sequence(const char* name, bool loop, bool with_transition);
  void apply_octant_face();
  void pick_next_auto_face();
  void handle_stick_faceset(const PadState& pad);
  void handle_stick_settings(const PadState& pad, const PadState& prev);
  void handle_stick_snake(const PadState& pad);
  void snake_reset();
  void snake_game_over();
  void add_settings_cursor(int delta);
  void nudge_setting(int delta);
  void activate_setting();
  void sync_brightness();
  void apply_blob_to_state();
  void capture_state_to_blob();
  void render_oled_header();
  void draw_settings_pager();
  void draw_face_thumb(int x, int y, const char* sequence, bool invert);
  void blit_oled_face_parts(int eye_x, int eye_y, int nose_x, int nose_y, int mouth_x, int mouth_y,
                            const char* sequence);
  void draw_toggle(int x, int y, bool on);
  void apply_dizzy_motion(std::uint32_t now_ms);
  void apply_wink_motion(std::uint32_t now_ms);
  void apply_angry_motion(std::uint32_t now_ms);
  void blit_mouth_2x(const char* id, bool rot180);
  void apply_mouth_effect();
  void apply_blink_overlay();
  void apply_boop_overlay(std::uint32_t now_ms);
  void apply_gyro_nudge();
  void emit_wink_sparks();
  void draw_sparks();
  bool boop_allowed() const;
  bool blink_allowed() const;
  Color visor_color() const;
  std::uint32_t rng();

  hal::IMatrix& matrix_;
  hal::IOled& oled_;
  hal::ILedRing& ring_;
  hal::IHidHost& hid_;
  hal::IClock& clock_;
  hal::ISensors& sensors_;
  hal::IFan& fan_;
  hal::IStore& store_;

  gfx::Framebuffer matrix_fb_;
  gfx::OledCanvas oled_fb_;
  DeviceState state_;
  PadState prev_pad_;

  Mode mode_ = Mode::Startup;
  BlinkState blink_ = BlinkState::Idle;
  const char* sequence_ = "Startup";
  const char* auto_next_ = "Joy";
  bool sequence_loop_ = false;
  bool blink_held_ = false;
  bool joystick_centered_ = true;
  bool prev_boop_ = false;
  bool action_done_ = false;
  bool hid_was_connected_ = false;
  int frame_index_ = 0;
  int frame_count_ = 1;
  int prev_octant_ = 255;
  int boop_triggers_ = 0;
  bool settings_y_latched_ = false;
  bool settings_x_latched_ = false;
  int mouth_calibrate_left_ = 0;
  int boop_calibrate_left_ = 0;
  int calib_percent_ = 0;
  int fps_ticks_ = 0;
  float boop_threshold_ = 0.45f;
  float mouth_level_ = 0;
  float mouth_peak_ = 0.35f;
  float mouth_baseline_ = 0;
  float mouth_baseline_acc_ = 0;
  float boop_cal_acc_ = 0;
  float boop_rest_ = 0;
  float prev_pitch_ = 0;
  float prev_roll_ = 0;
  float gyro_rest_pitch_ = 0;
  float gyro_rest_roll_ = 0;
  int gyro_kick_x_ = 0;
  int gyro_kick_y_ = 0;
  std::uint32_t gyro_kick_until_ = 0;
  std::uint32_t gyro_last_motion_ms_ = 0;
  std::uint32_t wink_cycle_ms_ = 0;
  bool wink_burst_ = false;

  struct Spark {
    float x = 0;
    float y = 0;
    float vx = 0;
    float vy = 0;
    int life = 0;
  };
  std::vector<Spark> sparks_;
  std::uint32_t tick_count_ = 0;
  std::uint32_t frame_started_ms_ = 0;
  std::uint32_t blink_started_ms_ = 0;
  std::uint32_t blink_wait_ms_ = 8000;
  std::uint32_t blink_closed_ms_ = 80;
  std::uint32_t auto_started_ms_ = 0;
  std::uint32_t auto_wait_ms_ = 8000;
  std::uint32_t last_auto_skip_ms_ = 0;
  std::uint32_t startup_started_ms_ = 0;
  std::uint32_t fps_window_ms_ = 0;
  std::uint32_t randomize_last_ms_ = 0;
  std::uint32_t boop_started_ms_ = 0;
  std::uint32_t toast_until_ms_ = 0;
  std::uint32_t rng_state_ = 0xA341316Cu;
  char toast_[20] = {};

  face::TransitionKind trans_kind_ = face::TransitionKind::None;
  std::uint32_t trans_start_ms_ = 0;
  std::uint32_t trans_duration_ms_ = 0;
  std::vector<Color> trans_from_;
  std::vector<Color> trans_to_;

  struct Snake {
    enum class Play { Stop, Wait, Run };
    Play play = Play::Wait;
    int dir = 0;
    int last_dir = 0;
    int score = 0;
    int grow = 5;
    int fruit_x = 10;
    int fruit_y = 3;
    bool fruit_on = true;
    std::uint32_t last_step_ms = 0;
    std::uint32_t last_blink_ms = 0;
    std::vector<std::pair<int, int>> body;
  } snake_;
};

}  // namespace koto
