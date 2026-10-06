# Chosuta

Chosuta是一个导入虚拟歌姬音声合成软件工程与口型立绘后直接输出歌唱视频的软件，当前已支持并验证Synthesizer V。


## 1. 功能

根据导入的svp等工程文件与对应不同口型的立绘，生成视频文件。
在编辑页面中，用户可根据具体情况手动编辑，调整动画的时间、立绘差分。另外，时间轴也可以以节拍形式表现。
当前版本支持生成纯色背景动画并在更高级的剪辑软件中编辑，与自行导入背景、更改人物位置直接生成简单PV两种用法。当前版本尚不支持添加字幕。


## 2. 当前版本

当前版本为0.2.2,设备支持等问题仍在测试中。

## 3. 系统要求

| 项目 | 要求 |
| --- | --- |
| Linux | 与下载包匹配的发行版及架构；当前已在Arch Linux x86_64（By the way I use……）完成构建和测试，不能直接视为兼容 Debian/Fedora |
| Windows | 构建部署脚本面向 Windows 10/11 x64；已在Windows11上完成构建和测试，Windows10仍需验证 |
| 应用运行库 | Qt ≥6.4 的 Core、Gui、Widgets、Concurrent、Multimedia（含相应平台/媒体插件），ICU ≥67；实际二进制需要匹配构建时的库 ABI |
| 视频导出 | 可选外部 FFmpeg；MP4 需 libx264/AAC，WebM 需 libvpx-vp9/libopus，透明 MOV 需 qtrle；以实际编码器检查为准 |
| 音频时长探测 | 可选 ffprobe；缺少时仍可附加音频，动画时长可手动设置 |

音频、FFmpeg、声库和音声合成软件本体都不是导入、生成口形、预览、编辑或保存的前提。FFmpeg 路径可在应用中指定。Python 仅用于开发打包辅助，不是应用运行依赖。

从源码构建另需 C++20 编译器、CMake ≥3.24、Ninja、Qt/ICU 开发包；启用测试时另需 Qt Test。Windows 的编译器、Qt、ICU 必须采用匹配的架构和 ABI，MSVC 运行目录还需匹配的官方 Visual C++ x64 Redistributable。

## 4. 安装方法

### 直接下载

在本 GitHub 仓库的 [Releases](https://github.com/Chosroes1/Chosuta/releases) 页面下载与操作系统和架构匹配的二进制包。

### 从源码构建：Linux

下载并解压本仓库源码，或在本仓库页面复制真实 Clone 地址取得源码。在源码根目录、已有匹配开发依赖的环境执行：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/chosuta
```

应用可直接从构建目录运行。需要暂存安装内容时，可安装到自己拥有的目录：

```sh
cmake --install build --prefix "$PWD/stage"
./stage/bin/chosuta
```

无需桌面的自动验证可执行 `QT_QPA_PLATFORM=offscreen ./build/chosuta --smoke-test`；这不代替真实桌面/声卡验收。关闭测试可传 `-DBUILD_TESTING=OFF`。脚本不会自动下载依赖；Arch 上应使用一致的系统仓库环境，不做局部升级。

### 从源码构建：Windows

在配置好 x64 C++20 工具链的 PowerShell 中，使用已安装的匹配 Qt/ICU 动态 SDK：

```powershell
.\scripts\build-windows.ps1 -QtPrefix '<Qt x64 SDK 前缀>' -IcuPrefix '<ICU x64 SDK 前缀>'
.\out\windows\Chosuta\bin\chosuta.exe
```

脚本构建、测试并部署 Qt/ICU，移除 SDK PATH 后执行启动检查，通过后才显示 Ready。MSVC 为默认工具链；MinGW 需传 `-Toolchain MinGW` 并使用完全匹配的 SDK。脚本不安装依赖、不修改永久 PATH。完整参数和验证边界见 [Windows 构建说明](docs/windows.zh-CN.md)，通用说明见 [构建文档](docs/building.zh-CN.md)。

## 5. 使用说明

可参考 [使用说明](docs/user-guide.zh-CN.md)。

## 6. 实现原理

Chosuta 使用 C++20 与 Qt 6 Widgets；核心模型独立于 GUI 控件。自动口形由工程文件中的乐谱、显式音素和歌词通过确定性规则生成，音频仅用于试听或视频配音。

1. **导入乐谱**：读取 SVP 实际引用的 mainGroup/mainRef 与 library 分组，展开引用实例并应用时间/音高偏移。保留整数音乐位置，SV 每四分音符为 705,600,000 blick；根据分段 BPM 换算秒数，拍号用于小节/拍显示。
2. **解析读音**：非空显式音素优先，否则解析歌词。日语使用假名/罗马音规则；中文使用拼音及 ICU Han-Latin 转写；英文使用原创有限词表，可读取用户本地提供的 CMU 格式词典。未知或多音字读音可人工修正，不标为精确结果。
3. **生成事件**：按确定性分段规则分配口形和估计时序，处理多轨优先级、延音、呼吸、休止及辅音默认分类。尚未核实含义的 leftOffset 只保留并诊断，不伪造精确边界。
4. **保存编辑**：原始来源、规则结果和人工覆盖分别保存。撤销/重做作用于工程操作，重新生成保留锁定修改；素材路径可相对工程保存并重定位。
5. **合成视频**：预览与导出共用 Scene，按每帧绝对时间求值，支持画布、背景图及立绘缩放/位置。通过程序路径和参数数组启动外部 FFmpeg 编码；无音频也能导出。

自动结果是动画时序估计，不复现 SV 私有声学模型。TimingRefiner 仅保留后续音频校正接口，当前没有校正算法、声库加载或神经网络模型。更多规则与局限见 [使用说明](docs/user-guide.zh-CN.md)。

## 7. 项目结构

```text
CMakeLists.txt              构建、测试、安装及 CPack 入口
src/core/                   时间与工程模型、外部工具定位
src/importers/              SVP 导入
src/pronunciation/          中英日读音解析
src/rules/                  口形事件生成与轨道选择
src/project/                工程保存与恢复
src/render/                 预览/导出合成、FFmpeg 与音频元数据
src/ui/                     Qt Widgets 界面、时间轴、偏好设置
src/cli/                    无界面演示、导入、生成与导出
resources/                  原创词表、图标及第三方许可文本
tests/                      核心/UI 测试与原创最小 SVP 夹具
cmake/                      构建信息和发行版打包模板
packaging/                  Linux 桌面项及 Arch 配方模板
scripts/                    Windows 部署、本地打包与公开源码整理
docs/                       构建、操作、打包和许可证说明
README.en.md / README.ja.md  英文与日文 README
LICENSE                     GNU GPL v3 完整文本
THIRD_PARTY_NOTICES.md       实际组件、来源与分发状态
```

公开源码不包含用户歌曲、立绘、音频、私有样本回归数据、历史开发会话、构建目录或凭据。公开测试使用原创夹具；本地私有回归缺失时明确跳过。

## 8. 依赖与第三方组件

| 组件 | 用途 | 许可/处理方式 |
| --- | --- | --- |
| Qt 6 Core/Gui/Widgets/Concurrent/Multimedia；传递 Qt Network | JSON、Unicode、图像、界面、后台任务、试听 | 当前以 LGPL-3.0-only 选项动态链接；系统依赖，不捆绑在 Arch 包中 |
| ICU4C 与 ICU 数据 | 中文转写与 Unicode 处理 | Unicode-3.0 及上游附带条款；完整原始许可文本已保留 |
| FFmpeg / ffprobe | 可选编码和音频时长探测 | 独立进程；许可取决于具体构建。本机为 GPL-3.0-or-later，不含 enable-nonfree，不捆绑 |
| Qt Test、moc、rcc | 开发/测试与代码生成 | 按 Qt 实际工具许可及适用例外；不成为应用运行依赖 |
| CMake / Ninja / C++ 编译器 | 构建与安装 | 构建工具；CMake 为 BSD-3-Clause，Ninja 为 Apache-2.0，编译器及运行库按实际工具链许可 |
| Python 3、makepkg、fakeroot | 本地开发打包辅助 | 仅开发用途；不捆绑，不成为应用运行依赖 |

内置常用英文词表、图标和测试素材为 Chosuta 原创，按项目 GPLv3 许可分发。没有集成完整 CMUdict、OpenJTalk、SV 程序或声库，也没有捆绑第三方字体。具体已验证版本、来源与附带条款见 [第三方组件清单](THIRD_PARTY_NOTICES.md)，许可核查见 [兼容性说明](docs/license-review.zh-CN.md)。

## 9. AI 使用声明

本软件的架构、使用方法、功能由人类提供，代码由天才程序员ChatGPT开发，并通过人工审阅和验证。

## 10. BUG 报告

如有BUG报告、功能建议，请邮箱联系2441002630@qq.com。

## 11. 许可证

Copyright (C) 2026 Chosuta contributors。

除另有明确标注外，Chosuta 源码、文档及原创资源按 **GNU General Public License 第 3 版或任何更新版本（GPL-3.0-or-later）** 分发，不提供任何担保。完整文本见 [LICENSE](LICENSE)。第三方组件保留各自许可，不因被调用而改为 Chosuta 的许可。

依据当前组件、实际链接方式和分发内容进行核查，未发现阻止当前源码与 Arch 应用包按 GPLv3 分发的明确许可证冲突。Qt 的 LGPLv3 选项允许与 GPLv3 应用组合；ICU 主许可为宽松 Unicode 许可，附带条款全文保留；构建工具与应用运行代码分别记录。[Qt 官方许可](https://doc.qt.io/qt-6.11/licensing.html)、[Unicode 官方说明](https://unicode.org/faq/unicode_license.html)。

分发应用二进制时需同时提供对应版本源码和构建材料，并保留版权/NOTICE。若以后捆绑 Qt、ICU、媒体后端、FFmpeg 或其他库，还须落实该具体构建的对应源码和适用分发义务；不能只提供 Chosuta 源码替代这些依赖的义务。带 `--enable-nonfree` 的 FFmpeg 不作为分发依赖。[FFmpeg 许可说明](https://ffmpeg.org/legal.html)。

用户导入的工程、PNG、背景及音频仍由各自权利人决定用途，本项目许可不授予其再分发权。
