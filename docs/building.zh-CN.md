# 构建、验证与本地打包

## Linux

使用已经安装且状态一致的 C++20 / Qt 6 / ICU 开发环境。CMake 最低 3.24，Qt 最低 6.4，ICU 最低 67。默认动态链接，未使用 FetchContent 或自动包下载。

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/chosuta
```

核心库依赖 Qt Core/Gui 和 ICU i18n/uc/data；GUI 另依赖 Widgets/Concurrent/Multimedia，Qt Network 为传递依赖；测试需要 Qt Test。关闭测试可用 `-DBUILD_TESTING=OFF`，产品运行不依赖 Qt Test 或 Python。

视频导出需系统 FFmpeg 或导出设置中指定的 FFmpeg 路径；附加音频时长探测及测试使用 ffprobe；优先查找所选 FFmpeg 同目录，之后查 PATH。缺少 ffprobe 时音频仍可附加，时长需手动设置。MP4 需要 libx264，WebM 需要 libvpx-vp9；配音分别需要 AAC/libopus；透明 MOV 需要 qtrle。缺少编码器只禁用对应导出，不影响编辑。0.5.0 的可选波形同样复用 FFmpeg 解码；缺少 FFmpeg 时波形不可用，工程生成/编辑/保存仍可用。没有新增模型或运行依赖。生产 Qt 音频后端仅在试听时初始化；无音频启动不接触声卡。

没有桌面时：

```sh
QT_QPA_PLATFORM=offscreen ./build/chosuta --smoke-test
```

工作区 0.5.0 的 ui-workflow 还覆盖默认隐藏的多字幕轴、对齐、布局手柄、撤销、导出尾部选择及保存重开，core 覆盖 schema 5（读取 1/2/3/4）兼容、字幕时间/来源/文字合成与短片解码，以及有界前导/尾辅音预算和旧规则/锁定覆盖兼容。ui-workflow 新增焦点与删除键、原生块内光标/输入法事件、独立标尺/留白不定位/贯穿红线、行高与区域分隔调整、多轴滚动及保存点撤销。该入口自动使用原创夹具验证替换/撤销/保存重开/重生成/导出/三语切换，若 FFmpeg 不在 PATH 中则跳过导出部分。CTest 的 `core`、`ui-workflow` 与 `gui-smoke` 都指定离屏平台；core 的 FFmpeg 集成测试在工具缺失时明确跳过。离屏结果不是实际桌面/高 DPI/声卡验收。

## Windows（完整构建/部署入口，原生执行待验证）

具体依赖、MSVC/MinGW 命令、默认可运行目录和验证日志见 [Windows 编译、部署与验证](windows.zh-CN.md)。已有匹配的 x64 开发环境时：

```powershell
.\scripts\build-windows.ps1 -QtPrefix '<Qt x64 SDK 前缀>' -IcuPrefix '<ICU x64 SDK 前缀>'
.\out\windows\Chosuta\bin\chosuta.exe
```

脚本不再只暂存 EXE：执行 CTest 后部署 Qt/平台/媒体插件、ICU 和离屏插件，并在清除 SDK PATH 的环境中测试 CLI、离屏 GUI、qwindows GUI 启动，成功后输出实际运行依赖清单。脚本不会安装 VC++ 运行库或其他工具；MSVC 目标需要已有官方 x64 Redistributable。当前 Linux 环境不能提供 Windows 原生通过证据。

## 本机开发包

```sh
bash scripts/package-linux.sh
```

脚本在 `build-release/` 构建、测试并用 CPack 输出 `out/linux/Chosuta-<当前代码版本>-Linux.zip`；相邻 `Chosuta-<当前代码版本>-source.zip` 是 Chosuta 对应源码，`runtime-dependencies.txt` 为本机直接动态库记录。源码归档使用明确白名单，排除用户原始材料、调研样本、build/out、Git/凭据目录；Python 只用于此开发辅助。旧版 0.2.1 交付包保留，不覆盖。

发行版安装包、AUR 配方和目标环境要求见 [Linux 发行版打包](packaging.zh-CN.md)。Linux 安装增加标准桌面菜单项和原创 SVG 图标；Windows 安装布局仍为 bin 与 share。

这些包只包含应用自身，不捆绑 Qt、ICU、FFmpeg 或系统库，必须使用匹配的系统环境。Arch 上构建成功不意味着兼容所有 Linux 发行版。未创建便携 AppImage、远端 CI 或公开发布；不把本地包当作依赖许可审查完整的最终分发包。对应源码 ZIP 若已经存在，脚本拒绝覆盖，需使用新输出目录。

`cmake --install build --prefix '<自己拥有的目录>'` 也可只安装应用与说明。不要为本地验证使用管理员目录。缺少依赖需按项目授权规则处理；Arch 软件库过期时不能通过局部升级或旧包拼接解决。

## 0.5.0 波形回归

`core` 增加原创 PCM/WAV 的流式解码、峰值摘要、工程偏移/时长上限、比例/身份不变、重复不累加、歧义/连续音回退、人工保护、schema 5 保存/非法参数、失效/取消/缺工具与 Scene 帧求值。`ui-workflow` 增加导入不校正、主动执行/回退/撤销/重开、准备取消/重读、波形行高/偏好、共享标尺/秒拍/字幕编辑器几何。两倍离屏可运行 `waveformWorkflow timelineHeightsAndEditorGeometry subtitleFocusCommandsAndRuler`；这些仍不代表真实声卡/输入法/Windows 验收。

CLI 保留独立 `correct/revert` 入口，参数与操作见 [user-guide](user-guide.zh-CN.md)。波形摘要为 10 ms 的最小/最大振幅及 RMS 加逐级峰值层，不保存整首 PCM 或谱图；最多解码六小时。当前会话内复用一个共享摘要，重新打开音频需重建，不增加模型/磁盘摘要缓存或隐式下载。

## 0.5.0 GUI 复验修复

本轮修改仅涉及时间轴布局与偏好默认值，使用 `cmake --build build --parallel 4`、`ctest --test-dir build -R '^(ui-workflow|gui-smoke)$' --output-on-failure --verbose` 验证。`waveformWorkflow` 增加最小显示区域内点击波形/字幕/发音后首字幕行仍可见、最小高度不变与工程字幕不变；`timelineHeightsAndEditorGeometry` 检查默认发音高度 96、收紧留白处的绘制/命中及恢复/保存。旧测试的字幕点击改为实际行几何，避免把历史行高常数当行为契约。两倍离屏相关回归使用 `QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR=2 ./build/chosuta-ui-tests waveformWorkflow timelineHeightsAndEditorGeometry subtitleFocusCommandsAndRuler subtitlesAndCanvasDrag`。本轮证据另存 `out/timeline-layout-validation`；历史核心/视频证据仍在 `out/waveform-validation`，不因 GUI 留白修订重复编码，也不重打包。
