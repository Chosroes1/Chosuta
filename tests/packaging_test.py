# SPDX-License-Identifier: GPL-3.0-or-later
"""Package ownership regression using synthetic tools, never a real Release."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
import zipfile
import importlib.util

ROOT = Path(__file__).resolve().parents[1]

class PackageTest(unittest.TestCase):
    def test_success_commits_complete_staging(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            bindir = work / "bin"
            bindir.mkdir()
            build = work / "build"
            build.mkdir()
            (build / "CPackConfig.cmake").write_text('set(CPACK_PACKAGE_VERSION "test")\n')
            commands = {
                "cmake": "exit 0",
                "ctest": "exit 0",
                "cpack": 'printf package > "$4/fixture.zip"',
                "ldd": "printf 'fixture dependencies\\n'",
            }
            for name, body in commands.items():
                script = bindir / name
                script.write_text("#!/bin/sh\n" + body + "\n")
                script.chmod(0o700)
            env = dict(os.environ, PATH=str(bindir) + os.pathsep + os.environ["PATH"])
            output = work / "delivery"
            result = subprocess.run(["bash", str(ROOT / "scripts/package-linux.sh"), str(build), str(output)], env=env, capture_output=True)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual((output / "fixture.zip").read_bytes(), b"package")
            self.assertEqual((output / "runtime-dependencies.txt").read_text(), "fixture dependencies\n")
            with zipfile.ZipFile(output / "Chosuta-test-source.zip") as source:
                self.assertTrue(any(name.endswith("src/core/model.h") for name in source.namelist()))
                for helper in ("scripts/btbn-ffmpeg.ps1",):
                    self.assertTrue(any(name.endswith("/" + helper) for name in source.namelist()), helper)
                for asset in ("icons.qrc", "icons/chosuta.png", "icons/chosuta.ico", "icons/chosuta.rc"):
                    matches = [name for name in source.namelist() if name.endswith("/resources/" + asset)]
                    self.assertEqual(len(matches), 1, asset)
                    self.assertEqual(source.read(matches[0]), (ROOT / "resources" / asset).read_bytes())
                self.assertFalse(any("development-" in name or "/svproject/" in name for name in source.namelist()))
            self.assertFalse(list(work.glob(".chosuta-package-*")))

    def test_existing_output_is_untouched(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            output = work / "delivery"
            output.mkdir()
            expected = {"old.zip": b"original zip", "runtime-dependencies.txt": b"original deps"}
            for name, contents in expected.items():
                (output / name).write_bytes(contents)
            result = subprocess.run(["bash", str(ROOT / "scripts/package-linux.sh"), str(work / "build"), str(output)], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((work / "build").exists())
            self.assertEqual({p.name: p.read_bytes() for p in output.iterdir()}, expected)

    def test_packaging_failure_cleans_only_staging(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            bindir = work / "bin"
            bindir.mkdir()
            for name in ("cmake", "ctest", "cpack"):
                script = bindir / name
                script.write_text("#!/bin/sh\nexit " + ("7" if name == "cpack" else "0") + "\n")
                script.chmod(0o700)
            sentinel = work / "keep"
            sentinel.write_bytes(b"user file")
            env = dict(os.environ, PATH=str(bindir) + os.pathsep + os.environ["PATH"])
            result = subprocess.run(["bash", str(ROOT / "scripts/package-linux.sh"), str(work / "build"), str(work / "delivery")], env=env, capture_output=True)
            self.assertEqual(result.returncode, 7)
            self.assertFalse((work / "delivery").exists())
            self.assertFalse(list(work.glob(".chosuta-package-*")))
            self.assertEqual(sentinel.read_bytes(), b"user file")

    def test_icon_audit_does_not_allow_arbitrary_binary_files(self):
        spec = importlib.util.spec_from_file_location("source_audit", ROOT / "scripts/audit-public-source.py")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            for name in ("resources/icons/chosuta.png", "resources/icons/chosuta.ico"):
                path = work / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes((ROOT / name).read_bytes())
            self.assertEqual(module.audit(work)["findings"], [])
            # Even at an allowed name, changing the reviewed bytes requires review.
            icon = work / "resources/icons/chosuta.png"
            icon.write_bytes(icon.read_bytes() + b"unreviewed metadata")
            (work / "another.png").write_bytes((ROOT / "resources/icons/chosuta.png").read_bytes())
            findings = {(row["file"], row["rule"]) for row in module.audit(work)["findings"]}
            self.assertIn(("resources/icons/chosuta.png", "unreviewed-application-icon"), findings)
            self.assertIn(("another.png", "non-text-file"), findings)

if __name__ == "__main__":
    unittest.main()
