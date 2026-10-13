# Windows 编译、部署与验证

当前源码版本 **0.9.2**。这里提供 Windows 原生编译和编译后运行的完整入口。当前开发环境为 Linux，没有 Windows 编译器和 Windows 版 Qt/ICU，因此尚无 Windows EXE 或原生测试通过的证据；以下脚本将在 Windows 上执行构建、测试、DLL/插件部署和部署后启动验证。

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

## 一次编译生成两种包

使用已有的 BtbN **win64 GPL shared** 解压目录；`-FfmpegDir` 指向其中同时含 `ffmpeg.exe`、`ffprobe.exe` 和共享 DLL 的 `bin`。此脚本不下载或自行编译 FFmpeg，也不让这套 DLL 与 Qt 共用。

```powershell
.\scripts\build-windows.ps1 `
  -QtPrefix '<Qt x64 SDK 前缀>' `
  -IcuPrefix '<ICU x64 SDK 前缀>' `
  -FfmpegDir '<BtbN GPL shared 的 bin 目录>' `
  -BothVariants -CreateZip `
  -StageDir 'out/windows/Chosuta-0.9.2'
```

只编译／跑 CTest 一次，再各自部署、验证两种目录；全部成功后才压缩：

- `out/windows/Chosuta-0.9.2/` 和 `.zip`：附带外部工具。
- `out/windows/Chosuta-0.9.2-no-ffmpeg/` 和 `.zip`：不附带外部工具，保留 Qt／ICU 和 Qt 媒体后端。

`-BothVariants` 必须与 `-FfmpegDir` 一起用；不传 `-CreateZip` 只生成目录。只要 no-ffmpeg 版时，省略 `-FfmpegDir`／`-BothVariants`，直接把 `-StageDir` 命名为所需的 no-ffmpeg 名称。目录必须为空，ZIP 必须不存在，不覆盖原产物。

no-ffmpeg 用户在“文件 → 导出设置”选择本机已有的 `ffmpeg.exe`，同目录保留 `ffprobe.exe` 和该构建的共享 DLL，然后“检测媒体工具”，确定保存。检测报告显示实际工具路径、版本及缺失能力；这不能替代实际导出和剪辑软件导入验收。命令行也可检查：

```powershell
.\out\windows\Chosuta-0.9.2-no-ffmpeg\chosuta-cli.exe --check-media-tools --ffmpeg '<现有 ffmpeg.exe 完整路径>'
```

如果手动从完整版复制出 no-ffmpeg 目录，删的是副本中的整个 `tools/ffmpeg/`，而不是只删两个 EXE，更不能删除部署根目录中的 Qt DLL。手动修改会令原来的清单和验证日志失效，正式交付建议直接使用双版本脚本。

实际体积以该次 `runtime-manifest.json` 的 `runtimeBytes`、`mediaToolsBytes` 和生成 ZIP 为准。目前没有 BtbN／Windows 实测大小，不承诺压到某个 MB；BtbN 共享版本身也不是最小功能裁剪版。

## 编译后的入口

默认路径均相对源码根目录：

| 产物 | 路径 |
| --- | --- |
| 可直接启动的 GUI | `out/windows/Chosuta/chosuta.exe` |
| 无界面 CLI | `out/windows/Chosuta/chosuta-cli.exe` |
| 原始构建 EXE（尚未单独部署） | `build-windows/chosuta.exe`、`build-windows/chosuta-cli.exe` |
| 实际版本/依赖文件哈希/验证状态 | `out/windows/Chosuta/runtime-manifest.json` |
| 构建及部署后验证日志 | `out/windows/Chosuta/validation/` |

脚本成功显示 `Ready: .../chosuta.exe` 后即可双击或执行：

```powershell
.\out\windows\Chosuta\chosuta.exe
.\out\windows\Chosuta\chosuta-cli.exe --version
```

从本轮根目录构建得到的 GUI/CLI 直接位于部署目录顶层，无需进入 bin 子目录，也不添加转发启动器。旧包保持原布局，应按其随附 RUN 说明启动。

必须保留整个 `Chosuta` 目录，不能只拿走 EXE。目录内有 Qt DLL、平台/图像/媒体插件、ICU DLL、`qt.conf`、应用许可与操作说明。GUI 使用 Windows 子系统，启动不附带控制台；CLI 保留控制台输出。

0.9.2 将附带的外部命令行工具放在 `tools/ffmpeg/`，与 Qt 的媒体 DLL 分开。查找顺序为本机已采纳的工程路径 → 本机默认 → `tools/ffmpeg/` → 应用 EXE 同目录 → PATH；明确指定的错误路径不会静默换用其他 FFmpeg。ffprobe 先查所选 FFmpeg 同目录，再查附带工具、应用目录、PATH。导出设置提供本机默认路径、工程路径建议/采纳、浏览及后台检测；未采纳的显式工程路径不会执行，FFmpeg/ffprobe文件身份变化后需重新采纳。

缺少外部 FFmpeg 时视频导出、波形解码和重新进行波形时间校正不可用；工程生成、编辑、保存仍可用。ffprobe 缺失时音频时长需手动设置。试听依赖 Qt Multimedia，no-ffmpeg 版也必须保留 windeployqt 部署的 Qt 媒体 DLL／插件。

## 脚本实际检查什么

1. 检查 SDK、编译器与目标 Windows x64，配置 C++20、MSVC UTF-8、Qt 应用入口和 ICU i18n/uc/data。
2. 构建 GUI/CLI/测试，执行 `ctest --output-on-failure`。公开源码包不含用户原始 SVP，私有样本回归会明确跳过；缺少 FFmpeg/ffprobe 的可选编码测试也会跳过，实际详情保存在日志中。
3. 以 CMake 安装应用和许可，使用所选 SDK 的 windeployqt 部署 GUI/CLI；单独复制 ICU 和离屏插件，检查 qwindows，写入相对插件路径的 `qt.conf`。
4. 如果选择复制 FFmpeg，分别检查 ffmpeg/ffprobe 的版本/构建配置并拒绝 `--enable-nonfree`；两个 EXE 和指定 bin 目录的全部 DLL 原样放到 `tools/ffmpeg/`，不复制 ffplay、不自行裁剪供应商 DLL。Qt 所需 DLL 留在原位置；同一目标位置的同名文件哈希不同时拒绝覆盖。
5. 清除当前进程的 Qt 插件/SDK 环境覆盖，并把 PATH 限于部署目录与 Windows 系统目录；附带版先运行 `--check-media-tools` 检查两个工具启动及当前编码器/滤镜，再分别运行 CLI `--version`、GUI 离屏完整冒烟、**Windows qwindows 实际窗口启动**完整冒烟。每项有 45 秒超时及独立 stdout/stderr 日志；这几项均通过后才生成成功清单并显示 Ready。
6. 记录构建器、Qt/ICU 版本、包的变体、根运行目录及独立媒体目录字节数、所有运行文件的大小和 SHA-256，并收集 SDK 内已提供的许可证文件。环境变量在脚本结束或失败时恢复，不写注册表或永久 PATH。

原始 EXE 位于 `build-windows`，只是编译产物；要直接运行，请使用部署目录 `out/windows/Chosuta`。脚本会拒绝非空 StageDir，避免混入旧 DLL，不会清空既有目录。需要重跑时选择新的 `-StageDir`；切换编译器/架构/SDK 时也使用新的 `-BuildDir`。

若失败，先看终端最后一步、构建目录的 CMake/CTest 日志或部署目录 `validation/*.stderr.log`；未完成阶段不会出现成功的 runtime-manifest。常见原因是未使用 x64 开发终端、Qt/ICU ABI 不匹配、ICU DLL 缺失、VC++ 运行库缺失或编码器缺失。

## 验收和分发边界

当前已在 Linux 验证共同源码构建、核心/GUI/编码回归及工具目录／全局／工程路径优先级；清空 PATH 后的附带工具与无工具目录烟测也通过。**本机没有执行 PowerShell、MSVC/MinGW、Windows DLL 部署或 qwindows 启动**；不能把脚本存在或 Linux 测试通过当成 Windows 验证通过。Windows 上执行脚本后，`validation/ctest.log` 与 `runtime-manifest.json` 才是该次原生构建和启动的证据。

qwindows 冒烟证明可启动并跑完整原创流程，仍需人工检查真实输入法、高 DPI、拖动、音频设备播放和视频封装。用户歌曲/立绘只用于自己的本地验收，公开源码包只有原创最小夹具。

应用源码为 **GPL-3.0-or-later**。本地部署目录会包含用户所选 SDK 的实际 DLL/插件；收集到的 SDK 许可不等于已完成其所有分发义务。对外分发前需按该目录的具体 Qt/ICU/媒体库/编译器运行库补齐对应源码、NOTICE 和许可条款；当前仅更新根目录构建入口，没有下载 BtbN、生成新的 Windows 包或更新已有源码包。实际组件记录见 `THIRD_PARTY_NOTICES.md`。

Qt 常用标准控件采用应用内原创中日译文，随应用编译；部署脚本的 `--no-translations` 不会删掉这些译文，不需另下载 qm。系统原生文件/颜色对话框仍可能跟随 Windows 语言；本轮只在 Linux 非原生 Qt 对话框验证，不能据此称 Windows 本地化验收完成。

## 应用图标与目录布局

应用使用项目维护者提供的软件标识；保留笔画/配色、裁剪多余留白并等比缩放。`resources/icons/chosuta.png` 嵌入 Qt 应用，`chosuta.ico` 包含16/24/32/48/64/128/256像素图层，由Windows资源编译器嵌入GUI EXE，资源管理器和窗口使用同一标识。PNG/ICO随源码保存，普通构建不需要Python/Pillow或图片生成服务；标识来源记录见 THIRD_PARTY_NOTICES.md。

```text
Chosuta/
  chosuta.exe
  chosuta-cli.exe
  Qt6*.dll / ICU DLL / Qt媒体DLL
  qt.conf
  platforms/  imageformats/  multimedia/ ...
  tools/ffmpeg/       # 仅附带工具版：ffmpeg、ffprobe及它们的DLL
  share/chosuta/     # 说明与许可证
  dependency-notices/
  validation/
  runtime-manifest.json
  RUN.txt
```

CMake只对Windows将应用安装到前缀根目录，Linux继续使用bin。构建脚本的部署、清理SDK环境后的启动验证、RUN说明和清单均指向根chosuta.exe；媒体工具仍相对程序目录查找。MSVC的`/utf-8`限定为C++编译，不传给RC；ICO路径通过资源包含目录传递，避免把本机绝对路径或Unicode目录写进RC字符串。

本轮Linux下用已有llvm-rc/llvm-cvtres检查了7张图层及图标组资源，不能替代Windows完整链接、资源管理器显示或PowerShell部署验证。
