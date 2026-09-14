# kotoproto-os

**English** | [Русский](README.ru.md)

Personal LED visor firmware for a Protogen-style helmet, **simulator-first**: P3 RGB 64×32 faces, SSD1306 HUD, Mocute pad mapping, WS2812 ring logic, microphone mouth bars, and a boop sensor. The ESP32 target is a stub until a board is chosen.

> **Roadmap / current status.** There is a **working PC simulator with a browser UI**. The ESP32 HAL is a stub. The firmware has **not been adapted or tested on real hardware**. The exact board, panel, and wiring will be documented later.

## License and authorship

kotoproto-os is released under the **GNU Affero General Public License v3.0** (`LICENSE`). That matches [Toaster Blaster](https://github.com/diodeface/ToasterBlaster) by [diodeface](https://github.com/diodeface), which is also AGPL-3.0.

- **Author of this project:** Kotaz (2026). Developed for personal use.
- **Upstream inspiration:** [Toaster Blaster](https://github.com/diodeface/ToasterBlaster) — sequences, HUD layout, Mocute mapping, overlays (blink, boop, mouth bars), and original 1-bit face art, now stored as RGB 64×32 frames. Many of those components were taken from that repository and then rewritten for a P3 RGB panel.
- This tree is **not** a drop-in MAX7219 port. Logic was reimplemented against a 64×32 RGB framebuffer, with changes for this visor. A large part of the new code was written with AI assistance in [Cursor](https://cursor.com).
- See `NOTICE` for the short attribution block. If you run the simulator as a network service, AGPL section 13 requires offering this source to users; locally the source is this repository.

## What works today (simulator)

- 26 named emotions as P3 64×32 RGB frames (left half; right half is mirrored in the atlas UI)
- Three Mocute face sets on X / A / Y, MENU cycles BT / Frame / settings, auto-cycle on B
- OLED HUD: header preview, emotion name, boop box, microphone bar, 8-face ring, Auto visor sprite, 14-item settings
- Blink, boop glitch, gyro nudge, snake, PWM fan value, rare transitions
- Browser pad: stick, buttons, mic / gyro / proximity sliders
- Emotion atlas at `/atlas.html` (classic/special lists, visor L+mirror, blink/mouth preview)

## Build and run the simulator

Need CMake 3.16+, a C++17 compiler, and Python 3 only if you re-pack PNG into firmware.

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
  firmware/src/app.cpp firmware/src/assets/bitmaps.cpp firmware/src/assets/emotions.cpp `
  firmware/src/face/transition.cpp firmware/src/gfx/framebuffer.cpp firmware/src/gfx/oled_canvas.cpp `
  firmware/src/gfx/font5x7.cpp firmware/src/protocol/mocute.cpp tests/test_hello.cpp `
  -o build/koto_test_hello.exe
.\build\koto_test_hello.exe

& $zig c++ -std=c++17 -O2 -I firmware/include -I platforms/sim/include `
  firmware/src/app.cpp firmware/src/assets/bitmaps.cpp firmware/src/assets/emotions.cpp `
  firmware/src/face/transition.cpp firmware/src/gfx/framebuffer.cpp firmware/src/gfx/oled_canvas.cpp `
  firmware/src/gfx/font5x7.cpp firmware/src/protocol/mocute.cpp `
  platforms/sim/src/main.cpp platforms/sim/src/hal_sim.cpp platforms/sim/src/http_server.cpp `
  -lws2_32 -o build/koto_sim.exe
.\build\koto_sim.exe --port 8080
```

Open http://127.0.0.1:8080/  (atlas: http://127.0.0.1:8080/atlas.html)

Stop an old `koto_sim` process before relinking the `.exe` on Windows.

## Firmware config

Tunable sizes, timings, and **placeholder** GPIO numbers live in `firmware/include/koto/config.hpp`.

| Symbol | Meaning | Default |
| --- | --- | --- |
| `kMatrixW` / `kMatrixH` | P3 RGB panel | 64×32 |
| `kOledW` / `kOledH` | SSD1306 HUD | 128×64 |
| `kFaceW` / `kFaceH` | Left half-face bitmap | 64×32 |
| `kEye*` / `kMouth*` | Eye and mouth overlays | eye 32×16 at (0,0); mouth 64×16 at (0,16) |
| `kStartupMs` | Splash length | 3000 |
| `kBoopTriggerCount` / `kBoopTriggersMax` | Boop hysteresis 4/6 | 4 / 6 |
| `kTickMs` | Sim / task period | 33 |
| `pins::*` | ESP32 GPIO | `-1` until hardware is chosen |

Other knobs:

- Emotion catalog, stick sets, HUD labels: `assets/emotions.json` (packed into `firmware/src/assets/emotions.cpp`)
- OLED 1bpp sprites: black/white `visor.png` / `splash1.png` / `splash2.png` in `assets/` (packed into `firmware/src/assets/bitmaps.cpp`)
- Settings blob / EEPROM-style flags: `firmware/include/koto/settings.hpp`
- Mocute button bits: `firmware/include/koto/protocol/mocute.hpp`
- LED ring count: `firmware/include/koto/hal/led_ring.hpp` (`kLedRingCount = 12`, drawn twice on device)
- Version string: `firmware/include/koto/version.hpp`

PNG files in `assets/` are the authored faces (RGB `Name_N.png`) and HUD sprites (black/white only). Pack embeds them into firmware (plus OLED thumbs from RGB faces). It does not rebuild pixels from Toaster Blaster.

Regenerate firmware tables after editing PNG or `assets/emotions.json`:

```bash
python tools/pack_assets.py
```

## ESP32 (not ready)

`platforms/esp32` links the same `koto::App` but `hal_esp32.cpp` does not drive a panel, OLED, BLE, or LEDs yet. Do not expect a visor to light up.

When ESP-IDF is installed and HAL is filled in:

```bash
cd platforms/esp32
idf.py set-target esp32
idf.py build
```

`sdkconfig.defaults` only enables 4 MB flash and NimBLE. Pin matrix, HUB75, I2C, and ADC will be added with the hardware notes.

## Mocute

GAME report, 6 bytes: X, Y, hat, buttons, mode, 0. Buttons: A B X Y OK ESC SELECT.

| Control | Action |
| --- | --- |
| (boot skip) | FaceSet with Neutral (not Settings) |
| X / A / Y | Face sets 1 / 2 / 3 |
| MENU / SELECT | Short: BT → Frame → settings → BT. Hold: settings list from any screen |
| B | Auto faces |
| ESC | Back from Settings only (KEY Esc is Esc, not B) |
| OK | Blink if the stick is centered; with stick held, cancels the pending face |

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
