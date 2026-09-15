"""Pack RGB tiles into KPIX blobs for the visor.

Header is two bytes: ncolors (0 = 256) and flags. Geometry lives in the
C++ sprite table, not in the blob. Index 0 is black when black is present.
"""

from __future__ import annotations

FLAG_RLE = 1
FLAG_RAW = 2


def pix_bpp(ncolors: int) -> int:
    if ncolors <= 2:
        return 1
    if ncolors <= 4:
        return 2
    if ncolors <= 16:
        return 4
    return 8


def pack_bits(indices: list[int], bpp: int) -> bytes:
    acc = 0
    nbits = 0
    out = bytearray()
    mask = (1 << bpp) - 1
    for idx in indices:
        acc = (acc << bpp) | (idx & mask)
        nbits += bpp
        while nbits >= 8:
            nbits -= 8
            out.append((acc >> nbits) & 0xFF)
            acc &= (1 << nbits) - 1
    if nbits:
        out.append((acc << (8 - nbits)) & 0xFF)
    return bytes(out)


def pack_rle(indices: list[int]) -> bytes:
    if not indices:
        return b""
    out = bytearray()
    run_idx = indices[0]
    run_len = 1
    for idx in indices[1:]:
        if idx == run_idx and run_len < 255:
            run_len += 1
            continue
        out.append(run_len)
        out.append(run_idx)
        run_idx = idx
        run_len = 1
    out.append(run_len)
    out.append(run_idx)
    return bytes(out)


def pack_pix(width: int, height: int, rgb: bytes) -> bytes:
    if width <= 0 or height <= 0:
        raise ValueError("empty pix tile")
    need = width * height * 3
    if len(rgb) < need:
        raise ValueError(f"rgb length {len(rgb)} < {need}")

    order: list[tuple[int, int, int]] = []
    index_of: dict[tuple[int, int, int], int] = {}

    def add_color(color: tuple[int, int, int]) -> int:
        found = index_of.get(color)
        if found is not None:
            return found
        idx = len(order)
        index_of[color] = idx
        order.append(color)
        return idx

    black = (0, 0, 0)
    has_black = False
    for i in range(0, need, 3):
        color = (rgb[i], rgb[i + 1], rgb[i + 2])
        if color == black:
            has_black = True
            break
    if has_black:
        add_color(black)

    indices: list[int] = []
    for i in range(0, need, 3):
        color = (rgb[i], rgb[i + 1], rgb[i + 2])
        indices.append(add_color(color))

    ncolors = len(order)
    raw = bytes([0, FLAG_RAW]) + rgb[:need]
    if ncolors > 256:
        return raw

    stored = 0 if ncolors == 256 else ncolors
    palette = bytearray()
    for r, g, b in order:
        palette.extend((r, g, b))
    bpp = pix_bpp(ncolors)
    packed = bytes([stored, 0]) + bytes(palette) + pack_bits(indices, bpp)
    rle = bytes([stored, FLAG_RLE]) + bytes(palette) + pack_rle(indices)
    return min((packed, rle, raw), key=len)
