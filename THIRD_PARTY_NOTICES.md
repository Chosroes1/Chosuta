# Chosuta 第三方组件与素材记录

更新时间：2026-10-13（新增软件标识记录；历史依赖版本按各次实际证据）。Chosuta 自有代码、中英日轻量读音表、原创几何素材生成器和合成测试夹具采用 **GPL-3.0-or-later**；主许可见 [LICENSE](LICENSE)。除本文件记录的许可文本外，没有移植或复制第三方源码。下表区分运行时组件、构建工具和参考材料。

| 组件 | 当前实际版本 / 来源 | 许可证与用途 | 修改与分发状态 |
| --- | --- | --- | --- |
| Qt Core / Gui / Widgets / Concurrent / Multimedia；传递 Qt Network | 本机 Qt 6.11.2；构建最低 Qt 6.4；[Qt 源码](https://code.qt.io/cgit/qt/) | 本项目按 LGPL-3.0-only 选项动态链接；JSON、Unicode、图像、GUI、任务及可选播放。[官方许可说明](https://doc.qt.io/qt-6.11/licensing.html) | 未修改 Qt。当前本地应用包不包含 Qt 库或平台/媒体插件；对应 Qt 包由系统提供。完整 GPL/LGPL 文本已保留于 resources/licenses。 |
| Qt Test、moc、rcc | 同上；仅开发/测试 | Qt Test 属于 Qt Base；工具的 GPL-3.0 与 Qt GPL exception 选项见 [工具许可](https://doc.qt.io/qt-6.11/licensing.html)。测试/元对象/资源编译 | 不包含在应用运行包；生成的元对象/资源代码由应用构建产生。 |
| ICU4C / ICU 数据 | 本机 78.3；最低 67；[上游](https://github.com/unicode-org/icu)、[变换文档](https://unicode-org.github.io/icu/userguide/transforms/general/) | Unicode-3.0 及 ICU LICENSE 所列附带条款（含历史 ICU、BSD、NAIST-2003 等）；直接链接 i18n/uc/data。Han-Latin 默认汉字读音与音调规范化 | 未修改、未复制字典数据。当前包不捆绑 ICU 动态库；本机完整 ICU LICENSE 原样保留为 resources/licenses/ICU.txt。默认读音不承诺上下文消歧。 |
| FFmpeg / ffprobe | 本机 9.0.2；[上游源码](https://ffmpeg.org/download.html)、[许可与构建组合](https://ffmpeg.org/legal.html) | 独立外部进程。实际本机构建 `-L` 声明 GPL-3.0-or-later，配置含 enable-gpl、enable-version3、libx264/libvpx/libopus，不含 enable-nonfree。编码 / 可选音频元数据探测 / 波形 PCM 解码 / 测试探测 | 未修改、不捆绑。编码结果由 FFmpeg 返回。Qt Multimedia 自身的 FFmpeg 后端属于实际 Qt 部署依赖，也需在以后捆绑时核查。 |
| GCC 16.2.1 | 系统开发工具；[GCC](https://gcc.gnu.org/) | GPL-3.0-or-later，运行时按 GCC Runtime Library Exception 3.1 等实际条款；C++20 编译 | 不捆绑编译器；本机动态 libstdc++ 属于系统运行依赖。 |
| CMake 4.4.4 | 系统开发工具；[CMake](https://cmake.org/) | BSD-3-Clause；构建/安装/CPack | 不捆绑，不下载组件。 |
| Ninja 1.13.2 | 系统开发工具；[Ninja](https://github.com/ninja-build/ninja) | Apache-2.0；构建 | 不捆绑。 |
| Python 3 | 系统开发工具；[Python](https://www.python.org/) | PSF-2.0；打包辅助 / 构造开发夹具 | **不是应用运行依赖**，不捆绑。 |
| 字体 / 应用标识 | 系统字体仅本机使用；当前 resources/icons/chosuta.png、chosuta.ico 来自项目维护者2026-10-13提供的标识 | 维护者明确要求用于软件，并授权本地裁剪/缩放集成；未另附第三方来源或独立许可文件，不宣称是先前的原创SVG | 只裁剪留白、等比缩放和格式编码；未新增外部图标库或字体。原始输入只读保留。 |
| 英文词表 resources/english.tsv | Chosuta 原创，562 个常用词条；可选 | 音节分界（非声学边界） | GPL-3.0-or-later；有限词汇覆盖，保留明确未知状态 | 随资源嵌入应用；未取自 CMUdict，不宣称完整英文发音词典。 |
| 日文可选读音表 resources/japanese.tsv / 中文词语表 resources/chinese.tsv | Chosuta 原创，192 条可选日文常见读音、50 条中文词语读音；与英文合计 13,796 字节 | GPL-3.0-or-later；有限估计覆盖，日文汉字自动估计默认关闭 | 本次用户允许原创轻量词典；没有复制 CMUdict/UniDic/CC-CEDICT/JMdict 或用户 SV 软件字典。三表由资源嵌入；自定义词条由用户提供，不能推定其再分发许可。 |
| 用户 PNG / 自定义背景图片 / 音频 / svproject / maca_tachie | 用户本地提供 | 不推定可再分发；只读输入 | 不纳入公开夹具、源码包、应用包或上传。用户自行导出的作品也不会被上传。 |

0.4.0 字幕使用已有 Qt Gui 的 QTextLayout/QPainter、QFont/QFontInfo 与 Widgets 的字体/颜色选择器，共享 Scene 合成；没有集成 libass、复制上游实现或增加运行依赖。字体仅从用户系统读取并保存字体族，不下载、嵌入或分发字体文件；缺字体使用 Qt 回退并诊断。新增字幕/交互/测试代码为 Chosuta 自有 GPL-3.0-or-later 代码，演示文字及几何图像为自造夹具。

0.4.1 继续复用现有 Qt Widgets 的单个 QPlainTextEdit 和 QSplitter，提供块内文字光标/输入法与视图高度调整；辅音组预算及角色元数据为 Chosuta 原创规则代码。没有新增运行组件、移植代码、词典/字体数据或 DAW 引擎。交互研究参考 Ardour 官方播放控制说明，仅参考标尺点击定位方式，没有集成其源码。

0.6.0 的高级口形条件匹配、姿态锚定、可配置辅音规则、规范名导入和有界图片缓存为 Chosuta 原创 GPL-3.0-or-later 代码，仅复用既有 Qt Core/Gui/Widgets/Concurrent 与 C++20 标准库；没有新增数据包、模型、图片/字体或运行库。IPA 官方发音部位图及 Open University 复合拍说明只作为规则设计参考，未复制图表、源码或素材。音素标识来自既有已支持的解析接口，词典数据范围不变；回归图形/音频自行构造。

## 参考材料与外部词典

0.5.0 的可选波形显示与有界时间校正为 Chosuta 原创 GPL-3.0-or-later 代码，复用已有 Qt Core/Concurrent/Gui/Widgets、QProcess/QPainter 和独立 FFmpeg。FFmpeg 新用途为流式解码单声道 16 kHz PCM，波形/能量摘要与校正由自有 C++ 代码计算；不捆绑新库、模型、音素字典或声部分离工具，不新增 Python 产品运行依赖。FFmpeg/Qt 的现有许可与捆绑边界继续适用。MFA、WhisperX、wav2vec2、Demucs 与歌唱对齐论文只用于本地早期方案比较，未复制源码、下载模型或集成依赖；不把候选代码许可当作模型/数据再分发授权。测试音频与纯色 PNG 自行构造，用户工程/歌曲/立绘仍不分发。


- SVP 字段依据项目已有受控样本研究，未复制 Synthesizer V 的声库、程序或缓存；未集成 UtaFormatix、OpenJTalk、模型或声学代码。
- [CMUdict](https://github.com/cmusphinx/cmudict) 仅用于确认可选外部词典格式与许可候选，[上游 LICENSE](https://github.com/cmusphinx/cmudict/blob/master/LICENSE) 为保留版权/免责声明的两项再分发条件。当前完整数据下载因网络环境受阻，**未集成或分发其数据**，所以没有把词典候选写作已使用组件。加载用户另行提供的 CMU 格式文件仅作本地读取；若以后集成具体版本，需先记录提交、哈希及完整 LICENSE。

本次语言规则完善参考 [SV2 发音说明](https://sv2.docs.dreamtonics.com/en/phonemes)、[SV2 轨道/声部与语言](https://sv2.docs.dreamtonics.com/en/voice-setup) 和 [官方 Note API](https://resource.dreamtonics.com/scripting/Note.html)，用于确认逐音符语言/音素集覆盖、空显式音素及均分音节字段；仅参考资料，没有复制声库、词典或声学实现。用户最新指令已允许原创轻量词典；0.3.0 扩展自有英文词表并增加自有中日表，提供自定义词典与 3 MB 合计上限，没有新增运行依赖。CMUdict 与 [JMdict 官方项目](https://www.edrdg.org/jmdict/j_jmdict.html) / [许可](https://www.edrdg.org/edrdg/licence.html) 只作外部数据候选；JMdict 日英部分的现行 CC-BY-SA-4.0 条款已查询，但未取得具体版本/数据，也未集成或据此宣称依赖许可交付完成。

## 本地包与后续分发边界

当前 Linux CPack ZIP 只包含 Chosuta 可执行文件、自有许可和说明；相邻 source.zip 包含构建这些可执行文件的 Chosuta 对应源码和脚本。它是本机动态链接开发包，不能当作已完成的跨发行版便携包。未捆绑 Qt、ICU、FFmpeg 或系统库。

0.2.2 的 Arch 配方使用系统 makepkg（pacman 项目，GPL-2.0-or-later）与 fakeroot（GPL-3.0-or-later）作为构建工具；没有复制或捆绑其代码/二进制。DEB/RPM 入口调用目标系统的 dpkg-shlibdeps/rpmbuild，本机未安装或调用这些工具，也没有捆绑。各系统具体工具版本与许可随实际目标构建另行记录。Arch 应用包附原创 SVG/桌面项、自有源码对应归档和许可；Qt/ICU/FFmpeg 仍由系统包提供，FFmpeg 为可选运行依赖。

公开源码整理与 Arch 包修订 2 没有新增运行依赖。主许可证保持 GPL-3.0-or-later；依据实际组件与分发内容，未发现阻止当前源码/Arch 应用包按 GPLv3 分发的明确许可证冲突。具体许可、ICU 附带条款、FFmpeg 构建边界和未来捆绑义务见 [兼容性核查](docs/license-review.zh-CN.md)。

以后若捆绑运行库、平台插件、媒体后端或 FFmpeg，必须按实际二进制来源/构建选项逐项补齐上游版权与 NOTICE、具体版本对应源码及 GPL/LGPL 的适用义务，保留用户替换/重链接库的能力；不能只附应用源码代替捆绑依赖的对应源码。不使用 enable-nonfree 的 FFmpeg 组合。Windows DLL 包与 Linux 便携基线尚待实际验证，当前没有公开发布。


## 0.2.1 Windows 本地部署入口

没有新增第三方代码、词典、字体或素材。Qt 入口/Windows 子系统、UTF-8 和 ICU i18n/uc/data 继续使用上表的已有组件。当前 Linux 宿主没有 MSVC/MinGW Windows SDK、Windows Qt/ICU，因此没有实际复制或分发 Windows 第三方二进制。

`scripts/build-windows.ps1` 在用户的 Windows 机器上，使用其明确指定的现有 Qt/ICU SDK 部署运行库和 Qt 插件；FFmpeg/ffprobe 只有传入 `-FfmpegDir` 或显式 `-DownloadFfmpeg` 才复制。Qt Multimedia 的 FFmpeg 后端 DLL（若所选 SDK 提供）是 Qt 部署的一部分，与独立 ffmpeg.exe 分开记录。脚本记录真实 Qt/ICU/编译器版本、全部运行文件 SHA-256，收集 SDK 中已有的许可证，并检查所选独立 FFmpeg 不含 enable-nonfree。MSVC 使用已安装的官方 Visual C++ x64 Redistributable，不从编译器目录复制未核来源的 CRT DLL；MinGW 运行库由所选 Qt 的 windeployqt 收集。

Windows 构建依赖的具体许可证、附带媒体库和源码义务以其实际 SDK/二进制为准，不能套用本机 Linux 版本表。MSVC 工具与 Visual C++ Redistributable 使用 Microsoft 的相应许可；当前仅提供调用脚本，没有下载、捆绑或安装它们。Windows 本地构建目录会含运行库，不能仅凭自动收集的许可文件就宣称已完成公开分发审查。源码 ZIP 仅含 Chosuta 自有源码、原有许可文本与原创夹具，没有第三方运行二进制或用户材料。


0.6.1 的乐理度数、调式拼写及同音反复规则为 Chosuta 原创 C++ 代码，无新增库/数据包。仅参考 [music21 官方音程说明](https://music21.org/music21docs/usersGuide/usersGuide_18_intervals.html) 和 [官方音阶文档](https://music21.org/music21docs/moduleReference/moduleScale.html)核对字母度数与半音距离、教会调式概念；未复制其实现、安装或集成 music21。

### 0.8.2 外部工具隔离与 no-ffmpeg

本次新增配置／检测／双版本部署为 Chosuta 原创代码，无新增库或模型，未下载或捆绑新的第三方二进制。Windows 脚本允许用户提供已有 BtbN GPL shared（或其他满足功能／许可要求的构建），不自行裁剪；两个工具和供应商 bin 中 DLL 放在 `bin/tools/ffmpeg/`，Qt 媒体 DLL 仍由 windeployqt 独立部署。两个工具都检查 nonfree 标记；构建信息、文件大小和当地可得许可随附带版记录。此检查和收集不是对供应商全部传递依赖及对应源码义务的自动审计。

no-ffmpeg 版不复制该外部工具目录或其许可附件，但仍捆绑所选 Qt／ICU 及 Qt 媒体后端，需要履行这些实际组件的义务。BtbN 构建脚本的许可证不能替代其生成的 FFmpeg／编解码库许可证。当前未生成 Windows 分发包，具体第三方版本和来源以实际部署清单及发布时的对应源码材料为准。

### 2026-10-13 软件标识与根目录启动入口

本次图标以维护者提供的原图为唯一像素来源，采用本地工具裁剪留白、白底居中、等比缩放及PNG/ICO编码；保留原有两组笔画和深蓝配色。生成服务曾给出改变笔画的草稿，未采用，也未放入任何源码/应用资源。PNG与多尺寸ICO的字节哈希由公开源码审计脚本逐项固定，其他二进制文件不因此获准进入公开清单。旧原创SVG已从当前根源码移除，历史交付未替换。

预处理使用本机已有Pillow 12.3.0，安装元数据标明MIT-CMU；它仅用于这次资源制作，不移植/捆绑其代码，普通构建和应用运行均不依赖Pillow。Windows资源编译属于已有目标工具链；Linux下使用已有LLVM工具检查资源，仅作开发验证，无新增运行组件或依赖下载。Windows目录调整保留Qt/ICU/媒体依赖的既有许可义务；当前外部工具位置为部署根目录的`tools/ffmpeg/`，历史0.8.2说明中的bin布局不回写。

### 2026-10-14 显式 BtbN 下载与缓存

PowerShell 下载代码为 Chosuta 原创 GPL-3.0-or-later 代码，没有复制 BtbN 的构建脚本；仅通过其[官方发布](https://github.com/BtbN/FFmpeg-Builds/releases)取得用户选择的 win64 GPL shared 二进制。缓存记录发布标签、资产名、官方 URL、上游 SHA-256 和原许可；新下载 ZIP 解包后删除；部署复制上游 LICENSE.txt/可用许可证目录以及来源和校验清单。两个媒体工具的 nonfree 配置拒绝规则保持。

FFmpeg 及捆绑库的实际许可和对应源码义务仍需按所选构建核查；BtbN 构建仓库的许可、GitHub 自动生成的该仓库源码 ZIP、SHA-256 或单份 LICENSE 都不能代替完整的 FFmpeg/传递依赖对应源码。缓存和第三方二进制不进入 Chosuta 公开源码。本次环境没有 PowerShell/Windows，未下载该二进制、验证其运行或生成分发包；此处说明取得和记录机制，不宣称已完成具体 Windows 包的许可审核。
