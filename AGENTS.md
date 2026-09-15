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
| `firmware/include/koto/app.hpp` + `firmware/src/app.cpp` | Scenes, pad, OLED HUD, blink/boop/mouth, effect hooks |
| `assets/` | Flat authored tree: `emotions.json`, RGB `Name_N.png` faces, BW `visor`/`splash1`/`logo`, settings/games icon atlases, classic face-component sheet |
| `firmware/src/assets/emotions.cpp` | Generated RGB atlas + Emotion table |
| `firmware/src/assets/bitmaps.cpp` | Generated OLED 1bpp sprites from `assets/*.png` |
| `firmware/include/koto/settings.hpp` | 16-byte settings blob, magic `0x13371337`; default flags include mouth |
| `platforms/sim/` | HTTP on `:8080`, `www/` UI, `/atlas.html` |
| `platforms/esp32/` | IDF skeleton; `hal_esp32.cpp` logs only |
| `tools/pack_assets.py` | PNG + `emotions.json` → `emotions.cpp` / `bitmaps.cpp`; also refreshes icon and face-component sheets |
| `tools/settings_atlas.py` | PNG → `settings_icons.json`; pack embeds 9×9/7×7 icons into `bitmaps.cpp` |
| `tools/dino_atlas.py` | Chromium `dino_offline.png` → `dino_sprites.png` (white tiles, purple, black labels) + `dino.cpp` |
| `tools/casino_atlas.py` | `casino_sprites.png` → `firmware/src/assets/casino.cpp` (16×16 reel tiles) |
| `tools/flappy_atlas.py` | `flappy_sprites.png` → `firmware/src/assets/flappy.cpp` (bird / pipe / ground) |
| `tools/tetris_atlas.py` | `tetris_sprites.png` → `firmware/src/assets/tetris.cpp` (mino / ghost / wall) |
| `tools/dvd_atlas.py` | `dvd_sprites.png` → `firmware/src/assets/dvd.cpp` (bouncing DVD oval) |
| `tools/bsod_atlas.py` | `bsod_sprites.png` → `firmware/src/assets/bsod.cpp` (64×2 bar; visor header is 5×7 text) |
| `tools/games_atlas.py` | PNG → `games_icons.json`; pack embeds 32×32 Snake/Casino/Dino/Apple/Flappy/Tetris/DVD/BSOD icons |
| `tools/badapple_pack.py` | `source.mp4` → `assets/badapple.ba1p` (BA1P 64×32 1-bit 25 fps, no zlib) |
| `tools/face_components_atlas.py` | classic RGB faces → unique purple-backed 1bpp eye/mouth/nose tiles in `face_components.png` (authoring sheet, not firmware) |

## Do not delete

- `tools/pack_assets.py`, `assets/emotions.json`, RGB face PNGs and BW HUD PNGs in `assets/`
- `assets/settings_icons.png`, `assets/settings_icons.json` (authoring sheet; packed into firmware HUD icons)
- `assets/games_icons.png`, `assets/games_icons.json` (32×32 Snake/Casino/Dino/Apple/Flappy/Tetris/DVD/BSOD; packed into firmware)
- `assets/badapple.ba1p`, `assets/badapple.json` (packed Bad Apple!! clip; third-party PV, not original art)
- `tools/badapple_pack.py`
- `assets/dino_offline.png` (Chromium 1x T-Rex sheet; packed by `tools/dino_atlas.py`)
- `assets/dino_sprites.png`, `assets/dino_sprites.json` (authoring sheet of unique T-Rex tiles)
- `assets/casino_sprites.png`, `assets/casino_sprites.json` (16×16 reel symbols)
- `assets/flappy_sprites.png`, `assets/flappy_sprites.json` (bird / pipe / ground tiles)
- `tools/flappy_atlas.py`
- `assets/tetris_sprites.png`, `assets/tetris_sprites.json` (mino / ghost / wall tiles)
- `tools/tetris_atlas.py`
- `assets/dvd_sprites.png`, `assets/dvd_sprites.json` (bouncing DVD oval)
- `tools/dvd_atlas.py`
- `assets/bsod_sprites.png`, `assets/bsod_sprites.json` (64×2 white bar; visor header is 5×7 text)
- `tools/bsod_atlas.py`
- `assets/face_components.png`, `assets/face_components.json` (classic eye/nose/mouth sheet; simulator atlas only)
- `trash/` is the retired Toaster pipeline; do not wire it back into pack or firmware
- Do not restore `tools/import_toasterblaster.py`, `tools/adapt_p3_face.py`, or `assets/face/` into the live tree
- `LICENSE`, `NOTICE`, `AUTHORS`, `README.md`, `README.ru.md`

## Commits

After each prompt, commit when the result is a real history step. Split into several commits if the changes are independent.

Commit:
- a large addition (new pipeline, feature, catalog layout)
- a bug fix
- a new emotion (PNG + `emotions.json`, then pack)

Do not commit:
- temp files, local backups, and in-progress polish
- experiments the user has not approved
- build trees, `.tools/venv`, `koto_settings.bin`, IDE junk — see `.gitignore`

Do not push unless the user asks.

## Simulator

```text
Zig (this machine):  .tools/venv/Lib/site-packages/ziglang/zig.exe
Kill old process:    Get-Process koto_sim | Stop-Process -Force
Test binary:         build/koto_test_hello.exe
Sim binary:          build/koto_sim.exe --port 8080
UI:                  http://127.0.0.1:8080/
Atlas:               http://127.0.0.1:8080/atlas.html
```

Windows cannot overwrite `koto_sim.exe` while it is running.

HID GAME report: `[x, y, hat, buttons, mode, 0]` hex POST to `/api/hid`.
Bits: A=1 B=2 X=4 Y=8 OK=16 ESC=32 SELECT=64.
Sim keyboard: WASD stick (WA/WD/AS/DS diagonals), Q=ESC, E=MENU, arrows=X/A/Y/B as on the diamond, Space=OK/blink.
Microphone: POST `/api/sensors` with `pcm=` (256 int8 samples as hex) plus `mic=` RMS. Simplified checkbox on = one-cycle sine from the slider (stable RMS); off = live mic. The sim page resends gyro/prox with that window so a tilt is not zeroed. Atlas may send `mic=` only — the sim HAL synthesizes the sine.

## Emotions

One object per face in `assets/emotions.json`. Codegen emits `Emotion(id, Kind, Effect, …)`.

- **Classic**: microphone mouth + blink + boop (unless `allow_blink`/`allow_boop` false)
- **Special**: overlays off (PowerOff, BatteryCheck, Randomize, Startup, NOPE, …)
- **Effects** (hardcoded hooks): `none`, `snarl`, `dizzy`, `wink`, `randomize`, `glitch`
- **Randomize** (special): every `kRandomizePeriodMs` (100) picks a classic frame with ≤70% lit pixels; blink/mouth overlays stay off
- **Snarl** stretches the mouth band only when `mouth_enabled`
- Default **Blink** face transition: the new eye, mouth and nose appear immediately; then a lid wipe plays over the left eye `32×16` only
- Face pixels are RGB. Accent color is for the LED ring. Long-hold boop tints lit pixels of the Boop face with a left-to-right hue cycle (black stays black).
- Mouth flip is baked into PNG, not a runtime flag
- HUD thumbs are generated 42×16 1bpp from the first RGB frame
- A frame may omit `file` for a black hold (PowerOff, None, flash-off of NOPE). Do not author empty PNGs.
- `Startup` is one visor sprite; the boot splash then switches to `kBootFaces`
- `None` lives in the catalog only, not in stick sets

All authored files sit in `assets/`: `emotions.json`, RGB `Name_N.png` faces, and black/white `visor.png` / `splash1.png` / `logo.png`. Pack embeds RGB faces into `emotions.cpp` and 1bpp HUD sprites into `bitmaps.cpp`. HUD conversion accepts only `#000000` and `#FFFFFF`. HUD 42×16 thumbs are derived from the RGB face at pack time.

Rebuild tables after editing PNG/JSON:

```text
python tools/pack_assets.py
```

`firmware/src/assets/emotions.cpp` and `firmware/src/assets/bitmaps.cpp` start with `// GENERATED by tools/pack_assets.py — do not edit.`

## Overlays on a static sprite

The PNG is a finished 64×32 RGB picture, not layered eye/mouth files. After `blit_rgb`, classic faces rewrite two rectangles in the framebuffer:

1. **Mouth (mic + snarl)** — `expand_column_y` on `y = 16..32`. Lit pixels (any non-black) are copied up/down. The microphone never switches Emotion. Snarl is skipped when mouth anim is off.
2. **Blink** — `fill_rect` black over the left eye `32×16` at `(0,0)` from the top, then a lid line in `accent`. Nose `16×16` at `(48,0)` is never painted.
3. **Boop hue** — after `kBoopHueHoldMs` of a held boop, `hue_cycle_lit` lerps lit pixels of the **Boop** face toward a scrolling HSV wheel. Mix ramps 0→100% over `kBoopHueRampMs`. Black pixels are skipped. The original emotion stays off-screen until boop ends.

Special faces skip both. Wink uses its own eye effect and disables the shared blink.

The default **Blink** emotion transition uses the same eye rectangle: the new eye, mouth and nose come from the new frame immediately; then a closing/opening lid wipe plays over the left eye only.

## Controls (parity notes)

- Startup (~3 s, or any button/stick) ends on **FaceSet** with Neutral — not Settings. OLED fills, then draws the Toaster Blaster visor/logo (no text) until 50% of `kStartupMs`, then KOTOPROTO / by Kotaz / version / bar
- X/A/Y = face sets 1/2/3; B = auto; ESC tap leaves settings or the games list (no boop cal on the main page)
- Hold ESC (~600 ms) opens the games list (Snake, Casino, Dino, Apple, Flappy, Tetris, DVD, BSOD). OLED: inverted 16px yellow header (title + chevrons) and a 32×32 icon in the blue band; do not invert the full 1bpp panel. Blink starts a game; left/right switch titles. In a game, ESC returns to the list.
- Snake: visor cells are 2×2 with a 1px border (left edge at x=0). OLED header shows the score, or `DEAD` on game over; the visor shows only the field.
- Dino: Chromium T-Rex sprites on the visor. Blink / stick up / Y jumps; stick down / A ducks. Visor HUD is `GO` / score / `DEAD`. OLED header is a 5-digit score, or `DEAD` after a crash.
- Flappy: original bird/pipe tiles on the visor. Blink / stick up / Y flaps. Wide gap, slower gravity. Visor HUD is `GO` / score / `DEAD`. OLED header is a 5-digit score, or `DEAD` after a crash.
- Tetris: 10×12 well, 2×2 cells, 7-bag; 8px visor strip above the well for `GO` / score / `DEAD`. Blink / stick up / Y rotates; left/right DAS; down / A soft drop. OLED header is a 5-digit score, or `DEAD` after a crash.
- DVD: white oval logo on the visor, 1px integer bounce. Color changes on each wall hit; OLED shows hit count and `CORNER` after a corner. ESC returns to the list.
- BSOD: Windows 10 blue `#0078D7`, 5×7 `:(` / `Your PC` header, then 2px white bars that appear in sequence. OLED shows “Your PC ran into a problem” and a percent. ESC returns to the list.
- Bad Apple: 64×32 1-bit BA1P clip at 25 fps on the visor (`assets/badapple.ba1p`). Loops. Blink toggles 2.5× playback; ESC returns to the games list. Third-party PV, not original art.
- MENU/SELECT short: BT → Frame → settings pages → BT. Stick does **not** open BT/Frame. Hold (~600 ms) jumps to the settings pages from any screen
- Settings pages (OLED): inverted 16px header with 9×9 page icon + short name; selecting a control swaps the header to that parameter's icon and title. Controls are icons. Horizontal stick on the header flips pages with a slide. Down selects a control; OK/blink toggles or runs a button; left/right on a slider nudges it. CAL/SENSE: B-CAL / M-CAL, then B-SNS / M-SNS (boop threshold and mouth gain). Games live in a separate list (hold ESC), not in settings.
- KEY-mode Esc is `kBtnEsc` only (not B). KEY Enter is still OK|A
- Stick dead zone for activity and octant: ±64 (`kStickDeadzone`); apply face on return to center **unless OK is held**
- OK starts blink only if stick is near center; displaced stick + OK = cancel pending face, no blink
- Header name follows the **hovered** octant while the stick is out
- FaceSet HUD: selected thumb is inverted with black `draw_corners` (length 3) for rounding; hovered octant gets white corner brackets, not a full rectangle
- Microphone default **on** (`mouth_enabled` / `kFlagMouth`); bars only, never changes the Emotion. Mouth gain is `mouth_sensitivity` (default 192) on CAL/SENSE as M-SNS, next to boop B-SNS.
- Auto HUD: original `visor` 54×38 at (4,26) plus generated thumb at (12,42)
- Blink covers **left eye 32×16 only**; do not paint over the nose
- Right P3 panel is not drawn in firmware; atlas UI mirrors the left half in CSS
- Gyro nudge always translates the full 64×32 (pitch/roll; yaw unused). Dizzy spins a 16×16 patch at `(8,0)`
- HID disconnect (`connected()` falling edge) enters Settings. ESP32 stub keeps `connected()` true

## Face layout on P3

```
eye   32×16 at (0, 0)
nose  16×16 at (48, 0)
mouth 64×16 at (0, 16)
```

Author full 64×32 RGB frames. Regions above are overlays (blink / mouth / snarl).

## Sprite sheets

Finished sprites for games, HUD, apps, and similar UI go on an **authoring sprite sheet**, not only as procedural draw or a buried crop from a source PNG.

- One sheet per set: unique tiles, purple `#FF00FF` gutters ≥1px, **black** labels on that gutter (labels are not packed; black is for contrast on the bright purple).
- Icon/game tiles themselves stay as they appear in firmware (usually white on black).
- PNG + JSON in `assets/` (e.g. `dino_sprites.png`, `casino_sprites.png`, `flappy_sprites.png`, `tetris_sprites.png`, `dvd_sprites.png`, `bsod_sprites.png`, `games_icons.png`, `settings_icons.png`). A `tools/*_atlas.py` script extracts tiles and emits firmware tables.
- Show the sheet on `/atlas.html`. Pack embeds the tiles the visor actually blits.
- Do not leave a new game or app with sprites that exist only in C++ drawing code or in an unreadable source atlas. Split them out like Dino and Casino.

## Style

- C++17, includes only at the top of the file (no local Python or C++ imports mid-function)
- Atlas labels (not packed into firmware) are black on `#FF00FF` for contrast; tiles themselves stay white-on-black
- Do not invent hardware pin numbers; keep `pins::*` at -1 until the user names the board
- Do not run `check-update.sh` / em-corp version scripts
- Prefer editing existing files over new layers of abstraction

## Build snippet (Zig)

Core test + sim: compile `firmware/src/{app,assets/badapple,assets/bitmaps,assets/casino,assets/dino,assets/flappy,assets/tetris,assets/dvd,assets/bsod,assets/emotions,face/transition,gfx/framebuffer,gfx/oled_canvas,gfx/font5x7,protocol/mocute}.cpp` plus `firmware/src/assets/badapple_blob.S` and `tests/test_hello.cpp` or `platforms/sim/src/{main,hal_sim,http_server}.cpp -lws2_32`.
