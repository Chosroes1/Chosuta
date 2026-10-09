// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include "core/appearance.h"
#include "core/intervals.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QImageReader>
namespace chosuta {
    void Window::showMouthSettings() {
        if(busy||mouthSettingsOpen)return;
        QScopedValueRollback<bool> settingsGuard(mouthSettingsOpen,true);
        stopPlayback();
        auto t=[this](const char *en,const char *zh,const char *ja){
            return QString::fromUtf8(uiLanguage=="zh"?zh:uiLanguage=="ja"?ja:en);
        };
        QDialog dialog(this);
        dialog.setObjectName("mouthSettingsDialog");
        dialog.setWindowTitle(trText("Advanced variant settings"));
        dialog.resize(860,650);
        Project draft=project;
        auto root=new QVBoxLayout(&dialog);
        auto enabled=new QCheckBox(trText("Enable advanced variants"));
        enabled->setObjectName("mouthEnabled");
        enabled->setChecked(draft.appearance.enabled);
        root->addWidget(enabled);
        auto pages=new QTabWidget;
        pages->setObjectName("mouthSettingsPages");
        root->addWidget(pages);
        auto mouthPage=new QWidget;
        auto mouthLayout=new QVBoxLayout(mouthPage);
        pages->addTab(mouthPage,t("Variant images","差分素材","差分画像"));
        auto selectors=new QHBoxLayout;
        mouthLayout->addLayout(selectors);
        auto directionOn=new QCheckBox(t("Direction","方向","方向"));
        directionOn->setObjectName("mouthDirectionEnabled");
        directionOn->setChecked(draft.appearance.direction);
        auto intervalOn=new QCheckBox(t("Interval","幅度","音程"));
        intervalOn->setObjectName("mouthIntervalEnabled");
        intervalOn->setChecked(draft.appearance.interval);
        auto beatOn=new QCheckBox(t("Beat","强弱拍","拍の強弱"));
        beatOn->setObjectName("mouthBeatEnabled");
        beatOn->setChecked(draft.appearance.beat);
        auto directionChoice=new QComboBox;
        directionChoice->setObjectName("mouthDirectionChoice");
        auto intervalChoice=new QComboBox;
        intervalChoice->setObjectName("mouthIntervalChoice");
        auto beatChoice=new QComboBox;
        beatChoice->setObjectName("mouthBeatChoice");
        for(auto choice:{
            directionChoice,intervalChoice,beatChoice
        })choice->addItem(t("Any / shared","通用 / 不区分","共通 / 区別しない"),"any");
        directionChoice->addItem(t("Up","上行","上行"),"up");
        directionChoice->addItem(t("Down","下行","下行"),"down");
        intervalChoice->addItem(t("Step","级进","順次進行"),"step");
        intervalChoice->addItem(t("Small leap","小跳","小さな跳躍"),"small");
        intervalChoice->addItem(t("Large leap","大跳","大きな跳躍"),"large");
        directionChoice->addItem(t("Repeated note","同音反复","同音反復"),"repeat");
        directionChoice->setItemData(directionChoice->findData("repeat"),t("For repeated notes, select Any / shared in the interval selector.","同音反复请将幅度选为“通用 / 不区分”。没有对应图时自动使用通用图，闭口、休息、呼吸也适用。","同音反復では音程を「共通 / 区別しない」にしてください。"),Qt::ToolTipRole);
        beatChoice->addItem(t("Strong","强拍","強拍"),"strong");
        beatChoice->addItem(t("Weak","弱拍","弱拍"),"weak");
        for(auto choice:{
            directionChoice,intervalChoice,beatChoice
        })choice->setCurrentIndex(1);
        auto addSelectorGroup=[&](QCheckBox *on,QComboBox *choice,const QString &name){
            auto group=new QWidget;
            group->setObjectName(name);
            auto layout=new QHBoxLayout(group);
            layout->setContentsMargins(0,0,0,0);
            layout->setSpacing(6);
            on->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Preferred);
            layout->addWidget(on);
            layout->addWidget(choice,1);
            selectors->addWidget(group,1);
        };
        selectors->setSpacing(20);
        addSelectorGroup(directionOn,directionChoice,"mouthDirectionGroup");
        addSelectorGroup(intervalOn,intervalChoice,"mouthIntervalGroup");
        addSelectorGroup(beatOn,beatChoice,"mouthBeatGroup");
        auto hint=new QLabel(t("Selectors choose the combination to edit. Playback classifies each note from the score. Uncheck a dimension to use shared images. For repeated notes, select Any / shared for the interval. Missing variants use shared images, including closed, rest and breath.",
        "选择按钮只切换正在编辑的组合；播放按工程逐音判断。取消某维勾选后使用该维的通用图。同音反复请将幅度选为“通用 / 不区分”。没有对应图时自动使用通用图，闭口、休息、呼吸也适用。","選択欄は編集する組合せを切り替えます。再生時は譜面から音符ごとに判定します。無効な次元には共通画像を使います。同音反復では音程を「共通 / 区別しない」にしてください。閉口・休止・ブレスも、対応画像がなければ共通画像を使います。"));
        hint->setObjectName("mouthSelectorHint");
        hint->setWordWrap(true);
        mouthLayout->addWidget(hint);
        auto summary=new QLabel;
        summary->setObjectName("mouthCombinationSummary");
        mouthLayout->addWidget(summary);
        auto states=new QTabWidget;
        states->setObjectName("mouthStatePages");
        mouthLayout->addWidget(states);
        QMap<QString,QToolButton *> shapeButtons;
        auto group=new QButtonGroup(&dialog);
        group->setExclusive(true);
        const QStringList vowels={
            "A","I","U","E","O"
        },specials={
            "closed","rest","breath"
        };
        auto shapeName=[&](const QString &shape){
            return shape=="closed"?t("Closed","闭口","閉口"):shape=="rest"?t("Rest","休息","休止"):shape=="breath"?t("Breath","呼吸","ブレス"):shape;
        };
        auto addSlots=[&](const QStringList &shapes,const QString &title){
            auto scroll=new QScrollArea;
            scroll->setWidgetResizable(true);
            auto widget=new QWidget;
            auto layout=new QHBoxLayout(widget);
            for(const auto &shape:shapes){
                auto button=new QToolButton;
                button->setObjectName("mouthSlot"+shape);
                button->setCheckable(true);
                button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
                button->setIconSize({
                    88,88
                });
                button->setMinimumSize(105,130);
                layout->addWidget(button);
                shapeButtons[shape]=button;
                group->addButton(button);
            }
            layout->addStretch();
            scroll->setWidget(widget);
            states->addTab(scroll,title);
        };
        addSlots(vowels,t("Vowels","发声口形","母音"));
        addSlots(specials,t("Special states","特殊口形","特殊な口形"));
        QString selected="A";
        shapeButtons[selected]->setChecked(true);
        auto pathRow=new QHBoxLayout;
        mouthLayout->addLayout(pathRow);
        auto path=new QLineEdit;
        path->setObjectName("mouthImagePath");
        path->setPlaceholderText(t("PNG path for the selected slot","选中槽位的 PNG 路径","選択した枠の PNG パス"));
        pathRow->addWidget(path);
        auto browse=new QPushButton(t("Choose PNG","选择 PNG","PNG を選択"));
        pathRow->addWidget(browse);
        auto assign=new QPushButton(t("Import into slot","导入此槽","この枠に読込"));
        assign->setObjectName("mouthAssignImage");
        pathRow->addWidget(assign);
        auto clear=new QPushButton(t("Clear slot","清除此槽","この枠をクリア"));
        clear->setObjectName("mouthClearImage");
        pathRow->addWidget(clear);
        auto mouthStatus=new QLabel;
        mouthStatus->setWordWrap(true);
        mouthStatus->setObjectName("mouthImageStatus");
        mouthLayout->addWidget(mouthStatus);
        mouthLayout->addStretch();
        auto currentContext=[&]{
            return AppearanceContext{
                directionOn->isChecked()?directionChoice->currentData().toString():QString("any"),intervalOn->isChecked()?intervalChoice->currentData().toString():QString("any"),
                beatOn->isChecked()?beatChoice->currentData().toString():QString("any"),{
                }
            };
        };
        auto currentId=[&]{
            return appearanceId(selected,currentContext());
        };
        auto refreshSlots=[&]{
            const auto c=currentContext();
            summary->setText(t("Editing: %1 · %2 · %3","正在编辑：%1 · %2 · %3","編集中：%1 · %2 · %3").arg(beatOn->isChecked()?beatChoice->currentText():t("Any beat",
            "不区分强弱拍","拍共通"),directionOn->isChecked()?directionChoice->currentText():t("Any direction","不区分方向","方向共通"),intervalOn->isChecked()?intervalChoice->currentText():t("Any interval",
            "不区分幅度","音程共通")));
            const auto &visible=states->currentIndex()==0?vowels:specials;
            for(const auto &shape:visible){
                const auto id=appearanceId(shape,c),file=draft.assets.value(id);
                QImage thumbnail;
                if(!file.isEmpty()){
                    QImageReader reader(file);
                    const auto size=reader.size();
                    if(size.isValid()&&qint64(size.width())*size.height()*4<=256LL*1024*1024){
                        auto target=size;
                        target.scale(88,88,Qt::KeepAspectRatio);
                        reader.setScaledSize(target);
                        thumbnail=reader.read();
                    }
                }
                shapeButtons[shape]->setIcon(thumbnail.isNull()?QIcon():QIcon(QPixmap::fromImage(thumbnail)));
                shapeButtons[shape]->setText(shapeName(shape)+"\n"+(file.isEmpty()?t("Not assigned","未设置","未設定"):thumbnail.isNull()?t("Unreadable","读取失败","読込失敗"):QFileInfo(file).fileName()));
                shapeButtons[shape]->setToolTip(id+".png\n"+file);
            }
            path->setText(draft.assets.value(currentId()));
            AppearanceResolver resolver;
            auto previewProject=draft;
            previewProject.appearance.enabled=true;
            const auto result=resolver.select(previewProject,selected,c);
            mouthStatus->setText(currentId()+".png\n"+(result.missing?t("No matching, shared, or fallback image. Assign an image.",
            "没有可用的匹配图、通用图或全局回退图，请设置图片。","一致する画像・共通画像・代替画像がありません。画像を設定してください。"):t("Resolved image: ","实际使用图片：","使用画像：")+result.id+"\n"+draft.assets.value(result.id)));
        };
        auto updateSelector=[&](QCheckBox *on,QComboBox *choice){
            QSignalBlocker block(choice);
            if(!on->isChecked()){
                if(choice->currentData()!="any")choice->setProperty("lastChoice",choice->currentData());
                choice->setCurrentIndex(0);
            }
            else if(choice->currentIndex()==0){
                const auto old=choice->property("lastChoice").toString();
                choice->setCurrentIndex(old.isEmpty()?1:choice->findData(old));
            }
            choice->setEnabled(on->isChecked());
        };
        updateSelector(directionOn,directionChoice);
        updateSelector(intervalOn,intervalChoice);
        updateSelector(beatOn,beatChoice);
        for(auto choice:{
            directionChoice,intervalChoice,beatChoice
        })connect(choice,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{
            refreshSlots();
        });
        connect(directionOn,&QCheckBox::toggled,&dialog,[&](bool value){
            draft.appearance.direction=value;
            updateSelector(directionOn,directionChoice);
            refreshSlots();
        });
        connect(intervalOn,&QCheckBox::toggled,&dialog,[&](bool value){
            draft.appearance.interval=value;
            updateSelector(intervalOn,intervalChoice);
            refreshSlots();
        });
        connect(beatOn,&QCheckBox::toggled,&dialog,[&](bool value){
            draft.appearance.beat=value;
            updateSelector(beatOn,beatChoice);
            refreshSlots();
        });
        connect(enabled,&QCheckBox::toggled,&dialog,[&](bool value){
            draft.appearance.enabled=value;
        });
        connect(states,&QTabWidget::currentChanged,&dialog,[&](int index){
            selected=index==0?"A":"closed";
            shapeButtons[selected]->setChecked(true);
            refreshSlots();
        });
        for(auto it=shapeButtons.cbegin();it!=shapeButtons.cend();++it)connect(it.value(),&QToolButton::clicked,&dialog,[&,shape=it.key()]{
            selected=shape;
            refreshSlots();
        });
        connect(browse,&QPushButton::clicked,&dialog,[&]{
            const auto file=QFileDialog::getOpenFileName(&dialog,t("Choose PNG","选择 PNG","PNG を選択"),path->text(),"PNG (*.png)");
            if(!file.isEmpty())path->setText(file);
        });
        connect(assign,&QPushButton::clicked,&dialog,[&]{
            const auto file=QFileInfo(path->text()).absoluteFilePath();
            QImageReader reader(file,"png");
            const auto size=reader.size();
            if(!size.isValid()||qint64(size.width())*size.height()*4>256LL*1024*1024){
                mouthStatus->setText(t("Invalid or oversized PNG","PNG 无效或超过单图上限","PNG が無効か画像サイズが上限を超えています"));
                return;
            }
            auto target=size;
            target.scale(88,88,Qt::KeepAspectRatio);
            reader.setScaledSize(target);
            if(reader.read().isNull()){
                mouthStatus->setText(reader.errorString());
                return;
            }
            draft.assets[currentId()]=file;
            refreshSlots();
        });
        connect(clear,&QPushButton::clicked,&dialog,[&]{
            draft.assets.remove(currentId());
            refreshSlots();
        });
        auto consonantPage=new QWidget;
        auto consonantLayout=new QVBoxLayout(consonantPage);
        pages->addTab(consonantPage,t("Consonant rules","辅音规则","子音規則"));
        auto recommended=new QLabel;
        recommended->setObjectName("consonantPresetSummary");
        recommended->setWordWrap(true);
        consonantLayout->addWidget(recommended);
        auto customize=new QToolButton;
        customize->setObjectName("consonantCustomize");
        customize->setText(t("Adjust consonant rules (usually unnecessary)","调整辅音规则（通常无需修改）","子音規則の変更（通常は不要）"));
        customize->setCheckable(true);
        consonantLayout->addWidget(customize);
        auto editor=new QWidget;
        editor->setObjectName("consonantEditor");
        auto editorLayout=new QVBoxLayout(editor);
        consonantLayout->addWidget(editor);
        editor->hide();
        connect(customize,&QToolButton::toggled,editor,&QWidget::setVisible);
        auto filterRow=new QHBoxLayout;
        editorLayout->addLayout(filterRow);
        auto languageChoice=new QComboBox;
        languageChoice->setObjectName("consonantLanguage");
        languageChoice->addItem("English / ARPAbet","en");
        languageChoice->addItem("中文 / 拼音音素","zh");
        languageChoice->addItem("日本語 / romaji","ja");
        filterRow->addWidget(languageChoice);
        auto modifiedOnly=new QCheckBox(t("Modified only","仅显示已修改","変更項目のみ"));
        modifiedOnly->setObjectName("consonantModifiedOnly");
        filterRow->addWidget(modifiedOnly);
        auto table=new QTableWidget(0,4);
        table->setObjectName("consonantTable");
        table->setHorizontalHeaderLabels({
            t("Phoneme","音素","音素"),t("Default","默认规则","既定規則"),t("Current","当前规则","現在の規則"),t("Status","状态","状態")
        });
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        editorLayout->addWidget(table);
        auto resetRow=new QHBoxLayout;
        editorLayout->addLayout(resetRow);
        auto resetOne=new QPushButton(t("Restore selected default","恢复此项默认","選択項目を既定に戻す"));
        resetOne->setObjectName("consonantResetOne");
        resetRow->addWidget(resetOne);
        auto resetAll=new QPushButton(t("Restore all defaults","恢复全部默认","全項目を既定に戻す"));
        resetAll->setObjectName("consonantResetAll");
        resetRow->addWidget(resetAll);
        auto modeName=[&](const QString &mode){
            return mode=="closed"?t("Closed","闭口","閉口"):t("Follow nearby vowel","跟随相邻元音","隣接する母音に合わせる");
        };
        auto status=new QLabel;
        status->setObjectName("mouthSettingsStatus");
        status->setWordWrap(true);
        auto buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);
        buttons->setObjectName("mouthSettingsButtons");
        buttons->button(QDialogButtonBox::Cancel)->setText(trText("Cancel"));
        auto updateConsonantStatus=[&]{
            recommended->setText(t("Recommended default: B/P/M close the lips; other consonants follow nearby vowels. Usually no changes are needed. Movement and expression stay in the same advanced pose.",
            "推荐默认：b/p/m 闭口，其他辅音跟随相邻元音，通常无需修改；动作与神态保留同一高级姿态。","推奨規則：b/p/m は閉口、その他は隣接母音に合わせます。通常は変更不要です。動作と表情は同じ高度な姿勢を保ちます。"));
            const bool changed=draft.rules.consonants!=project.rules.consonants;
            if(!draft.rules.consonants.overrides.isEmpty())recommended->setText(recommended->text()+"\n"+t("Customized entries: %1","自定义项：%1","カスタム項目：%1").arg(draft.rules.consonants.overrides.size()));
            buttons->button(QDialogButtonBox::Apply)->setText(changed?t("Apply and regenerate","应用并重新生成","適用して再生成"):t("Apply","应用","適用"));
            buttons->button(QDialogButtonBox::Apply)->setMinimumWidth(buttons->button(QDialogButtonBox::Apply)->sizeHint().width());
            if(changed)status->setText(t("Consonant changes regenerate events and clear waveform correction. Locked manual edits stay.","修改辅音规则将重新生成并清除旧波形校正，锁定人工修改保留。",
            "子音規則の変更は再生成し、波形補正を解除します。ロックした手動編集は保持します。"));
            else status->clear();
        };
        auto fillConsonants=[&]{
            table->setRowCount(0);
            for(const auto &key:consonantKeys(languageChoice->currentData().toString())){
                if(modifiedOnly->isChecked()&&!draft.rules.consonants.overrides.contains(key))continue;
                const int row=table->rowCount();
                table->insertRow(row);
                const auto base=consonantMode(key),current=consonantMode(key,draft.rules.consonants);
                auto phone=new QTableWidgetItem(key.section(':',2));
                phone->setData(Qt::UserRole,key);
                phone->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
                table->setItem(row,0,phone);
                auto defaultItem=new QTableWidgetItem(modeName(base));
                defaultItem->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
                table->setItem(row,1,defaultItem);
                auto choice=new QComboBox;
                choice->addItem(modeName("open"),"open");
                choice->addItem(modeName("closed"),"closed");
                choice->setCurrentIndex(choice->findData(current));
                table->setCellWidget(row,2,choice);
                auto stateItem=new QTableWidgetItem(current==base?t("Default","默认","既定"):t("Modified","已修改","変更済み"));
                stateItem->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
                table->setItem(row,3,stateItem);
                connect(choice,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&,key,row,choice,base]{
                    const auto value=choice->currentData().toString();
                    if(value==base)draft.rules.consonants.overrides.remove(key);
                    else draft.rules.consonants.overrides[key]=value;
                    table->item(row,3)->setText(value==base?t("Default","默认","既定"):t("Modified","已修改","変更済み"));
                    updateConsonantStatus();
                });
            }
            updateConsonantStatus();
        };
        connect(languageChoice,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{
            fillConsonants();
        });
        connect(modifiedOnly,&QCheckBox::toggled,&dialog,[&]{
            fillConsonants();
        });
        connect(resetOne,&QPushButton::clicked,&dialog,[&]{
            if(table->currentRow()>=0)draft.rules.consonants.overrides.remove(table->item(table->currentRow(),0)->data(Qt::UserRole).toString());
            fillConsonants();
        });
        connect(resetAll,&QPushButton::clicked,&dialog,[&]{
            draft.rules.consonants.overrides.clear();
            fillConsonants();
        });
        consonantLayout->addStretch();
        auto analysisPage=new QScrollArea;
        analysisPage->setWidgetResizable(true);
        auto analysisContent=new QWidget;
        auto analysisLayout=new QFormLayout(analysisContent);
        analysisPage->setWidget(analysisContent);
        pages->addTab(analysisPage,t("Classification","姿态规则","姿勢の規則"));
        auto step=new QSpinBox;
        step->setObjectName("mouthStepMax");
        step->setRange(1,24);
        step->setValue(draft.appearance.stepMax);
        auto small=new QSpinBox;
        small->setObjectName("mouthSmallMax");
        small->setRange(draft.appearance.stepMax+1,48);
        small->setValue(draft.appearance.smallMax);
        const auto semitoneSuffix=t(" semitones"," 半音"," 半音");
        step->setSuffix(semitoneSuffix);
        small->setSuffix(semitoneSuffix);
        auto unit=new QComboBox;
        unit->setObjectName("mouthIntervalUnit");
        unit->addItem(t("Semitones","半音","半音"),"semitones");
        unit->addItem(t("Diatonic degrees","乐理度数","楽理に基づく度数"),"diatonic");
        unit->setCurrentIndex(unit->findData(draft.appearance.intervalUnit));
        analysisLayout->addRow(t("Threshold units","阈值设置方式","閾値の設定方法"),unit);
        auto degreeStep=new QSpinBox;
        degreeStep->setObjectName("mouthStepDegree");
        degreeStep->setRange(2,15);
        degreeStep->setValue(draft.appearance.stepDegreeMax);
        auto degreeSmall=new QSpinBox;
        degreeSmall->setObjectName("mouthSmallDegree");
        degreeSmall->setRange(draft.appearance.stepDegreeMax+1,29);
        degreeSmall->setValue(draft.appearance.smallDegreeMax);
        const auto degreeSuffix=t(" degrees"," 度"," 度");
        degreeStep->setSuffix(degreeSuffix);
        degreeSmall->setSuffix(degreeSuffix);
        auto stepEditor=new QStackedWidget;
        stepEditor->addWidget(step);
        stepEditor->addWidget(degreeStep);
        auto smallEditor=new QStackedWidget;
        smallEditor->addWidget(small);
        smallEditor->addWidget(degreeSmall);
        analysisLayout->addRow(t("Step maximum","级进上限","順次進行の上限"),stepEditor);
        analysisLayout->addRow(t("Small leap maximum","小跳上限","小さな跳躍の上限"),smallEditor);
        auto degreeExtra=new QWidget;
        degreeExtra->setObjectName("mouthDegreeSettings");
        auto degreeLayout=new QVBoxLayout(degreeExtra);
        degreeLayout->setContentsMargins(0,0,0,0);
        auto toneForm=new QFormLayout;
        degreeLayout->addLayout(toneForm);
        auto tonic=new QComboBox;
        tonic->setObjectName("mouthDegreeTonic");
        for(const auto &letter:QStringList{"C","D","E","F","G","A","B"})for(const auto &accidental:QStringList{"b","","#"})tonic->addItem(letter+accidental,letter+accidental);
        if(tonic->findData(draft.appearance.degreeTonic)<0)tonic->addItem(draft.appearance.degreeTonic,draft.appearance.degreeTonic);
        tonic->setCurrentIndex(tonic->findData(draft.appearance.degreeTonic));
        toneForm->addRow(t("Tonic","主音","主音"),tonic);
        auto mode=new QComboBox;
        mode->setObjectName("mouthDegreeMode");
        mode->addItem(t("Natural major / Ionian","自然大调 / 伊奥尼亚","自然長音階 / イオニアン"),"major");
        mode->addItem(t("Natural minor / Aeolian","自然小调 / 爱奥利亚","自然短音階 / エオリアン"),"minor");
        mode->addItem(t("Dorian","多利亚","ドリアン"),"dorian");
        mode->addItem(t("Phrygian","弗里几亚","フリジアン"),"phrygian");
        mode->addItem(t("Lydian","利底亚","リディアン"),"lydian");
        mode->addItem(t("Mixolydian","混合利底亚","ミクソリディアン"),"mixolydian");
        mode->addItem(t("Locrian","洛克里亚","ロクリアン"),"locrian");
        mode->addItem(t("Harmonic minor","和声小调","和声短音階"),"harmonic-minor");
        mode->addItem(t("Melodic minor (ascending scale)","旋律小调（上行音阶）","旋律短音階（上行音階）"),"melodic-minor");
        mode->addItem(t("Harmonic major","和声大调","和声長音階"),"harmonic-major");
        mode->addItem(t("Custom seven-note scale","自定义七声音阶","カスタム七音音階"),"custom");
        mode->setCurrentIndex(mode->findData(draft.appearance.degreeMode));
        toneForm->addRow(t("Mode","调式","旋法"),mode);
        auto customRow=new QWidget;
        auto customLayout=new QHBoxLayout(customRow);
        customLayout->setContentsMargins(0,0,0,0);
        customLayout->addWidget(new QLabel(t("Scale offsets","音阶半音位置","音階の半音位置")));
        auto customScale=new QLineEdit;
        customScale->setObjectName("mouthCustomDegreeScale");
        QStringList offsets;for(int n:draft.appearance.customDegreeScale)offsets.append(QString::number(n));
        customScale->setText(offsets.join(','));
        customLayout->addWidget(customScale,1);
        degreeLayout->addWidget(customRow);
        customRow->setVisible(draft.appearance.degreeMode=="custom");
        auto spellingHint=new QLabel;
        spellingHint->setObjectName("mouthSpellingStatus");
        spellingHint->setWordWrap(true);
        degreeLayout->addWidget(spellingHint);
        auto spellingToggle=new QToolButton;
        spellingToggle->setObjectName("mouthSpellingToggle");
        spellingToggle->setText(t("Adjust written note names","校正音名","音名を補正"));
        spellingToggle->setCheckable(true);
        degreeLayout->addWidget(spellingToggle);
        auto spellingTable=new QTableWidget(0,5);
        spellingTable->setObjectName("mouthSpellingTable");
        spellingTable->setHorizontalHeaderLabels({t("Track","轨道","トラック"),t("Score position","谱面位置","譜面位置"),t("Pitch","音高","音高"),t("Mode spelling","调式音名","旋法による音名"),t("Override (blank = mode)","指定音名（空白按调式）","指定音名（空欄は旋法）")});
        spellingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        spellingTable->setMinimumHeight(160);
        spellingTable->setMaximumHeight(230);
        spellingTable->hide();
        degreeLayout->addWidget(spellingTable);
        QSet<QString> singingSources;
        for(const auto &event:project.generated)if(event.articulation=="vowel"||event.articulation=="extend"||(event.articulation.isEmpty()&&QStringList{"A","I","U","E","O"}.contains(event.shape)))singingSources.insert(event.source);
        auto refreshSpellings=[&](bool rebuild=true){
            const QSignalBlocker block(spellingTable);
            if(rebuild)spellingTable->setRowCount(0);
            int missing=0,total=0;
            auto inferred=draft.appearance;inferred.noteSpellings.clear();
            for(const auto &track:project.score.tracks)if(!track.muted&&project.selected.contains(track.id))for(const auto &note:track.notes)if(!note.muted&&singingSources.contains(note.id)){
                ++total;if(!writtenPitch(draft.appearance,note))++missing;
                if(!rebuild||!spellingToggle->isChecked())continue;
                const int row=spellingTable->rowCount();spellingTable->insertRow(row);
                const auto name=writtenPitch(inferred,note);
                const QStringList values={track.name,project.score.time.beatLabel(note.onset),QString::number(note.pitch),name?name->name:t("Not in scale","非调内音","音階外の音"),draft.appearance.noteSpellings.value(note.id)};
                for(int column=0;column<values.size();++column){auto item=new QTableWidgetItem(values[column]);if(column!=4)item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);spellingTable->setItem(row,column,item);}
                spellingTable->item(row,4)->setData(Qt::UserRole,note.id);
                spellingTable->item(row,4)->setData(Qt::UserRole+1,note.pitch);
            }
            spellingHint->setText(t("%1 / %2 note names unresolved. Enter names such as C#4 or Db4; unmatched spans use shared interval images. No key detection; use natural minor for descending melodic-minor spelling.",
                "%1 / %2 个音名待指定。可填 C#4、Db4；未确定音名的跨度用通用幅度。不自动识别调式；旋律小调下行可选自然小调或逐音指定。",
                "%1 / %2 音の音名が未指定です。C#4、Db4 などを入力し、未確定の音程は共通画像を使います。旋法は自動判定しません。旋律短音階の下行には自然短音階または個別指定を使います。").arg(missing).arg(total));
        };
        connect(spellingToggle,&QToolButton::toggled,&dialog,[&](bool value){spellingTable->setVisible(value);refreshSpellings();});
        connect(spellingTable,&QTableWidget::itemChanged,&dialog,[&](QTableWidgetItem *item){
            if(item->column()!=4)return;
            const auto id=item->data(Qt::UserRole).toString(),text=item->text().trimmed();
            if(text.isEmpty())draft.appearance.noteSpellings.remove(id);
            else {
                const auto parsed=parseWrittenPitch(text);
                if(!parsed||parsed->midi!=item->data(Qt::UserRole+1).toInt()){
                    draft.appearance.noteSpellings[id]=text;
                    spellingHint->setText(t("Written name must match the note pitch; fix it before applying.","指定音名必须与当前音高一致，请修正后再应用。","指定音名は現在の音高と一致する必要があります。修正してから適用してください。"));
                    return;
                }
                draft.appearance.noteSpellings[id]=parsed->name;
            }
            status->clear();
            refreshSpellings(false);
        });
        auto readCustomScale=[&]{
            QVector<int> values;
            for(const auto &field:customScale->text().split(QRegularExpression("[,，\\s]+"),Qt::SkipEmptyParts)){bool ok=false;const int n=field.toInt(&ok);if(!ok)throw Failure(t("Invalid scale offsets","音阶位置无效","音階の位置が無効です"));values.append(n);}
            if(values.size()!=7||values.front()!=0)throw Failure(t("Enter seven ascending offsets from 0 to 11, starting at zero.","请填写以 0 开头的七个递增半音位置（0–11）。","0 から始まる昇順の七つの半音位置（0–11）を入力してください。"));
            for(int i=0;i<values.size();++i)if(values[i]<0||values[i]>11||(i&&values[i]<=values[i-1]))throw Failure(t("Scale offsets must ascend within 0–11.","音阶半音位置须在 0–11 内递增。","音階の半音位置は 0–11 の範囲で昇順にしてください。"));
            return values;
        };
        connect(customScale,&QLineEdit::editingFinished,&dialog,[&]{try{draft.appearance.customDegreeScale=readCustomScale();status->clear();refreshSpellings();}catch(const std::exception &error){status->setText(QString::fromUtf8(error.what()));}});
        connect(tonic,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{draft.appearance.degreeTonic=tonic->currentData().toString();status->clear();refreshSpellings();});
        connect(mode,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{draft.appearance.degreeMode=mode->currentData().toString();customRow->setVisible(draft.appearance.degreeMode=="custom");status->clear();refreshSpellings();});
        analysisLayout->addRow(degreeExtra);
        auto showUnit=[&]{
            const int index=unit->currentData()=="diatonic"?1:0;
            stepEditor->setCurrentIndex(index);smallEditor->setCurrentIndex(index);degreeExtra->setVisible(index==1);
        };
        showUnit();refreshSpellings();
        connect(unit,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{draft.appearance.intervalUnit=unit->currentData().toString();showUnit();});
        connect(step,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&](int value){draft.appearance.stepMax=value;small->setMinimum(value+1);});
        connect(small,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&](int value){draft.appearance.smallMax=value;});
        connect(degreeStep,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&](int value){draft.appearance.stepDegreeMax=value;degreeSmall->setMinimum(value+1);});
        connect(degreeSmall,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&](int value){draft.appearance.smallDegreeMax=value;});
        auto intervalHint=new QLabel(t("Diatonic degrees count written letter positions across octaves. Repeated notes are a direction choice and use the shared interval; altered unisons also use the shared interval. Each mode keeps its own thresholds; direction follows score pitch. Tone and spellings are saved in this project.",
            "乐理度数按音名字母及八度计算。同音反复在方向栏设置，使用通用幅度；增减一度也用通用幅度。两种判定各保留阈值，方向仍按谱面音高；调式和音名保存到当前工程。",
            "度数は音名とオクターブから数えます。同音反復は方向欄で設定し、音程共通の画像を使います。増減1度も音程共通です。両方式の閾値を保持し、方向は譜面の音高を使います。旋法と音名は現在のプロジェクトに保存します。"));
        intervalHint->setObjectName("mouthIntervalUnitHint");intervalHint->setWordWrap(true);analysisLayout->addRow(intervalHint);
        auto addAnchor=[&](const QString &label,const QString &name,QString &value){
            auto choice=new QComboBox;
            choice->setObjectName(name);
            choice->addItem(t("Previous singing note","前一发声音符","前の発声音符"),"previous");
            choice->addItem(t("Next singing note","后一发声音符","次の発声音符"),"next");
            choice->addItem(t("Neutral","通用姿态","共通姿勢"),"neutral");
            choice->setCurrentIndex(choice->findData(value));
            analysisLayout->addRow(label,choice);
            auto target=&value;
            connect(choice,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[choice,target]{
                *target=choice->currentData().toString();
            });
        };
        addAnchor(t("Closure / cl pose","促音 / 独立闭口姿态","促音 / 単独閉口の姿勢"),"mouthClosureAnchor",draft.appearance.closureAnchor);
        addAnchor(t("Breath pose","呼吸姿态","ブレスの姿勢"),"mouthBreathAnchor",draft.appearance.breathAnchor);
        addAnchor(t("Rest pose","休息姿态","休止の姿勢"),"mouthRestAnchor",draft.appearance.restAnchor);
        auto accents=new QTableWidget(0,2);
        accents->setObjectName("mouthAccentTemplates");
        accents->setHorizontalHeaderLabels({
            t("Meter","拍号","拍子"),t("Strong positions (blank = default)","强拍位置（空白用默认）","強拍位置（空欄は既定）")
        });
        accents->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        analysisLayout->addRow(accents);
        QSet<QString> signatures;
        for(const auto &meter:project.score.time.meters)signatures.insert(QString("%1/%2").arg(meter.numerator).arg(meter.denominator));
        for(const auto &key:draft.appearance.strongBeats.keys())signatures.insert(key);
        auto sorted=signatures.values();
        sorted.sort();
        for(const auto &key:sorted){
            const int row=accents->rowCount();
            accents->insertRow(row);
            auto item=new QTableWidgetItem(key);
            item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
            accents->setItem(row,0,item);
            QStringList values;
            for(int n:draft.appearance.strongBeats.value(key))values.append(QString::number(n));
            accents->setItem(row,1,new QTableWidgetItem(values.join(',')));
        }
        auto analysisHint=new QLabel(t("Incoming changes compare the preceding singing note in the same instance. First notes use shared movement; repeated notes have their own class. Beats use original positions. Default strong beats: 4/4 = 1,3; compound = group starts; others = 1.",
        "入向变化比较同实例前一发声音符，首音用通用幅度、同音反复独立分类；强弱拍按原谱起点。默认强拍：4/4 为 1、3，复合拍为分组首拍，其他拍号为 1。","同じ参照内の前の発声音符と比較します。最初は共通、同音反復は独立分類です。拍は元の位置を使います。既定の強拍：4/4 は 1,3、複合拍は各群の先頭、その他は 1。"));
        analysisHint->setWordWrap(true);
        analysisLayout->addRow(analysisHint);
        auto importPage=new QWidget;
        auto importLayout=new QVBoxLayout(importPage);
        pages->addTab(importPage,t("Folder import","文件夹导入","フォルダー読込"));
        auto importHint=new QLabel(t("Names: A_strong_up_step.png, closed_strong_up_step.png. any means shared. Simple names A.png, closed.png, etc. are accepted. Only the selected folder is scanned; existing mappings are unchecked by default.",
        "命名：A_strong_up_step.png、closed_strong_up_step.png；any 表示通用。也接受 A.png、closed.png 等简单名称。只扫描所选文件夹一层，替换已有映射默认不勾选。","命名：A_strong_up_step.png、closed_strong_up_step.png。any は共通です。A.png、closed.png 等も使用できます。選択したフォルダーのみ確認し、既存の置換は既定では選択しません。"));
        importHint->setWordWrap(true);
        importLayout->addWidget(importHint);
        auto folderRow=new QHBoxLayout;
        importLayout->addLayout(folderRow);
        auto folder=new QLineEdit;
        folder->setObjectName("mouthImportDirectory");
        folderRow->addWidget(folder);
        auto chooseFolder=new QPushButton(t("Choose folder","选择文件夹","フォルダーを選択"));
        folderRow->addWidget(chooseFolder);
        auto scan=new QPushButton(t("Scan folder","扫描文件夹","フォルダーを確認"));
        scan->setObjectName("mouthScanDirectory");
        folderRow->addWidget(scan);
        auto cancelScan=new QPushButton(trText("Cancel"));
        cancelScan->setObjectName("mouthCancelScan");
        cancelScan->setEnabled(false);
        folderRow->addWidget(cancelScan);
        auto importTable=new QTableWidget(0,4);
        importTable->setObjectName("mouthImportTable");
        importTable->setHorizontalHeaderLabels({
            t("File / select","文件 / 选择","ファイル / 選択"),t("Slot","槽位","枠"),t("Status","状态","状態"),t("Details","说明","詳細")
        });
        importTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        importTable->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
        importTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        importLayout->addWidget(importTable);
        auto importProgress=new QProgressBar;
        importProgress->hide();
        importLayout->addWidget(importProgress);
        auto applyImport=new QPushButton(t("Import selected into draft","将勾选项导入设置草稿","選択項目を設定の下書きに読込"));
        applyImport->setObjectName("mouthApplyDirectory");
        applyImport->setEnabled(false);
        importLayout->addWidget(applyImport);
        auto importStatus=new QLabel;
        importStatus->setObjectName("mouthImportStatus");
        importStatus->setWordWrap(true);
        importLayout->addWidget(importStatus);
        QFutureWatcher<AssetImportPlan> watcher;
        auto scanCancel=std::make_shared<std::atomic_bool>(false);
        connect(chooseFolder,&QPushButton::clicked,&dialog,[&]{
            const auto dir=QFileDialog::getExistingDirectory(&dialog,t("Choose folder","选择文件夹","フォルダーを選択"),folder->text());
            if(!dir.isEmpty())folder->setText(dir);
        });
        connect(cancelScan,&QPushButton::clicked,&dialog,[&]{
            scanCancel->store(true);
        });
        connect(scan,&QPushButton::clicked,&dialog,[&]{
            scanCancel=std::make_shared<std::atomic_bool>(false);
            auto token=scanCancel;
            const auto directory=folder->text();
            const auto existing=draft.assets;
            scan->setEnabled(false);
            applyImport->setEnabled(false);
            cancelScan->setEnabled(true);
            importProgress->setValue(0);
            importProgress->show();
            importTable->setRowCount(0);
            importStatus->clear();
            buttons->button(QDialogButtonBox::Apply)->setEnabled(false);
            QPointer<QProgressBar> progressGuard=importProgress;
            watcher.setFuture(QtConcurrent::run([directory,existing,token,progressGuard]{
                return scanAssetDirectory(directory,existing,token.get(),[progressGuard](int n){
                    if(progressGuard)QMetaObject::invokeMethod(progressGuard,[progressGuard,n]{
                        if(progressGuard)progressGuard->setValue(n);
                    },Qt::QueuedConnection);
                });
            }));
        });
        connect(&watcher,&QFutureWatcher<AssetImportPlan>::finished,&dialog,[&]{
            const auto result=watcher.result();
            scan->setEnabled(true);
            cancelScan->setEnabled(false);
            importProgress->hide();
            buttons->button(QDialogButtonBox::Apply)->setEnabled(true);
            if(result.cancelled){
                importStatus->setText(trText("Cancelled"));
                return;
            }
            if(!result.error.isEmpty()){
                importStatus->setText(result.error);
                return;
            }
            int eligible=0;
            for(const auto &entry:result.entries){
                const int row=importTable->rowCount();
                importTable->insertRow(row);
                auto item=new QTableWidgetItem(QFileInfo(entry.path).fileName());
                item->setData(Qt::UserRole,entry.id);
                item->setData(Qt::UserRole+1,entry.path);
                item->setToolTip(entry.path);
                item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
                const bool valid=entry.status=="new"||entry.status=="replace";
                if(valid){
                    item->setFlags(item->flags()|Qt::ItemIsUserCheckable);
                    item->setCheckState(entry.status=="new"?Qt::Checked:Qt::Unchecked);
                    ++eligible;
                }
                importTable->setItem(row,0,item);
                importTable->setItem(row,1,new QTableWidgetItem(entry.id));
                const auto label=entry.status=="new"?t("New","新增","新規"):entry.status=="replace"?t("Replace","替换","置換"):entry.status=="conflict"?t("Conflict",
                "重复冲突","重複競合"):entry.status=="invalid"?t("Invalid PNG","PNG 无效","無効な PNG"):t("Unknown filename","文件名未识别","未認識の名前");
                importTable->setItem(row,2,new QTableWidgetItem(label));
                auto detail=entry.message;
                if(detail=="Duplicate normalized slot")detail=t("Duplicate normalized slot","多个文件对应同一槽位","複数のファイルが同じ枠に対応します");
                else if(detail=="Unrecognized asset filename")detail=t("Unrecognized asset filename","文件名不符合规定","ファイル名が規定に一致しません");
                else if(detail=="Invalid or oversized PNG")detail=t("Invalid or oversized PNG","PNG 无效或超过单图上限","PNG が無効か画像サイズが上限を超えています");
                importTable->setItem(row,3,new QTableWidgetItem(detail));
            }
            applyImport->setEnabled(eligible>0);
            importStatus->setText(t("%1 files checked; %2 eligible.","核查 %1 个 PNG，%2 个可选择。","%1 個の PNG を確認、%2 個が選択可能です。").arg(result.entries.size()).arg(eligible));
        });
        connect(applyImport,&QPushButton::clicked,&dialog,[&]{
            int count=0;
            for(int row=0;row<importTable->rowCount();++row){
                auto item=importTable->item(row,0);
                if(item->flags().testFlag(Qt::ItemIsUserCheckable)&&item->checkState()==Qt::Checked){
                    draft.assets[item->data(Qt::UserRole).toString()]=item->data(Qt::UserRole+1).toString();
                    ++count;
                }
            }
            refreshSlots();
            importStatus->setText(t("%1 mappings queued. Apply the settings window to save; Cancel discards them.","%1 个映射已进入草稿；应用设置窗口才保存，取消丢弃。","%1 件の対応を下書きに追加しました。設定画面の適用で保存、キャンセルで破棄します。").arg(count));
        });
        root->addWidget(status);
        root->addWidget(buttons);
        connect(buttons->button(QDialogButtonBox::Cancel),&QPushButton::clicked,&dialog,&QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&]{
            try{
                auto options=draft.appearance;
                if(options.degreeMode=="custom")options.customDegreeScale=readCustomScale();
                for(const auto &track:project.score.tracks)for(const auto &note:track.notes)if(options.noteSpellings.contains(note.id)&&!writtenPitch(options,note))throw Failure(t("Written name must match the note pitch","指定音名必须与当前音高一致","指定音名は現在の音高と一致する必要があります"));
                options.strongBeats.clear();
                for(int row=0;row<accents->rowCount();++row){
                    const auto value=accents->item(row,1)->text().trimmed();
                    if(value.isEmpty())continue;
                    QVector<int> positions;
                    for(const auto &field:value.split(QRegularExpression("[,，\\s]+"),Qt::SkipEmptyParts)){
                        bool ok=false;
                        const int n=field.toInt(&ok);
                        if(!ok)throw Failure(t("Invalid strong-beat positions","强拍位置无效","強拍位置が無効です"));
                        positions.append(n);
                    }
                    options.strongBeats[accents->item(row,0)->text()]=positions;
                }
                validateAppearance(options);
                validateConsonants(draft.rules.consonants);
                draft.appearance=options;
                dialog.accept();
            }
            catch(const std::exception &e){
                status->setText(QString::fromUtf8(e.what()));
            }
        });
        refreshSlots();
        fillConsonants();
        const int result=dialog.exec();
        scanCancel->store(true);
        if(watcher.isRunning())watcher.waitForFinished();
        mouthSettingsOpen=false;
        QTimer::singleShot(0,this,[this]{
            ensureWaveform();
        });
        if(result!=QDialog::Accepted)return;
        if(draft.rules.consonants==project.rules.consonants){
            change(trText("Advanced variant settings"),[draft](Project &p){
                p.appearance=draft.appearance;
                p.assets=draft.assets;
            });
            return;
        }
        Project next=project;
        next.appearance=draft.appearance;
        next.assets=draft.assets;
        next.rules.consonants=draft.rules.consonants;
        loadIsRegenerate=true;
        cancel=std::make_shared<std::atomic_bool>(false);
        auto token=cancel;
        setBusy(true);
        auto report=[this](int n){
            QMetaObject::invokeMethod(this,[this,n]{
                progress->setValue(n);
            },Qt::QueuedConnection);
        };
        loadWatcher.setFuture(QtConcurrent::run([next,token,report]()mutable{
            LoadResult r;
            try{
                next.regenerate(token.get(),report);
                r.project=next;
                r.cancelled=token->load();
            }
            catch(const std::exception &e){
                r.cancelled=token->load();
                r.error=QString::fromUtf8(e.what());
            }
            return r;
        }));
    }
}
