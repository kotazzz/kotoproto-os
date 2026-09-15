# kotoproto-os

[English](README.md) | **Русский**

Личная прошивка LED-визора для Protogen-шлема, **сначала симулятор**: лица P3 RGB 64×32, HUD SSD1306, карта Mocute, логика кольца WS2812, рот с микрофона и датчик бупа. Цель ESP32 — заглушка, пока нет платы.

> **Роадмап / статус.** Есть **тестовая сборка с браузером**. HAL под ESP32 — заглушки. Прошивка **ещё не адаптирована и не тестировалась на реальном оборудовании**. Плата, панель и распиновка будут указаны позже.

## Лицензия и авторство

kotoproto-os выходит под **GNU Affero General Public License v3.0** (`LICENSE`) — как и [Toaster Blaster](https://github.com/diodeface/ToasterBlaster) автора [diodeface](https://github.com/diodeface).

- **Автор этого проекта:** Kotaz (2026). Разработка для личных целей.
- **Исходное вдохновение:** [Toaster Blaster](https://github.com/diodeface/ToasterBlaster) — последовательности лиц, раскладка HUD, карта Mocute, оверлеи (моргание, буп, полоски рта) и исходная 1-битная графика, теперь в RGB 64×32. Многие компоненты взяты оттуда и перенесены на P3 RGB.
- Это **не** клон прошивки MAX7219 один в один. Логика переписана под RGB-кадр 64×32 и изменена под свои требования. Большая часть нового кода написана с помощью ИИ в [Cursor](https://cursor.com).
- Короткий блок атрибуции — в `NOTICE`. Если симулятор торчит в сеть, по AGPL §13 нужно отдавать исходники; локально исходник — этот репозиторий.

## Что уже можно гонять (сим)

- 25 имён лиц кадрами P3 64×32 RGB (левая половина; правая в атласе зеркалится в UI)
- Три сета на X / A / Y, MENU листает BT / Frame / настройки, автосмена на B
- OLED: шапка, имя эмоции, буп, бар микрофона, кольцо из 8 лиц, спрайт visor в Auto, меню из 14 пунктов
- Морг, глитч бупа, радуга при долгом бупе, гироскоп, змейка, казино, динозаврик Chromium, Flappy Bird, Tetris, DVD-заставка, Windows BSOD, спектрометр, Bad Apple!!, вентилятор, редкие переходы
- В браузере: стик, кнопки, слайдеры микрофона / гиро / приближения
- Атлас эмоций: http://127.0.0.1:8080/atlas.html

## Сборка и запуск симулятора

Нужны CMake 3.16+, компилятор C++17. Python 3 — только чтобы упаковать PNG в прошивку.

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Windows (Zig C++ как в этом репозитории):

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

Откройте http://127.0.0.1:8080/  (атлас: http://127.0.0.1:8080/atlas.html)

Перед перелинковкой `.exe` на Windows остановите старый `koto_sim`.

## Конфиг прошивки

Размеры, тайминги и **заглушки GPIO** — в `firmware/include/koto/config.hpp`.

| Символ | Смысл | По умолчанию |
| --- | --- | --- |
| `kMatrixW` / `kMatrixH` | панель P3 RGB | 64×32 |
| `kOledW` / `kOledH` | HUD SSD1306 | 128×64 |
| `kOledYellowH` | Dual-color 1bpp | 16 жёлтых / остальное голубое |
| `kFaceW` / `kFaceH` | левая половина лица | 64×32 |
| `kEye*` / `kMouth*` | глаз и рот | глаз 32×16 в (0,0); рот 64×16 в (0,16) |
| `kStartupMs` | заставка | 3000 |
| `kBoopTriggerCount` / `kBoopTriggersMax` | гистерезис бупа 4/6 | 4 / 6 |
| `kTickMs` | период тика | 33 |
| `pins::*` | GPIO ESP32 | `-1`, пока нет железа |

Ещё:

- сеты лиц, авто-пул, подписи HUD: `assets/emotions.json` (KPIX в `firmware/src/assets/emotions.cpp`)
- OLED-спрайты 1bpp: чёрно-белые `visor.png` / `splash1.png` / `logo.png` в `assets/` (собирается в `firmware/src/assets/bitmaps.cpp`)
- атласы иконок настроек и игр: `assets/settings_icons.png`, `assets/games_icons.png` (остаются 1bpp)
- Bad Apple!!: `assets/badapple.ba1p` (BA1P 64×32 1-бит 25 fps, патч / маска `0xFE` / сырой кадр; чужой PV)
- динозаврик: `assets/dino_offline.png` → `assets/dino_sprites.png` + `firmware/src/assets/dino.cpp`
- казино: `assets/casino_sprites.png` → `firmware/src/assets/casino.cpp`
- Flappy Bird: `assets/flappy_sprites.png` → `firmware/src/assets/flappy.cpp`
- Tetris: `assets/tetris_sprites.png` → `firmware/src/assets/tetris.cpp`
- DVD: `assets/dvd_sprites.png` → `firmware/src/assets/dvd.cpp`
- BSOD: `assets/bsod_sprites.png` → `firmware/src/assets/bsod.cpp`
- спектрометр: `assets/spectrum_sprites.png` → `firmware/src/assets/spectrum.cpp`
- компоненты classic-лиц (авторский лист): `assets/face_components.png` на `/atlas.html`
- блоб настроек: `firmware/include/koto/settings.hpp`
- биты Mocute: `firmware/include/koto/protocol/mocute.hpp`
- кольцо: `firmware/include/koto/hal/led_ring.hpp` (12 светодиодов, на плате дважды)
- версия: `firmware/include/koto/version.hpp`

PNG в `assets/` — готовые RGB-лица (`Name_N.png`) и спрайты OLED HUD (только чёрный и белый). Пакер вшивает лица и игровые тайлы как KPIX, иконки OLED — как 1bpp. Пиксели из Toaster Blaster не пересобираются.

Пересборка таблиц после правок PNG или `assets/emotions.json`:

```bash
python tools/pack_assets.py
```

## ESP32 (не готово)

`platforms/esp32` линкует тот же `koto::App`, но `hal_esp32.cpp` пока не управляет панелью, OLED, BLE и лентой. На визор это прошивать рано.

Когда появится ESP-IDF и живой HAL:

```bash
cd platforms/esp32
idf.py set-target esp32
idf.py build
```

В `sdkconfig.defaults` — 4 МБ флеша и только NimBLE (без Wi‑Fi и Bluedroid). Остаёмся на ESP-IDF: PlatformIO для ESP32 всё равно тянет тот же IDF. Матрица HUB75, I2C и АЦП допишутся вместе с железом.

## Mocute

GAME-репорт, 6 байт: X, Y, hat, кнопки, режим, 0. Кнопки: A B X Y OK ESC SELECT.

| Орган | Действие |
| --- | --- |
| (пропуск заставки) | FaceSet с Neutral (не Settings) |
| X / A / Y | сеты 1 / 2 / 3 |
| MENU / SELECT | коротко: BT → Frame → настройки → BT. Удержание: сразу список настроек |
| B | автосмена |
| ESC | только выход из настроек (в KEY Esc — только Esc, не B) |
| OK | морг, если стик в центре; со смещённым стиком отменяет смену лица |
| Клавиши сима | WASD стик (диагонали WA/WD/AS/DS), Q=ESC, E=MENU, стрелки=X/A/Y/B, пробел=морг |

## Дерево

```
firmware/           ядро: лица, HUD, HID, HAL, конфиг
platforms/sim/      HTTP + браузер
platforms/esp32/    каркас IDF, заглушки HAL
assets/             emotions.json + RGB-лица + чёрно-белые HUD-спрайты (плоско)
tools/              pack_assets.py (PNG → прошивка)
trash/              старый пайплайн Toaster, на всякий случай
tests/              тест ядра без браузера
AGENTS.md           заметки для агента
```

## Авторы

- Kotaz — kotoproto-os
- [diodeface / Toaster Blaster](https://github.com/diodeface/ToasterBlaster) — исходная прошивка визора, на которой основан проект (AGPL-3.0)
- [Chromium Authors](https://chromium.googlesource.com/chromium/src/) — спрайты T-Rex (лицензия BSD)
