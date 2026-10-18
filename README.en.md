# Chosuta

[简体中文](README.md) | **English** | [日本語](README.ja.md)

Chosuta imports projects from virtual singer voice synthesis software and character illustrations with different mouth shapes to create singing videos. SVP (Synthesizer V) support has been verified. UST (UTAU), USTX (OpenUtau), VSQ, VSQX and VPR (VOCALOID), CCS (CeVIO), VVPROJ (VOICEVOX), DS (DiffSinger), MusicXML, XML and MXL are also supported, but have not been manually tested with real singing projects.

## 1. Features

Generate video from an SVP or another supported project and illustrations for its mouth shapes.  
In the editor, manually adjust animation timing and image variants as needed. The timeline can also display bars and beats.  
You can export an animation on a solid background for further editing in a video editor, or import a background and position the character to make a simple music video directly.  
Version 0.6.4 archives the previous approach: advanced settings combined ascending/descending motion, steps/large leaps/small leaps, and strong/weak beats into more than a hundred variants. This became cumbersome and produced disappointing results, so a different direction was considered. That version is available only as an archived Release, without a separate branch.  
Version 0.8.2 introduced a new approach to automatic image variants.  
Version 1.0.0 added support for more synthesis programs, without manual testing of those formats.

## 2. Current version

The current version is **1.0.1 (schema14)**, including batch import row selection and timeline width adaptation. This delivery provides a Linux x86_64 ZIP and an Arch package; see Releases for published attachments. Native builds and real interaction on Windows/macOS remain unverified for this version.

## 3. System requirements

| Environment | Requirements |
| --- | --- |
| Linux | A matching distribution and architecture; these binaries target Arch Linux x86_64. The ZIP links to system libraries and is not a self-contained cross-distribution package |
| Windows | Build scripts target Windows 10/11 x64; native 1.0.1 compilation/deployment remains unverified. Linux results do not establish Windows support |
| macOS | An experimental local build script is available; existing Xcode command-line tools and SDKs for the same architecture are required. No native validation yet |
| Runtime libraries | Qt ≥6.4 (Core, Gui, Widgets, Concurrent, Multimedia and platform/media plugins), ICU ≥67; LibYAML and traditional MiniZip/zlib use system shared libraries or static builds, depending on platform |
| Video export / waveforms | Optional external FFmpeg; MP4 needs libx264/AAC, WebM needs libvpx-vp9/libopus, transparent MOV needs qtrle |
| Audio duration detection | Optional ffprobe; without it, audio can still be attached and its duration set manually |

Audio, voicebanks and Synthesizer V itself are not prerequisites for import, mouth generation, preview, editing or saving. Set the FFmpeg path in the application. Python/PowerShell are not application runtime dependencies.

**Building from source also requires a development environment:**

| Platform | Prepare beforehand | What the scripts can fetch |
| --- | --- | --- |
| Linux | C++20 compiler, CMake ≥3.24, Ninja and Qt/ICU/LibYAML/traditional MiniZip/zlib development packages | Uses system libraries by default; no automatic system package installation |
| Windows | PowerShell ≥5.1, CMake ≥3.24, Ninja, matching MSVC or x86_64 MinGW compiler, Qt/ICU shared SDKs | `-DownloadDependencies` fetches LibYAML/zlib sources; `-DownloadFfmpeg` separately fetches optional media tools |
| macOS (not natively validated) | Xcode command-line tools, CMake ≥3.24, Ninja, matching Qt/ICU SDKs; system LibYAML/MiniZip/zlib development libraries by default | Local sources or explicit `--download-dependencies` for LibYAML/zlib sources |

Windows Qt, ICU and compiler architectures/ABIs must match. Use an x64 developer shell for MSVC; running MSVC applications also requires the official VC++ x64 runtime. **The scripts do not install Qt, ICU, CMake, Ninja or compilers.** Qt Test is required only when developer tests are explicitly enabled.

## 4. Installation

### Download binaries

Choose an attachment matching your system and architecture from this repository's **Releases** page. Each Release states what is published, verified and bundled. GitHub's automatic Source code archives contain source and need compilation.

- **Arch Linux**: install `chosuta-<version>-<package-release>-x86_64.pkg.tar.zst`, then run `chosuta` or use the desktop entry. System packages provide runtime libraries.
- **Linux ZIP**: extract the complete archive and run `bin/chosuta`; the CLI is `bin/chosuta-cli`. Matching system ABI and Qt platform/media plugins are required.
- **Windows**: if a verified runtime ZIP is available, extract it completely and run `chosuta.exe` at its root, keeping DLLs and plugins. Having a build script does not mean Windows binaries have been published.
- **Other Linux/macOS**: these Arch binaries are unsuitable; build from source on the target system. DEB/RPM and native macOS remain unverified.

AUR hosts build recipes. No AUR entry has been published, so there is no yay/paru installation command. See the [Linux packaging guide (Chinese)](docs/packaging.zh-CN.md).

### Build from source: Linux

With the development dependencies above already installed, extract the corresponding source ZIP or obtain this repository's source, then run from its root:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build --parallel 4
./build/chosuta
```

To install into a directory you own:

```sh
cmake --install build --prefix "$PWD/stage"
./stage/bin/chosuta
```

By default, dependencies are not downloaded and tests are not built or run. Linux packages use system Qt/ICU/LibYAML/MiniZip/zlib, without bundling those libraries or FFmpeg. Rebuild in the target environment for other distributions.

### Build from source: Windows

Prepare the tools and Qt/ICU SDKs listed above; for MSVC, use x64 Native Tools / Developer PowerShell. This command explicitly enables both kinds of downloads:

```powershell
.\scripts\build-windows.ps1 `
  -QtPrefix '<Qt x64 SDK prefix>' `
  -IcuPrefix '<ICU x64 SDK prefix>' `
  -DownloadDependencies -DownloadFfmpeg
```

| Option | Behavior |
| --- | --- |
| `-DownloadDependencies` | Fetches pinned LibYAML 0.2.5 and zlib 1.3.2 (including MiniZip) sources, verifies SHA-256 and builds them statically with the existing C/C++ toolchain; the actual complete sources and licenses accompany deployment |
| `-DownloadFfmpeg` | After the application builds successfully, fetches win64 GPL shared FFmpeg/ffprobe and DLLs from official BtbN releases, verifies and caches them; subsequent builds can reuse them offline |
| `-LibYamlSourceDir` and `-ZlibSourceDir` | Supply two complete local source trees instead of `-DownloadDependencies` for an offline build; specified local sources take precedence |
| `-FfmpegDir` | Supplies an existing bin directory containing ffmpeg.exe, ffprobe.exe and DLLs; mutually exclusive with `-DownloadFfmpeg` |
| `-Toolchain MinGW` | Uses x86_64 MinGW matching Qt/ICU; MSVC is the default |
| `-BothVariants -CreateZip` | Builds once and creates bundled-tools and no-ffmpeg directories and ZIPs; a media tool source must also be specified |

**Builds are offline by default; downloads require explicit options.** Without media tool options, FFmpeg is neither downloaded nor bundled, and editing/saving remain available. The no-ffmpeg variant retains Qt media backends; it does not remove all media DLLs.

Default output is `out/windows/Chosuta/chosuta.exe` and `chosuta-cli.exe`, with external tools in `tools/ffmpeg/`. The script pins the preflight CMake/Ninja/compiler paths, uses Ninja/Release and `BUILD_TESTING=OFF`, and does not install SDKs, change the permanent PATH, run CTest or launch the application. After Built is displayed, launch the GUI manually and keep the whole deployment directory. That directory must be empty and ZIPs must not exist; use `-StageDir 'out/windows/Chosuta-new'` for another build. See the [Windows guide (Chinese)](docs/windows.zh-CN.md).

### Build from source: macOS (not natively validated)

```sh
bash scripts/build-macos.sh --qt-prefix '<Qt macOS SDK prefix>' --icu-prefix '<ICU prefix>'
```

Offline by default; specify `--libyaml-source-dir` and `--zlib-source-dir`, or explicitly use `--download-dependencies`. Produces `out/macos/Chosuta/chosuta.app` and `bin/chosuta-cli`, using local SDKs. It does not install tools, copy runtimes, download FFmpeg, sign or notarize. Native macOS remains unverified; see the [macOS guide (Chinese)](docs/macos.zh-CN.md) and [build guide (Chinese)](docs/building.zh-CN.md).

## 5. Usage

See the [user guide (Chinese)](docs/user-guide.zh-CN.md).

### Quick start

Choose **File → Open score / Project** at the top left, or drag a project into the window. Chosuta recognizes it and generates the timeline.  
<img width="1280" height="930" alt="image" src="https://github.com/user-attachments/assets/0e13acc1-31c7-41af-a8e0-2174131c41f9" />  
<sub>Example song: 古今煌々//生々流転 by 仕事してP.</sub>  
For projects with multiple melody or harmony tracks, select the tracks to use and set their priorities under **Tracks**.  
Select **Assets** in the page selector at the top left and import the mouth variants.  
<img width="1923" height="1397" alt="image" src="https://github.com/user-attachments/assets/e9d76976-e94d-44a9-8988-8de95fbf1ab4" />  
<sub>Example illustration: [宮舞モカ立ち絵](https://commons.nicovideo.jp/works/im11390154) by かれい.</sub>  
Click **Play / Stop** or press Space to make your singer sing.  
Click **Export Video** at the top right to export the video.

### Other features

#### Canvas settings and character layout

Select **Canvas** in the page selector to change canvas size, background color, character size and position. Stop playback and choose **Move character** to drag the illustration directly on the canvas.  
<img width="780" height="797" alt="image" src="https://github.com/user-attachments/assets/9fc52b8d-6455-47a8-8c67-829c8969e7f8" />  
The usual workflow is to export green-screen footage and finish it in a video editor such as Premiere Pro. For a simple music video, you can instead import a custom background and export directly.

#### Audio and waveform timing correction

Click **Attach Audio** below the canvas, or drag an audio file into the window. If it is longer than the project, Chosuta asks whether to extend the animation. Make sure the audio and project are aligned.  
Click **Correct timing from waveform** above the timeline to adjust syllable timing from waveform boundaries. This can help with differences between score timing and the rendered singing caused by timing adjustments in synthesis software.  
<img width="1911" height="477" alt="image" src="https://github.com/user-attachments/assets/562e6437-f97d-4e33-ac13-233bca0c2b14" />  
Correction estimates timing from waveform energy; it does not recognize phonemes, mouth shapes or melodies, so treat it as an aid. Replacing the audio invalidates the previous correction and requires correction against the new audio; vocal boundaries in a mix may be unclear.  
The maximum shift is bounded to prevent one incorrect estimate from displacing the whole song. You can adjust this limit.

#### Timeline, transport and manual correction

Chosuta supports manual correction, with these controls to assist editing:

- **Bars / Beats**: switch between seconds and bars/beats above the timeline. BPM comes from the project and cannot be edited here.
- **Return to playback start on Stop**: enable this DAW-style behavior in **Transport** at the top left.
- **Cursor: beat** / **Cursor: syllable**: choose beat or syllable snapping above the timeline.
- **Timeline zoom and panning**: use the zoom slider or Ctrl+wheel to zoom; use the horizontal scrollbar or middle-button dragging to pan.
- **Mouth blocks**: click a block to set its image variant and start/end positions, or split/merge it. Drag the block to move it, or its edges to change its duration.

<img width="1923" height="1397" alt="image" src="https://github.com/user-attachments/assets/7cc489c2-5d5d-45a8-a25d-b1a09690697f" />

#### Subtitles

Enable **Add subtitles** above the timeline to add a subtitle track. Double-click an empty part of the subtitle lane to create a block. Drag blocks or their edges to move/resize them; double-click a block to edit its text.  
<img width="1923" height="1397" alt="image" src="https://github.com/user-attachments/assets/cbb67c66-f3f3-4731-ad0b-e37195f70d4d" />  
The editor lets you choose font, color, size and position. Subtitles can also be repositioned directly on the canvas.  
Chosuta is not a video editor; subtitles are a convenience. For full subtitle work or effects, finish the exported video in an editor with dedicated subtitle tools.

#### Consonant rules

Most synthesis software keeps the actual vowel/consonant timing in rendering caches rather than project files. Chosuta therefore uses a configurable fraction and duration cap, available under **Rules**. You can also drag boundaries manually.  
<img width="764" height="274" alt="image" src="https://github.com/user-attachments/assets/7e17986a-2bb4-41a2-a248-f91d01110f0b" />  
Different consonants use different mouth positions: /k/, /g/, /h/ are open, /b/, /p/, /m/ closed, and /f/, /v/ partly closed. Enumerating every shape would greatly increase the required images. Chosuta defaults to closed mouths for /b/, /p/, /m/ and uses the vowel's mouth shape for other supported consonants. Customize this in **Assets → Advanced variant settings → Consonant rules**, although changing the defaults is not recommended.  
<img width="1474" height="1127" alt="image" src="https://github.com/user-attachments/assets/d66b0c24-cb2c-4c88-aaf9-47e53a7732ec" />

### Advanced variants and automatic performance

#### A different approach

The abandoned 0.6.4 approach combined ascending/descending motion, steps/leaps and strong/weak beats, greatly increasing the required variants. Simple melodic intervals also proved insufficient for useful performance decisions. That approach is retired, but the archived Release remains available to try.  
The current approach estimates emphasis and restraint from surrounding notes. It cannot yet cover most musical situations, so more songs and feedback are welcome.

#### Enable advanced variants and configure groups

Enable **Enable advanced variants** at the top of **Assets**.  
Open **Advanced variant settings → Variant images**. Initially there is only the **Base** group, used for **Normal**, **Emphasis** and **Restrained**. Click **Add group**, assign its mouth images, then map a performance role to that group.  
<img width="1474" height="1127" alt="image" src="https://github.com/user-attachments/assets/fac226ee-f85c-4255-ae78-85ae12330ee6" />  
Group changes are not committed automatically; click **Apply** below to apply them to the performance.  
You can also import PNGs or a folder in **Batch import**, click **Select all rows**, choose a destination group and click **Set group for selected rows**. **Clear selection** clears row selection. Selecting rows does not change the **Import** checkboxes; replacing existing images still requires checking them.

#### Automatic performance settings

Changing the default rules is not recommended, but please get in touch if you find useful new ones!

**Tonality assistance**: not especially useful yet. Without a model, expecting rules alone to decide which tonic calls for restraint or which dominant belongs in a climax is ambitious. The controls are **Use tonality**, **Tonic** and **Mode**.  
<img width="1415" height="312" alt="image" src="https://github.com/user-attachments/assets/7052aab5-729d-4331-87f0-565bab1acdfe" />  
**Rule selection**: these rules were designed around scores by 仕事してP and cannot cover every song. Feedback about new rules is welcome.  
<img width="465" height="219" alt="image" src="https://github.com/user-attachments/assets/d26187ac-6d80-4caa-89a3-bc99a406ede0" />  
**Detailed thresholds and note spelling**: adjust conditions such as cumulative ascent, leap distance and the duration required to accumulate evidence. Custom rules are also supported.  
<img width="1474" height="1224" alt="image" src="https://github.com/user-attachments/assets/f427b03e-88b1-4aa1-beea-e120f1f134ce" />  
<img width="1415" height="656" alt="image" src="https://github.com/user-attachments/assets/b497136a-b162-4649-bb3f-88c37eb698dd" />

#### Manual performance correction

After applying advanced performance settings, a performance lane appears in the timeline. Click a performance block to adjust its duration, target variant groups and other settings.  
<img width="1923" height="1389" alt="image" src="https://github.com/user-attachments/assets/2e3c5f62-4966-4c72-8729-4d549527ba0b" />

## 6. How it works

Chosuta uses C++20, Qt 6 Widgets and ICU, with a core model independent of the GUI. Deterministic rules generate mouth shapes from the source score, explicit phonemes and lyrics, without synthesis applications, voicebanks, rendering caches or neural networks. Audio serves playback, the video soundtrack and explicitly requested timing correction.

1. **Sources and timing**: a shared pipeline imports SVP, UST, USTX, legacy VSQ (DSB301), VSQX, MusicXML/MXL, CCS song, basic VPR singing tracks, VVPROJ song, LAB, fully timed DS and VVPROJ talk with complete queries. Original bytes, integer positions, source resolution and track/group-instance identities are preserved. SVP expands only referenced mainGroup/mainRef and library groups, applying time/pitch offsets. One quarter note is 705,600,000 blick; tempo conversion is piecewise and meter supplies bar/beat positions. LAB/DS/talk use absolute-time phonemes without invented pitches or beats. Mixed VVPROJ files offer separate song/dialogue selection. Each format has a defined supported subset; implementation does not imply manual validation of every product version. See [format details (Chinese)](docs/user-guide.zh-CN.md).
2. **Readings and mouth shapes**: locked edits, per-note corrections and nonempty explicit source phonemes retain priority, followed by custom dictionaries and built-in rules. Japanese defaults to kana/romaji with kanji estimation off; Chinese supports common words, pinyin and ICU Han-Latin, and English uses a small original word list. The three original lists total about 14 KB. Built-in, custom and optional external dictionary data together may not exceed 3,000,000 bytes, with editable entries and JSON import/export. Unknown readings remain flagged. Empty lyrics default to rest without explicit phonemes or a manual reading; saved project policies remain. Continuations, extensions, closures, breaths and rests have separate handling. Leading and trailing consonant groups each use proportional allocation with a default 80 ms cap; 0 removes the fixed cap. Bilabials such as b/p/m default to closed mouths; other supported consonants follow the vowel, with configurable rules. SV leftOffset is retained with diagnostics because its meaning has not been verified.
3. **Variant groups and automatic performance**: simple mode is the default. Advanced mode chooses complete images by variant group × mouth shape; mouth generation and performance group selection are independent. Each group has five vowels plus closed/rest/breath as eight main slots, with an auxiliary unknown image. Missing images fall back from the current group's mouth to the base group's same mouth, then the global fallback. Automatic performance examines score context, limited repeated motifs and arrival-note groups. Restraint requires sustained descent, wider onset spacing and ending-boundary support; repeated notes, short turns and breaths can retain the current state. It does not recognize actual loudness or emotion. Tonic and mode are separate; only the selected duration basis (musical time/seconds) and distance basis (semitones/degrees) apply. **Apply** in advanced settings updates automatic performance while preserving protected manual segments. Separate generation offers draft comparison, apply/cancel and selected-range regeneration; the CLI exports score diagnostics.
4. **Manual editing and asset import**: mouth events and independent performance segments support editing, splitting/merging, protection and undo. Performance templates are **Hold**, **Expand**, **Contract** and **Arch**. Ordinary mouth edits continue following the variant group; explicitly fixing a complete image bypasses it. Performance edits do not clear waveform correction. Split/moved/resized mouth edits and merges from the same source retain source fragments for later time mapping. Ambiguous cross-source mappings retain second-based snapshots with diagnostics. PNG/folder import supports arbitrary filenames, per-row mapping, **Select all rows** / **Clear selection**, group assignment, thumbnails and drag/drop. Row selection and import checkboxes are independent. Checking a replacement confirms it; the last checked valid item for a slot wins, first updating the settings draft.
5. **Timeline and subtitles**: the timeline fills at least its viewport and adapts to window width, with horizontal scrolling for longer content. Window width does not change row heights or the time scale. Middle-button dragging pans; the zoom slider anchors to a visible playhead, or the view centre if it is off screen. Ctrl+wheel anchors to the pointer, within scroll limits. Seek on the ruler; other empty areas clear selection. The cursor can snap to beats/syllables. The play button, Space and **Transport** share play/stop behavior and the option to return to the playback start. Subtitles are off by default, with multiple tracks, in-block text editing and basic styles. Optional lyric alignment uses the selected track's score timing, without audio or sentence recognition. Subtitles are independent of mouth events and survive regeneration. Character and subtitle layouts can be edited in the stopped preview, with one undo step per drag. Text editing reuses one native Qt control; local fonts may be substituted on other computers.
6. **Waveforms and correction**: the waveform sits below the ruler and above mouth events. Its display height is adjustable without changing audio timing. Import only displays it; **Correct timing from waveform** estimates clear starts/ends near score boundaries. Defaults are 100 ms maximum shift and 25% maximum duration change, adjustable and saved. Corrections are independent and do not accumulate, alter phonemes/mouth shapes or change proportions inside a segment. Unclear, conflicting or manually protected positions retain their original results. Corrections support revert/undo and are shared by preview/export. Validity uses score information and audio path, size and modification time; regeneration or changed inputs/audio can invalidate it. Audio contents are not hashed. The TimingRefiner provider interface remains, with no phoneme recognition, source separation or model dependency. CLI commands include correct/revert.
7. **Projects and languages**: only current schema14 is read/written, without migration from older schemas; keep old projects and open them with the corresponding old application. Projects embed the original source; sourceFormat selects its parser, defaulting to SVP when absent from a current schema14 project. Original sources, generated results, manual overrides, performance and timing correction are stored separately. Regeneration preserves locked edits; project operations support undo/redo, relative asset paths and relocation. The interface, errors, common Qt controls and structured performance reasons are available in Chinese, English and Japanese. Raw external-tool errors remain in technical details. Changing language does not change data IDs or undo history.
8. **Composition and external tools**: preview, timeline and export share event evaluation and Scene composition, evaluating mouths, performance, subtitles, backgrounds and character positions at each frame's absolute time. Images decode on demand with a shared bounded cache; media paths are used directly. External FFmpeg is launched with an executable path and argument array; export also works without audio. Export settings offer a computer default, project override and asynchronous tool checks, with ffprobe following the selected FFmpeg. Windows deploys GUI/CLI executables at the root alongside Qt/ICU DLLs and plugins, with external tools in tools/ffmpeg/. One build can produce bundled-tools and no-ffmpeg variants; the latter retains Qt media dependencies. The supplied logo is used for the window, Windows EXE and Linux desktop icon, without new image-processing runtime dependencies.

Automatic results are animation-timing estimates, not reproductions of synthesis software's private acoustic models. See the [user guide (Chinese)](docs/user-guide.zh-CN.md) for operations, format boundaries and limitations.

## 7. Project structure

```text
CMakeLists.txt              Build, test, install and CPack entry point
src/core/                   Time/project models and external tool discovery
src/importers/              Score/absolute-time phoneme import and shared ZIP reading
src/pronunciation/          Chinese, English and Japanese readings
src/rules/                  Mouth event generation and track selection
src/project/                Project storage and restoration
src/render/                 Preview/export composition, FFmpeg, audio metadata/waveforms
src/ui/                     Qt Widgets interface, timeline and preferences
src/cli/                    Headless demo, import, generation and export
resources/                  Original word lists, icons and third-party license texts
tests/                      Core/UI/packaging tests and original minimal format fixtures
cmake/                      Build information and distribution packaging templates
packaging/                  Linux desktop entry and Arch recipe template
scripts/                    Windows deployment, local packaging and public source preparation
docs/                       Build, usage, packaging and license documentation
README.en.md / README.ja.md  English and Japanese READMEs
LICENSE                     Complete GNU GPL v3 text
THIRD_PARTY_NOTICES.md       Actual components, sources and distribution status
```

Public source excludes user songs, illustrations, audio, private regression data, development history, build directories and credentials. Public tests use original fixtures; missing local private regressions are explicitly skipped.

## 8. Dependencies and third-party components

| Component | Purpose | Acquisition/deployment |
| --- | --- | --- |
| Qt ≥6.4 Core/Gui/Widgets/Concurrent/Multimedia; transitive Qt Network | Data, images, UI, background tasks and playback | Prepare the development environment; Arch uses system shared libraries, Windows uses windeployqt from the matching SDK for Qt libraries/plugins |
| ICU ≥67 i18n/uc/data | Unicode handling and transliteration | Prepare development libraries; Windows deploys matching shared DLLs |
| LibYAML ≥0.2.5 | USTX YAML parsing | Linux system library; Windows builds pinned 0.2.5 sources statically, supplied offline or fetched with `-DownloadDependencies` |
| Traditional MiniZip/zlib | ZIP/DEFLATE reading for VPR/MXL | Linux system libraries; Windows builds reading/decompression from the complete zlib 1.3.2 source, selected like LibYAML |
| FFmpeg/ffprobe (optional) | Encoding, duration detection and waveform decoding | Separate processes with configurable existing paths; Windows explicitly fetches BtbN GPL shared with `-DownloadFfmpeg`, independently of parser dependencies |
| C++ compiler, CMake, Ninja, moc/rcc | Building and code generation | Prepare beforehand; not application runtime dependencies. MSVC applications use the official VC++ x64 runtime |
| Qt Test | Developer validation | Required only with `BUILD_TESTING=ON`; ordinary builds and packaging disable tests |
| Python 3, makepkg, fakeroot | Source archives and local Linux packaging | Development helpers only; not bundled |

LibYAML uses MIT, MiniZip/zlib use the Zlib license, ICU retains Unicode and accompanying licenses, and Qt follows the license of the actual build/deployment. Static source builds retain the complete actual source trees and licenses. Automatically collecting Windows license files does not replace checking the corresponding-source obligations of specific binaries. FFmpeg builds must not use `--enable-nonfree`. See [third-party notices](THIRD_PARTY_NOTICES.md) and the [license review (Chinese)](docs/license-review.zh-CN.md).

The small built-in reading lists and fixtures are original Chosuta work. No complete CMUdict, voicebank, synthesis engine, model or third-party font is bundled. Users supply images/audio. The experimental macOS script uses existing SDK shared libraries without copying them.

## 9. AI usage statement

A human provided the architecture, usage design and features. The code was developed by the genius programmer Codex, assisted by the big blue fish DeepSeek, with human review and verification.

## 10. Bug reports

For bug reports and feature suggestions, email 2441002630@qq.com.

## 11. License

Copyright (C) 2026 Chosuta contributors.

Unless otherwise noted, Chosuta source, documentation and original resources are distributed under the **GNU General Public License, version 3 or any later version (GPL-3.0-or-later)**, without warranty. See [LICENSE](LICENSE) for the full text. Third-party components retain their own licenses; invoking them does not change their licenses to Chosuta's.

Review of the current components, linkage and distributed content found no clear license conflict preventing the current source and Arch application package from being distributed under GPLv3. Qt's LGPLv3 option can be combined with GPLv3 applications. ICU's main license is the permissive Unicode license, with accompanying terms retained in full. Build tools and application runtime code are recorded separately. See [Qt licensing](https://doc.qt.io/qt-6.11/licensing.html) and the [Unicode explanation](https://unicode.org/faq/unicode_license.html).

When distributing binaries, provide the corresponding version's source and build materials and retain copyright/NOTICE files. Future bundling of Qt, ICU, media backends, FFmpeg or other libraries requires the corresponding sources and applicable obligations for those particular builds; Chosuta source alone does not satisfy them. FFmpeg builds using `--enable-nonfree` are not distribution dependencies. See [FFmpeg licensing](https://ffmpeg.org/legal.html).

Imported projects, PNGs, backgrounds and audio remain subject to their owners' rights. This project's license does not grant redistribution rights to those materials.
