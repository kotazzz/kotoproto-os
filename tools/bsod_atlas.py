"""Windows BSOD tiles: authoring sheet + firmware RGB tables.

    python tools/bsod_atlas.py

PNG is the source after the first run. Missing PNG is created with the
white ":(" header and a 64×2 bar (black labels). Pack embeds the tiles
into firmware/src/assets/bsod.cpp and paints the 32×32 game_bsod icon
into games_icons.png.
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

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
ATLAS_PNG = ASSETS / "bsod_sprites.png"
ATLAS_JSON = ASSETS / "bsod_sprites.json"
GAMES_PNG = ASSETS / "games_icons.png"
GEN_CPP = ROOT / "firmware" / "src" / "assets" / "bsod.cpp"

GAP = 1
MARGIN = 1
LABEL_W = 48
ICON = 32
GAMES_TILE_X = LABEL_W + GAP
SAD_W = 18
SAD_H = 12
BAR_W = 64
BAR_H = 2

INK = (0, 0, 0)
LIT = (255, 255, 255)

TILES = (
    ("sad", "SAD", SAD_W, SAD_H),
    ("bar", "BAR", BAR_W, BAR_H),
)

# Windows 10 BSOD header ":(" — white on black, tinted by the visor blue fill.
SAD = (
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
    "                  ",
    "                  ",
)


def ident(name: str) -> str:
    return "kBsod_" + re.sub(r"[^A-Za-z0-9_]", "_", name)


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


def paint_sad(buf: bytearray, w: int, h: int) -> None:
    for y, row in enumerate(SAD):
        if y >= h:
            break
        for x, ch in enumerate(row):
            if ch == "#":
                set_px(buf, w, h, x, y, LIT)


def paint_bar(buf: bytearray, w: int, h: int) -> None:
    for y in range(h):
        for x in range(w):
            set_px(buf, w, h, x, y, LIT)


def make_tile(name: str, w: int, h: int) -> bytes:
    buf = bytearray(w * h * 3)
    if name == "sad":
        paint_sad(buf, w, h)
    elif name == "bar":
        paint_bar(buf, w, h)
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


def paint_icon_sad(rgb: bytearray, width: int, ox: int, oy: int) -> None:
    for y, row in enumerate(SAD):
        for x, ch in enumerate(row):
            if ch == "#":
                atlas_set(rgb, width, ox + 7 + x, oy + 4 + y, ATLAS_LIT)
    for i in range(3):
        y = 22 + i * 3
        for x in range(4, 28):
            atlas_set(rgb, width, ox + x, oy + y, ATLAS_LIT)
            atlas_set(rgb, width, ox + x, oy + y + 1, ATLAS_LIT)


def paint_game_icon() -> None:
    if not GAMES_PNG.exists():
        return
    png_w, png_h, raw = read_png_rgb(GAMES_PNG)
    tile_y0 = (ICON + GAP) * 7
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
    paint_icon_sad(rgb, png_w, ox, oy)
    atlas_text(rgb, png_w, 1, oy + 12, "BSOD")
    write_png_rgb(GAMES_PNG, png_w, png_h, bytes(rgb))
    print(f"painted game_bsod icon in {GAMES_PNG.name}")


def emit_cpp(tiles: list[tuple[str, int, int, bytes]]) -> None:
    chunks = [
        "// GENERATED by tools/bsod_atlas.py — do not edit.",
        '#include "koto/assets/bsod.hpp"',
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
        chunks.append(cpp_bytes(rgb))
        chunks.append("};")
        chunks.append("")
        rows.append(f'    {{"{name}", {w}, {h}, {var}, sizeof({var})}},')
    chunks.append("}  // namespace")
    chunks.append("")
    chunks.append("const BsodSprite kBsodSprites[] = {")
    chunks.append("\n".join(rows))
    chunks.append("};")
    chunks.append(
        "const int kBsodSpriteCount = static_cast<int>(sizeof(kBsodSprites) / sizeof(kBsodSprites[0]));"
    )
    chunks.append(
        """
const BsodSprite* find_bsod_sprite(std::string_view id) {
  for (int i = 0; i < kBsodSpriteCount; ++i) {
    if (id == kBsodSprites[i].id) {
      return &kBsodSprites[i];
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
        "how_to": "python tools/bsod_atlas.py  # PNG → firmware RGB tiles",
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
