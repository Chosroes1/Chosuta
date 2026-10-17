# Chosuta

Chosuta是一个导入虚拟歌姬音声合成软件工程与口型立绘后直接输出歌唱视频的软件，当前已支持并验证svp（Synthesizer V）。另有ust（UTAU）、ustx（OpenUtau）、vsq、vsqx、vpr（VOCALOID）、css（CeVIO）、vvproj（VOICEVOX）、ds（DiffSinger）、musicxml、xml、mxl支持，但均未经过真实工程人工测试。


## 1. 功能

根据导入的svp等工程文件与对应不同口型的立绘，生成视频文件。  
在编辑页面中，用户可根据具体情况手动编辑，调整动画的时间、立绘差分。另外，时间轴也可以以节拍形式表现。  
当前版本支持生成纯色背景动画并在剪辑软件中编辑，与自行导入背景、更改人物位置直接生成简单PV两种用法。  
0.6.4为旧方向归档版本，在高级设置里增加了上行/下行、级进/大跳/小跳、强拍/弱排等差分，排列组合下来有一百多个。到最后十分麻烦且效果不佳，目前在考虑别的开放方向，仅在release放出归档，不做分支。    
0.8.2为自动差分做了新方案。  
1.0.0增加了更多软件的支持，但并未人工测试。

## 2. 当前版本

当前release版本为0.10.0,设备支持等问题仍在测试中。

## 3. 系统要求

| 使用环境 | 要求 |
| --- | --- |
| Linux | 与包匹配的发行版和架构；本次二进制面向 Arch Linux x86_64。ZIP动态链接系统库，不是跨发行版自包含包 |
| Windows | 构建脚本面向 Windows 10/11 x64；当前1.0.0原生编译/部署尚待验证，不能以Linux结果代替 |
| macOS | 提供实验性本机构建脚本；需要现有Xcode命令行工具及匹配架构的SDK，尚未实机验证 |
| 应用运行库 | Qt ≥6.4（Core、Gui、Widgets、Concurrent、Multimedia及平台/媒体插件）、ICU ≥67；LibYAML、传统MiniZip/zlib按平台采用系统动态库或静态编入 |
| 视频导出/波形 | 可选独立FFmpeg；MP4需libx264/AAC，WebM需libvpx-vp9/libopus，透明MOV需qtrle |
| 音频时长探测 | 可选ffprobe；缺少时仍可附加音频并手动设置时长 |

音频、声库和Synthesizer V本体不是导入、生成口形、预览、编辑或保存的前提。FFmpeg路径可在应用中指定；Python/PowerShell不是应用运行依赖。

**从源码构建还需开发环境：**

| 平台 | 必须提前准备 | 脚本可获取的内容 |
| --- | --- | --- |
| Linux | C++20编译器、CMake ≥3.24、Ninja、Qt/ICU/LibYAML及传统MiniZip/zlib开发包 | 默认复用系统库，不自动安装系统包 |
| Windows | PowerShell ≥5.1、CMake ≥3.24、Ninja、匹配的MSVC或x86_64 MinGW编译器、Qt/ICU动态SDK | `-DownloadDependencies`获取LibYAML/zlib源码；`-DownloadFfmpeg`另行获取可选媒体工具 |
| macOS（未实机验证） | Xcode命令行工具、CMake ≥3.24、Ninja、匹配的Qt/ICU SDK；默认还需系统LibYAML/MiniZip/zlib开发库 | 可指定离线源码，或显式`--download-dependencies`获取LibYAML/zlib源码 |

Windows的Qt、ICU与编译器须架构/ABI匹配；MSVC在x64开发者终端执行，运行MSVC应用还需官方VC++ x64运行库。**Qt、ICU、CMake、Ninja和编译器不会由脚本自动安装。** Qt Test只在显式启用开发测试时需要。

## 4. 安装方法

### 直接下载

在本仓库的 **Releases** 页面选择与系统和架构匹配的附件；是否已经发布、验证和捆绑依赖，以该Release说明为准。GitHub自动生成的Source code归档是源码，需要编译。

- **Arch Linux**：安装`chosuta-<版本>-<修订>-x86_64.pkg.tar.zst`后运行`chosuta`或桌面菜单入口；系统包管理器提供运行库。
- **Linux ZIP**：完整解压后运行`bin/chosuta`，CLI为`bin/chosuta-cli`；需要匹配的系统ABI及Qt平台/媒体插件。
- **Windows**：如提供已验证的运行目录ZIP，完整解压并运行根目录`chosuta.exe`，保留DLL和插件。当前提供构建脚本，不代表已发布Windows成品。
- **其他Linux/macOS**：当前Arch二进制不适用；在目标系统从源码构建。DEB/RPM和macOS实机尚未验证。

AUR存放构建配方；当前没有公布AUR条目，不能直接给出yay/paru安装命令。详见[Linux打包说明](docs/packaging.zh-CN.md)。

### 从源码构建：Linux

在已有上述开发依赖的环境中，解压对应源码ZIP或取得本仓库源码，进入源码根目录：

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --parallel 4
./build/chosuta
```

需要安装到自己拥有的目录时：

```sh
cmake --install build --prefix "$PWD/stage"
./stage/bin/chosuta
```

默认不下载依赖、不构建/运行测试。Linux包由系统提供Qt/ICU/LibYAML/MiniZip/zlib，不捆绑这些库或FFmpeg；其他发行版应在目标环境重新构建。

### 从源码构建：Windows

先准备系统要求表中的工具和Qt/ICU SDK；MSVC使用x64 Native Tools / Developer PowerShell。以下命令显式开启两类下载：

```powershell
.\scripts\build-windows.ps1 `
  -QtPrefix '<Qt x64 SDK 前缀>' `
  -IcuPrefix '<ICU x64 SDK 前缀>' `
  -DownloadDependencies -DownloadFfmpeg
```

| 参数 | 行为 |
| --- | --- |
| `-DownloadDependencies` | 下载固定LibYAML 0.2.5和zlib 1.3.2（含MiniZip）源码并校验SHA-256，用现有C/C++工具链静态编译；完整实际源码与许可随部署保留 |
| `-DownloadFfmpeg` | 应用编译成功后，从BtbN官方发布取得win64 GPL shared的FFmpeg/ffprobe及DLL，校验并缓存；下次可离线复用 |
| `-LibYamlSourceDir`和`-ZlibSourceDir` | 指定两份完整本地源码，可替代`-DownloadDependencies`离线构建；指定的本地源码优先 |
| `-FfmpegDir` | 指定已有ffmpeg.exe、ffprobe.exe及DLL的bin目录；与`-DownloadFfmpeg`互斥 |
| `-Toolchain MinGW` | 使用与Qt/ICU匹配的x86_64 MinGW；默认是MSVC |
| `-BothVariants -CreateZip` | 一次编译生成附带工具和no-ffmpeg两版目录及ZIP；须同时指定媒体工具来源 |

**默认离线，自动下载必须显式加参数。** 不指定媒体工具参数时不下载、不附带FFmpeg，仍可编辑保存。no-ffmpeg版保留Qt媒体后端，并非移除所有媒体DLL。

输出默认为`out/windows/Chosuta/chosuta.exe`及`chosuta-cli.exe`；外部工具放在`tools/ffmpeg/`。脚本固定预检取得的CMake/Ninja/编译器，使用Ninja/Release、`BUILD_TESTING=OFF`，不安装SDK、不修改永久PATH、不运行CTest或自动启动应用。完成显示Built后可手动运行GUI；保留整个部署目录。部署目录必须为空，ZIP不得已存在，重跑可用`-StageDir 'out/windows/Chosuta-new'`。详见[Windows说明](docs/windows.zh-CN.md)。

### 从源码构建：macOS（未实机验证）

```sh
bash scripts/build-macos.sh --qt-prefix '<Qt macOS SDK 前缀>' --icu-prefix '<ICU 前缀>'
```

默认离线，可指定`--libyaml-source-dir`、`--zlib-source-dir`或显式`--download-dependencies`。生成`out/macos/Chosuta/chosuta.app`和`bin/chosuta-cli`，依赖本机SDK；不安装工具、不复制运行库、不下载FFmpeg或签名公证。macOS实机未验证，详见[macOS说明](docs/macos.zh-CN.md)及[统一构建说明](docs/building.zh-CN.md)。

## 5. 使用说明

可参考 [使用说明](docs/user-guide.zh-CN.md)。  
### 快速开始：  
点击左上角“文件”→“打开乐谱/工程”，导入工程，也可直接将工程拖入窗口。Chosuta将自动识别工程并生成时间轴。  
<img width="1280" height="930" alt="image" src="https://github.com/user-attachments/assets/0e13acc1-31c7-41af-a8e0-2174131c41f9" />  
<sub>示例歌曲：古今煌々//生々流転by 仕事してP。</sub>  
当工程存在多个旋律或和声轨道时，需要在轨道设置中设置选用的轨道与轨道优先级。  
将左上角编辑项切换到PNG差分，导入不同口型的差分。  
<img width="1923" height="1397" alt="image" src="https://github.com/user-attachments/assets/e9d76976-e94d-44a9-8988-8de95fbf1ab4" />  
<sub>示例立绘：[宮舞モカ立ち絵](https://commons.nicovideo.jp/works/im11390154)by かれい。</sub>  
点击播放或按空格，即可让你的歌姬开口。  
点击右上角“导出视频”，即可导出视频。  
### 其他功能  
#### 画布设置与立绘布局  
在编辑项中选择画布/背景，可调整画布大小、背景颜色、立绘大小、立绘位置等。立绘也可以直接拖动。  
<img width="780" height="797" alt="image" src="https://github.com/user-attachments/assets/9fc52b8d-6455-47a8-8c67-829c8969e7f8" />  
虽然本软件的一般用法是导出一个GB素材，而后在pr等剪辑软件中进行详细编辑。但如果你并不想制作复杂的PV，也可以导入自定义背景，直接生成视频。  
#### 添加音频与波形校正  
点击画布下的“附加音频”，或直接将音频文件拖入编辑窗口，即可导入音频文件。当音频文件长度大于工程文件长度时，会询问是否延长动画。但一定要保证音频与工程对齐。  
点击时间轴上方的“按波形校正时间”，即可根据音频波形校正音节时间，可以在一定程度上解决调声软件的弱起等修正造成的真实歌声与乐谱不匹配的问题。  
<img width="1911" height="477" alt="image" src="https://github.com/user-attachments/assets/562e6437-f97d-4e33-ac13-233bca0c2b14" />  
波形校正仅根据波形图推算时间，没有任何音频解析，因此仅供参考和辅助。由于音频可以更换，可以先导入纯人声，在校正后再导入有伴奏的音频。 
为了防止一步读错导致全工程歌词错位，增加了最大校正偏移的限制，可以自行修改。  
#### 编辑视图、运行控制与人工校正  
Chosuta有人工校正功能，可以设置以下内容，方便人工修改。  
**按节拍计时** 在时间轴上方可切换时间轴是按秒显示还是按节拍显示。由于BPM等要素是读取工程获得的，因此不设BPM修改接口。  
**停止时回到开始位置** 更符合daw和编辑器操作习惯的设置，在左上方“运行控制”中启用。  
**游标吸附** 时间轴上方可选择是否让游标吸附音节或节拍，方便编辑。  
**音节块设置** 直接点击时间轴上的音节块，即可设置所用差分、起止位置等，也可拆分或合并。鼠标直接拖动音节块或音节块边缘则可以直接用鼠标调整位置和长度。  
<img width="1923" height="1397" alt="image" src="https://github.com/user-attachments/assets/7cc489c2-5d5d-45a8-a25d-b1a09690697f" />  
#### 添加字幕  
勾选时间轴上方的“添加字幕”，即可增加一条字幕轴。点击字幕轴空白处，即可添加字幕块。字幕块可以用鼠标拖动或改变长度，也可双击点击字幕块进入编辑。  
<img width="1923" height="1397" alt="image" src="https://github.com/user-attachments/assets/cbb67c66-f3f3-4731-ad0b-e37195f70d4d" />  
在编辑中可以选择字幕的字体、颜色、大小、位置等。字幕也可以直接在画布中拖动改变位置。  
这并不是一个剪辑软件，字幕功能仅供辅助。如果要添加正经字幕或字幕特效，请导出后在有正经字幕功能的软件中编辑。  
#### 辅音规则
由于大多数合成软件中的元音与辅音的比例都在软件缓存里，不在工程文件，Chosuta无法读取元音与辅音的比例，因此采用了固定比例与设置时常上限的方法。可在规则中调整。当然也可以人工拖动。  
<img width="764" height="274" alt="image" src="https://github.com/user-attachments/assets/7e17986a-2bb4-41a2-a248-f91d01110f0b" />  
现实中，不同的辅音对应不同的口形。如/k/、/g/、/h/为张开、/b/、/p/、/m/为闭合、/f/、/v/为半闭。但把不同辅音对应的所有口型全部列出会造成需要导入的素材爆炸增长，所以Chosuta仅设置/b/、/p/、/m/闭合，其他辅音选用差分同元音的默认设置。这一默认设置可在PNG差分→高级差分设置→辅音规则中自定义。但不建议修改。  
<img width="1474" height="1127" alt="image" src="https://github.com/user-attachments/assets/d66b0c24-cb2c-4c88-aaf9-47e53a7732ec" />  
  
### 高级差分与自动演出
#### 方案更换
在废案版本0.6.4中，尝试了为演出增加上行/下行、级进/大跳/小跳、强拍/弱排等差分的功能，造成了所需差分指数级爆炸，且以单纯的旋律音程为判断指标难以准确识别演出的问题。该方案已作废，欢迎在release中下载尝试。  
当前所采取的高级演出方案是根据前后音符观察所得出的强调/收敛判定。当前算法还无法涵盖大多情况，因此自动演出功能还需要更多的乐曲素材来进一步完善，欢迎大家的意见。  
#### 高级差分启用及差分组设置  
在PNG差分最上方勾选启用高级差分，即可启用高级差分。  
进入高级差分设置→差分素材设置高级差分。默认差分组只有base一个，基础、强调、收敛三种形态均启用base差分组。可点击“添加组”添加差分组，而后为新添加的差分组导入立绘差分，将某个形态设定为某个差分组。 
<img width="1474" height="1127" alt="image" src="https://github.com/user-attachments/assets/fac226ee-f85c-4255-ae78-85ae12330ee6" />  
差分组设置后不自动保存，点击下方的“应用”才能将高级差分应用于演出。  
#### 自动演出设置  
不建议修改默认自动演出规则，但如果发现了有什么好用的新规则，一定要联系我（  
**调性辅助** 没有什么用的功能，不接入模型，纯靠算法来算哪个主音该收敛，哪个属音在高潮还是太勉强了。  
<img width="1415" height="312" alt="image" src="https://github.com/user-attachments/assets/7052aab5-729d-4331-87f0-565bab1acdfe" />  
**规则选用** 当前有这么几个规则，全是根据仕事してP的谱设计的，并不能涵盖所有乐曲。希望发现新规则的用户能够及时提出意见。  
<img width="465" height="219" alt="image" src="https://github.com/user-attachments/assets/d26187ac-6d80-4caa-89a3-bc99a406ede0" />
**详细阈值与音名设置** 这里用来调整规则的适用条件。如怎样算累计上行、几度算跳进、持续多少拍算累计。也可设置自定义规则。  
<img width="1474" height="1224" alt="image" src="https://github.com/user-attachments/assets/f427b03e-88b1-4aa1-beea-e120f1f134ce" />  
<img width="1415" height="656" alt="image" src="https://github.com/user-attachments/assets/b497136a-b162-4649-bb3f-88c37eb698dd" />  
#### 高级演出人工校正  
应用高级演出后，时间轴上会出现一条演出轴。只要点击演出轴上的演出块，就能人工校正演出了。包括持续时间、应用差分组等。  
  
## 6. 实现原理

本轮维护：无显式音素或人工读音时，空歌词默认使用 rest；已有工程保留保存的特殊项策略。时间轴支持中键拖动视野；缩放滑块保持可见游标（游标在屏外时保持视野中心），Ctrl＋滚轮保持鼠标所指时间，首尾按可滚动范围限制，不改变事件或播放位置。Linux构建、GUI工作流（40项）及启动冒烟通过；Windows实际交互尚未验证。应用版本仍为1.0.0/schema14。

Chosuta 使用 C++20 与 Qt 6 Widgets；核心模型独立于 GUI 控件。自动口形由工程文件中的乐谱、显式音素和歌词通过确定性规则生成，音频可用于试听、配音及用户主动触发的有界时间校正，不决定口形内容。

1. **导入乐谱**：0.10.0新增UST与VSQX源码接入，复用现有Qt/ICU，详见[格式范围](docs/user-guide.zh-CN.md)。UST处理CP932/UTF-8、逐音速度及休止，VSQX处理part与前置小节、原始resolution和显式音素。读取 SVP 实际引用的 mainGroup/mainRef 与 library 分组，展开引用实例并应用时间/音高偏移。保留整数音乐位置，SV 每四分音符为 705,600,000 blick；根据分段 BPM 换算秒数，拍号用于小节/拍显示。
2. **解析读音**：非空显式音素优先，否则解析歌词；逐音符语言覆盖优先于分组及轨道继承，兼容全角续音符号。日语默认使用假名/罗马音规则，汉字估计为默认关闭的可选项；中文使用常见词语读音、拼音及 ICU Han-Latin；英文使用原创有限词表。三语内置读音数据约 14 KB，高级设置支持词条编辑、JSON 导入导出和持久保存；内置、自定义及可选外部词典合计不超过 3 MB。未知或多音字读音可人工修正，不标为精确结果。
3. **生成事件**：按确定性分段规则分配口形和估计时序，处理多轨优先级、延音、呼吸、休止及辅音默认分类。尚未核实含义的 leftOffset 只保留并诊断，不伪造精确边界。
4. **保存编辑**：schema14新增sourceFormat，重开按来源类型读取内嵌原文；已有无该字段的schema14仍按SVP读取。原始来源、规则结果和人工覆盖分别保存。撤销/重做作用于工程操作，重新生成保留锁定修改；素材路径可相对工程保存并重定位。
5. **合成视频**：预览与导出共用 Scene，按每帧绝对时间求值，支持画布、背景图及立绘缩放/位置。通过程序路径和参数数组启动外部 FFmpeg 编码；无音频也能导出。

0.10.0已完成Linux构建及现有回归，5份UST与1份VSQ4完成CLI生成、保存和重开元数据对照；尚未完成Windows及原合成软件兼容验收，VSQ3未做原软件对照。

自动结果是动画时序估计，不复现 SV 私有声学模型。TimingRefiner 的提供者接口保留；0.5.0 新增无模型波形时间估计，不加载声库或神经网络。更多规则与局限见 [使用说明](docs/user-guide.zh-CN.md)。

可选字幕默认关闭，启用后支持多字幕轴、双击手动添加、区间拖动、文字及基础样式；歌词时间对齐可选且指定来源轨道，依据谱面音符，不识别音频或自动匹配句意。字幕独立于口形事件，重生成不覆盖它。立绘和字幕可在停止播放的预览中拖动布局/调整大小，保留数值精调；一次拖动一次撤销。工程只读写当前 schema 14，字幕通过共享 Scene 进入预览与视频；字体使用本地系统字体，跨机器可能发生替换。操作见[用户说明](docs/user-guide.zh-CN.md)。

0.4.1 工作区修订：轴内播放定位只通过上方时间标尺，贯穿红线保留；空白取消选择/文字焦点而不跳转。字幕单击选中/拖动、双击块内编辑，Delete/Backspace 按焦点操作对象或字符；行高和显示区域可拖动调整。新工程辅音组按比例并设默认 80 毫秒上限，尾辅音独立分配；上限 0 表示不设固定毫秒上限，锁定覆盖保留。只复用一个按需 Qt 编辑控件，没有新依赖。

0.5.0 工作区新增：附加音频后显示位于标尺下、音节行上的波形，拖动下边界只调显示高度。导入不会自动校正；点击“按波形校正时间”才运行，默认最大偏移 100 ms、最大时长变化 25%，可调整并随校正保存。明确能量边界在工程附近独立估计，不改口形/音素/段内比例、不累计偏移；歧义、连续音、人工覆盖与冲突保留工程结果。校正层可回退/撤销，保存重开与预览/导出共用；重新生成或输入变化清除/失效旧校正。混音可作波形参考，但不保证人声边界准确；不加入模型或声部分离。CLI 提供 `correct/revert`，详见操作说明。

工作区 **0.9.2**：保留默认简单模式，高级模式改用“差分组×口形”和独立演出区段。支持保持/展开/收敛/先展后收模板、人工保护与撤销、显式生成草稿及比较应用、选区重算、主音/调式分别选择及可编辑条件规则。整图直接切换，同音/短回绕保持，不逐音按方向换图；缺槽依次回基础同口形和全局图。口形编辑与固定整图分开，演出不清时间校正。差分组目录采用每组八槽；仅读写 schema 14，无旧模式迁移。没有新增运行依赖或模型。详见[操作说明](docs/user-guide.zh-CN.md)。

自动规划采用原谱背景、有限重复动机和到达音群；短音可共同支持强调，主音/长音/句尾不直接收敛。时长（音乐时值/秒）和距离（半音/度数）先选后填，只有所选依据生效。CLI可导出谱面诊断；不识别实际力度或情绪。

0.8.1 操作改进：高级设置“应用”直接更新自动演出并保留受保护人工段，可整体撤销；批量 PNG／文件夹导入可逐行或批量指定差分组、口形，支持任意文件名及拖入。运行控制独立列出，空格、画布按钮与菜单统一为播放／停止，按同一开关决定留在当前位置或回本次起播点；标尺游标可选择节拍／音节吸附。

媒体工具：导出设置新增本机默认路径、工程路径、浏览和后台检测；ffprobe 自动跟随所选 FFmpeg。Windows 可用同一份构建生成附带工具与 no-ffmpeg 两种包，外部工具独立放在 `tools/ffmpeg/`，保留 Qt 媒体依赖。详见 [Windows 构建说明](docs/windows.zh-CN.md)。当前未完成 Windows 原生打包验收。

schema14 不迁移旧工程；保留旧文件并用原版程序打开。共享来源路由与分段时间映射避免裁切被误算为拉伸，预览/时间轴复用当前求值结果。校正有效性使用谱面依据与音频路径、大小、修改时间；媒体路径直接使用，导出在实际帧读取素材。

人工口形拆分、移动、缩放和同源合并保留来源片段，保存重开与撤销后仍可用于演出时间映射。

0.9.2 将错误说明、演出依据和常用 Qt 控件统一按界面语言呈现，外部原始报错保留在技术详情。切换语言保留撤销历史；口形显示名与数据 ID 分离，复制组/批量导入保留 unknown 辅助图。schema14 保存结构化演出理由，不迁移旧工程。

Windows构建的部署目录根部直接放置 `chosuta.exe`，DLL/插件随目录保留。应用窗口与Windows EXE使用提供的软件标识，原图仅裁剪留白和等比缩放，未重绘；Linux安装对应PNG桌面图标。

## 7. 项目结构

```text
CMakeLists.txt              构建、测试、安装及 CPack 入口
src/core/                   时间与工程模型、外部工具定位
src/importers/              SVP / UST / VSQX 导入
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

| 组件 | 用途 | 获取/部署方式 |
| --- | --- | --- |
| Qt ≥6.4 Core/Gui/Widgets/Concurrent/Multimedia；传递Qt Network | 数据、图像、界面、后台任务、试听 | 开发环境提前准备；Arch使用系统动态库，Windows由匹配SDK的windeployqt部署Qt库和插件 |
| ICU ≥67 i18n/uc/data | Unicode处理和转写 | 提前准备开发库；Windows部署匹配的共享DLL |
| LibYAML ≥0.2.5 | USTX YAML解析 | Linux系统库；Windows固定0.2.5源码，离线指定或`-DownloadDependencies`获取后静态编译 |
| 传统MiniZip/zlib | VPR/MXL的ZIP与DEFLATE读取 | Linux系统库；Windows从完整zlib 1.3.2源码构建读取/解压部分，来源参数与LibYAML同上 |
| FFmpeg/ffprobe（可选） | 编码、时长探测、波形解码 | 独立进程，可选择已有路径；Windows显式`-DownloadFfmpeg`获取BtbN GPL shared，与解析依赖下载开关独立 |
| C++编译器、CMake、Ninja、moc/rcc | 构建和代码生成 | 提前准备，不是应用运行依赖；MSVC应用使用官方VC++ x64运行库 |
| Qt Test | 开发验证 | 仅`BUILD_TESTING=ON`时需要；普通构建和打包关闭测试 |
| Python 3、makepkg、fakeroot | 源码归档及本地Linux打包 | 仅开发辅助，不随应用捆绑 |

LibYAML采用MIT，MiniZip/zlib采用Zlib许可，ICU保留Unicode及附带许可，Qt按实际构建许可与部署内容处理。静态源码路线附完整实际来源树与许可；Windows自动收集许可文件不代替具体二进制的对应源码义务核查。FFmpeg构建不得带`--enable-nonfree`。具体来源与许可见[第三方清单](THIRD_PARTY_NOTICES.md)和[兼容性说明](docs/license-review.zh-CN.md)。

内置小型读音表及测试夹具为Chosuta原创；不捆绑完整CMUdict、声库、合成引擎、模型或第三方字体。用户图像/音频仍由用户提供。macOS实验脚本使用既有SDK动态库，不复制这些运行库。

## 9. AI 使用声明

本软件的架构、使用方法、功能由人类提供，代码由天才程序员ChatGPT开发，并有蓝色大肥鱼DeepSeek辅助，并通过人工审阅和验证。

## 10. BUG 报告

如有BUG报告、功能建议，请邮箱联系2441002630@qq.com。

## 11. 许可证

Copyright (C) 2026 Chosuta contributors。

除另有明确标注外，Chosuta 源码、文档及原创资源按 **GNU General Public License 第 3 版或任何更新版本（GPL-3.0-or-later）** 分发，不提供任何担保。完整文本见 [LICENSE](LICENSE)。第三方组件保留各自许可，不因被调用而改为 Chosuta 的许可。

依据当前组件、实际链接方式和分发内容进行核查，未发现阻止当前源码与 Arch 应用包按 GPLv3 分发的明确许可证冲突。Qt 的 LGPLv3 选项允许与 GPLv3 应用组合；ICU 主许可为宽松 Unicode 许可，附带条款全文保留；构建工具与应用运行代码分别记录。[Qt 官方许可](https://doc.qt.io/qt-6.11/licensing.html)、[Unicode 官方说明](https://unicode.org/faq/unicode_license.html)。

分发应用二进制时需同时提供对应版本源码和构建材料，并保留版权/NOTICE。若以后捆绑 Qt、ICU、媒体后端、FFmpeg 或其他库，还须落实该具体构建的对应源码和适用分发义务；不能只提供 Chosuta 源码替代这些依赖的义务。带 `--enable-nonfree` 的 FFmpeg 不作为分发依赖。[FFmpeg 许可说明](https://ffmpeg.org/legal.html)。

用户导入的工程、PNG、背景及音频仍由各自权利人决定用途，本项目许可不授予其再分发权。
