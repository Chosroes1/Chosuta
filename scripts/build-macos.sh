#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Local macOS build with existing SDKs. Compatible with Apple's Bash 3.2.
set -euo pipefail

usage() {
 cat <<'USAGE'
Usage: bash scripts/build-macos.sh --qt-prefix DIR --icu-prefix DIR [options]
  --build-dir DIR          Build tree (default: build-macos)
  --stage-dir DIR          Empty install directory (default: out/macos/Chosuta)
  --jobs N                Parallel jobs (default: 4)
  --arch arm64|x86_64      Optional single target architecture; SDKs must match
  --libyaml-source-dir DIR Complete LibYAML 0.2.5 source tree
  --zlib-source-dir DIR    Complete zlib 1.3.2 source tree, including MiniZip
  --download-dependencies  Explicitly fetch missing pinned parser sources
  -h, --help              Show this help
Existing system LibYAML/MiniZip/zlib are used unless sources or downloads are selected.
Builds Release .app + CLI, with tests disabled. Does not install tools, start the
application, bundle SDK runtime libraries, download FFmpeg, sign or notarize.
USAGE
}
fail() { printf '%s\n' "$*" >&2; exit 1; }
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
qt_prefix='' icu_prefix='' build_dir='build-macos' stage_dir='out/macos/Chosuta'
yaml_source='' zlib_source='' arch='' jobs=4 download=OFF
while [[ $# -gt 0 ]]; do
 case "$1" in
  -h|--help) usage; exit 0 ;;
  --download-dependencies) download=ON; shift ;;
  --qt-prefix|--icu-prefix|--build-dir|--stage-dir|--jobs|--arch|--libyaml-source-dir|--zlib-source-dir)
   [[ $# -ge 2 && -n $2 ]] || fail "Missing value for $1"
   case "$1" in
    --qt-prefix) qt_prefix=$2 ;; --icu-prefix) icu_prefix=$2 ;;
    --build-dir) build_dir=$2 ;; --stage-dir) stage_dir=$2 ;;
    --jobs) jobs=$2 ;; --arch) arch=$2 ;;
    --libyaml-source-dir) yaml_source=$2 ;; --zlib-source-dir) zlib_source=$2 ;;
   esac
   shift 2 ;;
  *) fail "Unknown option: $1 (see --help)" ;;
 esac
done
[[ $(uname -s) == Darwin ]] || fail 'Run this script on macOS.'
[[ -n $qt_prefix && -n $icu_prefix ]] || fail '--qt-prefix and --icu-prefix are required.'
[[ $jobs =~ ^[1-9][0-9]*$ ]] || fail '--jobs must be a positive integer.'
[[ -z $arch || $arch == arm64 || $arch == x86_64 ]] || fail '--arch must be arm64 or x86_64.'
for tool in cmake ninja xcrun; do command -v "$tool" >/dev/null || fail "Missing $tool; no tools were installed."; done
cmake_tool=$(command -v cmake)
ninja_tool=$(command -v ninja)
project_path() {
 case "$1" in /*) printf '%s\n' "$1" ;; *) printf '%s/%s\n' "$project_dir" "$1" ;; esac
}
qt_prefix=$(cd -- "$qt_prefix" && pwd)
icu_prefix=$(cd -- "$icu_prefix" && pwd)
[[ -f $qt_prefix/lib/cmake/Qt6/Qt6Config.cmake ]] || fail 'Qt prefix must contain lib/cmake/Qt6/Qt6Config.cmake.'
[[ -f $icu_prefix/include/unicode/uversion.h ]] || fail 'ICU prefix must contain include/unicode/uversion.h.'
build_dir=$(project_path "$build_dir")
stage_dir=$(project_path "$stage_dir")
if [[ -e $stage_dir || -L $stage_dir ]]; then
 [[ -d $stage_dir && ! -L $stage_dir ]] || fail "Stage directory is not an ordinary directory: $stage_dir"
 [[ -z $(ls -A "$stage_dir") ]] || fail "Stage directory must be empty: $stage_dir"
fi
[[ -z $yaml_source ]] || yaml_source=$(cd -- "$yaml_source" && pwd)
[[ -z $zlib_source ]] || zlib_source=$(cd -- "$zlib_source" && pwd)
cxx_compiler=$(xcrun --find clang++)
c_compiler=$(xcrun --find clang)
sdk_path=$(xcrun --sdk macosx --show-sdk-path)
args=(-S "$project_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release
 -DBUILD_TESTING=OFF "-DCMAKE_CXX_COMPILER=$cxx_compiler" "-DCMAKE_C_COMPILER=$c_compiler"
 "-DCMAKE_MAKE_PROGRAM=$ninja_tool" "-DCMAKE_OSX_SYSROOT=$sdk_path"
 "-DCMAKE_INSTALL_RPATH=$qt_prefix/lib;$icu_prefix/lib" -DCMAKE_INSTALL_RPATH_USE_LINK_PATH=ON
 "-DQt6_DIR=$qt_prefix/lib/cmake/Qt6" "-DCMAKE_PREFIX_PATH=$qt_prefix;$icu_prefix"
 "-DICU_ROOT=$icu_prefix" "-DCHOSUTA_DOWNLOAD_DEPENDENCIES=$download"
 "-DCHOSUTA_LIBYAML_SOURCE_DIR=$yaml_source" "-DCHOSUTA_ZLIB_SOURCE_DIR=$zlib_source")
[[ -z $arch ]] || args+=("-DCMAKE_OSX_ARCHITECTURES=$arch")
"$cmake_tool" "${args[@]}"
"$cmake_tool" --build "$build_dir" --parallel "$jobs"
"$cmake_tool" --install "$build_dir" --prefix "$stage_dir"
[[ -x $stage_dir/chosuta.app/Contents/MacOS/chosuta && -x $stage_dir/bin/chosuta-cli ]] || fail 'Expected .app or CLI missing after install.'
cp "$build_dir/build-info.json" "$stage_dir/build-info.json"
cat > "$stage_dir/RUN.txt" <<'RUN'
Local macOS build (not a standalone distribution).
Open chosuta.app; the CLI is bin/chosuta-cli. Keep the selected Qt/ICU and any
system LibYAML/MiniZip/zlib runtime libraries installed at their original paths.
FFmpeg/ffprobe are optional, supplied separately in the application's Export
Settings. No tools were installed, no application/tests were run, and no signing,
notarization or portable runtime deployment was performed by this script.
RUN
printf 'Built: %s\nCLI: %s\n' "$stage_dir/chosuta.app" "$stage_dir/bin/chosuta-cli"
