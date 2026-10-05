#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create a release copy without local BUILDINFO; preserve payload/PKGINFO.

makepkg's original BUILDINFO contains personal build paths and a full host
package inventory. Keep that original privately; ship a separate release copy.
This does not install packages or upload anything.
"""
import argparse
import gzip
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("input", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
source = args.input.resolve()
output = args.output.resolve()
if not source.is_file() or output.exists():
    parser.error("Input must exist and output must be new")
listing = subprocess.run(["bsdtar", "-tf", str(source)], capture_output=True, text=True, check=True)
for name in listing.stdout.splitlines():
    if Path(name).is_absolute() or ".." in Path(name).parts:
        parser.error("Unsafe archive member path")
with tempfile.TemporaryDirectory(prefix="chosuta-release-package-") as folder:
    stage = Path(folder)
    subprocess.run(["bsdtar", "-xf", str(source), "-C", str(stage)], check=True)
    if any(p.is_symlink() for p in stage.rglob("*")):
        parser.error("Unexpected symbolic link in application package")
    (stage / ".BUILDINFO").unlink()
    mtree = stage / ".MTREE"
    text = gzip.decompress(mtree.read_bytes()).decode()
    lines = [line for line in text.splitlines(keepends=True) if not line.startswith("./.BUILDINFO ")]
    if len(lines) != len(text.splitlines(keepends=True)) - 1:
        parser.error("Expected exactly one BUILDINFO entry in MTREE")
    mtree.write_bytes(gzip.compress("".join(lines).encode(), mtime=0))
    output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["bsdtar", "--uid", "0", "--gid", "0", "--zstd", "-cf", str(output),
                    "-C", str(stage), ".PKGINFO", ".MTREE", "usr"], check=True)
print(f"Release package created; BUILDINFO omitted, application payload unchanged: {output}")
