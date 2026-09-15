"""Tetris mino tiles: authoring sheet + firmware RGB tables.

    python tools/tetris_atlas.py

PNG is the source after the first run. Missing PNG is created with the
unique I/O/T/S/Z/J/L minos, ghost and wall (RGB on black, black labels).
Pack embeds the tiles into firmware/src/assets/tetris.cpp and paints the
32×32 game_tetris icon into games_icons.png.
"""

from __future__ import annotations

import json
import re
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
    cpp_bytes,
    read_png_rgb,
    recolor_labels_black,
    write_png_rgb,
)
from pix_pack import pack_pix

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
ATLAS_PNG = ASSETS / "tetris_sprites.png"
ATLAS_JSON = ASSETS / "tetris_sprites.json"
GAMES_PNG = ASSETS / "games_icons.png"
GEN_CPP = ROOT / "firmware" / "src" / "assets" / "tetris.cpp"

GAP = 1
MARGIN = 1
LABEL_W = 48
ICON = 32
GAMES_TILE_X = LABEL_W + GAP

INK = (0, 0, 0)

TILES = (
    ("cell_i", "I", 8, 8),
    ("cell_o", "O", 8, 8),
    ("cell_t", "T", 8, 8),
    ("cell_s", "S", 8, 8),
    ("cell_z", "Z", 8, 8),
    ("cell_j", "J", 8, 8),
    ("cell_l", "L", 8, 8),
    ("ghost", "GHOST", 8, 8),
    ("wall", "WALL", 2, 8),
)


def ident(name: str) -> str:
    return "kTetris_" + re.sub(r"[^A-Za-z0-9_]", "_", name)


def tile_y(index: int, heights: list[int]) -> int:
    y = MARGIN
    for i in range(index):
        y += max(7, heights[i]) + GAP
    return y


def set_px(buf: bytearray, w: int, h: int, x: int, y: int, color: tuple[int, int, int]) -> None:
    if x < 0 or y < 0 or x >= w or y >= h:
        return
    i = (y * w + x) * 3
    buf[i], buf[i + 1], buf[i + 2] = color


def paint_mino(
    buf: bytearray,
    w: int,
    h: int,
    fill: tuple[int, int, int],
    hi: tuple[int, int, int],
    dark: tuple[int, int, int],
    style: str,
) -> None:
    for y in range(h):
        for x in range(w):
            color = fill
            if x == 0 or y == 0:
                color = hi
            if x == w - 1 or y == h - 1:
                color = dark
            if x == 0 and y == h - 1:
                color = fill
            if x == w - 1 and y == 0:
                color = fill
            if style == "bar" and 3 <= y <= 4:
                color = hi
            elif style == "dot" and 3 <= x <= 4 and 3 <= y <= 4:
                color = hi
            elif style == "tee" and y <= 2 and 2 <= x <= 5:
                color = hi
            elif style == "slash" and x + y in (6, 7, 8):
                color = hi
            elif style == "back" and (x - y) in (0, 1):
                color = hi
            elif style == "left" and x <= 2:
                color = hi
            elif style == "right" and x >= 5:
                color = hi
            set_px(buf, w, h, x, y, color)


def paint_ghost(buf: bytearray, w: int, h: int) -> None:
    ink = (80, 96, 120)
    for i in range(w):
        if i % 2 == 0:
            set_px(buf, w, h, i, 0, ink)
            set_px(buf, w, h, i, h - 1, ink)
            set_px(buf, w, h, 0, i, ink)
            set_px(buf, w, h, w - 1, i, ink)


def paint_wall(buf: bytearray, w: int, h: int) -> None:
    a = (36, 52, 72)
    b = (20, 28, 40)
    for y in range(h):
        for x in range(w):
            set_px(buf, w, h, x, y, a if ((y // 2) + x) % 2 == 0 else b)


PAINT = {
    "cell_i": lambda buf, w, h: paint_mino(buf, w, h, (0, 224, 232), (160, 255, 255), (0, 96, 120), "bar"),
    "cell_o": lambda buf, w, h: paint_mino(buf, w, h, (240, 212, 32), (255, 248, 140), (140, 110, 0), "dot"),
    "cell_t": lambda buf, w, h: paint_mino(buf, w, h, (168, 72, 220), (220, 160, 255), (88, 24, 120), "tee"),
    "cell_s": lambda buf, w, h: paint_mino(buf, w, h, (48, 200, 64), (160, 255, 140), (16, 96, 24), "slash"),
    "cell_z": lambda buf, w, h: paint_mino(buf, w, h, (232, 48, 48), (255, 140, 120), (120, 16, 16), "back"),
    "cell_j": lambda buf, w, h: paint_mino(buf, w, h, (48, 88, 232), (140, 180, 255), (16, 32, 120), "left"),
    "cell_l": lambda buf, w, h: paint_mino(buf, w, h, (240, 140, 32), (255, 200, 96), (140, 64, 0), "right"),
    "ghost": paint_ghost,
    "wall": paint_wall,
}


def make_tile(name: str, w: int, h: int) -> bytes:
    buf = bytearray(w * h * 3)
    PAINT[name](buf, w, h)
    return bytes(buf)


def blit_tile(canvas: bytearray, width: int, x: int, y: int, rgb: bytes, tw: int, th: int) -> None:
    for row in range(th):
        for col in range(tw):
            i = (row * tw + col) * 3
            atlas_set(canvas, width, x + col, y + row, (rgb[i], rgb[i + 1], rgb[i + 2]))


def crop_tile(rgb: bytes, width: int, x: int, y: int, w: int, h: int) -> bytes:
    out = bytearray(w * h * 3)
    for row in range(h):
        src = ((y + row) * width + x) * 3
        dst = row * w * 3
        out[dst : dst + w * 3] = rgb[src : src + w * 3]
    return bytes(out)


def sheet_size() -> tuple[int, int, list[int]]:
    heights = [max(7, h) for _n, _l, _w, h in TILES]
    max_w = max(w for _n, _l, w, _h in TILES)
    sheet_w = MARGIN + LABEL_W + GAP + max_w + MARGIN
    sheet_h = MARGIN * 2 + sum(heights) + GAP * (len(TILES) - 1)
    return sheet_w, sheet_h, heights


def init_png() -> None:
    if ATLAS_PNG.exists():
        return
    sheet_w, sheet_h, heights = sheet_size()
    canvas = bytearray(sheet_w * sheet_h * 3)
    atlas_fill(canvas, sheet_w, sheet_h, ATLAS_PURPLE)
    tx = MARGIN + LABEL_W + GAP
    for index, (name, label, w, h) in enumerate(TILES):
        oy = tile_y(index, heights)
        atlas_text(canvas, sheet_w, MARGIN, oy + (max(7, h) - 7) // 2, label)
        for row in range(h):
            for col in range(w):
                atlas_set(canvas, sheet_w, tx + col, oy + row, INK)
        blit_tile(canvas, sheet_w, tx, oy, make_tile(name, w, h), w, h)
    write_png_rgb(ATLAS_PNG, sheet_w, sheet_h, bytes(canvas))
    print(f"created {ATLAS_PNG}")


def paint_game_icon() -> None:
    if not GAMES_PNG.exists():
        return
    png_w, png_h, raw = read_png_rgb(GAMES_PNG)
    tile_y0 = (ICON + GAP) * 5
    need_h = tile_y0 + ICON
    need_w = max(png_w, GAMES_TILE_X + ICON + 2)
    if png_h < need_h or png_w < need_w:
        canvas = bytearray(need_w * need_h * 3)
        atlas_fill(canvas, need_w, need_h, ATLAS_PURPLE)
        for row in range(png_h):
            src = row * png_w * 3
            dst = row * need_w * 3
            canvas[dst : dst + png_w * 3] = raw[src : src + png_w * 3]
        png_w, png_h, raw = need_w, need_h, bytes(canvas)
    rgb = bytearray(raw)
    ox = GAMES_TILE_X
    oy = tile_y0
    for row in range(ICON):
        for col in range(ICON):
            atlas_set(rgb, png_w, ox + col, oy + row, ATLAS_INK)

    def lit(x: int, y: int) -> None:
        if 0 <= x < ICON and 0 <= y < ICON:
            atlas_set(rgb, png_w, ox + x, oy + y, ATLAS_LIT)

    def block(x: int, y: int, n: int) -> None:
        for dy in range(n):
            for dx in range(n):
                lit(x + dx, y + dy)

    # Well walls
    for y in range(4, 30):
        lit(4, y)
        lit(27, y)
    for x in range(4, 28):
        lit(x, 29)
    # Stacked minos
    block(6, 21, 6)
    block(12, 21, 6)
    block(18, 21, 6)
    block(12, 15, 6)
    block(18, 15, 6)
    block(18, 9, 6)
    atlas_text(rgb, png_w, 1, oy + 12, "TETRIS")
    write_png_rgb(GAMES_PNG, png_w, png_h, bytes(rgb))
    print(f"painted game_tetris icon in {GAMES_PNG.name}")


def emit_cpp(tiles: list[tuple[str, int, int, bytes]]) -> None:
    chunks = [
        "// GENERATED by tools/tetris_atlas.py — do not edit.",
        '#include "koto/assets/tetris.hpp"',
        "",
        "namespace koto {",
        "namespace assets {",
        "namespace {",
        "",
    ]
    rows: list[str] = []
    for name, w, h, rgb in tiles:
        var = ident(name)
        chunks.append(f"const std::uint8_t {var}[] = {{")
        chunks.append(cpp_bytes(pack_pix(w, h, rgb)))
        chunks.append("};")
        chunks.append("")
        rows.append(f'    {{"{name}", {w}, {h}, {var}, sizeof({var})}},')
    chunks.append("}  // namespace")
    chunks.append("")
    chunks.append("const TetrisSprite kTetrisSprites[] = {")
    chunks.append("\n".join(rows))
    chunks.append("};")
    chunks.append(
        "const int kTetrisSpriteCount = static_cast<int>(sizeof(kTetrisSprites) / sizeof(kTetrisSprites[0]));"
    )
    chunks.append(
        """
const TetrisSprite* find_tetris_sprite(std::string_view id) {
  for (int i = 0; i < kTetrisSpriteCount; ++i) {
    if (id == kTetrisSprites[i].id) {
      return &kTetrisSprites[i];
    }
  }
  return nullptr;
}

}  // namespace assets
}  // namespace koto
"""
    )
    GEN_CPP.parent.mkdir(parents=True, exist_ok=True)
    GEN_CPP.write_text("\n".join(chunks), encoding="utf-8")
    print(f"wrote {GEN_CPP} ({len(tiles)} sprites)")


def pack() -> None:
    init_png()
    png_w, png_h, raw = read_png_rgb(ATLAS_PNG)
    rgb = bytearray(raw)
    _, _, heights = sheet_size()
    tx = MARGIN + LABEL_W + GAP
    rects = []
    packed: list[tuple[str, int, int, bytes]] = []
    meta_tiles = []
    for index, (name, label, w, h) in enumerate(TILES):
        oy = tile_y(index, heights)
        rects.append((tx, oy, w, h))
        packed.append((name, w, h, crop_tile(bytes(rgb), png_w, tx, oy, w, h)))
        meta_tiles.append(
            {
                "id": name,
                "label": label,
                "label_x": MARGIN,
                "label_y": oy + (heights[index] - 7) // 2,
                "x": tx,
                "y": oy,
                "w": w,
                "h": h,
            }
        )
    changed = recolor_labels_black(rgb, png_w, png_h, rects)
    if changed:
        write_png_rgb(ATLAS_PNG, png_w, png_h, bytes(rgb))
        print(f"recolored {changed} label pixels black in {ATLAS_PNG.name}")
        packed = []
        for index, (name, _label, w, h) in enumerate(TILES):
            oy = tile_y(index, heights)
            packed.append((name, w, h, crop_tile(bytes(rgb), png_w, tx, oy, w, h)))
    emit_cpp(packed)
    meta = {
        "how_to": "python tools/tetris_atlas.py  # PNG → firmware RGB tiles",
        "packed": True,
        "grid": {"color": list(ATLAS_PURPLE)},
        "ink": [0, 0, 0],
        "gap": GAP,
        "atlas": {"file": ATLAS_PNG.name, "width": png_w, "height": png_h},
        "tiles": meta_tiles,
    }
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"updated {ATLAS_JSON.name}")
    paint_game_icon()


if __name__ == "__main__":
    pack()
