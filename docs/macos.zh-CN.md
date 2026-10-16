# macOS 本地构建（实验性）

提供本机构建入口，尚未在 macOS 实机编译或运行验证，不作为正式支持平台或可分发成品的保证。当前版本1.0.0/schema14。

需要已经可用的 Xcode Command Line Tools（clang/clang++、macOS SDK、xcrun）、CMake ≥3.24、Ninja，以及架构匹配的 Qt ≥6.4 动态 SDK 和 ICU ≥67 开发库。Qt 需包含 Core、Gui、Widgets、Concurrent、Multimedia；普通构建不需要 Qt Test。脚本不会安装这些工具或修改系统配置。

在源码目录运行：

```sh
bash scripts/build-macos.sh \
  --qt-prefix '<Qt macOS SDK 前缀>' \
  --icu-prefix '<ICU 前缀>'
```

默认离线，使用已有 LibYAML、传统 MiniZip/zlib 开发库。若未装这两项，可使用已有源码，或显式获取项目固定版本的源码：

```sh
bash scripts/build-macos.sh \
  --qt-prefix '<Qt macOS SDK 前缀>' \
  --icu-prefix '<ICU 前缀>' \
  --libyaml-source-dir '<完整 LibYAML 0.2.5 源码目录>' \
  --zlib-source-dir '<完整 zlib 1.3.2 源码目录，含 contrib/minizip>'

# 或用 --download-dependencies 替代两个源码目录参数。
```

下载参数只允许 CMake 获取未指定本地源码的固定 LibYAML/zlib 归档，校验 SHA-256；不下载 Qt、ICU、编译器、模型或 FFmpeg。源码路线使用当前 clang 静态编译，并随安装保留实际依赖源码和许可。完整版本/许可见[第三方清单](../THIRD_PARTY_NOTICES.md)。

脚本兼容系统 Bash 3.2，可选参数：

| 参数 | 用途 |
| --- | --- |
| `--build-dir` | 默认 `build-macos`，可复用同一环境的构建树 |
| `--stage-dir` | 默认 `out/macos/Chosuta`，必须不存在或为空，不清理已有产物 |
| `--jobs` | 并行任务数，默认4 |
| `--arch arm64` / `--arch x86_64` | 可选单架构；Qt、ICU及系统依赖必须匹配，不自动生成通用二进制 |
| `--help` | 查看完整参数 |

构建/安装路径按源码根目录解析；SDK及离线源码参数按调用目录解析。路径可包含空格与Unicode。配置使用 Ninja/Release、`BUILD_TESTING=OFF`，通过 xcrun 选择现有编译器和 SDK；编译失败不继续安装，不自动运行应用或测试。

产物：

```text
out/macos/Chosuta/
  chosuta.app                       # GUI，可用 open 启动
  bin/chosuta-cli                   # 命令行
  share/chosuta/                    # 许可、操作文档及可选静态依赖源码
  build-info.json
  RUN.txt
```

例如手动运行 `open out/macos/Chosuta/chosuta.app`。GUI使用CMake的macOS bundle布局；实现依据见[Qt官方macOS部署说明](https://doc.qt.io/qt-6/macos-deployment.html)。

这是依赖本机 SDK 的构建目录：脚本保留 Qt/ICU 及其他已链接依赖的安装搜索路径，不复制这些运行库，不调用 macdeployqt，不签名或公证。移动到另一台电脑未必可用，不能将该目录直接称为独立发行包。可在本机用 `otool -L out/macos/Chosuta/chosuta.app/Contents/MacOS/chosuta` 查看实际链接库；跨机器部署须另行验证。

不附带 FFmpeg/ffprobe；需要导出和波形时，在应用导出设置中指定已有工具。Finder启动的进程不一定继承终端PATH，明确选择工具路径更直接。无音频/编码工具仍可导入、预览、编辑与保存。没有新增产品依赖或字体/图像处理工具。
