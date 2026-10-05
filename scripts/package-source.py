#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Create a reviewed Chosuta source archive without private/local assets.

Development helper only. Python is not an application runtime dependency.
"""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import argparse
import re
from public_source import public_files

root = Path(__file__).resolve().parent.parent
version = re.search(r"project\(Chosuta VERSION ([0-9.]+)", (root / "CMakeLists.txt").read_text()).group(1)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
args = parser.parse_args()
output = args.output.resolve()
if output.exists():
    parser.error("output already exists; choose a new archive name")
files = public_files(root)
output.parent.mkdir(parents=True, exist_ok=True)
with ZipFile(output, "x", compression=ZIP_DEFLATED) as archive:
    for file in sorted(set(files)):
        archive.write(file, f"Chosuta-{version}/" + file.relative_to(root).as_posix())
print(output)
