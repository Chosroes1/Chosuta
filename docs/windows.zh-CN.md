# Windows 编译、部署与验证

当前源码版本 **0.2.2**。这里提供 Windows 原生编译和编译后运行的完整入口。当前开发环境为 Linux，没有 Windows 编译器和 Windows 版 Qt/ICU，因此尚无 Windows EXE 或原生测试通过的证据；以下脚本将在 Windows 上执行构建、测试、DLL/插件部署和部署后启动验证。

## 所需已有环境

- Windows 10/11 **x64**，PowerShell 5.1 或更新版本。
- C++20 编译器。默认使用 MSVC（例如 Visual Studio 2022 的 C++ 工具），在 **x64 Native Tools / Developer PowerShell** 中执行；也可用与 Qt 完全匹配的 x86_64 MinGW，通过 `-Toolchain MinGW` 选择。
- CMake ≥3.24、Ninja，均在当前终端 PATH 中。
- 同一架构/编译器的 **Qt ≥6.4 动态 SDK**，包含 Core、Gui、Widgets、Concurrent、Multimedia、Test 及 `bin/windeployqt.exe`。`QtPrefix` 指向具体编译器套件目录，其下有 `bin`、`lib/cmake/Qt6` 和 `plugins`，不是 Qt 安装器根目录。
- 同一架构/编译器的 **ICU ≥67 动态 SDK**，包含 `include/unicode`、导入库及 uc/i18n/data DLL。`IcuPrefix` 指向 SDK 根目录；脚本优先寻找 `bin64`，其次 `bin`，可用 `-IcuRuntimeDir` 指定。MSVC 常见 DLL 为 `icuucXX.dll`、`icuinXX.dll`、`icudtXX.dll`，MinGW 名称可带 `lib` 前缀。不要混用 MSVC、MinGW、32 位、ARM64 或不同版本库。
- MSVC 构建机器已有官方 **Visual C++ x64 Redistributable**。脚本不会安装它，也不会从 Visual Studio 目录随意复制编译器 DLL；部署后移除 SDK PATH 的启动检查会发现缺失运行库。MinGW 使用 windeployqt 部署其对应运行库。
- FFmpeg/ffprobe 可选。没有音频和编码工具也可运行、导入、预览、编辑与保存；完整视频回归需 libx264、libvpx-vp9、qtrle、AAC/libopus。可在导出设置中指定现有 FFmpeg，或用脚本参数 `-FfmpegDir` 指定现有工具的 `bin` 目录以一起复制。

脚本只使用本机已有组件，不安装、下载依赖或修改系统配置。Qt 部署使用 [官方 windeployqt 流程](https://doc.qt.io/qt-6.8/windows-deployment.html)；额外复制应用直接依赖的 ICU，因为 windeployqt 不负责收集所有非 Qt 第三方库。ICU 的定位使用 [CMake FindICU](https://cmake.org/cmake/help/latest/module/FindICU.html)。

## 一条命令完成构建和部署

把源码包解压到自己拥有的目录，在该目录启动已配置编译器的 PowerShell，将两个占位参数替换为实际 SDK 路径：

```powershell
.\scripts\build-windows.ps1 -QtPrefix '<Qt x64 SDK 前缀>' -IcuPrefix '<ICU x64 SDK 前缀>'
```

MinGW：

```powershell
.\scripts\build-windows.ps1 -Toolchain MinGW -QtPrefix '<Qt MinGW x64 SDK 前缀>' -IcuPrefix '<匹配的 ICU SDK 前缀>'
```

自定义运行库/FFmpeg/输出目录：

```powershell
.\scripts\build-windows.ps1 `
  -QtPrefix '<Qt x64 SDK 前缀>' `
  -IcuPrefix '<ICU x64 SDK 前缀>' `
  -IcuRuntimeDir '<ICU DLL 目录>' `
  -FfmpegDir '<已安装的 FFmpeg bin 目录>' `
  -BuildDir 'build-windows' `
  -StageDir 'out/windows/Chosuta'
```

相对构建/输出路径按**源码根目录**解析，不依赖当前工作目录；支持空格和 Unicode。默认使用 Ninja/Release，启用并运行所有 CTest；失败即停止，不把尚未验证的目录标为就绪。

## 编译后的入口

默认路径均相对源码根目录：

| 产物 | 路径 |
| --- | --- |
| 可直接启动的 GUI | `out/windows/Chosuta/bin/chosuta.exe` |
| 无界面 CLI | `out/windows/Chosuta/bin/chosuta-cli.exe` |
| 原始构建 EXE（尚未单独部署） | `build-windows/chosuta.exe`、`build-windows/chosuta-cli.exe` |
| 实际版本/依赖文件哈希/验证状态 | `out/windows/Chosuta/runtime-manifest.json` |
| 构建及部署后验证日志 | `out/windows/Chosuta/validation/` |

脚本成功显示 `Ready: .../bin/chosuta.exe` 后即可双击或执行：

```powershell
.\out\windows\Chosuta\bin\chosuta.exe
.\out\windows\Chosuta\bin\chosuta-cli.exe --version
```

必须保留整个 `Chosuta` 目录，不能只拿走 EXE。目录内有 Qt DLL、平台/图像/媒体插件、ICU DLL、`qt.conf`、应用许可与操作说明。GUI 使用 Windows 子系统，启动不附带控制台；CLI 保留控制台输出。

若附带 FFmpeg/ffprobe，应用优先寻找 EXE 同目录的程序；显式路径仍优先，否则使用系统 PATH。不需写死 SDK 或开发机路径。缺少 FFmpeg 只影响导出，ffprobe 缺失时音频时长需手动设置。

## 脚本实际检查什么

1. 检查 SDK、编译器与目标 Windows x64，配置 C++20、MSVC UTF-8、Qt 应用入口和 ICU i18n/uc/data。
2. 构建 GUI/CLI/测试，执行 `ctest --output-on-failure`。公开源码包不含用户原始 SVP，私有样本回归会明确跳过；缺少 FFmpeg/ffprobe 的可选编码测试也会跳过，实际详情保存在日志中。
3. 以 CMake 安装应用和许可，使用所选 SDK 的 windeployqt 部署 GUI/CLI；单独复制 ICU 和离屏插件，检查 qwindows，写入相对插件路径的 `qt.conf`。
4. 如果选择复制 FFmpeg，检查其版本/构建配置并拒绝 `--enable-nonfree`，复制工具目录的对应 DLL，记录构建信息。不同来源的同名 DLL 哈希不同会拒绝覆盖。
5. 清除当前进程的 Qt 插件/SDK 环境覆盖，并把 PATH 限于部署目录与 Windows 系统目录；分别运行 CLI `--version`、GUI 离屏完整冒烟、**Windows qwindows 实际窗口启动**完整冒烟。每项有 45 秒超时及独立 stdout/stderr 日志；这几项均通过后才生成成功清单并显示 Ready。
6. 记录构建器、Qt/ICU 版本、运行目录所有文件的 SHA-256，并收集 SDK 内已提供的许可证文件。环境变量在脚本结束或失败时恢复，不写注册表或永久 PATH。

原始 EXE 位于 `build-windows`，只是编译产物；要直接运行，请使用部署目录 `out/windows/Chosuta/bin`。脚本会拒绝非空 StageDir，避免混入旧 DLL，不会清空既有目录。需要重跑时选择新的 `-StageDir`；切换编译器/架构/SDK 时也使用新的 `-BuildDir`。

若失败，先看终端最后一步、构建目录的 CMake/CTest 日志或部署目录 `validation/*.stderr.log`；未完成阶段不会出现成功的 runtime-manifest。常见原因是未使用 x64 开发终端、Qt/ICU ABI 不匹配、ICU DLL 缺失、VC++ 运行库缺失或编码器缺失。

## 验收和分发边界

当前已在 Linux 验证共同源码构建、核心/GUI/编码回归及同目录外部工具查找。**本机没有执行 PowerShell、MSVC/MinGW、Windows DLL 部署或 qwindows 启动**；不能把脚本存在或 Linux 测试通过当成 Windows 验证通过。Windows 上执行脚本后，`validation/ctest.log` 与 `runtime-manifest.json` 才是该次原生构建和启动的证据。

qwindows 冒烟证明可启动并跑完整原创流程，仍需人工检查真实输入法、高 DPI、拖动、音频设备播放和视频封装。用户歌曲/立绘只用于自己的本地验收，公开源码包只有原创最小夹具。

应用源码为 **GPL-3.0-or-later**。本地部署目录会包含用户所选 SDK 的实际 DLL/插件；收集到的 SDK 许可不等于已完成其所有分发义务。对外分发前需按该目录的具体 Qt/ICU/媒体库/编译器运行库补齐对应源码、NOTICE 和许可条款；本轮只生成本地构建入口及不含第三方二进制的 Windows 源码包。实际组件记录见 `THIRD_PARTY_NOTICES.md`。
