# trash — retired Toaster pipeline

Kept so the live tree can show the final face layout: flat `assets/*.png` + `assets/emotions.json`.

This folder is **not** an input to the firmware. Do not wire these files back into `tools/pack_assets.py`. Live codegen is flat `assets/` PNG + `emotions.json` → `emotions.cpp` / `bitmaps.cpp`.

| Path | What it was |
| --- | --- |
| `toaster-pipeline/assets-face/` | MAX7219 parts + assembled 64×32 sequences (1bpp / paletted) |
| `toaster-pipeline/hud/` + `system/` | original OLED HUD / visor / digit bitmaps |
| `toaster-pipeline/catalog.json` | old sequence catalog |
| `toaster-pipeline/import_toasterblaster.py` | import from upstream Toaster Blaster |
| `toaster-pipeline/adapt_p3_face.py` | scale parts to P3 + emit `face_p3.cpp` |
| `toaster-pipeline/tools-face/` | pixel editor |
| `toaster-pipeline/firmware/face_p3.*` | last 1bpp C++ tables |

Live faces to edit by hand: RGB `assets/Name_N.png`. HUD marks: `assets/visor.png`, `splash1.png`, `logo.png`.
