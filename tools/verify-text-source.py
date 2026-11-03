#!/usr/bin/env python3
import argparse
import hashlib
import io
import json
import subprocess
import tarfile
import re
import struct
from pathlib import Path


def version(path: Path) -> tuple[int, int, int]:
    text = path.read_text(encoding="utf-8").strip()
    match = re.fullmatch(r"(\d+)\.(\d+)\.(\d+)", text)
    if not match:
        raise SystemExit(f"invalid version: {path}: {text!r}")
    return tuple(int(part) for part in match.groups())


def bmp_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    if len(data) < 26 or data[:2] != b"BM":
        raise SystemExit(f"invalid BMP capture: {path}")
    width, height = struct.unpack_from("<ii", data, 18)
    if width <= 0 or height == 0:
        raise SystemExit(f"invalid BMP dimensions: {path}: {width}x{height}")
    return width, abs(height)


# Verify the committed module without importing the shared workspace's dirty tree.
def verify_text_source(root: Path, canonical: Path) -> None:
    pin=json.loads((Path(__file__).parent / "text-render-source-pin.json").read_text())
    commit=pin["commit"]
    if not re.fullmatch(r"[0-9a-f]{40}",commit): raise SystemExit("invalid text source pin")
    raw=subprocess.check_output(["git","-C",str(canonical),"archive",commit,"kit/kit_render"])
    with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
        names=set()
        for item in archive.getmembers():
            if not item.isfile(): continue
            names.add(item.name)
            path=root/item.name
            if not path.is_file() or path.is_symlink() or path.read_bytes()!=archive.extractfile(item).read():
                raise SystemExit("text source mismatch: "+item.name)
            if hashlib.sha256(path.read_bytes()).hexdigest()!=pin["files"].get(item.name):
                raise SystemExit("text pin manifest mismatch: "+item.name)
    if names!=set(pin["files"]): raise SystemExit("text pin manifest file mismatch")
    print("sCulpt text source contract: "+commit+" files="+str(len(names)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--shared-root", type=Path, default=Path("third_party/codework_shared"))
    parser.add_argument("--canonical-shared-root", type=Path, required=True)
    args = parser.parse_args()
    verify_text_source(args.shared_root.resolve(), args.canonical_shared_root.resolve())
