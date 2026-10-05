#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Copy the public allowlist to a new directory; do not commit or upload."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys
from public_source import public_files

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("output", type=Path)
parser.add_argument("--report", type=Path, required=True)
args = parser.parse_args()
output = args.output.resolve()
report = args.report.resolve()
if output.exists() or report.is_relative_to(output):
    parser.error("Output must be new; report must be outside the source directory")
files = public_files(root)
output.mkdir(parents=True)
for file in files:
    target = output / file.relative_to(root)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(file, target)
subprocess.run([sys.executable, str(root / "scripts/audit-public-source.py"), str(output), "--report", str(report)], check=True)
print(f"Prepared {len(files)} public source files: {output}")
