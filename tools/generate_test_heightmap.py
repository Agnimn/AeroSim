"""Generate the small, dependency-free grayscale prototype terrain heightmap."""

from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path

WIDTH = 129
HEIGHT = 129
OUTPUT = Path(__file__).resolve().parents[1] / "assets" / "terrain" / "test_heightmap.png"

# Normalized mountain centers (x, y), amplitude, and radius. The overlapping
# smooth peaks and broad ridges give HeightFieldGeometry visible, varied relief.
PEAKS = (
    (0.30, 0.32, 0.95, 0.14),
    (0.52, 0.25, 0.72, 0.11),
    (0.72, 0.39, 0.88, 0.16),
    (0.39, 0.62, 0.66, 0.18),
    (0.68, 0.72, 0.98, 0.13),
    (0.18, 0.76, 0.55, 0.14),
)


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def grayscale_height(x: int, y: int) -> int:
    nx = x / (WIDTH - 1)
    ny = y / (HEIGHT - 1)
    height = 0.11 + 0.035 * math.sin(nx * math.tau * 2.2) * math.cos(ny * math.tau * 1.8)

    for peak_x, peak_y, amplitude, radius in PEAKS:
        distance_squared = ((nx - peak_x) ** 2 + (ny - peak_y) ** 2) / (radius**2)
        height += amplitude * math.exp(-2.3 * distance_squared)

    # Gentle ridges add landscape variation without sharp pixel noise.
    height += 0.07 * max(0.0, math.sin((nx * 1.1 + ny * 0.82) * math.tau * 2.5))
    return max(0, min(255, round(20 + min(1.0, height) * 220)))


def main() -> None:
    rows = bytearray()
    for y in range(HEIGHT):
        rows.append(0)  # PNG filter: None
        rows.extend(grayscale_height(x, y) for x in range(WIDTH))

    header = struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 0, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header)
    png += png_chunk(b"IDAT", zlib.compress(bytes(rows), level=9))
    png += png_chunk(b"IEND", b"")

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(png)
    print(f"Wrote {WIDTH}x{HEIGHT} 8-bit grayscale heightmap: {OUTPUT}")


if __name__ == "__main__":
    main()
