#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Run inside an existing target distro; no installation, downloads or publishing.
set -euo pipefail
generator=${1:-}
case "$generator" in
 DEB) required_tools=(cmake ninja ctest cpack python3 dpkg dpkg-shlibdeps) ;;
 RPM) required_tools=(cmake ninja ctest cpack python3 rpmbuild) ;;
 *) echo 'Usage: bash scripts/package-native.sh DEB|RPM [real maintainer contact]' >&2; exit 2 ;;
esac
for tool in "${required_tools[@]}"; do
 command -v "$tool" >/dev/null || { echo "Missing $tool; no tools were installed." >&2; exit 1; }
done
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
if [[ ! -r /etc/os-release ]]; then
 echo 'Cannot identify target distro; use CPack manually in a verified target environment.' >&2; exit 1
fi
chosuta_distro_id=$(sed -n 's/^ID=//p' /etc/os-release | tr -d '"')
chosuta_distro_like=$(sed -n 's/^ID_LIKE=//p' /etc/os-release | tr -d '"')
case "$generator:$chosuta_distro_id $chosuta_distro_like" in
 DEB:*debian*|DEB:*ubuntu*) ;;
 RPM:*fedora*|RPM:*rhel*|RPM:*centos*|RPM:*suse*) ;;
 *) echo "Refusing $generator binaries built against $chosuta_distro_id; build on the target distro." >&2; exit 1 ;;
esac
contact=${2:-}
if [[ $generator == DEB && -z $contact ]]; then
 echo 'DEB requires a real maintainer contact: Name <email>.' >&2; exit 2
fi
if [[ $generator == RPM && -z ${CHOSUTA_RPM_PLUGIN_REQUIRES:-} ]]; then
 echo 'Set CHOSUTA_RPM_PLUGIN_REQUIRES to the target Qt QPA plugin package(s).' >&2; exit 2
fi
build_dir=${CHOSUTA_PACKAGE_BUILD_DIR:-"$project_dir/build-package-${generator,,}"}
package_dir=${CHOSUTA_PACKAGE_OUTPUT_DIR:-"$project_dir/out/${generator,,}"}
if [[ -d $package_dir && -n $(ls -A -- "$package_dir") ]]; then
 echo 'Output directory is not empty; choose a new owned directory.' >&2; exit 1
fi
cmake_args=(-S "$project_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr)
[[ -z $contact ]] || cmake_args+=("-DCPACK_PACKAGE_CONTACT=$contact")
if [[ $generator == RPM ]]; then
 cmake_args+=("-DCPACK_RPM_PACKAGE_REQUIRES=$CHOSUTA_RPM_PLUGIN_REQUIRES")
fi
cmake "${cmake_args[@]}"
cmake --build "$build_dir" --parallel 4
ctest --test-dir "$build_dir" --output-on-failure
mkdir -p -- "$package_dir"
cpack --config "$build_dir/CPackConfig.cmake" -G "$generator" -B "$package_dir"
package_version=$(sed -n 's/^set(CPACK_PACKAGE_VERSION "\([^"]*\)")/\1/p' "$build_dir/CPackConfig.cmake")
python3 "$project_dir/scripts/package-source.py" "$package_dir/Chosuta-$package_version-source.zip"
