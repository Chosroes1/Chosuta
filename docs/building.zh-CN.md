# 构建、验证与本地打包

当前根源码为 0.9.3/schema14。C++20、CMake ≥3.24、Qt ≥6.4、ICU ≥67；使用已经安装且状态一致的开发环境，不自动下载依赖。Linux 构建结果不能代替 Windows 原生验证。

## Linux

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/chosuta
```

核心依赖 Qt Core/Gui、ICU i18n/uc/data；GUI 另依赖 Widgets/Concurrent/Multimedia，Qt Network 为传递依赖。测试用 Qt Test；`-DBUILD_TESTING=OFF` 可关闭。标准 Qt 控件的中日译文由应用源码编译，不需部署 qm 文件；OS 原生对话框语言仍由系统影响。Python 仅用于开发打包及 Linux 打包预检测试，不是产品运行依赖。

媒体执行使用一次选定的 FFmpeg/ffprobe 上下文。工程携带显式工具路径时须在本机采纳；CLI 可用 `--ffmpeg <路径>` 明确选定本次程序。自动标记沿用本机默认、tools/ffmpeg、应用目录、PATH 顺序；ffprobe 优先在所选 FFmpeg 旁边。缺工具仍可编辑保存。MP4 使用 libx264/AAC，WebM 使用 libvpx-vp9/libopus，透明 MOV 使用 qtrle；波形解码复用 FFmpeg，无模型。Qt 试听后端在播放音频时才初始化。

## 当前回归入口

CTest 的 `core`、`ui-workflow`、`gui-smoke` 在Windows使用原生 `windows` 平台及系统字体（需要可用桌面会话），其他平台使用 `offscreen`；Linux 上找到 Python 时另启用 `packaging-preflight`。缺 FFmpeg 的媒体测试明确跳过。私有样本未提供时私有检查跳过，不复制用户材料到公开测试。

- 当前 Linux Debug/Release 各 CTest 4/4，核心 52、UI 32 无失败/跳过；离屏结果不代表真实桌面或 Windows 验收。
- 新增 `structuredMessagesAndReasons`、`localizedControlsAndErrors`、`localizedIdsAndUndo`、`localizedPerformanceReasons`、`unknownCopyAndImport`；涵盖三语实际控件、保存点/撤销、结构化理由、错误原文、素材回退与保存重开。
- 核心覆盖 SVP 引用/整数时间、词典/辅音、人工层、schema14 严格往返、共享来源路由/分段正逆映射（拆分、移动、缩放、同源合并与保存重开）、演出模板/保护/相位、字幕、素材回退、校正与短片编码解码。
- `strictProjectContract`、`mediaToolTrust`、`routedSourceTime`、`groupReferencesAndImportTransactions`、`exportPreflight` 对应此次读写、工具、裁切、素材事务与音频复核回归。
- UI 覆盖原生字幕输入/焦点、撤销、播放/停止、标尺/行高、草稿、设置及导入。新增 `projectToolDoesNotExecuteOnOpen`、`damagedPreferences`、`ruleTransactionAndStablePlayback`、`draftCloseKeepsReview`、`compactWorkspace`。
- 打包预检使用临时目录和合成工具，验证已有交付零写入、失败只清任务暂存目录、成功完整提交暂存产物，不以真实旧 Release 做实验。

```sh
QT_QPA_PLATFORM=offscreen ./build/chosuta --smoke-test
QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 ./build/chosuta-ui-tests compactWorkspace waveformWorkflow timelineHeightsAndEditorGeometry subtitleFocusCommandsAndRuler performanceV2Controls
CHOSUTA_TEST_UI_LANGUAGE=ja QT_QPA_PLATFORM=offscreen ./build/chosuta-ui-tests performanceWorkflow performanceTemplateEditing
```

`CHOSUTA_COMPACT_ARTIFACTS=<本机目录>` 可保存三语 1024×768、1366×768 截图；`CHOSUTA_ADVANCED_ARTIFACTS`、`CHOSUTA_WAVEFORM_ARTIFACTS`、`CHOSUTA_SUBTITLE_ARTIFACTS` 用于对应原创测试产物。这些可选证据不进入公开源码包。离屏/合成色图不等于真实桌面、输入法、声卡、完整立绘效果或 Windows 验收。

0.9.3 修正 Windows 离屏环境缺少字体导致的小屏布局测试失败；UI 测试入口检查真实字体可用性并输出平台/字体，保留三语及1024×768/1366×768尺寸断言。仅修改脚本外的环境变量不能代替重新运行CMake，因为CTest属性会指定测试平台。

## Windows

依赖、MSVC/MinGW、Qt/ICU 部署和双版本媒体工具布局见 [Windows 编译、部署与验证](windows.zh-CN.md)。已有匹配的 x64 环境时：

```powershell
.\scripts\build-windows.ps1 -QtPrefix '<Qt x64 SDK 前缀>' -IcuPrefix '<ICU x64 SDK 前缀>'
.\out\windows\Chosuta\chosuta.exe
```

脚本部署 Qt/媒体/平台插件、ICU；清理 SDK PATH 后验证 CLI、离屏与 qwindows 启动。`-BothVariants -CreateZip` 可从同次编译部署附带工具及 no-ffmpeg 两版；外部媒体 DLL 与 Qt 媒体依赖隔离。不会下载工具或安装 VC++ 运行库；Windows/PowerShell 原生结果仍需在目标环境记录。

## 本机开发包

```sh
bash scripts/package-linux.sh build-release out/linux-new
```

输出目录必须尚不存在；在任何构建或打包写入之前检查。所有打包步骤先写本次独有暂存目录，成功后以不覆盖方式移至目标；过程中出现的目标也保留原样。包含 Linux ZIP、对应白名单源码 ZIP、runtime-dependencies.txt。失败/取消只清理本次暂存目录，旧包不覆盖。

发行版安装包见 [Linux 发行版打包](packaging.zh-CN.md)。应用包动态链接本机 Qt/ICU/系统库，不是跨发行版自包含包；应在目标发行版重编。也可用 `cmake --install build --prefix '<自己拥有的目录>'` 安装到本机目录，不需管理员权限。缺依赖按项目授权处理；Arch 不以局部升级或归档旧包拼接环境。

应用图标已内置于GUI资源，Linux安装到`share/icons/hicolor/512x512/apps/chosuta.png`并关联chosuta桌面项；Windows GUI/CLI安装在部署根目录，图标ICO编译进GUI EXE。PNG/ICO均已提供，构建不新增图像处理工具依赖。
