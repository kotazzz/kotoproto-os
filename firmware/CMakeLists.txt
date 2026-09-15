add_library(koto_core
  src/app.cpp
  src/assets/badapple.cpp
  src/assets/badapple_blob.S
  src/assets/bitmaps.cpp
  src/assets/casino.cpp
  src/assets/dino.cpp
  src/assets/flappy.cpp
  src/assets/tetris.cpp
  src/assets/dvd.cpp
  src/assets/bsod.cpp
  src/assets/emotions.cpp
  src/gfx/framebuffer.cpp
  src/gfx/oled_canvas.cpp
  src/gfx/font5x7.cpp
  src/protocol/mocute.cpp
  src/face/transition.cpp
)

target_include_directories(koto_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
