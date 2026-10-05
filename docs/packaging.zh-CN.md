# Linux 发行版打包

Chosuta 0.2.2 增加发行版打包入口。`apt` 是包管理工具，其本地安装文件是 `.deb`。当前宿主是 Arch Linux：可以本地构建 Arch 包，不能把宿主的 Qt/ICU 二进制换个扩展名当作 Debian/Fedora 可运行包。

打包脚本只使用已有工具，不安装依赖、不启动容器拉取镜像、不上传源码或发布仓库。依赖缺失时明确失败。安装工具与系统环境准备由维护者自行按目标发行版的规范完成。

## Arch / AUR

在已安装本项目开发依赖、makepkg、fakeroot 的 Arch 环境中，从源码根目录执行：

```sh
python3 scripts/package-arch.py
```

默认输出到 `out/arch/0.2.2/`：

- `chosuta-0.2.2-2-x86_64.pkg.tar.zst`：本机 Arch 安装包。
- `Chosuta-0.2.2-source.zip`：对应源码、原创夹具、打包模板和许可，不包含用户材料。
- `PKGBUILD`：从 `packaging/arch/PKGBUILD.in` 生成的实际配方，带源码 SHA-256。
- `.SRCINFO`：由 `makepkg --printsrcinfo` 生成的包元数据。
- `src/build/Testing/Temporary/LastTest.log`：源码包独立构建的测试记录。

脚本使用 `makepkg --noconfirm`，**没有** `--syncdeps` 或 `--install`，缺少依赖时不会自动安装。输出目录存在即拒绝，可以给脚本传另一个新目录。配方当前声明并验证 x86_64；其他架构没有验证。运行依赖 glibc、gcc-libs、Qt 6 Base/Multimedia、ICU；构建依赖 CMake/Ninja，检查依赖 FFmpeg。FFmpeg 对普通用户仍是可选运行依赖，没有它也可编辑。

本轮不向系统安装。用户决定安装时可自行执行：

```sh
sudo pacman -U out/arch/0.2.2/chosuta-0.2.2-2-x86_64.pkg.tar.zst
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

默认构建目录 `build-package-deb/`，输出 `out/deb/chosuta_0.2.2-2_目标架构.deb` 与对应源码 ZIP。CPack 使用 `dpkg --print-architecture` 获取架构，避免非 Debian 宿主默认误标为 i386；`dpkg-shlibdeps` 扫描实际链接库生成版本依赖。额外声明运行时加载的 `qt6-qpa-plugins`，推荐可选 FFmpeg。安装前使用目标机 `dpkg-deb --info`、`--contents` 检查元数据，安装后验 CLI、GUI、音频和短视频。

```sh
# 在目标 Debian/Ubuntu 上，由用户决定是否安装
sudo apt install ./out/deb/chosuta_0.2.2-2_amd64.deb
```

本机没有 dpkg/dpkg-shlibdeps，未生成或安装 DEB。CPack 可以跨 Linux 发行版生成 DEB 容器，但可安装、可运行还取决于目标 ABI 和依赖；本入口不关闭依赖扫描来凑出文件。发布 apt 仓库还需单独维护索引、签名和托管，不属于生成本地 DEB。

## Fedora / 其他 RPM 发行版

同样在匹配目标发行版的既有环境编译，需要 rpmbuild、C++20、Qt/ICU 开发包、CMake/Ninja 及 Python 3。ELF 库依赖由 RPM 工具扫描；动态加载的 QPA 插件不在扫描范围，因此必须明确传入目标发行版的插件包依赖，不能假定所有 RPM 发行版使用同一包名。

```sh
CHOSUTA_RPM_PLUGIN_REQUIRES='目标发行版的 Qt 6 QPA 插件包名' \
  bash scripts/package-native.sh RPM
```

默认构建目录 `build-package-rpm/`，输出 `out/rpm/chosuta-0.2.2-2.目标架构.rpm` 与对应源码 ZIP。实际文件名由 rpmbuild 决定。FFmpeg 不列作统一强制依赖：发行版提供的编码器集合不同，用户可配置现有外部程序，MP4/libx264 等以实际编码器检查为准。本机没有 rpmbuild，未生成或安装 RPM；目标机需 `rpm -qip/-qlp` 检查及真实运行验证。

两个原生脚本入口可用 `CHOSUTA_PACKAGE_BUILD_DIR`、`CHOSUTA_PACKAGE_OUTPUT_DIR` 指定自有目录。输出目录存在且非空时拒绝，避免覆盖既有交付。直接调用 `cpack -G DEB|RPM` 也会要求相应工具和元数据，但调用者仍负责在目标环境编译。

## 许可和已验证边界

Chosuta 代码、配方、桌面项和原创 SVG 图标继续 GPL-3.0-or-later；包附 LICENSE、THIRD_PARTY_NOTICES、原有依赖许可文本和对应源码归档。没有捆绑 Qt/ICU/FFmpeg，系统库由发行版包提供。FFmpeg 不得使用 enable-nonfree 构建作为分发依赖。

本机 Arch 包可以验证构建、内容和解包运行；未安装到系统不等于已验证系统安装/卸载。DEB/RPM、AUR 公开提交、Windows 原生编译均不能因本机 Arch 测试而记为完成。公开整理和分发方式见 [源码发布说明](publication.zh-CN.md)。

参考：[CPack DEB](https://cmake.org/cmake/help/latest/cpack_gen/deb.html)、[CPack RPM](https://cmake.org/cmake/help/latest/cpack_gen/rpm.html)、[PKGBUILD 手册](https://man.archlinux.org/man/PKGBUILD.5.en)、[makepkg 手册](https://man.archlinux.org/man/makepkg.8.en)。
