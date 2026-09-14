"""Sync settings icon metadata from the authored PNG.

    python tools/settings_atlas.py

PNG is the source. This writes assets/settings_icons.json only and never
overwrites the atlas image.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pack_assets import read_png_rgb

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
ATLAS_PNG = ASSETS / "settings_icons.png"
ATLAS_JSON = ASSETS / "settings_icons.json"

LIT = (255, 255, 255)


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


def unpack() -> None:
    if not ATLAS_PNG.exists():
        raise SystemExit(f"missing {ATLAS_PNG}")
    if not ATLAS_JSON.exists():
        raise SystemExit(f"missing {ATLAS_JSON}")
    meta = json.loads(ATLAS_JSON.read_text(encoding="utf-8"))
    png_w, png_h, rgb = read_png_rgb(ATLAS_PNG)
    meta["how_to"] = "python tools/settings_atlas.py  # PNG → JSON only, never writes PNG"
    meta["atlas"]["file"] = ATLAS_PNG.name
    meta["atlas"]["width"] = png_w
    meta["atlas"]["height"] = png_h
    changed = 0
    for tile in meta["tiles"]:
        bits = extract_bits(rgb, png_w, tile["x"], tile["y"], tile["w"], tile["h"])
        if bits != tile.get("bits"):
            changed += 1
        tile["bits"] = bits
        tile["empty"] = not any("X" in row for row in bits)
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"updated {ATLAS_JSON.name} from {ATLAS_PNG.name} ({changed} tiles changed)")


if __name__ == "__main__":
    unpack()
