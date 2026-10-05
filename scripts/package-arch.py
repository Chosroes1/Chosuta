#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a local Arch package using existing tools only; no install or upload."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", nargs="?", type=Path, default=root / "out/arch/0.2.2")
args = parser.parse_args()
for tool in ("makepkg", "cmake", "ninja", "fakeroot"):
    if not shutil.which(tool):
        parser.error(f"missing {tool}; install requires separate authorization")
output = args.output.resolve()
if output.exists():
    parser.error("output already exists; choose a new owned directory")
version = re.search(r"project\(Chosuta VERSION ([0-9.]+)", (root / "CMakeLists.txt").read_text()).group(1)
output.mkdir(parents=True)
archive = output / f"Chosuta-{version}-source.zip"
subprocess.run([sys.executable, str(root / "scripts/package-source.py"), str(archive)], check=True)
checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
recipe = (root / "packaging/arch/PKGBUILD.in").read_text()
recipe = recipe.replace("@VERSION@", version).replace("@SHA256@", checksum)
(output / "PKGBUILD").write_text(recipe)
srcinfo = subprocess.run(["makepkg", "--printsrcinfo"], cwd=output, check=True, text=True, capture_output=True)
(output / ".SRCINFO").write_text(srcinfo.stdout)
# Deliberately omit -s/--syncdeps and -i/--install. Missing deps fail visibly.
subprocess.run(["makepkg", "--noconfirm"], cwd=output, check=True)
print(f"Local package and source: {output}")
