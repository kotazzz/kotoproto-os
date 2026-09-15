#include "koto/app.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "koto/assets/ba_player.hpp"
#include "koto/assets/bitmaps.hpp"
#include "koto/assets/casino.hpp"
#include "koto/assets/dino.hpp"
#include "koto/assets/flappy.hpp"
#include "koto/assets/tetris.hpp"
#include "koto/assets/dvd.hpp"
#include "koto/assets/bsod.hpp"
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

constexpr int kSnakeHudH = 16;
constexpr int kSnakeBorder = 1;
constexpr int kSnakeCell = 2;
constexpr int kSnakeW = (kMatrixW - 2 * kSnakeBorder) / kSnakeCell;
constexpr int kSnakeH = (kMatrixH - 2 * kSnakeBorder) / kSnakeCell;
constexpr int kSnakeOx = (kMatrixW - kSnakeW * kSnakeCell) / 2;
constexpr int kSnakeOy = (kMatrixH - kSnakeH * kSnakeCell) / 2;
constexpr int kOledSnakeInnerW = kSnakeW * kSnakeCell;
constexpr int kOledSnakeInnerH = kSnakeH * kSnakeCell;
constexpr int kOledSnakeX =
    kSnakeBorder + (kOledW - 2 * kSnakeBorder - kOledSnakeInnerW) / 2;
constexpr int kOledSnakeY = kSnakeHudH + kSnakeBorder +
                           (kOledH - kSnakeHudH - 2 * kSnakeBorder - kOledSnakeInnerH) / 2;
constexpr std::uint32_t kSnakeStepMs = 200;
constexpr std::uint32_t kSnakeBlinkMs = 100;
constexpr int kToggleW = 14;
constexpr int kToggleH = 7;
constexpr int kSliderH = 7;
constexpr int kParamIcon = 7;
constexpr int kScreenIcon = 9;
constexpr int kSettingsHeaderH = 16;
constexpr int kControlY0 = 22;
constexpr int kControlY1 = 42;
constexpr int kCol0X = 4;
constexpr int kCol1X = 66;
constexpr int kGameIcon = 32;
constexpr int kSlotH = 16;
constexpr int kSlotCount = 6;
constexpr int kReelY = 7;
const int kReelX[3] = {5, 24, 43};
const char* kSlotId[kSlotCount] = {"seven", "cherry", "coin", "bell", "star", "cup"};
const char* kGameTitle[kGameCount] = {"SNAKE", "CASINO", "DINO", "APPLE", "FLAPPY", "TETRIS", "DVD",
                                      "BSOD"};
const char* kGameIconId[kGameCount] = {"game_snake", "game_casino", "game_dino", "game_badapple",
                                      "game_flappy", "game_tetris", "game_dvd", "game_bsod"};

constexpr int kDinoX = 2;
constexpr int kDinoGroundY = 16;
constexpr int kDinoMinJump = 10;
constexpr int kDinoHorizonY = 28;
constexpr int kDinoHorizonW = 200;
constexpr int kDinoTailW = 4;
constexpr int kDinoDuckTailW = 5;
constexpr int kDinoCactusInset = 1;
constexpr int kDinoBirdInset = 2;
constexpr int kDinoJumpCeil = -2;
constexpr float kDinoFrameMs = 1000.f / 60.f;
constexpr float kDinoSpeed0 = 0.75f;
constexpr float kDinoAccel = 0.0008f;
constexpr float kDinoMaxSpeed = 1.55f;
constexpr float kDinoGravity = 0.16f;
constexpr float kDinoJumpV = -2.8f;
constexpr float kDinoSpeedDrop = 3.f;
constexpr float kDinoScoreK = 0.18f;
constexpr float kDinoCloudSpeed = 0.2f;
constexpr float kDinoBirdMinSpeed = 1.15f;
constexpr float kDinoGapK = 0.9f;
constexpr std::uint32_t kDinoClearMs = 3000;
constexpr std::uint32_t kDinoRestartMs = 750;
constexpr std::uint32_t kDinoRunAnimMs = 83;
constexpr std::uint32_t kDinoBirdAnimMs = 166;
constexpr std::uint32_t kDinoBlinkMs = 180;
constexpr std::uint32_t kDinoBlinkEveryMs = 2200;
constexpr std::uint32_t kBadAppleFrameMs = 40;
constexpr std::uint32_t kBadAppleSpeedNum = 5;
constexpr std::uint32_t kBadAppleSpeedDen = 2;

constexpr int kFlappyX = 12;
constexpr int kFlappyGroundY = 28;
constexpr int kFlappyGapH = 16;
constexpr int kFlappyPipeW = 10;
constexpr int kFlappyCapW = 12;
constexpr int kFlappyCapH = 5;
constexpr int kFlappyHitInset = 1;
constexpr float kFlappyGravity = 0.12f;
constexpr float kFlappyFlapV = -1.45f;
constexpr float kFlappyMaxFall = 1.7f;
constexpr float kFlappySpeed0 = 0.55f;
constexpr float kFlappyAccel = 0.00022f;
constexpr float kFlappyMaxSpeed = 0.95f;
constexpr float kFlappySpacing = 54.f;
constexpr float kFlappyCloudSpeed = 0.18f;
constexpr float kFlappyBobAmp = 1.6f;
constexpr std::uint32_t kFlappyRestartMs = 750;
constexpr std::uint32_t kFlappyAnimMs = 90;
constexpr std::uint32_t kFlappyBobMs = 220;

const char* kFlappyBird[3] = {"bird_0", "bird_1", "bird_2"};

constexpr int kTetrisW = 10;
constexpr int kTetrisH = 12;
constexpr int kTetrisCell = 2;
constexpr int kTetrisOx = 1;
constexpr int kTetrisOy = 8;
constexpr int kTetrisNextX = 26;
constexpr int kTetrisNextY = 10;
constexpr int kTetrisNextCell = 4;
constexpr std::uint32_t kTetrisRestartMs = 750;
constexpr std::uint32_t kTetrisDasMs = 160;
constexpr std::uint32_t kTetrisArrMs = 50;
constexpr std::uint8_t kDvdHueStep = 37;
constexpr std::uint32_t kDvdCornerMs = 800;
constexpr Color kBsodBlue{0, 120, 215};
constexpr int kBsodHeadX = 2;
constexpr int kBsodHeadY = 1;
constexpr int kBsodHeadLineH = gfx::kFontHeight + 1;
constexpr int kBsodHeadLines = 2;
constexpr int kBsodBarH = 2;
constexpr const char* kBsodHead0 = ":(";
constexpr const char* kBsodHead1 = "Your PC";
constexpr std::uint32_t kBsodBarMs = 100;
constexpr std::uint32_t kBsodHoldMs = 900;

const char* kTetrisCellId[7] = {"cell_i", "cell_o", "cell_t", "cell_s", "cell_z", "cell_j", "cell_l"};

// type, rotation, block, {dx, dy}
constexpr int kTetrisOff[7][4][4][2] = {
    {{{0, 1}, {1, 1}, {2, 1}, {3, 1}},
     {{2, 0}, {2, 1}, {2, 2}, {2, 3}},
     {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
     {{1, 0}, {1, 1}, {1, 2}, {1, 3}}},
    {{{1, 0}, {2, 0}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {2, 1}}},
    {{{1, 0}, {0, 1}, {1, 1}, {2, 1}},
     {{1, 0}, {1, 1}, {2, 1}, {1, 2}},
     {{0, 1}, {1, 1}, {2, 1}, {1, 2}},
     {{1, 0}, {0, 1}, {1, 1}, {1, 2}}},
    {{{1, 0}, {2, 0}, {0, 1}, {1, 1}},
     {{1, 0}, {1, 1}, {2, 1}, {2, 2}},
     {{1, 1}, {2, 1}, {0, 2}, {1, 2}},
     {{0, 0}, {0, 1}, {1, 1}, {1, 2}}},
    {{{0, 0}, {1, 0}, {1, 1}, {2, 1}},
     {{1, 0}, {0, 1}, {1, 1}, {0, 2}},
     {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
     {{2, 0}, {1, 1}, {2, 1}, {1, 2}}},
    {{{0, 0}, {0, 1}, {1, 1}, {2, 1}},
     {{1, 0}, {2, 0}, {1, 1}, {1, 2}},
     {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
     {{1, 0}, {1, 1}, {0, 2}, {1, 2}}},
    {{{2, 0}, {0, 1}, {1, 1}, {2, 1}},
     {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
     {{0, 1}, {1, 1}, {2, 1}, {0, 2}},
     {{0, 0}, {1, 0}, {1, 1}, {1, 2}}},
};

void blit_tetris_cell(gfx::Framebuffer& fb, int type, int px, int py, int cell) {
  const char* id = "ghost";
  if (type >= 1 && type <= 7) {
    id = kTetrisCellId[type - 1];
  } else if (type != 0) {
    id = "wall";
  }
  const assets::TetrisSprite* sprite = assets::find_tetris_sprite(id);
  if (sprite == nullptr || sprite->width <= 0 || sprite->height <= 0) {
    return;
  }
  for (int y = 0; y < cell; ++y) {
    for (int x = 0; x < cell; ++x) {
      const int sx = (x * sprite->width) / cell;
      const int sy = (y * sprite->height) / cell;
      const std::size_t i = static_cast<std::size_t>(sy * sprite->width + sx) * 3;
      if (i + 2 >= sprite->size) {
        continue;
      }
      if ((sprite->rgb[i] | sprite->rgb[i + 1] | sprite->rgb[i + 2]) == 0) {
        continue;
      }
      fb.set_pixel(px + x, py + y, Color{sprite->rgb[i], sprite->rgb[i + 1], sprite->rgb[i + 2]});
    }
  }
}

void blit_tetris_wall_col(gfx::Framebuffer& fb, int x, int y0, int h) {
  const assets::TetrisSprite* wall = assets::find_tetris_sprite("wall");
  if (wall == nullptr) {
    fb.fill_rect(x, y0, 1, h, Color{36, 52, 72});
    return;
  }
  for (int y = 0; y < h; ++y) {
    const int sy = y % wall->height;
    const std::size_t i = static_cast<std::size_t>(sy * wall->width) * 3;
    if (i + 2 >= wall->size) {
      continue;
    }
    fb.set_pixel(x, y0 + y, Color{wall->rgb[i], wall->rgb[i + 1], wall->rgb[i + 2]});
  }
}

void blit_tetris_piece(gfx::Framebuffer& fb, int gx, int gy, int type, int rot, int px0, int py0, int cell,
                       bool ghost) {
  const int t = std::clamp(type, 1, 7) - 1;
  const int r = rot & 3;
  for (int i = 0; i < 4; ++i) {
    const int cx = gx + kTetrisOff[t][r][i][0];
    const int cy = gy + kTetrisOff[t][r][i][1];
    if (cy < 0) {
      continue;
    }
    blit_tetris_cell(fb, ghost ? 0 : type, px0 + cx * cell, py0 + cy * cell, cell);
  }
}

void blit_dvd_logo(gfx::Framebuffer& fb, int x, int y, Color tint) {
  const assets::DvdSprite* sprite = assets::find_dvd_sprite("logo");
  if (sprite == nullptr || sprite->width <= 0 || sprite->height <= 0) {
    return;
  }
  for (int row = 0; row < sprite->height; ++row) {
    for (int col = 0; col < sprite->width; ++col) {
      const std::size_t i = static_cast<std::size_t>((row * sprite->width + col) * 3);
      if (i + 2 >= sprite->size) {
        continue;
      }
      if ((sprite->rgb[i] | sprite->rgb[i + 1] | sprite->rgb[i + 2]) == 0) {
        continue;
      }
      const Color pix{
          static_cast<std::uint8_t>((sprite->rgb[i] * tint.r) / 255),
          static_cast<std::uint8_t>((sprite->rgb[i + 1] * tint.g) / 255),
          static_cast<std::uint8_t>((sprite->rgb[i + 2] * tint.b) / 255),
      };
      if ((pix.r | pix.g | pix.b) == 0) {
        continue;
      }
      fb.set_pixel(x + col, y + row, pix);
    }
  }
}

void blit_bsod_sprite(gfx::Framebuffer& fb, const char* id, int x, int y) {
  const assets::BsodSprite* sprite = assets::find_bsod_sprite(id);
  if (sprite == nullptr) {
    return;
  }
  fb.blit_rgb(x, y, sprite->width, sprite->height, sprite->rgb, sprite->size, true);
}

const char* kDinoCactusS[3] = {"cactus_s1", "cactus_s2", "cactus_s3"};
const char* kDinoCactusL[3] = {"cactus_l1", "cactus_l2", "cactus_l3"};
const char* kDinoBird[2] = {"bird_0", "bird_1"};

void blit_dino_sprite(gfx::Framebuffer& fb, const char* id, int x, int y) {
  const assets::DinoSprite* sprite = assets::find_dino_sprite(id);
  if (sprite == nullptr) {
    return;
  }
  fb.blit_rgb(x, y, sprite->width, sprite->height, sprite->rgb, sprite->size, true);
}

void blit_flappy_sprite(gfx::Framebuffer& fb, const char* id, int x, int y) {
  const assets::FlappySprite* sprite = assets::find_flappy_sprite(id);
  if (sprite == nullptr) {
    return;
  }
  fb.blit_rgb(x, y, sprite->width, sprite->height, sprite->rgb, sprite->size, true);
}

bool flappy_sprite_lit(const assets::FlappySprite* sprite, int x, int y) {
  if (sprite == nullptr || x < 0 || y < 0 || x >= sprite->width || y >= sprite->height) {
    return false;
  }
  const std::size_t i = static_cast<std::size_t>(y * sprite->width + x) * 3;
  if (i + 2 >= sprite->size) {
    return false;
  }
  return (sprite->rgb[i] | sprite->rgb[i + 1] | sprite->rgb[i + 2]) != 0;
}

void blit_flappy_pipe(gfx::Framebuffer& fb, int px, int gap_y, int gap_h, int ground_y) {
  const assets::FlappySprite* body = assets::find_flappy_sprite("pipe_body");
  const int bw = body != nullptr ? body->width : kFlappyPipeW;
  const int bh = body != nullptr ? body->height : 4;
  const int cap_x = px - (kFlappyCapW - bw) / 2;
  const int top_cap_y = gap_y - kFlappyCapH;
  for (int y = 0; y < top_cap_y; y += bh) {
    blit_flappy_sprite(fb, "pipe_body", px, y);
  }
  blit_flappy_sprite(fb, "pipe_cap_top", cap_x, top_cap_y);
  const int bot_cap_y = gap_y + gap_h;
  for (int y = bot_cap_y; y < ground_y; y += bh) {
    blit_flappy_sprite(fb, "pipe_body", px, y);
  }
  blit_flappy_sprite(fb, "pipe_cap_bot", cap_x, bot_cap_y);
}

const char* flappy_bird_id(bool crash, int wing) {
  if (crash) {
    return kFlappyBird[2];
  }
  return kFlappyBird[std::clamp(wing, 0, 2)];
}

bool dino_sprite_lit(const assets::DinoSprite* sprite, int x, int y) {
  if (sprite == nullptr || x < 0 || y < 0 || x >= sprite->width || y >= sprite->height) {
    return false;
  }
  const std::size_t i = static_cast<std::size_t>(y * sprite->width + x) * 3;
  if (i + 2 >= sprite->size) {
    return false;
  }
  return (sprite->rgb[i] | sprite->rgb[i + 1] | sprite->rgb[i + 2]) != 0;
}

const char* dino_obstacle_id(int kind, int size, int bird_frame) {
  if (kind == 2) {
    return kDinoBird[bird_frame & 1];
  }
  const int n = std::clamp(size, 1, 3);
  if (kind == 1) {
    return kDinoCactusL[n - 1];
  }
  return kDinoCactusS[n - 1];
}

const char* dino_trex_id(bool crash, bool jumping, bool ducking, bool running, int run_frame,
                         bool eye_closed) {
  if (crash) {
    return "trex_crash";
  }
  if (jumping) {
    return "trex_wait_0";
  }
  if (ducking) {
    return run_frame ? "trex_duck_1" : "trex_duck_0";
  }
  if (running) {
    return run_frame ? "trex_run_1" : "trex_run_0";
  }
  if (eye_closed) {
    return "trex_wait_1";
  }
  return "trex_wait_0";
}

const int kThumbPos[8][2] = {
    {0, 32}, {0, 16}, {43, 16}, {86, 16}, {86, 32}, {86, 48}, {43, 48}, {0, 48},
};

enum class SettingsKind : std::uint8_t { Slider, Toggle, Button };

enum class SettingsBind : std::uint8_t {
  Brightness,
  MatrixOn,
  Led,
  AutoBlink,
  Boop,
  Mouth,
  BoopSens,
  MouthSens,
  Fan,
  Rare,
  BoopCal,
  MouthCal,
  Save,
  Restart,
};

struct SettingsSlot {
  SettingsKind kind;
  SettingsBind bind;
  const char* icon;
  const char* title;
};

struct SettingsPage {
  const char* title;
  const char* icon;
  int row_count;
  int cols[2];
  SettingsSlot cells[2][2];
};

const SettingsPage kPages[kSettingsPageCount] = {
    {"MATRIX",
     "screen_matrix",
     2,
     {1, 1},
     {{{SettingsKind::Slider, SettingsBind::Brightness, "param_matrix", "BRIGHT"},
       {SettingsKind::Slider, SettingsBind::Brightness, nullptr, nullptr}},
      {{SettingsKind::Toggle, SettingsBind::MatrixOn, "param_matrix", "POWER"},
       {SettingsKind::Toggle, SettingsBind::MatrixOn, nullptr, nullptr}}}},
    {"SWITCHES",
     "screen_switches",
     2,
     {2, 2},
     {{{SettingsKind::Toggle, SettingsBind::Led, "param_headphones_led", "LEDS"},
       {SettingsKind::Toggle, SettingsBind::Boop, "param_boop", "BOOP"}},
      {{SettingsKind::Toggle, SettingsBind::AutoBlink, "param_auto_blink", "BLINK"},
       {SettingsKind::Toggle, SettingsBind::Mouth, "param_mouth", "MOUTH"}}}},
    {"CAL/SENSE",
     "screen_calibration",
     2,
     {2, 2},
     {{{SettingsKind::Button, SettingsBind::BoopCal, "param_boop", "B-CAL"},
       {SettingsKind::Button, SettingsBind::MouthCal, "param_mouth", "M-CAL"}},
      {{SettingsKind::Slider, SettingsBind::BoopSens, "param_boop", "B-SNS"},
       {SettingsKind::Slider, SettingsBind::MouthSens, "param_mouth", "M-SNS"}}}},
    {"OTHER",
     "screen_other",
     2,
     {1, 1},
     {{{SettingsKind::Slider, SettingsBind::Fan, "param_fan", "FAN"},
       {SettingsKind::Slider, SettingsBind::Fan, nullptr, nullptr}},
      {{SettingsKind::Slider, SettingsBind::Rare, "param_rare_transitions", "RARE"},
       {SettingsKind::Slider, SettingsBind::Rare, nullptr, nullptr}}}},
    {"SAVE/RST",
     "screen_restart_save",
     1,
     {2, 0},
     {{{SettingsKind::Button, SettingsBind::Save, "param_save", "SAVE"},
       {SettingsKind::Button, SettingsBind::Restart, "param_restart", "RESTART"}},
      {{SettingsKind::Button, SettingsBind::Save, nullptr, nullptr},
       {SettingsKind::Button, SettingsBind::Restart, nullptr, nullptr}}}},
};

bool slot_used(const SettingsSlot& slot) {
  return slot.icon != nullptr;
}

const SettingsPage& page_at(int page) {
  return kPages[std::clamp(page, 0, kSettingsPageCount - 1)];
}

int text_width(const char* text) {
  return static_cast<int>(std::strlen(text)) * (gfx::kFontWidth + gfx::kFontSpacing);
}

float mouth_gain_of(std::uint8_t sensitivity) {
  return 1.0f + (static_cast<float>(sensitivity) / 255.0f) * 4.0f;
}

float mouth_deadzone_of(std::uint8_t sensitivity) {
  const float t = static_cast<float>(sensitivity) / 255.0f;
  return 0.02f * (1.0f - t * 0.8f);
}

void blit_arcade_status(gfx::Framebuffer& fb, int score, bool wait, bool crash) {
  char buf[8];
  const char* text = buf;
  if (crash) {
    text = "DEAD";
  } else if (wait) {
    text = "GO";
  } else {
    std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(std::max(0, score)));
  }
  const int w = text_width(text);
  fb.draw_text(kMatrixW - w - 1, 1, text, Color::white());
}

void draw_oled_chevron(gfx::OledCanvas& oled, int x, int y, int dir, bool on) {
  for (int row = 0; row < 11; ++row) {
    const int dist = row < 6 ? row : 10 - row;
    const int start = dir < 0 ? 4 - dist : 0;
    oled.draw_hline(x + start, y + row, dist + 1, on);
  }
}

void blit_casino_tile(gfx::Framebuffer& fb, int x, int y, int id, int y0, int y1) {
  const int index = ((id % kSlotCount) + kSlotCount) % kSlotCount;
  const assets::CasinoSprite* sprite = assets::find_casino_sprite(kSlotId[index]);
  if (sprite == nullptr || sprite->rgb == nullptr) {
    return;
  }
  for (int row = 0; row < sprite->height; ++row) {
    const int py = y + row;
    if (py < y0 || py >= y1) {
      continue;
    }
    for (int col = 0; col < sprite->width; ++col) {
      const std::size_t i = static_cast<std::size_t>((row * sprite->width + col) * 3);
      if ((sprite->rgb[i] | sprite->rgb[i + 1] | sprite->rgb[i + 2]) == 0) {
        continue;
      }
      fb.set_pixel(x + col, py, Color{sprite->rgb[i], sprite->rgb[i + 1], sprite->rgb[i + 2]});
    }
  }
}

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

float pcm_level(const float* samples, int count) {
  if (samples == nullptr || count <= 0) {
    return 0;
  }
  float acc = 0;
  for (int i = 0; i < count; ++i) {
    acc += samples[i] * samples[i];
  }
  return std::min(1.0f, std::sqrt(acc / static_cast<float>(count)) * 1.41421356f);
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
  state_.mouth_sensitivity = blob.mouth_sensitivity == 0 ? 192 : blob.mouth_sensitivity;
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
  blob.mouth_sensitivity = state_.mouth_sensitivity;
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
  mouth_peak_ = 0.18f;
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
  effect_started_ms_ = clock_.millis();
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
  if (next == nullptr || in_minigame()) {
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
  settings_page_ = 0;
  settings_row_ = 0;
  settings_col_ = 0;
  settings_slide_dir_ = 0;
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
    enter_settings_list();
  } else {
    state_.setting_index = kSettingStatus1;
    settings_slide_dir_ = 0;
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

void App::poll_esc_hold() {
  if (mode_ == Mode::Startup || !state_.pad.esc() || esc_long_fired_) {
    return;
  }
  if (esc_down_ms_ == 0) {
    esc_down_ms_ = clock_.millis();
    return;
  }
  if (clock_.millis() - esc_down_ms_ >= kEscHoldMs) {
    esc_long_fired_ = true;
    enter_games();
  }
}

bool App::in_minigame() const {
  return mode_ == Mode::Games || in_arcade();
}

bool App::in_arcade() const {
  return mode_ == Mode::Snake || mode_ == Mode::Casino || mode_ == Mode::Dino ||
         mode_ == Mode::BadApple || mode_ == Mode::Flappy || mode_ == Mode::Tetris ||
         mode_ == Mode::Dvd || mode_ == Mode::Bsod;
}

void App::enter_games() {
  mode_ = Mode::Games;
  state_.scene = "games";
  games_x_latched_ = false;
  if (state_.game_index < 0 || state_.game_index >= kGameCount) {
    state_.game_index = 0;
  }
}

void App::enter_snake() {
  mode_ = Mode::Snake;
  state_.scene = "snake";
  snake_reset();
}

void App::enter_casino() {
  mode_ = Mode::Casino;
  state_.scene = "casino";
  casino_.phase = Casino::Phase::Idle;
  casino_.result[0] = 1;
  casino_.result[1] = 2;
  casino_.result[2] = 3;
  for (int i = 0; i < 3; ++i) {
    casino_.rolling[i] = false;
    casino_.offset[i] = casino_.result[i] * kSlotH;
    casino_.stop_ms[i] = 0;
  }
}

void App::enter_dino() {
  mode_ = Mode::Dino;
  state_.scene = "dino";
  dino_reset();
}

void App::enter_badapple() {
  mode_ = Mode::BadApple;
  state_.scene = "badapple";
  badapple_accum_ms_ = 0;
  badapple_fast_ = false;
  if (!assets::ba_init(badapple_, assets::badapple_blob(),
                       static_cast<std::uint32_t>(assets::badapple_blob_size()))) {
    badapple_ = assets::BaPlayer{};
    return;
  }
  assets::ba_next(badapple_);
}

void App::enter_flappy() {
  mode_ = Mode::Flappy;
  state_.scene = "flappy";
  flappy_reset();
}

void App::enter_tetris() {
  mode_ = Mode::Tetris;
  state_.scene = "tetris";
  tetris_reset();
}

void App::enter_dvd() {
  mode_ = Mode::Dvd;
  state_.scene = "dvd";
  dvd_reset();
}

void App::enter_bsod() {
  mode_ = Mode::Bsod;
  state_.scene = "bsod";
  bsod_reset();
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
  move_settings_focus(0, delta);
}

void App::begin_settings_slide(int dir) {
  const int next = settings_page_ + dir;
  if (dir == 0 || next < 0 || next >= kSettingsPageCount) {
    return;
  }
  const bool animate = settings_slide_dir_ == 0 &&
                       settings_from_.size() == static_cast<std::size_t>(oled_fb_.packed_size());
  settings_page_ = next;
  settings_row_ = 0;
  settings_col_ = 0;
  state_.setting_index = settings_page_;
  action_done_ = false;
  if (animate) {
    settings_slide_dir_ = dir;
    settings_slide_start_ms_ = clock_.millis();
  } else {
    settings_slide_dir_ = 0;
  }
}

void App::move_settings_focus(int dx, int dy) {
  if (state_.setting_index == kSettingStatus1 || state_.setting_index == kSettingStatus2) {
    return;
  }
  if (dy != 0) {
    const int next_row = settings_row_ + dy;
    if (next_row < 0) {
      return;
    }
    if (next_row == 0) {
      settings_row_ = 0;
      settings_col_ = 0;
      return;
    }
    const SettingsPage& page = page_at(settings_page_);
    if (next_row > page.row_count) {
      return;
    }
    settings_row_ = next_row;
    const int cols = std::max(1, page.cols[settings_row_ - 1]);
    if (settings_col_ >= cols) {
      settings_col_ = cols - 1;
    }
    return;
  }
  if (dx == 0) {
    return;
  }
  if (settings_row_ == 0) {
    begin_settings_slide(dx);
    return;
  }
  const SettingsPage& page = page_at(settings_page_);
  const SettingsSlot& slot = page.cells[settings_row_ - 1][settings_col_];
  if (slot_used(slot) && slot.kind == SettingsKind::Slider) {
    nudge_focused_slider(dx);
    return;
  }
  const int cols = std::max(1, page.cols[settings_row_ - 1]);
  const int next_col = settings_col_ + dx;
  if (next_col >= 0 && next_col < cols && slot_used(page.cells[settings_row_ - 1][next_col])) {
    settings_col_ = next_col;
  }
}

void App::nudge_focused_slider(int delta) {
  if (settings_row_ <= 0) {
    return;
  }
  const SettingsPage& page = page_at(settings_page_);
  const SettingsSlot& slot = page.cells[settings_row_ - 1][settings_col_];
  if (!slot_used(slot) || slot.kind != SettingsKind::Slider) {
    return;
  }
  auto clamp_u8 = [](int value, int lo, int hi) {
    return static_cast<std::uint8_t>(std::clamp(value, lo, hi));
  };
  if (slot.bind == SettingsBind::Brightness) {
    state_.brightness_level = clamp_u8(static_cast<int>(state_.brightness_level) + delta, 0, 15);
    sync_brightness();
  } else if (slot.bind == SettingsBind::BoopSens) {
    state_.boop_sensitivity = clamp_u8(static_cast<int>(state_.boop_sensitivity) + delta * 17, 0, 255);
    const float span = (255.0f - static_cast<float>(state_.boop_sensitivity)) / 255.0f;
    boop_threshold_ = std::clamp(boop_rest_ + 0.08f + span * 0.35f, 0.08f, 0.95f);
  } else if (slot.bind == SettingsBind::MouthSens) {
    state_.mouth_sensitivity = clamp_u8(static_cast<int>(state_.mouth_sensitivity) + delta * 17, 0, 255);
  } else if (slot.bind == SettingsBind::Rare) {
    state_.rare_chance = clamp_u8(static_cast<int>(state_.rare_chance) + delta * 17, 0, 255);
  } else if (slot.bind == SettingsBind::Fan) {
    set_fan_speed(clamp_u8(static_cast<int>(state_.fan_speed) + delta * 51, 0, 255));
  }
}

void App::nudge_setting(int delta) {
  nudge_focused_slider(delta);
}

void App::activate_setting() {
  if (state_.setting_index == kSettingStatus1 || state_.setting_index == kSettingStatus2) {
    return;
  }
  if (settings_row_ <= 0) {
    return;
  }
  const SettingsPage& page = page_at(settings_page_);
  const SettingsSlot& slot = page.cells[settings_row_ - 1][settings_col_];
  if (!slot_used(slot)) {
    return;
  }
  switch (slot.bind) {
    case SettingsBind::MatrixOn:
      state_.matrix_enabled = !state_.matrix_enabled;
      break;
    case SettingsBind::Led:
      state_.led_enabled = !state_.led_enabled;
      break;
    case SettingsBind::AutoBlink:
      state_.auto_blink = !state_.auto_blink;
      break;
    case SettingsBind::Boop:
      state_.boop_enabled = !state_.boop_enabled;
      if (!state_.boop_enabled) {
        state_.boop = false;
        boop_triggers_ = 0;
      }
      break;
    case SettingsBind::Mouth:
      state_.mouth_enabled = !state_.mouth_enabled;
      if (!state_.mouth_enabled) {
        mouth_level_ = 0;
      }
      break;
    case SettingsBind::BoopCal:
      calibrate_boop();
      break;
    case SettingsBind::MouthCal:
      calibrate_mouth();
      break;
    case SettingsBind::Restart:
      restart();
      break;
    case SettingsBind::Save:
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

  if (in_minigame()) {
    if (mode_ == Mode::Games) {
      handle_stick_games(pad);
      if (pressed(pad.ok(), prev.ok())) {
        if (state_.game_index == 1) {
          enter_casino();
        } else if (state_.game_index == 2) {
          enter_dino();
        } else if (state_.game_index == 3) {
          enter_badapple();
        } else if (state_.game_index == 4) {
          enter_flappy();
        } else if (state_.game_index == 5) {
          enter_tetris();
        } else if (state_.game_index == 6) {
          enter_dvd();
        } else if (state_.game_index == 7) {
          enter_bsod();
        } else {
          enter_snake();
        }
      }
    } else if (mode_ == Mode::Snake) {
      handle_stick_snake(pad);
    } else if (mode_ == Mode::Dino) {
      handle_dino_pad(pad, prev);
    } else if (mode_ == Mode::Flappy) {
      handle_flappy_pad(pad, prev);
    } else if (mode_ == Mode::Casino) {
      handle_casino_pad(pad, prev);
    } else if (mode_ == Mode::BadApple) {
      handle_badapple_pad(pad, prev);
    } else if (mode_ == Mode::Tetris) {
      handle_tetris_pad(pad, prev);
    }
    blink_held_ = false;
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

  if (pressed(pad.esc(), prev.esc())) {
    esc_down_ms_ = clock_.millis();
    esc_long_fired_ = false;
  }
  if (released(pad.esc(), prev.esc())) {
    if (!esc_long_fired_) {
      if (in_arcade()) {
        enter_games();
      } else if (mode_ == Mode::Games || mode_ == Mode::Settings) {
        enter_faceset(state_.faceset);
      }
    }
    esc_down_ms_ = 0;
    esc_long_fired_ = false;
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
  const int dx = static_cast<int>(pad.x) - 128;
  const int dy = static_cast<int>(pad.y) - 128;
  const bool in_x = std::abs(dx) >= kStickDeadzone;
  const bool in_y = std::abs(dy) >= kStickDeadzone;
  if (!in_x) {
    settings_x_latched_ = false;
  }
  if (!in_y) {
    settings_y_latched_ = false;
  }
  if (state_.setting_index == kSettingStatus1 || state_.setting_index == kSettingStatus2) {
    return;
  }
  const bool horiz = in_x && (!in_y || std::abs(dx) >= std::abs(dy));
  const bool vert = in_y && !horiz;
  if (horiz && !settings_x_latched_) {
    settings_x_latched_ = true;
    move_settings_focus(dx > 0 ? 1 : -1, 0);
  }
  if (vert && !settings_y_latched_) {
    settings_y_latched_ = true;
    move_settings_focus(0, dy > 0 ? 1 : -1);
  }
}

void App::handle_stick_games(const PadState& pad) {
  const int dx = static_cast<int>(pad.x) - 128;
  const bool in_x = std::abs(dx) >= kStickDeadzone;
  if (!in_x) {
    games_x_latched_ = false;
    return;
  }
  if (games_x_latched_) {
    return;
  }
  games_x_latched_ = true;
  const int next = state_.game_index + (dx > 0 ? 1 : -1);
  if (next < 0) {
    state_.game_index = kGameCount - 1;
  } else if (next >= kGameCount) {
    state_.game_index = 0;
  } else {
    state_.game_index = next;
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

void App::handle_dino_pad(const PadState& pad, const PadState& prev) {
  const bool jump_btn =
      pad.ok() || pad.y_btn() || pad.up() || pad.y < static_cast<std::uint8_t>(128 - kStickDeadzone);
  const bool duck_btn =
      pad.a() || pad.down() || pad.y > static_cast<std::uint8_t>(128 + kStickDeadzone);
  const bool jump_edge = jump_btn && !dino_.jump_held;
  dino_.jump_held = jump_btn;
  dino_.duck_held = duck_btn;

  if (dino_.play == Dino::Play::Crash) {
    if (jump_edge && clock_.millis() - dino_.crash_ms >= kDinoRestartMs) {
      dino_reset();
      dino_.jump_held = jump_btn;
      dino_start_jump();
    }
    return;
  }

  if (jump_edge && !dino_.ducking) {
    if (dino_.play == Dino::Play::Wait) {
      dino_.play = Dino::Play::Run;
      dino_.speed = kDinoSpeed0;
      dino_.run_ms = 0;
      dino_.anim_ms = clock_.millis();
    }
    dino_start_jump();
  }
  if (!jump_btn) {
    dino_end_jump();
  }

  if (dino_.play != Dino::Play::Run) {
    dino_.ducking = false;
    dino_.speed_drop = false;
    return;
  }

  if (duck_btn) {
    if (dino_.jumping) {
      dino_.speed_drop = true;
      dino_.reached_min = true;
    } else {
      dino_.ducking = true;
    }
  } else {
    dino_.ducking = false;
    dino_.speed_drop = false;
  }
}

void App::dino_reset() {
  const int hi = dino_.hi;
  dino_ = Dino{};
  dino_.hi = hi;
  dino_.y = static_cast<float>(kDinoGroundY);
  dino_.blink_ms = clock_.millis();
  dino_.anim_ms = clock_.millis();
  state_.dino_score = 0;
  dino_spawn_cloud();
}

void App::handle_flappy_pad(const PadState& pad, const PadState& prev) {
  (void)prev;
  const bool flap_btn =
      pad.ok() || pad.y_btn() || pad.up() || pad.y < static_cast<std::uint8_t>(128 - kStickDeadzone);
  const bool flap_edge = flap_btn && !flappy_.flap_held;
  flappy_.flap_held = flap_btn;

  if (flappy_.play == Flappy::Play::Crash) {
    if (flap_edge && clock_.millis() - flappy_.crash_ms >= kFlappyRestartMs) {
      flappy_reset();
      flappy_.flap_held = flap_btn;
      flappy_flap();
    }
    return;
  }

  if (flap_edge) {
    flappy_flap();
  }
}

void App::flappy_reset() {
  const int hi = flappy_.hi;
  flappy_ = Flappy{};
  flappy_.hi = hi;
  flappy_.y = 10.f;
  flappy_.wing = 1;
  flappy_.anim_ms = clock_.millis();
  state_.flappy_score = 0;
  flappy_spawn_cloud();
}

void App::flappy_flap() {
  if (flappy_.play == Flappy::Play::Wait) {
    flappy_.play = Flappy::Play::Run;
    flappy_.speed = kFlappySpeed0;
    flappy_spawn_pipe();
  }
  flappy_.vy = kFlappyFlapV;
  flappy_.wing = 0;
  flappy_.anim_ms = clock_.millis();
}

void App::flappy_crash() {
  if (flappy_.play == Flappy::Play::Crash) {
    return;
  }
  flappy_.play = Flappy::Play::Crash;
  flappy_.crash_ms = clock_.millis();
  flappy_.wing = 2;
  if (flappy_.score > flappy_.hi) {
    flappy_.hi = flappy_.score;
  }
}

void App::flappy_spawn_pipe() {
  Flappy::Pipe pipe;
  pipe.x = static_cast<float>(kMatrixW + 10);
  const int min_y = kFlappyCapH;
  const int max_y = kFlappyGroundY - kFlappyGapH - kFlappyCapH;
  const int span = std::max(1, max_y - min_y + 1);
  pipe.gap_y = min_y + static_cast<int>(rng() % static_cast<std::uint32_t>(span));
  pipe.scored = false;
  flappy_.pipes.push_back(pipe);
}

void App::flappy_spawn_cloud() {
  Flappy::Cloud cloud;
  cloud.x = flappy_.clouds.empty() ? 18.f : static_cast<float>(kMatrixW + 4);
  cloud.y = 1 + static_cast<int>(rng() % 6u);
  flappy_.clouds.push_back(cloud);
}

bool App::flappy_pipe_at(int x, int y) const {
  if (y < 0 || y >= kFlappyGroundY) {
    return false;
  }
  for (const Flappy::Pipe& pipe : flappy_.pipes) {
    const int px = static_cast<int>(pipe.x);
    const int cap_x = px - (kFlappyCapW - kFlappyPipeW) / 2;
    const int gap0 = pipe.gap_y;
    const int gap1 = pipe.gap_y + kFlappyGapH;
    if (y < gap0) {
      if (x >= px && x < px + kFlappyPipeW) {
        return true;
      }
      if (y >= gap0 - kFlappyCapH && x >= cap_x && x < cap_x + kFlappyCapW) {
        return true;
      }
    }
    if (y >= gap1) {
      if (x >= px && x < px + kFlappyPipeW) {
        return true;
      }
      if (y < gap1 + kFlappyCapH && x >= cap_x && x < cap_x + kFlappyCapW) {
        return true;
      }
    }
  }
  return false;
}

bool App::flappy_hit() const {
  const assets::FlappySprite* bird = assets::find_flappy_sprite(flappy_bird_id(false, flappy_.wing));
  if (bird == nullptr) {
    return flappy_.y < 0.f || flappy_.y + 8.f >= static_cast<float>(kFlappyGroundY);
  }
  const int by = static_cast<int>(flappy_.y);
  for (int row = 0; row < bird->height; ++row) {
    for (int col = 0; col < bird->width; ++col) {
      if (!flappy_sprite_lit(bird, col, row)) {
        continue;
      }
      if (col < kFlappyHitInset || row < kFlappyHitInset || col >= bird->width - kFlappyHitInset ||
          row >= bird->height - kFlappyHitInset) {
        continue;
      }
      const int gx = kFlappyX + col;
      const int gy = by + row;
      if (gy < 0 || gy >= kFlappyGroundY || flappy_pipe_at(gx, gy)) {
        return true;
      }
    }
  }
  return false;
}

void App::handle_tetris_pad(const PadState& pad, const PadState& prev) {
  (void)prev;
  const bool rot_btn =
      pad.ok() || pad.y_btn() || pad.up() || pad.y < static_cast<std::uint8_t>(128 - kStickDeadzone);
  const bool left_btn = pad.left() || pad.x < static_cast<std::uint8_t>(128 - kStickDeadzone);
  const bool right_btn = pad.right() || pad.x > static_cast<std::uint8_t>(128 + kStickDeadzone);
  const bool down_btn =
      pad.a() || pad.down() || pad.y > static_cast<std::uint8_t>(128 + kStickDeadzone);
  const bool rot_edge = rot_btn && !tetris_.rot_held;
  tetris_.rot_held = rot_btn;
  tetris_.down_held = down_btn;

  if (tetris_.play == Tetris::Play::Crash) {
    if (rot_edge && clock_.millis() - tetris_.crash_ms >= kTetrisRestartMs) {
      tetris_reset();
      tetris_.rot_held = rot_btn;
    }
    tetris_.left_held = left_btn;
    tetris_.right_held = right_btn;
    return;
  }

  if (tetris_.play == Tetris::Play::Wait) {
    if (rot_edge || left_btn || right_btn || down_btn) {
      tetris_.play = Tetris::Play::Run;
      tetris_.fall_ms = clock_.millis();
    }
    tetris_.left_held = left_btn;
    tetris_.right_held = right_btn;
    return;
  }

  if (rot_edge) {
    const int next = (tetris_.rot + 1) & 3;
    if (tetris_fits(tetris_.x, tetris_.y, tetris_.type, next)) {
      tetris_.rot = next;
    } else if (tetris_fits(tetris_.x - 1, tetris_.y, tetris_.type, next)) {
      --tetris_.x;
      tetris_.rot = next;
    } else if (tetris_fits(tetris_.x + 1, tetris_.y, tetris_.type, next)) {
      ++tetris_.x;
      tetris_.rot = next;
    }
  }

  const bool left_edge = left_btn && !tetris_.left_held;
  const bool right_edge = right_btn && !tetris_.right_held;
  if (left_edge) {
    if (tetris_fits(tetris_.x - 1, tetris_.y, tetris_.type, tetris_.rot)) {
      --tetris_.x;
    }
    tetris_.das_ms = clock_.millis();
  } else if (right_edge) {
    if (tetris_fits(tetris_.x + 1, tetris_.y, tetris_.type, tetris_.rot)) {
      ++tetris_.x;
    }
    tetris_.das_ms = clock_.millis();
  }
  tetris_.left_held = left_btn;
  tetris_.right_held = right_btn;
}

void App::tetris_reset() {
  const int hi = tetris_.hi;
  tetris_ = Tetris{};
  tetris_.hi = hi;
  tetris_refill_bag();
  tetris_.next = tetris_draw_bag();
  tetris_spawn();
  tetris_.play = Tetris::Play::Wait;
  tetris_.fall_ms = clock_.millis();
  state_.tetris_score = 0;
}

void App::tetris_refill_bag() {
  for (int i = 0; i < 7; ++i) {
    tetris_.bag[i] = i + 1;
  }
  for (int i = 6; i > 0; --i) {
    const int j = static_cast<int>(rng() % static_cast<std::uint32_t>(i + 1));
    const int t = tetris_.bag[i];
    tetris_.bag[i] = tetris_.bag[j];
    tetris_.bag[j] = t;
  }
  tetris_.bag_n = 7;
}

int App::tetris_draw_bag() {
  if (tetris_.bag_n <= 0) {
    tetris_refill_bag();
  }
  return tetris_.bag[--tetris_.bag_n];
}

void App::tetris_spawn() {
  tetris_.type = tetris_.next;
  tetris_.next = tetris_draw_bag();
  tetris_.rot = 0;
  tetris_.x = 3;
  tetris_.y = 0;
  if (!tetris_fits(tetris_.x, tetris_.y, tetris_.type, tetris_.rot)) {
    tetris_game_over();
  }
}

bool App::tetris_fits(int x, int y, int type, int rot) const {
  const int t = std::clamp(type, 1, 7) - 1;
  const int r = rot & 3;
  for (int i = 0; i < 4; ++i) {
    const int cx = x + kTetrisOff[t][r][i][0];
    const int cy = y + kTetrisOff[t][r][i][1];
    if (cx < 0 || cx >= kTetrisW || cy >= kTetrisH) {
      return false;
    }
    if (cy < 0) {
      continue;
    }
    if (tetris_.grid[cy * kTetrisW + cx] != 0) {
      return false;
    }
  }
  return true;
}

void App::tetris_lock() {
  const int t = std::clamp(tetris_.type, 1, 7) - 1;
  const int r = tetris_.rot & 3;
  for (int i = 0; i < 4; ++i) {
    const int cx = tetris_.x + kTetrisOff[t][r][i][0];
    const int cy = tetris_.y + kTetrisOff[t][r][i][1];
    if (cx < 0 || cx >= kTetrisW || cy < 0 || cy >= kTetrisH) {
      continue;
    }
    tetris_.grid[cy * kTetrisW + cx] = tetris_.type;
  }
  const int cleared = tetris_clear_lines();
  if (cleared > 0) {
    const int add[5] = {0, 100, 300, 500, 800};
    tetris_.score += add[std::clamp(cleared, 0, 4)] * (tetris_.lines / 10 + 1);
    tetris_.lines += cleared;
    state_.tetris_score = tetris_.score;
    if (tetris_.score > tetris_.hi) {
      tetris_.hi = tetris_.score;
    }
  }
  tetris_spawn();
  tetris_.fall_ms = clock_.millis();
}

int App::tetris_clear_lines() {
  int cleared = 0;
  for (int y = kTetrisH - 1; y >= 0; --y) {
    bool full = true;
    for (int x = 0; x < kTetrisW; ++x) {
      if (tetris_.grid[y * kTetrisW + x] == 0) {
        full = false;
        break;
      }
    }
    if (!full) {
      continue;
    }
    ++cleared;
    for (int row = y; row > 0; --row) {
      for (int x = 0; x < kTetrisW; ++x) {
        tetris_.grid[row * kTetrisW + x] = tetris_.grid[(row - 1) * kTetrisW + x];
      }
    }
    for (int x = 0; x < kTetrisW; ++x) {
      tetris_.grid[x] = 0;
    }
    ++y;
  }
  return cleared;
}

void App::tetris_game_over() {
  if (tetris_.play == Tetris::Play::Crash) {
    return;
  }
  tetris_.play = Tetris::Play::Crash;
  tetris_.crash_ms = clock_.millis();
  if (tetris_.score > tetris_.hi) {
    tetris_.hi = tetris_.score;
  }
}

int App::tetris_ghost_y() const {
  int gy = tetris_.y;
  while (tetris_fits(tetris_.x, gy + 1, tetris_.type, tetris_.rot)) {
    ++gy;
  }
  return gy;
}

std::uint32_t App::tetris_fall_ms() const {
  const int level = tetris_.lines / 10;
  return static_cast<std::uint32_t>(std::max(80, 480 - level * 40));
}

void App::dvd_logo_size(int& w, int& h) const {
  const assets::DvdSprite* sprite = assets::find_dvd_sprite("logo");
  if (sprite == nullptr || sprite->width <= 0 || sprite->height <= 0) {
    w = 24;
    h = 11;
    return;
  }
  w = sprite->width;
  h = sprite->height;
}

void App::dvd_reset() {
  dvd_ = Dvd{};
  dvd_.dx = 1;
  dvd_.dy = 1;
  state_.dvd_hits = 0;
}

int App::bsod_bar_y0() const {
  return kBsodHeadY + kBsodHeadLines * kBsodHeadLineH;
}

int App::bsod_bar_max() const {
  return std::max(0, (kMatrixH - bsod_bar_y0()) / kBsodBarH);
}

void App::bsod_reset() {
  bsod_ = Bsod{};
  bsod_.start_ms = clock_.millis();
  state_.bsod_bars = 0;
}

void App::dino_start_jump() {
  if (dino_.jumping || dino_.ducking) {
    return;
  }
  dino_.jumping = true;
  dino_.reached_min = false;
  dino_.speed_drop = false;
  dino_.vy = kDinoJumpV - dino_.speed * 0.05f;
}

void App::dino_end_jump() {
  if (!dino_.jumping || !dino_.reached_min) {
    return;
  }
  if (dino_.vy < -0.8f) {
    dino_.vy = -0.8f;
  }
}

void App::dino_crash() {
  dino_.play = Dino::Play::Crash;
  dino_.jumping = false;
  dino_.ducking = false;
  dino_.speed_drop = false;
  dino_.crash_ms = clock_.millis();
  if (dino_.score > dino_.hi) {
    dino_.hi = dino_.score;
  }
}

void App::dino_spawn_obstacle() {
  int kind = 0;
  for (int attempt = 0; attempt < 8; ++attempt) {
    kind = static_cast<int>(rng() % 3u);
    if (kind == 2 && dino_.speed < kDinoBirdMinSpeed) {
      kind = static_cast<int>(rng() % 2u);
    }
    if (kind == dino_.last_kind && dino_.same_kind >= 2) {
      continue;
    }
    break;
  }
  if (kind == dino_.last_kind) {
    ++dino_.same_kind;
  } else {
    dino_.last_kind = kind;
    dino_.same_kind = 1;
  }

  Dino::Obstacle ob;
  ob.kind = kind;
  ob.size = 1;
  if (kind != 2) {
    const float need = kind == 1 ? 1.25f : 1.0f;
    if (dino_.speed >= need) {
      ob.size = 1 + static_cast<int>(rng() % 3u);
    }
  }
  const assets::DinoSprite* sprite =
      assets::find_dino_sprite(dino_obstacle_id(ob.kind, ob.size, 0));
  ob.w = sprite != nullptr ? sprite->width : 8;
  ob.h = sprite != nullptr ? sprite->height : 11;
  if (kind == 2) {
    ob.alt = static_cast<int>(rng() % 3u);
    if (ob.alt == 0) {
      ob.y = 1;
    } else if (ob.alt == 1) {
      ob.y = 7;
    } else {
      ob.y = kDinoGroundY + 15 - ob.h;
    }
    ob.speed_off = (rng() & 1u) != 0 ? 0.12f : -0.12f;
  } else {
    ob.y = kDinoGroundY + 15 - ob.h;
  }
  const float min_gap = (kind == 2 ? 50.f : 40.f) * kDinoGapK;
  const float min_g = static_cast<float>(ob.w) * dino_.speed + min_gap;
  ob.gap = min_g + (min_g * 0.5f) * static_cast<float>(rng() % 1000u) / 1000.f;
  ob.x = static_cast<float>(kMatrixW);
  dino_.obstacles.push_back(ob);
}

void App::dino_spawn_cloud() {
  if (dino_.clouds.size() >= 3) {
    return;
  }
  Dino::Cloud cloud;
  cloud.x = static_cast<float>(kMatrixW + static_cast<int>(rng() % 12u));
  cloud.y = static_cast<int>(rng() % 4u);
  dino_.clouds.push_back(cloud);
}

bool App::dino_hit() const {
  const bool crash = dino_.play == Dino::Play::Crash;
  const char* trex = dino_trex_id(crash, dino_.jumping, dino_.ducking && !dino_.jumping,
                                     dino_.play == Dino::Play::Run, dino_.run_frame, dino_.eye_closed);
  const assets::DinoSprite* dino_s = assets::find_dino_sprite(trex);
  if (dino_s == nullptr) {
    return false;
  }
  const int dy = static_cast<int>(dino_.y);
  const int tail = (dino_.ducking && !dino_.jumping) ? kDinoDuckTailW : kDinoTailW;
  for (const Dino::Obstacle& ob : dino_.obstacles) {
    const assets::DinoSprite* ob_s =
        assets::find_dino_sprite(dino_obstacle_id(ob.kind, ob.size, dino_.bird_frame));
    if (ob_s == nullptr) {
      continue;
    }
    const int ox = static_cast<int>(ob.x);
    const int inset = ob.kind == 2 ? kDinoBirdInset : kDinoCactusInset;
    const int x0 = std::max(kDinoX, ox);
    const int y0 = std::max(dy, ob.y);
    const int x1 = std::min(kDinoX + dino_s->width, ox + ob_s->width);
    const int y1 = std::min(dy + dino_s->height, ob.y + ob_s->height);
    for (int y = y0; y < y1; ++y) {
      for (int x = x0; x < x1; ++x) {
        const int ldx = x - kDinoX;
        const int ldy = y - dy;
        const int oxl = x - ox;
        const int oyl = y - ob.y;
        if (ldx < tail) {
          continue;
        }
        if (oxl < inset || oxl >= ob_s->width - inset || oyl < inset) {
          continue;
        }
        if (dino_sprite_lit(dino_s, ldx, ldy) && dino_sprite_lit(ob_s, oxl, oyl)) {
          return true;
        }
      }
    }
  }
  return false;
}

void App::handle_casino_pad(const PadState& pad, const PadState& prev) {
  if (pressed(pad.ok(), prev.ok()) &&
      (casino_.phase == Casino::Phase::Idle || casino_.phase == Casino::Phase::Stop)) {
    casino_.phase = Casino::Phase::Hold;
    for (int i = 0; i < 3; ++i) {
      casino_.rolling[i] = true;
      casino_.speed[i] = 4 + i;
    }
  }
  if (released(pad.ok(), prev.ok()) && casino_.phase == Casino::Phase::Hold) {
    roll_casino();
    const std::uint32_t now = clock_.millis();
    casino_.phase = Casino::Phase::Coast;
    casino_.stop_ms[0] = now + 320;
    casino_.stop_ms[1] = now + 640;
    casino_.stop_ms[2] = now + 980;
  }
}

void App::roll_casino() {
  auto other_than = [this](int skip) {
    int value = static_cast<int>(rng() % static_cast<std::uint32_t>(kSlotCount - 1));
    if (value >= skip) {
      ++value;
    }
    return value;
  };
  const int roll = static_cast<int>(rng() % 100u);
  if (roll < 5) {
    casino_.result[0] = 0;
    casino_.result[1] = 0;
    casino_.result[2] = 0;
  } else if (roll < 22) {
    const int sym = 1 + static_cast<int>(rng() % static_cast<std::uint32_t>(kSlotCount - 1));
    casino_.result[0] = sym;
    casino_.result[1] = sym;
    casino_.result[2] = sym;
  } else if (roll < 72) {
    const int a = static_cast<int>(rng() % static_cast<std::uint32_t>(kSlotCount));
    casino_.result[0] = a;
    casino_.result[1] = a;
    casino_.result[2] = other_than(a);
  } else if (roll < 82) {
    const int a = static_cast<int>(rng() % static_cast<std::uint32_t>(kSlotCount));
    const int b = other_than(a);
    if ((rng() & 1u) != 0) {
      casino_.result[0] = b;
      casino_.result[1] = a;
      casino_.result[2] = a;
    } else {
      casino_.result[0] = a;
      casino_.result[1] = b;
      casino_.result[2] = a;
    }
  } else {
    const int a = static_cast<int>(rng() % static_cast<std::uint32_t>(kSlotCount));
    int b = other_than(a);
    int c = other_than(a);
    if (c == b) {
      c = other_than(b);
    }
    casino_.result[0] = a;
    casino_.result[1] = b;
    casino_.result[2] = c;
  }
}

void App::update_casino(std::uint32_t now_ms) {
  if (mode_ != Mode::Casino) {
    return;
  }
  if (casino_.phase != Casino::Phase::Hold && casino_.phase != Casino::Phase::Coast) {
    return;
  }
  const int span = kSlotCount * kSlotH;
  for (int i = 0; i < 3; ++i) {
    if (!casino_.rolling[i]) {
      continue;
    }
    if (casino_.phase == Casino::Phase::Coast && now_ms >= casino_.stop_ms[i]) {
      casino_.rolling[i] = false;
      casino_.offset[i] = casino_.result[i] * kSlotH;
      continue;
    }
    casino_.offset[i] = (casino_.offset[i] + casino_.speed[i]) % span;
  }
  if (casino_.phase == Casino::Phase::Coast && !casino_.rolling[0] && !casino_.rolling[1] &&
      !casino_.rolling[2]) {
    casino_.phase = Casino::Phase::Stop;
  }
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
  mic_pcm_count_ = sensors_.copy_microphone_pcm(mic_pcm_, kMicPcmSize);
  if (mic_pcm_count_ > 0) {
    state_.mic = pcm_level(mic_pcm_, mic_pcm_count_);
  } else {
    state_.mic = std::clamp(sensors_.microphone(), 0.0f, 1.0f);
  }
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
      mouth_peak_ = 0.18f;
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

  if (prev_boop_ && !state_.boop && !in_arcade() &&
      mode_ != Mode::Startup) {
    snapshot(trans_from_, matrix_fb_);
    trans_kind_ = face::TransitionKind::Glitch;
    trans_start_ms_ = clock_.millis();
    trans_duration_ms_ = face::transition_duration_ms(trans_kind_);
    state_.transition = face::transition_name(trans_kind_);
  }
  prev_boop_ = state_.boop;

  if (state_.mouth_enabled) {
    const float dead = mouth_deadzone_of(state_.mouth_sensitivity);
    const float gain = mouth_gain_of(state_.mouth_sensitivity);
    const float reading =
        std::max(0.0f, std::fabs(state_.mic - mouth_baseline_) - dead) * gain;
    mouth_level_ = std::max(reading, mouth_level_ * 0.60f);
    if (mouth_level_ > mouth_peak_) {
      mouth_peak_ = mouth_level_;
    } else {
      mouth_peak_ = std::max(0.18f, mouth_peak_ - 0.02f);
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

  if (in_arcade()) {
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
  if (in_arcade() || mode_ == Mode::Startup || !blink_allowed()) {
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
    for (int n = 0; n < 128; ++n) {
      snake_.fruit_x = static_cast<int>(rng() % static_cast<std::uint32_t>(kSnakeW));
      snake_.fruit_y = static_cast<int>(rng() % static_cast<std::uint32_t>(kSnakeH));
      bool hit = false;
      for (const auto& part : snake_.body) {
        if (part.first == snake_.fruit_x && part.second == snake_.fruit_y) {
          hit = true;
          break;
        }
      }
      if (!hit) {
        break;
      }
    }
  }
  if (snake_.grow > 0) {
    --snake_.grow;
  } else if (snake_.body.size() > 1) {
    snake_.body.pop_back();
  }
  snake_.last_dir = snake_.dir;
}

void App::update_dino(std::uint32_t now_ms) {
  if (mode_ != Mode::Dino) {
    return;
  }
  const float frames = static_cast<float>(kTickMs) / kDinoFrameMs;

  if (now_ms - dino_.anim_ms >= kDinoRunAnimMs) {
    dino_.anim_ms = now_ms;
    dino_.run_frame ^= 1;
  }
  if (now_ms - dino_.blink_ms >= (dino_.eye_closed ? kDinoBlinkMs : kDinoBlinkEveryMs)) {
    dino_.blink_ms = now_ms;
    dino_.eye_closed = !dino_.eye_closed;
  }

  if (dino_.play == Dino::Play::Wait) {
    if (dino_.clouds.empty()) {
      dino_spawn_cloud();
    }
    return;
  }

  if (dino_.play == Dino::Play::Crash) {
    return;
  }

  dino_.run_ms += kTickMs;
  if (dino_.speed < kDinoMaxSpeed) {
    dino_.speed = std::min(kDinoMaxSpeed, dino_.speed + kDinoAccel * frames);
  }
  dino_.distance += dino_.speed * frames;
  dino_.score = static_cast<int>(dino_.distance * kDinoScoreK);
  state_.dino_score = dino_.score;
  dino_.horizon += dino_.speed * frames;
  while (dino_.horizon >= static_cast<float>(kDinoHorizonW)) {
    dino_.horizon -= static_cast<float>(kDinoHorizonW);
  }

  if (dino_.jumping) {
    const float g = kDinoGravity * (dino_.speed_drop ? kDinoSpeedDrop : 1.f);
    dino_.y += dino_.vy * frames;
    dino_.vy += g * frames;
    if (kDinoGroundY - static_cast<int>(dino_.y) >= kDinoMinJump || dino_.speed_drop) {
      dino_.reached_min = true;
    }
    if (dino_.y < static_cast<float>(kDinoJumpCeil)) {
      dino_.y = static_cast<float>(kDinoJumpCeil);
    }
    if (dino_.y >= static_cast<float>(kDinoGroundY)) {
      dino_.y = static_cast<float>(kDinoGroundY);
      dino_.vy = 0.f;
      dino_.jumping = false;
      dino_.speed_drop = false;
      dino_.reached_min = false;
    }
  }
  if (dino_.play == Dino::Play::Run && !dino_.jumping) {
    dino_.ducking = dino_.duck_held;
  }

  const float bird_step = dino_.speed * frames;
  for (Dino::Cloud& cloud : dino_.clouds) {
    cloud.x -= bird_step * kDinoCloudSpeed;
  }
  dino_.clouds.erase(std::remove_if(dino_.clouds.begin(), dino_.clouds.end(),
                                    [](const Dino::Cloud& c) { return c.x < -24.f; }),
                     dino_.clouds.end());
  if (dino_.clouds.empty() || dino_.clouds.back().x < static_cast<float>(kMatrixW) - 36.f) {
    if ((rng() % 100u) < 8u) {
      dino_spawn_cloud();
    }
  }

  dino_.bird_frame = static_cast<int>((now_ms / kDinoBirdAnimMs) & 1u);

  for (Dino::Obstacle& ob : dino_.obstacles) {
    float step = dino_.speed * frames;
    if (ob.kind == 2) {
      step += ob.speed_off * frames * 0.25f;
    }
    ob.x -= step;
  }
  dino_.obstacles.erase(std::remove_if(dino_.obstacles.begin(), dino_.obstacles.end(),
                                      [](const Dino::Obstacle& o) { return o.x + o.w < -2.f; }),
                        dino_.obstacles.end());

  if (dino_.run_ms >= kDinoClearMs) {
    if (dino_.obstacles.empty()) {
      dino_spawn_obstacle();
    } else {
      const Dino::Obstacle& last = dino_.obstacles.back();
      if (last.x + static_cast<float>(last.w) + last.gap < static_cast<float>(kMatrixW)) {
        dino_spawn_obstacle();
      }
    }
  }

  if (dino_hit()) {
    dino_crash();
  }
}

void App::handle_badapple_pad(const PadState& pad, const PadState& prev) {
  if (pressed(pad.ok(), prev.ok())) {
    badapple_fast_ = !badapple_fast_;
  }
}

void App::update_badapple(std::uint32_t now_ms) {
  (void)now_ms;
  if (mode_ != Mode::BadApple) {
    return;
  }
  if (badapple_.base == nullptr) {
    return;
  }
  const std::uint32_t num = badapple_fast_ ? kBadAppleSpeedNum : 1;
  const std::uint32_t den = badapple_fast_ ? kBadAppleSpeedDen : 1;
  badapple_accum_ms_ += (kTickMs * num) / den;
  while (badapple_accum_ms_ >= kBadAppleFrameMs) {
    badapple_accum_ms_ -= kBadAppleFrameMs;
    if (!assets::ba_next(badapple_)) {
      assets::ba_rewind(badapple_);
      assets::ba_next(badapple_);
    }
  }
}

void App::update_flappy(std::uint32_t now_ms) {
  if (mode_ != Mode::Flappy) {
    return;
  }

  if (now_ms - flappy_.anim_ms >= kFlappyAnimMs) {
    flappy_.anim_ms = now_ms;
    if (flappy_.play != Flappy::Play::Crash) {
      flappy_.wing = (flappy_.wing + 1) % 3;
    }
  }

  if (flappy_.play == Flappy::Play::Wait) {
    const float phase = static_cast<float>((now_ms / kFlappyBobMs) % 6u);
    flappy_.y = 10.f + ((phase < 3.f) ? phase * 0.5f : (5.f - phase) * 0.5f);
    if (flappy_.clouds.empty()) {
      flappy_spawn_cloud();
    }
    return;
  }

  if (flappy_.play == Flappy::Play::Crash) {
    return;
  }

  if (flappy_.speed < kFlappyMaxSpeed) {
    flappy_.speed = std::min(kFlappyMaxSpeed, flappy_.speed + kFlappyAccel);
  }
  flappy_.vy = std::min(kFlappyMaxFall, flappy_.vy + kFlappyGravity);
  flappy_.y += flappy_.vy;
  flappy_.ground += flappy_.speed;
  while (flappy_.ground >= 16.f) {
    flappy_.ground -= 16.f;
  }

  for (Flappy::Cloud& cloud : flappy_.clouds) {
    cloud.x -= flappy_.speed * kFlappyCloudSpeed;
  }
  flappy_.clouds.erase(std::remove_if(flappy_.clouds.begin(), flappy_.clouds.end(),
                                      [](const Flappy::Cloud& c) { return c.x < -18.f; }),
                       flappy_.clouds.end());
  if (flappy_.clouds.empty() || flappy_.clouds.back().x < static_cast<float>(kMatrixW) - 28.f) {
    if ((rng() % 100u) < 10u) {
      flappy_spawn_cloud();
    }
  }

  for (Flappy::Pipe& pipe : flappy_.pipes) {
    pipe.x -= flappy_.speed;
    if (!pipe.scored && pipe.x + static_cast<float>(kFlappyPipeW) < static_cast<float>(kFlappyX)) {
      pipe.scored = true;
      ++flappy_.score;
      state_.flappy_score = flappy_.score;
    }
  }
  flappy_.pipes.erase(std::remove_if(flappy_.pipes.begin(), flappy_.pipes.end(),
                                      [](const Flappy::Pipe& p) { return p.x < -16.f; }),
                      flappy_.pipes.end());
  if (flappy_.pipes.empty()) {
    flappy_spawn_pipe();
  } else if (flappy_.pipes.back().x < static_cast<float>(kMatrixW) - kFlappySpacing) {
    flappy_spawn_pipe();
  }

  if (flappy_hit()) {
    flappy_crash();
  }
}

void App::update_tetris(std::uint32_t now_ms) {
  if (mode_ != Mode::Tetris) {
    return;
  }
  if (tetris_.play != Tetris::Play::Run) {
    return;
  }

  if (tetris_.left_held || tetris_.right_held) {
    if (now_ms - tetris_.das_ms >= kTetrisDasMs) {
      while (tetris_.das_ms + kTetrisArrMs <= now_ms) {
        tetris_.das_ms += kTetrisArrMs;
        const int dir = tetris_.left_held ? -1 : 1;
        if (tetris_fits(tetris_.x + dir, tetris_.y, tetris_.type, tetris_.rot)) {
          tetris_.x += dir;
        }
      }
    }
  }

  const std::uint32_t grav = tetris_.down_held ? kTickMs : tetris_fall_ms();
  while (now_ms - tetris_.fall_ms >= grav) {
    tetris_.fall_ms += grav;
    if (tetris_fits(tetris_.x, tetris_.y + 1, tetris_.type, tetris_.rot)) {
      ++tetris_.y;
      if (tetris_.down_held) {
        ++tetris_.score;
        state_.tetris_score = tetris_.score;
      }
    } else {
      tetris_lock();
      if (tetris_.play != Tetris::Play::Run) {
        break;
      }
    }
  }
}

void App::update_dvd(std::uint32_t now_ms) {
  if (mode_ != Mode::Dvd) {
    return;
  }
  int w = 24;
  int h = 11;
  dvd_logo_size(w, h);
  dvd_.x += dvd_.dx;
  dvd_.y += dvd_.dy;
  bool hit_x = false;
  bool hit_y = false;
  if (dvd_.x <= 0) {
    dvd_.x = 0;
    dvd_.dx = 1;
    hit_x = true;
  } else if (dvd_.x + w >= kMatrixW) {
    dvd_.x = kMatrixW - w;
    dvd_.dx = -1;
    hit_x = true;
  }
  if (dvd_.y <= 0) {
    dvd_.y = 0;
    dvd_.dy = 1;
    hit_y = true;
  } else if (dvd_.y + h >= kMatrixH) {
    dvd_.y = kMatrixH - h;
    dvd_.dy = -1;
    hit_y = true;
  }
  if (!hit_x && !hit_y) {
    return;
  }
  dvd_.hue = static_cast<std::uint8_t>(dvd_.hue + kDvdHueStep);
  ++dvd_.hits;
  state_.dvd_hits = dvd_.hits;
  if (hit_x && hit_y) {
    ++dvd_.corners;
    dvd_.corner_ms = now_ms;
  }
}

void App::update_bsod(std::uint32_t now_ms) {
  if (mode_ != Mode::Bsod) {
    return;
  }
  const int max_bars = bsod_bar_max();
  if (max_bars <= 0) {
    bsod_.bars = 0;
    state_.bsod_bars = 0;
    return;
  }
  const std::uint32_t fill_ms = static_cast<std::uint32_t>(max_bars) * kBsodBarMs;
  const std::uint32_t elapsed = now_ms - bsod_.start_ms;
  if (elapsed >= fill_ms + kBsodHoldMs) {
    bsod_.start_ms = now_ms;
    bsod_.bars = 0;
  } else {
    bsod_.bars = static_cast<int>(std::min(static_cast<std::uint32_t>(max_bars), elapsed / kBsodBarMs));
  }
  state_.bsod_bars = bsod_.bars;
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
  matrix_fb_.draw_rect(kSnakeOx - kSnakeBorder, kSnakeOy - kSnakeBorder,
                       kSnakeW * kSnakeCell + 2 * kSnakeBorder, kSnakeH * kSnakeCell + 2 * kSnakeBorder, border);
  for (const auto& part : snake_.body) {
    matrix_fb_.fill_rect(kSnakeOx + part.first * kSnakeCell, kSnakeOy + part.second * kSnakeCell, kSnakeCell,
                         kSnakeCell, body);
  }
  if (snake_.fruit_on || snake_.play != Snake::Play::Run) {
    matrix_fb_.fill_rect(kSnakeOx + snake_.fruit_x * kSnakeCell, kSnakeOy + snake_.fruit_y * kSnakeCell, kSnakeCell,
                         kSnakeCell, fruit);
  }
}

void App::render_dino(std::uint32_t now_ms) {
  (void)now_ms;
  matrix_fb_.clear(Color::black());
  const assets::DinoSprite* hz = assets::find_dino_sprite("horizon");
  const int hw = hz != nullptr ? hz->width : kDinoHorizonW;
  float hx_off = dino_.horizon;
  while (hx_off >= static_cast<float>(hw) && hw > 0) {
    hx_off -= static_cast<float>(hw);
  }
  const int hx = -static_cast<int>(hx_off);
  blit_dino_sprite(matrix_fb_, "horizon", hx, kDinoHorizonY);
  blit_dino_sprite(matrix_fb_, "horizon", hx + hw, kDinoHorizonY);
  for (const Dino::Cloud& cloud : dino_.clouds) {
    blit_dino_sprite(matrix_fb_, "cloud", static_cast<int>(cloud.x), cloud.y);
  }
  for (const Dino::Obstacle& ob : dino_.obstacles) {
    blit_dino_sprite(matrix_fb_, dino_obstacle_id(ob.kind, ob.size, dino_.bird_frame),
                     static_cast<int>(ob.x), ob.y);
  }
  const int dy = static_cast<int>(dino_.y);
  blit_dino_sprite(matrix_fb_,
                   dino_trex_id(dino_.play == Dino::Play::Crash, dino_.jumping, dino_.ducking,
                                dino_.play == Dino::Play::Run, dino_.run_frame, dino_.eye_closed),
                   kDinoX, dy);
  blit_arcade_status(matrix_fb_, dino_.score, dino_.play == Dino::Play::Wait,
                     dino_.play == Dino::Play::Crash);
}

void App::render_badapple() {
  matrix_fb_.clear(Color::black());
  const Color on = Color::white();
  for (int y = 0; y < assets::kBaHeight; ++y) {
    for (int x = 0; x < assets::kBaWidth; ++x) {
      if (assets::ba_pixel(badapple_, x, y) != 0) {
        matrix_fb_.set_pixel(x, y, on);
      }
    }
  }
}

void App::render_flappy(std::uint32_t now_ms) {
  (void)now_ms;
  matrix_fb_.clear(Color{10, 24, 48});
  for (const Flappy::Cloud& cloud : flappy_.clouds) {
    blit_flappy_sprite(matrix_fb_, "cloud", static_cast<int>(cloud.x), cloud.y);
  }
  for (const Flappy::Pipe& pipe : flappy_.pipes) {
    blit_flappy_pipe(matrix_fb_, static_cast<int>(pipe.x), pipe.gap_y, kFlappyGapH, kFlappyGroundY);
  }
  const int gx = -static_cast<int>(flappy_.ground);
  blit_flappy_sprite(matrix_fb_, "ground", gx, kFlappyGroundY);
  blit_flappy_sprite(matrix_fb_, "ground", gx + 16, kFlappyGroundY);
  blit_flappy_sprite(matrix_fb_, "ground", gx + 32, kFlappyGroundY);
  blit_flappy_sprite(matrix_fb_, "ground", gx + 48, kFlappyGroundY);
  blit_flappy_sprite(matrix_fb_, "ground", gx + 64, kFlappyGroundY);
  blit_flappy_sprite(matrix_fb_, flappy_bird_id(flappy_.play == Flappy::Play::Crash, flappy_.wing),
                     kFlappyX, static_cast<int>(flappy_.y));
  blit_arcade_status(matrix_fb_, flappy_.score, flappy_.play == Flappy::Play::Wait,
                     flappy_.play == Flappy::Play::Crash);
}

void App::render_tetris(std::uint32_t now_ms) {
  (void)now_ms;
  matrix_fb_.clear(Color{6, 10, 18});
  const int well_h = kTetrisH * kTetrisCell;
  blit_tetris_wall_col(matrix_fb_, kTetrisOx - 1, kTetrisOy, well_h);
  blit_tetris_wall_col(matrix_fb_, kTetrisOx + kTetrisW * kTetrisCell, kTetrisOy, well_h);
  for (int y = 0; y < kTetrisH; ++y) {
    for (int x = 0; x < kTetrisW; ++x) {
      const int cell = tetris_.grid[y * kTetrisW + x];
      if (cell == 0) {
        continue;
      }
      blit_tetris_cell(matrix_fb_, cell, kTetrisOx + x * kTetrisCell, kTetrisOy + y * kTetrisCell,
                       kTetrisCell);
    }
  }
  if (tetris_.play != Tetris::Play::Crash) {
    const int gy = tetris_ghost_y();
    if (gy != tetris_.y) {
      blit_tetris_piece(matrix_fb_, tetris_.x, gy, tetris_.type, tetris_.rot, kTetrisOx, kTetrisOy,
                        kTetrisCell, true);
    }
  }
  blit_tetris_piece(matrix_fb_, tetris_.x, tetris_.y, tetris_.type, tetris_.rot, kTetrisOx, kTetrisOy,
                    kTetrisCell, false);
  blit_tetris_piece(matrix_fb_, 0, 0, tetris_.next, 0, kTetrisNextX, kTetrisNextY, kTetrisNextCell, false);
  blit_arcade_status(matrix_fb_, tetris_.score, tetris_.play == Tetris::Play::Wait,
                     tetris_.play == Tetris::Play::Crash);
}

void App::render_dvd(std::uint32_t now_ms) {
  (void)now_ms;
  matrix_fb_.clear(Color::black());
  blit_dvd_logo(matrix_fb_, dvd_.x, dvd_.y, hue_rgb(dvd_.hue));
}

void App::render_bsod(std::uint32_t now_ms) {
  (void)now_ms;
  matrix_fb_.clear(kBsodBlue);
  matrix_fb_.draw_text(kBsodHeadX, kBsodHeadY, kBsodHead0, Color::white());
  matrix_fb_.draw_text(kBsodHeadX, kBsodHeadY + kBsodHeadLineH, kBsodHead1, Color::white());
  const int y0 = bsod_bar_y0();
  for (int i = 0; i < bsod_.bars; ++i) {
    blit_bsod_sprite(matrix_fb_, "bar", 0, y0 + i * kBsodBarH);
  }
}

void App::render_casino(std::uint32_t now_ms) {
  matrix_fb_.clear(Color{12, 8, 24});
  const int y0 = kReelY;
  const int y1 = kReelY + kSlotH;
  const int span = kSlotCount * kSlotH;
  for (int i = 0; i < 3; ++i) {
    const int x = kReelX[i];
    matrix_fb_.fill_rect(x - 1, y0 - 1, kSlotH + 2, kSlotH + 2, Color{40, 20, 50});
    const int off = ((casino_.offset[i] % span) + span) % span;
    const int pixel = off % kSlotH;
    const int top = off / kSlotH;
    const int next = (top + 1) % kSlotCount;
    blit_casino_tile(matrix_fb_, x, y0 - pixel, top, y0, y1);
    blit_casino_tile(matrix_fb_, x, y0 - pixel + kSlotH, next, y0, y1);
    matrix_fb_.draw_rect(x - 1, y0 - 1, kSlotH + 2, kSlotH + 2, Color{255, 200, 70});
  }
  if (casino_.phase == Casino::Phase::Stop && casino_.result[0] == casino_.result[1] &&
      casino_.result[1] == casino_.result[2]) {
    const bool flash = ((now_ms / 120) % 2) == 0;
    if (flash) {
      if (casino_.result[0] == 0) {
        matrix_fb_.draw_text(20, 24, "777", Color{255, 40, 40});
      } else {
        matrix_fb_.draw_text(20, 24, "WIN", Color{255, 220, 80});
      }
    }
  } else if (casino_.phase == Casino::Phase::Idle) {
    matrix_fb_.draw_text(8, 24, "HOLD", Color{180, 180, 200});
  }
}

void App::render_oled_games() {
  oled_fb_.fill_rect(0, 0, kOledW, kOledYellowH, true);
  const int game = std::clamp(state_.game_index, 0, kGameCount - 1);
  const char* title = kGameTitle[game];
  const int tw = text_width(title);
  oled_fb_.draw_text((kOledW - tw) / 2, 4, title, false);
  draw_oled_chevron(oled_fb_, 6, 2, -1, false);
  draw_oled_chevron(oled_fb_, kOledW - 12, 2, 1, false);
  const int ix = (kOledW - kGameIcon) / 2;
  const int iy = kOledYellowH + (kOledH - kOledYellowH - kGameIcon) / 2;
  draw_icon(ix, iy, kGameIconId[game], true);
}

void App::render_oled_snake() {
  char score[12];
  const char* head = "DEAD";
  if (snake_.play != Snake::Play::Stop) {
    std::snprintf(score, sizeof(score), "%u", static_cast<unsigned>(snake_.score));
    head = score;
  }
  const int hw = text_width(head) * 2;
  oled_fb_.draw_text((kOledW - hw) / 2, 1, head, true, 2);
  oled_fb_.draw_rect(0, kSnakeHudH, kOledW, kOledH - kSnakeHudH, true);
  for (const auto& part : snake_.body) {
    oled_fb_.fill_rect(kOledSnakeX + part.first * kSnakeCell, kOledSnakeY + part.second * kSnakeCell, kSnakeCell,
                      kSnakeCell, true);
  }
  if (snake_.fruit_on || snake_.play != Snake::Play::Run) {
    oled_fb_.fill_rect(kOledSnakeX + snake_.fruit_x * kSnakeCell, kOledSnakeY + snake_.fruit_y * kSnakeCell,
                      kSnakeCell, kSnakeCell, true);
  }
}

void App::render_oled_dino() {
  char score[12];
  const char* head = "DEAD";
  if (dino_.play != Dino::Play::Crash) {
    std::snprintf(score, sizeof(score), "%05u", static_cast<unsigned>(std::max(0, dino_.score)));
    head = score;
  }
  const int hw = text_width(head) * 2;
  oled_fb_.draw_text((kOledW - hw) / 2, 1, head, true, 2);
  if (dino_.hi > 0) {
    char hi[16];
    std::snprintf(hi, sizeof(hi), "HI %05u", static_cast<unsigned>(dino_.hi));
    const int hiw = text_width(hi);
    oled_fb_.draw_text((kOledW - hiw) / 2, 22, hi, true);
  } else if (dino_.play == Dino::Play::Wait) {
    const int lw = text_width("JUMP");
    oled_fb_.draw_text((kOledW - lw) / 2, 28, "JUMP", true);
  }
}

void App::render_oled_badapple() {
  oled_fb_.fill_rect(0, 0, kOledW, kOledYellowH, true);
  const int title_w = text_width("APPLE");
  oled_fb_.draw_text((kOledW - title_w) / 2, 4, "APPLE", false);
  if (badapple_.fps == 0) {
    const int lw = text_width("NO DATA");
    oled_fb_.draw_text((kOledW - lw) / 2, 28, "NO DATA", true);
    return;
  }
  const int sec = static_cast<int>(badapple_.frame_index) / static_cast<int>(badapple_.fps);
  char line[16];
  std::snprintf(line, sizeof(line), "%d:%02d", sec / 60, sec % 60);
  const int lw = text_width(line);
  oled_fb_.draw_text((kOledW - lw) / 2, 28, line, true);
  if (badapple_fast_) {
    const int sw = text_width("2.5X");
    oled_fb_.draw_text((kOledW - sw) / 2, 42, "2.5X", true);
  }
}

void App::render_oled_flappy() {
  char score[12];
  const char* head = "DEAD";
  if (flappy_.play != Flappy::Play::Crash) {
    std::snprintf(score, sizeof(score), "%05u", static_cast<unsigned>(std::max(0, flappy_.score)));
    head = score;
  }
  const int hw = text_width(head) * 2;
  oled_fb_.draw_text((kOledW - hw) / 2, 1, head, true, 2);
  if (flappy_.hi > 0) {
    char hi[16];
    std::snprintf(hi, sizeof(hi), "HI %05u", static_cast<unsigned>(flappy_.hi));
    const int hiw = text_width(hi);
    oled_fb_.draw_text((kOledW - hiw) / 2, 22, hi, true);
  } else if (flappy_.play == Flappy::Play::Wait) {
    const int lw = text_width("FLAP");
    oled_fb_.draw_text((kOledW - lw) / 2, 28, "FLAP", true);
  }
}

void App::render_oled_tetris() {
  char score[12];
  const char* head = "DEAD";
  if (tetris_.play != Tetris::Play::Crash) {
    std::snprintf(score, sizeof(score), "%05u", static_cast<unsigned>(std::max(0, tetris_.score)));
    head = score;
  }
  const int hw = text_width(head) * 2;
  oled_fb_.draw_text((kOledW - hw) / 2, 1, head, true, 2);
  if (tetris_.hi > 0) {
    char hi[16];
    std::snprintf(hi, sizeof(hi), "HI %05u", static_cast<unsigned>(tetris_.hi));
    const int hiw = text_width(hi);
    oled_fb_.draw_text((kOledW - hiw) / 2, 22, hi, true);
  } else if (tetris_.play == Tetris::Play::Wait) {
    const int lw = text_width("PLAY");
    oled_fb_.draw_text((kOledW - lw) / 2, 28, "PLAY", true);
  }
}

void App::render_oled_dvd() {
  oled_fb_.fill_rect(0, 0, kOledW, kOledYellowH, true);
  const int title_w = text_width("DVD");
  oled_fb_.draw_text((kOledW - title_w) / 2, 4, "DVD", false);
  char hits[12];
  std::snprintf(hits, sizeof(hits), "%05u", static_cast<unsigned>(std::max(0, dvd_.hits)));
  const int hw = text_width(hits) * 2;
  oled_fb_.draw_text((kOledW - hw) / 2, 22, hits, true, 2);
  if (dvd_.corners > 0 && clock_.millis() - dvd_.corner_ms < kDvdCornerMs) {
    const int cw = text_width("CORNER");
    oled_fb_.draw_text((kOledW - cw) / 2, 42, "CORNER", true);
  }
}

void App::render_oled_bsod() {
  oled_fb_.fill_rect(0, 0, kOledW, kOledYellowH, true);
  const int title_w = text_width("BSOD");
  oled_fb_.draw_text((kOledW - title_w) / 2, 4, "BSOD", false);
  const char* line1 = "Your PC ran into";
  const char* line2 = "a problem";
  oled_fb_.draw_text((kOledW - text_width(line1)) / 2, 22, line1, true);
  oled_fb_.draw_text((kOledW - text_width(line2)) / 2, 32, line2, true);
  const int max_bars = std::max(1, bsod_bar_max());
  const int pct = (bsod_.bars * 100) / max_bars;
  char line[12];
  std::snprintf(line, sizeof(line), "%d%%", pct);
  const int pw = text_width(line);
  oled_fb_.draw_text((kOledW - pw) / 2, 46, line, true);
}

void App::render_oled_casino() {
  oled_fb_.fill_rect(0, 0, kOledW, kOledYellowH, true);
  const int title_w = text_width("CASINO");
  oled_fb_.draw_text((kOledW - title_w) / 2, 4, "CASINO", false);
  const char* line = "HOLD BLINK";
  if (casino_.phase == Casino::Phase::Hold) {
    line = "SPIN";
  } else if (casino_.phase == Casino::Phase::Coast) {
    line = "STOP";
  } else if (casino_.phase == Casino::Phase::Stop) {
    if (casino_.result[0] == 0 && casino_.result[1] == 0 && casino_.result[2] == 0) {
      line = "JACKPOT 777";
    } else if (casino_.result[0] == casino_.result[1] && casino_.result[1] == casino_.result[2]) {
      line = "THREE";
    } else if (casino_.result[0] == casino_.result[1] || casino_.result[1] == casino_.result[2] ||
               casino_.result[0] == casino_.result[2]) {
      line = "PAIR";
    } else {
      line = "MISS";
    }
  }
  const int lw = text_width(line);
  oled_fb_.draw_text((kOledW - lw) / 2, 28, line, true);
  oled_fb_.draw_text(28, 50, "ESC BACK", true);
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
    apply_boop_hue(now_ms);
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
  if (!state_.boop_enabled || in_arcade() || mode_ == Mode::Startup) {
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

void App::apply_boop_hue(std::uint32_t now_ms) {
  const std::uint32_t elapsed = now_ms - boop_started_ms_;
  if (elapsed < kBoopHueHoldMs) {
    return;
  }
  const std::uint32_t into = elapsed - kBoopHueHoldMs;
  std::uint16_t mix = 256;
  if (into < kBoopHueRampMs) {
    mix = static_cast<std::uint16_t>((into * 256u) / kBoopHueRampMs);
  }
  if (mix == 0) {
    return;
  }
  const std::uint8_t phase =
      static_cast<std::uint8_t>((now_ms / (kBoopHuePeriodMs / 256u)) & 0xFFu);
  matrix_fb_.hue_cycle_lit(phase, mix);
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
    case assets::Effect::Glitch:
      apply_glitch_motion(now_ms);
      break;
    default:
      break;
  }
}

void App::apply_glitch_motion(std::uint32_t now_ms) {
  const std::uint32_t span = kGlitchBurstMs + kGlitchPauseMs;
  const std::uint32_t t = (now_ms - effect_started_ms_) % span;
  if (t < kGlitchPauseMs) {
    return;
  }
  const std::uint32_t into = t - kGlitchPauseMs;
  const float fall = 1.0f - static_cast<float>(into) / static_cast<float>(kGlitchBurstMs);
  const int amp = std::max(1, static_cast<int>(fall * 3.0f));
  matrix_fb_.glitch_rows(0, matrix_fb_.height(), amp, rng_state_ + into);
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

void App::draw_icon(int x, int y, const char* id, bool on) {
  if (id == nullptr) {
    return;
  }
  const assets::Bitmap* bmp = assets::find_icon(id);
  if (bmp == nullptr) {
    return;
  }
  oled_fb_.blit_bitmap_1bpp(x, y, bmp->width, bmp->height, bmp->data, bmp->size, bmp->width, bmp->height, on);
}

void App::draw_toggle(int x, int y, bool on) {
  static const char* kOff[kToggleH] = {
      "XXXXXXXXXXXXXX", "X............X", "X.XXXXX......X", "X.XXXXX......X",
      "X.XXXXX......X", "X............X", "XXXXXXXXXXXXXX",
  };
  static const char* kOn[kToggleH] = {
      "XXXXXXXXXXXXXX", "X............X", "X......XXXXX.X", "X......XXXXX.X",
      "X......XXXXX.X", "X............X", "XXXXXXXXXXXXXX",
  };
  const char** rows = on ? kOn : kOff;
  for (int row = 0; row < kToggleH; ++row) {
    for (int col = 0; col < kToggleW; ++col) {
      oled_fb_.set_pixel(x + col, y + row, rows[row][col] == 'X');
    }
  }
}

void App::draw_slider(int x, int y, int w, int raw, int max_value) {
  if (w < 8) {
    return;
  }
  oled_fb_.draw_rect(x, y, w, kSliderH, true);
  const int inner_w = w - 4;
  const int fill = max_value > 0 ? std::clamp(raw * inner_w / max_value, 0, inner_w) : 0;
  if (fill > 0) {
    oled_fb_.fill_rect(x + 2, y + 2, fill, 3, true);
  }
}

void App::render_settings_page() {
  const SettingsPage& page = page_at(settings_page_);
  oled_fb_.fill_rect(0, 0, kOledW, kSettingsHeaderH, true);
  const char* head_icon = page.icon;
  const char* head_title = page.title;
  if (settings_row_ > 0) {
    const SettingsSlot& slot = page.cells[settings_row_ - 1][settings_col_];
    if (slot_used(slot)) {
      head_icon = slot.icon;
      if (slot.title != nullptr) {
        head_title = slot.title;
      }
    }
  }
  const assets::Bitmap* head_bmp = assets::find_icon(head_icon);
  const int icon_w = head_bmp != nullptr ? head_bmp->width : kScreenIcon;
  const int icon_h = head_bmp != nullptr ? head_bmp->height : kScreenIcon;
  draw_icon(3 + (kScreenIcon - icon_w) / 2, 3 + (kScreenIcon - icon_h) / 2, head_icon, false);
  oled_fb_.draw_text(15, 4, head_title, false);
  if (settings_row_ == 0) {
    oled_fb_.draw_rect(2, 2, kScreenIcon + 2, kScreenIcon + 2, false);
  }
  const int dot0 = 108;
  for (int i = 0; i < kSettingsPageCount; ++i) {
    const int x = dot0 + i * 4;
    oled_fb_.set_pixel(x, 7, false);
    if (i == settings_page_) {
      oled_fb_.fill_rect(x - 1, 6, 3, 3, false);
    }
  }

  auto control_y = [](int row) { return row == 1 ? kControlY0 : kControlY1; };
  auto control_x = [](int col) { return col == 0 ? kCol0X : kCol1X; };

  auto slot_raw = [this](SettingsBind bind) {
    switch (bind) {
      case SettingsBind::Brightness:
        return static_cast<int>(state_.brightness_level);
      case SettingsBind::MatrixOn:
        return state_.matrix_enabled ? 1 : 0;
      case SettingsBind::Led:
        return state_.led_enabled ? 1 : 0;
      case SettingsBind::AutoBlink:
        return state_.auto_blink ? 1 : 0;
      case SettingsBind::Boop:
        return state_.boop_enabled ? 1 : 0;
      case SettingsBind::Mouth:
        return state_.mouth_enabled ? 1 : 0;
      case SettingsBind::BoopSens:
        return static_cast<int>(state_.boop_sensitivity);
      case SettingsBind::MouthSens:
        return static_cast<int>(state_.mouth_sensitivity);
      case SettingsBind::Fan:
        return static_cast<int>(state_.fan_speed);
      case SettingsBind::Rare:
        return static_cast<int>(state_.rare_chance);
      default:
        return 0;
    }
  };
  auto slot_max = [](SettingsBind bind) {
    return bind == SettingsBind::Brightness ? 15 : 255;
  };

  for (int row = 0; row < page.row_count; ++row) {
    const int cols = std::max(1, page.cols[row]);
    const int y = control_y(row + 1);
    for (int col = 0; col < cols; ++col) {
      const SettingsSlot& slot = page.cells[row][col];
      if (!slot_used(slot)) {
        continue;
      }
      const int x = cols == 1 ? kCol0X : control_x(col);
      const bool selected = settings_row_ == row + 1 && settings_col_ == col;
      int box_w = 0;
      if (slot.kind == SettingsKind::Toggle) {
        draw_icon(x, y, slot.icon, true);
        draw_toggle(x + kParamIcon + 2, y, slot_raw(slot.bind) != 0);
        box_w = kParamIcon + 2 + kToggleW;
      } else if (slot.kind == SettingsKind::Button) {
        draw_icon(x, y, slot.icon, true);
        const char* label = "Blink";
        char pct[8];
        if (slot.bind == SettingsBind::BoopCal && boop_calibrate_left_ > 0) {
          std::snprintf(pct, sizeof(pct), "%d%%", calib_percent_);
          label = pct;
        } else if (slot.bind == SettingsBind::MouthCal && mouth_calibrate_left_ > 0) {
          std::snprintf(pct, sizeof(pct), "%d%%", calib_percent_);
          label = pct;
        } else if (slot.bind == SettingsBind::Save && action_done_) {
          label = "Done";
        }
        oled_fb_.draw_text(x + kParamIcon + 2, y, label, true);
        box_w = kParamIcon + 2 + text_width(label);
      } else {
        draw_icon(x, y, slot.icon, true);
        const int raw = slot_raw(slot.bind);
        const int max_value = slot_max(slot.bind);
        const int pct = max_value > 0 ? (raw * 100) / max_value : 0;
        char value[8];
        std::snprintf(value, sizeof(value), "%d%%", pct);
        const int vw = text_width(value);
        const int slider_x = x + kParamIcon + 2;
        const int col_right = cols == 1 ? kOledW - 2 : (col == 0 ? kCol1X - 3 : kOledW - 2);
        const int slider_w = std::max(8, col_right - slider_x - vw - 2);
        draw_slider(slider_x, y, slider_w, raw, max_value);
        oled_fb_.draw_text(slider_x + slider_w + 2, y, value, true);
        box_w = col_right - x;
      }
      if (selected) {
        oled_fb_.draw_rect(x - 2, y - 2, box_w + 3, kSliderH + 4, true);
      }
    }
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

  render_settings_page();
  if (settings_slide_dir_ != 0 && settings_from_.size() == static_cast<std::size_t>(oled_fb_.packed_size())) {
    std::vector<std::uint8_t> to(oled_fb_.packed(), oled_fb_.packed() + oled_fb_.packed_size());
    const float t = std::clamp(static_cast<float>(now_ms - settings_slide_start_ms_) /
                                    static_cast<float>(kSettingsSlideMs),
                                0.0f, 1.0f);
    const int dx = static_cast<int>(t * static_cast<float>(kOledW) + 0.5f);
    oled_fb_.clear();
    if (settings_slide_dir_ > 0) {
      oled_fb_.blit_packed_shift(settings_from_.data(), -dx);
      oled_fb_.blit_packed_shift(to.data(), kOledW - dx);
    } else {
      oled_fb_.blit_packed_shift(settings_from_.data(), dx);
      oled_fb_.blit_packed_shift(to.data(), dx - kOledW);
    }
    if (t >= 1.0f) {
      settings_slide_dir_ = 0;
      oled_fb_.clear();
      render_settings_page();
    }
  }
  if (settings_slide_dir_ == 0) {
    settings_from_.assign(oled_fb_.packed(), oled_fb_.packed() + oled_fb_.packed_size());
  }
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
    oled_fb_.blit_bitmap_1bpp(kAutoVisorX, kAutoVisorY, visor->width, visor->height, visor->data, visor->size,
                              visor->width, visor->height);
  }
  draw_face_thumb(kAutoThumbX, kAutoThumbY, auto_next_ != nullptr ? auto_next_->id : "Joy", false);
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

  if (mode_ == Mode::Games) {
    render_oled_games();
    return;
  }
  if (mode_ == Mode::Casino) {
    render_oled_casino();
    return;
  }

  if (mode_ == Mode::Settings) {
    if (state_.setting_index == kSettingStatus1 || state_.setting_index == kSettingStatus2) {
      render_oled_header();
    }
    render_oled_settings(now_ms);
    return;
  }

  if (mode_ == Mode::Snake) {
    render_oled_snake();
    return;
  }
  if (mode_ == Mode::Dino) {
    render_oled_dino();
    return;
  }
  if (mode_ == Mode::BadApple) {
    render_oled_badapple();
    return;
  }
  if (mode_ == Mode::Flappy) {
    render_oled_flappy();
    return;
  }
  if (mode_ == Mode::Tetris) {
    render_oled_tetris();
    return;
  }
  if (mode_ == Mode::Dvd) {
    render_oled_dvd();
    return;
  }
  if (mode_ == Mode::Bsod) {
    render_oled_bsod();
    return;
  }
  render_oled_header();
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
  poll_esc_hold();
  update_sensors(now);
  update_face_player(now);
  update_auto(now);
  update_blink(now);
  update_snake(now);
  update_casino(now);
  update_dino(now);
  update_badapple(now);
  update_flappy(now);
  update_tetris(now);
  update_dvd(now);
  update_bsod(now);
  update_fps(now);

  if (mode_ == Mode::Snake) {
    render_snake(now);
  } else if (mode_ == Mode::Casino) {
    render_casino(now);
  } else if (mode_ == Mode::Dino) {
    render_dino(now);
  } else if (mode_ == Mode::BadApple) {
    render_badapple();
  } else if (mode_ == Mode::Flappy) {
    render_flappy(now);
  } else if (mode_ == Mode::Tetris) {
    render_tetris(now);
  } else if (mode_ == Mode::Dvd) {
    render_dvd(now);
  } else if (mode_ == Mode::Bsod) {
    render_bsod(now);
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
  if (!state_.matrix_enabled && !in_arcade()) {
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
