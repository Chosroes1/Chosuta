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

视频导出需系统 FFmpeg 或导出设置中指定的 FFmpeg 路径；附加音频时长探测及测试使用 ffprobe；优先查找所选 FFmpeg 同目录，之后查 PATH。缺少 ffprobe 时音频仍可附加，时长需手动设置。MP4 需要 libx264，WebM 需要 libvpx-vp9；配音分别需要 AAC/libopus；透明 MOV 需要 qtrle。缺少编码器只禁用对应导出，不影响编辑。生产 Qt 音频后端仅在试听时初始化；无音频启动不接触声卡。

没有桌面时：

```sh
QT_QPA_PLATFORM=offscreen ./build/chosuta --smoke-test
```

该入口自动使用原创夹具验证替换/撤销/保存重开/重生成/导出/三语切换，若 FFmpeg 不在 PATH 中则跳过导出部分。CTest 的 `core`、`ui-workflow` 与 `gui-smoke` 都指定离屏平台；core 的 FFmpeg 集成测试在工具缺失时明确跳过。离屏结果不是实际桌面/高 DPI/声卡验收。

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

脚本在 `build-release/` 构建、测试并用 CPack 输出 `out/linux/Chosuta-0.2.2-Linux.zip`；相邻 `Chosuta-0.2.2-source.zip` 是 Chosuta 对应源码，`runtime-dependencies.txt` 为本机直接动态库记录。源码归档使用明确白名单，排除用户原始材料、调研样本、build/out、Git/凭据目录；Python 只用于此开发辅助。旧版 0.2.1 交付包保留，不覆盖。

发行版安装包、AUR 配方和目标环境要求见 [Linux 发行版打包](packaging.zh-CN.md)。Linux 安装增加标准桌面菜单项和原创 SVG 图标；Windows 安装布局仍为 bin 与 share。

这些包只包含应用自身，不捆绑 Qt、ICU、FFmpeg 或系统库，必须使用匹配的系统环境。Arch 上构建成功不意味着兼容所有 Linux 发行版。未创建便携 AppImage、远端 CI 或公开发布；不把本地包当作依赖许可审查完整的最终分发包。对应源码 ZIP 若已经存在，脚本拒绝覆盖，需使用新输出目录。

`cmake --install build --prefix '<自己拥有的目录>'` 也可只安装应用与说明。不要为本地验证使用管理员目录。缺少依赖需按项目授权规则处理；Arch 软件库过期时不能通过局部升级或旧包拼接解决。
