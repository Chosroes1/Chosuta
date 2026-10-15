# Windows 编译、部署与验证

当前源码版本 **0.9.3**。Windows 脚本仅负责配置、编译、部署及可选 ZIP，不构建/运行测试，也不自动启动 GUI/CLI 验收窗口。使用 `BUILD_TESTING=OFF`，复用旧 BuildDir 时同样更新该配置。

## 所需已有环境

- Windows 10/11 **x64**，PowerShell 5.1 或更新版本。
- C++20 编译器。默认使用 MSVC（例如 Visual Studio 2022 的 C++ 工具），在 **x64 Native Tools / Developer PowerShell** 中执行；也可用与 Qt 完全匹配的 x86_64 MinGW，通过 `-Toolchain MinGW` 选择。
- CMake ≥3.24、Ninja，均在当前终端 PATH 中。
- 同一架构/编译器的 **Qt ≥6.4 动态 SDK**，包含 Core、Gui、Widgets、Concurrent、Multimedia 及 `bin/windeployqt.exe`。`QtPrefix` 指向具体编译器套件目录，其下有 `bin`、`lib/cmake/Qt6` 和 `plugins`，不是 Qt 安装器根目录。
- 同一架构/编译器的 **ICU ≥67 动态 SDK**，包含 `include/unicode`、导入库及 uc/i18n/data DLL。`IcuPrefix` 指向 SDK 根目录；脚本优先寻找 `bin64`，其次 `bin`，可用 `-IcuRuntimeDir` 指定。MSVC 常见 DLL 为 `icuucXX.dll`、`icuinXX.dll`、`icudtXX.dll`，MinGW 名称可带 `lib` 前缀。不要混用 MSVC、MinGW、32 位、ARM64 或不同版本库。
- MSVC 构建机器已有官方 **Visual C++ x64 Redistributable**。脚本不会安装它，也不会从 Visual Studio 目录随意复制编译器 DLL；目标机器运行应用需要该运行库。MinGW 使用 windeployqt 部署其对应运行库。
- FFmpeg/ffprobe 可选。没有音频和编码工具也可运行、导入、预览、编辑与保存；完整视频回归需 libx264、libvpx-vp9、qtrle、AAC/libopus。可在导出设置中指定现有 FFmpeg，或用脚本参数 `-FfmpegDir` 指定现有工具的 `bin` 目录以一起复制。

默认只使用本机已有组件；仅显式传 `-DownloadFfmpeg` 才下载 BtbN。脚本不安装 Qt/ICU/编译器或修改系统配置。Qt 部署使用 [官方 windeployqt 流程](https://doc.qt.io/qt-6.8/windows-deployment.html)；额外复制应用直接依赖的 ICU，因为 windeployqt 不负责收集所有非 Qt 第三方库。ICU 的定位使用 [CMake FindICU](https://cmake.org/cmake/help/latest/module/FindICU.html)。

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

相对构建/输出路径按**源码根目录**解析，不依赖当前工作目录；支持空格和 Unicode。默认使用 Ninja/Release，关闭 BUILD_TESTING；编译或部署失败会报告实际失败步骤，成功后显示 Built。

## 自动下载 BtbN（显式启用）

```powershell
.\scripts\build-windows.ps1 `
  -QtPrefix '<Qt x64 SDK 前缀>' `
  -IcuPrefix '<ICU x64 SDK 前缀>' `
  -DownloadFfmpeg -BothVariants -CreateZip `
  -StageDir 'out/windows/Chosuta-auto'
```

自动取得 BtbN 官方 **win64 GPL shared** 的 ffmpeg、ffprobe 和共享 DLL。默认使用 `latest` 发布标签，优先其中编号稳定分支，不再限定 9.0 或日期标签。`-FfmpegRelease` 可指定其他实际发布标签。

缓存默认位于 `out/deps/btbn/`，`-FfmpegCacheDir` 可指定路径。首次下载校验上游 SHA-256 后使用系统 ZIP 库解包；解包后删除本次下载的原 ZIP，保留工具、许可证、来源和校验清单。已有缓存直接离线复用，不逐文件重算哈希、不禁止符号链接或目录嵌套。显式标签不会改变已有默认缓存选择。已有旧缓存中的原 ZIP 不会自动删除。

下载失败报告实际错误。已有本地工具也可以通过 `-FfmpegDir` 使用。

自动下载不需要 Python、包管理器、管理员权限或账户凭据，不安装 Qt/ICU/编译器/VC++ 运行库，也不修改永久 PATH。首次联网及 ZIP/解包占用是用户选择本参数后的构建成本；具体下载量以所选发布为准。部署仍仅复制两个 EXE、bin 中 DLL 及许可文件，来源/校验清单写入 `dependency-notices/FFmpeg` 和运行清单；no-ffmpeg 版不复制这些外部工具。

源码保留开发测试文件，但此构建脚本不运行测试，不把测试结果作为部署或生成 ZIP 的前置条件。

## 一次编译生成两种包

使用已有的 BtbN **win64 GPL shared** 解压目录；`-FfmpegDir` 指向其中同时含 `ffmpeg.exe`、`ffprobe.exe` 和共享 DLL 的 `bin`。也可将 `-FfmpegDir` 换成 `-DownloadFfmpeg` 自动取得 BtbN；两者互斥。脚本不自行编译 FFmpeg，也不让这套 DLL 与 Qt 共用。

```powershell
.\scripts\build-windows.ps1 `
  -QtPrefix '<Qt x64 SDK 前缀>' `
  -IcuPrefix '<ICU x64 SDK 前缀>' `
  -FfmpegDir '<BtbN GPL shared 的 bin 目录>' `
  -BothVariants -CreateZip `
  -StageDir 'out/windows/Chosuta-0.9.3'
```

只编译一次，再各自部署两种目录；部署完成后压缩：

- `out/windows/Chosuta-0.9.3/` 和 `.zip`：附带外部工具。
- `out/windows/Chosuta-0.9.3-no-ffmpeg/` 和 `.zip`：不附带外部工具，保留 Qt／ICU 和 Qt 媒体后端。

`-BothVariants` 必须与 `-FfmpegDir` 或 `-DownloadFfmpeg` 一起用；不传 `-CreateZip` 只生成目录。只要 no-ffmpeg 版时，省略 `-FfmpegDir`／`-DownloadFfmpeg`／`-BothVariants`，直接把 `-StageDir` 命名为所需的 no-ffmpeg 名称。目录必须为空，ZIP 必须不存在，不覆盖原产物。

no-ffmpeg 用户在“文件 → 导出设置”选择本机已有的 `ffmpeg.exe`，同目录保留 `ffprobe.exe` 和该构建的共享 DLL，然后“检测媒体工具”，确定保存。检测报告显示实际工具路径、版本及缺失能力；这不能替代实际导出和剪辑软件导入验收。命令行也可检查：

```powershell
.\out\windows\Chosuta-0.9.3-no-ffmpeg\chosuta-cli.exe --check-media-tools --ffmpeg '<现有 ffmpeg.exe 完整路径>'
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

0.9.3 将附带的外部命令行工具放在 `tools/ffmpeg/`，与 Qt 的媒体 DLL 分开。查找顺序为工程路径 → 本机默认 → `tools/ffmpeg/` → 应用 EXE 同目录 → PATH；明确指定的错误路径不会静默换用其他 FFmpeg。ffprobe 先查所选 FFmpeg 同目录，再查附带工具、应用目录、PATH。导出设置提供本机默认路径、工程路径、浏览及后台检测。

缺少外部 FFmpeg 时视频导出、波形解码和重新进行波形时间校正不可用；工程生成、编辑、保存仍可用。ffprobe 缺失时音频时长需手动设置。试听依赖 Qt Multimedia，no-ffmpeg 版也必须保留 windeployqt 部署的 Qt 媒体 DLL／插件。

## 脚本实际执行的步骤

1. 检查现有 SDK、编译器与 Windows x64 目标，配置 C++20、Qt、ICU 和 `BUILD_TESTING=OFF`。
2. 编译 GUI/CLI，用 CMake 安装到部署根目录，windeployqt 复制 Qt 依赖，补充 ICU 与 qt.conf。
3. 选择 `-DownloadFfmpeg` 时取得 BtbN 并校验 SHA-256；选择本地工具时直接使用指定 bin。保留 ffmpeg/ffprobe 的版本与 nonfree 分发配置检查；两工具及 DLL 放在 tools/ffmpeg。
4. 写入版本、来源、文件大小和许可证记录；清单中的测试/启动检查状态明确为未运行。不会启动 Chosuta、调用 CTest 或复制旧测试日志冒充本次结果。
5. 按 `-CreateZip` 压缩。完成显示 Built；进程环境在结束时恢复。

原始 EXE 位于 build-windows；部署后的入口是 `out/windows/Chosuta/chosuta.exe`。StageDir 仍须为空、目标 ZIP 不得已存在，避免混入旧 DLL 或覆盖原文件；重跑可指定新的 StageDir。切换编译器/架构/SDK 使用新的 BuildDir。

失败时查看终端报告的配置、编译、下载或部署步骤。Qt/ICU/编译器架构与 ABI 需匹配。编译产物和测试结论分别记录，自动测试不再构成本地打包门槛。

应用采用 GPL-3.0-or-later；第三方版本、原有许可及对应源码说明见 [第三方清单](../THIRD_PARTY_NOTICES.md)。本轮只交付源码，未生成新的 Windows 二进制包，也未替换旧 Release。

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

CMake只对Windows将应用安装到前缀根目录，Linux继续使用bin。构建脚本的部署、RUN说明和清单均指向根chosuta.exe；媒体工具仍相对程序目录查找。MSVC的`/utf-8`限定为C++编译，不传给RC；ICO路径通过资源包含目录传递，避免把本机绝对路径或Unicode目录写进RC字符串。

本轮Linux下用已有llvm-rc/llvm-cvtres检查了7张图层及图标组资源，不能替代Windows完整链接、资源管理器显示或PowerShell部署验证。
