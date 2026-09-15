"""Pack Bad Apple!! into BA1P for the visor (64×32, 1-bit, 25 fps).

    python tools/badapple_pack.py

Reads source.mp4 (Downloads folder or BADAPPLE_SOURCE), extracts with
ffmpeg -r 25, then writes assets/badapple.ba1p. Does not subsample
frames. zlib is not used.
"""

from __future__ import annotations

import json
import os
import struct
import subprocess
import sys
from pathlib import Path

import imageio_ffmpeg
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
BUILD = ROOT / "build"
PACKED = ASSETS / "badapple.ba1p"
META = ASSETS / "badapple.json"
RAW = BUILD / "badapple_64x32_1bit.bin"

WIDTH = 64
HEIGHT = 32
FPS = 25
THRESHOLD = 128
BPF = WIDTH * HEIGHT // 8
RAW_SENTINEL = 0xFF
HEADER_FMT = "<4sHHBHH"

DOWNLOADS = Path.home() / "Downloads" / "bad-apple-64x128"


def find_source() -> Path:
    named = [
        Path(p)
        for p in (
            os.environ.get("BADAPPLE_SOURCE", ""),
            str(DOWNLOADS / "source.mp4"),
            str(ASSETS / "badapple_source.mp4"),
        )
        if p
    ]
    for path in named:
        if path.is_file():
            return path
    raise SystemExit(
        "missing source.mp4 — set BADAPPLE_SOURCE or place it in "
        f"{DOWNLOADS}"
    )


def pack_horizontal_msb(gray: np.ndarray) -> bytes:
    bits = (gray >= THRESHOLD).astype(np.uint8)
    packed = np.packbits(bits, axis=1, bitorder="big")
    return packed.tobytes()


def extract_raw(src: Path) -> np.ndarray:
    BUILD.mkdir(parents=True, exist_ok=True)
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    cmd = [
        ffmpeg,
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(src),
        "-vf",
        f"scale={WIDTH}:{HEIGHT}:flags=lanczos,format=gray",
        "-r",
        str(FPS),
        "-f",
        "rawvideo",
        "-pix_fmt",
        "gray",
        "pipe:1",
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert proc.stdout is not None
    frame_size = WIDTH * HEIGHT
    frames: list[bytes] = []
    with RAW.open("wb") as out:
        while True:
            raw = proc.stdout.read(frame_size)
            if len(raw) < frame_size:
                break
            gray = np.frombuffer(raw, dtype=np.uint8).reshape(HEIGHT, WIDTH)
            packed = pack_horizontal_msb(gray)
            if len(packed) != BPF:
                raise RuntimeError(f"unexpected packed size {len(packed)}")
            out.write(packed)
            frames.append(packed)
            if len(frames) % 500 == 0:
                print(f"frames: {len(frames)}", flush=True)
    stderr = proc.stderr.read().decode("utf-8", errors="replace") if proc.stderr else ""
    ret = proc.wait()
    if ret != 0:
        print(stderr, file=sys.stderr)
        raise SystemExit(f"ffmpeg failed with code {ret}")
    if not frames:
        raise SystemExit("ffmpeg produced no frames")
    return np.frombuffer(b"".join(frames), dtype=np.uint8).reshape(-1, BPF).copy()


def encode_stream(frames: np.ndarray) -> bytes:
    prev = np.zeros(BPF, dtype=np.uint8)
    chunks = [struct.pack(HEADER_FMT, b"BA1P", WIDTH, HEIGHT, FPS, len(frames), BPF)]
    for frame in frames:
        changed = np.flatnonzero(frame != prev)
        n = int(changed.size)
        if n == 0:
            chunks.append(b"\x00")
        elif n >= RAW_SENTINEL or (1 + 2 * n) >= (1 + BPF):
            chunks.append(bytes([RAW_SENTINEL]) + frame.tobytes())
        else:
            payload = bytearray([n])
            for idx in changed:
                payload.append(int(idx))
                payload.append(int(frame[idx]))
            chunks.append(bytes(payload))
        prev = frame
    return b"".join(chunks)


def decode_stream(packed: bytes) -> np.ndarray:
    magic, width, height, fps, count, bpf = struct.unpack_from(HEADER_FMT, packed)
    if magic != b"BA1P":
        raise RuntimeError("bad magic")
    if width != WIDTH or height != HEIGHT or bpf != BPF:
        raise RuntimeError(f"bad geometry {width}x{height} bpf={bpf}")
    i = struct.calcsize(HEADER_FMT)
    prev = np.zeros(bpf, dtype=np.uint8)
    frames = np.empty((count, bpf), dtype=np.uint8)
    for idx in range(count):
        n = packed[i]
        i += 1
        if n == RAW_SENTINEL:
            prev = np.frombuffer(packed[i : i + bpf], dtype=np.uint8).copy()
            i += bpf
        else:
            for _ in range(n):
                pos = packed[i]
                val = packed[i + 1]
                i += 2
                prev[pos] = val
        frames[idx] = prev
    if i != len(packed):
        raise RuntimeError(f"trailing {len(packed) - i}")
    return frames, fps


def pack() -> None:
    src = find_source()
    print(f"source {src}")
    frames = extract_raw(src)
    packed = encode_stream(frames)
    decoded, fps = decode_stream(packed)
    if not np.array_equal(decoded, frames):
        raise RuntimeError("roundtrip failed")
    if fps != FPS:
        raise RuntimeError(f"fps {fps} != {FPS}")
    if len(packed) > 1024 * 1024:
        raise SystemExit(f"packed {len(packed)} B exceeds 1 MiB")
    PACKED.write_bytes(packed)
    report = {
        "file": PACKED.name,
        "source": src.name,
        "frames": int(len(frames)),
        "fps": FPS,
        "width": WIDTH,
        "height": HEIGHT,
        "bpf": BPF,
        "raw_bytes": int(frames.size),
        "packed_bytes": len(packed),
        "avg_bytes_per_frame": round(len(packed) / len(frames), 2),
        "fits_1mb": True,
        "format": "BA1P sparse byte-patch, no zlib",
        "note": "Bad Apple!! PV is a third-party asset, not original kotoproto art",
    }
    META.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    pack()
