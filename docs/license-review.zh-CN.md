# 当前组件与分发许可说明

当前0.14.0/schema14。具体来源、版权和保留文本见[THIRD_PARTY_NOTICES](../THIRD_PARTY_NOTICES.md)；本页说明当前集成及交付边界，不将未审查的未来二进制组合标为已核查。

| 对象 | 当前使用与许可 |
| --- | --- |
| Chosuta代码、文档、原创三语词表和测试夹具 | GPL-3.0-or-later；未集成未授权声库、词典、字体或用户媒体 |
| Qt | 最低6.4；当前Linux构建6.12.0，按LGPLv3选项动态链接Core/Gui/Widgets/Concurrent/Multimedia及传递依赖。Linux包不捆绑Qt；完整GPL/LGPL文本位于resources/licenses |
| ICU4C | 当前Linux构建78.3，最低67；链接i18n/uc/data，保留完整实际LICENSE及其附带条款。Linux包不捆绑ICU |
| LibYAML | MIT，当前系统0.2.5；完整版权/许可已保留。Linux动态链接系统库；显式源码路线静态编译并安装实际源码/许可，不捆绑声库或转换器 |
| FFmpeg/ffprobe | 可选独立进程；许可取决于实际构建及依赖，不使用enable-nonfree构建作为分发依赖。Linux交付不捆绑 |
| GCC/libstdc++ | Linux构建使用GCC 16.2.1；运行库适用相应Runtime Library Exception及实际条款 |
| CMake/Ninja/Python/makepkg/fakeroot、Qt Test/moc/rcc | 构建或验证工具，非应用新增运行依赖；不捆绑其二进制 |
| 软件标识 | 项目维护者提供并授权裁剪/缩放集成，实际PNG/ICO及来源见第三方清单 |
| UtaFormatix、OpenUtau、DiffSinger、VOICEVOX参考 | 本地格式参考；当前未移植其转换器/引擎，不将其代码许可扩展到歌曲、声库或音频 |

当前Linux ZIP与Arch安装包仅包含应用、说明及许可，使用系统Qt/ICU；提供对应源码和构建材料。Windows脚本可复制用户选择的SDK/媒体组件，其实际版本、版权/NOTICE及GPL/LGPL对应源码义务须按具体分发内容落实。自动收集许可证或一次构建成功不等于完成所有依赖分发义务。

原创三表合计13,796字节；自定义词条由用户提供，不推定可公开。没有集成完整CMUdict、UniDic、CC-CEDICT或JMdict。用户歌曲、立绘和音频只作本地输入，不进入源码、夹具或应用包。

参考：[Qt许可](https://doc.qt.io/qt-6/licensing.html)、[Unicode许可](https://unicode.org/faq/unicode_license.html)、[FFmpeg许可](https://ffmpeg.org/legal.html)、[GCC运行库许可](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html)。

0.14.0的MusicXML导入器、有理数时序及原创谱面为Chosuta自有实现，复用已有Qt，不增加第三方运行组件。W3C规格和已有UtaFormatix格式资料仅供字段参照；本地第三方空白模板没有加入公开夹具，未移植其源码或运行时。
