"""Generate the settings icon atlas (authoring sheet, not packed into firmware).

    python tools/settings_atlas.py

Layout is a vertical list: 5x7 inverted label on #FF00FF, then a black 1-bit
icon square flush right. One purple pixel between rows. Screens are 9x9,
parameters are 7x7. White pixels are the icon; the square stays black.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pack_assets import write_png_rgb

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
FONT_CPP = ROOT / "firmware" / "src" / "gfx" / "font5x7.cpp"
ATLAS_PNG = ASSETS / "settings_icons.png"
ATLAS_JSON = ASSETS / "settings_icons.json"

GRID = (255, 0, 255)
INK = (0, 0, 0)
LIT = (255, 255, 255)
FONT_W = 5
FONT_H = 7
FONT_GAP = 1
ROW_GAP = 1
LABEL_ICON_GAP = 1
SCREEN_SIZE = 9
PARAM_SIZE = 7

# 1-bit white-on-black. X is lit. Rows must match the tile size.
ICONS = {
    "screen_matrix": (
        "XXXXXXXXX",
        "X.X.X.X.X",
        "XXXXXXXXX",
        "X.X.X.X.X",
        "XXXXXXXXX",
        "X.X.X.X.X",
        "XXXXXXXXX",
        "X.X.X.X.X",
        "XXXXXXXXX",
    ),
    "screen_switches": (
        ".........",
        ".XXXXXXX.",
        "X.......X",
        "X....XXX.",
        "X....XXX.",
        "X.......X",
        ".XXXXXXX.",
        ".........",
        ".........",
    ),
    "screen_calibration": (
        "....X....",
        "....X....",
        "..XXXXX..",
        ".X..X..X.",
        "XXXX.XXXX",
        ".X..X..X.",
        "..XXXXX..",
        "....X....",
        "....X....",
    ),
    "screen_other": (
        ".........",
        ".........",
        ".........",
        ".XX.XX.XX",
        ".XX.XX.XX",
        ".........",
        ".........",
        ".........",
        ".........",
    ),
    "screen_restart_save": (
        ".XXXXXXX.",
        ".X.....X.",
        ".XXXXXXX.",
        ".X.....X.",
        ".X.XXX.X.",
        ".X.X.X.X.",
        ".X.....X.",
        ".XXXXXXX.",
        ".........",
    ),
    "param_matrix": (
        "XXXXXXX",
        "X.X.X.X",
        "XXXXXXX",
        "X.X.X.X",
        "XXXXXXX",
        "X.X.X.X",
        "XXXXXXX",
    ),
    "param_headphones_led": (
        ".XXXXX.",
        "X.....X",
        "XX...XX",
        "X.....X",
        "XXX.XXX",
        "X.X.X.X",
        ".X...X.",
    ),
    "param_auto_blink": (
        ".......",
        ".XXXXX.",
        "XXXXXXX",
        ".XXXXX.",
        "X.....X",
        ".XXXXX.",
        "..XXX..",
    ),
    "param_boop": (
        "..XXX..",
        ".X...X.",
        "X..X..X",
        "X.XXX.X",
        "X..X..X",
        ".X...X.",
        "..XXX..",
    ),
    "param_fan": (
        ".X...X.",
        ".XXXXX.",
        "..X.X..",
        "XXXXXXX",
        "..X.X..",
        ".XXXXX.",
        ".X...X.",
    ),
    "param_rare_transitions": (
        "...X...",
        ".X.X.X.",
        "..XXX..",
        "XXXXXXX",
        "..XXX..",
        ".X.X.X.",
        "...X...",
    ),
    "param_save": (
        "XXXXXXX",
        "XX.X.XX",
        "XXXXXXX",
        "X.....X",
        "X.XXX.X",
        "X.X.X.X",
        "XXXXXXX",
    ),
    "param_restart": (
        ".XXXXX.",
        "X.....X",
        "X......",
        "X..X..X",
        "X...XX.",
        ".XXXXX.",
        "...X...",
    ),
}

SCREENS = (
    ("matrix", "MATRIX"),
    ("switches", "SWITCHES"),
    ("calibration", "CALIBRATION & SENSITIVITY"),
    ("other", "OTHER"),
    ("restart_save", "RESTART & SAVE"),
)

PARAMS = (
    ("matrix", "MATRIX"),
    ("headphones_led", "HEADPHONES LED"),
    ("auto_blink", "AUTO-BLINK"),
    ("boop", "BOOP"),
    ("fan", "FAN"),
    ("rare_transitions", "RARE TRANSITIONS"),
    ("save", "SAVE"),
    ("restart", "RESTART"),
)

GLYPH_RE = re.compile(
    r"case '((?:\\.|[^'\\]))':\s*return\s*((?:\"[X.]{5}\"\s*){7});",
)


def unescape_char(raw: str) -> str:
    if len(raw) == 2 and raw[0] == "\\":
        return raw[1]
    return raw


def load_glyphs(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    glyphs: dict[str, str] = {}
    for match in GLYPH_RE.finditer(text):
        ch = unescape_char(match.group(1))
        bits = "".join(match.group(2).replace('"', "").split())
        if len(bits) != FONT_W * FONT_H:
            continue
        glyphs[ch] = bits
    if " " not in glyphs:
        glyphs[" "] = "." * (FONT_W * FONT_H)
    return glyphs


def text_width(text: str) -> int:
    if not text:
        return 0
    return len(text) * FONT_W + (len(text) - 1) * FONT_GAP


def put(rgb: bytearray, width: int, x: int, y: int, color: tuple[int, int, int]) -> None:
    if x < 0 or y < 0 or x >= width:
        return
    i = (y * width + x) * 3
    if i + 2 >= len(rgb):
        return
    rgb[i] = color[0]
    rgb[i + 1] = color[1]
    rgb[i + 2] = color[2]


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
            put(rgb, width, x + col, y + row, color)


def icon_rows(tile_id: str, size: int) -> tuple[str, ...]:
    rows = ICONS.get(tile_id)
    if rows is None:
        raise SystemExit(f"missing icon {tile_id}")
    if len(rows) != size or any(len(row) != size for row in rows):
        raise SystemExit(f"{tile_id} must be {size}x{size}")
    if any(ch not in ".X" for row in rows for ch in row):
        raise SystemExit(f"{tile_id} has a non .X pixel")
    return rows


def blit_icon(rgb: bytearray, width: int, x: int, y: int, rows: tuple[str, ...]) -> None:
    for row, line in enumerate(rows):
        for col, ch in enumerate(line):
            if ch == "X":
                put(rgb, width, x + col, y + row, LIT)


def draw_inverted_text(
    rgb: bytearray,
    width: int,
    x: int,
    y: int,
    text: str,
    glyphs: dict[str, str],
) -> None:
    cursor = x
    for ch in text.upper():
        bits = glyphs.get(ch, glyphs.get("?"))
        if bits is None:
            bits = glyphs[" "]
        for row in range(FONT_H):
            for col in range(FONT_W):
                if bits[row * FONT_W + col] == "X":
                    put(rgb, width, cursor + col, y + row, INK)
        cursor += FONT_W + FONT_GAP


def layout_rows() -> tuple[int, int, list[dict]]:
    labels = [label for _, label in SCREENS] + [label for _, label in PARAMS]
    max_label = max(text_width(label) for label in labels)
    width = max_label + LABEL_ICON_GAP + SCREEN_SIZE
    tiles: list[dict] = []
    y = 0
    for ident, label in SCREENS:
        tiles.append(
            {
                "id": f"screen_{ident}",
                "kind": "screen",
                "label": label,
                "label_x": 0,
                "label_y": y + (SCREEN_SIZE - FONT_H) // 2,
                "x": width - SCREEN_SIZE,
                "y": y,
                "w": SCREEN_SIZE,
                "h": SCREEN_SIZE,
                "empty": False,
                "bits": list(icon_rows(f"screen_{ident}", SCREEN_SIZE)),
            }
        )
        y += SCREEN_SIZE + ROW_GAP
    for ident, label in PARAMS:
        tiles.append(
            {
                "id": f"param_{ident}",
                "kind": "param",
                "label": label,
                "label_x": 0,
                "label_y": y,
                "x": width - PARAM_SIZE,
                "y": y,
                "w": PARAM_SIZE,
                "h": PARAM_SIZE,
                "empty": False,
                "bits": list(icon_rows(f"param_{ident}", PARAM_SIZE)),
            }
        )
        y += PARAM_SIZE + ROW_GAP
    height = y - ROW_GAP
    return width, height, tiles


def generate() -> None:
    glyphs = load_glyphs(FONT_CPP)
    width, height, tiles = layout_rows()
    rgb = bytearray(width * height * 3)
    fill_rect(rgb, width, 0, 0, width, height, GRID)
    for tile in tiles:
        draw_inverted_text(rgb, width, tile["label_x"], tile["label_y"], tile["label"], glyphs)
        fill_rect(rgb, width, tile["x"], tile["y"], tile["w"], tile["h"], INK)
        blit_icon(rgb, width, tile["x"], tile["y"], tuple(tile["bits"]))
    write_png_rgb(ATLAS_PNG, width, height, bytes(rgb))
    meta = {
        "how_to": "python tools/settings_atlas.py",
        "packed": False,
        "grid": {"color": list(GRID)},
        "ink": list(INK),
        "lit": list(LIT),
        "font": {
            "width": FONT_W,
            "height": FONT_H,
            "spacing": FONT_GAP,
            "inverted": True,
        },
        "gap": ROW_GAP,
        "atlas": {
            "file": ATLAS_PNG.name,
            "width": width,
            "height": height,
        },
        "groups": [
            {"id": "screens", "size": SCREEN_SIZE, "count": len(SCREENS)},
            {"id": "params", "size": PARAM_SIZE, "count": len(PARAMS)},
        ],
        "tiles": tiles,
    }
    ATLAS_JSON.write_text(json.dumps(meta, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote {ATLAS_PNG.name} {width}x{height} ({len(tiles)} icons)")


if __name__ == "__main__":
    generate()
