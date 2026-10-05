#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Local dynamic build; no tool installation, dependency download, or publishing.
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=${1:-"$project_dir/build-release"}
package_dir=${2:-"$project_dir/out/linux"}
cmake -S "$project_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --parallel 4
ctest --test-dir "$build_dir" --output-on-failure
mkdir -p -- "$package_dir"
cpack --config "$build_dir/CPackConfig.cmake" -B "$package_dir"
ldd "$build_dir/chosuta" > "$package_dir/runtime-dependencies.txt"
package_version=$(sed -n 's/^set(CPACK_PACKAGE_VERSION "\([^"]*\)")/\1/p' "$build_dir/CPackConfig.cmake")
python3 "$project_dir/scripts/package-source.py" "$package_dir/Chosuta-$package_version-source.zip"
