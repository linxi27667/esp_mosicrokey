"""Export the device's actual game source through read-only paginated diagnostics.

This archives source on the PC. It does not install or persist a game on TF.
"""
import argparse
import json
from pathlib import Path
import time
import serial


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("id")
    parser.add_argument("output")
    parser.add_argument("--port", default="COM8")
    args = parser.parse_args()
    if not args.id or len(args.id) > 32 or any(c not in "abcdefghijklmnopqrstuvwxyz0123456789-_" for c in args.id):
        raise ValueError("Invalid game id")
    link = serial.Serial()
    link.port, link.baudrate, link.timeout = args.port, 115200, 0.1
    link.dtr = link.rts = False
    link.open()
    source, offset, revision = [], 0, None
    try:
        for _ in range(17):
            link.write(f"game_source {args.id} {offset}\n".encode())
            link.flush()
            buffer = bytearray()
            deadline, page = time.monotonic() + 10, None
            while time.monotonic() < deadline and page is None:
                buffer.extend(link.read(link.in_waiting or 1))
                while b"\n" in buffer:
                    line, _, rest = buffer.partition(b"\n")
                    buffer = bytearray(rest)
                    prefix = b"MOSI_SOURCE_JSON "
                    if line.startswith(prefix):
                        page = json.loads(line[len(prefix):])
                        break
            if page is None:
                raise RuntimeError("No source page; check id and device status")
            if page.get("id") != args.id or (revision is not None and page.get("revision") != revision):
                raise RuntimeError("Game changed during export; retry")
            revision = page["revision"]
            part = page["source"]
            if page["next_offset"] != offset + len(part.encode("utf-8")):
                raise RuntimeError("Invalid source pagination")
            source.append(part)
            offset = page["next_offset"]
            if page["eof"]:
                output = Path(args.output)
                output.parent.mkdir(parents=True, exist_ok=True)
                output.write_text("".join(source), encoding="utf-8", newline="")
                print(f"Exported device source id={args.id} revision={revision}, {offset} UTF-8 bytes; {output}")
                return
        raise RuntimeError("Source exceeds 64 KiB export limit")
    finally:
        link.close()


if __name__ == "__main__":
    main()
