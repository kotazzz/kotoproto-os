# Agent notes — kotoproto-os

Always-on context for Cursor (and any other agent) working in this repo.
Reply to the user in Russian unless they write in another language.

## What this is

C++17 visor firmware: `koto::App` is shared by a **PC browser simulator** and a **stub ESP32 target**. Real hardware is not chosen yet. Do not claim the visor was flashed or tested on a panel.

Upstream inspiration: [Toaster Blaster](https://github.com/diodeface/ToasterBlaster) (AGPL-3.0). This tree must stay AGPL-3.0. Credit Kotaz + diodeface in user-facing docs. Do not strip `LICENSE` / `NOTICE`.

## Layout

| Path | Role |
| --- | --- |
| `firmware/include/koto/config.hpp` | Sizes, timings, placeholder GPIO (`pins::*` = -1) |
| `firmware/include/koto/app.hpp` + `firmware/src/app.cpp` | Scenes, pad, faces, OLED HUD, blink/boop/mouth |
| `firmware/src/assets/face_p3.cpp` | Embedded 64×32 1bpp frames (MSB row-packed) |
| `firmware/src/assets/bitmaps.cpp` | Native HUD parts 16×8 / 8×8 / 32×8 + visor XBM (bit-reversed from original) |
| `firmware/include/koto/settings.hpp` | 16-byte settings blob, magic `0x13371337` |
| `platforms/sim/` | HTTP on `:8080`, `www/` UI, HID inject `/api/hid` |
| `platforms/esp32/` | IDF skeleton; `hal_esp32.cpp` logs only |
| `tools/import_toasterblaster.py` | Parse original sequences → catalog |
| `tools/adapt_p3_face.py` | Scale parts to P3 C++. `flip_mouth` is a **runtime** Rotate180, not baked into pixels |
| `tools/face/` | Pixel editor; keep it |

## Do not delete

- `tools/*.py`, `tools/face/`, `assets/` — needed to regenerate faces
- `LICENSE`, `NOTICE`, `AUTHORS`, `README.md`, `README.ru.md`

## Do not commit

Build trees, `.tools/venv`, `koto_settings.bin`, IDE junk — see `.gitignore`.

## Simulator

```text
Zig (this machine):  .tools/venv/Lib/site-packages/ziglang/zig.exe
Kill old process:    Get-Process koto_sim | Stop-Process -Force
Test binary:         build/koto_test_hello.exe
Sim binary:          build/koto_sim.exe --port 8080
UI:                  http://127.0.0.1:8080/
```

Windows cannot overwrite `koto_sim.exe` while it is running.

HID GAME report: `[x, y, hat, buttons, mode, 0]` hex POST to `/api/hid`.
Bits: A=1 B=2 X=4 Y=8 OK=16 ESC=32 SELECT=64.

## Controls (parity notes)

- X/A/Y = face sets 1/2/3; MENU/SELECT = auto; B = settings; ESC = **only** leave settings (no boop cal on the main page)
- Stick dead zone for octant: ±64; apply face on return to center **unless OK is held**
- OK starts blink only if stick is near center; displaced stick + OK = cancel pending face, no blink
- Header name follows the **hovered** octant while the stick is out
- Microphone default **on** (`mouth_enabled`); bars only, never changes Sequence
- Angry / Annoyed `flipMouth` is **runtime**: native 32×8 mouth drawn 2× with Rotate180, then a snarl pulse. Do **not** bake 180° into the P3 bitmap
- HUD ring uses native 16×8 eye + 8×8 nose + 32×8 mouth (Toaster Blaster cell 42×16). Angry/Annoyed HUD mouth is rotated 180 to match runtime P3 `flipMouth`
- Auto HUD: original `visor` 54×38 at (0,26) plus parts at (15,40)/(44,40)/(18,51)
- Blink covers **left eye 32×16 only**; do not paint over the nose
- Right P3 panel is not drawn in the sim; comment in `face_p3.hpp` still describes the mirror plan

## Face layout on P3

```
eye   32×16 at (0, 0)
nose  16×16 at (48, 0)
mouth 64×16 at (0, 16)
```

Bitmaps are 2× nearest-neighbor from MAX7219 16×8 / 8×8 / 32×8. Duplicate rows are expected.

## Style

- C++17, includes only at the top of the file (no local Python or C++ imports mid-function)
- Do not invent hardware pin numbers; keep `pins::*` at -1 until the user names the board
- Do not commit unless the user asks
- Do not run `check-update.sh` / em-corp version scripts
- Prefer editing existing files over new layers of abstraction

## Build snippet (Zig)

Core test + sim: compile `firmware/src/{app,assets/bitmaps,assets/face_p3,face/transition,gfx/framebuffer,gfx/oled_canvas,gfx/font5x7,protocol/mocute}.cpp` plus `tests/test_hello.cpp` or `platforms/sim/src/{main,hal_sim,http_server}.cpp -lws2_32`.
