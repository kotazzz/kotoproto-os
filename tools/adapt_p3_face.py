"""Scale MAX7219 face parts (8/16/32 x 8) onto one P3 64x32 half-face.

One panel stores the left half. The right panel is the same bitmap
mirrored horizontally.

Upper half is 8x2 modules. Eye occupies the first 8 (4x2), nose the last 4 (2x2).

  eye    32x16 at ( 0,  0)   first 8 modules, 2x of 16x8
  nose   16x16 at (48,  0)   last 4 modules of the upper half, 2x of 8x8
  mouth  64x16 at ( 0, 16)   lower 16 modules, 2x of 32x8
"""

from __future__ import annotations

import json
import shutil
import struct
import zlib
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PARTS_SRC = ROOT / "assets" / "face" / "parts"
CATALOG_SRC = ROOT / "assets" / "catalog.json"
P3 = ROOT / "assets" / "face" / "p3"
EDITOR = ROOT / "tools" / "face"

CANVAS_W = 64
CANVAS_H = 32
SCALE = 2

REGIONS = {
    "eye": (0, 0, 32, 16),
    "nose": (48, 0, 16, 16),
    "mouth": (0, 16, 64, 16),
}

NATIVE = {"eye": (16, 8), "nose": (8, 8), "mouth": (32, 8), "other": (32, 8)}


def load_1bit(path: Path) -> Image.Image:
    return Image.open(path).convert("1", dither=Image.Dither.NONE)


def scale2(im: Image.Image) -> Image.Image:
    return im.resize((im.width * SCALE, im.height * SCALE), Image.Resampling.NEAREST)


def crop_to(im: Image.Image, width: int, height: int) -> Image.Image:
    w = min(width, im.width)
    h = min(height, im.height)
    cropped = im.crop((0, 0, w, h))
    if w == width and h == height:
        return cropped
    out = Image.new("1", (width, height), 0)
    out.paste(cropped, (0, 0))
    return out


def load_part(kind: str, name: str) -> Image.Image:
    direct = PARTS_SRC / kind / f"{name}.png"
    if direct.exists():
        return crop_to(load_1bit(direct), *NATIVE[kind])
    for fallback in ("other", "mouth", "eye", "nose"):
        cand = PARTS_SRC / fallback / f"{name}.png"
        if cand.exists():
            return crop_to(load_1bit(cand), *NATIVE[kind])
    raise FileNotFoundError(f"{kind}/{name}")


def blit_on(dst: Image.Image, src: Image.Image, x: int, y: int) -> None:
    for sy in range(src.height):
        dy = y + sy
        if dy < 0 or dy >= dst.height:
            continue
        for sx in range(src.width):
            dx = x + sx
            if dx < 0 or dx >= dst.width:
                continue
            if src.getpixel((sx, sy)):
                dst.putpixel((dx, dy), 1)


def write_png_1bit(path: Path, im: Image.Image) -> None:
    bw = im.convert("1", dither=Image.Dither.NONE)
    width, height = bw.size
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        byte = 0
        bit = 7
        for x in range(width):
            if bw.getpixel((x, y)):
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


def pack_rows(im: Image.Image) -> list[int]:
    width, height = im.size
    stride = (width + 7) // 8
    out: list[int] = []
    for y in range(height):
        for s in range(stride):
            byte = 0
            for bit in range(8):
                x = s * 8 + bit
                if x < width and im.getpixel((x, y)):
                    byte |= 1 << (7 - bit)
            out.append(byte)
    return out


def grid_to_modules(im: Image.Image) -> list[list[int]]:
    modules = [[0] * 8 for _ in range(32)]
    for y in range(CANVAS_H):
        for x in range(CANVAS_W):
            if not im.getpixel((x, y)):
                continue
            m = (y // 8) * 8 + (x // 8)
            r = y % 8
            c = x % 8
            modules[m][r] |= 1 << (7 - c)
    return modules


def compose_face(frame: dict, scaled: dict[str, dict[str, Image.Image]]) -> Image.Image:
    canvas = Image.new("1", (CANVAS_W, CANVAS_H), 0)
    if frame.get("empty_all"):
        return canvas
    if frame.get("fill_all"):
        return Image.new("1", (CANVAS_W, CANVAS_H), 1)

    def take(kind: str, name: str | None, flip180: bool = False) -> Image.Image | None:
        if not name:
            return None
        im = scaled[kind][name].copy()
        if flip180:
            im = im.transpose(Image.Transpose.ROTATE_180)
        return im

    eye = take("eye", frame.get("eye_r") or frame.get("eye"))
    nose = take("nose", frame.get("nose_r") or frame.get("nose"))
    mouth = take("mouth", frame.get("mouth_r") or frame.get("mouth"), bool(frame.get("flip_mouth")))
    if eye:
        blit_on(canvas, eye, *REGIONS["eye"][:2])
    if mouth:
        blit_on(canvas, mouth, *REGIONS["mouth"][:2])
    if nose:
        blit_on(canvas, nose, *REGIONS["nose"][:2])
    return canvas


def visor_pair(half: Image.Image) -> Image.Image:
    pair = Image.new("1", (CANVAS_W * 2, CANVAS_H), 0)
    pair.paste(half, (0, 0))
    pair.paste(half.transpose(Image.Transpose.FLIP_LEFT_RIGHT), (CANVAS_W, 0))
    return pair


def cpp_bytes(data: list[int], indent: str = "    ") -> str:
    lines: list[str] = []
    row: list[str] = []
    for i, b in enumerate(data):
        row.append(f"0x{b:02X}")
        if len(row) == 16 or i == len(data) - 1:
            lines.append(indent + ", ".join(row) + ("," if i != len(data) - 1 else ""))
            row = []
    return "\n".join(lines)


def emit_cpp(frames: list[tuple[str, int, int, list[int]]]) -> None:
    hdr = ROOT / "firmware" / "include" / "koto" / "assets" / "face_p3.hpp"
    src = ROOT / "firmware" / "src" / "assets" / "face_p3.cpp"
    hdr.parent.mkdir(parents=True, exist_ok=True)
    src.parent.mkdir(parents=True, exist_ok=True)
    hdr.write_text(
        """#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace koto {
namespace assets {

// One P3 64x32 left half-face. The right panel is this bitmap flipped on X.

struct FaceFrame {
  const char* sequence;
  int index;
  int duration_ms;
  const std::uint8_t* data;
  std::size_t size;
};

extern const FaceFrame kFaceFrames[];
extern const int kFaceFrameCount;

const FaceFrame* find_face_frame(std::string_view sequence, int index);

}  // namespace assets
}  // namespace koto
""",
        encoding="utf-8",
    )

    chunks: list[str] = [
        '#include "koto/assets/face_p3.hpp"',
        "",
        "namespace koto {",
        "namespace assets {",
        "namespace {",
        "",
    ]
    table: list[str] = []
    for seq, idx, duration, data in frames:
        ident = f"kFace_{seq}_{idx:02d}"
        chunks.append(f"const std::uint8_t {ident}[] = {{")
        chunks.append(cpp_bytes(data))
        chunks.append("};")
        chunks.append("")
        table.append(
            f'    {{"{seq}", {idx}, {duration}, {ident}, sizeof({ident})}}'
        )
    chunks.append("}  // namespace")
    chunks.append("")
    chunks.append("const FaceFrame kFaceFrames[] = {")
    chunks.append(",\n".join(table))
    chunks.append("};")
    chunks.append(
        "const int kFaceFrameCount = static_cast<int>(sizeof(kFaceFrames) / sizeof(kFaceFrames[0]));"
    )
    chunks.append(
        """
const FaceFrame* find_face_frame(std::string_view sequence, int index) {
  for (int i = 0; i < kFaceFrameCount; ++i) {
    if (sequence == kFaceFrames[i].sequence && index == kFaceFrames[i].index) {
      return &kFaceFrames[i];
    }
  }
  return nullptr;
}

}  // namespace assets
}  // namespace koto
"""
    )
    src.write_text("\n".join(chunks), encoding="utf-8")


def contact_sheet(images: list[tuple[str, Image.Image]], path: Path, cols: int = 4) -> None:
    if not images:
        return
    pad = 2
    cell_w, cell_h = CANVAS_W * 2, CANVAS_H
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("1", (cols * (cell_w + pad) + pad, rows * (cell_h + pad + 8) + pad), 0)
    for i, (_name, im) in enumerate(images):
        r, c = divmod(i, cols)
        x = pad + c * (cell_w + pad)
        y = pad + r * (cell_h + pad + 8)
        sheet.paste(visor_pair(im), (x, y))
    write_png_1bit(path, sheet)


def main() -> None:
    catalog = json.loads(CATALOG_SRC.read_text(encoding="utf-8"))
    if P3.exists():
        shutil.rmtree(P3)
    (P3 / "parts").mkdir(parents=True)
    (P3 / "sequences").mkdir(parents=True)
    EDITOR.mkdir(parents=True, exist_ok=True)

    scaled: dict[str, dict[str, Image.Image]] = {"eye": {}, "mouth": {}, "nose": {}, "other": {}}
    parts_out = []
    for kind in ("eye", "mouth", "nose", "other"):
        for item in catalog["parts"][kind]:
            name = item["id"]
            src = load_part(kind, name)
            dst = scale2(src)
            scaled[kind][name] = dst
            rel = P3 / "parts" / kind / f"{name}.png"
            write_png_1bit(rel, dst)
            parts_out.append(
                {
                    "id": name,
                    "kind": kind,
                    "file": f"face/p3/parts/{kind}/{name}.png",
                    "width": dst.width,
                    "height": dst.height,
                    "source": item["file"],
                }
            )
            if kind != "other":
                scaled[kind].setdefault(name, dst)
        if kind == "other":
            for name, im in scaled["other"].items():
                w, h = NATIVE["eye"]
                scaled["eye"].setdefault(name, scale2(crop_to(load_part("other", name), w, h)))
                w, h = NATIVE["nose"]
                scaled["nose"].setdefault(name, scale2(crop_to(load_part("other", name), w, h)))
                w, h = NATIVE["mouth"]
                scaled["mouth"].setdefault(name, scale2(crop_to(load_part("other", name), w, h)))

    for kind in ("eye", "mouth", "nose"):
        for name in list(catalog["parts"]["mouth"]):
            ident = name["id"] if isinstance(name, dict) else name
            if ident not in scaled[kind]:
                try:
                    scaled[kind][ident] = scale2(load_part(kind, ident))
                except FileNotFoundError:
                    pass

    extra_names = set()
    for seq in catalog["sequences"]:
        for frame in seq["frames"]:
            for key in ("eye", "eye_l", "eye_r", "mouth", "mouth_l", "mouth_r", "nose", "nose_l", "nose_r"):
                if frame.get(key):
                    extra_names.add((key.split("_")[0], frame[key]))
    for kind, name in extra_names:
        if name not in scaled[kind]:
            scaled[kind][name] = scale2(load_part(kind, name))
            write_png_1bit(P3 / "parts" / kind / f"{name}.png", scaled[kind][name])

    project = {
        "canvas": {"width": CANVAS_W, "height": CANVAS_H},
        "layout": {
            "scale": SCALE,
            "panel": "half-face 64x32",
            "mirror": "right panel = flip X of left master",
            "origin": "x=0 outer/eye, x=48 snout",
            "modules": [
                {"id": key, "x": box[0], "y": box[1], "w": box[2], "h": box[3]}
                for key, box in REGIONS.items()
            ],
        },
        "animations": [],
    }
    seq_meta = []
    cpp_frames: list[tuple[str, int, int, list[int]]] = []
    sheet_items: list[tuple[str, Image.Image]] = []

    for seq in catalog["sequences"]:
        frames_js = []
        names = []
        durations = []
        out_frames = []
        for idx, frame in enumerate(seq["frames"]):
            im = compose_face(frame, scaled)
            filename = f"{idx:02d}.png"
            write_png_1bit(P3 / "sequences" / seq["id"] / filename, im)
            frames_js.append(grid_to_modules(im))
            names.append(filename)
            durations.append(int(frame.get("duration_ms", 500)))
            cpp_frames.append((seq["id"], idx, int(frame.get("duration_ms", 500)), pack_rows(im)))
            item = dict(frame)
            item["file"] = f"face/p3/sequences/{seq['id']}/{filename}"
            out_frames.append(item)
            if idx == 0:
                sheet_items.append((seq["id"], im))
        project["animations"].append(
            {
                "name": seq["id"],
                "group": seq.get("group", "basic"),
                "transition": seq.get("transition"),
                "frames": frames_js,
                "frameNames": names,
                "durations": durations,
            }
        )
        seq_meta.append(
            {
                "id": seq["id"],
                "group": seq.get("group", "basic"),
                "transition": seq.get("transition"),
                "frames": out_frames,
            }
        )

    p3_catalog = {
        "standard": "p3-64x32-half",
        "canvas": {"width": CANVAS_W, "height": CANVAS_H},
        "scale": SCALE,
        "layout": project["layout"],
        "parts": parts_out,
        "sequences": seq_meta,
    }
    (P3 / "catalog.json").write_text(
        json.dumps(p3_catalog, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    catalog["layout_p3"] = {
        "canvas": {"width": CANVAS_W, "height": CANVAS_H},
        "scale": SCALE,
        "panel": "half-face 64x32",
        "mirror": "right panel = flip X of left master",
        "origin": "x=0 outer/eye, x=48 snout",
        "modules": project["layout"]["modules"],
        "catalog": "face/p3/catalog.json",
    }
    CATALOG_SRC.write_text(json.dumps(catalog, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    contact_sheet(sheet_items, P3 / "sheet.png")
    EDITOR.mkdir(parents=True, exist_ok=True)
    (EDITOR / "project.js").write_text(
        "window.PRELOADED_PROJECT = "
        + json.dumps(project, separators=(",", ":"))
        + ";\n",
        encoding="utf-8",
    )
    emit_cpp(cpp_frames)

    cmake = ROOT / "firmware" / "CMakeLists.txt"
    text = cmake.read_text(encoding="utf-8")
    if "src/assets/face_p3.cpp" not in text:
        cmake.write_text(text.replace("src/assets/bitmaps.cpp", "src/assets/bitmaps.cpp\n  src/assets/face_p3.cpp"), encoding="utf-8")

    n_png = len(list(P3.rglob("*.png")))
    print(f"P3 visor {CANVAS_W}x{CANVAS_H}: {n_png} PNGs, {len(seq_meta)} sequences -> {P3}")
    print(f"Editor data -> {EDITOR / 'project.js'}")


if __name__ == "__main__":
    main()
