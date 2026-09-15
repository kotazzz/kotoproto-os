"""Sync settings icon metadata from the authored PNG.

    python tools/settings_atlas.py

PNG is the source. This writes assets/settings_icons.json. A missing
param_mouth tile is added to the PNG once as a placeholder to redraw.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pack_assets import (
    ATLAS_LIT,
    ATLAS_PURPLE,
    atlas_bits,
    atlas_text,
    read_png_rgb,
    recolor_labels_black,
    write_png_rgb,
)

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
ATLAS_PNG = ASSETS / "settings_icons.png"
ATLAS_JSON = ASSETS / "settings_icons.json"

LIT = (255, 255, 255)

MOUTH_BITS = [
    "..XXX..",
    ".X...X.",
    ".X.X.X.",
    ".X...X.",
    "..X.X..",
    "...X...",
    "..XXX..",
]


def pixel(rgb: bytes, width: int, x: int, y: int) -> tuple[int, int, int]:
    i = (y * width + x) * 3
    return rgb[i], rgb[i + 1], rgb[i + 2]


def extract_bits(rgb: bytes, width: int, x: int, y: int, w: int, h: int) -> list[str]:
    rows: list[str] = []
    for row in range(h):
        cells: list[str] = []
        for col in range(w):
            r, g, b = pixel(rgb, width, x + col, y + row)
            cells.append("X" if (r, g, b) == LIT else ".")
        rows.append("".join(cells))
    return rows


def ensure_mouth() -> None:
    if not ATLAS_PNG.exists() or not ATLAS_JSON.exists():
        raise SystemExit(f"missing {ATLAS_PNG} or {ATLAS_JSON}")
    meta = json.loads(ATLAS_JSON.read_text(encoding="utf-8"))
    if any(tile.get("id") == "param_mouth" for tile in meta.get("tiles", [])):
        return
    png_w, png_h, rgb = read_png_rgb(ATLAS_PNG)
    tile_x, tile_y, tile_w, tile_h = 152, 114, 7, 7
    new_h = max(png_h, tile_y + tile_h)
    canvas = bytearray(png_w * new_h * 3)
    for i in range(0, len(canvas), 3):
        canvas[i], canvas[i + 1], canvas[i + 2] = ATLAS_PURPLE
    old = bytearray(rgb)
    canvas[: len(old)] = old
    atlas_text(canvas, png_w, 0, tile_y + 1, "MOUTH")
    atlas_bits(canvas, png_w, tile_x, tile_y, MOUTH_BITS, ATLAS_LIT)
    write_png_rgb(ATLAS_PNG, png_w, new_h, bytes(canvas))
    for group in meta.get("groups", []):
        if group.get("id") == "params":
            group["count"] = int(group.get("count", 0)) + 1
    meta["tiles"].append(
        {
            "id": "param_mouth",
            "kind": "param",
            "label": "MOUTH",
            "label_x": 0,
            "label_y": tile_y,
            "x": tile_x,
            "y": tile_y,
            "w": tile_w,
            "h": tile_h,
            "empty": False,
            "bits": MOUTH_BITS,
        }
    )
    meta["atlas"]["width"] = png_w
    meta["atlas"]["height"] = new_h
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"added param_mouth to {ATLAS_PNG.name}")


def unpack() -> None:
    if not ATLAS_PNG.exists():
        raise SystemExit(f"missing {ATLAS_PNG}")
    if not ATLAS_JSON.exists():
        raise SystemExit(f"missing {ATLAS_JSON}")
    meta = json.loads(ATLAS_JSON.read_text(encoding="utf-8"))
    png_w, png_h, raw = read_png_rgb(ATLAS_PNG)
    rgb = bytearray(raw)
    rects = [(int(tile["x"]), int(tile["y"]), int(tile["w"]), int(tile["h"])) for tile in meta["tiles"]]
    relabeled = recolor_labels_black(rgb, png_w, png_h, rects)
    if relabeled:
        write_png_rgb(ATLAS_PNG, png_w, png_h, bytes(rgb))
        print(f"recolored {relabeled} label pixels black in {ATLAS_PNG.name}")
    meta["how_to"] = "python tools/settings_atlas.py  # PNG → JSON; adds param_mouth slot if missing"
    meta["atlas"]["file"] = ATLAS_PNG.name
    meta["atlas"]["width"] = png_w
    meta["atlas"]["height"] = png_h
    changed = 0
    for tile in meta["tiles"]:
        bits = extract_bits(bytes(rgb), png_w, tile["x"], tile["y"], tile["w"], tile["h"])
        if bits != tile.get("bits"):
            changed += 1
        tile["bits"] = bits
        tile["empty"] = not any("X" in row for row in bits)
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"updated {ATLAS_JSON.name} from {ATLAS_PNG.name} ({changed} tiles changed)")


if __name__ == "__main__":
    ensure_mouth()
    unpack()
