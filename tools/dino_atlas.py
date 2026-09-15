"""Extract Chromium T-Rex sprites, authoring sheet, and firmware tables.

    python tools/dino_atlas.py

Source is assets/dino_offline.png (Chromium 1x offline sprite sheet).
Lit pixels become white for the visor. Unique tiles are laid out on a
purple sheet (assets/dino_sprites.png) with black labels. Also paints the
32×32 game_dino icon into games_icons.png.
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
    write_png_rgb,
)

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
SHEET = ASSETS / "dino_offline.png"
ATLAS_PNG = ASSETS / "dino_sprites.png"
ATLAS_JSON = ASSETS / "dino_sprites.json"
GAMES_PNG = ASSETS / "games_icons.png"
GEN_CPP = ROOT / "firmware" / "src" / "assets" / "dino.cpp"

SCALE = 3
CLOUD_SCALE = 2
TREX_X = 848
TREX_Y = 2
LABEL_W = 48
GAP = 1
MARGIN = 1
ICON = 32
TILE_X = LABEL_W + GAP
HORIZON_W = 200
HORIZON_H = 4
HILL_MIN = 3
HILL_GAP = 6

# Chromium LDPI spriteDefinition + Trex.animFrames / Obstacle.types.
SPRITES = (
    ("cloud", "CLOUD", 86, 2, 46, 14),
    ("bird_0", "BIRD0", 134, 2, 46, 40),
    ("bird_1", "BIRD1", 180, 2, 46, 40),
    ("cactus_s1", "S1", 228, 2, 17, 35),
    ("cactus_s2", "S2", 245, 2, 34, 35),
    ("cactus_s3", "S3", 279, 2, 51, 35),
    ("cactus_l1", "L1", 332, 2, 25, 50),
    ("cactus_l2", "L2", 357, 2, 50, 50),
    ("cactus_l3", "L3", 407, 2, 75, 50),
    ("horizon", "GROUND", 2, 54, 600, 12),
    ("trex_wait_0", "WAIT0", TREX_X + 0, TREX_Y, 44, 47),
    ("trex_wait_1", "WAIT1", TREX_X + 44, TREX_Y, 44, 47),
    ("trex_run_0", "RUN0", TREX_X + 88, TREX_Y, 44, 47),
    ("trex_run_1", "RUN1", TREX_X + 132, TREX_Y, 44, 47),
    ("trex_crash", "CRASH", TREX_X + 220, TREX_Y, 44, 47),
    ("trex_duck_0", "DUCK0", TREX_X + 262, TREX_Y, 59, 47),
    ("trex_duck_1", "DUCK1", TREX_X + 321, TREX_Y, 59, 47),
)


def crop(rgb: bytes, width: int, x: int, y: int, w: int, h: int) -> bytes:
    out = bytearray(w * h * 3)
    for row in range(h):
        src = ((y + row) * width + x) * 3
        dst = row * w * 3
        out[dst : dst + w * 3] = rgb[src : src + w * 3]
    return bytes(out)


def lit_white(rgb: bytes) -> bytes:
    out = bytearray(len(rgb))
    for i in range(0, len(rgb), 3):
        r, g, b = rgb[i], rgb[i + 1], rgb[i + 2]
        if r >= 240 and g >= 240 and b >= 240:
            continue
        if r | g | b == 0:
            continue
        out[i] = 255
        out[i + 1] = 255
        out[i + 2] = 255
    return bytes(out)


def pixel_lit(rgb: bytes, width: int, x: int, y: int) -> bool:
    i = (y * width + x) * 3
    return (rgb[i] | rgb[i + 1] | rgb[i + 2]) != 0


def scale_any_lit(rgb: bytes, width: int, height: int, dw: int, dh: int) -> bytes:
    """Keep a dest pixel if any source pixel in its block is lit (preserves 1px strokes)."""
    out = bytearray(dw * dh * 3)
    for row in range(dh):
        y0 = row * height // dh
        y1 = max(y0 + 1, (row + 1) * height // dh)
        for col in range(dw):
            x0 = col * width // dw
            x1 = max(x0 + 1, (col + 1) * width // dw)
            lit = False
            for y in range(y0, y1):
                for x in range(x0, x1):
                    if pixel_lit(rgb, width, x, y):
                        lit = True
                        break
                if lit:
                    break
            if lit:
                di = (row * dw + col) * 3
                out[di] = 255
                out[di + 1] = 255
                out[di + 2] = 255
    return bytes(out)


def scale_nn(rgb: bytes, width: int, height: int, scale: int) -> tuple[int, int, bytes]:
    dw = max(1, width // scale)
    dh = max(1, height // scale)
    return dw, dh, scale_any_lit(rgb, width, height, dw, dh)


def punch_interior_holes(src: bytes, sw: int, sh: int, dest: bytes, dw: int, dh: int) -> bytes:
    """Keep 2×2 enclosed holes (the T-Rex eye) after max-pool fill."""
    out = bytearray(dest)
    punched: set[tuple[int, int]] = set()
    neighbors = ((-1, 0), (-1, 1), (2, 0), (2, 1), (0, -1), (1, -1), (0, 2), (1, 2))
    for y in range(sh - 1):
        for x in range(sw - 1):
            if any(pixel_lit(src, sw, x + dx, y + dy) for dx in (0, 1) for dy in (0, 1)):
                continue
            enclosed = True
            for ox, oy in neighbors:
                xx, yy = x + ox, y + oy
                if not (0 <= xx < sw and 0 <= yy < sh) or not pixel_lit(src, sw, xx, yy):
                    enclosed = False
                    break
            if not enclosed:
                continue
            dx = int((x + 1) * dw / sw + 0.5)
            dy = int((y + 1) * dh / sh + 0.5)
            if 0 <= dx < dw and 0 <= dy < dh:
                punched.add((dx, dy))
    for dx, dy in punched:
        i = (dy * dw + dx) * 3
        out[i] = 0
        out[i + 1] = 0
        out[i + 2] = 0
    return bytes(out)


def make_horizon(ink: bytes, width: int, height: int) -> tuple[int, int, bytes]:
    """Solid 2px ground line plus a few 4–6px hills — not the Chromium dirt specks."""
    dw, dh = HORIZON_W, HORIZON_H
    out = bytearray(dw * dh * 3)
    white = (255, 255, 255)
    for x in range(dw):
        atlas_set(out, dw, x, dh - 2, white)
        atlas_set(out, dw, x, dh - 1, white)

    line = 0
    for y in range(height):
        n = sum(1 for x in range(width) if pixel_lit(ink, width, x, y))
        if n > width // 2:
            line = y
            break

    visited = [[False] * width for _ in range(height)]
    placed: list[int] = []

    def overlaps(cx: int) -> bool:
        return any(abs(cx - px) < HILL_GAP for px in placed)

    def stamp_hill(cx: int, wide: bool) -> None:
        hw = 6 if wide else 4
        x0 = max(0, min(dw - hw, cx - hw // 2))
        peak = (1, 2, 2, 1) if not wide else (1, 2, 3, 3, 2, 1)
        for col, rise in enumerate(peak):
            x = x0 + col
            if x >= dw:
                break
            for r in range(rise):
                yy = dh - 2 - 1 - r
                if yy >= 0:
                    atlas_set(out, dw, x, yy, white)

    for y in range(line + 1, height):
        for x in range(width):
            if visited[y][x] or not pixel_lit(ink, width, x, y):
                continue
            stack = [(x, y)]
            visited[y][x] = True
            cells = []
            while stack:
                cx, cy = stack.pop()
                cells.append((cx, cy))
                for nx in range(cx - 2, cx + 3):
                    for ny in range(cy - 1, cy + 2):
                        if 0 <= nx < width and line < ny < height:
                            if not visited[ny][nx] and pixel_lit(ink, width, nx, ny):
                                visited[ny][nx] = True
                                stack.append((nx, ny))
            if len(cells) < HILL_MIN:
                continue
            xs = [c[0] for c in cells]
            cx = (min(xs) + max(xs)) * dw // (2 * width)
            if overlaps(cx):
                continue
            stamp_hill(cx, wide=len(cells) >= 4)
            placed.append(cx)
    return dw, dh, bytes(out)


def ident(name: str) -> str:
    return "kDino_" + re.sub(r"[^A-Za-z0-9_]", "_", name)


def blit_tile(
    dest: bytearray,
    dest_w: int,
    dx: int,
    dy: int,
    src: bytes,
    src_w: int,
    src_h: int,
) -> None:
    for row in range(src_h):
        for col in range(src_w):
            i = (row * src_w + col) * 3
            color = (src[i], src[i + 1], src[i + 2])
            if color[0] | color[1] | color[2]:
                atlas_set(dest, dest_w, dx + col, dy + row, color)
            else:
                atlas_set(dest, dest_w, dx + col, dy + row, ATLAS_INK)


def write_sheet(tiles: list[tuple[str, str, int, int, bytes]]) -> None:
    max_w = max(w for _n, _l, w, _h, _r in tiles)
    sheet_w = MARGIN + LABEL_W + GAP + max_w + MARGIN
    row_heights = [max(7, h) for _n, _l, _w, h, _r in tiles]
    sheet_h = MARGIN * 2 + sum(row_heights) + GAP * (len(tiles) - 1)
    canvas = bytearray(sheet_w * sheet_h * 3)
    atlas_fill(canvas, sheet_w, sheet_h, ATLAS_PURPLE)
    meta_tiles = []
    y = MARGIN
    for (name, label, w, h, rgb), cell_h in zip(tiles, row_heights):
        atlas_text(canvas, sheet_w, MARGIN, y + (cell_h - 7) // 2, label)
        tx = MARGIN + LABEL_W + GAP
        ty = y + (cell_h - h) // 2
        blit_tile(canvas, sheet_w, tx, ty, rgb, w, h)
        meta_tiles.append(
            {
                "id": name,
                "label": label,
                "label_x": MARGIN,
                "label_y": y + (cell_h - 7) // 2,
                "x": tx,
                "y": ty,
                "w": w,
                "h": h,
            }
        )
        y += cell_h + GAP
    write_png_rgb(ATLAS_PNG, sheet_w, sheet_h, bytes(canvas))
    meta = {
        "how_to": "python tools/dino_atlas.py  # Chromium sheet → authoring PNG + firmware",
        "source": SHEET.name,
        "packed": True,
        "grid": {"color": list(ATLAS_PURPLE)},
        "ink": [0, 0, 0],
        "lit": [255, 255, 255],
        "gap": GAP,
        "atlas": {"file": ATLAS_PNG.name, "width": sheet_w, "height": sheet_h},
        "tiles": meta_tiles,
    }
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote {ATLAS_PNG.name} ({len(tiles)} tiles, {sheet_w}x{sheet_h})")


def paint_game_icon(run_rgb: bytes, run_w: int, run_h: int) -> None:
    if not GAMES_PNG.exists():
        return
    png_w, png_h, raw = read_png_rgb(GAMES_PNG)
    tile_y = (ICON + GAP) * 2
    need_h = tile_y + ICON
    need_w = max(png_w, TILE_X + ICON + 2)
    if png_h < need_h or png_w < need_w:
        canvas = bytearray(need_w * need_h * 3)
        atlas_fill(canvas, need_w, need_h, ATLAS_PURPLE)
        for row in range(png_h):
            src = row * png_w * 3
            dst = row * need_w * 3
            canvas[dst : dst + png_w * 3] = raw[src : src + png_w * 3]
        png_w, png_h, raw = need_w, need_h, bytes(canvas)
    rgb = bytearray(raw)
    for row in range(ICON):
        for col in range(ICON):
            atlas_set(rgb, png_w, TILE_X + col, tile_y + row, ATLAS_INK)
    ox = TILE_X + (ICON - run_w) // 2
    oy = tile_y + (ICON - run_h) // 2
    for row in range(run_h):
        for col in range(run_w):
            i = (row * run_w + col) * 3
            if run_rgb[i] | run_rgb[i + 1] | run_rgb[i + 2]:
                atlas_set(rgb, png_w, ox + col, oy + row, ATLAS_LIT)
    atlas_text(rgb, png_w, 1, tile_y + 12, "DINO")
    write_png_rgb(GAMES_PNG, png_w, png_h, bytes(rgb))
    print(f"painted game_dino icon in {GAMES_PNG.name}")


def emit_cpp(tiles: list[tuple[str, str, int, int, bytes]]) -> None:
    chunks = [
        "// GENERATED by tools/dino_atlas.py — do not edit.",
        '#include "koto/assets/dino.hpp"',
        "",
        "namespace koto {",
        "namespace assets {",
        "namespace {",
        "",
    ]
    rows: list[str] = []
    for name, _label, w, h, rgb in tiles:
        var = ident(name)
        chunks.append(f"const std::uint8_t {var}[] = {{")
        chunks.append(cpp_bytes(rgb))
        chunks.append("};")
        chunks.append("")
        rows.append(f'    {{"{name}", {w}, {h}, {var}, sizeof({var})}},')
    chunks.append("}  // namespace")
    chunks.append("")
    chunks.append("const DinoSprite kDinoSprites[] = {")
    chunks.append("\n".join(rows))
    chunks.append("};")
    chunks.append(
        "const int kDinoSpriteCount = static_cast<int>(sizeof(kDinoSprites) / sizeof(kDinoSprites[0]));"
    )
    chunks.append(
        """
const DinoSprite* find_dino_sprite(std::string_view id) {
  for (int i = 0; i < kDinoSpriteCount; ++i) {
    if (id == kDinoSprites[i].id) {
      return &kDinoSprites[i];
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
    if not SHEET.exists():
        raise SystemExit(f"missing {SHEET}")
    sheet_w, sheet_h, sheet = read_png_rgb(SHEET)
    tiles: list[tuple[str, str, int, int, bytes]] = []
    run_rgb = b""
    run_w = run_h = 0
    for name, label, x, y, w, h in SPRITES:
        if x + w > sheet_w or y + h > sheet_h:
            raise SystemExit(f"{name} crop {x},{y} {w}x{h} outside {sheet_w}x{sheet_h}")
        ink = lit_white(crop(sheet, sheet_w, x, y, w, h))
        if name == "horizon":
            dw, dh, scaled = make_horizon(ink, w, h)
        elif name == "cloud":
            dw = max(1, w // CLOUD_SCALE)
            dh = max(1, h // CLOUD_SCALE)
            scaled = scale_any_lit(ink, w, h, dw, dh)
        else:
            dw, dh, scaled = scale_nn(ink, w, h, SCALE)
            if name.startswith("trex_"):
                scaled = punch_interior_holes(ink, w, h, scaled, dw, dh)
        tiles.append((name, label, dw, dh, scaled))
        if name == "trex_run_0":
            run_w = max(1, w // 2)
            run_h = max(1, h // 2)
            run_rgb = punch_interior_holes(
                ink, w, h, scale_any_lit(ink, w, h, run_w, run_h), run_w, run_h
            )
    write_sheet(tiles)
    emit_cpp(tiles)
    if run_rgb:
        paint_game_icon(run_rgb, run_w, run_h)


if __name__ == "__main__":
    pack()
