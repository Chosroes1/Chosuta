# Chosuta 第三方组件与素材记录

更新时间：2026-10-07。Chosuta 自有代码、中英日轻量读音表、原创几何素材生成器和合成测试夹具采用 **GPL-3.0-or-later**；主许可见 [LICENSE](LICENSE)。除本文件记录的许可文本外，没有移植或复制第三方源码。下表区分运行时组件、构建工具和参考材料。

| 组件 | 当前实际版本 / 来源 | 许可证与用途 | 修改与分发状态 |
| --- | --- | --- | --- |
| Qt Core / Gui / Widgets / Concurrent / Multimedia；传递 Qt Network | 本机 Qt 6.11.2；构建最低 Qt 6.4；[Qt 源码](https://code.qt.io/cgit/qt/) | 本项目按 LGPL-3.0-only 选项动态链接；JSON、Unicode、图像、GUI、任务及可选播放。[官方许可说明](https://doc.qt.io/qt-6.11/licensing.html) | 未修改 Qt。当前本地应用包不包含 Qt 库或平台/媒体插件；对应 Qt 包由系统提供。完整 GPL/LGPL 文本已保留于 resources/licenses。 |
| Qt Test、moc、rcc | 同上；仅开发/测试 | Qt Test 属于 Qt Base；工具的 GPL-3.0 与 Qt GPL exception 选项见 [工具许可](https://doc.qt.io/qt-6.11/licensing.html)。测试/元对象/资源编译 | 不包含在应用运行包；生成的元对象/资源代码由应用构建产生。 |
| ICU4C / ICU 数据 | 本机 78.3；最低 67；[上游](https://github.com/unicode-org/icu)、[变换文档](https://unicode-org.github.io/icu/userguide/transforms/general/) | Unicode-3.0 及 ICU LICENSE 所列附带条款（含历史 ICU、BSD、NAIST-2003 等）；直接链接 i18n/uc/data。Han-Latin 默认汉字读音与音调规范化 | 未修改、未复制字典数据。当前包不捆绑 ICU 动态库；本机完整 ICU LICENSE 原样保留为 resources/licenses/ICU.txt。默认读音不承诺上下文消歧。 |
| FFmpeg / ffprobe | 本机 9.0.2；[上游源码](https://ffmpeg.org/download.html)、[许可与构建组合](https://ffmpeg.org/legal.html) | 独立外部进程。实际本机构建 `-L` 声明 GPL-3.0-or-later，配置含 enable-gpl、enable-version3、libx264/libvpx/libopus，不含 enable-nonfree。编码 / 可选音频元数据探测 / 波形 PCM 解码 / 测试探测 | 未修改、不捆绑。程序检查具体编码器而不根据扩展名宣称支持。Qt Multimedia 自身的 FFmpeg 后端属于实际 Qt 部署依赖，也需在以后捆绑时核查。 |
| GCC 16.2.1 | 系统开发工具；[GCC](https://gcc.gnu.org/) | GPL-3.0-or-later，运行时按 GCC Runtime Library Exception 3.1 等实际条款；C++20 编译 | 不捆绑编译器；本机动态 libstdc++ 属于系统运行依赖。 |
| CMake 4.4.4 | 系统开发工具；[CMake](https://cmake.org/) | BSD-3-Clause；构建/安装/CPack | 不捆绑，不下载组件。 |
| Ninja 1.13.2 | 系统开发工具；[Ninja](https://github.com/ninja-build/ninja) | Apache-2.0；构建 | 不捆绑。 |
| Python 3 | 系统开发工具；[Python](https://www.python.org/) | PSF-2.0；打包辅助 / 构造开发夹具 | **不是应用运行依赖**，不捆绑。 |
| 字体 / 图标 | 系统已安装字体（字幕可选）；文本及代码绘制图形；resources/icons/chosuta.svg 为 Chosuta 原创矢量图标 | 原创 SVG 为 GPL-3.0-or-later；没有复制、嵌入或分发第三方字体或图标 | 不把用户系统字体许可假定为允许捆绑。 |
| 英文词表 resources/english.tsv | Chosuta 原创，562 个常用词条；可选 | 音节分界（非声学边界） | GPL-3.0-or-later；有限词汇覆盖，保留明确未知状态 | 随资源嵌入应用；未取自 CMUdict，不宣称完整英文发音词典。 |
| 日文可选读音表 resources/japanese.tsv / 中文词语表 resources/chinese.tsv | Chosuta 原创，192 条可选日文常见读音、50 条中文词语读音；与英文合计 13,796 字节 | GPL-3.0-or-later；有限估计覆盖，日文汉字自动估计默认关闭 | 本次用户允许原创轻量词典；没有复制 CMUdict/UniDic/CC-CEDICT/JMdict 或用户 SV 软件字典。三表由资源嵌入；自定义词条由用户提供，不能推定其再分发许可。 |
| 用户 PNG / 自定义背景图片 / 音频 / svproject / maca_tachie | 用户本地提供 | 不推定可再分发；只读输入 | 不纳入公开夹具、源码包、应用包或上传。用户自行导出的作品也不会被上传。 |

0.4.0 字幕使用已有 Qt Gui 的 QTextLayout/QPainter、QFont/QFontInfo 与 Widgets 的字体/颜色选择器，共享 Scene 合成；没有集成 libass、复制上游实现或增加运行依赖。字体仅从用户系统读取并保存字体族，不下载、嵌入或分发字体文件；缺字体使用 Qt 回退并诊断。新增字幕/交互/测试代码为 Chosuta 自有 GPL-3.0-or-later 代码，演示文字及几何图像为自造夹具。

0.4.1 继续复用现有 Qt Widgets 的单个 QPlainTextEdit 和 QSplitter，提供块内文字光标/输入法与视图高度调整；辅音组预算及角色元数据为 Chosuta 原创规则代码。没有新增运行组件、移植代码、词典/字体数据或 DAW 引擎。交互研究参考 Ardour 官方播放控制说明，仅参考标尺点击定位方式，没有集成其源码。

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

`scripts/build-windows.ps1` 在用户的 Windows 机器上，使用其明确指定的现有 Qt/ICU SDK 部署运行库和 Qt 插件；FFmpeg/ffprobe 只有传入 `-FfmpegDir` 才复制。Qt Multimedia 的 FFmpeg 后端 DLL（若所选 SDK 提供）是 Qt 部署的一部分，与独立 ffmpeg.exe 分开记录。脚本记录真实 Qt/ICU/编译器版本、全部运行文件 SHA-256，收集 SDK 中已有的许可证，并检查所选独立 FFmpeg 不含 enable-nonfree。MSVC 使用已安装的官方 Visual C++ x64 Redistributable，不从编译器目录复制未核来源的 CRT DLL；MinGW 运行库由所选 Qt 的 windeployqt 收集。

Windows 构建依赖的具体许可证、附带媒体库和源码义务以其实际 SDK/二进制为准，不能套用本机 Linux 版本表。MSVC 工具与 Visual C++ Redistributable 使用 Microsoft 的相应许可；当前仅提供调用脚本，没有下载、捆绑或安装它们。Windows 本地构建目录会含运行库，不能仅凭自动收集的许可文件就宣称已完成公开分发审查。源码 ZIP 仅含 Chosuta 自有源码、原有许可文本与原创夹具，没有第三方运行二进制或用户材料。
