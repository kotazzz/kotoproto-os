#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "koto/color.hpp"

namespace koto {
namespace gfx {

class Framebuffer {
 public:
  Framebuffer(int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }
  const Color* data() const { return pixels_.data(); }
  Color* data() { return pixels_.data(); }

  void clear(Color color = Color::black());
  void set_pixel(int x, int y, Color color);
  Color get_pixel(int x, int y) const;
  void fill_rect(int x, int y, int w, int h, Color color);
  void draw_rect(int x, int y, int w, int h, Color color);
  void draw_hline(int x, int y, int w, Color color);
  void blit_rgb(int x, int y, int bitmap_w, int bitmap_h, const std::uint8_t* rgb, std::size_t size,
               bool skip_black = false);
  void blit_pix(int x, int y, int bitmap_w, int bitmap_h, const std::uint8_t* pix, std::size_t size,
                bool skip_black = false, Color tint = Color::white(), int clip_y0 = 0, int clip_y1 = 0);
  void expand_column_y(int x, int y0, int h, int amount);
  void rotate_square_cw(int x, int y, int size, int turns);
  void translate_rect(int x, int y, int w, int h, int dx, int dy);
  void glitch_rows(int y0, int h, int amplitude, std::uint32_t seed);
  void hue_cycle_lit(std::uint8_t phase, std::uint16_t mix);
  int draw_char(int x, int y, char ch, Color color);
  int draw_text(int x, int y, std::string_view text, Color color);

 private:
  void put_pixel(int x, int y, Color color);

  int width_;
  int height_;
  std::vector<Color> pixels_;
  std::vector<Color> scratch_;
};

}  // namespace gfx
}  // namespace koto
