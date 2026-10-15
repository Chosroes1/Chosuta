# 构建、验证与本地打包

当前根源码为 0.9.3/schema14。C++20、CMake ≥3.24、Qt ≥6.4、ICU ≥67；使用已经安装且状态一致的开发环境，不自动下载依赖。Linux 构建结果不能代替 Windows 原生验证。

## Linux

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF
cmake --build build --parallel 4
./build/chosuta
```

核心依赖 Qt Core/Gui、ICU i18n/uc/data；GUI 另依赖 Widgets/Concurrent/Multimedia，Qt Network 为传递依赖。普通构建不需要 Qt Test。标准 Qt 控件的中日译文由应用源码编译，不需部署 qm 文件；OS 原生对话框语言仍由系统影响。Python 仅用于开发打包及 Linux 打包预检测试，不是产品运行依赖。

媒体执行使用一次选定的 FFmpeg/ffprobe 上下文。工程显式路径直接使用；CLI 可用 `--ffmpeg <路径>` 明确选定本次程序。自动标记沿用本机默认、tools/ffmpeg、应用目录、PATH 顺序；ffprobe 优先在所选 FFmpeg 旁边。缺工具仍可编辑保存。MP4 使用 libx264/AAC，WebM 使用 libvpx-vp9/libopus，透明 MOV 使用 qtrle；波形解码复用 FFmpeg，无模型。Qt 试听后端在播放音频时才初始化。

## 开发测试

CMake 的 `BUILD_TESTING` 默认关闭。测试源码供开发者主动使用，普通构建和 Linux/Windows 打包均不运行测试，不要求 Qt Test 或 PowerShell 测试夹具。

## Windows

依赖、MSVC/MinGW、Qt/ICU 部署和双版本媒体工具布局见 [Windows 编译、部署与验证](windows.zh-CN.md)。已有匹配的 x64 环境时：

```powershell
.\scripts\build-windows.ps1 -QtPrefix '<Qt x64 SDK 前缀>' -IcuPrefix '<ICU x64 SDK 前缀>'
.\out\windows\Chosuta\chosuta.exe
```

脚本配置 `BUILD_TESTING=OFF`，编译并部署 Qt/媒体/平台插件、ICU；不运行 CTest 或启动应用验收。`-BothVariants -CreateZip` 可从同次编译部署附带工具及 no-ffmpeg 两版；外部媒体 DLL 与 Qt 媒体依赖隔离。显式传 `-DownloadFfmpeg` 可校验下载并缓存 BtbN win64 GPL shared，或用 `-FfmpegDir` 选择已有工具；默认不下载，不安装 VC++ 运行库；构建清单明确记录测试未运行。

## 本机开发包

```sh
bash scripts/package-linux.sh build-release out/linux-new
```

输出目录必须尚不存在；在任何构建或打包写入之前检查。所有打包步骤先写本次独有暂存目录，成功后以不覆盖方式移至目标；过程中出现的目标也保留原样。包含 Linux ZIP、对应白名单源码 ZIP、runtime-dependencies.txt。失败/取消只清理本次暂存目录，旧包不覆盖。

发行版安装包见 [Linux 发行版打包](packaging.zh-CN.md)。应用包动态链接本机 Qt/ICU/系统库，不是跨发行版自包含包；应在目标发行版重编。也可用 `cmake --install build --prefix '<自己拥有的目录>'` 安装到本机目录，不需管理员权限。缺依赖按项目授权处理；Arch 不以局部升级或归档旧包拼接环境。

应用图标已内置于GUI资源，Linux安装到`share/icons/hicolor/512x512/apps/chosuta.png`并关联chosuta桌面项；Windows GUI/CLI安装在部署根目录，图标ICO编译进GUI EXE。PNG/ICO均已提供，构建不新增图像处理工具依赖。
