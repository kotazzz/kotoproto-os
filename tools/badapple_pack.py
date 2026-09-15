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
MASK_SENTINEL = 0xFE
MASK_BYTES = BPF // 8
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
        patch_cost = 1 + 2 * n
        mask_cost = 1 + MASK_BYTES + n
        raw_cost = 1 + BPF
        if n == 0:
            chunks.append(b"\x00")
        elif n > 32 and mask_cost < raw_cost and mask_cost <= patch_cost:
            mask = bytearray(MASK_BYTES)
            values = bytearray()
            for idx in changed:
                index = int(idx)
                mask[index >> 3] |= 0x80 >> (index & 7)
                values.append(int(frame[index]))
            chunks.append(bytes([MASK_SENTINEL]) + bytes(mask) + bytes(values))
        elif n < RAW_SENTINEL and patch_cost < raw_cost:
            payload = bytearray([n])
            for idx in changed:
                payload.append(int(idx))
                payload.append(int(frame[idx]))
            chunks.append(bytes(payload))
        else:
            chunks.append(bytes([RAW_SENTINEL]) + frame.tobytes())
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
        elif n == MASK_SENTINEL:
            mask = packed[i : i + MASK_BYTES]
            i += MASK_BYTES
            for byte_i in range(bpf):
                bit = 0x80 >> (byte_i & 7)
                if mask[byte_i >> 3] & bit:
                    prev[byte_i] = packed[i]
                    i += 1
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
    src_name = "badapple.ba1p"
    frames = None
    for path in (
        Path(os.environ.get("BADAPPLE_SOURCE", "")),
        Path.home() / "Downloads" / "bad-apple-64x128" / "source.mp4",
        ASSETS / "badapple_source.mp4",
    ):
        if path and path.is_file():
            print(f"source {path}")
            frames = extract_raw(path)
            src_name = path.name
            break
    if frames is None:
        if not PACKED.exists():
            raise SystemExit(
                "missing source.mp4 and assets/badapple.ba1p — set BADAPPLE_SOURCE"
            )
        print(f"transcode {PACKED}")
        frames, fps = decode_stream(PACKED.read_bytes())
        if fps != FPS:
            raise RuntimeError(f"fps {fps} != {FPS}")
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
        "source": src_name,
        "frames": int(len(frames)),
        "fps": FPS,
        "width": WIDTH,
        "height": HEIGHT,
        "bpf": BPF,
        "raw_bytes": int(frames.size),
        "packed_bytes": len(packed),
        "avg_bytes_per_frame": round(len(packed) / len(frames), 2),
        "fits_1mb": True,
        "format": "BA1P patch / 0xFE byte-mask / raw, no zlib",
        "note": "Bad Apple!! PV is a third-party asset, not original kotoproto art",
    }
    META.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    pack()
