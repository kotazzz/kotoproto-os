"""Flappy Bird tiles: authoring sheet + firmware RGB tables.

    python tools/flappy_atlas.py

PNG is the source after the first run. Missing PNG is created with the
unique bird / pipe / ground / cloud tiles (RGB on black, black labels).
Pack embeds the tiles into firmware/src/assets/flappy.cpp and paints the
32×32 game_flappy icon into games_icons.png.
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
ATLAS_PNG = ASSETS / "flappy_sprites.png"
ATLAS_JSON = ASSETS / "flappy_sprites.json"
GAMES_PNG = ASSETS / "games_icons.png"
GEN_CPP = ROOT / "firmware" / "src" / "assets" / "flappy.cpp"

GAP = 1
MARGIN = 1
LABEL_W = 48
ICON = 32
GAMES_TILE_X = LABEL_W + GAP

# Unique visor tiles. Black is transparent at blit time.
TILES = (
    ("bird_0", "BIRD0", 12, 8),
    ("bird_1", "BIRD1", 12, 8),
    ("bird_2", "BIRD2", 12, 8),
    ("pipe_body", "BODY", 10, 4),
    ("pipe_cap_top", "CAPT", 12, 5),
    ("pipe_cap_bot", "CAPB", 12, 5),
    ("ground", "GROUND", 16, 5),
    ("cloud", "CLOUD", 16, 6),
)

INK = (0, 0, 0)
YELLOW = (250, 196, 36)
BELLY = (255, 236, 176)
EYE = (255, 255, 255)
PUPIL = (24, 20, 16)
BEAK = (236, 118, 24)
BEAK_D = (196, 78, 12)
WING = (214, 148, 28)
WING_D = (168, 104, 16)
PIPE_D = (32, 86, 16)
PIPE_M = (86, 186, 40)
PIPE_H = (168, 228, 88)
PIPE_L = (24, 64, 12)
GRASS = (112, 196, 48)
DIRT = (214, 176, 96)
DIRT_D = (176, 132, 64)
CLOUD_C = (236, 240, 255)


def ident(name: str) -> str:
    return "kFlappy_" + re.sub(r"[^A-Za-z0-9_]", "_", name)


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


def fill_rect(
    buf: bytearray,
    w: int,
    h: int,
    x: int,
    y: int,
    rw: int,
    rh: int,
    color: tuple[int, int, int],
) -> None:
    for row in range(rh):
        for col in range(rw):
            set_px(buf, w, h, x + col, y + row, color)


def paint_map(buf: bytearray, w: int, h: int, rows: tuple[str, ...], pal: dict[str, tuple[int, int, int]]) -> None:
    for y, row in enumerate(rows):
        for x, ch in enumerate(row):
            set_px(buf, w, h, x, y, pal[ch])


def paint_bird(buf: bytearray, w: int, h: int, wing: str) -> None:
    pal = {
        ".": INK,
        "Y": YELLOW,
        "W": BELLY,
        "E": EYE,
        "K": PUPIL,
        "O": BEAK,
        "D": BEAK_D,
        "N": WING,
        "M": WING_D,
    }
    if wing == "up":
        rows = (
            "..MNYYYY....",
            ".MNYYYYYY...",
            "..WEKYYYYO..",
            ".YYYYYYYYOD.",
            "YYYYYYYYYOD.",
            ".WWYYYYYY...",
            "..WWWW......",
            "............",
        )
    elif wing == "down":
        rows = (
            "....YYYY....",
            "...YYYYYY...",
            "..WEKYYYYO..",
            ".YYYYYYYYOD.",
            "YYYYYYYYYOD.",
            ".WWNYYYYY...",
            "..WMNNN.....",
            "...MM.......",
        )
    else:
        rows = (
            "....YYYY....",
            "...YYYYYY...",
            "..WEKYYYYO..",
            ".YYYYYYYYOD.",
            "YYYYYYYYYOD.",
            ".WWNYYYYY...",
            "..WWNNM.....",
            "...NN.......",
        )
    paint_map(buf, w, h, rows, pal)


def paint_pipe_body(buf: bytearray, w: int, h: int) -> None:
    pal = {"D": PIPE_D, "M": PIPE_M, "H": PIPE_H, "L": PIPE_L}
    rows = (
        "DMMMMMMHHL",
        "DMMMMMMHHL",
        "DMMMMMMHHL",
        "DMMMMMMHHL",
    )
    paint_map(buf, w, h, rows, pal)


def paint_pipe_cap(buf: bytearray, w: int, h: int, top: bool) -> None:
    pal = {".": INK, "D": PIPE_D, "M": PIPE_M, "H": PIPE_H, "L": PIPE_L}
    lip = (
        "DMMMMMMMMHHL",
        "DMMMMMMMMHHL",
        ".DMMMMMMHHL.",
        ".DMMMMMMHHL.",
        ".DMMMMMMHHL.",
    )
    rows = lip if not top else tuple(reversed(lip))
    paint_map(buf, w, h, rows, pal)


def paint_ground(buf: bytearray, w: int, h: int) -> None:
    fill_rect(buf, w, h, 0, 0, w, 1, GRASS)
    fill_rect(buf, w, h, 0, 1, w, 1, (88, 156, 36))
    fill_rect(buf, w, h, 0, 2, w, 2, DIRT)
    fill_rect(buf, w, h, 0, 4, w, 1, DIRT_D)
    for x in (2, 7, 11, 14):
        set_px(buf, w, h, x, 3, DIRT_D)


def paint_cloud(buf: bytearray, w: int, h: int) -> None:
    pal = {".": INK, "C": CLOUD_C}
    rows = (
        "....CCCC........",
        "..CCCCCCCC......",
        "CCCCCCCCCCCCC...",
        ".CCCCCCCCCCCCCC.",
        "...CCCCCCCCCC...",
        "......CCCC......",
    )
    paint_map(buf, w, h, rows, pal)


PAINT = {
    "bird_0": lambda buf, w, h: paint_bird(buf, w, h, "up"),
    "bird_1": lambda buf, w, h: paint_bird(buf, w, h, "mid"),
    "bird_2": lambda buf, w, h: paint_bird(buf, w, h, "down"),
    "pipe_body": paint_pipe_body,
    "pipe_cap_top": lambda buf, w, h: paint_pipe_cap(buf, w, h, True),
    "pipe_cap_bot": lambda buf, w, h: paint_pipe_cap(buf, w, h, False),
    "ground": paint_ground,
    "cloud": paint_cloud,
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
        atlas_text(canvas, sheet_w, MARGIN, oy + (heights[index] - 7) // 2, label)
        fill_rect(canvas, sheet_w, sheet_h, tx, oy, w, h, INK)
        blit_tile(canvas, sheet_w, tx, oy, make_tile(name, w, h), w, h)
    write_png_rgb(ATLAS_PNG, sheet_w, sheet_h, bytes(canvas))
    print(f"created {ATLAS_PNG}")


def paint_game_icon(bird_rgb: bytes, bird_w: int, bird_h: int) -> None:
    if not GAMES_PNG.exists():
        return
    png_w, png_h, raw = read_png_rgb(GAMES_PNG)
    tile_y0 = (ICON + GAP) * 4
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

    for x0, top, bot in ((3, 10, 21), (21, 8, 19)):
        for y in range(0, top):
            for x in range(6):
                lit(x0 + x, y)
        for x in range(8):
            lit(x0 - 1 + x, top)
            lit(x0 - 1 + x, bot)
        for y in range(bot + 1, ICON):
            for x in range(6):
                lit(x0 + x, y)
    scale = 2
    bw = bird_w * scale
    bh = bird_h * scale
    bx = (ICON - bw) // 2
    by = 10
    for row in range(bird_h):
        for col in range(bird_w):
            i = (row * bird_w + col) * 3
            if bird_rgb[i] | bird_rgb[i + 1] | bird_rgb[i + 2]:
                for dy in range(scale):
                    for dx in range(scale):
                        lit(bx + col * scale + dx, by + row * scale + dy)
    atlas_text(rgb, png_w, 1, oy + 12, "FLAPPY")
    write_png_rgb(GAMES_PNG, png_w, png_h, bytes(rgb))
    print(f"painted game_flappy icon in {GAMES_PNG.name}")


def emit_cpp(tiles: list[tuple[str, int, int, bytes]]) -> None:
    chunks = [
        "// GENERATED by tools/flappy_atlas.py — do not edit.",
        '#include "koto/assets/flappy.hpp"',
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
    chunks.append("const FlappySprite kFlappySprites[] = {")
    chunks.append("\n".join(rows))
    chunks.append("};")
    chunks.append(
        "const int kFlappySpriteCount = static_cast<int>(sizeof(kFlappySprites) / sizeof(kFlappySprites[0]));"
    )
    chunks.append(
        """
const FlappySprite* find_flappy_sprite(std::string_view id) {
  for (int i = 0; i < kFlappySpriteCount; ++i) {
    if (id == kFlappySprites[i].id) {
      return &kFlappySprites[i];
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
        "how_to": "python tools/flappy_atlas.py  # PNG → firmware RGB tiles",
        "packed": True,
        "grid": {"color": list(ATLAS_PURPLE)},
        "ink": [0, 0, 0],
        "gap": GAP,
        "atlas": {"file": ATLAS_PNG.name, "width": png_w, "height": png_h},
        "tiles": meta_tiles,
    }
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"updated {ATLAS_JSON.name}")
    bird = next((rgb for name, _w, _h, rgb in packed if name == "bird_1"), packed[0][3])
    bird_w = next(w for name, w, _h, _rgb in packed if name == "bird_1")
    bird_h = next(h for name, _w, h, _rgb in packed if name == "bird_1")
    paint_game_icon(bird, bird_w, bird_h)


if __name__ == "__main__":
    pack()
