// SPDX-License-Identifier: GPL-3.0-or-later
#include "text.h"
namespace chosuta {
    QString uiText(const QString &uiLanguage,const char*key,const char *zh,const char *ja) {
        if(zh&&ja)return QString::fromUtf8(uiLanguage=="zh"?zh:uiLanguage=="ja"?ja:key);
        struct Entry {
            const char*en;
            const char*zh;
            const char*ja;
        };
        static const Entry entries[]= {
            {"Scores / Chosuta (*.svp *.ust *.vsqx *.chosuta)","乐谱 / Chosuta (*.svp *.ust *.vsqx *.chosuta)","楽譜 / Chosuta (*.svp *.ust *.vsqx *.chosuta)"},
            {"Default FFmpeg (this computer)","默认 FFmpeg（本机）","既定の FFmpeg（このPC）"},
            {"Override for this project","为本工程单独指定","このプロジェクトで個別指定"},
            {"Browse…","浏览…","参照…"},
            {"Check media tools","检测媒体工具","メディアツールを確認"},
            {"Checking…","正在检测…","確認中…"},
            {"Automatic: bundled tools, then PATH","自动：内置工具，然后系统 PATH","自動：同梱ツール、次に PATH"},
            {"Choose ffmpeg; ffprobe and shared DLLs should stay beside it. The default is saved only on this computer when you press OK.","选择 ffmpeg；ffprobe 和共享 DLL 请保留在其旁边。点击确定后，本机默认路径才会保存。","ffmpeg を選択し、ffprobe と共有 DLL は同じ場所に置いてください。既定のパスは OK でこの PC に保存されます。"},
            {"Tool startup and listed capabilities passed. Export/playback still require a real media test.","工具启动及所列能力检查通过；实际导出和播放仍需媒体测试。","ツールの起動と一覧の機能を確認しました。書き出し・再生は実際のメディアで確認してください。"},
            {"Missing encoders","缺少编码器","不足するエンコーダー"},
            {"Missing filters","缺少滤镜","不足するフィルター"},
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
            {"Also save as defaults for new score imports","同时保存为新导入乐谱的默认设置","新しい楽譜の既定設定としても保存"},
            {"Dictionary data: %1 / 3,000,000 bytes","词典数据：%1 / 3,000,000 字节","辞書データ：%1 / 3,000,000 バイト"},
            {"Custom readings override built-in readings. Note corrections and explicit source phonemes keep priority. Regenerate to apply.","自定义读音优先于内置读音。逐音符人工修正和来源显式音素保持优先。保存后重新生成口形即可应用。","独自の読みは内蔵の読みより優先されます。ノートの修正とソースの明示音素は最優先です。保存後に再生成してください。"},
            {"Japanese uses kana by default. This small word table cannot guarantee kanji readings; unknown words still need correction. Explicit custom readings work even with this option off.","日文默认以假名为准。小型词表不能保证汉字读音准确；未知词仍需修正。关闭此选项也可以使用您明确添加的自定义读音。","日本語は仮名を基本とします。小さな語彙表では漢字の読みの正確さを保証できません。未知の語は修正が必要です。無効でも登録した独自の読みを使えます。"},
            {"Settings","设置","設定"},
            {"Preferences","偏好设置","環境設定"},
            {"System language","跟随系统","システム言語"},
            {"Interface language","界面语言","表示言語"},
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
                "Open score / Project","打开乐谱 / 工程","楽譜 / プロジェクトを開く"
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
                "Play / Stop","播放 / 停止","再生 / 停止"
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
                "Open an SVP, UST or VSQX, select tracks, generate, import PNGs, edit the timeline, save, and export. Drag event edges to resize; drag the middle to move. Ctrl-click selects multiple events. Ctrl-wheel zooms. Priority: lower number wins. Corrections are estimates; unknown words remain marked. Demo assets are original test drawings.","打开 SVP／UST／VSQX → 勾选轨道 → 生成口形 → 导入 PNG → 编辑时间轴 → 保存 → 导出。拖动边缘调整边界，拖动中部移动，Ctrl 点击多选，Ctrl 滚轮缩放。优先级数值较小者优先。自动结果均为时序估计，未知读音保持待处理标记。测试素材为原创几何图形。","SVP／UST／VSQX を開く → トラック選択 → 生成 → PNG 読み込み → タイムライン編集 → 保存 → 書き出し。端をドラッグして長さを変更、中央をドラッグして移動。Ctrl クリックで複数選択、Ctrl ホイールでズーム。小さい優先度の値を優先。自動結果は推定で、未知の読みは要確認として表示されます。テスト素材はオリジナルの図形です。"
            }
        };
        for(const auto&e:entries)if(qstrcmp(e.en,key)==0)return QString::fromUtf8(uiLanguage=="zh"?e.zh:uiLanguage=="ja"?e.ja:e.en);
        return QString::fromUtf8(key);
    }
}
