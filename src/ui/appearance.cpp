// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include "file_drop.h"
#include "tonality_selector.h"
#include "core/appearance.h"
#include "core/intervals.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QImageReader>
namespace chosuta {
void Window::showMouthSettings(const QStringList &droppedPaths){
    if(busy||mouthSettingsOpen)return;QScopedValueRollback<bool> guard(mouthSettingsOpen,true);stopPlayback();
    auto t=[this](const char *en,const char *zh,const char *ja){return trText(en,zh,ja);};
    QDialog dialog(this);dialog.setObjectName("mouthSettingsDialog");dialog.setWindowTitle(trText("Advanced variant settings"));dialog.resize(980,720);
    Project draft=project;auto root=new QVBoxLayout(&dialog);
    auto enabled=new QCheckBox(trText("Enable advanced variants"));enabled->setObjectName("mouthEnabled");enabled->setChecked(draft.appearance.enabled);root->addWidget(enabled);
    connect(enabled,&QCheckBox::toggled,&dialog,[&](bool v){draft.appearance.enabled=v;});
    auto pages=new QTabWidget;pages->setObjectName("mouthSettingsPages");root->addWidget(pages);
    auto material=new QWidget;auto ml=new QHBoxLayout(material);pages->addTab(material,t("Variant images","差分素材","差分画像"));
    auto left=new QVBoxLayout;ml->addLayout(left,1);auto groupList=new QListWidget;groupList->setObjectName("variantGroups");left->addWidget(groupList);
    auto groupButtons=new QGridLayout;left->addLayout(groupButtons);
    auto makeButton=[&](const QString &label,const QString &name){auto b=new QPushButton(label);b->setObjectName(name);return b;};
    auto add=makeButton(t("Add group","添加组","グループ追加"),"addVariantGroup"),copy=makeButton(t("Copy group","复制组","複製"),"copyVariantGroup"),rename=makeButton(t("Rename","重命名","名前変更"),"renameVariantGroup"),remove=makeButton(t("Remove…","删除…","削除…"),"removeVariantGroup");
    groupButtons->addWidget(add,0,0);groupButtons->addWidget(copy,0,1);groupButtons->addWidget(rename,1,0);groupButtons->addWidget(remove,1,1);
    auto right=new QVBoxLayout;ml->addLayout(right,3);auto hint=new QLabel(t("A variant group holds a consistent pose with different mouths. Missing slots use the base group, then global fallback. Choosing a group only changes the editing target.","差分组保存动作/神态一致、口形不同的完整图片。缺槽自动回基础组同口形，再回全局图。切换分组只改变编辑对象。","差分グループは同じ動作・表情の口形画像です。未設定の枠は基本グループ、全体の代替画像の順に使います。"));hint->setWordWrap(true);right->addWidget(hint);
    QString selected="A",groupId="base";QMap<QString,QToolButton *> slotButtons;auto states=new QTabWidget;states->setObjectName("mouthStatePages");right->addWidget(states);
    auto mouthName=[this](const QString &s){return mouthLabel(uiLanguage,s);};
    auto addSlots=[&](const QStringList &names,const QString &label){auto area=new QScrollArea;area->setWidgetResizable(true);auto panel=new QWidget;auto layout=new QHBoxLayout(panel);for(const auto &name:names){auto b=new QToolButton;b->setObjectName("mouthSlot"+name);b->setMinimumSize(110,142);b->setIconSize({82,82});b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);b->setCheckable(true);layout->addWidget(b);slotButtons[name]=b;}layout->addStretch();area->setWidget(panel);states->addTab(area,label);};
    addSlots({"A","I","U","E","O"},t("Vowels","发声口形","母音"));addSlots({"closed","rest","breath"},t("Special states","特殊口形","特殊な口形"));
    auto pathRow=new QHBoxLayout;right->addLayout(pathRow);auto path=new QLineEdit;path->setObjectName("mouthImagePath");pathRow->addWidget(path);
    auto browse=makeButton(t("Choose PNG","选择 PNG","PNG を選択"),"mouthBrowseImage"),assign=makeButton(t("Assign slot","导入此槽","この枠に設定"),"mouthAssignImage"),clear=makeButton(t("Clear slot","清除此槽","枠をクリア"),"mouthClearImage");pathRow->addWidget(browse);pathRow->addWidget(assign);pathRow->addWidget(clear);
    auto imageStatus=new QLabel;imageStatus->setObjectName("mouthImageStatus");imageStatus->setWordWrap(true);right->addWidget(imageStatus);
    auto mappings=new QFormLayout;right->addLayout(mappings);QMap<QString,QComboBox *> roleChoices;
    for(const auto &role:QStringList{"relaxed","normal","emphasis"}){auto box=new QComboBox;box->setObjectName("performanceRole"+role);roleChoices[role]=box;mappings->addRow(role=="relaxed"?t("Restrained","收敛","控えめ"):role=="normal"?t("Normal","常态","通常"):t("Emphasis","强调","強調"),box);connect(box,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&,box,role]{if(box->currentIndex()>=0)draft.performance.roles[role]=box->currentData().toString();});}
    right->addStretch();
    auto refreshSlots=[&]{for(auto it=slotButtons.cbegin();it!=slotButtons.cend();++it){const auto id=variantAssetId(groupId,it.key());const auto file=draft.assets.value(id);const bool own=!file.isEmpty();QString display=file;if(display.isEmpty())display=draft.assets.value(it.key());if(display.isEmpty())display=draft.assets.value(draft.fallback);
            QImageReader reader(display);auto size=display.isEmpty()?QSize{}:reader.size();if(size.isValid()){size.scale(82,82,Qt::KeepAspectRatio);reader.setScaledSize(size);it.value()->setIcon(QPixmap::fromImage(reader.read()));}else it.value()->setIcon(QIcon());
            it.value()->setText(mouthName(it.key())+"\n"+(own?t("Assigned","已配图","設定済み"):!display.isEmpty()?t("Fallback","回退","代替"):t("Missing","缺图","未設定")));it.value()->setToolTip(display);it.value()->setChecked(it.key()==selected);}
        path->setText(draft.assets.value(variantAssetId(groupId,selected)));remove->setEnabled(groupId!="base");};
    QMap<QString,QString> removedTargets;std::function<void()> refreshRuleTargets;auto refreshGroups=[&]{if(refreshRuleTargets)refreshRuleTargets();QSignalBlocker block(groupList);groupList->clear();for(const auto &g:draft.performance.groups){auto item=new QListWidgetItem(g.name,groupList);item->setData(Qt::UserRole,g.id);if(g.id==groupId)groupList->setCurrentItem(item);}for(auto it=roleChoices.cbegin();it!=roleChoices.cend();++it){QSignalBlocker b(it.value());it.value()->clear();for(const auto &g:draft.performance.groups)it.value()->addItem(g.name,g.id);it.value()->setCurrentIndex(it.value()->findData(draft.performance.roles.value(it.key())));}refreshSlots();};
    connect(groupList,&QListWidget::currentItemChanged,&dialog,[&](QListWidgetItem *item){if(item){groupId=item->data(Qt::UserRole).toString();refreshSlots();}});
    connect(states,&QTabWidget::currentChanged,&dialog,[&](int i){selected=i?"closed":"A";refreshSlots();});
    for(auto it=slotButtons.cbegin();it!=slotButtons.cend();++it)connect(it.value(),&QToolButton::clicked,&dialog,[&,name=it.key()]{selected=name;refreshSlots();});
    auto newGroup=[&](bool duplicate){bool ok=false;QString name=QInputDialog::getText(&dialog,t("Variant group","差分组","差分グループ"),t("Name","名称","名前"),QLineEdit::Normal,duplicate?variantGroupName(draft,groupId)+" (2)":QString{},&ok);if(!ok||name.trimmed().isEmpty())return;const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);draft.performance.groups.append({id,name.trimmed()});if(duplicate)for(const auto &slot:mouthStates()){auto file=draft.assets.value(variantAssetId(groupId,slot));if(!file.isEmpty())draft.assets[variantAssetId(id,slot)]=file;}groupId=id;refreshGroups();};
    connect(add,&QPushButton::clicked,&dialog,[&]{newGroup(false);});connect(copy,&QPushButton::clicked,&dialog,[&]{newGroup(true);});
    connect(rename,&QPushButton::clicked,&dialog,[&]{bool ok=false;auto name=QInputDialog::getText(&dialog,t("Rename group","重命名组","グループ名変更"),t("Name","名称","名前"),QLineEdit::Normal,variantGroupName(draft,groupId),&ok);if(ok&&!name.trimmed().isEmpty()){for(auto &g:draft.performance.groups)if(g.id==groupId)g.name=name.trimmed();refreshGroups();}});
    connect(remove,&QPushButton::clicked,&dialog,[&]{
        if(groupId=="base")return;
        QDialog choice(&dialog);choice.setWindowTitle(t("Replace removed group","替换被删除的组","削除するグループの置換"));
        auto layout=new QVBoxLayout(&choice);auto target=new QComboBox;target->setObjectName("replacementGroup");
        for(const auto &g:draft.performance.groups)if(g.id!=groupId)target->addItem(g.name+" ["+g.id+"]",g.id);
        layout->addWidget(target);auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout->addWidget(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&choice,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&choice,&QDialog::reject);
        if(choice.exec()!=QDialog::Accepted)return;const auto id=target->currentData().toString();
        removedTargets["group:"+groupId]="group:"+id;removeVariantGroup(draft,groupId,id);groupId="base";refreshGroups();
    });
    connect(browse,&QPushButton::clicked,&dialog,[&]{auto f=QFileDialog::getOpenFileName(&dialog,trText("PNG"),path->text(),"PNG (*.png)");if(!f.isEmpty())path->setText(f);});
    connect(assign,&QPushButton::clicked,&dialog,[&]{auto scan=scanAssetFiles({path->text()},{});if(!scan.error.isEmpty()||scan.entries.size()!=1||scan.entries.front().status=="invalid"){imageStatus->setText(t("Invalid PNG","PNG 无效","無効な PNG"));return;}
        const auto size=QImageReader(path->text()).size();
        bool mismatch=false;for(const auto &slot:slotButtons.keys()){auto otherPath=draft.assets.value(variantAssetId(groupId,slot));if(otherPath.isEmpty())continue;QImageReader other(otherPath);if(other.size().isValid()&&other.size()!=size)mismatch=true;}
        auto entry=scan.entries.front();entry.id=variantAssetId(groupId,selected);applyAssetImport(draft,{entry});refreshSlots();imageStatus->setText(mismatch?t("Assigned; canvas sizes differ in this group.","已配图；本组画布尺寸不同，请预览人物定位。","設定済み。グループ内で画像サイズが異なります。"):t("Assigned","已配图","設定済み"));});
    connect(clear,&QPushButton::clicked,&dialog,[&]{draft.assets.remove(variantAssetId(groupId,selected));refreshSlots();});refreshGroups();
    auto automatic=new QScrollArea;automatic->setWidgetResizable(true);auto autoContent=new QWidget;automatic->setWidget(autoContent);auto al=new QFormLayout(autoContent);pages->addTab(automatic,t("Automatic performance","自动演出","自動演出"));pages->setTabEnabled(pages->indexOf(automatic),draft.score.hasMusic);
    auto settingsHint=new QLabel(t("Apply updates automatic performance immediately. Protected manual segments stay. Quarter note = 1. No audio analysis.","应用时自动更新工程演出，保留受保护人工段，可撤销；无需另点生成。四分音符=1，不分析音频。","適用時に自動演出を更新します。保護した手動区間は維持され、元に戻せます。四分音符=1。音声解析なし。"));settingsHint->setWordWrap(true);al->addRow(settingsHint);
    auto &ps=draft.performance.settings;
    auto key=new TonalitySelector(draft.performance.analysis,false,t);key->setObjectName("performanceKey");al->addRow(key);
    auto timeUnit=new QComboBox;timeUnit->setObjectName("performanceTimeBasis");timeUnit->addItem(t("Musical duration (quarter = 1)","音乐时值（四分音符=1）","音楽時間（四分音符=1）"),"music");timeUnit->addItem(t("Elapsed score seconds","谱面经过时间（秒）","楽譜の経過秒"),"seconds");timeUnit->setCurrentIndex(ps.timeBasis==PerformanceTimeBasis::Music?0:1);al->addRow(t("Duration basis","时长依据","時間の単位"),timeUnit);
    auto unit=new QComboBox;unit->setObjectName("mouthIntervalUnit");unit->addItem(t("Semitones","半音数","半音数"),"semitones");unit->addItem(t("Written degrees","音程度数（按音名）","音程度数"),"diatonic");unit->setCurrentIndex(unit->findData(draft.performance.analysis.intervalUnit));al->addRow(t("Pitch distance basis","音高距离依据","音高差の単位"),unit);
    auto settingsFlag=[&](const QString &label,const QString &name,bool &value){auto b=new QCheckBox(label);b->setObjectName(name);b->setChecked(value);al->addRow(b);connect(b,&QCheckBox::toggled,&dialog,[&value](bool v){value=v;});};
    settingsFlag(t("A1: leap arrival","强调：跳进到达（A1）","A1：跳躍の到達"),"performanceLeap",ps.leap);
    settingsFlag(t("A1: cumulative ascent","强调：累计上行（A1）","A1：上行の累積"),"performanceProgression",ps.progression);
    settingsFlag(t("A2: repeated high motif","强调：高区重复动机（A2）","A2：高音域の反復動機"),"performanceMotif",ps.motif);
    settingsFlag(t("A3: elevated section","强调：段落展开（A3）","A3：セクションの展開"),"performanceSection",ps.section);
    settingsFlag(t("R1: evidence-based release","收敛：持续回落＋节奏放宽＋结束边界（R1）","R1：根拠のある収束"),"performanceRelease",ps.release);
    auto releaseHelp=new QLabel(t("A1–A3 select Emphasis; R1 selects Restrained. Release needs a sustained lower register, two wider onset gaps and an ending boundary together, with no active emphasis evidence. Tonic, long notes or phrase endings alone do not trigger release. Group names are user-defined.","A1–A3 对应强调，R1 对应收敛。收敛须同时具备：音区持续低于背景、连续两次起音间距放宽、临近结束边界，且没有正在成立的强调依据。主音、长音或句尾本身不触发收敛。差分组名称（如 A、B）由你定义，并无固定强弱含义。","A1–A3 は強調、R1 は抑制です。低音域の持続・2回連続の発音間隔の拡大・終端境界がそろい、強調の根拠がない場合に収束します。主音・長音・句末だけでは収束しません。グループ名は任意です。"));releaseHelp->setWordWrap(true);releaseHelp->setObjectName("performanceRuleHelp");al->addRow(releaseHelp);
    settingsFlag(t("Retain targets, turns and short breaths","目标、回绕与短换气保持","到達音・折返し・短い息継ぎを維持"),"performanceContinuity",ps.continuity);
    auto details=new QWidget;auto detailForm=new QFormLayout(details);auto detailsToggle=new QToolButton;detailsToggle->setText(t("Detailed thresholds and note spelling","详细阈值与音名","詳細な閾値と音名"));detailsToggle->setCheckable(true);detailsToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);detailsToggle->setArrowType(Qt::RightArrow);al->addRow(detailsToggle);al->addRow(details);details->hide();connect(detailsToggle,&QToolButton::toggled,&dialog,[=](bool show){details->setVisible(show);detailsToggle->setArrowType(show?Qt::DownArrow:Qt::RightArrow);});
    QVector<std::function<void()>> refreshParameters;int commonRow=al->rowCount()-2;
    auto duration=[&](const QString &name,const QString &label,double PerformanceTiming::*field,bool common=false){auto box=new QDoubleSpinBox;box->setObjectName("performance"+name);box->setDecimals(3);box->setRange(.001,128);if(common)al->insertRow(commonRow++,label,box);else detailForm->addRow(label,box);
        auto refresh=[&,box,field]{QSignalBlocker block(box);box->setValue(ps.timing().*field);box->setSuffix(ps.timeBasis==PerformanceTimeBasis::Music?t(" quarters"," 四分音符"," 四分音符"):t(" s"," 秒"," 秒"));};refreshParameters.append(refresh);refresh();connect(box,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[&,field](double v){(ps.timeBasis==PerformanceTimeBasis::Music?ps.music:ps.seconds).*field=v;});};
    duration("window",t("Local window","局部观察窗","局所観察範囲"),&PerformanceTiming::window,true);
    duration("minimum",t("Minimum hold","最低画面保持","最短維持"),&PerformanceTiming::minimum,true);
    duration("breath",t("Short breath carry","短换气延续上限","短い息継ぎの維持上限"),&PerformanceTiming::breath,true);
    duration("background",t("Background lookback","背景回看窗","背景参照範囲"),&PerformanceTiming::background);
    duration("lookahead",t("Local lookahead","局部前看上限","局所先読み上限"),&PerformanceTiming::lookahead);
    duration("support",t("Sounding support","目标累计发声支持","到達音の発声時間"),&PerformanceTiming::support);
    duration("confirm",t("Change confirmation","变化确认","変化の確認"),&PerformanceTiming::confirm);
    duration("emphasis",t("Emphasis hold","强调最低保持","強調の最短維持"),&PerformanceTiming::emphasis);
    duration("exit",t("Departure confirmation","回落确认","下降の確認"),&PerformanceTiming::exit);
    duration("phrase",t("Phrase gap","明显分句间隙","フレーズの休止"),&PerformanceTiming::phrase);
    duration("reset",t("Section reset gap","段落重置间隙","セクションを区切る休止"),&PerformanceTiming::reset);
    duration("release",t("Release support","收敛支持时长","収束の支持時間"),&PerformanceTiming::release);
    auto distance=[&](const QString &name,const QString &label,int &semitones,int &degrees,int minSemi,int minDegree){auto box=new QSpinBox;box->setObjectName("performance"+name);detailForm->addRow(label,box);auto refresh=[&,box,minSemi,minDegree]{QSignalBlocker block(box);bool d=unit->currentData()=="diatonic";box->setRange(d?minDegree:minSemi,48);box->setValue(d?degrees:semitones);box->setSuffix(d?t(" degree"," 度"," 度"):t(" semitones"," 半音"," 半音"));};refreshParameters.append(refresh);refresh();connect(box,qOverload<int>(&QSpinBox::valueChanged),&dialog,[&,unit](int v){(unit->currentData()=="diatonic"?degrees:semitones)=v;});};
    distance("leapDistance",t("Leap","显著跳进","跳躍"),ps.leapSemitones,ps.leapDegrees,1,2);
    distance("progressionDistance",t("Net ascent","累计上行距离","上行距離"),ps.progressionSemitones,ps.progressionDegrees,1,2);
    distance("contrastDistance",t("Register contrast","音区明显改变","音域の変化"),ps.contrastSemitones,ps.contrastDegrees,1,2);
    distance("nearDistance",t("Target radius","同目标半径","同じ到達音の範囲"),ps.nearSemitones,ps.nearDegrees,0,1);
    auto ratio=[&](const QString &name,const QString &label,double &value,double minimum,double maximum){auto box=new QDoubleSpinBox;box->setObjectName("performance"+name);box->setDecimals(3);box->setRange(minimum,maximum);box->setValue(value);detailForm->addRow(label,box);connect(box,qOverload<double>(&QDoubleSpinBox::valueChanged),&dialog,[&value](double n){value=n;});refreshParameters.append([box,&value]{QSignalBlocker block(box);box->setValue(value);});};
    ratio("efficiency",t("Net / total motion","净运动／累计运动","純移動／総移動"),ps.efficiency,.001,1);
    ratio("highShare",t("High-register share","高区发声占比","高音域の割合"),ps.highShare,.001,1);
    ratio("motifShare",t("Motif high-target share","动机高目标占比","動機の高音割合"),ps.motifShare,.001,1);
    ratio("activityRatio",t("Activity increase ratio","起音活动增加比例","活動増加比"),ps.activityRatio,1,64);
    ratio("wideningRatio",t("Onset widening ratio","起音间距放宽比例","発音間隔の拡大比"),ps.wideningRatio,1,64);
    ratio("motifTolerance",t("Rhythm similarity tolerance","动机节奏容差","動機のリズム許容差"),ps.motifTolerance,0,1);
    auto scale=new QLineEdit;scale->setObjectName("mouthCustomScale");QStringList scaleValues;for(int n:draft.performance.analysis.customDegreeScale)scaleValues<<QString::number(n);scale->setText(scaleValues.join(' '));detailForm->addRow(t("Custom seven-note offsets","自定义七音偏移（选择自定义调式）","独自7音の半音位置"),scale);
    connect(scale,&QLineEdit::textChanged,&dialog,[key](const QString &text){QVector<int> values;for(auto token:text.split(QRegularExpression("[,，\\s]+"),Qt::SkipEmptyParts)){bool ok=false;int n=token.toInt(&ok);if(!ok)return;values.append(n);}if(values.size()==7&&values.front()==0&&values.last()<12&&std::adjacent_find(values.begin(),values.end(),std::greater_equal<int>())==values.end())key->setCustomScale(values);});
    auto spellingButton=makeButton(t("Written note names…","逐音音名…","音符名の指定…"),"performanceSpellings");detailForm->addRow(spellingButton);
    connect(spellingButton,&QPushButton::clicked,&dialog,[&]{
        QDialog d(&dialog);d.setWindowTitle(spellingButton->text());d.resize(740,540);auto l=new QVBoxLayout(&d);auto page=new QSpinBox;l->addWidget(page);auto table=new QTableWidget(0,4);table->setHorizontalHeaderLabels({t("Track","轨道","トラック"),t("Beat","拍位","拍位置"),t("MIDI pitch","MIDI 音高","MIDI 音高"),t("Written name (e.g. C#4)","音名（如 C#4）","音名（例 C#4）")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);l->addWidget(table);auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);l->addWidget(buttons);
        QVector<QPair<QString,Note>> notes;for(const auto &track:draft.score.tracks)if(draft.selected.contains(track.id))for(const auto &n:track.notes)notes.append({track.name,n});auto names=draft.performance.analysis.noteSpellings;page->setRange(1,std::max(1,(int(notes.size())+255)/256));page->setPrefix(t("Page ","页 ","ページ "));bool filling=false;
        auto fill=[&]{filling=true;table->setRowCount(0);for(int i=(page->value()-1)*256;i<std::min(int(notes.size()),page->value()*256);++i){int r=table->rowCount();table->insertRow(r);const auto &n=notes[i].second;QStringList values={notes[i].first,draft.score.time.beatLabel(n.onset),QString::number(n.pitch),names.value(n.id)};for(int c=0;c<4;++c){auto cell=new QTableWidgetItem(values[c]);if(c<3)cell->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);cell->setData(Qt::UserRole,n.id);cell->setData(Qt::UserRole+1,n.pitch);table->setItem(r,c,cell);}}filling=false;};
        connect(page,qOverload<int>(&QSpinBox::valueChanged),&d,[&]{fill();});connect(table,&QTableWidget::itemChanged,&d,[&](QTableWidgetItem *item){if(filling||item->column()!=3)return;QSignalBlocker block(table);auto text=item->text().trimmed();auto written=parseWrittenPitch(text);auto id=item->data(Qt::UserRole).toString();if(text.isEmpty())names.remove(id);else names[id]=text;item->setBackground(text.isEmpty()||(written&&written->midi==item->data(Qt::UserRole+1).toInt())?QBrush():QBrush(QColor("#ffd5d5")));});
        connect(buttons,&QDialogButtonBox::accepted,&d,[&]{for(const auto &entry:notes){auto text=names.value(entry.second.id);auto name=parseWrittenPitch(text);if(!text.isEmpty()&&(!name||name->midi!=entry.second.pitch)){QMessageBox::warning(&d,t("Invalid note name","音名无效","音名が無効"),text);return;}}d.accept();});connect(buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);fill();if(d.exec()==QDialog::Accepted)draft.performance.analysis.noteSpellings=names;
    });
    auto customRules=new QTableWidget(0,7);customRules->setObjectName("performanceRules");customRules->setHorizontalHeaderLabels({t("On","启用","有効"),t("Priority","优先级","優先度"),t("Condition","条件","条件"),t("Threshold","阈值","閾値"),t("Min quarters","最少四分音符","最短四分音符"),t("Target","目标","対象"),t("Track","轨道","トラック")});customRules->setMinimumHeight(230);detailForm->addRow(customRules);
    auto addRule=[&](const PerformanceRule &rule){int row=customRules->rowCount();customRules->insertRow(row);auto on=new QTableWidgetItem;on->setData(Qt::UserRole,QVariantList{rule.threshold,rule.thresholdSeconds,rule.thresholdDegrees,rule.minBeats,rule.minSeconds});on->setCheckState(rule.enabled?Qt::Checked:Qt::Unchecked);customRules->setItem(row,0,on);
        for(auto pair:{qMakePair(1,QString::number(rule.priority)),qMakePair(3,QString::number(rule.condition=="long"&&ps.timeBasis==PerformanceTimeBasis::Seconds?rule.thresholdSeconds:QStringList{"leap","up","down"}.contains(rule.condition)&&draft.performance.analysis.intervalUnit=="diatonic"?rule.thresholdDegrees:rule.threshold)),qMakePair(4,QString::number(ps.timeBasis==PerformanceTimeBasis::Music?rule.minBeats:rule.minSeconds))})customRules->setItem(row,pair.first,new QTableWidgetItem(pair.second));
        auto condition=new QComboBox;for(const auto &c:QStringList{"leap","up","down","long","degree","outside","tonic","strong","high"})condition->addItem(c=="leap"?t("Leap distance","跳进距离","跳躍距離"):c=="up"?t("Net ascent","净上行距离","上行距離"):c=="down"?t("Net descent","净下行距离","下行距離"):c=="long"?t("Duration","时值","長さ"):c=="degree"?t("Scale degree","音级","音級"):c=="outside"?t("Outside scale","调外音","音階外音"):c=="tonic"?t("Tonic","主音","主音"):c=="strong"?t("Strong beat","强拍","強拍"):t("MIDI pitch at least","MIDI 音高至少","MIDI 音高の下限"),c);condition->setCurrentIndex(condition->findData(rule.condition));customRules->setCellWidget(row,2,condition);
        auto thresholdItem=customRules->item(row,3);auto updateThreshold=[condition,thresholdItem]{bool used=!QStringList{"tonic","outside","strong"}.contains(condition->currentData().toString());thresholdItem->setFlags(used?Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable:Qt::NoItemFlags);thresholdItem->setForeground(used?QBrush():QBrush(Qt::transparent));};connect(condition,&QComboBox::currentIndexChanged,&dialog,updateThreshold);updateThreshold();
        auto target=new QComboBox;for(const auto &r:QStringList{"relaxed","normal","emphasis"})target->addItem(r=="relaxed"?t("Relaxed","收敛","抑制"):r=="normal"?t("Normal","常态","通常"):t("Emphasis","强调","強調"),r);for(const auto &g:draft.performance.groups)target->addItem(g.name,"group:"+g.id);target->setCurrentIndex(target->findData(rule.target));customRules->setCellWidget(row,5,target);
        auto track=new QComboBox;track->addItem(t("All","全部","すべて"),"");for(const auto &v:draft.score.tracks)track->addItem(v.name,v.id);track->setCurrentIndex(track->findData(rule.track));customRules->setCellWidget(row,6,track);};
    for(const auto &rule:ps.rules)addRule(rule);auto ruleButtons=new QWidget;auto rbl=new QHBoxLayout(ruleButtons);auto addRuleButton=makeButton(t("Add rule","添加规则","規則追加"),"addPerformanceRule"),deleteRule=makeButton(t("Remove rule","删除规则","規則削除"),"deletePerformanceRule");rbl->addWidget(addRuleButton);rbl->addWidget(deleteRule);detailForm->addRow(ruleButtons);connect(addRuleButton,&QPushButton::clicked,&dialog,[&]{addRule(PerformanceRule{});});connect(deleteRule,&QPushButton::clicked,&dialog,[&]{if(customRules->currentRow()>=0)customRules->removeRow(customRules->currentRow());});
    auto ruleHint=new QLabel(t("Higher priority runs first; ties use row order. Rules still respect automatic hold limits. Tonic/degree conditions require an explicit key. A tonic is not automatically quiet.","优先级数值大者先匹配，同级按行顺序；仍遵守自动保持。音级条件需要明确调性，主音不自动等于收敛。","優先度の大きい規則が先。同値は行順。音級条件には調性が必要です。主音を必ず弱くしません。"));ruleHint->setWordWrap(true);detailForm->addRow(ruleHint);
    auto defaults=new QCheckBox(t("Save general settings and all-track role rules as new-project defaults","将通用参数与全轨道档位规则保存为新工程默认","共通設定・全トラックの役割規則を新規既定に保存"));defaults->setObjectName("performanceSaveDefaults");al->addRow(defaults);
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
    refreshRuleTargets=[&]{for(int row=0;row<customRules->rowCount();++row){auto combo=qobject_cast<QComboBox *>(customRules->cellWidget(row,5));auto previous=combo->currentData().toString();while(removedTargets.contains(previous))previous=removedTargets.value(previous);QSignalBlocker block(combo);combo->clear();combo->addItem(t("Relaxed","收敛","抑制"),"relaxed");combo->addItem(t("Normal","常态","通常"),"normal");combo->addItem(t("Emphasis","强调","強調"),"emphasis");for(auto g:draft.performance.groups)combo->addItem(g.name,"group:"+g.id);int index=combo->findData(previous);combo->setCurrentIndex(index<0?combo->findData("group:base"):index);}};
    auto readRules=[&]{QVector<PerformanceRule> list;for(int row=0;row<customRules->rowCount();++row){PerformanceRule r;r.enabled=customRules->item(row,0)->checkState()==Qt::Checked;bool ok=false;r.priority=customRules->item(row,1)->text().toInt(&ok);if(!ok)throw Failure("Invalid rule priority");double threshold=customRules->item(row,3)->text().toDouble(&ok);if(!ok)throw Failure("Invalid rule threshold");double minimum=customRules->item(row,4)->text().toDouble(&ok);if(!ok)throw Failure("Invalid rule duration");r.condition=qobject_cast<QComboBox *>(customRules->cellWidget(row,2))->currentData().toString();r.target=qobject_cast<QComboBox *>(customRules->cellWidget(row,5))->currentData().toString();r.track=qobject_cast<QComboBox *>(customRules->cellWidget(row,6))->currentData().toString();auto values=customRules->item(row,0)->data(Qt::UserRole).toList();r.threshold=values[0].toDouble();r.thresholdSeconds=values[1].toDouble();r.thresholdDegrees=values[2].toDouble();r.minBeats=values[3].toDouble();r.minSeconds=values[4].toDouble();bool seconds=ps.timeBasis==PerformanceTimeBasis::Seconds;bool degrees=draft.performance.analysis.intervalUnit=="diatonic";
        (seconds?r.minSeconds:r.minBeats)=minimum;
        if(r.condition=="long"&&seconds)r.thresholdSeconds=threshold;else if(degrees&&QStringList{"leap","up","down"}.contains(r.condition))r.thresholdDegrees=threshold;else r.threshold=threshold;list.append(r);}return list;};
    for(int direction:{-1,1}){auto b=new QPushButton(direction<0?t("Up","上移","上へ"):t("Down","下移","下へ"));rbl->addWidget(b);connect(b,&QPushButton::clicked,&dialog,[&,direction]{try{int row=customRules->currentRow(),other=row+direction;if(row<0||other<0||other>=customRules->rowCount())return;auto list=readRules();list.swapItemsAt(row,other);customRules->setRowCount(0);for(const auto &r:list)addRule(r);customRules->setCurrentCell(other,0);}catch(const std::exception &e){status->setText(messageText(uiLanguage,exceptionMessage(e)));}});}
    auto showTable=[&]{customRules->horizontalHeaderItem(3)->setText(t("Threshold (selected basis)","阈值（所选依据）","閾値（選択単位）"));customRules->horizontalHeaderItem(4)->setText(ps.timeBasis==PerformanceTimeBasis::Seconds?t("Min seconds","最少秒数","最短秒"):t("Min quarters","最少四分音符","最短四分音符"));};showTable();
    auto switchBasis=[&]{try{auto rules=readRules();ps.timeBasis=timeUnit->currentIndex()==0?PerformanceTimeBasis::Music:PerformanceTimeBasis::Seconds;draft.performance.analysis.intervalUnit=unit->currentData().toString();customRules->setRowCount(0);for(const auto &r:rules)addRule(r);for(auto &refresh:refreshParameters)refresh();showTable();}catch(const std::exception &e){QSignalBlocker tb(timeUnit),pb(unit);timeUnit->setCurrentIndex(ps.timeBasis==PerformanceTimeBasis::Music?0:1);unit->setCurrentIndex(unit->findData(draft.performance.analysis.intervalUnit));status->setText(messageText(uiLanguage,exceptionMessage(e)));}};
    connect(timeUnit,&QComboBox::currentIndexChanged,&dialog,switchBasis);connect(unit,&QComboBox::currentIndexChanged,&dialog,switchBasis);
    auto restore=makeButton(t("Restore recommended rules","恢复推荐规则","推奨規則に戻す"),"restorePerformanceRules");al->addRow(restore);connect(restore,&QPushButton::clicked,&dialog,[&]{customRules->setRowCount(0);ps=PerformanceSettings{};timeUnit->setCurrentIndex(0);unit->setCurrentIndex(0);for(auto &refresh:refreshParameters)refresh();for(const auto &name:{"performanceLeap","performanceProgression","performanceMotif","performanceSection","performanceRelease","performanceContinuity"})dialog.findChild<QCheckBox *>(name)->setChecked(true);});
    auto importPage=new QWidget;auto il=new QVBoxLayout(importPage);pages->addTab(importPage,t("Batch import","批量导入","一括読込"));
    auto importHint=new QLabel(t("Drop PNGs or a folder here. Filenames suggest slots; any valid PNG can be mapped manually. Choose each destination group and mouth, or select rows and assign a group together. Checked replacements overwrite the settings draft only. If checked rows share a slot, the last row wins.","可拖入 PNG 或文件夹。文件名只用于建议槽位，任意有效 PNG 都能指定差分组和口形。可逐行选择，或选中多行后统一指定组。勾选替换项表示确认替换；此处先改设置草稿，原图不变。同一槽位勾选多行时采用最后一行。","PNGまたはフォルダーをドロップできます。任意のPNGにグループと口形を指定でき、選択した複数行のグループを一括変更できます。チェックした置換は設定の下書きに適用されます。同じ枠を選んだ場合は最後の行を使います。"));importHint->setWordWrap(true);il->addWidget(importHint);
    auto folderRow=new QHBoxLayout;il->addLayout(folderRow);auto folder=new QLineEdit;folder->setObjectName("mouthImportFolder");folderRow->addWidget(folder);auto chooseFolder=makeButton(t("Choose folder","选择文件夹","フォルダー選択"),"mouthChooseFolder"),scan=makeButton(t("Scan folder","扫描文件夹","確認"),"mouthScanFolder"),chooseFiles=makeButton(t("Choose PNGs","选择多个 PNG","PNG を選択"),"mouthChooseFiles"),cancelScan=makeButton(trText("Cancel"),"mouthCancelScan");folderRow->addWidget(chooseFolder);folderRow->addWidget(scan);folderRow->addWidget(chooseFiles);folderRow->addWidget(cancelScan);cancelScan->setEnabled(false);
    auto groupRow=new QHBoxLayout;il->addLayout(groupRow);auto batchGroup=new QComboBox;batchGroup->setObjectName("importTargetGroup");groupRow->addWidget(batchGroup);auto mapSelected=makeButton(t("Set group for selected rows","选中行指定此组","選択行のグループを変更"),"importMapSelected"),newImportGroup=makeButton(t("New group…","新建组…","新規グループ…"),"importNewGroup");groupRow->addWidget(mapSelected);groupRow->addWidget(newImportGroup);
    auto importTable=new QTableWidget(0,5);importTable->setObjectName("mouthImportTable");importTable->setSelectionBehavior(QAbstractItemView::SelectRows);importTable->setHorizontalHeaderLabels({t("Import","导入","読込"),t("Destination group","目标差分组","対象グループ"),t("Mouth","口形","口形"),t("File","文件","ファイル"),t("Status","状态","状態")});importTable->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);il->addWidget(importTable);
    auto selectionRow=new QHBoxLayout;il->insertLayout(il->count()-1,selectionRow);auto selectAllRows=makeButton(t("Select all rows","全选行","すべての行を選択"),"importSelectAll"),clearSelection=makeButton(t("Clear selection","取消选择","選択を解除"),"importClearSelection");selectionRow->addWidget(selectAllRows);selectionRow->addWidget(clearSelection);selectionRow->addStretch();
    connect(selectAllRows,&QPushButton::clicked,importTable,&QTableWidget::selectAll);connect(clearSelection,&QPushButton::clicked,importTable,&QTableWidget::clearSelection);
    auto thumbnail=new QLabel;thumbnail->setObjectName("importPreview");thumbnail->setFixedHeight(96);thumbnail->setAlignment(Qt::AlignCenter);il->addWidget(thumbnail);
    auto applyImport=makeButton(t("Import checked rows into settings draft","将勾选项导入设置草稿","選択項目を設定の下書きに読込"),"mouthApplyImport");il->addWidget(applyImport);auto importProgress=new QProgressBar;il->addWidget(importProgress);importProgress->hide();
    QFutureWatcher<AssetImportPlan> watcher;auto scanCancel=std::make_shared<std::atomic_bool>(false);AssetImportPlan importPlan;
    auto groupOptions=[&](QComboBox *box,const QString &current){QSignalBlocker b(box);box->clear();for(const auto &g:draft.performance.groups)box->addItem(g.name,g.id);for(const auto &g:importPlan.groups)if(box->findData(g.id)<0)box->addItem(g.name+t(" (new)","（新组）","（新規）"),g.id);box->setCurrentIndex(std::max(0,box->findData(current)));};
    auto destination=[&](int row){auto g=qobject_cast<QComboBox *>(importTable->cellWidget(row,1)),mouth=qobject_cast<QComboBox *>(importTable->cellWidget(row,2));return mouth->currentData().toString().isEmpty()?QString{}:variantAssetId(g->currentData().toString(),mouth->currentData().toString());};
    auto updateImportRow=[&](int row){const auto &e=importPlan.entries[row];auto id=destination(row);bool invalid=e.status=="invalid",replacement=draft.assets.contains(id),conflict=e.status=="conflict"&&id==e.id;auto cell=importTable->item(row,0);cell->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|(!invalid&&!id.isEmpty()?Qt::ItemIsUserCheckable:Qt::NoItemFlags));cell->setCheckState(!invalid&&!id.isEmpty()&&!replacement&&!conflict?Qt::Checked:Qt::Unchecked);auto text=invalid?t("Invalid PNG","PNG 无效","無効な PNG"):id.isEmpty()?t("Choose mouth","请选择口形","口形を選択"):replacement?t("Replacement: check to confirm","替换：勾选确认","置換：チェックして確認"):conflict?t("Duplicate: remap or select one","重名：改映射或选一个","重複：変更または1つ選択"):t("New","新增","新規");importTable->item(row,4)->setText(text);importTable->item(row,4)->setToolTip(messageText(uiLanguage,e.message));};
    auto fillImport=[&]{groupOptions(batchGroup,groupId);importTable->setRowCount(0);for(int i=0;i<importPlan.entries.size();++i){const auto &e=importPlan.entries[i];importTable->insertRow(i);auto check=new QTableWidgetItem;importTable->setItem(i,0,check);auto g=new QComboBox;groupOptions(g,e.group.isEmpty()?groupId:e.group);importTable->setCellWidget(i,1,g);auto mouth=new QComboBox;mouth->addItem(t("Choose…","请选择…","選択…"),"");for(const auto &slot:mouthStates())mouth->addItem(mouthName(slot),slot);mouth->setCurrentIndex(std::max(0,mouth->findData(e.id.section('/',-1))));importTable->setCellWidget(i,2,mouth);for(int c:{3,4}){auto item=new QTableWidgetItem(c==3?QFileInfo(e.path).fileName():QString{});item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);item->setToolTip(e.path);importTable->setItem(i,c,item);}updateImportRow(i);connect(g,&QComboBox::currentIndexChanged,&dialog,[&,i]{updateImportRow(i);});connect(mouth,&QComboBox::currentIndexChanged,&dialog,[&,i]{updateImportRow(i);});}};
    connect(importTable,&QTableWidget::currentCellChanged,&dialog,[&](int row){thumbnail->clear();if(row<0||row>=importPlan.entries.size())return;QImageReader reader(importPlan.entries[row].path);auto size=reader.size();if(!size.isValid()||importPlan.entries[row].status=="invalid")return;size.scale(96,96,Qt::KeepAspectRatio);reader.setScaledSize(size);thumbnail->setPixmap(QPixmap::fromImage(reader.read()));});
    connect(mapSelected,&QPushButton::clicked,&dialog,[&]{for(const auto &index:importTable->selectionModel()->selectedRows()){auto combo=qobject_cast<QComboBox *>(importTable->cellWidget(index.row(),1));combo->setCurrentIndex(combo->findData(batchGroup->currentData()));}});
    connect(newImportGroup,&QPushButton::clicked,&dialog,[&]{newGroup(false);groupOptions(batchGroup,groupId);for(int row=0;row<importTable->rowCount();++row){auto box=qobject_cast<QComboBox *>(importTable->cellWidget(row,1));groupOptions(box,box->currentData().toString());}});
    connect(chooseFolder,&QPushButton::clicked,&dialog,[&]{auto f=QFileDialog::getExistingDirectory(&dialog,chooseFolder->text(),folder->text());if(!f.isEmpty())folder->setText(f);});connect(cancelScan,&QPushButton::clicked,&dialog,[&]{scanCancel->store(true);});
    auto scanPaths=[&](QStringList paths){
        if(watcher.isRunning()||paths.isEmpty())return;
        bool directory=paths.size()==1&&QFileInfo(paths.front()).isDir();
        if(!directory){for(const auto &path:paths)if(QFileInfo(path).isDir()||QFileInfo(path).suffix().compare("png",Qt::CaseInsensitive)){status->setText(t("Drop one folder or PNG files together.","请一次拖入一个文件夹，或一批 PNG 文件。","フォルダー1つ、または複数のPNGをドロップしてください。"));return;}}
        if(directory)folder->setText(paths.front());pages->setCurrentWidget(importPage);scanCancel->store(false);scan->setEnabled(false);chooseFiles->setEnabled(false);cancelScan->setEnabled(true);applyImport->setEnabled(false);buttons->button(QDialogButtonBox::Apply)->setEnabled(false);material->setEnabled(false);automatic->setEnabled(false);consonantPage->setEnabled(false);importTable->setEnabled(false);importProgress->setValue(0);importProgress->show();auto snapshot=draft;auto id=groupId;
        watcher.setFuture(QtConcurrent::run([snapshot,paths,id,directory,token=scanCancel,&dialog,importProgress]{auto report=[&dialog,importProgress](int n){QMetaObject::invokeMethod(&dialog,[importProgress,n]{importProgress->setValue(n);},Qt::QueuedConnection);};if(directory)return scanVariantDirectory(paths.front(),snapshot,id,token.get(),report);auto plan=scanAssetFiles(paths,{},token.get(),report);for(auto &entry:plan.entries){entry.group=id;if(!entry.id.isEmpty())entry.id=variantAssetId(id,entry.id);}return plan;}));
    };
    connect(scan,&QPushButton::clicked,&dialog,[&]{scanPaths({folder->text()});});
    connect(chooseFiles,&QPushButton::clicked,&dialog,[&]{scanPaths(QFileDialog::getOpenFileNames(&dialog,chooseFiles->text(),{},"PNG (*.png)"));});
    connect(&watcher,&QFutureWatcher<AssetImportPlan>::finished,&dialog,[&]{importPlan=watcher.result();scan->setEnabled(true);chooseFiles->setEnabled(true);cancelScan->setEnabled(false);applyImport->setEnabled(true);buttons->button(QDialogButtonBox::Apply)->setEnabled(true);material->setEnabled(true);automatic->setEnabled(true);consonantPage->setEnabled(true);importTable->setEnabled(true);importProgress->hide();fillImport();status->setText(importPlan.cancelled?trText("Cancelled"):messageText(uiLanguage,importPlan.error));});
    connect(applyImport,&QPushButton::clicked,&dialog,[&]{QMap<QString,QString> additions;QSet<QString> added;for(int i=0;i<importPlan.entries.size();++i)if(importTable->item(i,0)->checkState()==Qt::Checked){const auto &e=importPlan.entries[i];auto id=destination(i);if(e.status=="invalid"||id.isEmpty())continue;additions[id]=e.path;if(id.contains('/'))added.insert(id.section('/',0,0));}
        QVector<AssetImportEntry> entries;for(auto it=additions.cbegin();it!=additions.cend();++it){entries.append({it.value(),it.key(),"selected",{}, {}});}applyAssetImport(draft,entries);for(const auto &g:importPlan.groups)if(added.contains(g.id)){bool found=false;for(const auto &v:draft.performance.groups)if(v.id==g.id)found=true;if(!found)draft.performance.groups.append(g);}refreshGroups();status->setText(t("Imported %1 slots into settings draft.","已向设置草稿导入 %1 个槽位。","設定の下書きに %1 枠を読込みました。").arg(additions.size()));for(int row=0;row<importTable->rowCount();++row)updateImportRow(row);});
    new FileDropFilter(&dialog,scanPaths);new FileDropFilter(importTable->viewport(),scanPaths);new FileDropFilter(folder,scanPaths);
    for(auto it=slotButtons.cbegin();it!=slotButtons.cend();++it)new FileDropFilter(it.value(),[&,slot=it.key()](QStringList paths){if(watcher.isRunning())return;if(paths.size()==1&&QFileInfo(paths.front()).suffix().compare("png",Qt::CaseInsensitive)==0){selected=slot;refreshSlots();path->setText(paths.front());assign->click();}else scanPaths(paths);});
    if(!droppedPaths.isEmpty())QTimer::singleShot(0,&dialog,[&,droppedPaths]{scanPaths(droppedPaths);});
    root->addWidget(status);root->addWidget(buttons);fillConsonants();
    connect(buttons->button(QDialogButtonBox::Cancel),&QPushButton::clicked,&dialog,&QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&]{try{
        key->apply(draft.performance.analysis);
        QVector<int> offsets;for(const auto &token:scale->text().split(QRegularExpression("[,，\\s]+"),Qt::SkipEmptyParts)){bool ok=false;int n=token.toInt(&ok);if(!ok)throw Failure("Invalid custom scale");offsets.append(n);}draft.performance.analysis.customDegreeScale=offsets;
        draft.performance.settings.rules=readRules();validateAnalysisSettings(draft.performance.analysis);validatePerformance(draft);validateConsonants(draft.rules.consonants);dialog.accept();
    }catch(const std::exception &e){status->setText(messageText(uiLanguage,exceptionMessage(e)));}});
    const int result=dialog.exec();scanCancel->store(true);if(watcher.isRunning())watcher.waitForFinished();mouthSettingsOpen=false;QTimer::singleShot(0,this,[this]{ensureWaveform();});if(result!=QDialog::Accepted)return;
    if(defaults->isChecked()){try{preferences.performanceSettings=draft.performance.settings;auto &rules=preferences.performanceSettings.rules;rules.erase(std::remove_if(rules.begin(),rules.end(),[](const auto &r){return r.target.startsWith("group:")||!r.track.isEmpty();}),rules.end());savePreferences(preferences);}catch(const std::exception &e){error(exceptionMessage(e));}}
    const bool mouthChanged=draft.rules.consonants!=project.rules.consonants;
    const bool performanceChanged=draft.score.hasMusic&&draft.appearance.enabled&&!performanceCurrent(draft);
    if(!mouthChanged&&!performanceChanged){change(actionText("Advanced variant settings"),[draft](Project &p){p=draft;});return;}
    Project next=draft;
    loadIsRegenerate=true;cancel=std::make_shared<std::atomic_bool>(false);auto token=cancel;setBusy(true);
    auto report=taskProgress();
    loadWatcher.setFuture(QtConcurrent::run([next,token,report,mouthChanged,performanceChanged]()mutable{LoadResult r;try{
        if(mouthChanged)next.regenerate(token.get(),report);
        if(performanceChanged&&!token->load()){
            auto plan=generatePerformance(next,token.get(),report);
            if(!plan.error.isEmpty())throw Failure(plan.error);
            if(!plan.cancelled&&!token->load())applyPerformanceDraft(next,plan);
        }
        r.project=std::move(next);r.cancelled=token->load();
    }catch(const std::exception &e){r.cancelled=token->load();r.error=exceptionMessage(e);}return r;}));
}
}
