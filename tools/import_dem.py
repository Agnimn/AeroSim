"""Import a DEM GeoTIFF into an offline Qt Quick 3D heightmap package.

Requires rasterio and numpy. The input DEM must be licensed for the project.
The generated PNG stores normalized elevation samples; the JSON sidecar stores
CRS, vertical datum, and the original elevation range used by the renderer.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import zlib
from pathlib import Path


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return (struct.pack(">I", len(data)) + kind + data +
            struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))


def write_grayscale_png(path: Path, pixels) -> None:
    height, width = pixels.shape
    rows = bytearray()
    for row in pixels:
        rows.append(0)
        rows.extend(int(value) for value in row)
    header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + png_chunk(b"IHDR", header)
    png += png_chunk(b"IDAT", zlib.compress(bytes(rows), level=9))
    png += png_chunk(b"IEND", b"")
    path.write_bytes(png)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="licensed DEM GeoTIFF")
    parser.add_argument("--output-dir", type=Path, default=Path("assets/terrain"))
    parser.add_argument("--name", default="north_india_dem")
    parser.add_argument(
        "--bbox", nargs=4, type=float, metavar=("MIN_LON", "MIN_LAT", "MAX_LON", "MAX_LAT"),
        default=(70.0, 24.0, 84.0, 35.4),
        help="target WGS84 bounds; defaults to the North India test region",
    )
    parser.add_argument("--vertical-datum", required=True,
                        help="vertical datum reported by the DEM, for example EGM2008")
    parser.add_argument("--max-size", type=int, default=512,
                        help="maximum raster dimension after reprojection")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    try:
        import numpy as np
        import rasterio
        from rasterio.enums import Resampling
        from rasterio.transform import from_bounds
        from rasterio.warp import reproject, transform_bounds
    except ImportError as error:
        raise SystemExit(
            "This importer requires rasterio and numpy. Install them in the "
            "tool environment before importing a DEM."
        ) from error

    if not args.input.is_file():
        raise SystemExit(f"DEM file does not exist: {args.input}")
    if args.max_size < 2:
        raise SystemExit("--max-size must be at least 2")

    minimum_longitude, minimum_latitude, maximum_longitude, maximum_latitude = args.bbox
    if not (minimum_longitude < maximum_longitude and minimum_latitude < maximum_latitude):
        raise SystemExit("--bbox must be min_lon min_lat max_lon max_lat")

    with rasterio.open(args.input) as source:
        if source.crs is None:
            raise SystemExit("DEM has no CRS; refuse to guess its geographic coordinates")
        source_bounds = transform_bounds(source.crs, "EPSG:4326", *source.bounds)
        if (source_bounds[2] <= minimum_longitude or source_bounds[0] >= maximum_longitude or
                source_bounds[3] <= minimum_latitude or source_bounds[1] >= maximum_latitude):
            raise SystemExit("DEM does not intersect the requested WGS84 bounds")

        longitude_span = maximum_longitude - minimum_longitude
        latitude_span = maximum_latitude - minimum_latitude
        if longitude_span >= latitude_span:
            width = args.max_size
            height = max(2, round(args.max_size * latitude_span / longitude_span))
        else:
            height = args.max_size
            width = max(2, round(args.max_size * longitude_span / latitude_span))
        destination = np.full((height, width), np.nan, dtype="float32")
        reproject(
            source=rasterio.band(source, 1),
            destination=destination,
            src_transform=source.transform,
            src_crs=source.crs,
            src_nodata=source.nodata,
            dst_transform=from_bounds(minimum_longitude, minimum_latitude,
                                      maximum_longitude, maximum_latitude,
                                      width, height),
            dst_crs="EPSG:4326",
            dst_nodata=np.nan,
            resampling=Resampling.bilinear,
        )
        finite = np.isfinite(destination)
        if not finite.all():
            raise SystemExit("Reprojected DEM contains nodata in the requested bounds")
        minimum_elevation = float(destination.min())
        maximum_elevation = float(destination.max())
        if not math.isfinite(minimum_elevation) or not math.isfinite(maximum_elevation):
            raise SystemExit("DEM elevation range is not finite")
        if maximum_elevation <= minimum_elevation:
            raise SystemExit("DEM has no usable elevation range")

        normalized = np.rint(
            (destination - minimum_elevation) /
            (maximum_elevation - minimum_elevation) * 255.0
        ).clip(0, 255).astype("uint8")

        args.output_dir.mkdir(parents=True, exist_ok=True)
        heightmap_path = args.output_dir / f"{args.name}.png"
        metadata_path = args.output_dir / f"{args.name}.json"
        write_grayscale_png(heightmap_path, normalized)
        metadata = {
            "source_file": str(args.input),
            "source_crs": str(source.crs),
            "target_crs": "EPSG:4326",
            "vertical_datum": args.vertical_datum,
            "vertical_units": "meters",
            "minimum_elevation_meters": minimum_elevation,
            "maximum_elevation_meters": maximum_elevation,
            "width": width,
            "height": height,
            "bbox_wgs84": [minimum_longitude, minimum_latitude,
                           maximum_longitude, maximum_latitude],
            "encoding": "8-bit grayscale normalized from the DEM elevation range",
        }
        metadata_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="ascii")
        print(f"Wrote {heightmap_path} ({width}x{height})")
        print(f"Wrote {metadata_path}")
        print(f"Source CRS: {source.crs}; target CRS: EPSG:4326")
        print(f"Vertical datum: {args.vertical_datum}; units: meters")
        print(f"Elevation range: {minimum_elevation:.2f}..{maximum_elevation:.2f} m")


if __name__ == "__main__":
    main()
