# GPLv3 兼容性核查

核查日期：2026-10-05。范围为已交付的 Chosuta 0.2.2 公开源码与仅含应用自身的 Arch Linux x86_64 包。主许可证保持 **GPL-3.0-or-later**，即 GNU GPL 第三版或更新版本；不是 GPL-3.0-only。依据实际源码、动态链接方式、系统包许可证及上游许可文本，**未发现阻止当前这两种交付按 GPLv3 分发的明确许可证冲突**。

## 当前组件

| 对象 | 核查结果 |
| --- | --- |
| Chosuta 自有代码、文档、英文词表、SVG、合成夹具 | GPL-3.0-or-later；没有移植第三方代码或集成未授权声库/词典 |
| Qt 6.11.2 Core/Gui/Widgets/Concurrent/Multimedia/Network | 使用 LGPL-3.0-only 选项动态链接。Qt 官方允许遵守 LGPLv3 或 GPLv3 的应用使用。当前 Arch 包不复制 Qt 库或插件；未采用其商业许可 |
| ICU 78.3 i18n/uc/data | 主许可 Unicode-3.0 为宽松许可，保留版权/许可是主要再分发条件。完整实际 LICENSE 已保留，包括历史 ICU、BSD、NAIST-2003 等附带条款，未简化为仅有主许可。使用 Han-Latin，不复制词典；当前包不捆绑 ICU 库或数据 |
| FFmpeg/ffprobe 9.0.2 | 独立进程。本机实际构建为 GPL-3.0-or-later，含 enable-gpl/enable-version3，不含 enable-nonfree，未捆绑。配置/外部库会改变许可，不能套用于任意 EXE/DLL |
| GCC/libstdc++ | 编译器 GPL，运行库适用 GCC Runtime Library Exception 3.1。标准动态链接没有发现与 GPLv3 的冲突。Windows 工具链与部署库尚未实际核查 |
| CMake/Ninja/Python/makepkg/fakeroot、Qt Test/moc/rcc | 构建工具与运行代码分开记录，没有移植或捆绑。Apache-2.0 可进入 GPLv3 作品；调用 Ninja 本身也不是将其代码并入应用 |

ICU 的数据附带条款不能忽略，当前未重新包装或捆绑这些数据。以后若制作含 ICU/Qt/媒体后端的便携包，应按对应版本逐项列出数据、传递依赖并落实条款；不能仅依据主许可摘要宣布整个便携包通过审查。这里没有将未来未知构建标为兼容，也没有静默忽略已证明不兼容的依赖。

## 发布当前二进制

- 提供与二进制对应的 Chosuta 源码、CMake/配方、LICENSE 和 NOTICE。建议在同一 Release 附构建时源码 ZIP；作者之后修改 README 不影响保留确切对应源码。
- 当前 Arch 包不含第三方库。若以后捆绑库/媒体后端/FFmpeg，则还须提供适用的依赖对应源码、版权、许可、构建材料以及替换/重链接能力；Chosuta 源码不能替代这些义务。
- 不使用 enable-nonfree 的 FFmpeg 作为分发依赖，不加入未授权用户歌曲、立绘、声库、字体或外部词典。当前没有集成完整 CMUdict。

## 核查依据

- [Qt 6.11 官方许可](https://doc.qt.io/qt-6.11/licensing.html)：开源选项、第三方代码及工具例外。
- [Unicode 官方 FAQ](https://unicode.org/faq/unicode_license.html)与[保留的完整 ICU 许可](../resources/licenses/ICU.txt)；[ICU 上游 LICENSE](https://github.com/unicode-org/icu/blob/main/LICENSE)。主分支不能替代实际版本副本。
- [FFmpeg 官方说明](https://ffmpeg.org/legal.html)与[上游 LICENSE.md](https://github.com/FFmpeg/FFmpeg/blob/master/LICENSE.md)：配置改变许可及 nonfree 分发限制。
- [GCC libstdc++ 许可](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html)：运行库例外。
- [Apache 官方 GPLv3 兼容性说明](https://www.apache.org/licenses/GPL-compatibility.html)。

实际版本与来源见 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)；不同构建的分发义务须重新按实际组成核查。

## 0.3.0 轻量读音数据补充（2026-10-06）

本轮用户允许原创轻量词典。新增/扩充的三语 TSV 均为 Chosuta 原创，GPL-3.0-or-later，合计 13,796 字节；未复制第三方词典、声库或用户截图中的整套字典。自定义词条和 JSON 由用户提供，其许可不能推定为 GPL，当前只在用户应用配置/工程中使用，不自动公开。没有新增运行依赖；此前 Qt/ICU/FFmpeg 具体构建和未来捆绑义务保持。0.3.0 当前为工作区未发布构建，本轮没有重新整理 GitHub、源码 ZIP 或 Release。
