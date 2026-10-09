# Chosuta

Chosuta是一个导入虚拟歌姬音声合成软件工程与口型立绘后直接输出歌唱视频的软件，当前已支持并验证Synthesizer V。


## 1. 功能

根据导入的svp等工程文件与对应不同口型的立绘，生成视频文件。  
在编辑页面中，用户可根据具体情况手动编辑，调整动画的时间、立绘差分。另外，时间轴也可以以节拍形式表现。  
当前版本支持生成纯色背景动画并在更高级的剪辑软件中编辑，与自行导入背景、更改人物位置直接生成简单PV两种用法。当前版本尚不支持添加字幕。  
0.4.1新增字幕功能。  
0.5.0新增音频波形校正。 
0.6.4为旧方向归档版本，在高级设置里增加了上行/下行、级进/大跳/小跳、强拍/弱排等差分，排列组合下来有一百多个。到最后十分麻烦且效果不佳，目前在考虑别的开放方向，仅在release放出归档，不做分支。

## 2. 当前版本

当前release版本为0.6.4,设备支持等问题仍在测试中。

## 3. 系统要求

| 项目 | 要求 |
| --- | --- |
| Linux | 与下载包匹配的发行版及架构；当前本机二进制为 Arch Linux x86_64，不能直接视为兼容 Debian/Ubuntu/Fedora |
| Windows | 构建部署脚本面向 Windows 10/11 x64；原生构建和运行仍需 Windows 环境验证 |
| 应用运行库 | Qt ≥6.4 的 Core、Gui、Widgets、Concurrent、Multimedia（含相应平台/媒体插件），ICU ≥67；实际二进制需要匹配构建时的库 ABI |
| 视频导出 | 可选外部 FFmpeg；MP4 需 libx264/AAC，WebM 需 libvpx-vp9/libopus，透明 MOV 需 qtrle；以实际编码器检查为准 |
| 音频时长探测 | 可选 ffprobe；缺少时仍可附加音频，动画时长可手动设置 |

音频、FFmpeg、声库和 Synthesizer V 本体都不是导入、生成口形、预览、编辑或保存的前提。FFmpeg 路径可在应用中指定。Python 仅用于开发打包辅助，不是应用运行依赖。

从源码构建另需 C++20 编译器、CMake ≥3.24、Ninja、Qt/ICU 开发包；启用测试时另需 Qt Test。Windows 的编译器、Qt、ICU 必须采用匹配的架构和 ABI，MSVC 运行目录还需匹配的官方 Visual C++ x64 Redistributable。

## 4. 安装方法

### 直接下载

在本 GitHub 仓库的 **Releases** 页面下载与操作系统和架构匹配的二进制包。文件是否可用、捆绑哪些依赖及验证范围以该 Release 的说明为准；GitHub 自动生成的 Source code ZIP/tar.gz 是源码，需要编译。

- **Arch Linux**：下载 `chosuta-<版本>-<打包修订>-x86_64.pkg.tar.zst`，在下载目录执行 `sudo pacman -U ./chosuta-<版本>-<打包修订>-x86_64.pkg.tar.zst`。安装后运行 `chosuta` 或桌面菜单中的 Chosuta；CLI 为 `chosuta-cli`。本机包不捆绑 Qt/ICU/FFmpeg，由系统包管理器提供依赖。
- **Windows**：如 Release 提供经过原生验证的运行目录 ZIP，完整解压并运行其中的 `bin/chosuta.exe`；保留 DLL、插件及其他相邻文件。当前仓库提供构建部署脚本，不能据此认定 Windows 二进制已经发布或验证。
- **其他 Linux**：只选用 Release 明确匹配目标发行版的包。DEB 可用 `sudo apt install ./<文件名>.deb`，RPM 按目标发行版用其包管理器安装；当前这些目标包尚待实际构建验证。Linux 通用 ZIP 仍需要匹配的系统运行库。

AUR 分发的是构建配方，用户通过 makepkg 得到安装包。当前没有公布 AUR 条目，不能直接给出 yay/paru 安装命令；公开条目需维护者配置真实上游源码地址。详见 [发行版打包说明](docs/packaging.zh-CN.md)。

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

Chosuta 使用 C++20 与 Qt 6 Widgets；核心模型独立于 GUI 控件。自动口形由工程文件中的乐谱、显式音素和歌词通过确定性规则生成，音频可用于试听、配音及用户主动触发的有界时间校正，不决定口形内容。

1. **导入乐谱**：读取 SVP 实际引用的 mainGroup/mainRef 与 library 分组，展开引用实例并应用时间/音高偏移。保留整数音乐位置，SV 每四分音符为 705,600,000 blick；根据分段 BPM 换算秒数，拍号用于小节/拍显示。
2. **解析读音**：非空显式音素优先，否则解析歌词；逐音符语言覆盖优先于分组及轨道继承，兼容全角续音符号。日语默认使用假名/罗马音规则，汉字估计为默认关闭的可选项；中文使用常见词语读音、拼音及 ICU Han-Latin；英文使用原创有限词表。三语内置读音数据约 14 KB，高级设置支持词条编辑、JSON 导入导出和持久保存；内置、自定义及可选外部词典合计不超过 3 MB。未知或多音字读音可人工修正，不标为精确结果。
3. **生成事件**：按确定性分段规则分配口形和估计时序，处理多轨优先级、延音、呼吸、休止及辅音默认分类。尚未核实含义的 leftOffset 只保留并诊断，不伪造精确边界。
4. **保存编辑**：原始来源、规则结果和人工覆盖分别保存。撤销/重做作用于工程操作，重新生成保留锁定修改；素材路径可相对工程保存并重定位。
5. **合成视频**：预览与导出共用 Scene，按每帧绝对时间求值，支持画布、背景图及立绘缩放/位置。通过程序路径和参数数组启动外部 FFmpeg 编码；无音频也能导出。

自动结果是动画时序估计，不复现 SV 私有声学模型。TimingRefiner 的提供者接口保留；0.5.0 新增无模型波形时间估计，不加载声库或神经网络。更多规则与局限见 [使用说明](docs/user-guide.zh-CN.md)。

可选字幕默认关闭，启用后支持多字幕轴、双击手动添加、区间拖动、文字及基础样式；歌词时间对齐可选且指定来源轨道，依据谱面音符，不识别音频或自动匹配句意。字幕独立于口形事件，重生成不覆盖它。立绘和字幕可在暂停预览中拖动布局/调整大小，保留数值精调；一次拖动一次撤销。工程只读写当前 schema 9，字幕通过共享 Scene 进入预览与视频；字体使用本地系统字体，跨机器可能发生替换。操作见[用户说明](docs/user-guide.zh-CN.md)。

0.4.1 工作区修订：轴内播放定位只通过上方时间标尺，贯穿红线保留；空白取消选择/文字焦点而不跳转。字幕单击选中/拖动、双击块内编辑，Delete/Backspace 按焦点操作对象或字符；行高和显示区域可拖动调整。新工程辅音组按比例并设默认 80 毫秒上限，尾辅音独立分配；上限 0 表示不设固定毫秒上限，锁定覆盖保留。只复用一个按需 Qt 编辑控件，没有新依赖。

0.5.0 工作区新增：附加音频后显示位于标尺下、音节行上的波形，拖动下边界只调显示高度。导入不会自动校正；点击“按波形校正时间”才运行，默认最大偏移 100 ms、最大时长变化 25%，可调整并随校正保存。明确能量边界在工程附近独立估计，不改口形/音素/段内比例、不累计偏移；歧义、连续音、人工覆盖与冲突保留工程结果。校正层可回退/撤销，保存重开与预览/导出共用；重新生成或输入变化清除/失效旧校正。混音可作波形参考，但不保证人声边界准确；不加入模型或声部分离。CLI 提供 `correct/revert`，详见操作说明。

工作区 **0.6.4**：默认简单口形保留；独立高级差分窗口提供方向、半音幅度、强弱拍的选择与启用框，发声固定五槽、特殊固定三槽。闭口/促音/休息/呼吸优先匹配动作与神态条件，缺对应图时自动使用通用/基础图和全局回退；仅所有候选均无效才缺图并拒绝导出。支持规范名文件夹扫描与冲突清单、工程保存/撤销；新工程推荐 b/p/m 闭口，其他辅音跟随元音，规则修改入口默认收起。高级命名统一单下划线，如 A_strong_up_step.png；只读写 schema 9，旧工程兼容与预设已移除；只改外观不清波形校正，改辅音规则明确重新生成。图片按需、同路径共用且缓存有界，无新增运行依赖。详见[操作说明](docs/user-guide.zh-CN.md)。

幅度另可选择严格乐理度数（含教会调式和音名校正），方向另提供“同音反复”，提示幅度选通用；默认半音与原页面结构保留。

## 7. 项目结构

```text
CMakeLists.txt              构建、测试、安装及 CPack 入口
src/core/                   时间与工程模型、外部工具定位
src/importers/              SVP 导入
src/pronunciation/          中英日读音解析
src/rules/                  口形事件生成与轨道选择
src/project/                工程保存与恢复
src/render/                 预览/导出合成、FFmpeg、音频元数据与波形摘要
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
| FFmpeg / ffprobe | 可选编码、音频时长探测与波形解码 | 独立进程；许可取决于具体构建。本机为 GPL-3.0-or-later，不含 enable-nonfree，不捆绑 |
| Qt Test、moc、rcc | 开发/测试与代码生成 | 按 Qt 实际工具许可及适用例外；不成为应用运行依赖 |
| CMake / Ninja / C++ 编译器 | 构建与安装 | 构建工具；CMake 为 BSD-3-Clause，Ninja 为 Apache-2.0，编译器及运行库按实际工具链许可 |
| Python 3、makepkg、fakeroot | 本地开发打包辅助 | 仅开发用途；不捆绑，不成为应用运行依赖 |

内置中英日轻量读音表、图标和测试素材为 Chosuta 原创，按项目 GPLv3 许可分发。没有集成完整 CMUdict、OpenJTalk、SV 程序或声库，也没有捆绑第三方字体。具体已验证版本、来源与附带条款见 [第三方组件清单](THIRD_PARTY_NOTICES.md)，许可核查见 [兼容性说明](docs/license-review.zh-CN.md)。

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
