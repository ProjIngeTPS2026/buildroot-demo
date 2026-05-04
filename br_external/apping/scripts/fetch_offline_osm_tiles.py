#!/usr/bin/env python3

import math
import pathlib
import sys
import time
import urllib.request


USER_AGENT = "apping-offline-map/1.0"
TILE_SERVER = "https://tile.openstreetmap.org/{z}/{x}/{y}.png"


def lon_to_tile_x(lon: float, zoom: int) -> int:
    return int((lon + 180.0) / 360.0 * (1 << zoom))


def lat_to_tile_y(lat: float, zoom: int) -> int:
    lat_rad = math.radians(lat)
    return int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * (1 << zoom))


def iter_tiles(min_lat: float, max_lat: float, min_lon: float, max_lon: float, zooms):
    for zoom in zooms:
        min_x = lon_to_tile_x(min_lon, zoom)
        max_x = lon_to_tile_x(max_lon, zoom)
        min_y = lat_to_tile_y(max_lat, zoom)
        max_y = lat_to_tile_y(min_lat, zoom)
        for x in range(min_x, max_x + 1):
            for y in range(min_y, max_y + 1):
                yield zoom, x, y


def main() -> int:
    if len(sys.argv) != 8:
        print(
            "usage: fetch_offline_osm_tiles.py OUTPUT_DIR MIN_LAT MAX_LAT MIN_LON MAX_LON ZOOM_START ZOOM_END",
            file=sys.stderr,
        )
        return 1

    output_dir = pathlib.Path(sys.argv[1])
    min_lat = float(sys.argv[2])
    max_lat = float(sys.argv[3])
    min_lon = float(sys.argv[4])
    max_lon = float(sys.argv[5])
    zoom_start = int(sys.argv[6])
    zoom_end = int(sys.argv[7])
    zooms = range(zoom_start, zoom_end + 1)

    tiles = list(iter_tiles(min_lat, max_lat, min_lon, max_lon, zooms))
    print(f"Downloading {len(tiles)} tiles into {output_dir}")

    opener = urllib.request.build_opener()
    opener.addheaders = [("User-Agent", USER_AGENT)]

    for index, (zoom, x, y) in enumerate(tiles, start=1):
        destination = output_dir / str(zoom) / str(x) / f"{y}.png"
        if destination.exists():
            continue

        destination.parent.mkdir(parents=True, exist_ok=True)
        url = TILE_SERVER.format(z=zoom, x=x, y=y)
        with opener.open(url, timeout=20) as response:
            destination.write_bytes(response.read())

        if index % 25 == 0 or index == len(tiles):
            print(f"  {index}/{len(tiles)}")
        time.sleep(0.1)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
