"""Offline proof that recorded patches restore the pinned upstream file hashes."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parent.parent
lock = json.loads((root / "upstream-lock.json").read_text())
provenance = json.loads((root / "patches/provenance.json").read_text())
patches = {"xiaozhi": "xiaozhi-mosicrokey.patch", "esp-claw": "espclaw-glm-auth.patch"}
count = 0
for snapshot, patch in patches.items():
    directory = root / "build/patch-verify" / snapshot
    items = [item for item in provenance["files"] if item["snapshot"] == snapshot]
    if not items:
        raise ValueError("Missing patch provenance: " + snapshot)
    for item in items:
        relative = Path(item["path"])
        if relative.is_absolute() or ".." in relative.parts or item["commit"] != lock[snapshot]["commit"]:
            raise ValueError("Invalid patch provenance path or commit")
        source = root / "third_party" / snapshot / relative
        if hashlib.sha256(source.read_bytes()).hexdigest() != item["patched_sha256"]:
            raise ValueError("Patched source differs from provenance: " + item["path"])
        destination = directory / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    # Avoid Windows global Git autocrlf rewriting restored upstream bytes.
    subprocess.run(["git", "-c", "core.autocrlf=false", "-c", "core.eol=lf",
                    "apply", "--reverse", str(root / "patches" / patch)], cwd=directory, check=True)
    for item in items:
        if hashlib.sha256((directory / item["path"]).read_bytes()).hexdigest() != item["pristine_sha256"]:
            raise ValueError("Patch does not restore pinned upstream content: " + item["path"])
        count += 1
print(f"Verified complete patches restore {count} pinned upstream files byte-for-byte; no network used")
