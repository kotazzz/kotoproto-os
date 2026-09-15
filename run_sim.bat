@echo off
cd /d D:\prj\kotoproto-os
python tools\pack_assets.py || exit /b 1
taskkill /IM koto_sim.exe /F 2>nul
if not exist build mkdir build

.tools\venv\Lib\site-packages\ziglang\zig.exe c++ -std=c++17 -O2 ^
  -I firmware/include ^
  -I platforms/sim/include ^
  firmware/src/app.cpp ^
  firmware/src/assets/badapple.cpp ^
  firmware/src/assets/badapple_blob.S ^
  firmware/src/assets/bitmaps.cpp ^
  firmware/src/assets/casino.cpp ^
  firmware/src/assets/dino.cpp ^
  firmware/src/assets/flappy.cpp ^
  firmware/src/assets/tetris.cpp ^
  firmware/src/assets/dvd.cpp ^
  firmware/src/assets/bsod.cpp ^
  firmware/src/assets/emotions.cpp ^
  firmware/src/face/transition.cpp ^
  firmware/src/gfx/framebuffer.cpp ^
  firmware/src/gfx/oled_canvas.cpp ^
  firmware/src/gfx/font5x7.cpp ^
  firmware/src/protocol/mocute.cpp ^
  platforms/sim/src/main.cpp ^
  platforms/sim/src/hal_sim.cpp ^
  platforms/sim/src/http_server.cpp ^
  -lws2_32 -o build/koto_sim.exe || exit /b 1

build\koto_sim.exe --port 8080