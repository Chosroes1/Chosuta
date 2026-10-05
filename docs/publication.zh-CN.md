# 源码与 Release 文件整理

此文档说明如何生成可公开上传的源码目录；脚本只整理本地文件，不提交 Git、不登录或上传 GitHub。源码和二进制分开：源码目录整体作为仓库内容，安装包与构建时的对应源码 ZIP 作为 Release 附件。

## README 编辑分工

README.md 主体为中文，链接到 README.ja.md 和 README.en.md。作者填写项目的一句话简介及 1、2、5、9、10；系统要求、下载/源码安装、实现原理、结构、组件和 GPLv3 已按当前实现填写。英文/日文使用相同章节分工。作者补写中文后，应同步另两种语言或明确其更新状态。

当前代码版本 0.2.2，Arch 包修订 2。Windows 原生构建、DEB/RPM 运行和 AUR 公开条目没有验证/发布，不应在 README 或 Release 中写成已完成。

## 整理源码

在源码根目录执行，输出目录必须尚不存在：

```sh
python3 scripts/prepare-github.py out/github/Chosuta --report out/github/source-audit.json
```

上传的是 `out/github/Chosuta/` 中的内容，包括隐藏文件 `.gitignore`，不是整个开发工作区，也不包括旁边的核验报告。共享白名单 `scripts/public_source.py` 同时用于源码 ZIP：包含应用、CMake、必要脚本、三语 README、公开文档、原创夹具与许可；不遍历用户工程/素材、构建和凭据目录。

私有样本回归保留在本地 `tests/private_samples.inc`，不纳入公开文件清单。没有该文件时，测试仅跳过这一项，其他合成测试照常执行。不公开原始用户文件，也不公开其中的歌曲文件名或哈希。

敏感信息检查覆盖私钥标记、常见服务令牌、凭据赋值、带凭据 URL、个人目录路径和高熵字符串，并检查不允许的目录/文件及符号链接；报告仅输出规则和位置，不输出疑似秘密内容。上传前编辑过任何文件，重新检查：

```sh
python3 scripts/audit-public-source.py out/github/Chosuta --report out/github/source-audit-after-edit.json
```

检查为本地规则扫描及文件审查，不提供任意未来编辑绝不会泄密的保证。当前整理目录经核验没有发现凭据，也没有打包敏感配置或私有材料。`.gitignore` 是后续使用的防护，不代替实际上传目录的检查。

## 二进制与对应源码

```sh
python3 scripts/package-arch.py out/release/arch
```

脚本产生 `chosuta-0.2.2-2-x86_64.pkg.tar.zst`、`Chosuta-0.2.2-source.zip`、带实际 SHA-256 的 PKGBUILD 和 `.SRCINFO`。把前两个文件一起作为 Release 附件，可保留二进制的确切对应源码。脚本不安装工具或应用，输出目录存在即拒绝。

makepkg 默认的 `.BUILDINFO` 会记录开发者构建路径和本机全部已安装包。公开 Release 使用另存副本，移除这份本机信息并同步更新 `.MTREE`，保留应用及 `.PKGINFO` 不变；原始构建信息留在本地。不要把原始构建目录整体作为 Release 上传。

```sh
python3 scripts/sanitize-arch-package.py out/release/arch/chosuta-0.2.2-2-x86_64.pkg.tar.zst out/release/assets/chosuta-0.2.2-2-x86_64.pkg.tar.zst
```

这个副本不提供本机完整环境的 `.BUILDINFO` 复现记录；对应源码、配方和实际 CMake 组件版本另行保留，不伪造构建环境。它是标准 pacman 应用安装包，不是已签名或已经上传的 AUR 条目。

所谓“AUR 二进制包”在这里是可由 pacman 安装的 Arch 二进制包；AUR 仓库实际存放构建配方。当前配方使用同目录源码 ZIP。发布真实项目地址及源码下载地址之后，维护者再更新 `url`、`source`、校验和与维护者信息，重生成 `.SRCINFO` 并验证。当前没有虚构 GitHub 地址、添加凭据或提交 AUR。

仓库源码通过 `.gitignore` 排除 build/out/stage 和安装包。公开源码独立验证时请把构建目录放在源码目录之外，以保留上传目录的整洁。GPLv3 及依赖核查见 [许可证说明](license-review.zh-CN.md)。
