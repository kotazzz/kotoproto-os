# kotoproto-os

**English** | [Русский](README.ru.md)

Personal LED visor firmware for a Protogen-style helmet. Development is simulator-first: faces render on a P3 RGB 64×32 panel, the HUD on an SSD1306, and input comes from a Mocute pad. The same logic drives the WS2812 ring, microphone mouth bars, and the boop sensor. The ESP32 HAL remains a stub.

> **Roadmap / current status.** There is a working PC simulator with a browser UI. The ESP32 HAL is a stub. The firmware has not been adapted or tested on real hardware. The board is a LILYGO T8 V1.8. Wiring is in the [pinout](#pinout).

## License and authorship

kotoproto-os is released under the **GNU Affero General Public License v3.0** (`LICENSE`). That matches [Toaster Blaster](https://github.com/diodeface/ToasterBlaster) by [diodeface](https://github.com/diodeface), which is also AGPL-3.0.

- **Author of this project:** Kotaz (2026). Developed for personal use.
- **Upstream inspiration:** [Toaster Blaster](https://github.com/diodeface/ToasterBlaster). Sequences, HUD layout, Mocute mapping, overlays (blink, boop, mouth bars), and the original 1-bit face art all come from that repository; they are now stored as RGB 64×32 frames. Many of those components were taken from that tree and rewritten for a P3 RGB panel.
- This tree is **not** a drop-in MAX7219 port. Logic was reimplemented against a 64×32 RGB framebuffer, with changes for this visor. A large part of the new code was written with AI assistance in [Cursor](https://cursor.com).
- See `NOTICE` for the short attribution block. If you run the simulator as a network service, AGPL section 13 requires offering this source to users; locally the source is this repository.

## What works today (simulator)

The simulator has 25 named emotions as P3 64×32 RGB frames. The atlas stores the left half of the face; the right half is mirrored in the UI.

X, A, and Y select the three Mocute face sets. MENU cycles BT, Frame, and settings. B starts auto-cycle.

The OLED HUD shows a header preview, the emotion name, the boop box, the microphone bar, and an 8-face ring. In Auto it also draws the visor sprite. Settings contain 14 items.

Blink, boop glitch, a long-hold boop hue cycle, gyro nudge, PWM fan value, and rare transitions all run in the simulator. It also supports these games:

* snake;
* casino;
* Chromium T-Rex;
* Flappy Bird;
* Tetris;
* DVD screensaver;
* Windows BSOD;
* spectrum analyzer;
* Bad Apple!!.

The browser pad exposes the stick, buttons, and mic / gyro / proximity sliders. The emotion atlas is at http://127.0.0.1:8080/atlas.html and lists classic and special faces, the left visor plus its mirror, and blink/mouth preview.

## Build and run the simulator

You need CMake 3.16+ and a C++17 compiler. Python 3 is required only if you re-pack PNG into firmware.

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Windows (Zig C++ as used in this repo):

```powershell
python -m venv .tools\venv
.\.tools\venv\Scripts\pip install ziglang
$zig = ".\.tools\venv\Lib\site-packages\ziglang\zig.exe"
& $zig c++ -std=c++17 -O2 -I firmware/include `
  firmware/src/app.cpp firmware/src/assets/badapple.cpp firmware/src/assets/badapple_blob.S firmware/src/assets/bitmaps.cpp firmware/src/assets/casino.cpp firmware/src/assets/dino.cpp firmware/src/assets/flappy.cpp firmware/src/assets/tetris.cpp firmware/src/assets/dvd.cpp firmware/src/assets/bsod.cpp firmware/src/assets/spectrum.cpp firmware/src/assets/emotions.cpp `
  firmware/src/face/transition.cpp firmware/src/gfx/framebuffer.cpp firmware/src/gfx/oled_canvas.cpp `
  firmware/src/gfx/font5x7.cpp firmware/src/protocol/mocute.cpp tests/test_hello.cpp `
  -o build/koto_test_hello.exe
.\build\koto_test_hello.exe

& $zig c++ -std=c++17 -O2 -I firmware/include -I platforms/sim/include `
  firmware/src/app.cpp firmware/src/assets/badapple.cpp firmware/src/assets/badapple_blob.S firmware/src/assets/bitmaps.cpp firmware/src/assets/casino.cpp firmware/src/assets/dino.cpp firmware/src/assets/flappy.cpp firmware/src/assets/tetris.cpp firmware/src/assets/dvd.cpp firmware/src/assets/bsod.cpp firmware/src/assets/spectrum.cpp firmware/src/assets/emotions.cpp `
  firmware/src/face/transition.cpp firmware/src/gfx/framebuffer.cpp firmware/src/gfx/oled_canvas.cpp `
  firmware/src/gfx/font5x7.cpp firmware/src/protocol/mocute.cpp `
  platforms/sim/src/main.cpp platforms/sim/src/hal_sim.cpp platforms/sim/src/http_server.cpp `
  -lws2_32 -o build/koto_sim.exe
.\build\koto_sim.exe --port 8080
```

Open http://127.0.0.1:8080/. The atlas is at http://127.0.0.1:8080/atlas.html.

Stop an old `koto_sim` process before relinking the `.exe` on Windows.

## Firmware config

Tunable sizes, timings, and T8 GPIO numbers live in `firmware/include/koto/config.hpp`. Key parameters are listed below.

| Symbol | Meaning | Default |
| --- | --- | --- |
| `kMatrixW` / `kMatrixH` | P3 RGB panel | 64×32 |
| `kOledW` / `kOledH` | SSD1306 HUD | 128×64 |
| `kOledYellowH` | Dual-color split (1bpp) | 16 yellow / rest blue |
| `kFaceW` / `kFaceH` | Left half-face bitmap | 64×32 |
| `kEye*` / `kMouth*` | Eye and mouth overlays | eye 32×16 at (0,0); mouth 64×16 at (0,16) |
| `kStartupMs` | Splash length | 3000 |
| `kBoopTriggerCount` / `kBoopTriggersMax` | Boop hysteresis | 4 / 6 |
| `kTickMs` | Sim / task period | 33 |
| `pins::*` | LILYGO T8 V1.8 GPIO | [Pinout](#pinout) |

Face sets, the auto-cycle pool, and HUD labels are defined in `assets/emotions.json`. After packing they are embedded as KPIX through `firmware/src/assets/emotions.cpp`.

OLED 1bpp sprites come from the black/white `visor.png`, `splash1.png`, and `logo.png` files in `assets/` and are packed into `firmware/src/assets/bitmaps.cpp`. Settings and games icon atlases live in `assets/settings_icons.png` and `assets/games_icons.png` and stay 1bpp.

Bad Apple!! is stored in `assets/badapple.ba1p` as BA1P 64×32 1-bit at 25 fps (patch, `0xFE` mask, or raw). It is a third-party PV, not original art.

The Chromium source `assets/dino_offline.png` is converted into the sprite sheet `assets/dino_sprites.png`, which is then embedded through `firmware/src/assets/dino.cpp`. The remaining game sheets pack as follows:

| Game | Sheet | Firmware |
| --- | --- | --- |
| Casino | `assets/casino_sprites.png` | `firmware/src/assets/casino.cpp` |
| Flappy Bird | `assets/flappy_sprites.png` | `firmware/src/assets/flappy.cpp` |
| Tetris | `assets/tetris_sprites.png` | `firmware/src/assets/tetris.cpp` |
| DVD | `assets/dvd_sprites.png` | `firmware/src/assets/dvd.cpp` |
| BSOD | `assets/bsod_sprites.png` | `firmware/src/assets/bsod.cpp` |
| Spectrum | `assets/spectrum_sprites.png` | `firmware/src/assets/spectrum.cpp` |

Classic face components for authoring sit in `assets/face_components.png` and are shown on `/atlas.html`.

The settings blob is described in `firmware/include/koto/settings.hpp`. Mocute button bits live in `firmware/include/koto/protocol/mocute.hpp`. The LED ring is defined in `firmware/include/koto/hal/led_ring.hpp` as 12 LEDs, drawn twice on the device. The version string is in `firmware/include/koto/version.hpp`.

PNG files in `assets/` are the authored faces (RGB `Name_N.png`) and HUD sprites (black/white only). Pack embeds visor tiles as KPIX and OLED thumbs/icons as 1bpp. It does not rebuild pixels from Toaster Blaster.

Regenerate firmware tables after editing PNG or `assets/emotions.json`:

```bash
python tools/pack_assets.py
```

## Pinout

The visor uses a LILYGO T8 V1.8. Pin numbers are `koto::pins` in `firmware/include/koto/config.hpp`, taken from `esp32-tools/docs/t8_visor_wiring.html`. Two 64×32 P3 panels share one HUB75 chain (`kMatrixChain` is 2, brightness is 77). GPIO2 stays unused, and the TF card slot stays empty. GPIO34, GPIO35, and GPIO36 are input-only.

| GPIO | Symbol | Connection |
| --- | --- | --- |
| 0 | `kFanPwm` | Fan PWM, 25 kHz. Strap pin: pull-up until PWM attaches, then duty starts at 0 |
| 2 | — | Unused (strapping / SD-MISO) |
| 4 | `kMatrixB2` | HUB75 blue, lower half |
| 5 | `kMatrixC` | HUB75 address C |
| 12 | `kLedDin` | WS2812 DIN. 12 LEDs; the frame is written twice so a second ring on DOUT copies the first |
| 13 | `kMatrixG2` | HUB75 green, lower half |
| 14 | `kMatrixR2` | HUB75 red, lower half |
| 15 | `kMatrixLat` | HUB75 latch |
| 18 | `kMatrixD` | HUB75 address D |
| 19 | `kMatrixB` | HUB75 address B |
| 21 | `kOledSda` | I2C SDA. SSD1306 at `0x3C`, MPU6050 at `0x68` |
| 22 | `kOledScl` | I2C SCL, 400 kHz |
| 23 | `kMatrixA` | HUB75 address A |
| 25 | `kMatrixR1` | HUB75 red, upper half |
| 26 | `kMatrixG1` | HUB75 green, upper half |
| 27 | `kMatrixB1` | HUB75 blue, upper half |
| 32 | `kMatrixClk` | HUB75 clock |
| 33 | `kMatrixOe` | HUB75 output enable |
| 34 | `kFanRpm` | Fan tachometer |
| 35 | `kMicAdc` | MAX9814 analog out, ADC1 channel 7 |
| 36 | `kBoopAdc` | TCRT5000 analog out, ADC1 channel 0 |
| — | `kMatrixE` | HUB75 address E is not wired |

## ESP32 (T8)

The live HAL is PlatformIO `env:t8` at the repo root (`platforms/pio`). The IDF tree in `platforms/esp32` is still a log stub.

```bash
pio run -e t8 -t upload
```

When ESP-IDF is installed and HAL is filled in:

```bash
cd platforms/esp32
idf.py set-target esp32
idf.py build
```

`sdkconfig.defaults` enables 4 MB flash and NimBLE only, with no Wi-Fi and no Bluedroid. Stay on ESP-IDF; PlatformIO would still vendor IDF for ESP32 BLE. Pin matrix, HUB75, I2C, and ADC will be added with the hardware notes.

## Mocute

The pad sends a 6-byte GAME report: X, Y, hat, buttons, mode, and a trailing 0. Buttons are A, B, X, Y, OK, ESC, and SELECT.

| Control | Action |
| --- | --- |
| (boot skip) | FaceSet with Neutral, not Settings |
| X / A / Y | Face sets 1 / 2 / 3 |
| MENU / SELECT | a short press cycles BT, Frame, and settings; a hold opens the settings list from any screen |
| B | Auto faces |
| ESC | Back from Settings only (KEY Esc is Esc, not B) |
| OK | Blink if the stick is centered; with stick held, cancels the pending face |
| Sim keys | WASD is the stick (diagonals WA/WD/AS/DS), Q=ESC, E=MENU, arrows=X/A/Y/B, Space=blink |

## Tree

```
firmware/           core: faces, HUD, HID, HAL interfaces, config
platforms/sim/      HTTP + browser UI
platforms/esp32/    IDF skeleton, HAL stubs
assets/             emotions.json + RGB faces + black/white HUD sprites (flat)
tools/              pack_assets.py (PNG → firmware)
trash/              retired Toaster import/adapt, kept for reference
tests/              headless core test
AGENTS.md           notes for Cursor / other agents
```

## Credits

- Kotaz — kotoproto-os
- [diodeface / Toaster Blaster](https://github.com/diodeface/ToasterBlaster) — original visor firmware this project is based on (AGPL-3.0)
- [Chromium Authors](https://chromium.googlesource.com/chromium/src/) — T-Rex runner sprites (BSD-style)
