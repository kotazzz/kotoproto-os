"""Pack hand-authored assets into firmware C++ tables.

All authored files live in assets/: emotions.json, RGB face PNGs, and
black/white OLED sprites (visor, splash1, logo).

A frame in emotions.json may omit `file` to hold a black screen (PowerOff,
flash-off). This script does not regenerate faces from Toaster Blaster or
any other source.
"""

from __future__ import annotations

import json
import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
EMOTIONS_JSON = ASSETS / "emotions.json"
GEN_CPP = ROOT / "firmware" / "src" / "assets" / "emotions.cpp"
GEN_BITMAPS = ROOT / "firmware" / "src" / "assets" / "bitmaps.cpp"

SYSTEM_SPRITES = (
    ("visor", "visor.png"),
    ("splash1", "splash1.png"),
    ("logo", "logo.png"),
)

FACE_W = 64
FACE_H = 32
HUD_W = 42
HUD_H = 16
HUD_BYTES = (HUD_W + 7) // 8 * HUD_H

CPP_KIND = {"classic": "Kind::Classic", "special": "Kind::Special"}
CPP_EFFECT = {
    "none": "Effect::None",
    "snarl": "Effect::Snarl",
    "dizzy": "Effect::Dizzy",
    "wink": "Effect::Wink",
    "randomize": "Effect::Randomize",
}
CPP_TRANS = {
    "none": "face::TransitionKind::None",
    "blink": "face::TransitionKind::Blink",
    "crossfade": "face::TransitionKind::Crossfade",
    "drop": "face::TransitionKind::Drop",
    "slide": "face::TransitionKind::Slide",
    "glitch": "face::TransitionKind::Glitch",
    "explode": "face::TransitionKind::Explode",
    "fizz": "face::TransitionKind::Fizz",
    "doomMelt": "face::TransitionKind::DoomMelt",
    "losePower": "face::TransitionKind::LosePower",
    "earthquake": "face::TransitionKind::Earthquake",
    "shuffle": "face::TransitionKind::Shuffle",
}


def paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def unfilter(data: bytes, width: int, height: int, bpp: int) -> bytes:
    stride = width * bpp
    out = bytearray(height * stride)
    i = 0
    for y in range(height):
        ftype = data[i]
        i += 1
        row = bytearray(data[i : i + stride])
        i += stride
        for x in range(stride):
            left = row[x - bpp] if x >= bpp else 0
            up = out[(y - 1) * stride + x] if y else 0
            ul = out[(y - 1) * stride + x - bpp] if y and x >= bpp else 0
            if ftype == 1:
                row[x] = (row[x] + left) & 255
            elif ftype == 2:
                row[x] = (row[x] + up) & 255
            elif ftype == 3:
                row[x] = (row[x] + ((left + up) // 2)) & 255
            elif ftype == 4:
                row[x] = (row[x] + paeth(left, up, ul)) & 255
        out[y * stride : (y + 1) * stride] = row
    return bytes(out)


def read_png_rgb(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG: {path}")
    pos = 8
    width = height = bit_depth = color_type = 0
    raw = bytearray()
    palette = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        tag = data[pos + 4 : pos + 8]
        chunk_data = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if tag == b"IHDR":
            width, height, bit_depth, color_type, *_ = struct.unpack(">IIBBBBB", chunk_data)
        elif tag == b"PLTE":
            palette = chunk_data
        elif tag == b"IDAT":
            raw.extend(chunk_data)
        elif tag == b"IEND":
            break
    decoded = zlib.decompress(bytes(raw))
    if bit_depth != 8:
        raise ValueError(f"unsupported PNG bit depth {bit_depth} in {path}")
    if color_type == 2:
        pixels = unfilter(decoded, width, height, 3)
        return width, height, pixels
    if color_type == 6:
        src = unfilter(decoded, width, height, 4)
        rgb = bytearray(width * height * 3)
        for i in range(width * height):
            rgb[i * 3] = src[i * 4]
            rgb[i * 3 + 1] = src[i * 4 + 1]
            rgb[i * 3 + 2] = src[i * 4 + 2]
        return width, height, bytes(rgb)
    if color_type == 0:
        src = unfilter(decoded, width, height, 1)
        rgb = bytearray(width * height * 3)
        for i, v in enumerate(src):
            rgb[i * 3] = rgb[i * 3 + 1] = rgb[i * 3 + 2] = v
        return width, height, bytes(rgb)
    if color_type == 3:
        src = unfilter(decoded, width, height, 1)
        rgb = bytearray(width * height * 3)
        for i, idx in enumerate(src):
            p = idx * 3
            rgb[i * 3] = palette[p]
            rgb[i * 3 + 1] = palette[p + 1]
            rgb[i * 3 + 2] = palette[p + 2]
        return width, height, bytes(rgb)
    raise ValueError(f"unsupported PNG color type {color_type} in {path}")


def chunk(tag: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def write_png_rgb(path: Path, width: int, height: int, rgb: bytes) -> None:
    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        raw.extend(rgb[y * stride : (y + 1) * stride])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def write_png_bw(path: Path, width: int, height: int, on_at) -> None:
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
    ihdr = struct.pack(">IIBBBBB", width, height, 1, 0, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def _png_chunks(path: Path) -> tuple[int, int, int, int, bytes, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not a PNG: {path}")
    pos = 8
    width = height = bit_depth = color_type = 0
    raw = bytearray()
    palette = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        tag = data[pos + 4 : pos + 8]
        chunk_data = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if tag == b"IHDR":
            width, height, bit_depth, color_type, *_ = struct.unpack(">IIBBBBB", chunk_data)
        elif tag == b"PLTE":
            palette = chunk_data
        elif tag == b"IDAT":
            raw.extend(chunk_data)
        elif tag == b"IEND":
            break
    return width, height, bit_depth, color_type, bytes(palette), bytes(raw)


def _require_bw(path: Path, r: int, g: int, b: int) -> bool:
    if r == 0 and g == 0 and b == 0:
        return False
    if r == 255 and g == 255 and b == 255:
        return True
    raise ValueError(f"{path} has a non black/white pixel ({r},{g},{b})")


def read_png_bw(path: Path) -> tuple[int, int, list[list[bool]]]:
    width, height, bit_depth, color_type, palette, idat = _png_chunks(path)
    decoded = zlib.decompress(idat)
    rows: list[list[bool]] = []
    if bit_depth == 1 and color_type == 0:
        stride = 1 + (width + 7) // 8
        pixels = unfilter(decoded, (width + 7) // 8, height, 1)
        packed_stride = (width + 7) // 8
        for y in range(height):
            row: list[bool] = []
            for x in range(width):
                byte = pixels[y * packed_stride + (x // 8)]
                row.append(((byte >> (7 - (x & 7))) & 1) != 0)
            rows.append(row)
        return width, height, rows
    if bit_depth != 8:
        raise ValueError(f"unsupported PNG bit depth {bit_depth} in {path}")
    if color_type == 2:
        src = unfilter(decoded, width, height, 3)
        for y in range(height):
            row = []
            for x in range(width):
                i = (y * width + x) * 3
                row.append(_require_bw(path, src[i], src[i + 1], src[i + 2]))
            rows.append(row)
        return width, height, rows
    if color_type == 6:
        src = unfilter(decoded, width, height, 4)
        for y in range(height):
            row = []
            for x in range(width):
                i = (y * width + x) * 4
                row.append(_require_bw(path, src[i], src[i + 1], src[i + 2]))
            rows.append(row)
        return width, height, rows
    if color_type == 0:
        src = unfilter(decoded, width, height, 1)
        for y in range(height):
            row = []
            for x in range(width):
                v = src[y * width + x]
                row.append(_require_bw(path, v, v, v))
            rows.append(row)
        return width, height, rows
    if color_type == 3:
        src = unfilter(decoded, width, height, 1)
        for y in range(height):
            row = []
            for x in range(width):
                idx = src[y * width + x]
                row.append(_require_bw(path, palette[idx * 3], palette[idx * 3 + 1], palette[idx * 3 + 2]))
            rows.append(row)
        return width, height, rows
    raise ValueError(f"unsupported PNG color type {color_type} in {path}")


def pack_bw_bits(rows: list[list[bool]]) -> tuple[int, int, int, bytes]:
    height = len(rows)
    width = len(rows[0]) if height else 0
    stride = (width + 7) // 8
    out = bytearray(stride * height)
    for y, row in enumerate(rows):
        for x, on in enumerate(row):
            if on:
                out[y * stride + (x // 8)] |= 0x80 >> (x & 7)
    return width, height, stride, bytes(out)


def sample_rgb(rgb: bytes, x: int, y: int, src_w: int, src_h: int) -> tuple[int, int, int]:
    x = min(max(x, 0), src_w - 1)
    y = min(max(y, 0), src_h - 1)
    i = (y * src_w + x) * 3
    return rgb[i], rgb[i + 1], rgb[i + 2]


def blit_scaled(dst: list[list[bool]], dx: int, dy: int, dw: int, dh: int, rgb: bytes, sx: int, sy: int, sw: int, sh: int) -> None:
    for y in range(dh):
        src_y = sy + y * sh // dh
        for x in range(dw):
            src_x = sx + x * sw // dw
            r, g, b = sample_rgb(rgb, src_x, src_y, FACE_W, FACE_H)
            dst[dy + y][dx + x] = (r | g | b) > 20


def make_hud(rgb: bytes) -> bytes:
    bits = [[False] * HUD_W for _ in range(HUD_H)]
    blit_scaled(bits, 7, 0, 16, 8, rgb, 0, 0, 32, 16)
    blit_scaled(bits, 29, 0, 8, 8, rgb, 48, 0, 16, 16)
    blit_scaled(bits, 5, 8, 32, 8, rgb, 0, 16, 64, 16)
    stride = (HUD_W + 7) // 8
    out = bytearray(stride * HUD_H)
    for y in range(HUD_H):
        for x in range(HUD_W):
            if bits[y][x]:
                out[y * stride + (x // 8)] |= 0x80 >> (x & 7)
    return bytes(out)


def cpp_bytes(data: bytes, indent: str = "    ") -> str:
    lines: list[str] = []
    row: list[str] = []
    last = len(data) - 1
    for i, b in enumerate(data):
        row.append(f"0x{b:02X}")
        if len(row) == 16 or i == last:
            lines.append(indent + ", ".join(row) + ("," if i != last else ""))
            row = []
    return "\n".join(lines)


def load_frame_rgb(filename: str) -> bytes:
    path = ASSETS / filename
    width, height, rgb = read_png_rgb(path)
    if width != FACE_W or height != FACE_H:
        raise ValueError(f"{path} is {width}x{height}, expected {FACE_W}x{FACE_H}")
    return rgb


def emotion_frames(emo: dict) -> list[dict]:
    frames = emo.get("frames")
    if not frames:
        return [{"ms": 500}]
    return list(frames)


def emit_cpp(catalog: dict) -> None:
    chunks = [
        "// GENERATED by tools/pack_assets.py — do not edit.",
        '#include "koto/assets/emotions.hpp"',
        "",
        "namespace koto {",
        "namespace assets {",
        "namespace {",
        "",
        "const std::uint8_t kHud_Empty[] = {",
        cpp_bytes(bytes(HUD_BYTES)),
        "};",
        "",
    ]
    emotion_rows: list[str] = []
    for emo in catalog["emotions"]:
        ident = re.sub(r"[^A-Za-z0-9_]", "_", emo["id"])
        frames = emotion_frames(emo)
        frame_idents: list[tuple[str | None, int]] = []
        first_file = ""
        for idx, frame in enumerate(frames):
            filename = str(frame.get("file") or "")
            ms = int(frame.get("ms", 500))
            if not filename:
                frame_idents.append((None, ms))
                continue
            if not first_file:
                first_file = filename
            rgb = load_frame_rgb(filename)
            rgb_name = f"kRgb_{ident}_{idx:02d}"
            chunks.append(f"const std::uint8_t {rgb_name}[] = {{")
            chunks.append(cpp_bytes(rgb))
            chunks.append("};")
            chunks.append("")
            frame_idents.append((rgb_name, ms))
        if first_file:
            hud_name = f"kHud_{ident}"
            chunks.append(f"const std::uint8_t {hud_name}[] = {{")
            chunks.append(cpp_bytes(make_hud(load_frame_rgb(first_file))))
            chunks.append("};")
            chunks.append("")
        else:
            hud_name = "kHud_Empty"
        table_name = f"kFrames_{ident}"
        chunks.append(f"const EmotionFrame {table_name}[] = {{")
        rows = []
        for rgb_name, ms in frame_idents:
            if rgb_name is None:
                rows.append(f"    {{nullptr, 0, {ms}}}")
            else:
                rows.append(f"    {{{rgb_name}, sizeof({rgb_name}), {ms}}}")
        chunks.append(",\n".join(rows))
        chunks.append("};")
        chunks.append("")
        accent = emo.get("accent", [90, 220, 255])
        emotion_rows.append(
            "    {"
            f'"{emo["id"]}", "{emo.get("label", emo["id"])}", "{emo.get("short", emo["id"])}", '
            f'{CPP_KIND[emo["kind"]]}, {CPP_EFFECT.get(emo["effect"], "Effect::None")}, '
            f'{CPP_TRANS.get(emo.get("transition", "blink"), "face::TransitionKind::Blink")}, '
            f'Color{{{int(accent[0])}, {int(accent[1])}, {int(accent[2])}}}, '
            f'{"true" if emo.get("loop", True) else "false"}, '
            f'{"true" if emo.get("allow_blink", emo["kind"] == "classic") else "false"}, '
            f'{"true" if emo.get("allow_boop", emo["kind"] == "classic") else "false"}, '
            f"{table_name}, {len(frames)}, {hud_name}, sizeof({hud_name})"
            "}"
        )

    chunks.append("}  // namespace")
    chunks.append("")
    chunks.append("const Emotion kEmotions[] = {")
    chunks.append(",\n".join(emotion_rows))
    chunks.append("};")
    chunks.append("const int kEmotionCount = static_cast<int>(sizeof(kEmotions) / sizeof(kEmotions[0]));")
    chunks.append("")
    chunks.append("const char* const kFaceSets[3][8] = {")
    for row in catalog["sets"]:
        inner = ", ".join(f'"{name}"' for name in row)
        chunks.append(f"    {{{inner}}},")
    chunks.append("};")
    chunks.append("")
    auto = catalog["auto"]
    chunks.append("const char* const kAutoFaces[] = {")
    chunks.append("    " + ", ".join(f'"{name}"' for name in auto))
    chunks.append("};")
    chunks.append(f"const int kAutoFaceCount = {len(auto)};")
    chunks.append("")
    boot = catalog["boot"]
    chunks.append("const char* const kBootFaces[] = {")
    chunks.append("    " + ", ".join(f'"{name}"' for name in boot))
    chunks.append("};")
    chunks.append(f"const int kBootFaceCount = {len(boot)};")
    chunks.append(
        """
const Emotion* find_emotion(std::string_view id) {
  for (int i = 0; i < kEmotionCount; ++i) {
    if (id == kEmotions[i].id) {
      return &kEmotions[i];
    }
  }
  return nullptr;
}

const Emotion* emotion_at(int index) {
  if (index < 0 || index >= kEmotionCount) {
    return nullptr;
  }
  return &kEmotions[index];
}

const char* kind_name(Kind kind) {
  return kind == Kind::Special ? "special" : "classic";
}

const char* effect_name(Effect effect) {
  switch (effect) {
    case Effect::Snarl:
      return "snarl";
    case Effect::Dizzy:
      return "dizzy";
    case Effect::Wink:
      return "wink";
    case Effect::Randomize:
      return "randomize";
    default:
      return "none";
  }
}

}  // namespace assets
}  // namespace koto
"""
    )
    GEN_CPP.write_text("\n".join(chunks), encoding="utf-8")
    print(f"wrote {GEN_CPP} ({len(catalog['emotions'])} emotions)")


def emit_bitmaps() -> None:
    chunks = [
        "// GENERATED by tools/pack_assets.py — do not edit.",
        '#include "koto/assets/bitmaps.hpp"',
        "",
        "#include <string_view>",
        "",
        "namespace koto {",
        "namespace assets {",
        "namespace {",
        "",
    ]
    table_rows: list[str] = []
    for sprite_id, filename in SYSTEM_SPRITES:
        path = ASSETS / filename
        if not path.exists():
            raise SystemExit(f"missing {path}")
        width, height, rows = read_png_bw(path)
        packed_w, packed_h, stride, packed = pack_bw_bits(rows)
        ident = re.sub(r"[^A-Za-z0-9_]", "_", sprite_id)
        name = f"kSystem_{ident}"
        chunks.append(f"const std::uint8_t {name}[] = {{")
        chunks.append(cpp_bytes(packed))
        chunks.append("};")
        chunks.append("")
        table_rows.append(
            f'    {{"{sprite_id}", {packed_w}, {packed_h}, {stride}, {name}, sizeof({name})}},'
        )
    chunks.append("}  // namespace")
    chunks.append("")
    chunks.append("const Bitmap kSystem[] = {")
    chunks.append("\n".join(table_rows))
    chunks.append("};")
    chunks.append("const int kSystemCount = static_cast<int>(sizeof(kSystem) / sizeof(kSystem[0]));")
    chunks.append(
        """
const Bitmap* find_system(std::string_view id) {
  for (int i = 0; i < kSystemCount; ++i) {
    if (id == kSystem[i].id) {
      return &kSystem[i];
    }
  }
  return nullptr;
}

}  // namespace assets
}  // namespace koto
"""
    )
    GEN_BITMAPS.write_text("\n".join(chunks), encoding="utf-8")
    print(f"wrote {GEN_BITMAPS} ({len(SYSTEM_SPRITES)} sprites)")


def pack() -> None:
    if not EMOTIONS_JSON.exists():
        raise SystemExit(f"missing {EMOTIONS_JSON}")
    catalog = json.loads(EMOTIONS_JSON.read_text(encoding="utf-8"))
    emit_cpp(catalog)
    emit_bitmaps()


if __name__ == "__main__":
    pack()
