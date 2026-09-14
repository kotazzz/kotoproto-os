#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace koto {
namespace gfx {

class OledCanvas {
 public:
  OledCanvas(int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }
  const std::uint8_t* packed() const { return bits_.data(); }
  int packed_size() const { return static_cast<int>(bits_.size()); }

  void clear();
  void set_pixel(int x, int y, bool on);
  bool get_pixel(int x, int y) const;
  void fill_rect(int x, int y, int w, int h, bool on);
  void invert_rect(int x, int y, int w, int h);
  void draw_hline(int x, int y, int w, bool on);
  void draw_vline(int x, int y, int h, bool on);
  void draw_rect(int x, int y, int w, int h, bool on);
  void draw_line(int x0, int y0, int x1, int y1, bool on);
  void blit_bitmap_1bpp(int x, int y, int bitmap_w, int bitmap_h, const std::uint8_t* data,
                        std::size_t size, int dst_w, int dst_h, bool on = true, bool invert = false);
  int draw_char(int x, int y, char ch, bool on, int scale = 1);
  int draw_text(int x, int y, std::string_view text, bool on, int scale = 1);

 private:
  int width_;
  int height_;
  int bytes_per_row_;
  std::vector<std::uint8_t> bits_;
};

}  // namespace gfx
}  // namespace koto
