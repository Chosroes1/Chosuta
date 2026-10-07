# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared public-source allowlist; never traverse private or build directories."""
from pathlib import Path

ROOT_FILES = (
    "CMakeLists.txt", "LICENSE", "README.md",
    "THIRD_PARTY_NOTICES.md", ".gitignore",
)
DOC_FILES = (
    "building.zh-CN.md", "user-guide.zh-CN.md", "windows.zh-CN.md",
    "packaging.zh-CN.md", "license-review.zh-CN.md", "publication.zh-CN.md",
)


def public_files(root: Path) -> list[Path]:
    paths = [root / name for name in ROOT_FILES]
    # Some published repositories keep only the author's main README.
    # Never create or replace translations merely to prepare their source.
    paths.extend(root / name for name in ("README.en.md", "README.ja.md")
                 if (root / name).exists() or (root / name).is_symlink())
    paths.extend(root / "docs" / name for name in DOC_FILES)
    paths.extend(root / "tests" / name for name in ("core_test.cpp", "ui_test.cpp", "fixtures/basic.svp"))
    paths.extend(root / "resources" / name for name in (
        "data.qrc", "english.tsv", "japanese.tsv", "chinese.tsv", "icons/chosuta.svg", "licenses/ICU.txt",
        "licenses/Qt-GPL-3.0.txt", "licenses/Qt-LGPL-3.0.txt",
    ))
    for directory, suffixes in (
        ("src", {".cpp", ".h"}), ("cmake", {".in"}),
        ("packaging", {".in", ".desktop"}), ("scripts", {".py", ".ps1", ".sh"}),
    ):
        paths.extend(p for p in (root / directory).rglob("*")
                     if p.is_file() and p.suffix in suffixes and "__pycache__" not in p.parts)
    for path in paths:
        if not path.is_file() or path.is_symlink():
            raise ValueError(f"Missing file or symlink in public manifest: {path.relative_to(root)}")
        for parent in path.parents:
            if parent == root:
                break
            if parent.is_symlink():
                raise ValueError(f"Symlink parent: {path.relative_to(root)}")
    return sorted(set(paths))
