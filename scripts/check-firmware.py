"""Check linked entry points, partition sizes and the merged factory image."""
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parent.parent
build = root / "build"
flash = json.loads((build / "flasher_args.json").read_text())
assert flash["extra_esptool_args"]["chip"] == "esp32s31", "Unexpected target"
symbols = subprocess.check_output(["riscv32-esp-elf-nm", "-C", str(build / "mosicrokey.elf")], text=True)
for name in ["app_main", "create_board()", "Application::Run()", "claw_core_submit"]:
    matches = [line for line in symbols.splitlines() if line.endswith(" T " + name)]
    assert len(matches) == 1, f"{name}: expected one linked implementation, found {len(matches)}"
    print(matches[0])
for name, limit in [("mosicrokey.bin", 6 * 1024 * 1024), ("assets.bin", 8 * 1024 * 1024), ("system.bin", 512 * 1024)]:
    size = (build / name).stat().st_size
    assert 0 < size <= limit, f"{name}: exceeds partition"
    print(f"{name}: {size} bytes / {limit} bytes partition")
version = json.loads((build / "project_description.json").read_text())["project_version"]
image = root / "release" / f"mosicrokey-v{version}-esp32s31.bin"
data = image.read_bytes()
assert len(data) == 16 * 1024 * 1024, "Incorrect factory image length"
previous_end = 0
for offset, filename in sorted(flash["flash_files"].items(), key=lambda entry: int(entry[0], 16)):
    start = int(offset, 16)
    segment = (build / filename).read_bytes()
    assert start >= previous_end and start + len(segment) <= len(data), "Overlapping flash segments"
    assert data[start:start + len(segment)] == segment, f"Merged segment mismatch: {filename}"
    previous_end = start + len(segment)
assert data[0x9000:0xd000] == b"\xff" * 0x4000, "Factory NVS is not blank"
print("PASS: unique entry/factory, real upstream symbols, partition fit, all merged segments and blank factory NVS")
