"""Verify pinned, patched source snapshots without the original checkout."""
import hashlib
import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
lock = json.loads((root / "upstream-lock.json").read_text(encoding="utf-8"))
manifest = json.loads((root / "source-manifest.json").read_text(encoding="utf-8"))
failures = []
count = 0
for name, snapshot in manifest["snapshots"].items():
    if snapshot["commit"] != lock[name]["commit"]:
        failures.append(f"{name}: commit differs from upstream-lock.json")
    base = root / "third_party" / name
    actual = {p.relative_to(base).as_posix() for p in base.rglob("*")
              if p.is_file() and "__pycache__" not in p.parts and p.suffix != ".pyc"}
    expected = snapshot["files"]
    failures.extend(f"{name}: unexpected file {p}" for p in sorted(actual - expected.keys()))
    for relative, digest in expected.items():
        path = base / relative
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            failures.append(f"{name}: missing or changed {relative}")
        count += 1
if failures:
    print("\n".join(failures), file=sys.stderr)
    sys.exit(1)
print(f"Verified {count} pinned source files, including documented MosiCrokey patches")
