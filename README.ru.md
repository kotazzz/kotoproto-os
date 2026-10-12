# kotoproto-os

[English](README.md) | **Русский**

Личная прошивка LED-визора для Protogen-шлема. Разработка ведётся сначала в симуляторе: лица выводятся на панель P3 RGB 64×32, HUD — на SSD1306, ввод идёт с карты Mocute. Та же логика управляет кольцом WS2812, ртом по микрофону и датчиком бупа. HAL для ESP32 пока остаётся заглушкой.

> **Роадмап / статус.** Есть тестовая сборка с браузерным интерфейсом. HAL под ESP32 состоит из заглушек. Прошивка ещё не адаптирована и не тестировалась на реальном оборудовании. Плата, панель и распиновка будут указаны позже.

## Лицензия и авторство

kotoproto-os выходит под **GNU Affero General Public License v3.0** (`LICENSE`), как и [Toaster Blaster](https://github.com/diodeface/ToasterBlaster) автора [diodeface](https://github.com/diodeface).

- **Автор этого проекта:** Kotaz (2026). Разработка для личных целей.
- **Исходное вдохновение:** [Toaster Blaster](https://github.com/diodeface/ToasterBlaster). Оттуда взяты последовательности лиц, раскладка HUD, карта Mocute, оверлеи моргания, бупа и полосок рта, а также исходная 1-битная графика, которая теперь хранится как RGB-кадры 64×32. Многие компоненты перенесены и переписаны под панель P3 RGB.
- Это **не** клон прошивки MAX7219 один в один. Логика переписана под RGB-кадр 64×32 и изменена под свои требования. Большая часть нового кода написана с помощью ИИ в [Cursor](https://cursor.com).
- Короткий блок атрибуции находится в `NOTICE`. Если симулятор доступен по сети, AGPL §13 требует отдавать исходники; локально исходник — этот репозиторий.

## Что уже можно гонять (сим)

В симуляторе доступно 25 лиц, представленных RGB-кадрами размером 64×32 пикселя. В атласе хранится левая половина лица, а правая зеркалится в интерфейсе.

Кнопки X, A и Y выбирают три сета лиц. MENU листает экраны BT, Frame и настроек. B включает автосмену.

OLED отображает шапку, имя текущей эмоции, состояние бупа, уровень микрофона и кольцо из восьми лиц. В режиме Auto он также показывает спрайт визора, а меню содержит 14 пунктов.

Работают морг, глитч бупа, радуга при долгом удержании бупа, сдвиг от гироскопа, вентилятор и редкие переходы лиц. Симулятор также поддерживает следующие игры:

* змейку;
* казино;
* динозаврика Chromium;
* Flappy Bird;
* Tetris;
* DVD-заставку;
* Windows BSOD;
* спектрометр;
* Bad Apple!!.

В браузере доступны стик, кнопки и слайдеры микрофона, гироскопа и приближения. Атлас эмоций открывается по адресу http://127.0.0.1:8080/atlas.html.

## Сборка и запуск симулятора

Нужны CMake 3.16+ и компилятор C++17. Python 3 нужен только чтобы упаковать PNG в прошивку.

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

Откройте http://127.0.0.1:8080/. Атлас доступен по адресу http://127.0.0.1:8080/atlas.html.

Перед перелинковкой `.exe` на Windows остановите старый процесс `koto_sim`.

## Конфиг прошивки

Размеры, тайминги и GPIO платы T8 задаются в `firmware/include/koto/config.hpp`. Ключевые параметры приведены ниже.

| Символ | Смысл | По умолчанию |
| --- | --- | --- |
| `kMatrixW` / `kMatrixH` | панель P3 RGB | 64×32 |
| `kOledW` / `kOledH` | HUD SSD1306 | 128×64 |
| `kOledYellowH` | Dual-color 1bpp | 16 жёлтых / остальное голубое |
| `kFaceW` / `kFaceH` | левая половина лица | 64×32 |
| `kEye*` / `kMouth*` | глаз и рот | глаз 32×16 в (0,0); рот 64×16 в (0,16) |
| `kStartupMs` | заставка | 3000 |
| `kBoopTriggerCount` / `kBoopTriggersMax` | гистерезис бупа | 4 / 6 |
| `kTickMs` | период тика | 33 |
| `pins::*` | GPIO LILYGO T8 V1.8 | схема `esp32-tools/docs/t8_visor_wiring.html` |

Наборы лиц, пул для автоматической смены и подписи HUD задаются в `assets/emotions.json`. После упаковки эти данные встраиваются в прошивку как KPIX через `firmware/src/assets/emotions.cpp`.

OLED-спрайты 1bpp берутся из чёрно-белых `visor.png`, `splash1.png` и `logo.png` в `assets/` и собираются в `firmware/src/assets/bitmaps.cpp`. Атласы иконок настроек и игр лежат в `assets/settings_icons.png` и `assets/games_icons.png` и остаются 1bpp.

Bad Apple!! хранится в `assets/badapple.ba1p` как BA1P 64×32 1-бит при 25 fps (патч, маска `0xFE` или сырой кадр). Это чужой PV, а не оригинальная графика.

Исходное изображение `assets/dino_offline.png` преобразуется в набор спрайтов `assets/dino_sprites.png`, который затем встраивается через `firmware/src/assets/dino.cpp`. Остальные игровые листы упаковываются так:

| Игра | Лист | Прошивка |
| --- | --- | --- |
| Казино | `assets/casino_sprites.png` | `firmware/src/assets/casino.cpp` |
| Flappy Bird | `assets/flappy_sprites.png` | `firmware/src/assets/flappy.cpp` |
| Tetris | `assets/tetris_sprites.png` | `firmware/src/assets/tetris.cpp` |
| DVD | `assets/dvd_sprites.png` | `firmware/src/assets/dvd.cpp` |
| BSOD | `assets/bsod_sprites.png` | `firmware/src/assets/bsod.cpp` |
| Спектрометр | `assets/spectrum_sprites.png` | `firmware/src/assets/spectrum.cpp` |

Компоненты classic-лиц для авторства лежат в `assets/face_components.png` и показываются на `/atlas.html`.

Блоб настроек описан в `firmware/include/koto/settings.hpp`. Биты кнопок Mocute задаются в `firmware/include/koto/protocol/mocute.hpp`. Кольцо описано в `firmware/include/koto/hal/led_ring.hpp`: 12 светодиодов, на плате рисуется дважды. Строка версии находится в `firmware/include/koto/version.hpp`.

PNG в `assets/` — готовые RGB-лица (`Name_N.png`) и спрайты OLED HUD (только чёрный и белый). Пакер вшивает лица и игровые тайлы как KPIX, а иконки OLED — как 1bpp. Пиксели из Toaster Blaster не пересобираются.

Пересоберите таблицы после правок PNG или `assets/emotions.json`:

```bash
python tools/pack_assets.py
```

## ESP32 (T8)

Живой HAL реализован через PlatformIO `env:t8` в корне репозитория (`platforms/pio`). IDF-скелет в `platforms/esp32` по-прежнему остаётся заглушкой.

```bash
pio run -e t8 -t upload
```

Когда появятся ESP-IDF и живой HAL:

```bash
cd platforms/esp32
idf.py set-target esp32
idf.py build
```

В `sdkconfig.defaults` включены 4 МБ флеша и только NimBLE, без Wi‑Fi и Bluedroid. Сборку оставляем на ESP-IDF: PlatformIO для ESP32 всё равно тянет тот же IDF. Матрица HUB75, I2C и АЦП допишутся вместе с железом.

## Mocute

Контроллер отдаёт GAME-репорт из 6 байт: X, Y, hat, кнопки, режим и завершающий 0. Кнопки — A, B, X, Y, OK, ESC и SELECT.

| Орган | Действие |
| --- | --- |
| (пропуск заставки) | FaceSet с Neutral, не Settings |
| X / A / Y | сеты 1 / 2 / 3 |
| MENU / SELECT | короткое нажатие циклически переключает BT, Frame и настройки; удержание сразу открывает список настроек |
| B | автосмена |
| ESC | только выход из настроек (в KEY Esc — только Esc, не B) |
| OK | морг, если стик в центре; со смещённым стиком отменяет смену лица |
| Клавиши сима | WASD задаёт стик (диагонали WA/WD/AS/DS), Q=ESC, E=MENU, стрелки=X/A/Y/B, пробел=морг |

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
