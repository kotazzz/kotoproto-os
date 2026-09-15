#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "koto/assets/ba_player.hpp"
#include "koto/assets/emotions.hpp"
#include "koto/color.hpp"
#include "koto/config.hpp"
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
  bool set_face(const char* id, bool with_transition = true);
  void preview_blink();
  void poll_sensors() { update_sensors(clock_.millis()); }

  std::uint32_t tick_count() const { return tick_count_; }
  const DeviceState& state() const { return state_; }
  const gfx::Framebuffer& matrix_buffer() const { return matrix_fb_; }
  const gfx::OledCanvas& oled_buffer() const { return oled_fb_; }
  const assets::Emotion* emotion() const { return emotion_; }

 private:
  enum class Mode {
    Startup,
    FaceSet,
    Auto,
    Settings,
    Games,
    Snake,
    Casino,
    Dino,
    BadApple,
    Flappy,
    Tetris,
    Dvd,
    Bsod,
    Spectrum
  };
  enum class BlinkState { Idle, Closing, Closed, Opening };

  void apply_pad(const PadState& pad);
  void update_sensors(std::uint32_t now_ms);
  void update_hid_link();
  void update_face_player(std::uint32_t now_ms);
  void update_auto(std::uint32_t now_ms);
  void update_blink(std::uint32_t now_ms);
  void update_snake(std::uint32_t now_ms);
  void update_dino(std::uint32_t now_ms);
  void update_badapple(std::uint32_t now_ms);
  void update_flappy(std::uint32_t now_ms);
  void update_tetris(std::uint32_t now_ms);
  void update_dvd(std::uint32_t now_ms);
  void update_bsod(std::uint32_t now_ms);
  void update_spectrum(std::uint32_t now_ms);
  void update_fps(std::uint32_t now_ms);
  void render_face(std::uint32_t now_ms);
  void render_snake(std::uint32_t now_ms);
  void render_dino(std::uint32_t now_ms);
  void render_badapple();
  void render_flappy(std::uint32_t now_ms);
  void render_tetris(std::uint32_t now_ms);
  void render_dvd(std::uint32_t now_ms);
  void render_bsod(std::uint32_t now_ms);
  void render_spectrum();
  void render_oled(std::uint32_t now_ms);
  void render_oled_settings(std::uint32_t now_ms);
  void render_oled_faceset();
  void render_oled_auto(std::uint32_t now_ms);
  void render_ring(std::uint32_t now_ms);
  void enter_faceset(int set);
  void enter_auto();
  void enter_settings();
  void enter_settings_list();
  void cycle_menu_pages();
  void poll_menu_hold();
  void poll_esc_hold();
  void enter_games();
  void enter_snake();
  void enter_casino();
  void enter_dino();
  void enter_badapple();
  void handle_badapple_pad(const PadState& pad, const PadState& prev);
  void enter_flappy();
  void enter_tetris();
  void enter_dvd();
  void dvd_reset();
  void dvd_logo_size(int& w, int& h) const;
  void enter_bsod();
  void bsod_reset();
  int bsod_bar_y0() const;
  int bsod_bar_max() const;
  void enter_spectrum();
  void enter_safe_mode();
  void finish_startup();
  void set_sequence(const char* name, bool loop, bool with_transition);
  void apply_octant_face();
  void pick_next_auto_face();
  void handle_stick_faceset(const PadState& pad);
  void handle_stick_settings(const PadState& pad, const PadState& prev);
  void handle_stick_games(const PadState& pad);
  void handle_stick_snake(const PadState& pad);
  void snake_reset();
  void snake_game_over();
  void handle_dino_pad(const PadState& pad, const PadState& prev);
  void dino_reset();
  void handle_flappy_pad(const PadState& pad, const PadState& prev);
  void flappy_reset();
  void flappy_flap();
  void flappy_crash();
  void flappy_spawn_pipe();
  void flappy_spawn_cloud();
  bool flappy_hit() const;
  bool flappy_pipe_at(int x, int y) const;
  void handle_tetris_pad(const PadState& pad, const PadState& prev);
  void handle_spectrum_pad(const PadState& pad, const PadState& prev);
  void tetris_reset();
  void tetris_spawn();
  void tetris_refill_bag();
  int tetris_draw_bag();
  bool tetris_fits(int x, int y, int type, int rot) const;
  void tetris_lock();
  int tetris_clear_lines();
  void tetris_game_over();
  int tetris_ghost_y() const;
  std::uint32_t tetris_fall_ms() const;
  void dino_start_jump();
  void dino_end_jump();
  void dino_crash();
  void dino_spawn_obstacle();
  void dino_spawn_cloud();
  bool dino_hit() const;
  void handle_casino_pad(const PadState& pad, const PadState& prev);
  void roll_casino();
  void update_casino(std::uint32_t now_ms);
  void render_casino(std::uint32_t now_ms);
  void render_oled_games();
  void render_oled_casino();
  void render_oled_snake();
  void render_oled_dino();
  void render_oled_badapple();
  void render_oled_flappy();
  void render_oled_tetris();
  void render_oled_dvd();
  void render_oled_bsod();
  void render_oled_spectrum();
  bool in_minigame() const;
  bool in_arcade() const;
  void add_settings_cursor(int delta);
  void nudge_setting(int delta);
  void activate_setting();
  void begin_settings_slide(int dir);
  void move_settings_focus(int dx, int dy);
  void nudge_focused_slider(int delta);
  void sync_brightness();
  void render_oled_header();
  void render_oled_startup(std::uint32_t now_ms);
  void render_settings_page();
  void draw_icon(int x, int y, const char* id, bool on);
  void draw_face_thumb(int x, int y, const char* id, bool invert);
  void draw_toggle(int x, int y, bool on);
  void draw_slider(int x, int y, int w, int raw, int max_value);
  void apply_effect(std::uint32_t now_ms);
  void apply_dizzy_motion(std::uint32_t now_ms);
  void apply_wink_motion(std::uint32_t now_ms);
  void apply_angry_motion(std::uint32_t now_ms);
  void apply_glitch_motion(std::uint32_t now_ms);
  void apply_mouth_effect();
  void apply_blink_overlay();
  void apply_boop_overlay(std::uint32_t now_ms);
  void apply_boop_hue(std::uint32_t now_ms);
  void apply_gyro_nudge();
  void emit_wink_sparks();
  void draw_sparks();
  bool boop_allowed() const;
  bool blink_allowed() const;
  Color accent() const;
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
  const assets::Emotion* emotion_ = nullptr;
  const assets::Emotion* auto_next_ = nullptr;
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
  int settings_page_ = 0;
  int settings_row_ = 0;
  int settings_col_ = 0;
  int settings_slide_dir_ = 0;
  std::uint32_t settings_slide_start_ms_ = 0;
  std::vector<std::uint8_t> settings_from_;
  bool menu_long_fired_ = false;
  std::uint32_t menu_down_ms_ = 0;
  bool esc_long_fired_ = false;
  std::uint32_t esc_down_ms_ = 0;
  bool games_x_latched_ = false;
  int mouth_calibrate_left_ = 0;
  int boop_calibrate_left_ = 0;
  int calib_percent_ = 0;
  int fps_ticks_ = 0;
  float boop_threshold_ = 0.45f;
  float mouth_level_ = 0;
  float mouth_peak_ = 0.18f;
  float mouth_baseline_ = 0;
  float mouth_baseline_acc_ = 0;
  float boop_cal_acc_ = 0;
  float boop_rest_ = 0;
  float mic_pcm_[kMicPcmSize]{};
  int mic_pcm_count_ = 0;
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
  std::uint32_t effect_started_ms_ = 0;

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
  const assets::Emotion* randomize_pick_ = nullptr;
  int randomize_frame_ = 0;
  std::uint32_t boop_started_ms_ = 0;
  std::uint32_t rng_state_ = 0xA341316Cu;

  face::TransitionKind trans_kind_ = face::TransitionKind::None;
  std::uint32_t trans_start_ms_ = 0;
  std::uint32_t trans_duration_ms_ = 0;
  std::vector<Color> trans_from_;
  std::vector<Color> trans_to_;

  struct Casino {
    enum class Phase { Idle, Hold, Coast, Stop };
    Phase phase = Phase::Idle;
    int result[3] = {1, 2, 3};
    int offset[3] = {16, 32, 48};
    int speed[3] = {4, 5, 6};
    bool rolling[3] = {false, false, false};
    std::uint32_t stop_ms[3] = {0, 0, 0};
  } casino_;

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

  struct Dino {
    enum class Play { Wait, Run, Crash };
    Play play = Play::Wait;
    bool jumping = false;
    bool ducking = false;
    bool jump_held = false;
    bool duck_held = false;
    bool reached_min = false;
    bool speed_drop = false;
    bool eye_closed = false;
    float y = 16.f;
    float vy = 0.f;
    float speed = 0;
    float distance = 0;
    float horizon = 0;
    int score = 0;
    int hi = 0;
    int run_frame = 0;
    int bird_frame = 0;
    int last_kind = -1;
    int same_kind = 0;
    std::uint32_t run_ms = 0;
    std::uint32_t crash_ms = 0;
    std::uint32_t anim_ms = 0;
    std::uint32_t blink_ms = 0;
    struct Obstacle {
      int kind = 0;
      int size = 1;
      int alt = 0;
      float x = 0;
      int y = 0;
      int w = 0;
      int h = 0;
      float gap = 0;
      float speed_off = 0;
    };
    std::vector<Obstacle> obstacles;
    struct Cloud {
      float x = 0;
      int y = 0;
    };
    std::vector<Cloud> clouds;
  } dino_;

  struct Flappy {
    enum class Play { Wait, Run, Crash };
    Play play = Play::Wait;
    bool flap_held = false;
    float y = 10.f;
    float vy = 0.f;
    float speed = 0;
    float ground = 0;
    int score = 0;
    int hi = 0;
    int wing = 1;
    std::uint32_t crash_ms = 0;
    std::uint32_t anim_ms = 0;
    struct Pipe {
      float x = 0;
      int gap_y = 6;
      bool scored = false;
    };
    std::vector<Pipe> pipes;
    struct Cloud {
      float x = 0;
      int y = 0;
    };
    std::vector<Cloud> clouds;
  } flappy_;

  struct Tetris {
    enum class Play { Wait, Run, Crash };
    Play play = Play::Wait;
    int grid[120] = {};
    int x = 3;
    int y = 0;
    int rot = 0;
    int type = 1;
    int next = 1;
    int score = 0;
    int hi = 0;
    int lines = 0;
    int bag[7] = {};
    int bag_n = 0;
    bool left_held = false;
    bool right_held = false;
    bool down_held = false;
    bool rot_held = false;
    std::uint32_t fall_ms = 0;
    std::uint32_t das_ms = 0;
    std::uint32_t crash_ms = 0;
  } tetris_;

  struct Dvd {
    int x = 0;
    int y = 0;
    int dx = 1;
    int dy = 1;
    std::uint8_t hue = 0;
    int hits = 0;
    int corners = 0;
    std::uint32_t corner_ms = 0;
  } dvd_;

  struct Bsod {
    int bars = 0;
    std::uint32_t start_ms = 0;
  } bsod_;

  struct Spectrum {
    float level[kSpectrumBands]{};
    float peak[kSpectrumBands]{};
    float norm = 0.08f;
  } spectrum_;

  assets::BaPlayer badapple_{};
  std::uint32_t badapple_accum_ms_ = 0;
  bool badapple_fast_ = false;
  Color present_dim_[kMatrixW * kMatrixH]{};
};

}  // namespace koto
