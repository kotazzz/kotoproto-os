"""Games icon atlas: 32×32 Snake / Casino / Dino / Apple / Flappy / Tetris / DVD / BSOD tiles.

    python tools/games_atlas.py

PNG is the source after the first run. Missing PNG is created with
placeholder icons to redraw. Icon tiles are white on black; labels are
black on the purple gutter. JSON is always extracted from the PNG.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pack_assets import (
    ATLAS_INK,
    ATLAS_LIT,
    ATLAS_PURPLE,
    atlas_fill,
    atlas_set,
    atlas_text,
    read_png_rgb,
    recolor_labels_black,
    write_png_rgb,
)

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
ATLAS_PNG = ASSETS / "games_icons.png"
ATLAS_JSON = ASSETS / "games_icons.json"

SIZE = 32
GAP = 1
LABEL_W = 48
TILE_X = LABEL_W + GAP
TILES = (
    ("game_snake", TILE_X, 0),
    ("game_casino", TILE_X, SIZE + GAP),
    ("game_dino", TILE_X, (SIZE + GAP) * 2),
    ("game_badapple", TILE_X, (SIZE + GAP) * 3),
    ("game_flappy", TILE_X, (SIZE + GAP) * 4),
    ("game_tetris", TILE_X, (SIZE + GAP) * 5),
    ("game_dvd", TILE_X, (SIZE + GAP) * 6),
    ("game_bsod", TILE_X, (SIZE + GAP) * 7),
)


def fill_rect(
    rgb: bytearray,
    width: int,
    x: int,
    y: int,
    w: int,
    h: int,
    color: tuple[int, int, int],
) -> None:
    for row in range(h):
        for col in range(w):
            atlas_set(rgb, width, x + col, y + row, color)


def blit_snake(rgb: bytearray, width: int, ox: int, oy: int) -> None:
    for t in range(52):
        a = t * 0.28
        x = ox + 3 + t * 22 // 52
        y = oy + 16 + int(math.sin(a) * 9)
        for dx in range(3):
            for dy in range(3):
                atlas_set(rgb, width, x + dx, y + dy, ATLAS_LIT)
    for dx in range(7):
        for dy in range(7):
            atlas_set(rgb, width, ox + 23 + dx, oy + 12 + dy, ATLAS_LIT)
    atlas_set(rgb, width, ox + 29, oy + 13, ATLAS_INK)
    for dx in range(5):
        for dy in range(5):
            atlas_set(rgb, width, ox + 24 + dx, oy + 3 + dy, ATLAS_LIT)
    for dx in range(2):
        for dy in range(3):
            atlas_set(rgb, width, ox + 26 + dx, oy + 1 + dy, ATLAS_LIT)


def blit_casino(rgb: bytearray, width: int, ox: int, oy: int) -> None:
    for i in range(3):
        x = ox + 2 + i * 10
        for col in range(9):
            atlas_set(rgb, width, x + col, oy + 4, ATLAS_LIT)
            atlas_set(rgb, width, x + col, oy + 27, ATLAS_LIT)
        for row in range(24):
            atlas_set(rgb, width, x, oy + 4 + row, ATLAS_LIT)
            atlas_set(rgb, width, x + 8, oy + 4 + row, ATLAS_LIT)
        for dx in range(5):
            atlas_set(rgb, width, x + 2 + dx, oy + 7, ATLAS_LIT)
            atlas_set(rgb, width, x + 2 + dx, oy + 8, ATLAS_LIT)
            atlas_set(rgb, width, x + 2 + dx, oy + 17, ATLAS_LIT)
            atlas_set(rgb, width, x + 2 + dx, oy + 18, ATLAS_LIT)
        for dy in range(10):
            atlas_set(rgb, width, x + 5, oy + 9 + dy, ATLAS_LIT)
            atlas_set(rgb, width, x + 6, oy + 9 + dy, ATLAS_LIT)


def blit_apple(rgb: bytearray, width: int, ox: int, oy: int) -> None:
    for dx in range(3):
        atlas_set(rgb, width, ox + 16 + dx, oy + 2, ATLAS_LIT)
        atlas_set(rgb, width, ox + 17 + dx, oy + 3, ATLAS_LIT)
    for dx in range(5):
        atlas_set(rgb, width, ox + 18 + dx, oy + 4, ATLAS_LIT)
    body = (
        (8, 6, 16),
        (6, 7, 20),
        (5, 8, 22),
        (4, 9, 24),
        (4, 10, 24),
        (3, 11, 26),
        (3, 12, 26),
        (3, 13, 26),
        (3, 14, 26),
        (3, 15, 26),
        (3, 16, 26),
        (3, 17, 26),
        (4, 18, 24),
        (4, 19, 24),
        (5, 20, 22),
        (6, 21, 20),
        (8, 22, 16),
        (10, 23, 12),
        (12, 24, 8),
    )
    for x0, row, w in body:
        for dx in range(w):
            atlas_set(rgb, width, ox + x0 + dx, oy + row, ATLAS_LIT)
    atlas_set(rgb, width, ox + 11, oy + 10, ATLAS_INK)
    atlas_set(rgb, width, ox + 12, oy + 11, ATLAS_INK)


def blit_flappy(rgb: bytearray, width: int, ox: int, oy: int) -> None:
    for x0, top, gap in ((4, 11, 10), (18, 9, 10)):
        for col in range(8):
            atlas_set(rgb, width, ox + x0 + col, oy + top, ATLAS_LIT)
            atlas_set(rgb, width, ox + x0 + col, oy + top + gap, ATLAS_LIT)
        for row in range(top + 1):
            for col in range(6):
                atlas_set(rgb, width, ox + x0 + 1 + col, oy + row, ATLAS_LIT)
        for row in range(oy + top + gap, oy + SIZE):
            for col in range(6):
                atlas_set(rgb, width, ox + x0 + 1 + col, row, ATLAS_LIT)
    body = (
        (10, 12, 8),
        (9, 13, 12),
        (8, 14, 14),
        (8, 15, 16),
        (9, 16, 14),
        (11, 17, 10),
        (12, 18, 6),
    )
    for x0, row, w in body:
        for dx in range(w):
            atlas_set(rgb, width, ox + x0 + dx, oy + row, ATLAS_LIT)
    atlas_set(rgb, width, ox + 12, oy + 14, ATLAS_INK)
    atlas_set(rgb, width, ox + 22, oy + 15, ATLAS_LIT)
    atlas_set(rgb, width, ox + 23, oy + 15, ATLAS_LIT)
    atlas_set(rgb, width, ox + 22, oy + 16, ATLAS_LIT)


def blit_tetris(rgb: bytearray, width: int, ox: int, oy: int) -> None:
    for y in range(6, 30):
        atlas_set(rgb, width, ox + 4, oy + y, ATLAS_LIT)
        atlas_set(rgb, width, ox + 27, oy + y, ATLAS_LIT)
    for x in range(4, 28):
        atlas_set(rgb, width, ox + x, oy + 29, ATLAS_LIT)
    for x0, y0 in ((6, 21), (12, 21), (18, 21), (12, 15), (18, 15), (18, 9)):
        for dy in range(6):
            for dx in range(6):
                atlas_set(rgb, width, ox + x0 + dx, oy + y0 + dy, ATLAS_LIT)


def blit_dvd(rgb: bytearray, width: int, ox: int, oy: int) -> None:
    for y in range(8, 25):
        for x in range(2, 30):
            nx = (x + 0.5 - 16.0) / 14.0
            ny = (y + 0.5 - 16.0) / 8.0
            if nx * nx + ny * ny <= 1.0:
                atlas_set(rgb, width, ox + x, oy + y, ATLAS_LIT)
    letters = (
        (8, ((1, 1, 1), (1, 0, 1), (1, 0, 1), (1, 0, 1), (1, 1, 1))),
        (14, ((1, 0, 1), (1, 0, 1), (1, 0, 1), (0, 1, 0), (0, 1, 0))),
        (20, ((1, 1, 1), (1, 0, 1), (1, 0, 1), (1, 0, 1), (1, 1, 1))),
    )
    for gx, glyph in letters:
        for row, line in enumerate(glyph):
            for col, bit in enumerate(line):
                if bit:
                    atlas_set(rgb, width, ox + gx + col, oy + 13 + row, ATLAS_INK)
    for x in range(10, 22):
        atlas_set(rgb, width, ox + x, oy + 19, ATLAS_INK)


def blit_bsod(rgb: bytearray, width: int, ox: int, oy: int) -> None:
    sad = (
        "###           ####",
        "###          #   #",
        "###         #     ",
        "            #     ",
        "            #     ",
        "###         #     ",
        "###         #     ",
        "###         #     ",
        "             #   #",
        "              ####",
    )
    for y, row in enumerate(sad):
        for x, ch in enumerate(row):
            if ch == "#":
                atlas_set(rgb, width, ox + 7 + x, oy + 4 + y, ATLAS_LIT)
    for i in range(3):
        y = 22 + i * 3
        for x in range(4, 28):
            atlas_set(rgb, width, ox + x, oy + y, ATLAS_LIT)
            atlas_set(rgb, width, ox + x, oy + y + 1, ATLAS_LIT)


def ensure_canvas() -> None:
    need_h = SIZE * 8 + GAP * 7
    need_w = TILE_X + SIZE + 2
    if not ATLAS_PNG.exists():
        return
    png_w, png_h, raw = read_png_rgb(ATLAS_PNG)
    if png_h >= need_h and png_w >= need_w:
        return
    canvas = bytearray(need_w * need_h * 3)
    atlas_fill(canvas, need_w, need_h, ATLAS_PURPLE)
    for row in range(png_h):
        src = row * png_w * 3
        dst = row * need_w * 3
        canvas[dst : dst + png_w * 3] = raw[src : src + png_w * 3]
    write_png_rgb(ATLAS_PNG, need_w, need_h, bytes(canvas))
    print(f"grew {ATLAS_PNG.name} to {need_w}x{need_h}")


def init_png() -> None:
    if ATLAS_PNG.exists():
        ensure_canvas()
        return
    width = TILE_X + SIZE + 2
    height = SIZE * 8 + GAP * 7
    rgb = bytearray(width * height * 3)
    atlas_fill(rgb, width, height, ATLAS_PURPLE)
    for _, ox, oy in TILES:
        fill_rect(rgb, width, ox, oy, SIZE, SIZE, ATLAS_INK)
    atlas_text(rgb, width, 1, 12, "SNAKE")
    blit_snake(rgb, width, TILE_X, 0)
    atlas_text(rgb, width, 1, SIZE + GAP + 12, "CASINO")
    blit_casino(rgb, width, TILE_X, SIZE + GAP)
    atlas_text(rgb, width, 1, (SIZE + GAP) * 2 + 12, "DINO")
    atlas_text(rgb, width, 1, (SIZE + GAP) * 3 + 12, "APPLE")
    blit_apple(rgb, width, TILE_X, (SIZE + GAP) * 3)
    atlas_text(rgb, width, 1, (SIZE + GAP) * 4 + 12, "FLAPPY")
    blit_flappy(rgb, width, TILE_X, (SIZE + GAP) * 4)
    atlas_text(rgb, width, 1, (SIZE + GAP) * 5 + 12, "TETRIS")
    blit_tetris(rgb, width, TILE_X, (SIZE + GAP) * 5)
    atlas_text(rgb, width, 1, (SIZE + GAP) * 6 + 12, "DVD")
    blit_dvd(rgb, width, TILE_X, (SIZE + GAP) * 6)
    atlas_text(rgb, width, 1, (SIZE + GAP) * 7 + 12, "BSOD")
    blit_bsod(rgb, width, TILE_X, (SIZE + GAP) * 7)
    write_png_rgb(ATLAS_PNG, width, height, bytes(rgb))
    print(f"created {ATLAS_PNG}")


def paint_apple_if_empty() -> None:
    png_w, png_h, raw = read_png_rgb(ATLAS_PNG)
    ox, oy = TILE_X, (SIZE + GAP) * 3
    if oy + SIZE > png_h:
        return
    lit = False
    for row in range(SIZE):
        for col in range(SIZE):
            i = ((oy + row) * png_w + (ox + col)) * 3
            if (raw[i], raw[i + 1], raw[i + 2]) == ATLAS_LIT:
                lit = True
                break
        if lit:
            break
    if lit:
        return
    rgb = bytearray(raw)
    fill_rect(rgb, png_w, ox, oy, SIZE, SIZE, ATLAS_INK)
    blit_apple(rgb, png_w, ox, oy)
    atlas_text(rgb, png_w, 1, oy + 12, "APPLE")
    write_png_rgb(ATLAS_PNG, png_w, png_h, bytes(rgb))
    print(f"painted game_badapple icon in {ATLAS_PNG.name}")


def paint_flappy_if_empty() -> None:
    png_w, png_h, raw = read_png_rgb(ATLAS_PNG)
    ox, oy = TILE_X, (SIZE + GAP) * 4
    if oy + SIZE > png_h:
        return
    lit = False
    for row in range(SIZE):
        for col in range(SIZE):
            i = ((oy + row) * png_w + (ox + col)) * 3
            if (raw[i], raw[i + 1], raw[i + 2]) == ATLAS_LIT:
                lit = True
                break
        if lit:
            break
    if lit:
        return
    rgb = bytearray(raw)
    fill_rect(rgb, png_w, ox, oy, SIZE, SIZE, ATLAS_INK)
    blit_flappy(rgb, png_w, ox, oy)
    atlas_text(rgb, png_w, 1, oy + 12, "FLAPPY")
    write_png_rgb(ATLAS_PNG, png_w, png_h, bytes(rgb))
    print(f"painted game_flappy icon in {ATLAS_PNG.name}")


def paint_tetris_if_empty() -> None:
    png_w, png_h, raw = read_png_rgb(ATLAS_PNG)
    ox, oy = TILE_X, (SIZE + GAP) * 5
    if oy + SIZE > png_h:
        return
    lit = False
    for row in range(SIZE):
        for col in range(SIZE):
            i = ((oy + row) * png_w + (ox + col)) * 3
            if (raw[i], raw[i + 1], raw[i + 2]) == ATLAS_LIT:
                lit = True
                break
        if lit:
            break
    if lit:
        return
    rgb = bytearray(raw)
    fill_rect(rgb, png_w, ox, oy, SIZE, SIZE, ATLAS_INK)
    blit_tetris(rgb, png_w, ox, oy)
    atlas_text(rgb, png_w, 1, oy + 12, "TETRIS")
    write_png_rgb(ATLAS_PNG, png_w, png_h, bytes(rgb))
    print(f"painted game_tetris icon in {ATLAS_PNG.name}")


def paint_dvd_if_empty() -> None:
    png_w, png_h, raw = read_png_rgb(ATLAS_PNG)
    ox, oy = TILE_X, (SIZE + GAP) * 6
    if oy + SIZE > png_h:
        return
    lit = False
    for row in range(SIZE):
        for col in range(SIZE):
            i = ((oy + row) * png_w + (ox + col)) * 3
            if (raw[i], raw[i + 1], raw[i + 2]) == ATLAS_LIT:
                lit = True
                break
        if lit:
            break
    if lit:
        return
    rgb = bytearray(raw)
    fill_rect(rgb, png_w, ox, oy, SIZE, SIZE, ATLAS_INK)
    blit_dvd(rgb, png_w, ox, oy)
    atlas_text(rgb, png_w, 1, oy + 12, "DVD")
    write_png_rgb(ATLAS_PNG, png_w, png_h, bytes(rgb))
    print(f"painted game_dvd icon in {ATLAS_PNG.name}")


def paint_bsod_if_empty() -> None:
    png_w, png_h, raw = read_png_rgb(ATLAS_PNG)
    ox, oy = TILE_X, (SIZE + GAP) * 7
    if oy + SIZE > png_h:
        return
    lit = False
    for row in range(SIZE):
        for col in range(SIZE):
            i = ((oy + row) * png_w + (ox + col)) * 3
            if (raw[i], raw[i + 1], raw[i + 2]) == ATLAS_LIT:
                lit = True
                break
        if lit:
            break
    if lit:
        return
    rgb = bytearray(raw)
    fill_rect(rgb, png_w, ox, oy, SIZE, SIZE, ATLAS_INK)
    blit_bsod(rgb, png_w, ox, oy)
    atlas_text(rgb, png_w, 1, oy + 12, "BSOD")
    write_png_rgb(ATLAS_PNG, png_w, png_h, bytes(rgb))
    print(f"painted game_bsod icon in {ATLAS_PNG.name}")


def paint_tile_ink() -> None:
    png_w, png_h, raw = read_png_rgb(ATLAS_PNG)
    rgb = bytearray(raw)
    changed = 0
    for _, ox, oy in TILES:
        for row in range(SIZE):
            for col in range(SIZE):
                i = ((oy + row) * png_w + (ox + col)) * 3
                pix = (rgb[i], rgb[i + 1], rgb[i + 2])
                if pix != ATLAS_LIT and pix != ATLAS_INK:
                    rgb[i], rgb[i + 1], rgb[i + 2] = ATLAS_INK
                    changed += 1
    if changed:
        write_png_rgb(ATLAS_PNG, png_w, png_h, bytes(rgb))
        print(f"filled {changed} tile pixels with black in {ATLAS_PNG.name}")


def extract_bits(rgb: bytes, width: int, x: int, y: int, w: int, h: int) -> list[str]:
    rows: list[str] = []
    for row in range(h):
        cells: list[str] = []
        for col in range(w):
            i = ((y + row) * width + (x + col)) * 3
            lit = (rgb[i], rgb[i + 1], rgb[i + 2]) == ATLAS_LIT
            cells.append("X" if lit else ".")
        rows.append("".join(cells))
    return rows


def unpack() -> None:
    png_w, png_h, raw = read_png_rgb(ATLAS_PNG)
    rgb = bytearray(raw)
    changed = recolor_labels_black(
        rgb, png_w, png_h, [(ox, oy, SIZE, SIZE) for _, ox, oy in TILES]
    )
    if changed:
        write_png_rgb(ATLAS_PNG, png_w, png_h, bytes(rgb))
        print(f"recolored {changed} label pixels black in {ATLAS_PNG.name}")
    tiles = [
        {
            "id": "game_snake",
            "kind": "game",
            "label": "SNAKE",
            "label_x": 1,
            "label_y": 12,
            "x": TILE_X,
            "y": 0,
            "w": SIZE,
            "h": SIZE,
        },
        {
            "id": "game_casino",
            "kind": "game",
            "label": "CASINO",
            "label_x": 1,
            "label_y": SIZE + GAP + 12,
            "x": TILE_X,
            "y": SIZE + GAP,
            "w": SIZE,
            "h": SIZE,
        },
        {
            "id": "game_dino",
            "kind": "game",
            "label": "DINO",
            "label_x": 1,
            "label_y": (SIZE + GAP) * 2 + 12,
            "x": TILE_X,
            "y": (SIZE + GAP) * 2,
            "w": SIZE,
            "h": SIZE,
        },
        {
            "id": "game_badapple",
            "kind": "game",
            "label": "APPLE",
            "label_x": 1,
            "label_y": (SIZE + GAP) * 3 + 12,
            "x": TILE_X,
            "y": (SIZE + GAP) * 3,
            "w": SIZE,
            "h": SIZE,
        },
        {
            "id": "game_flappy",
            "kind": "game",
            "label": "FLAPPY",
            "label_x": 1,
            "label_y": (SIZE + GAP) * 4 + 12,
            "x": TILE_X,
            "y": (SIZE + GAP) * 4,
            "w": SIZE,
            "h": SIZE,
        },
        {
            "id": "game_tetris",
            "kind": "game",
            "label": "TETRIS",
            "label_x": 1,
            "label_y": (SIZE + GAP) * 5 + 12,
            "x": TILE_X,
            "y": (SIZE + GAP) * 5,
            "w": SIZE,
            "h": SIZE,
        },
        {
            "id": "game_dvd",
            "kind": "game",
            "label": "DVD",
            "label_x": 1,
            "label_y": (SIZE + GAP) * 6 + 12,
            "x": TILE_X,
            "y": (SIZE + GAP) * 6,
            "w": SIZE,
            "h": SIZE,
        },
        {
            "id": "game_bsod",
            "kind": "game",
            "label": "BSOD",
            "label_x": 1,
            "label_y": (SIZE + GAP) * 7 + 12,
            "x": TILE_X,
            "y": (SIZE + GAP) * 7,
            "w": SIZE,
            "h": SIZE,
        },
    ]
    for tile in tiles:
        bits = extract_bits(rgb, png_w, tile["x"], tile["y"], tile["w"], tile["h"])
        tile["bits"] = bits
        tile["empty"] = not any("X" in row for row in bits)
    meta = {
        "how_to": "python tools/games_atlas.py  # PNG → JSON; creates PNG if missing",
        "packed": True,
        "grid": {"color": list(ATLAS_PURPLE)},
        "ink": [0, 0, 0],
        "lit": [255, 255, 255],
        "gap": GAP,
        "atlas": {"file": ATLAS_PNG.name, "width": png_w, "height": png_h},
        "groups": [{"id": "games", "size": SIZE, "count": 8}],
        "tiles": tiles,
    }
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"updated {ATLAS_JSON.name} from {ATLAS_PNG.name}")


if __name__ == "__main__":
    init_png()
    paint_apple_if_empty()
    paint_flappy_if_empty()
    paint_tetris_if_empty()
    paint_dvd_if_empty()
    paint_bsod_if_empty()
    paint_tile_ink()
    unpack()
