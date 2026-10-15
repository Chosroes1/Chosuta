#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Local dynamic build; no tool installation, dependency download, or publishing.
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-"$project_dir/build-release"}
package_dir=${2:-"$project_dir/out/linux"}
# Check the entire destination before configure/build/cpack can write anything.
if [[ -e "$package_dir" || -L "$package_dir" ]]; then
    echo "Output already exists; choose a new package directory: $package_dir" >&2
    exit 1
fi
cmake -S "$project_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build "$build_dir" --parallel 4
mkdir -p -- "$(dirname -- "$package_dir")"
staging=$(mktemp -d -- "$(dirname -- "$package_dir")/.chosuta-package-XXXXXX")
trap 'rm -rf -- "$staging"' EXIT
cpack --config "$build_dir/CPackConfig.cmake" -B "$staging"
ldd "$build_dir/chosuta" > "$staging/runtime-dependencies.txt"
package_version=$(sed -n 's/^set(CPACK_PACKAGE_VERSION "\([^"]*\)")/\1/p' "$build_dir/CPackConfig.cmake")
[[ -n "$package_version" ]] || { echo 'Missing package version' >&2; exit 1; }
python3 "$project_dir/scripts/package-source.py" "$staging/Chosuta-$package_version-source.zip"
# GNU mv's no-clobber directory rename also protects a destination created meanwhile.
mv -T -n -- "$staging" "$package_dir"
if [[ -d "$staging" ]]; then
    echo "Output appeared during packaging; kept it untouched: $package_dir" >&2
    exit 1
fi
trap - EXIT
