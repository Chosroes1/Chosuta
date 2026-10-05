#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only credential/privacy checks; reports locations, never matched values.

This is a local heuristic check plus a manifest check, not a proof that arbitrary
future files contain no secrets. It uploads nothing and uses no external tools.
"""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re


def audit(root: Path) -> dict:
    findings = []
    records = []
    blocked_parts = {".git", ".ssh", ".aws", ".codex", ".agents", "svproject", "maca_tachie", "__pycache__", "out"}
    blocked_names = {".env", ".netrc", ".npmrc", ".pypirc", "credentials", "id_rsa", "id_ed25519", "private_samples.inc"}
    blocked_suffixes = {".pem", ".key", ".p12", ".pfx", ".kdbx", ".pyc", ".log", ".exe", ".dll", ".zip", ".zst"}
    patterns = {
        "private-key": re.compile(r"-----BEGIN (?:[A-Z0-9 ]*PRIVATE KEY|PGP PRIVATE KEY BLOCK)-----"),
        "github-token": re.compile(r"\b(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,})\b"),
        "aws-access-key": re.compile(r"\b(?:AKIA|ASIA)[A-Z0-9]{16}\b"),
        "google-api-key": re.compile(r"\bAIza[A-Za-z0-9_-]{30,}\b"),
        "slack-token": re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{20,}\b"),
        "credential-in-url": re.compile(r"https?://[^\s/:]+:[^\s/@]+@"),
        "credential-assignment": re.compile(r'''(?i)\b(?:password|passwd|api[_-]?key|client[_-]?secret|access[_-]?token|secret[_-]?key)\s*[=:]\s*["']([^"'\s]{6,})["']'''),
        "local-personal-path": re.compile(r"(?:/" + r"home/[^\s/]+/|/" + r"Users/[^\s/]+/|[A-Za-z]:\\Users\\[^\s\\]+\\)"),
    }
    candidates = re.compile(r'''["']([A-Za-z0-9_+/=-]{32,})["']''')
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        name = relative.as_posix()
        if path.is_symlink():
            findings.append({"file": name, "rule": "symlink"})
            continue
        if any(p in blocked_parts for p in relative.parts) or any(p.startswith("build") for p in relative.parent.parts) or (path.is_dir() and path.name.startswith("build")) or path.name in blocked_names or path.name.startswith(".env.") or path.suffix in blocked_suffixes:
            findings.append({"file": name, "rule": "excluded-file-or-directory"})
        if not path.is_file():
            continue
        data = path.read_bytes()
        records.append({"file": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
        try:
            content = data.decode("utf-8")
        except UnicodeDecodeError:
            findings.append({"file": name, "rule": "non-text-file"})
            continue
        for rule, regex in patterns.items():
            for match in regex.finditer(content):
                findings.append({"file": name, "line": content.count("\n", 0, match.start()) + 1, "rule": rule})
        for match in candidates.finditer(content):
            value = match.group(1)
            # Hash literals are checked for credential context above, not flagged
            # just because a published SHA-256 has high entropy.
            if re.fullmatch(r"[A-Fa-f0-9]{32,128}", value):
                continue
            frequencies = Counter(value)
            entropy = -sum((n / len(value)) * math.log2(n / len(value)) for n in frequencies.values())
            if entropy >= 4.4:
                findings.append({"file": name, "line": content.count("\n", 0, match.start()) + 1, "rule": "high-entropy-literal"})
    return {"filesChecked": len(records), "findings": findings, "files": records,
            "checks": list(patterns) + ["excluded-paths", "text-only", "high-entropy-literals"],
            "uploaded": False, "scope": "Current directory snapshot; future edits require a new check."}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    root = args.directory.resolve()
    report = args.report.resolve()
    if not root.is_dir() or report.is_relative_to(root):
        parser.error("Use an existing source directory and a report path outside it")
    result = audit(root)
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"filesChecked": result["filesChecked"], "findings": result["findings"]}, ensure_ascii=False))
    raise SystemExit(bool(result["findings"]))
