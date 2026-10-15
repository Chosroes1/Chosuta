# Linux 发行版打包

当前源码为 Chosuta 0.10.0，发行版打包入口自 0.2.2 起提供；以下是未来按当前源码生成的命令/文件名，本轮本地交付位于 `out/release/0.10.0/`，包含Linux ZIP、Arch安装包与对应源码；Windows成品未生成。`apt` 是包管理工具，其本地安装文件是 `.deb`。当前宿主是 Arch Linux：可以本地构建 Arch 包，不能把宿主的 Qt/ICU 二进制换个扩展名当作 Debian/Fedora 可运行包。

打包脚本只使用已有工具，不安装依赖、不启动容器拉取镜像、不上传源码或发布仓库。依赖缺失时明确失败。安装工具与系统环境准备由维护者自行按目标发行版的规范完成。

## Arch / AUR

在已安装本项目开发依赖、makepkg、fakeroot 的 Arch 环境中，从源码根目录执行：

```sh
python3 scripts/package-arch.py out/arch/0.10.0
```

上述命令输出到新的 `out/arch/0.10.0/`（脚本无参数的历史默认目录仍为 `out/arch/0.2.2/`，建议始终显式指定新目录）：

- `chosuta-0.10.0-2-x86_64.pkg.tar.zst`：本机 Arch 安装包。
- `Chosuta-0.10.0-source.zip`：对应源码、原创夹具、打包模板和许可，不包含用户材料。
- `PKGBUILD`：从 `packaging/arch/PKGBUILD.in` 生成的实际配方，带源码 SHA-256。
- `.SRCINFO`：由 `makepkg --printsrcinfo` 生成的包元数据。

脚本使用 `makepkg --noconfirm`，**没有** `--syncdeps` 或 `--install`，缺少依赖时不会自动安装。输出目录存在即拒绝，可以给脚本传另一个新目录。配方当前声明并验证 x86_64；其他架构没有验证。运行依赖 glibc、gcc-libs、Qt 6 Base/Multimedia、ICU；构建依赖 CMake/Ninja；配方关闭测试，不要求 FFmpeg 作为检查依赖。FFmpeg 对普通用户仍是可选运行依赖，没有它也可编辑。

本轮不向系统安装。用户决定安装时可自行执行：

```sh
sudo pacman -U out/arch/0.10.0/chosuta-0.10.0-2-x86_64.pkg.tar.zst
```

安装后运行 `chosuta` 或桌面菜单中的 Chosuta；CLI 为 `chosuta-cli`。卸载由包管理器管理，工程和个人设置不属于包文件。图标与桌面项在标准 `/usr/share` 目录中。

**AUR 存放构建配方，用户机器通过 makepkg 编译源码得到安装包。** 当前生成的是使用同目录源码 ZIP 的本地配方，不是已经可以提交的 AUR 条目。还需：

1. 确定并发布真实项目主页及可公开下载的对应版本源码 ZIP，检查其 SHA-256；不要将用户 `svproject/` 或立绘纳入发布。
2. 在 PKGBUILD 增加真实 `url` 和 `Maintainer` 信息，将 `source` 改为 `"Chosuta-${pkgver}-source.zip::真实的源码下载地址"`，保留核验后的 SHA-256。
3. 重新执行 `makepkg --printsrcinfo > .SRCINFO`，在 Arch 干净构建环境验证，再由获授权的维护者提交 PKGBUILD 与 `.SRCINFO` 到 AUR。源码由上游托管，不把构建目录和二进制包作为 AUR 配方上传。

没有预设或虚构 GitHub/AUR 地址，也没有发布。若以后提供预编译下载，可以另做 `chosuta-bin`，但这不代替当前源码构建配方及对应源码义务。

## Debian / Ubuntu：DEB

使用匹配目标发行版的既有开发环境进行编译。需要 Qt ≥6.4、ICU ≥67、C++20、CMake/Ninja、Python 3（仅源码归档）和提供 `dpkg-shlibdeps` 的 Debian 打包工具。不能使用当前 Arch 可执行文件。目标发行版具体开发包名称和版本以其仓库为准。

```sh
bash scripts/package-native.sh DEB '真实维护者姓名 <真实邮箱>'
```

默认构建目录 `build-package-deb/`，输出 `out/deb/chosuta_0.10.0-1_目标架构.deb` 与对应源码 ZIP。CPack 使用 `dpkg --print-architecture` 获取架构，避免非 Debian 宿主默认误标为 i386；`dpkg-shlibdeps` 扫描实际链接库生成版本依赖。额外声明运行时加载的 `qt6-qpa-plugins`，推荐可选 FFmpeg。安装前使用目标机 `dpkg-deb --info`、`--contents` 检查元数据，安装后验 CLI、GUI、音频和短视频。

```sh
# 在目标 Debian/Ubuntu 上，由用户决定是否安装
sudo apt install ./out/deb/chosuta_0.10.0-1_amd64.deb
```

当前 Arch 本机没有 dpkg/dpkg-shlibdeps/dpkg-deb，未生成或安装 DEB。CPack 可以跨 Linux 发行版生成 DEB 容器，但可安装、可运行还取决于目标 ABI 和依赖；本入口不关闭依赖扫描来凑出文件。发布 apt 仓库还需单独维护索引、签名和托管，不属于生成本地 DEB。

## Fedora / 其他 RPM 发行版

同样在匹配目标发行版的既有环境编译，需要 rpmbuild、C++20、Qt/ICU 开发包、CMake/Ninja 及 Python 3。ELF 库依赖由 RPM 工具扫描；动态加载的 QPA 插件不在扫描范围，因此必须明确传入目标发行版的插件包依赖，不能假定所有 RPM 发行版使用同一包名。

```sh
CHOSUTA_RPM_PLUGIN_REQUIRES='目标发行版的 Qt 6 QPA 插件包名' \
  bash scripts/package-native.sh RPM
```

默认构建目录 `build-package-rpm/`，输出 `out/rpm/chosuta-0.10.0-1.目标架构.rpm` 与对应源码 ZIP。实际文件名由 rpmbuild 决定。FFmpeg 不列作统一强制依赖：发行版提供的编码器集合不同，用户可配置现有外部程序，MP4/libx264 等以实际编码器检查为准。本机没有 rpmbuild，未生成或安装 RPM；目标机需 `rpm -qip/-qlp` 检查及真实运行验证。

两个原生脚本入口可用 `CHOSUTA_PACKAGE_BUILD_DIR`、`CHOSUTA_PACKAGE_OUTPUT_DIR` 指定自有目录。输出目录存在且非空时拒绝，避免覆盖既有交付。直接调用 `cpack -G DEB|RPM` 也会要求相应工具和元数据，但调用者仍负责在目标环境编译。

## 许可和已验证边界

Chosuta 代码、配方和桌面项继续 GPL-3.0-or-later；当前PNG标识由项目维护者提供并授权集成，具体来源记录见THIRD_PARTY_NOTICES；包附 LICENSE、THIRD_PARTY_NOTICES、原有依赖许可文本和对应源码归档。没有捆绑 Qt/ICU/FFmpeg，系统库由发行版包提供。FFmpeg 不得使用 enable-nonfree 构建作为分发依赖。

本机 Arch 包可以验证构建、内容和解包运行；未安装到系统不等于已验证系统安装/卸载。DEB/RPM、AUR 公开提交、Windows 原生编译均不能因本机 Arch 测试而记为完成。公开整理和分发方式见 [源码发布说明](publication.zh-CN.md)。

参考：[CPack DEB](https://cmake.org/cmake/help/latest/cpack_gen/deb.html)、[CPack RPM](https://cmake.org/cmake/help/latest/cpack_gen/rpm.html)、[PKGBUILD 手册](https://man.archlinux.org/man/PKGBUILD.5.en)、[makepkg 手册](https://man.archlinux.org/man/makepkg.8.en)。

## Windows 附带工具与 no-ffmpeg

0.10.0 的 `scripts/build-windows.ps1 -FfmpegDir <已有工具bin> -BothVariants -CreateZip` 支持编译一次、分别部署并压缩两种目录（Windows 脚本关闭自动测试和应用启动验收）；也可将 `-FfmpegDir <已有工具bin>` 换成 `-DownloadFfmpeg`，显式校验下载并缓存 BtbN win64 GPL shared（其他 Qt／ICU 参数见 [Windows 说明](windows.zh-CN.md)）。GUI/CLI可执行文件位于部署根目录，外部 ffmpeg／ffprobe 及其共享 DLL 独立位于 `tools/ffmpeg/`；无工具版仍保留 Qt 媒体后端。脚本记录各自大小及测试未运行状态，不覆盖现有产物。当前环境未执行 Windows 部署，没有 Windows 成品；指定工具的许可和对应源码义务按实际构建核对。

## 当前 Arch 环境下准备 DEB／RPM（0.10.0）

本机已有 CMake／Ninja／makepkg／fakeroot，可直接构建 Arch 安装包和本机动态链接 ZIP；没有 dpkg 系列、rpmbuild／rpm，也没有 Podman／Docker／Distrobox。此任务不安装工具或拉取镜像。只在 Arch 装上 dpkg／rpm 不能解决 Qt／ICU／glibc 的目标 ABI 和依赖包名差异；项目原生脚本也会拒绝 Arch 宿主。

可在已有的目标系统、虚拟机或获授权后准备的容器里使用公开源码包；从 Ubuntu／Debian 或 Fedora 的软件源安装编译依赖，然后执行原生脚本。以下为用户在目标系统中自行执行的准备命令，当前机器未执行：

```sh
# Debian / Ubuntu，例如 Ubuntu 24.04；须满足 Qt >=6.4、ICU >=67、CMake >=3.24
sudo apt update
sudo apt install build-essential cmake ninja-build python3 pkg-config \
  qt6-base-dev qt6-multimedia-dev qt6-qpa-plugins libicu-dev dpkg-dev
# 可选 ffmpeg 用于媒体功能与编码回归；缺失时媒体测试明确跳过
# 解压对应源码 ZIP，进入 Chosuta-0.10.0 后：
CHOSUTA_PACKAGE_OUTPUT_DIR="$PWD/out/deb-0.10.0" \
  bash scripts/package-native.sh DEB '真实维护者姓名 <真实邮箱>'
```

旧 Debian／Ubuntu 的仓库版本可能达不到要求；先核对 SDK 版本，选择满足要求的目标环境，不将 Arch SDK 混入目标系统。DEB 文件预计为 `chosuta_0.10.0-1_amd64.deb`，还会输出对应源码 ZIP；真实维护者联系信息必须由用户填写。

```sh
# Fedora 目标环境
sudo dnf install gcc-c++ cmake ninja-build python3 pkgconf-pkg-config \
  qt6-qtbase-devel qt6-qtmultimedia-devel qt6-qtbase-gui libicu-devel rpm-build
# 解压对应源码 ZIP，进入 Chosuta-0.10.0 后：
CHOSUTA_RPM_PLUGIN_REQUIRES='qt6-qtbase-gui' \
CHOSUTA_PACKAGE_OUTPUT_DIR="$PWD/out/rpm-0.10.0" \
  bash scripts/package-native.sh RPM
```

Fedora 的 Qt GUI/QPA 包名已核对为 `qt6-qtbase-gui`；其他 RPM 发行版须重新确认，不能套用 Fedora 包名。媒体测试需要该目标环境可用的 FFmpeg 编码器，缺失时不能声称已验收编码能力。软件运行库由实际 ELF 依赖扫描生成，Qt QPA 动态插件另行声明。

打包后先在目标机检查 `dpkg-deb --info/--contents` 或 `rpm -qip/-qlp`，再在干净目标系统安装并验证 GUI／CLI、Qt 插件、音频和短片；生成 `.deb` 不等于已经建立 apt 软件源，后者还有独立的索引／签名／托管工作。

来源：[CPack DEB](https://cmake.org/cmake/help/latest/cpack_gen/deb.html)、[CPack RPM](https://cmake.org/cmake/help/latest/cpack_gen/rpm.html)、[Ubuntu Qt QPA](https://packages.ubuntu.com/noble/qt6-qpa-plugins)、[Fedora Qt GUI](https://packages.fedoraproject.org/pkgs/qt6-qtbase/qt6-qtbase-gui/)。
