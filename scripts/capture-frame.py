"""Decode a frozen RGB565 frame emitted by the board's `frame` command.

All 128 rows and the FNV-1a checksum must match. Pillow is only needed for PNG.
"""
import argparse
import base64
from pathlib import Path
import re
import struct


def decode(text):
    begin = re.search(r"MOSI_FRAME_BEGIN (\d+) (\d+) ([0-9a-f]{8})", text)
    if not begin:
        raise ValueError("No device frame header")
    width, height = map(int, begin.group(1, 2))
    if (width, height) != (160, 128):
        raise ValueError("Unexpected canvas dimensions")
    end = text.find("MOSI_FRAME_END", begin.end())
    if end < 0:
        raise ValueError("Incomplete device frame")
    rows = {}
    for match in re.finditer(r"MOSI_FRAME_ROW (\d+) ([A-Za-z0-9+/=]+)", text[begin.end():end]):
        y = int(match.group(1))
        row = base64.b64decode(match.group(2), validate=True)
        if y in rows or y >= height or len(row) != width * 2:
            raise ValueError("Invalid or duplicate device row")
        rows[y] = row
    if len(rows) != height:
        raise ValueError(f"Missing device rows: {len(rows)}/{height}")
    pixels = b"".join(rows[y] for y in range(height))
    checksum = 2166136261
    for value in pixels:
        checksum = ((checksum ^ value) * 16777619) & 0xffffffff
    if checksum != int(begin.group(3), 16):
        raise ValueError("Device frame checksum mismatch")
    return width, height, pixels, checksum


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log")
    parser.add_argument("output")
    args = parser.parse_args()
    width, height, pixels, checksum = decode(Path(args.log).read_text(encoding="utf-8"))
    from PIL import Image
    rgb = []
    for (pixel,) in struct.iter_unpack("<H", pixels):
        rgb.append(((pixel >> 11) * 255 // 31, ((pixel >> 5) & 63) * 255 // 63, (pixel & 31) * 255 // 31))
    image = Image.new("RGB", (width, height))
    image.putdata(rgb)
    image.save(args.output)
    print(f"Verified actual board frame {width}x{height}, FNV1a={checksum:08x}; saved {args.output}")


if __name__ == "__main__":
    main()
