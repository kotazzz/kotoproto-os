"""Import ToasterBlaster face/HUD/system bitmaps into kotoproto-os assets/.

Source of truth for parts is the original PNG tree (Eye/Mouth/Nose/Other).
Sequence previews are composed onto a 64x16 canvas with LEFT_SIDE mirrored,
matching MAX7219 layout. Firmware C++ tables are emitted from the same data.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import struct
import zlib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
CPP_HDR = ROOT / "firmware" / "include" / "koto" / "assets" / "bitmaps.hpp"
CPP_SRC = ROOT / "firmware" / "src" / "assets" / "bitmaps.cpp"


def mirror_int(number: int) -> int:
    result = 0
    for _ in range(8):
        result = (result << 1) | (number & 1)
        number >>= 1
    return result


def mirror_bitmap(buf: list[int]) -> list[int]:
    size = len(buf)
    out = list(buf)
    if size == 8:
        return [mirror_int(b) for b in out]
    for i in range(size // 16):
        for j in range(8):
            temp = mirror_int(out[i * 8 + j])
            out[i * 8 + j] = mirror_int(out[size - (i + 1) * 8 + j])
            out[size - (i + 1) * 8 + j] = temp
    return out


def vertical_mirror(buf: list[int]) -> list[int]:
    out = list(buf)
    for i in range(0, len(out), 8):
        for j in range(4):
            out[i + j], out[i + 7 - j] = out[i + 7 - j], out[i + j]
    return out


def rotate180(buf: list[int]) -> list[int]:
    return mirror_bitmap(vertical_mirror(buf))


def parse_hex_bytes(blob: str) -> list[int]:
    return [int(x, 0) for x in re.findall(r"0[xX][0-9A-Fa-f]+|0[bB][01]+|\b\d+\b", blob)]


def parse_face_bitmaps(path: Path) -> dict[str, dict[str, list[int]]]:
    text = path.read_text(encoding="utf-8")
    result: dict[str, dict[str, list[int]]] = {}
    current: str | None = None
    for line in text.splitlines():
        ns = re.match(r"\s*namespace\s+(\w+)\s*\{", line)
        if ns and ns.group(1) not in ("Bitmaps",):
            current = ns.group(1)
            result.setdefault(current, {})
            continue
        m = re.match(r"\s*Bitmap\s+(\w+)\[(\d+)\]\s*\{(.+)\}\s*;", line)
        if m and current:
            result[current][m.group(1)] = parse_hex_bytes(m.group(3))
    return result


def parse_named_byte_arrays(path: Path, pattern: str) -> dict[str, list[int]]:
    text = path.read_text(encoding="utf-8")
    found: dict[str, list[int]] = {}
    for m in re.finditer(pattern, text, re.S):
        found[m.group(1)] = parse_hex_bytes(m.group(2))
    return found


def write_png_1bit(path: Path, width: int, height: int, on_at: Callable[[int, int], bool]) -> None:
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        byte = 0
        bit = 7
        for x in range(width):
            if on_at(x, y):
                byte |= 1 << bit
            bit -= 1
            if bit < 0:
                raw.append(byte)
                byte = 0
                bit = 7
        if bit != 7:
            raw.append(byte)

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    ihdr = struct.pack(">IIBBBBB", width, height, 1, 0, 0, 0, 0)
    png = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def bitmap_to_grid(data: list[int]) -> list[list[bool]]:
    width = len(data)
    grid = [[False] * width for _ in range(8)]
    for i, byte in enumerate(data):
        tile = i // 8
        y = i % 8
        for bit in range(8):
            x = tile * 8 + bit
            grid[y][x] = (byte & (1 << (7 - bit))) != 0
    return grid


def save_bitmap_png(path: Path, data: list[int]) -> None:
    grid = bitmap_to_grid(data)
    write_png_1bit(path, len(data), 8, lambda x, y: grid[y][x])


def pack_rows(grid: list[list[bool]]) -> tuple[int, int, list[int]]:
    height = len(grid)
    width = len(grid[0]) if height else 0
    stride = (width + 7) // 8
    out: list[int] = []
    for y in range(height):
        for s in range(stride):
            byte = 0
            for bit in range(8):
                x = s * 8 + bit
                if x < width and grid[y][x]:
                    byte |= 1 << (7 - bit)
            out.append(byte)
    return width, height, out


def empty_frame() -> list[list[int]]:
    return [[0] * 8 for _ in range(14)]


def stamp(frame: list[list[int]], data: list[int], matrix_start: int) -> None:
    count = len(data) // 8
    for m in range(count):
        for r in range(8):
            frame[matrix_start + m][r] = data[m * 8 + r] & 0xFF


def lookup(bitmaps: dict[str, dict[str, list[int]]], name: str, kind: str) -> list[int]:
    if kind == "Eye" and name in bitmaps["Eye"]:
        return list(bitmaps["Eye"][name])
    if kind == "Mouth" and name in bitmaps["Mouth"]:
        return list(bitmaps["Mouth"][name])
    if kind == "Nose" and name in bitmaps["Nose"]:
        return list(bitmaps["Nose"][name])
    if name in bitmaps.get("Other", {}):
        src = list(bitmaps["Other"][name])
        sizes = {"Eye": 16, "Mouth": 32, "Nose": 8}
        return src[: sizes[kind]]
    if kind == "Eye" and name in bitmaps["Mouth"]:
        return list(bitmaps["Mouth"][name][:16])
    if kind == "Nose" and name in bitmaps["Mouth"]:
        return list(bitmaps["Mouth"][name][:8])
    raise KeyError(f"{kind}::{name}")


@dataclass
class Face:
    eye: str | None = None
    mouth: str | None = None
    nose: str | None = None
    eye_l: str | None = None
    eye_r: str | None = None
    mouth_l: str | None = None
    mouth_r: str | None = None
    nose_l: str | None = None
    nose_r: str | None = None
    mirror_eyes: bool = True
    mirror_nose: bool = True
    mirror_mouth: bool = True
    flip_mouth: bool = False
    fill_all: bool = False
    empty_all: bool = False
    duration_ms: int = 500
    loop: bool = False
    note: str = ""


@dataclass
class Sequence:
    seq_id: str
    group: str
    frames: list[Face] = field(default_factory=list)
    transition: str | None = "any"
    note: str = ""


def compose_face(face: Face, bitmaps: dict[str, dict[str, list[int]]]) -> list[list[int]]:
    frame = empty_frame()
    if face.empty_all:
        return frame
    if face.fill_all:
        for m in range(14):
            for r in range(8):
                frame[m][r] = 0xFF
        return frame

    eye_r = face.eye_r or face.eye
    eye_l = face.eye_l or face.eye
    nose_r = face.nose_r or face.nose
    nose_l = face.nose_l or face.nose
    mouth_r = face.mouth_r or face.mouth
    mouth_l = face.mouth_l or face.mouth

    if eye_r:
        stamp(frame, lookup(bitmaps, eye_r, "Eye"), 0)
    if eye_l:
        data = lookup(bitmaps, eye_l, "Eye")
        if face.mirror_eyes:
            data = mirror_bitmap(data)
        stamp(frame, data, 4)

    if nose_r:
        stamp(frame, lookup(bitmaps, nose_r, "Nose"), 2)
    if nose_l:
        data = lookup(bitmaps, nose_l, "Nose")
        if face.mirror_nose:
            data = mirror_bitmap(data)
        stamp(frame, data, 3)

    def mouth_data(name: str, mirrored: bool) -> list[int]:
        data = lookup(bitmaps, name, "Mouth")
        if face.flip_mouth:
            data = rotate180(data)
        if mirrored:
            data = mirror_bitmap(data)
        return data

    if mouth_r:
        stamp(frame, mouth_data(mouth_r, False), 6)
    if mouth_l:
        stamp(frame, mouth_data(mouth_l, face.mirror_mouth), 10)

    return frame


def frame_on_at(frame: list[list[int]], x: int, y: int) -> bool:
    if y < 8:
        if x >= 48:
            return False
        m, c = divmod(x, 8)
        return (frame[m][y] & (1 << (7 - c))) != 0
    if x >= 64:
        return False
    m, c = divmod(x, 8)
    return (frame[6 + m][y - 8] & (1 << (7 - c))) != 0


def xbm_to_grid(data: list[int], width: int, height: int, padded_width: int | None = None) -> list[list[bool]]:
    stride = ((padded_width or width) + 7) // 8
    grid = [[False] * width for _ in range(height)]
    for y in range(height):
        for x in range(width):
            idx = y * stride + x // 8
            if idx >= len(data):
                continue
            grid[y][x] = (data[idx] & (1 << (x % 8))) != 0
    return grid


def column_digit_to_grid(cols: list[int]) -> list[list[bool]]:
    width = len(cols)
    grid = [[False] * width for _ in range(8)]
    for x, value in enumerate(cols):
        for y in range(8):
            grid[y][x] = (value & (1 << (7 - y))) != 0
    return grid


def natural_key(name: str):
    parts = re.split(r"(\d+)", name)
    return [int(p) if p.isdigit() else p.lower() for p in parts]


def load_metadata(folder: Path) -> dict:
    path = folder / "metadata.json"
    if not path.exists():
        return {"randomizerExceptions": []}
    return json.loads(path.read_text(encoding="utf-8"))


def copy_part_pngs(src_root: Path, dest_root: Path) -> dict[str, list[str]]:
    mapping = {
        "eye": src_root / "src" / "Assets" / "Bitmaps" / "Eye",
        "mouth": src_root / "src" / "Assets" / "Bitmaps" / "Mouth",
        "nose": src_root / "src" / "Assets" / "Bitmaps" / "Nose",
        "other": src_root / "src" / "Assets" / "Bitmaps" / "Other",
    }
    copied: dict[str, list[str]] = {}
    for kind, src in mapping.items():
        dest = dest_root / kind
        dest.mkdir(parents=True, exist_ok=True)
        names: list[str] = []
        if src.exists():
            meta_src = src / "metadata.json"
            if meta_src.exists():
                shutil.copy2(meta_src, dest / "metadata.json")
            for png in sorted(src.glob("*.png"), key=lambda p: natural_key(p.stem)):
                shutil.copy2(png, dest / png.name)
                names.append(png.stem)
        copied[kind] = names
    return copied


def face_to_catalog(face: Face) -> dict:
    item: dict = {
        "duration_ms": face.duration_ms,
        "mirror_eyes": face.mirror_eyes,
        "mirror_nose": face.mirror_nose,
        "mirror_mouth": face.mirror_mouth,
        "flip_mouth": face.flip_mouth,
        "loop": face.loop,
    }
    for key in (
        "eye",
        "mouth",
        "nose",
        "eye_l",
        "eye_r",
        "mouth_l",
        "mouth_r",
        "nose_l",
        "nose_r",
    ):
        value = getattr(face, key)
        if value:
            item[key] = value
    if face.empty_all:
        item["empty_all"] = True
    if face.fill_all:
        item["fill_all"] = True
    if face.note:
        item["note"] = face.note
    return item


def sequences() -> list[Sequence]:
    n = Face
    return [
        Sequence("Neutral", "basic", [n(eye="neutral", mouth="neutral", nose="neutral")], "any"),
        Sequence("Joy", "basic", [n(eye="joy", mouth="happy", nose="neutral")], "any"),
        Sequence("JoyBlush", "basic", [n(eye="joyBlush", mouth="happy", nose="neutral")], "any"),
        Sequence("Blushing", "basic", [n(eye="blushing", mouth="sad", nose="neutral")], "any"),
        Sequence("Angry", "basic", [n(eye="angry", mouth="sad", nose="neutral", flip_mouth=True)], "any"),
        Sequence("AngryHappy", "basic", [n(eye="angry", mouth="neutral", nose="neutral")], "any"),
        Sequence("Annoyed", "basic", [n(eye="annoyed", mouth="sad", nose="neutral", flip_mouth=True)], "any"),
        Sequence("Spooked", "basic", [n(eye="flushed", mouth="sad", nose="neutral")], "any"),
        Sequence("Squinting", "basic", [n(eye="boop", mouth="happy", nose="neutral")], "any"),
        Sequence(
            "Questioning",
            "basic",
            [n(eye="questionMark", mouth="smirk", nose="neutral", mirror_eyes=False)],
            "drop",
        ),
        Sequence(
            "Exclamation",
            "basic",
            [
                n(eye="exclamationPoint", mouth="smirk", nose="neutral", duration_ms=500),
                n(eye="empty", mouth="smirk", nose="neutral", duration_ms=200, loop=True),
            ],
            "drop",
        ),
        Sequence("UWU", "basic", [n(eye="u", mouth="w", nose="neutral")], "drop"),
        Sequence("OWO", "basic", [n(eye="o", mouth="w", nose="neutral")], "earthquake"),
        Sequence(
            "NOPE",
            "basic",
            [
                n(
                    eye="nope",
                    mouth="nope",
                    nose_r="nope",
                    nose_l="o",
                    mirror_eyes=False,
                    mirror_nose=False,
                    mirror_mouth=False,
                    duration_ms=500,
                ),
                n(empty_all=True, duration_ms=100, loop=True),
            ],
            "earthquake",
        ),
        Sequence(
            "Wink",
            "animated",
            [
                n(eye="neutral", mouth="neutral", nose="neutral", duration_ms=150),
                n(
                    eye="neutral",
                    mouth="smile",
                    nose="neutral",
                    duration_ms=250,
                    note="runtime: blink right eye, translate, particles",
                ),
            ],
            "crossfade",
        ),
        Sequence(
            "HeartEyes",
            "animated",
            [
                n(eye="heart0", mouth="smile", nose="neutral", duration_ms=400),
                n(eye="heart1", mouth="smile", nose="neutral", duration_ms=120),
                n(eye="heart2", mouth="smile", nose="neutral", duration_ms=180),
                n(eye="heart1", mouth="smile", nose="neutral", duration_ms=120),
                n(eye="heart2", mouth="smile", nose="neutral", duration_ms=140),
                n(eye="heart1", mouth="smile", nose="neutral", duration_ms=140, loop=True),
            ],
            "any",
        ),
        Sequence(
            "Dead",
            "basic",
            [
                n(
                    eye="dead",
                    mouth_l="smile",
                    mouth_r="smileTongue",
                    nose="neutral",
                    duration_ms=33,
                    loop=True,
                    note="runtime: small glitch",
                )
            ],
            "glitch",
        ),
        Sequence("PowerOff", "animated", [n(empty_all=True)], "losePower"),
        Sequence(
            "BatteryCheck",
            "animated",
            [
                n(
                    eye="batteryCheck",
                    mouth="batteryCheck",
                    nose="empty",
                    mirror_eyes=False,
                    mirror_nose=False,
                    mirror_mouth=False,
                    duration_ms=250,
                ),
                n(empty_all=True, duration_ms=250, loop=True),
            ],
            None,
        ),
        Sequence(
            "Crying",
            "animated",
            [
                n(eye="crying0", mouth="shaking0", nose="neutral", duration_ms=100),
                n(eye="crying0", mouth="shaking1", nose="neutral", duration_ms=100),
                n(eye="crying0", mouth="shaking2", nose="neutral", duration_ms=100),
                n(eye="crying1", mouth="shaking0", nose="neutral", flip_mouth=True, duration_ms=100),
                n(eye="crying1", mouth="shaking1", nose="neutral", flip_mouth=True, duration_ms=100),
                n(
                    eye="crying1",
                    mouth="shaking2",
                    nose="neutral",
                    flip_mouth=True,
                    duration_ms=100,
                    loop=True,
                ),
            ],
            "crossfade",
            note="runtime: eye translate + tear particles",
        ),
        Sequence(
            "Dizzy",
            "animated",
            [
                n(
                    eye="dizzy0",
                    mouth_l="smile",
                    mouth_r="smileTongue",
                    nose="neutral",
                    duration_ms=75,
                    note="runtime: rotate eyes 0/90/180/270 and glitch",
                ),
                n(
                    eye="dizzy1",
                    mouth_l="smile",
                    mouth_r="smileTongue",
                    nose="neutral",
                    duration_ms=75,
                    loop=True,
                ),
            ],
            "glitch",
        ),
        Sequence(
            "Randomize",
            "misc",
            [n(eye="questionMark", mouth="neutral", nose="neutral")],
            "shuffle",
            note="runtime: RandomizeFace overlay",
        ),
        Sequence(
            "Startup",
            "animated",
            [
                n(empty_all=True, duration_ms=0, note="runtime: particle wipe"),
                n(eye="neutral", mouth="neutral", nose="neutral", duration_ms=0),
            ],
            "crossfade",
        ),
        Sequence("DisplayTest", "misc", [n(fill_all=True)], None),
        Sequence("None", "misc", [n(empty_all=True)], None),
        Sequence("Boop", "overlay", [n(eye="boop", mouth="happy", nose="neutral")], None),
    ]


def cpp_bytes(data: list[int], indent: str = "    ") -> str:
    lines: list[str] = []
    row: list[str] = []
    for i, b in enumerate(data):
        row.append(f"0x{b:02X}")
        if len(row) == 16 or i == len(data) - 1:
            lines.append(indent + ", ".join(row) + ("," if i != len(data) - 1 else ""))
            row = []
    return "\n".join(lines)


def emit_cpp(
    parts: dict[str, dict[str, tuple[int, int, list[int]]]],
    hud: dict[str, tuple[int, int, list[int]]],
    system: dict[str, tuple[int, int, list[int]]],
) -> None:
    hdr = """#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

struct Bitmap {
  const char* id;
  int width;
  int height;
  int stride;
  const std::uint8_t* data;
  std::size_t size;
};

const Bitmap* find_part(std::string_view kind, std::string_view id);
const Bitmap* find_hud(std::string_view id);
const Bitmap* find_system(std::string_view id);

extern const Bitmap kEyes[];
extern const Bitmap kMouths[];
extern const Bitmap kNoses[];
extern const Bitmap kOther[];
extern const Bitmap kHud[];
extern const Bitmap kSystem[];
extern const int kEyeCount;
extern const int kMouthCount;
extern const int kNoseCount;
extern const int kOtherCount;
extern const int kHudCount;
extern const int kSystemCount;

}  // namespace assets
}  // namespace koto
"""
    CPP_HDR.parent.mkdir(parents=True, exist_ok=True)
    CPP_HDR.write_text(hdr, encoding="utf-8")

    src_parts: list[str] = [
        '#include "koto/assets/bitmaps.hpp"',
        "",
        "#include <cstring>",
        "",
        "namespace koto {",
        "namespace assets {",
        "namespace {",
        "",
    ]

    def emit_blob(prefix: str, items: dict[str, tuple[int, int, list[int]]]) -> list[str]:
        table: list[str] = []
        for ident, (w, h, data) in items.items():
            sym = f"k{prefix}_{ident}"
            src_parts.append(f"const std::uint8_t {sym}[] = {{")
            src_parts.append(cpp_bytes(data))
            src_parts.append("};")
            src_parts.append("")
            table.append(
                f'    {{"{ident}", {w}, {h}, {(w + 7) // 8}, {sym}, sizeof({sym})}}'
            )
        return table

    kind_tables = {
        "Eye": emit_blob("Eye", parts["eye"]),
        "Mouth": emit_blob("Mouth", parts["mouth"]),
        "Nose": emit_blob("Nose", parts["nose"]),
        "Other": emit_blob("Other", parts["other"]),
        "Hud": emit_blob("Hud", hud),
        "System": emit_blob("System", system),
    }

    src_parts.append("}  // namespace")
    src_parts.append("")

    def emit_table(name: str, count_name: str, rows: list[str]) -> None:
        src_parts.append(f"const Bitmap {name}[] = {{")
        src_parts.append(",\n".join(rows))
        src_parts.append("};")
        src_parts.append(f"const int {count_name} = static_cast<int>(sizeof({name}) / sizeof({name}[0]));")
        src_parts.append("")

    emit_table("kEyes", "kEyeCount", kind_tables["Eye"])
    emit_table("kMouths", "kMouthCount", kind_tables["Mouth"])
    emit_table("kNoses", "kNoseCount", kind_tables["Nose"])
    emit_table("kOther", "kOtherCount", kind_tables["Other"])
    emit_table("kHud", "kHudCount", kind_tables["Hud"])
    emit_table("kSystem", "kSystemCount", kind_tables["System"])

    src_parts.append(
        """namespace {

const Bitmap* find_in(const Bitmap* table, int count, std::string_view id) {
  for (int i = 0; i < count; ++i) {
    if (id == table[i].id) {
      return &table[i];
    }
  }
  return nullptr;
}

}  // namespace

const Bitmap* find_part(std::string_view kind, std::string_view id) {
  if (kind == "eye") {
    return find_in(kEyes, kEyeCount, id);
  }
  if (kind == "mouth") {
    return find_in(kMouths, kMouthCount, id);
  }
  if (kind == "nose") {
    return find_in(kNoses, kNoseCount, id);
  }
  if (kind == "other") {
    return find_in(kOther, kOtherCount, id);
  }
  return nullptr;
}

const Bitmap* find_hud(std::string_view id) {
  return find_in(kHud, kHudCount, id);
}

const Bitmap* find_system(std::string_view id) {
  return find_in(kSystem, kSystemCount, id);
}

}  // namespace assets
}  // namespace koto
"""
    )
    CPP_SRC.parent.mkdir(parents=True, exist_ok=True)
    CPP_SRC.write_text("\n".join(src_parts), encoding="utf-8")


def parse_system_digits(path: Path) -> dict[str, list[int]]:
    text = path.read_text(encoding="utf-8")
    m = re.search(r"Bitmap digits\[10\]\[3\]\s*\{(.+?)\};", text, re.S)
    if not m:
        return {}
    rows = re.findall(r"\{([^}]+)\}", m.group(1))
    return {str(i): parse_hex_bytes(row) for i, row in enumerate(rows[:10])}


def parse_system_named(path: Path) -> dict[str, list[int]]:
    text = path.read_text(encoding="utf-8")
    found: dict[str, list[int]] = {}
    for name in ("splash1", "splash2", "visor"):
        m = re.search(rf"Bitmap {name}\[\]\s*\{{(.+?)\}};", text, re.S)
        if m:
            found[name] = parse_hex_bytes(m.group(1))
    return found


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--src",
        type=Path,
        default=Path(r"C:\Users\NewLi\Downloads\ToasterBlaster-main\ToasterBlaster-main"),
    )
    args = parser.parse_args()
    src: Path = args.src
    if not src.exists():
        raise SystemExit(f"ToasterBlaster path not found: {src}")

    if ASSETS.exists():
        shutil.rmtree(ASSETS)
    face_parts = ASSETS / "face" / "parts"
    face_seq = ASSETS / "face" / "sequences"
    hud_dir = ASSETS / "hud"
    sys_dir = ASSETS / "system"
    for folder in (face_parts, face_seq, hud_dir, sys_dir / "digits"):
        folder.mkdir(parents=True, exist_ok=True)

    cpp_bitmaps = src / "src" / "Assets" / "Bitmaps" / "Standard.cpp"
    bitmaps = parse_face_bitmaps(cpp_bitmaps)
    copied = copy_part_pngs(src, face_parts)

    packed_parts: dict[str, dict[str, tuple[int, int, list[int]]]] = {
        "eye": {},
        "mouth": {},
        "nose": {},
        "other": {},
    }
    ns_map = {"Eye": "eye", "Mouth": "mouth", "Nose": "nose", "Other": "other"}
    for ns, kind in ns_map.items():
        for name, data in sorted(bitmaps.get(ns, {}).items(), key=lambda kv: natural_key(kv[0])):
            dest = face_parts / kind / f"{name}.png"
            if not dest.exists():
                save_bitmap_png(dest, data)
            grid = bitmap_to_grid(data)
            packed_parts[kind][name] = pack_rows(grid)

    seq_catalog = []
    for seq in sequences():
        frames_meta = []
        for idx, face in enumerate(seq.frames):
            frame = compose_face(face, bitmaps)
            filename = f"{idx:02d}.png"
            png_path = face_seq / seq.seq_id / filename
            write_png_1bit(png_path, 64, 16, lambda x, y, f=frame: frame_on_at(f, x, y))
            item = face_to_catalog(face)
            item["file"] = f"face/sequences/{seq.seq_id}/{filename}"
            frames_meta.append(item)
        entry = {
            "id": seq.seq_id,
            "group": seq.group,
            "transition": seq.transition,
            "frames": frames_meta,
        }
        if seq.note:
            entry["note"] = seq.note
        seq_catalog.append(entry)

    hud_src = src / "src" / "System" / "Peripherals" / "HUD" / "HUDIcons.cpp"
    hud_raw = parse_named_byte_arrays(
        hud_src,
        r"Bitmap\s+(\w+)\[8\]\s*=\s*\{([^}]+)\}",
    )
    packed_hud: dict[str, tuple[int, int, list[int]]] = {}
    hud_catalog = []
    for name, data in hud_raw.items():
        grid = xbm_to_grid(data, 8, 8)
        write_png_1bit(hud_dir / f"{name}.png", 8, 8, lambda x, y, g=grid: g[y][x])
        packed_hud[name] = pack_rows(grid)
        hud_catalog.append({"id": name, "file": f"hud/{name}.png", "width": 8, "height": 8})

    system_cpp = src / "src" / "Assets" / "Bitmaps" / "System.cpp"
    system_raw = parse_system_named(system_cpp)
    system_meta = {
        "splash1": (88, 55, 88),
        "splash2": (30, 20, 32),
        "visor": (54, 38, 56),
    }
    packed_system: dict[str, tuple[int, int, list[int]]] = {}
    system_catalog = []
    for name, (w, h, padded) in system_meta.items():
        data = system_raw[name]
        grid = xbm_to_grid(data, w, h, padded)
        write_png_1bit(sys_dir / f"{name}.png", w, h, lambda x, y, g=grid: g[y][x])
        packed_system[name] = pack_rows(grid)
        system_catalog.append(
            {
                "id": name,
                "file": f"system/{name}.png",
                "width": w,
                "height": h,
                "padded_width": padded,
                "encoding": "xbm",
            }
        )

    digits = parse_system_digits(system_cpp)
    digit_catalog = []
    for ident, cols in digits.items():
        grid = column_digit_to_grid(cols)
        write_png_1bit(sys_dir / "digits" / f"{ident}.png", 3, 8, lambda x, y, g=grid: g[y][x])
        packed_system[f"digit_{ident}"] = pack_rows(grid)
        digit_catalog.append({"id": ident, "file": f"system/digits/{ident}.png", "width": 3, "height": 8})

    parts_catalog = {}
    for kind, ns in (("eye", "Eye"), ("mouth", "Mouth"), ("nose", "Nose"), ("other", "Other")):
        meta = load_metadata(src / "src" / "Assets" / "Bitmaps" / ns)
        exceptions = {
            Path(name).stem for name in meta.get("randomizerExceptions", [])
        }
        items = []
        for name in sorted(bitmaps.get(ns, {}), key=natural_key):
            w = len(bitmaps[ns][name])
            items.append(
                {
                    "id": name,
                    "file": f"face/parts/{kind}/{name}.png",
                    "width": w,
                    "height": 8,
                    "randomizer": name not in exceptions,
                    "copied_from_source": name in copied.get(kind, []),
                }
            )
        parts_catalog[kind] = items

    catalog = {
        "source": "ToasterBlaster",
        "source_path": str(src),
        "pixel_format": "1-bit, on=white",
        "layout": {
            "canvas": {"width": 64, "height": 16},
            "matrix": {"width": 64, "height": 32, "place_y": 8},
            "mirror_default": "LEFT_SIDE (eye_l, nose_l, mouth_l)",
            "modules": [
                {"id": "eye_r", "x": 0, "y": 0, "w": 16, "h": 8},
                {"id": "nose_r", "x": 16, "y": 0, "w": 8, "h": 8},
                {"id": "nose_l", "x": 24, "y": 0, "w": 8, "h": 8},
                {"id": "eye_l", "x": 32, "y": 0, "w": 16, "h": 8},
                {"id": "mouth_r", "x": 0, "y": 8, "w": 32, "h": 8},
                {"id": "mouth_l", "x": 32, "y": 8, "w": 32, "h": 8},
            ],
        },
        "parts": parts_catalog,
        "sequences": seq_catalog,
        "overlays": {
            "boop": {
                "eye": "boop",
                "mouth": "happy",
                "nose": "neutral",
                "mirror_left": True,
                "file": "face/sequences/Boop/00.png",
            },
            "eye_blink": {
                "note": "runtime overlay only (Blink/Translate/Expand tweens), no extra bitmaps",
            },
        },
        "hud": hud_catalog,
        "system": system_catalog,
        "digits": digit_catalog,
    }
    (ASSETS / "catalog.json").write_text(
        json.dumps(catalog, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    emit_cpp(packed_parts, packed_hud, packed_system)

    n_png = len(list(ASSETS.rglob("*.png")))
    n_seq = len(seq_catalog)
    print(f"Imported {n_png} PNGs, {n_seq} sequences -> {ASSETS}")
    print(f"Wrote {CPP_HDR.relative_to(ROOT)}")
    print(f"Wrote {CPP_SRC.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
