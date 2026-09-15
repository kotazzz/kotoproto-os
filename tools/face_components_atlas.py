"""Build a monochrome classic-face component atlas.

    python tools/face_components_atlas.py

Classic RGB frames in assets/ are split into eye and mouth tiles.
Only unique 1bpp sprites are kept (no labels, no duplicate or
in-between animation tiles). Purple background makes sprite bounds
visible. Not packed into firmware.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pack_assets import (
    ASSETS,
    ATLAS_INK,
    ATLAS_LIT,
    ATLAS_PURPLE,
    EMOTIONS_JSON,
    FACE_COMPONENTS_JSON,
    ROOT,
    atlas_fill,
    read_png_rgb,
    write_png_rgb,
)

OUT_PNG = ASSETS / "face_components.png"
WWW_PNG = ROOT / "platforms" / "sim" / "www" / "face_components.png"

EYE_W, EYE_H = 32, 16
NOSE_W, NOSE_H = 16, 16
MOUTH_W, MOUTH_H = 64, 16
NOSE_X = 48
MOUTH_Y = 16
GAP = 1
MARGIN = 1
SHEET_W = 4 * EYE_W + 3 * GAP + 2 * MARGIN

PARTS = (
    ("eye", 0, 0, EYE_W, EYE_H),
    ("nose", NOSE_X, 0, NOSE_W, NOSE_H),
    ("mouth", 0, MOUTH_Y, MOUTH_W, MOUTH_H),
)


def region_key(src: bytes, src_w: int, sx: int, sy: int, w: int, h: int) -> tuple[str, ...]:
    rows: list[str] = []
    for row in range(h):
        cells: list[str] = []
        for col in range(w):
            i = ((sy + row) * src_w + (sx + col)) * 3
            cells.append("X" if (src[i] | src[i + 1] | src[i + 2]) else ".")
        rows.append("".join(cells))
    return tuple(rows)


def blit_bits(dest: bytearray, dest_w: int, dx: int, dy: int, bits: tuple[str, ...]) -> None:
    for row, line in enumerate(bits):
        for col, ch in enumerate(line):
            color = ATLAS_LIT if ch == "X" else ATLAS_INK
            i = ((dy + row) * dest_w + (dx + col)) * 3
            dest[i], dest[i + 1], dest[i + 2] = color


MAX_PER_EMOTION = {
    "eye": 3,
    "mouth": 2,
    "nose": 1,
}


def collect_unique(catalog: dict) -> dict[str, list[dict]]:
    unique: dict[str, list[dict]] = {"eye": [], "nose": [], "mouth": []}
    seen: dict[str, set[tuple[str, ...]]] = {"eye": set(), "nose": set(), "mouth": set()}
    per_emotion: dict[str, dict[str, int]] = {}
    for emo in catalog.get("emotions", []):
        if emo.get("kind") != "classic":
            continue
        eid = str(emo.get("id") or "")
        counts = per_emotion.setdefault(eid, {"eye": 0, "nose": 0, "mouth": 0})
        for frame in emo.get("frames") or []:
            name = frame.get("file")
            if not name:
                continue
            path = ASSETS / name
            if not path.exists():
                raise SystemExit(f"missing {path}")
            src_w, src_h, src = read_png_rgb(path)
            if src_w < 64 or src_h < 32:
                raise SystemExit(f"{path.name} is not 64x32")
            for part, sx, sy, w, h in PARTS:
                if counts[part] >= MAX_PER_EMOTION[part]:
                    continue
                key = region_key(src, src_w, sx, sy, w, h)
                if key in seen[part]:
                    continue
                seen[part].add(key)
                counts[part] += 1
                unique[part].append(
                    {
                        "id": f"{path.stem}_{part}",
                        "part": part,
                        "source": name,
                        "w": w,
                        "h": h,
                        "bits": key,
                    }
                )
    return unique


def layout_group(tiles: list[dict], y0: int) -> int:
    x = MARGIN
    y = y0
    row_h = 0
    for tile in tiles:
        w = int(tile["w"])
        h = int(tile["h"])
        if x > MARGIN and x + w + MARGIN > SHEET_W:
            x = MARGIN
            y += row_h + GAP
            row_h = 0
        tile["x"] = x
        tile["y"] = y
        x += w + GAP
        row_h = max(row_h, h)
    if tiles:
        return y + row_h + GAP
    return y0


def emit() -> None:
    if not EMOTIONS_JSON.exists():
        raise SystemExit(f"missing {EMOTIONS_JSON}")
    catalog = json.loads(EMOTIONS_JSON.read_text(encoding="utf-8"))
    unique = collect_unique(catalog)
    y = MARGIN
    y = layout_group(unique["eye"], y)
    y = layout_group(unique["mouth"], y)
    if len(unique["nose"]) > 1:
        y = layout_group(unique["nose"], y)
    else:
        unique["nose"] = []
    height = max(y + MARGIN - GAP, MARGIN + EYE_H + MARGIN)
    rgb = bytearray(SHEET_W * height * 3)
    atlas_fill(rgb, SHEET_W, height, ATLAS_PURPLE)
    tiles: list[dict] = []
    for part in ("eye", "mouth", "nose"):
        for tile in unique[part]:
            blit_bits(rgb, SHEET_W, tile["x"], tile["y"], tile["bits"])
            tiles.append(
                {
                    "id": tile["id"],
                    "part": tile["part"],
                    "source": tile["source"],
                    "x": tile["x"],
                    "y": tile["y"],
                    "w": tile["w"],
                    "h": tile["h"],
                }
            )
    write_png_rgb(OUT_PNG, SHEET_W, height, bytes(rgb))
    WWW_PNG.parent.mkdir(parents=True, exist_ok=True)
    write_png_rgb(WWW_PNG, SHEET_W, height, bytes(rgb))
    meta = {
        "how_to": "python tools/face_components_atlas.py  # unique classic eye/mouth/nose, no labels",
        "kind": "classic",
        "gap": GAP,
        "grid": {"color": list(ATLAS_PURPLE)},
        "lit": list(ATLAS_LIT),
        "ink": list(ATLAS_INK),
        "atlas": {"file": OUT_PNG.name, "width": SHEET_W, "height": height},
        "parts": [
            {"id": "eye", "w": EYE_W, "h": EYE_H, "count": len(unique["eye"])},
            {"id": "mouth", "w": MOUTH_W, "h": MOUTH_H, "count": len(unique["mouth"])},
        ],
        "tiles": tiles,
    }
    FACE_COMPONENTS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote {OUT_PNG} ({len(unique['eye'])} eyes, {len(unique['mouth'])} mouths)")


if __name__ == "__main__":
    emit()
