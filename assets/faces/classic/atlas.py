"""Classic face part atlas: unique 1-bit eye/nose/mouth tiles with a purple grid.

    python assets/faces/classic/atlas.py pack     # *_N.png → atlas.png
    python assets/faces/classic/atlas.py unpack     # atlas.png → colored *_N.png

Atlas gutters are #FF00FF. Tile interiors are black/white. Unpack paints each
frame with its accent from assets/emotions.json. Only kind=classic is converted.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools"))

from pack_assets import read_png_rgb, write_png_rgb

HERE = Path(__file__).resolve().parent
EMOTIONS_JSON = ROOT / "assets" / "emotions.json"
TOASTER_CATALOG = ROOT / "trash" / "toaster-pipeline" / "catalog.json"
ATLAS_PNG = HERE / "atlas.png"
ATLAS_JSON = HERE / "atlas.json"

FACE_W = 64
FACE_H = 32
REGIONS = {
    "eye": (0, 0, 32, 16),
    "nose": (48, 0, 16, 16),
    "mouth": (0, 16, 64, 16),
}
PAD = 2
SECTION_GAP = 12
LIT = 255
THRESHOLD = 128
GRID = (255, 0, 255)
COLUMNS = {"eye": 7, "nose": 8, "mouth": 4}


def is_white(r: int, g: int, b: int) -> bool:
    return r >= THRESHOLD and g >= THRESHOLD and b >= THRESHOLD


def is_grid_pixel(r: int, g: int, b: int) -> bool:
    return (r, g, b) == GRID


def crop(rgb: bytes, x0: int, y0: int, w: int, h: int, src_w: int = FACE_W) -> bytes:
    out = bytearray(w * h * 3)
    for y in range(h):
        src = ((y0 + y) * src_w + x0) * 3
        dst = y * w * 3
        out[dst : dst + w * 3] = rgb[src : src + w * 3]
    return bytes(out)


def blit(
    dst: bytearray,
    dst_w: int,
    dx: int,
    dy: int,
    src: bytes,
    w: int,
    h: int,
    color: tuple[int, int, int] | None,
    white_only: bool = False,
) -> None:
    cr, cg, cb = color if color is not None else (LIT, LIT, LIT)
    for y in range(h):
        for x in range(w):
            i = (y * w + x) * 3
            r, g, b = src[i], src[i + 1], src[i + 2]
            if is_grid_pixel(r, g, b):
                continue
            lit = is_white(r, g, b) if white_only else (r | g | b) >= THRESHOLD
            if not lit:
                continue
            di = ((dy + y) * dst_w + (dx + x)) * 3
            dst[di] = cr
            dst[di + 1] = cg
            dst[di + 2] = cb


def tile_inside(tiles: list[dict], width: int, height: int) -> list[bool]:
    inside = [False] * (width * height)
    for tile in tiles:
        for y in range(tile["y"], tile["y"] + tile["h"]):
            for x in range(tile["x"], tile["x"] + tile["w"]):
                inside[y * width + x] = True
    return inside


def apply_grid(plain: bytes, width: int, height: int, tiles: list[dict]) -> bytes:
    inside = tile_inside(tiles, width, height)
    out = bytearray(plain)
    gr, gg, gb = GRID
    for i, on_tile in enumerate(inside):
        if on_tile:
            continue
        o = i * 3
        out[o] = gr
        out[o + 1] = gg
        out[o + 2] = gb
    return bytes(out)


def mask_key(rgb: bytes) -> str:
    mask = bytes(1 if (rgb[i] | rgb[i + 1] | rgb[i + 2]) >= THRESHOLD else 0 for i in range(0, len(rgb), 3))
    return hashlib.sha256(mask).hexdigest()[:16]


def rgb_sha(rgb: bytes) -> str:
    return hashlib.sha256(rgb).hexdigest()


def toaster_index() -> dict[tuple[str, int], dict]:
    if not TOASTER_CATALOG.exists():
        return {}
    data = json.loads(TOASTER_CATALOG.read_text(encoding="utf-8"))
    out: dict[tuple[str, int], dict] = {}
    for seq in data.get("sequences", []):
        for idx, frame in enumerate(seq.get("frames", [])):
            out[(seq["id"], idx)] = {
                "eye": frame.get("eye"),
                "nose": frame.get("nose"),
                "mouth": frame.get("mouth"),
                "mouth_l": frame.get("mouth_l"),
                "mouth_r": frame.get("mouth_r"),
                "nose_l": frame.get("nose_l"),
                "nose_r": frame.get("nose_r"),
                "flip_mouth": bool(frame.get("flip_mouth")),
                "empty_all": bool(frame.get("empty_all")),
            }
    return out


def toaster_part_name(kind: str, recipe: dict | None) -> str | None:
    if not recipe or recipe.get("empty_all"):
        return None
    if kind == "eye":
        return recipe.get("eye")
    if kind == "nose":
        if recipe.get("nose"):
            return recipe["nose"]
        if recipe.get("nose_l") or recipe.get("nose_r"):
            return f"{recipe.get('nose_l')}_{recipe.get('nose_r')}"
        return None
    if recipe.get("mouth"):
        name = recipe["mouth"]
    elif recipe.get("mouth_l") or recipe.get("mouth_r"):
        name = f"{recipe.get('mouth_l')}_{recipe.get('mouth_r')}"
    else:
        return None
    if recipe.get("flip_mouth"):
        return f"{name}_flip"
    return name


def load_classic() -> list[dict]:
    catalog = json.loads(EMOTIONS_JSON.read_text(encoding="utf-8"))
    origin = toaster_index()
    frames = []
    for emo in catalog["emotions"]:
        if emo["kind"] != "classic":
            continue
        accent = tuple(int(v) for v in emo["accent"])
        for idx, frame in enumerate(emo["frames"]):
            path = HERE / frame["file"]
            w, h, rgb = read_png_rgb(path)
            if (w, h) != (FACE_W, FACE_H):
                raise SystemExit(f"{path} is {w}x{h}")
            frames.append(
                {
                    "id": emo["id"],
                    "index": idx,
                    "file": frame["file"],
                    "color": list(accent),
                    "rgb": rgb,
                    "sha": rgb_sha(rgb),
                    "origin": origin.get((emo["id"], idx)),
                }
            )
    return frames


def collect_tiles(frames: list[dict]) -> dict[str, list[dict]]:
    groups: dict[str, dict[str, dict]] = {kind: {} for kind in REGIONS}
    order: dict[str, list[str]] = {kind: [] for kind in REGIONS}
    for frame in frames:
        for kind, box in REGIONS.items():
            part = crop(frame["rgb"], *box)
            if not any(part):
                continue
            key = mask_key(part)
            bucket = groups[kind]
            if key not in bucket:
                suggested = toaster_part_name(kind, frame["origin"]) or f"{frame['id']}_{frame['index']}"
                tile_id = f"{kind}_{suggested}"
                n = 2
                while any(t["id"] == tile_id for t in bucket.values()):
                    tile_id = f"{kind}_{suggested}_{n}"
                    n += 1
                bucket[key] = {
                    "id": tile_id,
                    "kind": kind,
                    "w": box[2],
                    "h": box[3],
                    "mask": part,
                    "used_by": [],
                    "toaster": suggested,
                }
                order[kind].append(key)
            tile = bucket[key]
            tile["used_by"].append(frame["file"])
            frame.setdefault("parts", {})[kind] = tile["id"]
    return {kind: [groups[kind][k] for k in order[kind]] for kind in REGIONS}


def layout_atlas(tiles_by_kind: dict[str, list[dict]]) -> tuple[int, int, list[dict]]:
    y = PAD
    width = PAD
    placed: list[dict] = []
    for kind, tiles in tiles_by_kind.items():
        if not tiles:
            continue
        cell_w, cell_h = REGIONS[kind][2], REGIONS[kind][3]
        cols = COLUMNS[kind]
        rows = math.ceil(len(tiles) / cols)
        width = max(width, PAD + cols * (cell_w + PAD))
        for i, tile in enumerate(tiles):
            col = i % cols
            row = i // cols
            placed.append(
                {
                    "id": tile["id"],
                    "kind": kind,
                    "x": PAD + col * (cell_w + PAD),
                    "y": y + row * (cell_h + PAD),
                    "w": cell_w,
                    "h": cell_h,
                    "used_by": tile["used_by"],
                    "toaster": tile.get("toaster"),
                    "mask": tile["mask"],
                }
            )
        y += rows * (cell_h + PAD) + SECTION_GAP
    height = max(y - SECTION_GAP + PAD, PAD)
    return width, height, placed


def restore_rgb(meta: dict, atlas_rgb: bytes, atlas_w: int, frame: dict) -> bytes:
    color = tuple(frame["color"])
    tiles = {t["id"]: t for t in meta["tiles"]}
    out = bytearray(FACE_W * FACE_H * 3)
    for kind, box in REGIONS.items():
        tile_id = (frame.get("parts") or {}).get(kind)
        if not tile_id:
            continue
        tile = tiles[tile_id]
        part = crop(atlas_rgb, tile["x"], tile["y"], tile["w"], tile["h"], atlas_w)
        blit(out, FACE_W, box[0], box[1], part, tile["w"], tile["h"], color, white_only=True)
    return bytes(out)


def pack() -> int:
    frames = load_classic()
    tiles_by_kind = collect_tiles(frames)
    width, height, placed = layout_atlas(tiles_by_kind)
    public_tiles = [{k: tile[k] for k in ("id", "kind", "x", "y", "w", "h", "used_by", "toaster")} for tile in placed]
    plain = bytearray(width * height * 3)
    for tile in placed:
        blit(plain, width, tile["x"], tile["y"], tile["mask"], tile["w"], tile["h"], None)
    grid = apply_grid(bytes(plain), width, height, public_tiles)
    write_png_rgb(ATLAS_PNG, width, height, grid)
    meta = {
        "how_to": "python assets/faces/classic/atlas.py pack|unpack",
        "face": [FACE_W, FACE_H],
        "regions": {name: list(box) for name, box in REGIONS.items()},
        "grid": {"color": list(GRID)},
        "atlas": {"file": "atlas.png", "width": width, "height": height, "threshold": THRESHOLD},
        "tiles": public_tiles,
        "frames": [
            {
                "id": f["id"],
                "index": f["index"],
                "file": f["file"],
                "color": f["color"],
                "sha256": f["sha"],
                "parts": {kind: f.get("parts", {}).get(kind) for kind in REGIONS},
            }
            for f in frames
        ],
    }
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    bad = 0
    for frame in frames:
        rebuilt = restore_rgb(meta, grid, width, frame)
        if rebuilt != frame["rgb"]:
            bad += 1
            print(f"PIXEL {frame['file']}")
    print(f"wrote {ATLAS_PNG.name} {width}x{height} ({sum(len(v) for v in tiles_by_kind.values())} tiles, {len(frames)} frames)")
    if bad:
        print(f"pack round-trip failed: {bad}")
        return bad
    print("ok pack round-trip (pixel + sha256)")
    return 0


def unpack() -> int:
    if not ATLAS_JSON.exists() or not ATLAS_PNG.exists():
        raise SystemExit("atlas.png / atlas.json missing")
    meta = json.loads(ATLAS_JSON.read_text(encoding="utf-8"))
    aw, ah, atlas_rgb = read_png_rgb(ATLAS_PNG)
    if aw != meta["atlas"]["width"] or ah != meta["atlas"]["height"]:
        raise SystemExit(f"atlas size {aw}x{ah} != meta")
    for frame in meta["frames"]:
        rgb = restore_rgb(meta, atlas_rgb, aw, frame)
        write_png_rgb(HERE / frame["file"], FACE_W, FACE_H, rgb)
        frame["sha256"] = rgb_sha(rgb)
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"unpacked {len(meta['frames'])} frames -> {HERE}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Classic PNG <-> atlas.png")
    parser.add_argument("cmd", choices=["pack", "unpack"])
    args = parser.parse_args()
    if args.cmd == "pack":
        return pack()
    return unpack()


if __name__ == "__main__":
    sys.exit(main())
