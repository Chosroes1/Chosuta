#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Linux developer check: relocated executables find adjacent tools without PATH.

Only original demo material is used. This is not Windows/DLL validation and
Python is not an application runtime dependency.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--build-dir", type=Path, default=Path("build-release"))
args = parser.parse_args()
if sys.platform != "linux":
    parser.error("this check is Linux-only; use build-windows.ps1 on Windows")
build = args.build_dir.resolve()
for name in ("chosuta", "chosuta-cli"):
    if not (build / name).is_file():
        parser.error(f"missing built executable: {build / name}")
tools = {name: shutil.which(name) for name in ("ffmpeg", "ffprobe")}
if not all(tools.values()):
    parser.error("existing FFmpeg/ffprobe are required for this check")
env = dict(os.environ)
for name in ("QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML2_IMPORT_PATH", "QML_IMPORT_PATH"):
    env.pop(name, None)
env["QT_QPA_PLATFORM"] = "offscreen"
env["PATH"] = ""
with tempfile.TemporaryDirectory(prefix="chosuta-portability-", dir=build) as temporary:
    root = Path(temporary)
    binary = root / "移动程序 空格"
    working = root / "工作目录 別の場所"
    binary.mkdir()
    working.mkdir()
    for name in ("chosuta", "chosuta-cli"):
        shutil.copy2(build / name, binary / name)
    for name, path in tools.items():
        shutil.copy2(path, binary / name)

    def run(name, *arguments):
        result = subprocess.run([str(binary / name), *map(str, arguments)], cwd=working,
                                env=env, text=True, encoding="utf-8", capture_output=True, timeout=45)
        if result.returncode:
            raise RuntimeError(f"{name} failed ({result.returncode}): {result.stderr}")
        return result.stdout

    version = run("chosuta-cli", "--version").strip()
    assets = working / "原创素材"
    run("chosuta-cli", "demo", assets)
    project = working / "动画.chosuta"
    video = working / "短片 $(literal).mp4"
    run("chosuta-cli", "generate", assets / "demo.svp", project, "--assets", assets,
        "--size", "64x64", "--duration", "1", "--fps", "4")
    run("chosuta-cli", "export", project, video)
    metadata = json.loads(run("ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0",
                              "-show_entries", "stream=width,height,r_frame_rate,nb_read_frames",
                              "-of", "json", video))["streams"][0]
    if (metadata["width"], metadata["height"], metadata["r_frame_rate"], metadata["nb_read_frames"]) != (64, 64, "4/1", "4"):
        raise RuntimeError(f"unexpected video metadata: {metadata}")
    run("chosuta", "--smoke-test")
    evidence = {"platform": "Linux", "windowsValidated": False, "version": version,
                "PATH": "empty", "cwdDifferentFromExecutable": True, "unicodeAndSpaces": True,
                "cliExport": metadata, "guiSmokeExit": 0,
                "scope": "adjacent external tools; Qt/ICU remain existing system libraries"}
    path = build / "portable-tools-validation.json"
    path.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Adjacent tools / Unicode / relocated CLI and GUI: passed; evidence: {path}")
