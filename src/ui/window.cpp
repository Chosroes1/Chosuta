// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include <QInputMethod>
#include "core/executable.h"
#include <QtConcurrent/QtConcurrentRun>
#include <cmath>
namespace chosuta {
    class ProjectCommand:public QUndoCommand {
        Window *window;
        Project before,after;
        bool timingOnly=false;
        public:
        ProjectCommand(Window*w,Project a,Project b,QString label,bool onlyTiming=false):QUndoCommand(label),window(w),before(std::move(a)),after(std::move(b)),timingOnly(onlyTiming){}
        void undo()override {
            window->assignProject(before,timingOnly);
        }
        void redo()override {
            window->assignProject(after,timingOnly);
        }
    };
    QString Window::trText(const char*key)const {
        struct Entry {
            const char*en;
            const char*zh;
            const char*ja;
        };
        static const Entry entries[]= {
            {"Advanced variant settings","高级差分设置","高度な差分設定"},
            {"Missing compatible pose","缺少对应姿态图片","対応する姿勢画像がありません"},
            {"Enable advanced variants","启用高级差分","高度な差分を有効化"},
            {"These rules also apply to advanced variants: they determine mouth states and timing; advanced settings select images that preserve pose and expression.","这些规则同样适用于高级差分：决定口形状态与时序，高级设置据此选择包含动作和神态的图片。","これらの規則は高度な差分にも適用されます。口形とタイミングを決め、高度な設定で動作や表情を保つ画像を選択します。"},
            {"Waveform timing","波形时间校正","波形によるタイミング調整"},
            {"Correct timing from waveform","按波形校正时间","波形でタイミングを補正"},
            {"Revert waveform correction","回退波形校正","波形補正を元に戻す"},
            {"Waveform correction reverted.","已回退波形校正。","波形補正を元に戻しました。"},
            {"Reload waveform","重读波形","波形を再読込"},
            {"Max shift (ms)","最大偏移（毫秒）","最大ずれ（ms）"},
            {"Max duration change (%)","最大时长变化（%）","最大時間変化（%）"},
            {"Preparing waveform…","正在准备波形…","波形を準備中…"},
            {"Waveform unavailable: %1","波形不可用：%1","波形を利用できません：%1"},
            {"Waveform preparation cancelled. Reload to retry.","波形准备已取消，可点击重读重试。","波形の準備を中止しました。再読込で再試行できます。"},
            {"Correction is optional and starts only when clicked. Clear onset/end estimates stay near score timing; unclear or mixed audio may keep the original timing. Limits apply when correcting.","点击后才校正，可回退；只在工程附近估计明确起止。连唱或混音中不明确的位置保留工程时间。上限在点击校正时应用。","クリックした時だけ補正し、元に戻せます。譜面付近の明確な開始・終了のみ推定します。連続歌唱やミックスで不明確な箇所は元の時間を保ちます。上限は補正時に適用されます。"},
            {"Waveform correction: %1 changed, %2 protected, %3 unclear, %4 conflicting.","波形校正：%1 个来源已调整，%2 个受人工修改保护，%3 个不明确，%4 个冲突。","波形補正：%1 件を調整、%2 件は手動編集を保護、%3 件は不明確、%4 件は競合。"},
            {"Saved waveform correction is stale; score timing is used. Correct again if needed.","已保存的波形校正已失效，使用工程时间；需要时可重新校正。","保存された波形補正は無効です。譜面時間を使用します。必要なら再補正してください。"},
            {"Edit subtitle text","编辑字幕文字","字幕の文字を編集"},
            {"Subtitle text limit is 4096 characters.","单条字幕文字上限为 4096 字符，已拒绝本次超限输入。","字幕の文字数は最大 4096 です。上限を超える入力は適用されません。"},
            {"Subtitle ends after the animation. Extend the duration to include it.","字幕超过动画末尾，可延长动画时长以完整显示。","字幕がアニメーションの終わりを超えています。全体を表示するには時間を延長してください。"},
            {"Some subtitles exceed the fixed duration. Extend before exporting? No exports only the chosen duration.","部分字幕超过固定动画时长。导出前是否延长？选择“否”只导出当前指定时长。","一部の字幕が指定時間を超えています。書き出す前に延長しますか？「いいえ」では指定した時間だけを書き出します。"},
            {"Scene diagnostics","画布与字幕诊断","画面と字幕の診断"},
            {"Subtitles","字幕","字幕"},
            {"Add subtitles","添加字幕","字幕を追加"},
            {"Subtitle track","字幕轴","字幕トラック"},
            {"New subtitle track","新增字幕轴","字幕トラックを追加"},
            {"Delete subtitle track","删除字幕轴","字幕トラックを削除"},
            {"Enable this track","启用此轴","このトラックを有効化"},
            {"Align to lyrics","对齐歌词","歌詞に合わせる"},
            {"Lyric source track","歌词来源轨道","歌詞の参照トラック"},
            {"Choose a lyric source","选择歌词来源","歌詞の参照先を選択"},
            {"Double-click an empty subtitle lane to enter text. Apply confirms style and timing. Lyric alignment uses score timing.","双击字幕轴空白处输入文字；样式和时间修改后点击应用。歌词对齐依据谱面时间。","字幕トラックの空白をダブルクリックして文字を入力します。スタイルと時間は適用ボタンで確定します。歌詞との同期は譜面時間に基づきます。"},
            {"Enter subtitle text","输入字幕文字","字幕の文字を入力"},
            {"Text","文字","文字"},
            {"Name","名称","名前"},
            {"First lyric","起始歌词","開始歌詞"},
            {"Last lyric","结束歌词","終了歌詞"},
            {"Use lyric range","使用歌词范围","歌詞の範囲を使用"},
            {"Override style for this subtitle","仅此字幕覆盖样式","この字幕のスタイルを上書き"},
            {"Delete subtitle","删除字幕","字幕を削除"},
            {"Font","字体","フォント"},
            {"Edit text in subtitle block","在字幕块中编辑文字","字幕ブロック内で文字を編集"},
            {"Restore timeline heights","恢复轴高度","トラックの高さをリセット"},
            {"Consonant limit (ms; 0 = no cap)","辅音组时长上限（毫秒；0 为不限）","子音群の時間上限（ms、0 は上限なし）"},
            {"Font height (% of canvas)","字号（画布高度 %）","文字サイズ（画面高さ %）"},
            {"Center X (%)","中心 X（%）","中心 X（%）"},
            {"Center Y (%)","中心 Y（%）","中心 Y（%）"},
            {"Text width (%)","文字框宽度（%）","テキスト幅（%）"},
            {"Text color","文字颜色","文字色"},
            {"Bold","粗体","太字"},
            {"Italic","斜体","斜体"},
            {"Outline","描边","縁取り"},
            {"Left","左对齐","左揃え"},
            {"Center","居中","中央"},
            {"Right","右对齐","右揃え"},
            {"Text alignment","文字对齐","文字揃え"},
            {"Apply subtitles","应用字幕与样式","字幕とスタイルを適用"},
            {"Extend to subtitle end","延长至字幕末尾","字幕の終わりまで延長"},
            {"Move character","拖动立绘布局","立ち絵の配置をドラッグ"},
            {"Move subtitles","拖动字幕布局","字幕の配置をドラッグ"},
            {"Lyric source missing; subtitle text and timing were retained.","歌词来源缺失；已保留字幕文字及时间。","参照歌詞がありません。字幕の文字と時間は保持されています。"},
            {"No nearby lyric; kept the manual interval.","附近无可对齐歌词；保留手动区间。","近くに対応する歌詞がありません。手動の区間を保持します。"},
            {"Advanced settings","高级设置","詳細設定"},
            {"Language","语言","言語"},
            {"Word / phrase","词 / 词组","単語 / 語句"},
            {"Notation","读音格式","読みの形式"},
            {"Reading / phonemes","读音 / 音素","読み / 音素"},
            {"Kana / pinyin / word","假名 / 拼音 / 单词","仮名 / ピンイン / 単語"},
            {"Phonemes","音素","音素"},
            {"Add word","添加词条","語句を追加"},
            {"Remove selected","删除所选词条","選択した語句を削除"},
            {"Import dictionary","导入词典","辞書を読み込む"},
            {"Export dictionary","导出词典","辞書を書き出す"},
            {"Pronunciation dictionary","读音词典","発音辞書"},
            {"Japanese","日文","日本語"},
            {"Estimate Japanese kanji readings (optional)","估计日文汉字读音（可选）","日本語の漢字の読みを推定（任意）"},
            {"Also save as defaults for new SVP imports","同时保存为新导入 SVP 的默认设置","新しい SVP の既定設定としても保存"},
            {"Dictionary data: %1 / 3,000,000 bytes","词典数据：%1 / 3,000,000 字节","辞書データ：%1 / 3,000,000 バイト"},
            {"Dictionary exceeds 20000 entries","词典超过 20,000 个词条","辞書が 20,000 語を超えています"},
            {"Custom readings override built-in readings. Note corrections and explicit SVP phonemes keep priority. Regenerate to apply.","自定义读音优先于内置读音。逐音符人工修正和 SVP 显式音素保持优先。保存后重新生成口形即可应用。","独自の読みは内蔵の読みより優先されます。ノートの修正と SVP の明示音素は最優先です。保存後に再生成してください。"},
            {"Japanese uses kana by default. This small word table cannot guarantee kanji readings; unknown words still need correction. Explicit custom readings work even with this option off.","日文默认以假名为准。小型词表不能保证汉字读音准确；未知词仍需修正。关闭此选项也可以使用您明确添加的自定义读音。","日本語は仮名を基本とします。小さな語彙表では漢字の読みの正確さを保証できません。未知の語は修正が必要です。無効でも登録した独自の読みを使えます。"},
            {"Settings","设置","設定"},
            {"Preferences","偏好设置","環境設定"},
            {"System language","跟随系统","システム言語"},
            {"Interface language","界面语言","表示言語"},
            {"Pause behavior","暂停行为","一時停止時の動作"},
            {"Resume from pause","从暂停处继续（默认）","停止位置から再開（既定）"},
            {"Return to marker","暂停后回到指定位置","停止後に指定位置へ戻る"},
            {"Return position (seconds)","返回位置（秒）","戻る位置（秒）"},
            {"Set return point","设为返回位置","戻る位置に設定"},
            {"Canvas","画布 / 背景","キャンバス / 背景"},
            {"Aspect ratio","画面比例","縦横比"},
            {"Custom","自定义","カスタム"},
            {"Animation duration","动画时长（秒）","アニメーションの長さ（秒）"},
            {"Auto (score)","自动（随乐谱）","自動（楽譜）"},
            {"Background image","背景图片","背景画像"},
            {"Choose image","选择图片","画像を選択"},
            {"Remove image","移除背景图","背景画像を削除"},
            {"Background fit","背景铺放","背景の配置"},
            {"Cover","填满（等比裁切）","全体を覆う（切り抜き）"},
            {"Contain","完整显示（等比）","全体を表示（余白）"},
            {"Stretch","拉伸至画布","キャンバスに伸縮"},
            {"Character scale (%)","立绘大小（%）","キャラクターの大きさ（%）"},
            {"Character center X (%)","立绘中心 X（%）","キャラクター中心 X（%）"},
            {"Character center Y (%)","立绘中心 Y（%）","キャラクター中心 Y（%）"},
            {"Reset layout","重置立绘布局","配置をリセット"},
            {"Transparent canvas","透明画布","透明キャンバス"},
            {"Use audio duration","使用音频时长","音声の長さを使用"},
            {"Extend animation","延长动画","アニメーションを延長"},
            {"Audio is longer than animation. Extend to %1 seconds?","音频比当前动画长。是否将动画延长到 %1 秒？","音声がアニメーションより長くなっています。%1 秒に延長しますか？"},

            {
                "File","文件","ファイル"
            }, {
                "Edit","编辑","編集"
            }, {
                "View","视图","表示"
            }, {
                "Help","帮助","ヘルプ"
            },
            {
                "Open SVP / Project","打开 SVP / 工程","SVP / プロジェクトを開く"
            }, {
                "Save","保存","保存"
            }, {
                "Save As","另存为","名前を付けて保存"
            }, {
                "Export Video","导出视频","動画を書き出す"
            }, {
                "Export Settings","导出设置","書き出し設定"
            }, {
                "Quit","退出","終了"
            },
            {
                "Undo","撤销","元に戻す"
            }, {
                "Redo","重做","やり直す"
            }, {
                "Generate","生成口形","口形を生成"
            }, {
                "Tracks","轨道","トラック"
            }, {
                "Track","轨道","トラック"
            }, {
                "Priority (lower first)","优先级（小值优先）","優先度（小さい値を優先）"
            }, {
                "Notes","音符","ノート"
            },
            {
                "Assets","PNG 差分","PNG 差分"
            }, {
                "Import PNGs","导入 PNG","PNG を読み込む"
            }, {
                "Demo Assets","创建测试素材","テスト素材を作成"
            }, {
                "Relocate Assets","重定位素材","素材を再指定"
            }, {
                "Custom Shape","自定义差分","口形を追加"
            }, {
                "Shape","口形 ID","口形 ID"
            }, {
                "Path","路径","パス"
            }, {
                "Fallback","缺图回退","画像未設定時の口形"
            },
            {
                "Rules","规则","ルール"
            }, {
                "Lyrics Language","歌词语言","歌詞の言語"
            }, {
                "Auto","跟随工程 / 自动","プロジェクト / 自動"
            }, {
                "Harmony takeover","主轨休止时和声接管","主トラックの休止中はハーモニーを使用"
            }, {
                "Consonant fraction","辅音占音节比例","子音の時間比率"
            },
            {
                "Special","特殊项","特殊項目"
            }, {
                "Mode","行为","動作"
            }, {
                "Hold (seconds)","保持秒数","保持秒数"
            }, {
                "Shape immediately","立即指定口形","口形を即時指定"
            }, {
                "Keep previous","保持前一口形","直前の口形を保持"
            }, {
                "Hold then shape","保持后指定口形","保持後に口形を指定"
            },
            {
                "Rest","休止","休止"
            }, {
                "Empty lyric","空歌词","空の歌詞"
            }, {
                "Closure cl","促音 cl","閉鎖 cl"
            }, {
                "Breath br","呼吸 br","ブレス br"
            }, {
                "Nasal","鼻音","鼻音"
            }, {
                "Extension - / ー","延音 - / ー","長音 - / ー"
            },
            {
                "English Dictionary","加载英文词典","英語辞書を読み込む"
            }, {
                "Diagnostics","诊断 / 待处理","診断 / 要確認"
            }, {
                "Preview","预览","プレビュー"
            }, {
                "Play / Pause","播放 / 暂停","再生 / 一時停止"
            }, {
                "Audio","音频","音声"
            }, {
                "Attach Audio","附加音频","音声を追加"
            }, {
                "Remove Audio","移除音频","音声を削除"
            },
            {
                "Seconds","秒 / 毫秒","秒 / ミリ秒"
            }, {
                "Bars / Beats","小节 / 拍","小節 / 拍"
            }, {
                "Zoom","缩放","ズーム"
            }, {
                "Position (seconds)","播放位置（秒）","再生位置（秒）"
            }, {
                "Selection","选择事件","イベントを選択"
            }, {
                "Start (seconds)","起点（秒）","開始（秒）"
            }, {
                "End (seconds)","终点（秒）","終了（秒）"
            }, {
                "Anchor","时间锚定","時間の基準"
            }, {
                "Fixed seconds","固定秒数","秒に固定"
            }, {
                "Follow beats","跟随节拍","拍に追従"
            }, {
                "Locked","锁定人工修改","手動変更をロック"
            },
            {
                "Apply to selection","应用到所选事件","選択イベントに適用"
            }, {
                "Split at cursor","在播放位置拆分","カーソルで分割"
            }, {
                "Merge selection","合并所选事件","選択イベントを結合"
            }, {
                "Batch offset","批量偏移","一括オフセット"
            }, {
                "Correct pronunciation","修正读音","読みを修正"
            }, {
                "Delete selection","删除所选事件","選択イベントを削除"
            }, {
                "Reset manual edits","重置全部人工修改","手動変更をすべてリセット"
            },
            {
                "Cancel","取消","キャンセル"
            }, {
                "Ready","就绪","準備完了"
            }, {
                "Error","错误","エラー"
            }, {
                "Cancelled","已取消","キャンセルしました"
            }, {
                "Export complete","导出完成","書き出し完了"
            }, {
                "Working…","处理中…","処理中…"
            },
            {
                "Unsaved changes","尚有未保存修改","未保存の変更"
            }, {
                "Save before closing?","是否保存当前工程？","プロジェクトを保存しますか？"
            }, {
                "Overwrite","覆盖输出","出力を上書き"
            }, {
                "Replace existing file?","是否替换已有文件？","既存のファイルを置き換えますか？"
            },
            {
                "Width","宽度","幅"
            }, {
                "Height","高度","高さ"
            }, {
                "Frame rate NUM/DEN","帧率 NUM/DEN","フレームレート NUM/DEN"
            }, {
                "Format","视频格式","動画形式"
            }, {
                "Background","背景颜色","背景色"
            }, {
                "Transparent (MOV)","透明背景（MOV）","透明背景（MOV）"
            }, {
                "Quality CRF","质量 CRF","品質 CRF"
            }, {
                "Video kbps (0 = CRF)","视频 kbps（0 使用 CRF）","動画 kbps（0 = CRF）"
            }, {
                "Duration (0 = score)","时长（0 随乐谱）","長さ（0 = 楽譜）"
            }, {
                "Animation offset (seconds)","动画同步偏移（秒）","アニメーションのオフセット（秒）"
            }, {
                "Audio offset (seconds)","音频偏移（秒）","音声のオフセット（秒）"
            }, {
                "FFmpeg executable","FFmpeg 程序路径","FFmpeg 実行ファイル"
            },
            {
                "Estimated size","估计体积","推定サイズ"
            }, {
                "Quality mode: size varies","质量模式：体积不确定","品質モード：サイズは変動"
            }, {
                "Size estimate excludes audio/container","体积估计不含音频及封装开销","推定サイズは音声とコンテナを含まない"
            },
            {
                "Enter shape ID","输入差分 ID","口形 ID を入力"
            }, {
                "Offset in seconds","偏移秒数","オフセット秒数"
            }, {
                "Explicit pronunciation for source note","输入源音符的明确读音 / 音素","元のノートの読み / 音素を入力"
            }, {
                "No event selected","尚未选择事件","イベントが選択されていません"
            }, {
                "Select one event","请选择一个事件","イベントを一つ選択してください"
            }, {
                "Missing assets","素材缺失","素材が見つかりません"
            }, {
                "Orphan manual edits","孤立人工修改","元のイベントがない手動変更"
            }, {
                "Unknown pronunciations","待修正读音","未解決の読み"
            }, {
                "Estimated timing","规则估计时序","ルールによる推定タイミング"
            },
            {
                "Guide","操作说明","使い方"
            }, {
                "Open an SVP, select tracks, generate, import PNGs, edit the timeline, save, and export. Drag event edges to resize; drag the middle to move. Ctrl-click selects multiple events. Ctrl-wheel zooms. Priority: lower number wins. Corrections are estimates; unknown words remain marked. Demo assets are original test drawings.","打开 SVP → 勾选轨道 → 生成口形 → 导入 PNG → 编辑时间轴 → 保存 → 导出。拖动边缘调整边界，拖动中部移动，Ctrl 点击多选，Ctrl 滚轮缩放。优先级数值较小者优先。自动结果均为时序估计，未知读音保持待处理标记。测试素材为原创几何图形。","SVP を開く → トラック選択 → 生成 → PNG 読み込み → タイムライン編集 → 保存 → 書き出し。端をドラッグして長さを変更、中央をドラッグして移動。Ctrl クリックで複数選択、Ctrl ホイールでズーム。小さい優先度の値を優先。自動結果は推定で、未知の読みは要確認として表示されます。テスト素材はオリジナルの図形です。"
            }
        };
        for(const auto&e:entries)if(qstrcmp(e.en,key)==0)return QString::fromUtf8(uiLanguage=="zh"?e.zh:uiLanguage=="ja"?e.ja:e.en);
        return QString::fromUtf8(key);
    }
    Window::Window() {
        preferences=loadPreferences();
        project.rules.pronunciation=preferences.pronunciation;
        uiLanguage=resolveUiLanguage(preferences.language,QLocale::system().uiLanguages());
        qApp->installEventFilter(this);
        setWindowTitle("Chosuta");
        resize(1280,900);
        progress=new QProgressBar;
        progress->setMaximumWidth(210);
        progress->hide();
        cancelButton=new QPushButton;
        cancelButton->setObjectName("cancelTask");
        cancelButton->hide();
        statusBar()->addPermanentWidget(progress);
        statusBar()->addPermanentWidget(cancelButton);
        connect(cancelButton,&QPushButton::clicked,this,[this] {
            if(cancel)cancel->store(true);
        });
        connect(&history,&QUndoStack::cleanChanged,this,[this](bool clean) {
            setWindowModified(!clean);
        });
        connect(&loadWatcher,&QFutureWatcher<LoadResult>::finished,this,[this] {
            auto result=loadWatcher.result();
            setBusy(false);
            if(result.cancelled) {
                statusBar()->showMessage(trText("Cancelled"));
                return;
            }
            if(!result.error.isEmpty()) {
                error(result.error);
                return;
            }
            if(loadIsRegenerate)history.push(new ProjectCommand(this,project,result.project,trText("Generate")));
            else {
                projectPath=activePath.endsWith(".chosuta",Qt::CaseInsensitive)?activePath:QString{};
                history.clear();
                assignProject(result.project);
                history.setClean();
            }
        });
        connect(&exportWatcher,&QFutureWatcher<ExportResult>::finished,this,[this] {
            auto result=exportWatcher.result();
            setBusy(false);
            if(result.cancelled)statusBar()->showMessage(trText("Cancelled"));
            else if(!result.success)error(result.error);
            else statusBar()->showMessage(trText("Export complete")+": "+activePath+(result.diagnostics.isEmpty()?QString{}:" — "+result.diagnostics.join("; ")));
        });
        connect(&audioWatcher,&QFutureWatcher<AudioInfo>::finished,this,[this]{
            auto r=audioWatcher.result();setBusy(false);
            if(r.cancelled){statusBar()->showMessage(trText("Cancelled"));return;}
            if(!r.error.isEmpty()){error(r.error);return;}
            double audioEnd=std::max(0.,r.duration+project.output.audioOffset);bool extend=false;
            if(r.duration>0&&audioEnd>project.duration()+.001&&audioEnd<=21600){
                extend=QMessageBox::question(this,trText("Extend animation"),trText("Audio is longer than animation. Extend to %1 seconds?").arg(audioEnd,0,'f',3),QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes;
            }
            change(trText("Audio"),[r,audioEnd,extend](Project&p){p.audioPath=r.path;p.audioDuration=r.duration;if(extend)p.output.duration=audioEnd;});
            if(!r.warning.isEmpty())statusBar()->showMessage(r.warning);
        });
        connect(&waveformWatcher,&QFutureWatcher<WaveformResult>::finished,this,[this]{
            auto r=waveformWatcher.result();setBusy(false);waveform=r.wave;
            if(waveform)project.audioContentHash=waveform->hash;
            else {
                QFileInfo info(project.audioPath);
                waveformKey=project.audioPath+"\n"+QString::number(info.size())+"\n"+QString::number(info.lastModified().toMSecsSinceEpoch())+"\n"+project.output.ffmpeg;
            }
            waveformMessage=r.cancelled?trText("Waveform preparation cancelled. Reload to retry."):r.error.isEmpty()?QString{}:trText("Waveform unavailable: %1").arg(r.error);
            // Recheck saved timing against the decoded content, without reloading PNGs.
            if(scene)scene->setTiming(project);
            timeline->setProject(project);refreshWaveform();refreshSelection();refreshPreview();
            auto stale=trText("Saved waveform correction is stale; score timing is used. Correct again if needed.");
            auto text=diagnostics->toPlainText();text.remove(stale);
            if(!project.timing.sources.isEmpty()&&!timingCorrectionCurrent(project))text+="\n\n"+stale;
            diagnostics->setPlainText(text);
            if(!waveformMessage.isEmpty())statusBar()->showMessage(waveformMessage);
        });
        connect(&correctionWatcher,&QFutureWatcher<WaveCorrectionResult>::finished,this,[this]{
            auto r=correctionWatcher.result();setBusy(false);
            if(r.cancelled){statusBar()->showMessage(trText("Cancelled"));return;}
            if(!r.error.isEmpty()){error(r.error);return;}
            change(trText("Correct timing from waveform"),[r](Project &p){p.timing=r.timing;},true);
            statusBar()->showMessage(trText("Waveform correction: %1 changed, %2 protected, %3 unclear, %4 conflicting.").arg(r.accepted).arg(r.protectedSources).arg(r.unclear).arg(r.conflicts));
        });
        playbackTimer.setInterval(16);
        connect(&playbackTimer,&QTimer::timeout,this,[this] {
            playTime=playOrigin+playbackClock.elapsed()/1000.;
            if(playTime>=project.duration()) {
                stopPlayback();
                playTime=project.duration();
            }
            syncAudio();
            refreshPreview();
            followCursor(true);
        });
        buildUi();
        refresh();
    }
    void Window::buildUi() {
        if(timeline)timeline->finishSubtitleEditing(false);
        auto old=takeCentralWidget();
        delete old;
        subtitlePage=nullptr;subtitleCuePanel=nullptr;subtitleText=nullptr;timeline=nullptr;preview=nullptr;
        for(auto action:menuBar()->actions())if(action->menu())delete action->menu();
        menuBar()->clear();
        auto file=menuBar()->addMenu(trText("File"));
        auto edit=menuBar()->addMenu(trText("Edit"));
        auto settings=menuBar()->addMenu(trText("Settings"));
        settings->setObjectName("settingsMenu");
        auto help=menuBar()->addMenu(trText("Help"));
        auto action=[&](QMenu*menu,const char*name,const std::function<void()>&fn,QKeySequence shortcut={}) {
            auto a=menu->addAction(trText(name));
            if(!shortcut.isEmpty())a->setShortcut(shortcut);
            connect(a,&QAction::triggered,this,fn);
            return a;
        };
        action(file,"Open SVP / Project",[this] {
            openDialog();
        },QKeySequence::Open);
        action(file,"Save",[this] {
            save();
        },QKeySequence::Save);
        action(file,"Save As",[this] {
            save(true);
        },QKeySequence::SaveAs);
        file->addSeparator();
        action(file,"Export Settings",[this] {
            showExportSettings();
        });
        action(file,"Export Video",[this] {
            startExport();
        },QKeySequence(Qt::CTRL|Qt::Key_E));
        file->addSeparator();
        action(file,"Quit",[this] {
            close();
        },QKeySequence::Quit);
        auto undo=history.createUndoAction(edit,trText("Undo"));
        undo->setShortcut(QKeySequence::Undo);
        edit->addAction(undo);
        auto redo=history.createRedoAction(edit,trText("Redo"));
        redo->setShortcut(QKeySequence::Redo);
        edit->addAction(redo);
        action(edit,"Reset manual edits",[this] {
            resetOverrides();
        });
        action(settings,"Preferences",[this]{showPreferences();})->setObjectName("preferencesAction");
        action(settings,"Advanced variant settings",[this]{showMouthSettings();})->setObjectName("advancedMouthAction");
        action(settings,"Advanced settings",[this]{showAdvancedSettings();})->setObjectName("advancedSettingsAction");
        action(settings,"Canvas",[this]{tabs->setCurrentWidget(canvasPage);});
        action(help,"Guide",[this] {
            QMessageBox::information(this,trText("Guide"),trText("Open an SVP, select tracks, generate, import PNGs, edit the timeline, save, and export. Drag event edges to resize; drag the middle to move. Ctrl-click selects multiple events. Ctrl-wheel zooms. Priority: lower number wins. Corrections are estimates; unknown words remain marked. Demo assets are original test drawings."));
        });
        auto central=new QWidget;
        auto root=new QVBoxLayout(central);
        setCentralWidget(central);
        auto canvasBanner=new QHBoxLayout;root->addLayout(canvasBanner);
        auto canvasButton=new QPushButton(trText("Canvas"));canvasButton->setObjectName("canvasButton");canvasBanner->addWidget(canvasButton);
        canvasLabel=new QLabel;canvasBanner->addWidget(canvasLabel);canvasBanner->addStretch();
        connect(canvasButton,&QPushButton::clicked,this,[this]{tabs->setCurrentWidget(canvasPage);});
        timelineSplitter=new QSplitter(Qt::Vertical);timelineSplitter->setObjectName("timelineSplitter");root->addWidget(timelineSplitter,1);
        auto horizontal=new QSplitter;timelineSplitter->addWidget(horizontal);
        tabs=new QTabWidget;
        tabs->setMinimumWidth(300);
        horizontal->addWidget(tabs);
        auto button=[&](QLayout*layout,const char*key,const std::function<void()>&fn,QString name={}) {
            auto b=new QPushButton(trText(key));
            if(!name.isEmpty())b->setObjectName(name);
            layout->addWidget(b);
            connect(b,&QPushButton::clicked,this,fn);
            return b;
        };
        auto trackPage=new QWidget;
        auto trackLayout=new QVBoxLayout(trackPage);
        tracks=new QTableWidget(0,3);
        tracks->setObjectName("tracks");
        tracks->setHorizontalHeaderLabels( {
            trText("Track"),trText("Priority (lower first)"),trText("Notes")
        });
        tracks->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
        tracks->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
        tracks->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
        trackLayout->addWidget(tracks);
        button(trackLayout,"Generate",[this] {
            regenerate();
        },"generate");
        tabs->addTab(trackPage,trText("Tracks"));
        connect(tracks,&QTableWidget::itemChanged,this,[this](QTableWidgetItem*item) {
            if(refreshing)return;
            Project next=project;
            next.selected.clear();
            for(int row=0;row<tracks->rowCount();++row) {
                auto id=project.score.tracks[row].id;
                bool ok=false;
                int priority=tracks->item(row,1)->text().toInt(&ok);
                if(!ok) {
                    refresh();
                    return;
                }
                next.priorities[id]=priority;
                if(tracks->item(row,0)->checkState()==Qt::Checked)next.selected<<id;
            }
            history.push(new ProjectCommand(this,project,next,trText("Tracks")));
            Q_UNUSED(item);
        });
        auto assetPage=new QWidget;
        auto assetLayout=new QVBoxLayout(assetPage);
        advancedMouth=new QCheckBox(trText("Enable advanced variants"));advancedMouth->setObjectName("advancedMouthEnabled");assetLayout->addWidget(advancedMouth);
        connect(advancedMouth,&QCheckBox::toggled,this,[this](bool value){if(!refreshing&&!busy)change(trText("Advanced variant settings"),[value](Project &p){p.appearance.enabled=value;});});
        auto mouthSettings=new QPushButton(trText("Advanced variant settings"));mouthSettings->setObjectName("advancedMouthButton");assetLayout->addWidget(mouthSettings);connect(mouthSettings,&QPushButton::clicked,this,&Window::showMouthSettings);
        assets=new QTableWidget(0,2);
        assets->setHorizontalHeaderLabels( {
            trText("Shape"),trText("Path")
        });
        assets->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
        assets->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);
        assets->setEditTriggers(QAbstractItemView::NoEditTriggers);
        assetLayout->addWidget(assets);
        auto assetButtons=new QHBoxLayout;
        assetLayout->addLayout(assetButtons);
        button(assetButtons,"Import PNGs",[this] {
            importImages();
        });
        button(assetButtons,"Demo Assets",[this] {
            createDemo();
        });
        auto extraButtons=new QHBoxLayout;
        assetLayout->addLayout(extraButtons);
        button(extraButtons,"Relocate Assets",[this] {
            relocateAssets();
        });
        button(extraButtons,"Custom Shape",[this] {
            addCustom();
        });
        auto fallbackRow=new QHBoxLayout;
        assetLayout->addLayout(fallbackRow);
        fallbackRow->addWidget(new QLabel(trText("Fallback")));
        fallback=new QComboBox;
        fallback->setEditable(true);
        fallbackRow->addWidget(fallback);
        connect(fallback,&QComboBox::currentTextChanged,this,[this](const QString&s) {
            if(!refreshing&&!s.isEmpty())change(trText("Fallback"),[s](Project&p) {
                p.fallback=s;
            });
        });
        connect(assets,&QTableWidget::cellDoubleClicked,this,[this](int row,int) {
            QString id=assets->item(row,0)->text();
            auto path=QFileDialog::getOpenFileName(this,trText("Import PNGs"),{},"PNG (*.png)");
            if(!path.isEmpty())change(trText("Assets"),[=](Project&p) {
                p.assets[id]=path;
            });
        });
        tabs->addTab(assetPage,trText("Assets"));
        canvasPage=buildCanvasPanel();tabs->addTab(canvasPage,trText("Canvas"));
        auto rulePage=new QWidget;
        auto ruleLayout=new QVBoxLayout(rulePage);
        auto form=new QFormLayout;
        ruleLayout->addLayout(form);
        language=new QComboBox;
        language->addItem(trText("Auto"),"auto");
        language->addItem("日本語","ja");
        language->addItem("中文","zh");
        language->addItem("English","en");
        form->addRow(trText("Lyrics Language"),language);
        takeover=new QCheckBox(trText("Harmony takeover"));
        form->addRow(takeover);
        consonant=new QDoubleSpinBox;
        consonant->setRange(0,.8);
        consonant->setSingleStep(.01);
        form->addRow(trText("Consonant fraction"),consonant);
        consonantLimit=new QDoubleSpinBox;consonantLimit->setObjectName("consonantLimit");consonantLimit->setRange(0,1000);consonantLimit->setDecimals(1);consonantLimit->setSingleStep(10);form->addRow(trText("Consonant limit (ms; 0 = no cap)"),consonantLimit);
        special=new QTableWidget(6,4);
        special->setHorizontalHeaderLabels( {
            trText("Special"),trText("Mode"),trText("Shape"),trText("Hold (seconds)")
        });
        special->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        special->setMinimumHeight(230);
        ruleLayout->addWidget(special);
        auto scopeHint=new QLabel(trText("These rules also apply to advanced variants: they determine mouth states and timing; advanced settings select images that preserve pose and expression."));
        scopeHint->setObjectName("rulesAdvancedScopeHint");
        scopeHint->setWordWrap(true);
        ruleLayout->addWidget(scopeHint);
        QStringList keys= {
            "rest","empty","cl","br","nasal","extend"
        };
        QStringList labels= {
            "Rest","Empty lyric","Closure cl","Breath br","Nasal","Extension - / ー"
        };
        for(int i=0;i<6;++i) {
            auto item=new QTableWidgetItem(trText(labels[i].toUtf8().constData()));
            item->setFlags(Qt::ItemIsEnabled);
            item->setData(Qt::UserRole,keys[i]);
            special->setItem(i,0,item);
            auto mode=new QComboBox;
            mode->addItem(trText("Shape immediately"),"shape");
            mode->addItem(trText("Keep previous"),"hold");
            mode->addItem(trText("Hold then shape"),"timed");
            special->setCellWidget(i,1,mode);
            auto s=new QComboBox;
            s->setEditable(true);
            s->addItems( {
                "closed","rest","breath","unknown","A","I","U","E","O"
            });
            special->setCellWidget(i,2,s);
            auto hold=new QDoubleSpinBox;
            hold->setRange(0,60);
            hold->setDecimals(3);
            hold->setSingleStep(.01);
            special->setCellWidget(i,3,hold);
        }
        button(ruleLayout,"Generate",[this] {
            regenerate();
        });
        button(ruleLayout,"English Dictionary",[this] {
            auto path=QFileDialog::getOpenFileName(this,trText("English Dictionary"));
            if(!path.isEmpty())change(trText("English Dictionary"),[=](Project&p) {
                p.rules.englishDictionary=path;
            });
        });
        tabs->addTab(rulePage,trText("Rules"));
        // Store panel changes in the undoable project immediately; generation remains an explicit operation.
        auto updateRules=[this] {
            if(refreshing)return;
            Project next=project;
            collectRules(next);
            history.push(new ProjectCommand(this,project,next,trText("Rules")));
        };
        connect(language,&QComboBox::currentIndexChanged,this,updateRules);
        connect(takeover,&QCheckBox::toggled,this,updateRules);
        connect(consonant,&QDoubleSpinBox::valueChanged,this,updateRules);
        for(int i=0;i<special->rowCount();++i) {
            connect(qobject_cast<QComboBox*>(special->cellWidget(i,1)),&QComboBox::currentIndexChanged,this,updateRules);
            connect(qobject_cast<QComboBox*>(special->cellWidget(i,2)),&QComboBox::currentTextChanged,this,updateRules);
            connect(qobject_cast<QDoubleSpinBox*>(special->cellWidget(i,3)),&QDoubleSpinBox::valueChanged,this,updateRules);
        }
        diagnostics=new QPlainTextEdit;
        diagnostics->setReadOnly(true);
        tabs->addTab(diagnostics,trText("Diagnostics"));
        auto right=new QWidget;
        auto rightLayout=new QVBoxLayout(right);
        horizontal->addWidget(right);
        layoutTarget=new QComboBox;layoutTarget->setObjectName("layoutTarget");layoutTarget->addItem(trText("Move character"),QString{});rightLayout->addWidget(layoutTarget);
        preview=new PreviewCanvas;preview->setProperty("missingAssetLabel",trText("Missing compatible pose"));
        rightLayout->addWidget(preview,1);
        connect(layoutTarget,&QComboBox::currentIndexChanged,this,[this]{if(!refreshing)updateLayoutTarget();});
        connect(preview,&PreviewCanvas::layoutEdited,this,[this](Project p){changeSubtitles(trText("Canvas"),[p](Project&next){next.canvas=p.canvas;next.subtitles=p.subtitles;});});
        auto playerRow=new QHBoxLayout;
        rightLayout->addLayout(playerRow);
        playButton=button(playerRow,"Play / Pause",[this] {
            togglePlayback();
        },"play");
        button(playerRow,"Attach Audio",[this] {
            attachAudio();
        });
        button(playerRow,"Remove Audio",[this] {
            stopPlayback();
            change(trText("Remove Audio"),[](Project&p) {
                p.audioPath.clear();p.audioDuration=0;
            });
            if(player)player->setSource({});
        });
        button(playerRow,"Set return point",[this]{double marker=playTime;change(trText("Set return point"),[marker](Project&p){p.playbackReturnPosition=marker;});},"setReturnPoint");
        position=new QDoubleSpinBox;
        position->setObjectName("playPosition");
        position->setRange(0,21600);
        position->setDecimals(3);
        position->setSingleStep(.05);
        position->setToolTip(trText("Position (seconds)"));
        playerRow->addWidget(position);
        timeLabel=new QLabel;
        playerRow->addWidget(timeLabel);
        connect(position,&QDoubleSpinBox::valueChanged,this,[this](double t) {
            if(!refreshing)seek(t);
        });
        selectionLabel=new QLabel(trText("Selection"));
        selectionLabel->setObjectName("selectionLabel");
        rightLayout->addWidget(selectionLabel);
        auto properties=new QHBoxLayout;
        rightLayout->addLayout(properties);
        auto labeled=[&](const char*text,QWidget*w) {
            auto col=new QVBoxLayout;
            col->addWidget(new QLabel(trText(text)));
            col->addWidget(w);
            properties->addLayout(col);
        };
        start=new QDoubleSpinBox;
        end=new QDoubleSpinBox;
        for(auto spin: {
            start,end
        }) {
            spin->setRange(-21600,21600);
            spin->setDecimals(6);
            spin->setSingleStep(.01);
        }
        start->setObjectName("eventStart");
        end->setObjectName("eventEnd");
        labeled("Start (seconds)",start);
        labeled("End (seconds)",end);
        shape=new QComboBox;
        shape->setEditable(true);
        shape->setObjectName("eventShape");
        labeled("Shape",shape);
        anchor=new QComboBox;
        anchor->addItem(trText("Fixed seconds"),"seconds");
        anchor->addItem(trText("Follow beats"),"beats");
        labeled("Anchor",anchor);
        lock=new QCheckBox(trText("Locked"));
        lock->setChecked(true);
        properties->addWidget(lock);
        auto editButtons=new QGridLayout;
        rightLayout->addLayout(editButtons);
        QStringList buttons= {
            "Apply to selection","Split at cursor","Merge selection","Batch offset","Correct pronunciation","Delete selection"
        };
        QVector<std::function<void()>>functions= {
            [this] {
                editSelection();
            },[this] {
                splitSelection();
            },[this] {
                mergeSelection();
            },[this] {
                offsetSelection();
            },[this] {
                correctReading();
            },[this] {
                deleteSelection();
            }
        };
        for(int i=0;i<buttons.size();++i) {
            auto b=new QPushButton(trText(buttons[i].toUtf8().constData()));
            if(i==0)b->setObjectName("apply");
            editButtons->addWidget(b,i/3,i%3);
            connect(b,&QPushButton::clicked,this,functions[i]);
        }
        horizontal->setSizes( {
            390,810
        });
        auto timelineRow=new QHBoxLayout;
        auto timelinePanel=new QWidget;auto timelineLayout=new QVBoxLayout(timelinePanel);timelineLayout->setContentsMargins(0,0,0,0);timelineLayout->addLayout(timelineRow);timelineSplitter->addWidget(timelinePanel);timelineSplitter->setStretchFactor(0,1);timelineSplitter->setStretchFactor(1,0);
        auto units=new QComboBox;
        units->addItems( {
            trText("Seconds"),trText("Bars / Beats")
        });
        timelineRow->addWidget(units);
        timelineRow->addWidget(new QLabel(trText("Zoom")));
        auto zoom=new QSlider(Qt::Horizontal);
        zoom->setRange(5,1000);
        zoom->setValue(120);
        zoom->setMaximumWidth(200);
        timelineRow->addWidget(zoom);
        subtitleEnabled=new QCheckBox(trText("Add subtitles"));subtitleEnabled->setObjectName("subtitlesEnabled");timelineRow->addWidget(subtitleEnabled);
        connect(subtitleEnabled,&QCheckBox::toggled,this,[this](bool enabled){if(refreshing||busy)return;changeSubtitles(trText("Subtitles"),[this,enabled](Project&p){p.subtitlesEnabled=enabled;if(enabled&&p.subtitles.isEmpty()){SubtitleTrack t;t.id=QUuid::createUuid().toString(QUuid::Id128);t.name=trText("Subtitle track")+" 1";if(p.score.tracks.size()==1)t.sourceTrack=p.score.tracks[0].id;p.subtitles.append(t);}});if(enabled&&subtitlePage)tabs->setCurrentWidget(subtitlePage);});
        auto resetHeights=new QPushButton(trText("Restore timeline heights"));resetHeights->setObjectName("resetTimelineHeights");timelineRow->addWidget(resetHeights);connect(resetHeights,&QPushButton::clicked,this,[this]{timeline->resetLaneHeights();preferences.timelineHeight=250;timelineSplitter->setSizes({600,290});saveViewPreferences();});
        timelineRow->addStretch();
        button(timelineRow,"Export Settings",[this] {
            showExportSettings();
        });
        button(timelineRow,"Export Video",[this] {
            startExport();
        });
        waveformControls=new QWidget;waveformControls->setObjectName("waveformControls");auto waveRow=new QHBoxLayout(waveformControls);waveRow->setContentsMargins(0,0,0,0);
        waveRow->addWidget(new QLabel(trText("Waveform timing")));
        timingMaxShift=new QDoubleSpinBox;timingMaxShift->setObjectName("timingMaxShift");timingMaxShift->setRange(0,1000);timingMaxShift->setDecimals(0);timingMaxShift->setSuffix(" ms");
        waveRow->addWidget(new QLabel(trText("Max shift (ms)")));waveRow->addWidget(timingMaxShift);
        timingMaxDuration=new QDoubleSpinBox;timingMaxDuration->setObjectName("timingMaxDuration");timingMaxDuration->setRange(0,50);timingMaxDuration->setDecimals(0);timingMaxDuration->setSuffix(" %");waveRow->addWidget(new QLabel(trText("Max duration change (%)")));waveRow->addWidget(timingMaxDuration);
        correctTimingButton=new QPushButton(trText("Correct timing from waveform"));correctTimingButton->setObjectName("correctWaveformTiming");waveRow->addWidget(correctTimingButton);connect(correctTimingButton,&QPushButton::clicked,this,&Window::correctTiming);
        revertTimingButton=new QPushButton(trText("Revert waveform correction"));revertTimingButton->setObjectName("revertWaveformTiming");waveRow->addWidget(revertTimingButton);connect(revertTimingButton,&QPushButton::clicked,this,&Window::revertTiming);
        reloadWaveformButton=new QPushButton(trText("Reload waveform"));reloadWaveformButton->setObjectName("reloadWaveform");waveRow->addWidget(reloadWaveformButton);connect(reloadWaveformButton,&QPushButton::clicked,this,[this]{waveformKey.clear();ensureWaveform();});waveRow->addStretch();
        waveformControls->setToolTip(trText("Correction is optional and starts only when clicked. Clear onset/end estimates stay near score timing; unclear or mixed audio may keep the original timing. Limits apply when correcting."));timelineLayout->addWidget(waveformControls);
        timelineScroll=new QScrollArea;
        timelineScroll->setObjectName("timelineScroll");
        auto scroll=timelineScroll;
        scroll->setWidgetResizable(false);
        scroll->setMinimumHeight(185);
        timeline=new Timeline;
        scroll->setWidget(timeline);
        timelineLayout->addWidget(scroll);
        timeline->setLaneHeights(preferences.mouthLaneHeight,preferences.subtitleLaneHeight);
        timeline->setWaveformHeight(preferences.waveformLaneHeight);
        timelineSplitter->setSizes({600,preferences.timelineHeight+40});
        connect(timelineSplitter,&QSplitter::splitterMoved,this,[this]{preferences.timelineHeight=std::clamp(timelineScroll->height(),180,900);});
        connect(timeline,&Timeline::laneHeightChanged,this,[this](const QString&id,int h){if(id.isEmpty())preferences.mouthLaneHeight=h;else if(id=="@waveform")preferences.waveformLaneHeight=h;else preferences.subtitleLaneHeight=h;updateTimelineMinimum();saveViewPreferences();});
        connect(timeline,&Timeline::textEditingFinished,this,[this]{++subtitleTextSession;});
        connect(timeline,&Timeline::deleteRequested,this,&Window::deleteSelection);
        connect(timeline,&Timeline::blankClicked,this,&Window::clearObjectSelection);
        connect(preview,&PreviewCanvas::deleteRequested,this,&Window::deleteSelection);
        connect(preview,&PreviewCanvas::blankClicked,this,&Window::clearObjectSelection);
        connect(units,&QComboBox::currentIndexChanged,this,[this](int i) {
            timeline->setBeats(i==1);
        });
        connect(zoom,&QSlider::valueChanged,this,[this](int z) {
            timeline->setZoom(z);
        });
        connect(timeline,&Timeline::selectionChanged,this,[this] {
            subtitleCueId.clear();if(subtitlePage){bool old=refreshing;refreshing=true;refreshSubtitles();refreshing=old;}
            refreshSelection();
        });
        connect(timeline,&Timeline::subtitleCreated,this,&Window::addSubtitle);
        connect(timeline,&Timeline::subtitleSelected,this,&Window::selectSubtitle);
        connect(timeline,&Timeline::subtitleEdited,this,[this](QString id,SubtitleCue cue){changeSubtitles(trText("Subtitles"),[id,cue](Project&p){for(auto&t:p.subtitles)if(t.id==id)for(auto&c:t.cues)if(c.id==cue.id)c=cue;});});
        connect(timeline,&Timeline::editRejected,this,[this](const QString&m){statusBar()->showMessage(m,7000);});
        connect(scroll->verticalScrollBar(),&QScrollBar::valueChanged,timeline,qOverload<>(&QWidget::update));
        connect(timeline,&Timeline::seek,this,[this](double t) {
            seek(t);
        });
        connect(timeline,&Timeline::edited,this,[this](const QVector<Event>&events) {
            QString a=anchor->currentData().toString();
            bool locked=lock->isChecked();
            change(trText("Selection"),[=](Project&p) {
                for(const auto&e:events)p.edit(e,locked,a);
            });
        });
        cancelButton->setText(trText("Cancel"));
    }
    void Window::assignProject(Project p,bool timingOnly) {
        if(waveform&&waveform->path==p.audioPath)p.audioContentHash=waveform->hash;
        else p.audioContentHash.clear();
        project=std::move(p);
        refresh(timingOnly);
    }
    void Window::refresh(bool timingOnly) {
        refreshing=true;
        setWindowTitle("Chosuta — "+(projectPath.isEmpty()?trText("Estimated timing"):QFileInfo(projectPath).fileName())+"[*]");
        setWindowModified(!history.isClean());
        tracks->setRowCount(project.score.tracks.size());
        for(int i=0;i<project.score.tracks.size();++i) {
            const auto&t=project.score.tracks[i];
            auto name=new QTableWidgetItem(t.name);
            name->setData(Qt::UserRole,t.id);
            name->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable);
            name->setCheckState(project.selected.contains(t.id)?Qt::Checked:Qt::Unchecked);
            if(t.muted)name->setForeground(Qt::gray);
            tracks->setItem(i,0,name);
            tracks->setItem(i,1,new QTableWidgetItem(QString::number(project.priorities.value(t.id,i))));
            auto count=new QTableWidgetItem(QString::number(t.notes.size()));
            count->setFlags(Qt::ItemIsEnabled);
            tracks->setItem(i,2,count);
        }
        advancedMouth->setChecked(project.appearance.enabled);
        QStringList shapes= {
            "A","I","U","E","O","closed","rest","breath","unknown"
        };
        for(const auto&s:project.assets.keys())if(!shapes.contains(s))shapes<<s;
        QStringList simpleShapes;
        for(const auto &id:shapes)if(!id.contains("_")||!normalizedAssetId(id))simpleShapes.append(id);
        assets->setRowCount(simpleShapes.size());
        for(int i=0;i<simpleShapes.size();++i) {
            auto s=simpleShapes[i];
            assets->setItem(i,0,new QTableWidgetItem(s));
            auto item=new QTableWidgetItem(project.assets.value(s));
            if(!item->text().isEmpty()&&!QFileInfo::exists(item->text()))item->setForeground(Qt::red);
            assets->setItem(i,1,item);
        }
        fallback->clear();
        fallback->addItems(shapes);
        fallback->setCurrentText(project.fallback);
        auto previousShape=shape->currentText();
        shape->clear();
        shape->addItems(shapes);
        shape->setCurrentText(previousShape.isEmpty()?"A":previousShape);
        language->setCurrentIndex(language->findData(project.rules.language));
        takeover->setChecked(project.rules.harmonyTakeover);
        consonant->setValue(project.rules.consonantRatio);
        consonantLimit->setValue(project.rules.consonantMaxSeconds*1000);
        for(int i=0;i<special->rowCount();++i) {
            auto policy=project.rules.special.value(special->item(i,0)->data(Qt::UserRole).toString());
            auto mode=qobject_cast<QComboBox*>(special->cellWidget(i,1));
            mode->setCurrentIndex(mode->findData(policy.mode));
            qobject_cast<QComboBox*>(special->cellWidget(i,2))->setCurrentText(policy.shape);
            qobject_cast<QDoubleSpinBox*>(special->cellWidget(i,3))->setValue(policy.holdSeconds);
        }
        if(timingOnly&&scene)scene->setTiming(project);
        else scene=std::make_unique<Scene>(project);
        QStringList messages {
            trText("Estimated timing")
        };
        for(const auto&d:project.score.diagnostics)messages<<d.path+"\n"+d.code+": "+d.message;
        if(!scene->diagnostics.isEmpty())messages<<trText("Scene diagnostics")+": "+scene->diagnostics.join("; ");
        auto orphans=project.orphanOverrides();
        if(!orphans.isEmpty())messages<<trText("Orphan manual edits")+"\n"+orphans.join('\n');
        int unknown=0;
        for(const auto&e:project.effective())if(e.unknown) {
            ++unknown;
            messages<<trText("Unknown pronunciations")+": "+e.text+"\n"+e.source;
        }
        messages<<QString("%1: %2").arg(trText("Unknown pronunciations")).arg(unknown);
        if(!project.audioPath.isEmpty())messages<<trText("Audio")+": "+project.audioPath;
        if(!project.timing.sources.isEmpty()&&!timingCorrectionCurrent(project))messages<<trText("Saved waveform correction is stale; score timing is used. Correct again if needed.");
        diagnostics->setPlainText(messages.join("\n\n"));
        refreshCanvas();
        timeline->setProject(project);
        refreshSubtitles();
        timeline->setReturnPosition(project.playbackReturnPosition);
        refreshWaveform();
        refreshing=false;
        refreshSelection();
        refreshPreview();
    }
    void Window::refreshPreview() {
        if(!scene)return;
        preview->setState(project,scene.get(),playTime,!playing&&!busy);
        timeline->setEditable(!playing&&!busy);
        if(correctTimingButton)correctTimingButton->setEnabled(waveform&&waveform->path==project.audioPath&&!project.generated.isEmpty()&&!playing&&!busy);
        if(revertTimingButton)revertTimingButton->setEnabled(!project.timing.sources.isEmpty()&&!playing&&!busy);
        timeline->setCursor(playTime);
        bool old=refreshing;
        refreshing=true;
        position->setValue(playTime);
        if(playing)followCursor();
        refreshing=old;
        timeLabel->setText(QString("%1 s | %2").arg(playTime,0,'f',3).arg(project.score.time.beatLabel(project.score.time.blicks(playTime-project.output.syncOffset))));
    }
    void Window::refreshSelection() {
        auto ids=timeline->selectedIds();
        selectionLabel->setText(trText("Selection")+QString(" (%1)").arg(ids.size()));
        for(const auto&e:project.effective())if(ids.contains(e.id)) {
            start->setValue(e.start);
            end->setValue(e.end);
            shape->setCurrentText(e.shape);
            auto it=project.overrides.find(e.id);
            lock->setChecked(it==project.overrides.end()||it->locked);
            anchor->setCurrentIndex(anchor->findData(it==project.overrides.end()?"seconds":it->anchor));
            selectionLabel->setText(trText("Selection")+QString(" (%1)  ").arg(ids.size())+e.text+" | "+e.provenance);
            break;
        }
    }
    void Window::collectRules(Project&p)const {
        p.rules.language=language->currentData().toString();
        p.rules.harmonyTakeover=takeover->isChecked();
        p.rules.consonantRatio=consonant->value();
        p.rules.consonantMaxSeconds=consonantLimit->value()/1000;
        for(int i=0;i<special->rowCount();++i)p.rules.special[special->item(i,0)->data(Qt::UserRole).toString()]= {
            qobject_cast<QComboBox*>(special->cellWidget(i,1))->currentData().toString(),qobject_cast<QComboBox*>(special->cellWidget(i,2))->currentText(),qobject_cast<QDoubleSpinBox*>(special->cellWidget(i,3))->value()
        };
    }
    void Window::change(const QString&label,const std::function<void(Project&)>&fn,bool timingOnly) {
        if(busy)return;
        try {
            Project next=project;
            fn(next);
            history.push(new ProjectCommand(this,project,std::move(next),label,timingOnly));
        }
        catch(const std::exception&e) {
            error(QString::fromUtf8(e.what()));
        }
    }
    void Window::setBusy(bool value) {
        if(value&&timeline)timeline->finishSubtitleEditing(false);
        busy=value;
        centralWidget()->setEnabled(!value);
        menuBar()->setEnabled(!value);
        progress->setVisible(value);
        cancelButton->setVisible(value);
        if(value) {
            stopPlayback();
            progress->setValue(0);
            statusBar()->showMessage(trText("Working…"));
        }
        else statusBar()->showMessage(trText("Ready"));
    }
    void Window::error(const QString&message) {
        QMessageBox::warning(this,trText("Error"),message);
    }
    bool Window::confirmDiscard() {
        if(timeline)timeline->finishSubtitleEditing(false);
        if(history.isClean())return true;
        auto result=QMessageBox::question(this,trText("Unsaved changes"),trText("Save before closing?"),QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);
        return result==QMessageBox::Discard||(result==QMessageBox::Save&&save());
    }
    bool Window::save(bool choosePath) {
        if(timeline&&timeline->editingText())QGuiApplication::inputMethod()->commit();
        if(project.score.raw.isEmpty())return false;
        auto path=projectPath;
        if(path.isEmpty()||choosePath)path=QFileDialog::getSaveFileName(this,trText("Save As"),projectPath,"Chosuta (*.chosuta)");
        if(path.isEmpty())return false;
        if(!path.endsWith(".chosuta",Qt::CaseInsensitive))path+=".chosuta";
        try {
            saveProject(project,path);
            projectPath=path;
            history.setClean();
            refresh();
            return true;
        }
        catch(const std::exception&e) {
            error(QString::fromUtf8(e.what()));
            return false;
        }
    }
    void Window::openDialog() {
        if(!confirmDiscard())return;
        auto path=QFileDialog::getOpenFileName(this,trText("Open SVP / Project"),{},"SVP / Chosuta (*.svp *.chosuta)");
        if(!path.isEmpty())openPath(path);
    }
    void Window::openPath(const QString&path) {
        if(busy)return;
        activePath=path;
        loadIsRegenerate=false;
        cancel=std::make_shared<std::atomic_bool>(false);
        auto token=cancel;
        setBusy(true);
        auto report=[this](int n) {
            QMetaObject::invokeMethod(this,[this,n] {
                progress->setValue(n);
            },Qt::QueuedConnection);
        };
        loadWatcher.setFuture(QtConcurrent::run([path,token,report,defaults=preferences.pronunciation] {
            LoadResult r;
            try {
                if(path.endsWith(".chosuta",Qt::CaseInsensitive))r.project=loadProject(path);
                else {
                    r.project.rules.pronunciation=defaults;
                    r.project.score=importSvp(path,token.get(),report);
                    if(!r.project.score.tracks.isEmpty())r.project.selected= {
                        r.project.score.tracks[0].id
                    };
                    r.project.regenerate(token.get());
                }
                r.cancelled=token->load();
            }
            catch(const std::exception&e) {
                r.cancelled=token->load();
                r.error=QString::fromUtf8(e.what());
            }
            return r;
        }));
    }
    void Window::regenerate() {
        if(busy||project.score.raw.isEmpty())return;
        Project next=project;
        collectRules(next);
        loadIsRegenerate=true;
        cancel=std::make_shared<std::atomic_bool>(false);
        auto token=cancel;
        setBusy(true);
        loadWatcher.setFuture(QtConcurrent::run([next,token]()mutable {
            LoadResult r;
            try {
                next.regenerate(token.get());
                r.project=next;
                r.cancelled=token->load();
            }
            catch(const std::exception&e) {
                r.error=QString::fromUtf8(e.what());
            }
            return r;
        }));
    }
    void Window::importImages() {
        auto files=QFileDialog::getOpenFileNames(this,trText("Import PNGs"),{},"PNG (*.png)");
        if(files.isEmpty())return;
        change(trText("Assets"),[files](Project&p) {
            for(const auto&path:files) {
                QString id=QFileInfo(path).completeBaseName();
                if(QStringList {
                    "a","i","u","e","o"
                }
                .contains(id))id=id.toUpper();
                if(const auto canonical=normalizedAssetId(id))id=*canonical;
                p.assets[id]=path;
            }
        });
    }
    void Window::createDemo() {
        auto directory=QFileDialog::getExistingDirectory(this,trText("Demo Assets"));
        if(directory.isEmpty())return;
        try {
            createDemoAssets(directory);
            change(trText("Demo Assets"),[directory](Project&p) {
                for(const auto&s:QStringList {
                    "A","I","U","E","O","closed","rest","breath","unknown"
                })p.assets[s]=QDir(directory).filePath(s+".png");
            });
        }
        catch(const std::exception&e) {
            error(QString::fromUtf8(e.what()));
        }
    }
    void Window::relocateAssets() {
        auto directory=QFileDialog::getExistingDirectory(this,trText("Relocate Assets"));
        if(directory.isEmpty())return;
        change(trText("Relocate Assets"),[directory](Project&p) {
            for(auto it=p.assets.begin();it!=p.assets.end();++it) {
                auto path=QDir(directory).filePath(QFileInfo(it.value()).fileName());
                if(QFileInfo::exists(path))it.value()=path;
            }
            if(!p.canvas.backgroundImage.isEmpty()){auto path=QDir(directory).filePath(QFileInfo(p.canvas.backgroundImage).fileName());if(QFileInfo::exists(path))p.canvas.backgroundImage=path;}
            if(!p.audioPath.isEmpty()) {
                auto path=QDir(directory).filePath(QFileInfo(p.audioPath).fileName());
                if(QFileInfo::exists(path))p.audioPath=path;
            }
            auto dict=QDir(directory).filePath(QFileInfo(p.rules.englishDictionary).fileName());
            if(!p.rules.englishDictionary.isEmpty()&&QFileInfo::exists(dict))p.rules.englishDictionary=dict;
        });
    }
    void Window::addCustom() {
        bool ok=false;
        auto id=QInputDialog::getText(this,trText("Custom Shape"),trText("Enter shape ID"),QLineEdit::Normal,{},&ok);
        if(!ok||id.trimmed().isEmpty())return;
        auto path=QFileDialog::getOpenFileName(this,trText("Import PNGs"),{},"PNG (*.png)");
        if(!path.isEmpty())change(trText("Custom Shape"),[=](Project&p) {
            p.assets[id.trimmed()]=path;
        });
    }
    void Window::attachAudio(){
        auto path=QFileDialog::getOpenFileName(this,trText("Attach Audio"),{},"Audio (*.wav *.flac *.mp3 *.ogg *.m4a);;All (*)");
        if(!path.isEmpty())attachAudioPath(path);
    }
    void Window::attachAudioPath(const QString&path){
        if(busy)return;stopPlayback();cancel=std::make_shared<std::atomic_bool>(false);auto token=cancel;QString ffmpeg=project.output.ffmpeg;
        setBusy(true);audioWatcher.setFuture(QtConcurrent::run([path,ffmpeg,token]{return probeAudio(path,ffmpeg,*token);}));
    }
    void Window::editSelection() {
        auto ids=timeline->selectedIds();
        if(ids.isEmpty()) {
            error(trText("No event selected"));
            return;
        }
        double a=start->value(),b=end->value();
        QString s=shape->currentText(),anch=anchor->currentData().toString();
        bool locked=lock->isChecked();
        change(trText("Apply to selection"),[=](Project&p) {
            auto events=p.effective();
            for(auto&e:events)if(ids.contains(e.id)) {
                if(ids.size()==1) {
                    e.start=a;
                    e.end=b;
                }
                e.shape=s;
                e.unknown=s=="unknown";
                p.edit(e,locked,anch);
            }
        });
    }
    void Window::splitSelection() {
        auto ids=timeline->selectedIds();
        if(ids.size()!=1) {
            error(trText("Select one event"));
            return;
        }
        double cursor=playTime-project.output.syncOffset;
        change(trText("Split at cursor"),[=](Project&p) {
            for(const auto&e:p.effective())if(e.id==ids[0]) {
                p.split(e,cursor);
                break;
            }
        });
    }
    void Window::mergeSelection() {
        auto ids=timeline->selectedIds();
        change(trText("Merge selection"),[=](Project&p) {
            QVector<Event>events;
            for(const auto&e:p.effective())if(ids.contains(e.id))events.append(e);
            p.merge(events);
        });
    }
    void Window::offsetSelection() {
        auto ids=timeline->selectedIds();
        if(ids.isEmpty()) {
            error(trText("No event selected"));
            return;
        }
        bool ok=false;
        double offset=QInputDialog::getDouble(this,trText("Batch offset"),trText("Offset in seconds"),0,-21600,21600,6,&ok);
        if(!ok)return;
        QString anch=anchor->currentData().toString();
        bool locked=lock->isChecked();
        change(trText("Batch offset"),[=](Project&p) {
            for(auto e:p.effective())if(ids.contains(e.id)) {
                e.start+=offset;
                e.end+=offset;
                p.edit(e,locked,anch);
            }
        });
    }
    void Window::correctReading() {
        auto ids=timeline->selectedIds();
        if(ids.isEmpty()) {
            error(trText("No event selected"));
            return;
        }
        QString source;
        for(const auto&e:project.effective())if(ids.contains(e.id)) {
            source=e.source.section(';',0,0);
            break;
        }
        bool ok=false;
        auto reading=QInputDialog::getText(this,trText("Correct pronunciation"),trText("Explicit pronunciation for source note"),QLineEdit::Normal,project.rules.readings.value(source),&ok);
        if(!ok)return;
        change(trText("Correct pronunciation"),[=](Project&p) {
            if(reading.trimmed().isEmpty())p.rules.readings.remove(source);
            else p.rules.readings[source]=reading.trimmed();
        });
        regenerate();
    }
    void Window::resetOverrides() {
        change(trText("Reset manual edits"),[](Project&p) {
            p.overrides.clear();
        });
    }
    void Window::seek(double t) {
        playTime=std::clamp(t,0.,21600.);
        if(playing) {
            playOrigin=playTime;
            playbackClock.restart();
            audioStarted=false;
        }
        syncAudio();
        refreshPreview();
        followCursor(true);
    }
    void Window::togglePlayback() {
        if(busy)return;
        if(playing) {
            playTime=std::min(project.duration(),playOrigin+playbackClock.elapsed()/1000.);
            stopPlayback();
            if(preferences.returnOnPause)playTime=project.playbackReturnPosition;
            syncAudio();refreshPreview();followCursor(true);
            return;
        }
        if(playTime>=project.duration())playTime=0;
        timeline->finishSubtitleEditing(false);
        playing=true;
        playOrigin=playTime;
        playbackClock.restart();
        audioStarted=false;
        syncAudio();
        playbackTimer.start();
        refreshPreview();
    }
    void Window::stopPlayback() {
        playing=false;
        playbackTimer.stop();
        if(player)player->pause();
        audioStarted=false;
        if(preview&&scene)refreshPreview();
    }
    void Window::ensureAudio() {
        if(player)return;
        player=std::make_unique<QMediaPlayer>();
        audioOutput=std::make_unique<QAudioOutput>();
        player->setAudioOutput(audioOutput.get());
        audioOutput->setVolume(.7);
        connect(player.get(),&QMediaPlayer::errorOccurred,this,[this](QMediaPlayer::Error,const QString&message) {
            statusBar()->showMessage(trText("Audio")+": "+message);
        });
    }
    void Window::syncAudio() {
        if(project.audioPath.isEmpty())return;
        if(!playing&&!player)return;
        ensureAudio();
        auto url=QUrl::fromLocalFile(project.audioPath);
        if(player->source()!=url)player->setSource(url);
        double positionSeconds=playTime-project.output.audioOffset;
        if(positionSeconds<0) {
            if(player)player->pause();
            audioStarted=false;
            return;
        }
        if(!playing) {
            player->setPosition(qRound64(positionSeconds*1000));
            return;
        }
        if(!audioStarted) {
            player->setPosition(qRound64(positionSeconds*1000));
            player->play();
            audioStarted=true;
        }
    }
    void Window::resizeEvent(QResizeEvent*e) {
        QMainWindow::resizeEvent(e);
        if(preview)QTimer::singleShot(0,this,[this] {
            refreshPreview();
        });
    }
    void Window::closeEvent(QCloseEvent*e) {
        if(busy) {
            if(cancel)cancel->store(true);
            statusBar()->showMessage(trText("Cancelled"));
            e->ignore();
            return;
        }
        if(confirmDiscard())e->accept();
        else e->ignore();
    }
    void Window::showExportSettings() {
        QDialog dialog(this);
        dialog.setWindowTitle(trText("Export Settings"));
        auto layout=new QVBoxLayout(&dialog);
        auto form=new QFormLayout;
        layout->addLayout(form);
        auto s=project.output;
        auto number=[&](const char*label,int value,int min,int max) {
            auto spin=new QSpinBox;
            spin->setRange(min,max);
            spin->setValue(value);
            form->addRow(trText(label),spin);
            return spin;
        };
        auto real=[&](const char*label,double value,double min,double max) {
            auto spin=new QDoubleSpinBox;
            spin->setRange(min,max);
            spin->setDecimals(6);
            spin->setValue(value);
            form->addRow(trText(label),spin);
            return spin;
        };

        auto fps=new QLineEdit(QString("%1/%2").arg(s.fpsNum).arg(s.fpsDen));
        form->addRow(trText("Frame rate NUM/DEN"),fps);
        auto format=new QComboBox;
        format->addItems( {
            "mp4","webm","mov"
        });
        format->setCurrentText(s.format);
        form->addRow(trText("Format"),format);
        auto crf=number("Quality CRF",s.crf,0,51);
        auto bitrate=number("Video kbps (0 = CRF)",s.bitrateKbps,0,500000);

        auto sync=real("Animation offset (seconds)",s.syncOffset,-21600,21600);
        auto offset=real("Audio offset (seconds)",s.audioOffset,-21600,21600);
        auto ffmpeg=new QLineEdit(s.ffmpeg);
        form->addRow(trText("FFmpeg executable"),ffmpeg);
        auto estimate=new QLabel;
        layout->addWidget(estimate);
        auto updateEstimate=[&] {
            if(bitrate->value()==0)estimate->setText(trText("Quality mode: size varies"));
            else estimate->setText(trText("Estimated size")+QString(": ~%1 MB\n").arg(bitrate->value()*(project.duration())/8000.,0,'f',2)+trText("Size estimate excludes audio/container"));
        };
        connect(bitrate,&QSpinBox::valueChanged,&dialog,updateEstimate);

        updateEstimate();
        auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Cancel)->setText(trText("Cancel"));
        layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,[&] {
            try {
                auto rate=fps->text().split('/');
                bool a=false,b=true;
                if(rate.size()>2)throw Failure("Invalid frame rate");
                s.fpsNum=rate[0].toInt(&a);
                s.fpsDen=rate.size()==2?rate[1].toInt(&b):1;
                if(!a||!b)throw Failure("Invalid frame rate");
                s.format=format->currentText();
                s.crf=crf->value();
                s.bitrateKbps=bitrate->value();
                s.syncOffset=sync->value();
                s.audioOffset=offset->value();
                s.ffmpeg=ffmpeg->text();
                Project next=project;
                next.output=s;
                if(!next.score.raw.isEmpty())validateExport(next);
                change(trText("Export Settings"),[=](Project&p) {
                    p.output=s;
                });
                dialog.accept();
            }
            catch(const std::exception&e) {
                error(QString::fromUtf8(e.what()));
            }
        });
        dialog.exec();
    }
    void Window::startExport() {
        if(busy)return;
        if(project.subtitlesEnabled&&project.output.duration>0){
            double end=project.output.duration;const auto sources=subtitleSources(project);
            for(const auto&t:project.subtitles)if(t.enabled)for(const auto&c:t.cues)end=std::max(end,subtitleInterval(project,c,&sources).end);
            if(end>project.output.duration){auto answer=QMessageBox::question(this,trText("Subtitles"),trText("Some subtitles exceed the fixed duration. Extend before exporting? No exports only the chosen duration."),QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel);
                if(answer==QMessageBox::Cancel)return;
                if(answer==QMessageBox::Yes)changeSubtitles(trText("Animation duration"),[end](Project&p){p.output.duration=end;});
            }
        }
        try {
            validateExport(project);
        }
        catch(const std::exception&e) {
            error(QString::fromUtf8(e.what()));
            return;
        }
        auto path=QFileDialog::getSaveFileName(this,trText("Export Video"),{},QString("%1 (*.%1)").arg(project.output.format),nullptr,QFileDialog::DontConfirmOverwrite);
        if(path.isEmpty())return;
        if(!path.endsWith("."+project.output.format,Qt::CaseInsensitive))path+="."+project.output.format;
        // One explicit overwrite decision, before encoding begins.
        bool overwrite=false;
        if(QFileInfo::exists(path)) {
            auto reply=QMessageBox::question(this,trText("Overwrite"),trText("Replace existing file?"),QMessageBox::Yes|QMessageBox::No);
            if(reply!=QMessageBox::Yes)return;
            overwrite=true;
        }
        activePath=path;
        cancel=std::make_shared<std::atomic_bool>(false);
        auto token=cancel;
        auto snapshot=project;
        setBusy(true);
        exportWatcher.setFuture(QtConcurrent::run([this,snapshot,path,token,overwrite] {
            auto report=[this](int n) {
                QMetaObject::invokeMethod(this,[this,n] {
                    progress->setValue(n);
                },Qt::QueuedConnection);
            };
            return exportVideo(snapshot,path,*token,report,overwrite);
        }));
    }
    bool Window::runSmokeWorkflow(const QString&directory) {
        try {
            // Original synthetic content, kept separate from private user projects.
            QByteArray source=R"({"version":196,"time":{"tempo":[{"position":0,"bpm":120}],"meter":[{"index":0,"numerator":4,"denominator":4}]},"tracks":[{"name":"Original smoke","mainGroup":{"uuid":"smoke","notes":[{"uuid":"a","onset":0,"duration":705600000,"pitch":60,"lyrics":"あ","phonemes":"a"},{"uuid":"i","onset":705600000,"duration":705600000,"pitch":60,"lyrics":"い","phonemes":"i"}]},"mainRef":{"database":{"language":"japanese"}}}]})";
            Project p;
            p.score=parseSvp(source);
            p.selected= {
                p.score.tracks[0].id
            };
            p.canvas.width=64;
            p.canvas.height=64;
            p.regenerate();
            createDemoAssets(directory);
            for(auto id:QStringList {
                "A","I","U","E","O","closed","rest","breath","unknown"
            })p.assets[id]=QDir(directory).filePath(id+".png");
            assignProject(p);
            history.clear();
            auto first=project.generated[0];
            timeline->selectIds( {
                first.id
            });
            shape->setCurrentText("O");
            editSelection();
            if(project.effective()[0].shape!="O")return false;
            history.undo();
            if(project.effective()[0].shape!="A")return false;
            history.redo();
            if(project.effective()[0].shape!="O")return false;
            project.regenerate();
            if(project.effective()[0].shape!="O")return false;
            auto path=QDir(directory).filePath("smoke.chosuta");
            saveProject(project,path);
            assignProject(loadProject(path));
            if(project.effective()[0].shape!="O")return false;
            std::atomic_bool token=false;
            ExportResult result;
            result.success=true;
            if((QFileInfo(resolveExecutable("ffmpeg")).isAbsolute()||!QStandardPaths::findExecutable("ffmpeg").isEmpty()))result=exportVideo(project,QDir(directory).filePath("smoke.mp4"),token);
            if(!result.success) {
                qWarning()<<result.error;
                return false;
            }
            for(const auto&locale:QStringList {
                "zh","en","ja"
            }) {
                uiLanguage=locale;
                buildUi();
                refresh();
                if(tabs->tabText(0)!=trText("Tracks"))return false;
            }
            history.setClean();
            uiLanguage="zh";
            buildUi();
            refresh();
            return true;
        }
        catch(const std::exception&e) {
            qWarning()<<e.what();
            return false;
        }
    }
}
