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


def verify_text_source(root: Path, canonical: Path) -> None:
    pin = json.loads((Path(__file__).parent / "text-render-source-pin.json").read_text())
    commit, files = pin["commit"], pin["files"]
    if not re.fullmatch(r"[0-9a-f]{40}", commit) or not files:
        raise SystemExit("invalid text source pin")
    vendored = {}
    for name, expected in files.items():
        path = root / name
        if (not name.startswith("kit/kit_render/") or not path.resolve().is_relative_to(root)
                or path.is_symlink() or not path.is_file()):
            raise SystemExit("missing or invalid pinned text source: " + name)
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != expected:
            raise SystemExit("text pin manifest mismatch: " + name)
        vendored[name] = data

    # The release sandbox copies pinned inputs without the sibling Git history.
    # When the checkout is present, also verify the pin against its exact commit.
    evidence = "vendored-pin"
    git_marker = canonical / ".git"
    if git_marker.exists() or git_marker.is_symlink():
        raw = subprocess.check_output(["git", "-C", str(canonical), "archive", commit, "kit/kit_render"])
        with tarfile.open(fileobj=io.BytesIO(raw)) as archive:
            archived = {item.name: archive.extractfile(item).read()
                        for item in archive.getmembers() if item.isfile()}
        if archived.keys() != vendored.keys():
            raise SystemExit("text pin manifest file mismatch")
        for name, data in archived.items():
            if data != vendored[name]:
                raise SystemExit("text source mismatch: " + name)
        evidence = "vendored-pin+git-archive"
    print(f"sCulpt text source contract: {commit} files={len(vendored)} evidence={evidence}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--shared-root", type=Path, default=Path("third_party/codework_shared"))
    parser.add_argument("--canonical-shared-root", type=Path, required=True)
    args = parser.parse_args()
    verify_text_source(args.shared_root.resolve(), args.canonical_shared_root.resolve())
