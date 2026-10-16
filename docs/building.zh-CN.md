# 构建、验证与本地打包

当前根源码为 1.0.0/schema14。C++20、CMake ≥3.24、Qt ≥6.4、ICU ≥67、LibYAML ≥0.2.5、传统MiniZip/zlib；使用已经安装且状态一致的开发环境，不自动下载依赖。Linux 构建结果不能代替 Windows 原生验证。另有实验性macOS本地构建入口，不作为正式平台保证。

0.19.1的UST别名预设由Qt资源内嵌，并安装到share/chosuta/presets/ust-aliases.json；数据5,602字节，无新依赖、下载或构建开关。中文显式ü/v/u:复用已有口形规则；Linux构建与CTest 4/4通过（核心65项/UI38项）；相对0.19.0同配置去符号Debug的GUI增8,192字节、CLI文件大小不变，另随安装提供上述JSON。不是Windows/Release包体承诺。

1.0.0在既有0.19.1范围完成全流程复查后迭代版本；Linux Debug构建成功、CTest 4/4（核心65/UI38无跳过）通过，最终45组旧工程重生成保持。版本变更未增加依赖或改变schema14；Windows和原软件/声学边界仍未验证。未重新打包既有交付。

## Linux

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF
cmake --build build --parallel 4
./build/chosuta
```

核心依赖 Qt Core/Gui、ICU i18n/uc/data、LibYAML及MiniZip/zlib；GUI 另依赖 Widgets/Concurrent/Multimedia，Qt Network 为传递依赖。普通构建不需要 Qt Test。标准 Qt 控件的中日译文由应用源码编译，不需部署 qm 文件；OS 原生对话框语言仍由系统影响。Python 仅用于开发打包及 Linux 打包预检测试，不是产品运行依赖。

媒体执行使用一次选定的 FFmpeg/ffprobe 上下文。工程显式路径直接使用；CLI 可用 `--ffmpeg <路径>` 明确选定本次程序。自动标记沿用本机默认、tools/ffmpeg、应用目录、PATH 顺序；ffprobe 优先在所选 FFmpeg 旁边。缺工具仍可编辑保存。MP4 使用 libx264/AAC，WebM 使用 libvpx-vp9/libopus，透明 MOV 使用 qtrle；波形解码复用 FFmpeg，无模型。Qt 试听后端在播放音频时才初始化。

## 开发测试

CMake 的 `BUILD_TESTING` 默认关闭。开发时按改动范围显式开启并执行相关测试，需要已有 Qt Test；普通构建和 Linux/Windows 打包不自动运行测试，不要求 PowerShell 测试夹具。

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

测试结果与编译、打包结果分别记录；GUI 离屏检查不替代真实桌面交互。出现失败先核对行为和测试条件，不为通过测试擅改正常界面。

## Windows

依赖、MSVC/MinGW、Qt/ICU 部署和双版本媒体工具布局见 [Windows 编译、部署与验证](windows.zh-CN.md)。已有匹配的 x64 环境时：

```powershell
.\scripts\build-windows.ps1 -QtPrefix '<Qt x64 SDK 前缀>' -IcuPrefix '<ICU x64 SDK 前缀>' -DownloadDependencies
.\out\windows\Chosuta\chosuta.exe
```

脚本配置 `BUILD_TESTING=OFF`，编译并部署 Qt/媒体/平台插件、ICU；不运行 CTest 或启动应用验收。`-BothVariants -CreateZip` 可从同次编译部署附带工具及 no-ffmpeg 两版；外部媒体 DLL 与 Qt 媒体依赖隔离。显式传 `-DownloadFfmpeg` 可校验下载并缓存 BtbN win64 GPL shared，或用 `-FfmpegDir` 选择已有工具；默认不下载，不安装 VC++ 运行库；构建清单明确记录测试未运行。CMake/Ninja/编译器在修改PATH前解析并固定，FFmpeg可选下载在应用编译成功后开始。

## macOS（实验性）

使用已安装的Xcode命令行工具、CMake/Ninja和匹配架构的Qt/ICU：

```sh
bash scripts/build-macos.sh --qt-prefix '<Qt macOS SDK 前缀>' --icu-prefix '<ICU 前缀>'
```

默认离线、Release及BUILD_TESTING=OFF，生成`out/macos/Chosuta/chosuta.app`与`bin/chosuta-cli`。可指定LibYAML/zlib离线源码，或显式`--download-dependencies`使用相同固定归档。依赖本机SDK，不复制运行库、安装工具、下载FFmpeg、签名、公证或自动运行应用。仅完成Linux上的脚本语法和命令流程夹具检查，macOS实机未验证；详见[macOS说明](macos.zh-CN.md)。

## 本机开发包

```sh
bash scripts/package-linux.sh build-release out/linux-new
```

输出目录必须尚不存在；在任何构建或打包写入之前检查。所有打包步骤先写本次独有暂存目录，成功后以不覆盖方式移至目标；过程中出现的目标也保留原样。包含 Linux ZIP、对应白名单源码 ZIP、runtime-dependencies.txt。失败/取消只清理本次暂存目录，旧包不覆盖。

发行版安装包见 [Linux 发行版打包](packaging.zh-CN.md)。应用包动态链接本机 Qt/ICU/系统库，不是跨发行版自包含包；应在目标发行版重编。也可用 `cmake --install build --prefix '<自己拥有的目录>'` 安装到本机目录，不需管理员权限。缺依赖按项目授权处理；Arch 不以局部升级或归档旧包拼接环境。

应用图标已内置于GUI资源，Linux安装到`share/icons/hicolor/512x512/apps/chosuta.png`并关联chosuta桌面项；Windows GUI/CLI安装在部署根目录，图标ICO编译进GUI EXE。PNG/ICO均已提供，构建不新增图像处理工具依赖。

0.13.0新增LibYAML用于USTX；Linux复用系统yaml.h/libyaml，Arch配方已声明libyaml依赖，缺库时报错，默认不联网。Windows脚本以已有C/C++编译器静态构建源码：可用 `-LibYamlSourceDir '<LibYAML 0.2.5完整源码目录>'` 离线构建，或明确传 `-DownloadDependencies` 下载固定官方0.2.5归档并校验SHA-256。本地源目录优先，下载开关可获取其他缺省依赖；源码获取只写构建目录，不安装新SDK或系统包。

直接使用CMake可设 `-DCHOSUTA_LIBYAML_SOURCE_DIR='<目录>'` 及ZIP的 `-DCHOSUTA_ZLIB_SOURCE_DIR='<完整zlib目录>'`，或 `-DCHOSUTA_DOWNLOAD_DEPENDENCIES=ON`。构建源码时额外启用C语言，使用与C++相同的现有工具链；不会编译上游测试/工具。静态安装自动附完整实际源码及MIT许可：Windows位于 `dependency-sources/libyaml-0.2.5/`，Linux位于 `share/chosuta/dependency-sources/libyaml-0.2.5/`。源码应放在独立源码目录，构建产物放到外部BuildDir。

当前Linux采用已安装LibYAML 0.2.5及传统MiniZip/zlib 1.3.2，无需新增安装。VPR/MXL使用MiniZip读取及DEFLATE；Windows静态路线用完整zlib 1.3.2（含contrib/minizip），只编读取/解压和校验部分，NOUNCRYPT，不新增ZIP DLL。完整实际源码随Windows安装到dependency-sources/zlib-1.3.2，其他平台到share/chosuta/dependency-sources/zlib-1.3.2。固定归档和SHA-256见cmake/Zip.cmake及第三方清单；build-info含zipProvider与libyamlProvider。

本次Linux构建、默认离线且关闭测试的独立配置和打包夹具通过。两份依赖的实际下载各尝试一次均因DNS失败，0字节，未反复重试或安装其他工具。没有PowerShell/Windows SDK，Windows1.0.0及真实依赖源码静态构建/部署尚未验证；此前Windows成功反馈对应0.10.0。macOS仅作实验入口。普通构建仍不联网，FFmpeg显式下载与解析依赖开关独立。
