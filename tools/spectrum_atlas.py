"""Spectrum analyzer tiles: authoring sheet + firmware RGB tables.

    python tools/spectrum_atlas.py

PNG is the source after the first run. Missing PNG is created with the
peak marker and 64×2 baseline (black labels). Pack embeds the tiles into
firmware/src/assets/spectrum.cpp and paints the 32×32 game_spectrum icon
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
from pix_pack import pack_pix

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
ATLAS_PNG = ASSETS / "spectrum_sprites.png"
ATLAS_JSON = ASSETS / "spectrum_sprites.json"
GAMES_PNG = ASSETS / "games_icons.png"
GEN_CPP = ROOT / "firmware" / "src" / "assets" / "spectrum.cpp"

GAP = 1
MARGIN = 1
LABEL_W = 48
ICON = 32
GAMES_TILE_X = LABEL_W + GAP
PEAK_W = 3
PEAK_H = 2
BASE_W = 64
BASE_H = 2

INK = (0, 0, 0)
LIT = (255, 255, 255)

TILES = (
    ("peak", "PEAK", PEAK_W, PEAK_H),
    ("baseline", "BASE", BASE_W, BASE_H),
)

PEAK = (
    ".X.",
    "X.X",
)

ICON_BARS = (8, 14, 22, 26, 20, 16, 10, 6)


def ident(name: str) -> str:
    return "kSpectrum_" + re.sub(r"[^A-Za-z0-9_]", "_", name)


def tile_y(index: int, heights: list[int]) -> int:
    y = MARGIN
    for i in range(index):
        y += max(7, heights[i]) + GAP
    return y


def paint_peak(buf: bytearray, w: int, h: int) -> None:
    for y, row in enumerate(PEAK):
        if y >= h:
            break
        for x, ch in enumerate(row):
            if ch == "X" and x < w:
                i = (y * w + x) * 3
                buf[i], buf[i + 1], buf[i + 2] = LIT


def paint_baseline(buf: bytearray, w: int, h: int) -> None:
    for x in range(w):
        if h > 1:
            i = (1 * w + x) * 3
            buf[i], buf[i + 1], buf[i + 2] = LIT
        if x % 4 == 0:
            i = x * 3
            buf[i], buf[i + 1], buf[i + 2] = LIT


def make_tile(name: str, w: int, h: int) -> bytes:
    buf = bytearray(w * h * 3)
    if name == "peak":
        paint_peak(buf, w, h)
    elif name == "baseline":
        paint_baseline(buf, w, h)
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


def paint_icon_bars(rgb: bytearray, width: int, ox: int, oy: int) -> None:
    for i, h in enumerate(ICON_BARS):
        x = ox + 2 + i * 4
        y0 = oy + ICON - 2 - h
        for y in range(h):
            for dx in range(3):
                atlas_set(rgb, width, x + dx, y0 + y, ATLAS_LIT)
    for x in range(2, 30):
        atlas_set(rgb, width, ox + x, oy + 30, ATLAS_LIT)


def paint_game_icon() -> None:
    if not GAMES_PNG.exists():
        return
    png_w, png_h, raw = read_png_rgb(GAMES_PNG)
    tile_y0 = (ICON + GAP) * 8
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
    paint_icon_bars(rgb, png_w, ox, oy)
    atlas_text(rgb, png_w, 1, oy + 12, "SPECTRUM")
    write_png_rgb(GAMES_PNG, png_w, png_h, bytes(rgb))
    print(f"painted game_spectrum icon in {GAMES_PNG.name}")


def emit_cpp(tiles: list[tuple[str, int, int, bytes]]) -> None:
    chunks = [
        "// GENERATED by tools/spectrum_atlas.py — do not edit.",
        '#include "koto/assets/spectrum.hpp"',
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
    chunks.append("const SpectrumSprite kSpectrumSprites[] = {")
    chunks.append("\n".join(rows))
    chunks.append("};")
    chunks.append(
        "const int kSpectrumSpriteCount = "
        "static_cast<int>(sizeof(kSpectrumSprites) / sizeof(kSpectrumSprites[0]));"
    )
    chunks.append(
        """
const SpectrumSprite* find_spectrum_sprite(std::string_view id) {
  for (int i = 0; i < kSpectrumSpriteCount; ++i) {
    if (id == kSpectrumSprites[i].id) {
      return &kSpectrumSprites[i];
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
        raw = bytes(rgb)
        packed = []
        for index, (name, _label, w, h) in enumerate(TILES):
            oy = tile_y(index, heights)
            packed.append((name, w, h, crop_tile(raw, png_w, tx, oy, w, h)))
    paint_game_icon()
    emit_cpp(packed)
    meta = {
        "how_to": "python tools/spectrum_atlas.py  # PNG → firmware RGB tiles",
        "packed": True,
        "grid": {"color": list(ATLAS_PURPLE)},
        "ink": [0, 0, 0],
        "lit": [255, 255, 255],
        "gap": GAP,
        "atlas": {"file": ATLAS_PNG.name, "width": png_w, "height": png_h},
        "tiles": meta_tiles,
    }
    for tile, (_n, w, h, rgb) in zip(meta["tiles"], packed):
        bits = []
        for row in range(h):
            cells = []
            for col in range(w):
                i = (row * w + col) * 3
                lit = (rgb[i], rgb[i + 1], rgb[i + 2]) == LIT
                cells.append("X" if lit else ".")
            bits.append("".join(cells))
        tile["bits"] = bits
        tile["empty"] = not any("X" in row for row in bits)
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"updated {ATLAS_JSON.name} from {ATLAS_PNG.name}")


if __name__ == "__main__":
    pack()
