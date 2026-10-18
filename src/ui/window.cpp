// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include "text.h"
#include <QInputMethod>
#include "core/executable.h"
#include <QtConcurrent/QtConcurrentRun>
#include <cmath>
namespace chosuta {
    class ProjectCommand:public TranslatedCommand {
        Window *window;
        Project before,after;
        bool timingOnly=false;
        public:
        ProjectCommand(Window*w,Project a,Project b,ActionText label,bool onlyTiming=false):TranslatedCommand(label,w->uiLanguage),window(w),before(std::move(a)),after(std::move(b)),timingOnly(onlyTiming){}
        void undo()override {
            window->assignProject(before,timingOnly);
        }
        void redo()override {
            window->assignProject(after,timingOnly);
        }
    };
    QString Window::trText(const char *key,const char *zh,const char *ja)const{return uiText(uiLanguage,key,zh,ja);}
    Window::Window() {
        preferences=loadPreferences();
        project.rules.pronunciation=preferences.pronunciation;project.performance.settings=preferences.performanceSettings;
        uiLanguage=resolveUiLanguage(preferences.language,QLocale::system().uiLanguages());
        setWidgetLanguage(uiLanguage);
        qApp->installEventFilter(this);
        setAcceptDrops(true);
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
            if(!finishTask())return;
            if(result.cancelled||(cancel&&cancel->load())) {
                statusBar()->showMessage(trText("Cancelled"));
                return;
            }
            if(!result.error.isEmpty()) {
                if(!loadIsRegenerate&&result.error.key=="VVPROJ contains both talk and song; choose a section to import"){
                    QMessageBox choice(this);choice.setObjectName("vvprojSectionDialog");
                    choice.setWindowTitle(trText("Import VOICEVOX content","导入 VOICEVOX 内容","VOICEVOX の内容を読み込む"));
                    choice.setText(trText("This project contains songs and dialogue. Choose which to import.","此工程同时包含歌曲和对白，请选择要导入的内容。","このプロジェクトには歌とセリフがあります。読み込む内容を選択してください。"));
                    auto song=choice.addButton(trText("Song","歌曲","ソング"),QMessageBox::AcceptRole);
                    auto talk=choice.addButton(trText("Dialogue","对白","トーク"),QMessageBox::AcceptRole);
                    song->setObjectName("vvprojSong");talk->setObjectName("vvprojTalk");
                    choice.addButton(QMessageBox::Cancel);choice.exec();
                    if(choice.clickedButton()==song)openPath(activePath,"song");
                    else if(choice.clickedButton()==talk)openPath(activePath,"talk");
                    return;
                }
                error(result.error);
                return;
            }
            if(loadIsRegenerate)history.push(new ProjectCommand(this,project,result.project,actionText("Generate")));
            else {
                projectPath=activePath.endsWith(".chosuta",Qt::CaseInsensitive)?activePath:QString{};
                history.clear();
                playTime=playOrigin=playStart=0;
                assignProject(result.project);
                history.setClean();
            }
        });
        connect(&exportWatcher,&QFutureWatcher<ExportResult>::finished,this,[this] {
            auto result=exportWatcher.result();
            if(!finishTask())return;
            if(result.cancelled)statusBar()->showMessage(trText("Cancelled"));
            else if(!result.success)error(result.error);
            else statusBar()->showMessage(trText("Export complete")+": "+activePath+(result.diagnostics.isEmpty()?QString{}:" — "+messagesText(uiLanguage,result.diagnostics)));
        });
        connect(&audioWatcher,&QFutureWatcher<AudioInfo>::finished,this,[this]{
            auto r=audioWatcher.result();if(!finishTask())return;
            if(r.cancelled){statusBar()->showMessage(trText("Cancelled"));return;}
            if(!r.error.isEmpty()){error(r.error);return;}
            double audioEnd=std::max(0.,r.duration+project.output.audioOffset);bool extend=false;
            if(r.duration>0&&audioEnd>(evaluation?evaluation->duration:project.duration())+.001&&audioEnd<=21600){
                extend=QMessageBox::question(this,trText("Extend animation"),trText("Audio is longer than animation. Extend to %1 seconds?").arg(audioEnd,0,'f',3),QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes;
            }
            change(actionText("Audio"),[r,audioEnd,extend](Project&p){p.audioPath=r.path;p.audioDuration=r.duration;if(extend)p.output.duration=audioEnd;});
            if(!r.warning.isEmpty())statusBar()->showMessage(messageText(uiLanguage,r.warning));
        });
        connect(&waveformWatcher,&QFutureWatcher<WaveformResult>::finished,this,[this]{
            auto r=waveformWatcher.result();const bool valid=finishTask();if(!valid&&!r.cancelled)return;waveform=r.wave;
            if(!waveform) {
                QFileInfo info(project.audioPath);
                waveformKey=project.audioPath+"\n"+QString::number(info.size())+"\n"+QString::number(info.lastModified().toMSecsSinceEpoch())+"\n"+resolveExecutable(project.output.ffmpeg);
            }
            waveformMessage=r.cancelled?trText("Waveform preparation cancelled. Reload to retry."):r.error.isEmpty()?QString{}:trText("Waveform unavailable: %1").arg(messageText(uiLanguage,r.error));
            refreshWaveform();refreshSelection();refreshPreview();
            if(!waveformMessage.isEmpty())statusBar()->showMessage(waveformMessage);
        });
        connect(&correctionWatcher,&QFutureWatcher<WaveCorrectionResult>::finished,this,[this]{
            auto r=correctionWatcher.result();if(!finishTask())return;
            if(r.cancelled){statusBar()->showMessage(trText("Cancelled"));return;}
            if(!r.error.isEmpty()){error(r.error);return;}
            change(actionText("Correct timing from waveform"),[r](Project &p){p.timing=r.timing;},true);
            statusBar()->showMessage(trText("Waveform correction: %1 changed, %2 protected, %3 unclear, %4 conflicting.").arg(r.accepted).arg(r.protectedSources).arg(r.unclear).arg(r.conflicts));
        });
        playbackTimer.setInterval(16);
        connect(&playbackTimer,&QTimer::timeout,this,[this] {
            playTime=playOrigin+playbackClock.elapsed()/1000.;
            if(playTime>=(evaluation?evaluation->duration:project.duration())) {
                stopPlayback();
                playTime=(evaluation?evaluation->duration:project.duration());
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
        auto transport=menuBar()->addMenu(trText("Transport","运行控制","トランスポート"));transport->setObjectName("transportMenu");
        auto help=menuBar()->addMenu(trText("Help"));
        auto action=[&](QMenu*menu,const char*name,const std::function<void()>&fn,QKeySequence shortcut={}) {
            auto a=menu->addAction(trText(name));
            if(!shortcut.isEmpty())a->setShortcut(shortcut);
            connect(a,&QAction::triggered,this,fn);
            return a;
        };
        action(file,"Open score / Project",[this] {
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
        })->setObjectName("exportSettingsAction");
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
        auto playAction=transport->addAction(trText("Play / Stop","播放／停止","再生／停止"));playAction->setObjectName("transportToggleAction");connect(playAction,&QAction::triggered,this,&Window::togglePlayback);
        auto returnAction=transport->addAction(trText("Return to playback start on Stop","停止时回到本次播放起点","停止時に再生開始位置へ戻る"));returnAction->setObjectName("returnOnStopAction");returnAction->setCheckable(true);returnAction->setChecked(preferences.returnOnStop);connect(returnAction,&QAction::toggled,this,[this](bool v){preferences.returnOnStop=v;saveViewPreferences();});
        action(help,"Guide",[this] {
            QMessageBox::information(this,trText("Guide"),trText("Open an SVP, UST or VSQX, select tracks, generate, import PNGs, edit the timeline, save, and export. Drag event edges to resize; drag the middle to move. Ctrl-click selects multiple events. Ctrl-wheel zooms. Priority: lower number wins. Corrections are estimates; unknown words remain marked. Demo assets are original test drawings."));
        });
        auto central=new QWidget;
        auto root=new QVBoxLayout(central);
        setCentralWidget(central);
        performanceDraftBar=new QWidget;performanceDraftBar->setObjectName("performanceDraftBar");auto draftRow=new QHBoxLayout(performanceDraftBar);draftRow->setContentsMargins(0,0,0,0);performanceDraftBar->hide();
        draftRow->addWidget(new QLabel(trText("Compare draft (not applied)","比较草稿（尚未应用）","案を比較（未適用）")));
        for(int i=0;i<4;++i){QStringList labels={trText("Current","当前","現在"),trText("Draft","草稿","案"),trText("Apply","应用","適用"),trText("Cancel","取消","キャンセル")};auto b=new QPushButton(labels[i]);b->setObjectName("performanceReview"+QString::number(i));draftRow->addWidget(b);connect(b,&QPushButton::clicked,this,[this,i]{if(i<2)reviewPerformance(i==1);else finishPerformanceDraft(i==2);});}
        auto canvasBanner=new QHBoxLayout;root->addLayout(canvasBanner);canvasBanner->addWidget(performanceDraftBar);
        auto canvasButton=new QPushButton(trText("Canvas"));canvasButton->setObjectName("canvasButton");canvasBanner->addWidget(canvasButton);
        canvasLabel=new QLabel;canvasBanner->addWidget(canvasLabel);canvasBanner->addStretch();performanceGenerateTop=new QPushButton(trText("Generate performance","生成演出","演出を生成"));canvasBanner->addWidget(performanceGenerateTop);connect(performanceGenerateTop,&QPushButton::clicked,this,[this]{generatePerformanceDraft();});
        connect(canvasButton,&QPushButton::clicked,this,[this]{tabs->setCurrentWidget(canvasPage);});
        timelineSplitter=new QSplitter(Qt::Vertical);timelineSplitter->setObjectName("timelineSplitter");root->addWidget(timelineSplitter,1);
        auto horizontal=new QSplitter;workspaceSplitter=horizontal;horizontal->setObjectName("workspaceSplitter");horizontal->setChildrenCollapsible(false);timelineSplitter->setChildrenCollapsible(false);timelineSplitter->addWidget(horizontal);
        tabs=new QTabWidget;
        tabs->setMinimumWidth(300);tabs->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Expanding);
        auto sidebar=new QWidget;auto sidebarLayout=new QVBoxLayout(sidebar);sidebarLayout->setContentsMargins(0,0,0,0);
        auto pages=new QComboBox;pages->setObjectName("workspacePageChoice");sidebarLayout->addWidget(pages);sidebarLayout->addWidget(tabs);tabs->tabBar()->hide();horizontal->addWidget(sidebar);
        connect(pages,&QComboBox::currentIndexChanged,this,[this,pages]{if(pages->currentIndex()>=0)tabs->setCurrentIndex(pages->currentData().toInt());});
        connect(tabs,&QTabWidget::currentChanged,this,[this,pages](int current){QSignalBlocker block(pages);pages->clear();for(int i=0;i<tabs->count();++i)if(tabs->isTabVisible(i))pages->addItem(tabs->tabText(i),i);pages->setCurrentIndex(pages->findData(current));});
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
            history.push(new ProjectCommand(this,project,next,actionText("Tracks")));
            Q_UNUSED(item);
        });
        auto assetPage=new QWidget;
        auto assetLayout=new QVBoxLayout(assetPage);
        advancedMouth=new QCheckBox(trText("Enable advanced variants"));advancedMouth->setObjectName("advancedMouthEnabled");assetLayout->addWidget(advancedMouth);
        connect(advancedMouth,&QCheckBox::toggled,this,[this](bool value){if(!refreshing&&!busy)change(actionText("Advanced variant settings"),[value](Project &p){p.appearance.enabled=value;});});
        auto mouthSettings=new QPushButton(trText("Advanced variant settings"));mouthSettings->setObjectName("advancedMouthButton");assetLayout->addWidget(mouthSettings);connect(mouthSettings,&QPushButton::clicked,this,[this]{showMouthSettings();});
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
        connect(fallback,&QComboBox::currentTextChanged,this,[this](const QString&) {
            const auto id=selectedId(fallback);
            if(!refreshing&&!id.isEmpty())change(actionText("Fallback"),[id](Project&p) {
                p.fallback=id;
            });
        });
        connect(assets,&QTableWidget::cellDoubleClicked,this,[this](int row,int) {
            QString id=assets->item(row,0)->data(Qt::UserRole).toString();
            auto path=QFileDialog::getOpenFileName(this,trText("Import PNGs"),{},"PNG (*.png)");
            if(!path.isEmpty())change(actionText("Assets"),[=](Project&p) {
                auto scan=scanAssetFiles({path},{});if(scan.entries.size()!=1||scan.entries.front().status=="invalid")throw Failure("Invalid PNG");scan.entries[0].id=id;applyAssetImport(p,scan.entries);
            });
        });
        tabs->addTab(assetPage,trText("Assets"));
        performancePage=buildPerformancePanel();tabs->addTab(performancePage,trText("Performance","演出","演出"));
        canvasPage=buildCanvasPanel();tabs->addTab(canvasPage,trText("Canvas"));
        auto rulePage=new QScrollArea;rulePage->setWidgetResizable(true);rulePage->setObjectName("rulesPage");auto ruleBody=new QWidget;rulePage->setWidget(ruleBody);
        auto ruleLayout=new QVBoxLayout(ruleBody);
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
        special->setMinimumHeight(210);
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
            for(const auto &id:mouthStates())s->addItem(mouthLabel(uiLanguage,id),id);
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
            if(!path.isEmpty())change(actionText("English Dictionary"),[=](Project&p) {
                p.rules.englishDictionary=path;
            });
        });
        tabs->addTab(rulePage,trText("Rules"));
        // Store panel changes in the undoable project immediately; generation remains an explicit operation.
        auto updateRules=[this] {
            if(refreshing)return;
            Project next=project;
            collectRules(next);
            history.push(new ProjectCommand(this,project,next,actionText("Rules")));
        };
        connect(language,&QComboBox::currentIndexChanged,this,updateRules);
        connect(takeover,&QCheckBox::toggled,this,updateRules);
        connect(consonantLimit,qOverload<double>(&QDoubleSpinBox::valueChanged),this,updateRules);
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
        connect(preview,&PreviewCanvas::layoutEdited,this,[this](Project p){changeSubtitles(actionText("Canvas"),[p](Project&next){next.canvas=p.canvas;next.subtitles=p.subtitles;});});
        auto playerRow=new QHBoxLayout;
        rightLayout->addLayout(playerRow);
        playButton=button(playerRow,"Play / Stop",[this] {
            togglePlayback();
        },"play");
        button(playerRow,"Attach Audio",[this] {
            attachAudio();
        });
        button(playerRow,"Remove Audio",[this] {
            stopPlayback();
            change(actionText("Remove Audio"),[](Project&p) {
                p.audioPath.clear();p.audioDuration=0;
            });
            if(player)player->setSource({});
        });
        position=new QDoubleSpinBox;
        position->setObjectName("playPosition");
        position->setRange(0,21600);
        position->setDecimals(3);
        position->setSingleStep(.05);
        position->setToolTip(trText("Position (seconds)"));
        playerRow->addWidget(position);
        timeLabel=new QLabel;timeLabel->setMinimumWidth(80);
        playerRow->addWidget(timeLabel);
        connect(position,&QDoubleSpinBox::valueChanged,this,[this](double t) {
            if(!refreshing)seek(t);
        });
        selectionLabel=new QLabel(trText("Selection"));
        selectionLabel->setObjectName("selectionLabel");selectionLabel->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
        auto mouthScroll=new QScrollArea;mouthScroll->setWidgetResizable(true);mouthScroll->setObjectName("mouthPropertiesPage");mouthPage=mouthScroll;
        auto mouthBody=new QWidget;mouthScroll->setWidget(mouthBody);auto mouthLayout=new QVBoxLayout(mouthBody);mouthLayout->addWidget(selectionLabel);
        mouthProperties=new QWidget;mouthProperties->setObjectName("mouthProperties");auto mouthFields=new QVBoxLayout(mouthProperties);mouthFields->setContentsMargins(0,0,0,0);mouthLayout->addWidget(mouthProperties);mouthLayout->addStretch();
        auto properties=new QFormLayout;properties->setRowWrapPolicy(QFormLayout::WrapLongRows);mouthFields->addLayout(properties);
        tabs->addTab(mouthPage,trText("Mouth","口形","口形"));
        auto labeled=[&](const char*text,QWidget*w){properties->addRow(trText(text),w);};
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
        anchor=new QComboBox;anchor->setObjectName("eventAnchor");
        anchor->addItem(trText("Fixed seconds"),"seconds");
        anchor->addItem(trText("Follow beats"),"beats");
        labeled("Anchor",anchor);
        lock=new QCheckBox(trText("Locked"));
        lock->setChecked(true);
        properties->addRow(lock);fixedImage=new QCheckBox(trText("Fixed image","固定整图","画像を固定"));fixedImage->setObjectName("fixedImage");properties->addRow(fixedImage);
        auto editButtons=new QGridLayout;
        mouthFields->addLayout(editButtons);
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
            editButtons->addWidget(b,i/2,i%2);
            connect(b,&QPushButton::clicked,this,functions[i]);
        }
        horizontal->setSizes( {
            390,810
        });
        auto timelineRow=new QHBoxLayout;
        auto timelinePanel=new QWidget;auto timelineLayout=new QVBoxLayout(timelinePanel);timelineLayout->setContentsMargins(0,0,0,0);timelineLayout->addLayout(timelineRow);timelineSplitter->addWidget(timelinePanel);timelineSplitter->setStretchFactor(0,1);timelineSplitter->setStretchFactor(1,0);
        auto units=new QComboBox;units->setObjectName("timelineUnits");
        units->addItems( {
            trText("Seconds"),trText("Bars / Beats")
        });
        timelineRow->addWidget(units);
        auto snap=new QComboBox;snap->setObjectName("cursorSnap");snap->addItem(trText("Cursor: free","游标：不吸附","カーソル：自由"),"off");snap->addItem(trText("Cursor: beat","游标：吸附节拍","カーソル：拍に吸着"),"beat");snap->addItem(trText("Cursor: syllable","游标：吸附音节","カーソル：音節に吸着"),"syllable");snap->setCurrentIndex(snap->findData(preferences.cursorSnap));snap->setToolTip(trText("Ruler clicks and drags snap. Hold Alt to bypass. Typed positions and playback stay exact.","标尺点击、拖动时吸附；按住 Alt 临时关闭。手动输入位置和播放不吸附。","目盛のクリック・ドラッグに適用。Altで一時解除。数値入力と再生は吸着しません。"));timelineRow->addWidget(snap);connect(snap,&QComboBox::currentIndexChanged,this,[this,snap]{preferences.cursorSnap=snap->currentData().toString();timeline->setCursorSnap(preferences.cursorSnap);saveViewPreferences();});

        timelineRow->addWidget(new QLabel(trText("Zoom")));
        auto zoom=new QSlider(Qt::Horizontal);
        zoom->setRange(5,1000);
        zoom->setValue(120);
        zoom->setMaximumWidth(200);
        timelineRow->addWidget(zoom);
        subtitleEnabled=new QCheckBox(trText("Add subtitles"));subtitleEnabled->setObjectName("subtitlesEnabled");timelineRow->addWidget(subtitleEnabled);
        connect(subtitleEnabled,&QCheckBox::toggled,this,[this](bool enabled){if(refreshing||busy)return;changeSubtitles(actionText("Subtitles"),[this,enabled](Project&p){p.subtitlesEnabled=enabled;if(enabled&&p.subtitles.isEmpty()){SubtitleTrack t;t.id=QUuid::createUuid().toString(QUuid::Id128);t.name=trText("Subtitle track")+" 1";if(p.score.tracks.size()==1)t.sourceTrack=p.score.tracks[0].id;p.subtitles.append(t);}});if(enabled&&subtitlePage)tabs->setCurrentWidget(subtitlePage);});
        auto resetHeights=new QPushButton(trText("Restore timeline heights"));resetHeights->setObjectName("resetTimelineHeights");timelineRow->addWidget(resetHeights);connect(resetHeights,&QPushButton::clicked,this,[this]{timeline->resetLaneHeights();preferences.timelineHeight=250;timelineSplitter->setSizes({600,290});saveViewPreferences();});
        timelineRow->addStretch();
        button(canvasBanner,"Export Settings",[this] {
            showExportSettings();
        });
        button(canvasBanner,"Export Video",[this] {
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
        scroll->setWidgetResizable(true);
        scroll->setMinimumHeight(185);
        timeline=new Timeline;timeline->setCursorSnap(preferences.cursorSnap);
        scroll->setWidget(timeline);
        timelineLayout->addWidget(scroll);
        timeline->setLaneHeights(preferences.mouthLaneHeight,preferences.subtitleLaneHeight);
        timeline->setWaveformHeight(preferences.waveformLaneHeight);
        timeline->setPerformanceLane(preferences.performanceLaneHeight,preferences.performanceLaneVisible);
        connect(timeline,&Timeline::performanceSelected,this,&Window::selectPerformance);
        connect(timeline,&Timeline::performanceEdited,this,[this](QString id,double a,double b){if(busy)return;for(auto m:project.performance.manual)if(m.id==id){change(actionText("Move performance","移动演出段","演出区間を移動"),[=](Project &p)mutable{setPerformanceTime(p,m,a,b,m.anchor=="beats");putPerformanceSegment(p,m);},true);break;}});
        timelineSplitter->setSizes({600,preferences.timelineHeight+40});
        connect(timelineSplitter,&QSplitter::splitterMoved,this,[this]{preferences.timelineHeight=std::clamp(timelineScroll->height(),180,900);});
        connect(timeline,&Timeline::laneHeightChanged,this,[this](const QString&id,int h){if(id.isEmpty())preferences.mouthLaneHeight=h;else if(id=="@waveform")preferences.waveformLaneHeight=h;else if(id=="@performance")preferences.performanceLaneHeight=h;else preferences.subtitleLaneHeight=h;updateTimelineMinimum();saveViewPreferences();});
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
            performanceId.clear();subtitleCueId.clear();if(subtitlePage){bool old=refreshing;refreshing=true;refreshSubtitles();refreshing=old;}
            refreshSelection();if(!timeline->selectedIds().isEmpty())tabs->setCurrentWidget(mouthPage);
        });
        connect(timeline,&Timeline::subtitleCreated,this,&Window::addSubtitle);
        connect(timeline,&Timeline::subtitleSelected,this,&Window::selectSubtitle);
        connect(timeline,&Timeline::subtitleEdited,this,[this](QString id,SubtitleCue cue){changeSubtitles(actionText("Subtitles"),[id,cue](Project&p){for(auto&t:p.subtitles)if(t.id==id)for(auto&c:t.cues)if(c.id==cue.id)c=cue;});});
        connect(timeline,&Timeline::editRejected,this,[this](const Message&m){statusBar()->showMessage(messageText(uiLanguage,m),7000);});
        connect(scroll->verticalScrollBar(),&QScrollBar::valueChanged,timeline,qOverload<>(&QWidget::update));
        connect(timeline,&Timeline::seek,this,[this](double t) {
            seek(t);
        });
        connect(timeline,&Timeline::edited,this,[this](const QVector<Event>&events) {
            QString a=anchor->currentData().toString();
            bool locked=lock->isChecked(),fixed=fixedImage->isChecked();
            change(actionText("Selection"),[=](Project&p) {
                for(const auto&e:events)p.edit(e,locked,a);
            });
        });
        cancelButton->setText(trText("Cancel"));
    }
    static bool supportedDrop(const QMimeData *mime) {
        if(!mime->hasUrls()||mime->urls().isEmpty())return false;
        bool images=true;
        for(const auto &url:mime->urls()){
            if(!url.isLocalFile())return false;QFileInfo file(url.toLocalFile());if(file.isSymLink()||!file.exists())return false;
            if(file.isDir())return mime->urls().size()==1;
            auto suffix=file.suffix().toLower();
            if(!QStringList{"png","svp","ust","ustx","vsq","vsqx","musicxml","xml","mxl","ccs","vpr","lab","ds","vvproj","chosuta","wav","flac","mp3","ogg","m4a","aac"}.contains(suffix))return false;
            images=images&&suffix=="png";
        }
        return images||mime->urls().size()==1;
    }
    void Window::dragEnterEvent(QDragEnterEvent *event){if(!busy&&supportedDrop(event->mimeData()))event->acceptProposedAction();else event->ignore();}
    void Window::dropEvent(QDropEvent *event){
        if(busy||!supportedDrop(event->mimeData())){event->ignore();return;}
        QStringList paths;for(const auto &url:event->mimeData()->urls())paths.append(url.toLocalFile());event->acceptProposedAction();
        QTimer::singleShot(0,this,[this,paths]{if(busy)return;QFileInfo file(paths.front());auto suffix=file.suffix().toLower();if(file.isDir()||suffix=="png")showMouthSettings(paths);else if(suffix=="svp"||suffix=="ust"||suffix=="ustx"||suffix=="vsq"||suffix=="vsqx"||suffix=="musicxml"||suffix=="xml"||suffix=="mxl"||suffix=="ccs"||suffix=="vpr"||suffix=="lab"||suffix=="ds"||suffix=="vvproj"||suffix=="chosuta"){if(confirmDiscard())openPath(paths.front());}else attachAudioPath(paths.front());});
    }
    void Window::assignProject(Project p,bool timingOnly) {
        project=std::move(p);
        refresh(timingOnly);
    }
    void Window::refresh(bool timingOnly) {
        const auto horizontalSizes=workspaceSplitter->sizes(),verticalSizes=timelineSplitter->sizes();
        refreshing=true;
        rebuildEvaluation();
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
            auto count=new QTableWidgetItem(QString::number(project.score.hasMusic?t.notes.size():t.timedPhones.size()));
            count->setFlags(Qt::ItemIsEnabled);
            tracks->setItem(i,2,count);
        }
        tracks->horizontalHeaderItem(2)->setText(project.score.hasMusic?trText("Notes"):trText("Phonemes","音素","音素"));
        for(auto combo:{anchor,findChild<QComboBox *>("timelineUnits")}){
            if(auto model=qobject_cast<QStandardItemModel *>(combo->model()))model->item(1)->setEnabled(project.score.hasMusic);
            if(!project.score.hasMusic)combo->setCurrentIndex(0);
        }
        if(auto snap=findChild<QComboBox *>("cursorSnap")){
            if(auto model=qobject_cast<QStandardItemModel *>(snap->model()))model->item(snap->findData("beat"))->setEnabled(project.score.hasMusic);
            QSignalBlocker block(snap);const auto mode=!project.score.hasMusic&&preferences.cursorSnap=="beat"?QString("off"):preferences.cursorSnap;
            snap->setCurrentIndex(snap->findData(mode));timeline->setCursorSnap(mode);
            snap->setItemText(snap->findData("syllable"),project.score.hasMusic?trText("Cursor: syllable","游标：吸附音节","カーソル：音節に吸着"):trText("Cursor: phoneme","游标：吸附音素","カーソル：音素に吸着"));
        }
        advancedMouth->setChecked(project.appearance.enabled);refreshPerformance();
        QStringList shapes= {
            "A","I","U","E","O","closed","rest","breath","unknown"
        };
        for(const auto&s:project.assets.keys())if(!shapes.contains(s))shapes<<s;
        QStringList simpleShapes;
        for(const auto &id:shapes)if(!id.contains("/"))simpleShapes.append(id);
        assets->setRowCount(simpleShapes.size());
        for(int i=0;i<simpleShapes.size();++i) {
            auto s=simpleShapes[i];
            auto label=new QTableWidgetItem(mouthLabel(uiLanguage,s));label->setData(Qt::UserRole,s);assets->setItem(i,0,label);
            auto item=new QTableWidgetItem(project.assets.value(s));
            if(!item->text().isEmpty()&&!QFileInfo::exists(item->text()))item->setForeground(Qt::red);
            assets->setItem(i,1,item);
        }
        fallback->clear();
        for(const auto &id:shapes)fallback->addItem(mouthLabel(uiLanguage,id),id);
        selectId(fallback,project.fallback);
        auto previousShape=selectedId(shape);
        shape->clear();
        for(const auto &id:shapes)shape->addItem(mouthLabel(uiLanguage,id),id);
        selectId(shape,previousShape.isEmpty()?"A":previousShape);
        language->setCurrentIndex(language->findData(project.rules.language));
        takeover->setChecked(project.rules.harmonyTakeover);
        consonant->setValue(project.rules.consonantRatio);
        consonantLimit->setValue(project.rules.consonantMaxSeconds*1000);
        for(int i=0;i<special->rowCount();++i) {
            auto policy=project.rules.special.value(special->item(i,0)->data(Qt::UserRole).toString());
            auto mode=qobject_cast<QComboBox*>(special->cellWidget(i,1));
            mode->setCurrentIndex(mode->findData(policy.mode));
            selectId(qobject_cast<QComboBox*>(special->cellWidget(i,2)),policy.shape);
            qobject_cast<QDoubleSpinBox*>(special->cellWidget(i,3))->setValue(policy.holdSeconds);
        }
        if(timingOnly&&scene)scene->setTiming(project,evaluation);
        else scene=std::make_unique<Scene>(project,evaluation);
        QStringList messages {
            project.score.hasMusic?trText("Estimated timing"):project.score.sourceFormat=="vvproj"?trText("Query timing and mouth shapes are estimates","query 时序与口形均为推导估计","query の時刻と口形は推定です"):trText("Supplied phoneme timing; mouth shapes estimated","来源音素时序；口形为规则估计","元の音素時刻・口形はルールによる推定")
        };
        for(const auto&d:project.score.diagnostics)messages<<d.path+"\n"+d.code+": "+messageText(uiLanguage,d.message);
        if(!scene->diagnostics.isEmpty())messages<<trText("Scene diagnostics")+": "+messagesText(uiLanguage,scene->diagnostics);
        if(project.appearance.enabled){messages.append(messagesText(uiLanguage,evaluation->performance->diagnostics));if(project.score.hasMusic&&!performanceCurrent(project))messages<<trText("Automatic performance needs regeneration.","自动演出待生成或更新。","自動演出の生成・更新が必要です。");}
        auto orphans=project.orphanOverrides();
        if(!orphans.isEmpty())messages<<trText("Orphan manual edits")+"\n"+orphans.join('\n');
        int unknown=0;
        for(const auto&e:evaluation->events)if(e.unknown) {
            ++unknown;
            messages<<(e.provenance=="ust-alias-unresolved/estimated"
                ?messageText(uiLanguage,Message::format("Unmapped UST alias: %1. Add an alias preset or correct its reading.",{e.text}))
                :trText("Unknown pronunciations")+": "+e.text)+"\n"+e.source;
        }
        messages<<QString("%1: %2").arg(trText("Unknown pronunciations")).arg(unknown);
        if(!project.audioPath.isEmpty())messages<<trText("Audio")+": "+project.audioPath;
        if(!project.timing.sources.isEmpty()&&!timingCorrectionCurrent(project))messages<<trText("Saved waveform correction is stale; score timing is used. Correct again if needed.");
        diagnostics->setPlainText(messages.join("\n\n"));
        refreshCanvas();
        timeline->setProject(project,evaluation);
        refreshSubtitles();
        timeline->setReturnPosition(playStart);
        refreshWaveform();
        if(auto pages=findChild<QComboBox *>("workspacePageChoice")){QSignalBlocker block(pages);pages->clear();for(int i=0;i<tabs->count();++i)if(tabs->isTabVisible(i))pages->addItem(tabs->tabText(i),i);pages->setCurrentIndex(pages->findData(tabs->currentIndex()));}
        refreshing=false;
        refreshSelection();
        refreshPreview();
        centralWidget()->layout()->activate();
        workspaceSplitter->setSizes(horizontalSizes);timelineSplitter->setSizes(verticalSizes);
    }
    void Window::refreshPreview() {
        if(!scene)return;
        preview->setState(performanceCandidate&&performanceDraftShown?*performanceCandidate:project,scene.get(),playTime,!playing&&!busy);
        timeline->setEditable(!playing&&!busy);
        if(correctTimingButton)correctTimingButton->setEnabled(waveform&&waveform->path==project.audioPath&&!project.generated.isEmpty()&&!playing&&!busy);
        if(revertTimingButton)revertTimingButton->setEnabled(!project.timing.sources.isEmpty()&&!playing&&!busy);
        timeline->setCursor(playTime);
        bool old=refreshing;
        refreshing=true;
        position->setValue(playTime);
        if(playing)followCursor();
        refreshing=old;
        timeLabel->setText(project.score.hasMusic?QString("%1 s | %2").arg(playTime,0,'f',3).arg(project.score.time.beatLabel(project.score.time.blicks(playTime-project.output.syncOffset))):QString("%1 s").arg(playTime,0,'f',3));
    }
    void Window::refreshSelection() {
        auto ids=timeline->selectedIds();
        selectionLabel->setText(ids.isEmpty()?trText("Select a mouth event on the timeline","在时间轴选择口形事件以编辑","タイムラインで口形イベントを選択してください"):trText("Selection")+QString(" (%1)").arg(ids.size()));
        mouthProperties->setVisible(!ids.isEmpty());
        for(const auto&e:evaluation->events)if(ids.contains(e.id)) {
            start->setValue(e.start);
            end->setValue(e.end);
            selectId(shape,e.shape);fixedImage->setChecked(e.appearanceFixed);
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
            qobject_cast<QComboBox*>(special->cellWidget(i,1))->currentData().toString(),selectedId(qobject_cast<QComboBox*>(special->cellWidget(i,2))),qobject_cast<QDoubleSpinBox*>(special->cellWidget(i,3))->value()
        };
    }
    void Window::change(ActionText label,const std::function<void(Project&)>&fn,bool timingOnly) {
        if(busy)return;
        try {
            Project next=project;
            fn(next);
            history.push(new ProjectCommand(this,project,std::move(next),label,timingOnly));
        }
        catch(const std::exception&e) {
            error(exceptionMessage(e));
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
    void Window::rebuildEvaluation(){++evaluationBuilds;evaluation=std::make_shared<Evaluation>(project);}
    Progress Window::taskProgress(){return [this](int value){QMetaObject::invokeMethod(this,[this,value]{if(busy)progress->setValue(value);},Qt::QueuedConnection);};}
    bool Window::finishTask(){
        setBusy(false);
        if(cancel&&cancel->load()){statusBar()->showMessage(trText("Cancelled"));return false;}
        return true;
    }
    void Window::error(const Message &message) {
        QMessageBox box(QMessageBox::Warning,trText("Error"),messageText(uiLanguage,message,false),QMessageBox::Ok,this);
        box.setTextFormat(Qt::PlainText);
        const auto detail=messageDetails(uiLanguage,message);
        if(!detail.isEmpty())box.setDetailedText(detail);
        box.exec();
    }
    bool Window::confirmDiscard() {
        if(timeline)timeline->finishSubtitleEditing(false);
        if(history.isClean()&&!performanceCandidate)return true;
        auto prompt=performanceCandidate?trText("Save the current project before closing? The unapplied performance draft will be discarded.","关闭前保存当前工程吗？尚未应用的演出草稿将被丢弃。","閉じる前に現在のプロジェクトを保存しますか？未適用の演出案は破棄されます。"):trText("Save before closing?");
        auto result=QMessageBox::question(this,trText("Unsaved changes"),prompt,QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel);
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
            error(exceptionMessage(e));
            return false;
        }
    }
    void Window::openDialog() {
        if(!confirmDiscard())return;
        auto path=QFileDialog::getOpenFileName(this,trText("Open score / Project"),{},trText("Sources / Chosuta (*.svp *.ust *.ustx *.vsq *.vsqx *.musicxml *.xml *.mxl *.ccs *.vpr *.lab *.ds *.vvproj *.chosuta)"));
        if(!path.isEmpty())openPath(path);
    }
    void Window::openPath(const QString&path,const QString&vvprojSection) {
        if(busy)return;
        activePath=path;
        loadIsRegenerate=false;
        cancel=std::make_shared<std::atomic_bool>(false);
        auto token=cancel;
        setBusy(true);
        auto report=taskProgress();
        loadWatcher.setFuture(QtConcurrent::run([path,token,report,vvprojSection,defaults=preferences.pronunciation,performanceDefaults=preferences.performanceSettings] {
            LoadResult r;
            try {
                if(path.endsWith(".chosuta",Qt::CaseInsensitive))r.project=loadProject(path);
                else {
                    r.project.rules.pronunciation=defaults;r.project.performance.settings=performanceDefaults;
                    r.project.score=importSource(path,token.get(),report,vvprojSection);
                    if(!r.project.score.tracks.isEmpty())r.project.selected= {
                        r.project.score.tracks[0].id
                    };
                    r.project.regenerate(token.get());
                }
                r.cancelled=token->load();
            }
            catch(const std::exception&e) {
                r.cancelled=token->load();
                r.error=exceptionMessage(e);
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
                r.error=exceptionMessage(e);
            }
            return r;
        }));
    }
    void Window::importImages() {
        auto files=QFileDialog::getOpenFileNames(this,trText("Import PNGs"),{},"PNG (*.png)");
        if(files.isEmpty())return;
        showMouthSettings(files);
    }

    void Window::createDemo() {
        auto directory=QFileDialog::getExistingDirectory(this,trText("Demo Assets"));
        if(directory.isEmpty())return;
        try {
            createDemoAssets(directory);
            change(actionText("Demo Assets"),[directory](Project&p) {
                for(const auto&s:QStringList {
                    "A","I","U","E","O","closed","rest","breath","unknown"
                })p.assets[s]=QDir(directory).filePath(s+".png");
            });
        }
        catch(const std::exception&e) {
            error(exceptionMessage(e));
        }
    }
    void Window::relocateAssets() {
        auto directory=QFileDialog::getExistingDirectory(this,trText("Relocate Assets"));
        if(directory.isEmpty())return;
        change(actionText("Relocate Assets"),[directory](Project&p) {
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
        if(!path.isEmpty())change(actionText("Custom Shape"),[=](Project&p) {
            auto scan=scanAssetFiles({path},{});if(scan.entries.size()!=1||scan.entries.front().status=="invalid")throw Failure("Invalid PNG");scan.entries[0].id=id.trimmed();applyAssetImport(p,scan.entries);
        });
    }
    void Window::attachAudio(){
        auto path=QFileDialog::getOpenFileName(this,trText("Attach Audio"),{},"Audio (*.wav *.flac *.mp3 *.ogg *.m4a);;All (*)");
        if(!path.isEmpty())attachAudioPath(path);
    }
    void Window::attachAudioPath(const QString&path){
        if(busy)return;
        try{auto tools=selectMediaTools(project.output.ffmpeg);stopPlayback();cancel=std::make_shared<std::atomic_bool>(false);auto token=cancel;
            setBusy(true);audioWatcher.setFuture(QtConcurrent::run([path,tools,token]{return probeAudio(path,tools,*token);}));
        }catch(const std::exception &e){error(exceptionMessage(e));}
    }
    void Window::editSelection() {
        auto ids=timeline->selectedIds();
        if(ids.isEmpty()) {
            error(Message("No event selected"));
            return;
        }
        double a=start->value(),b=end->value();
        QString s=selectedId(shape),anch=anchor->currentData().toString();
        bool locked=lock->isChecked(),fixed=fixedImage->isChecked();
        change(actionText("Apply to selection"),[=](Project&p) {
            auto events=p.effective();
            for(auto&e:events)if(ids.contains(e.id)) {
                if(ids.size()==1) {
                    e.start=a;
                    e.end=b;
                }
                e.shape=s;e.appearanceFixed=fixed;
                e.unknown=s=="unknown";
                p.edit(e,locked,anch);
            }
        });
    }
    void Window::splitSelection() {
        if(!performanceId.isEmpty()){auto id=performanceId;double t=playTime;change(actionText("Split performance","拆分演出段","演出区間を分割"),[=](Project &p){splitPerformance(p,id,t);},true);return;}
        auto ids=timeline->selectedIds();
        if(ids.size()!=1) {
            error(Message("Select one event"));
            return;
        }
        double cursor=playTime-project.output.syncOffset;
        change(actionText("Split at cursor"),[=](Project&p) {
            for(const auto&e:p.effective())if(e.id==ids[0]) {
                p.split(e,cursor);
                break;
            }
        });
    }
    void Window::mergeSelection() {
        if(!performanceId.isEmpty()){mergePerformance();return;}
        auto ids=timeline->selectedIds();
        change(actionText("Merge selection"),[=](Project&p) {
            QVector<Event>events;
            for(const auto&e:p.effective())if(ids.contains(e.id))events.append(e);
            p.merge(events);
        });
    }
    void Window::offsetSelection() {
        auto ids=timeline->selectedIds();
        if(ids.isEmpty()) {
            error(Message("No event selected"));
            return;
        }
        bool ok=false;
        double offset=QInputDialog::getDouble(this,trText("Batch offset"),trText("Offset in seconds"),0,-21600,21600,6,&ok);
        if(!ok)return;
        QString anch=anchor->currentData().toString();
        bool locked=lock->isChecked(),fixed=fixedImage->isChecked();
        change(actionText("Batch offset"),[=](Project&p) {
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
            error(Message("No event selected"));
            return;
        }
        QString source;
        for(const auto&e:evaluation->events)if(ids.contains(e.id)) {
            source=e.source.section(';',0,0);
            break;
        }
        bool ok=false;
        auto reading=QInputDialog::getText(this,trText("Correct pronunciation"),(project.score.hasMusic?trText("Explicit pronunciation for source note"):trText("Replacement phoneme (optional language prefix, e.g. ja/a)","替换音素（可带语言前缀，例如 ja/a）","置き換える音素（言語接頭辞も可：ja/a）")),QLineEdit::Normal,project.rules.readings.value(source),&ok);
        if(!ok)return;
        change(actionText("Correct pronunciation"),[=](Project&p) {
            if(reading.trimmed().isEmpty())p.rules.readings.remove(source);
            else p.rules.readings[source]=reading.trimmed();
        });
        regenerate();
    }
    void Window::resetOverrides() {
        change(actionText("Reset manual edits"),[](Project&p) {
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
        if(busy&&!performanceCandidate)return;
        if(playing) {
            playTime=std::min((evaluation?evaluation->duration:project.duration()),playOrigin+playbackClock.elapsed()/1000.);
            stopPlayback();
            if(preferences.returnOnStop)playTime=playStart;
            syncAudio();refreshPreview();followCursor(true);
            return;
        }
        if(playTime>=(evaluation?evaluation->duration:project.duration()))playTime=0;
        timeline->finishSubtitleEditing(false);
        playing=true;
        playOrigin=playTime;playStart=playTime;timeline->setReturnPosition(playStart);
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
        if(performanceCandidate){
            if(!confirmDiscard()){e->ignore();return;}
            finishPerformanceDraft(false);e->accept();return;
        }
        if(busy){if(cancel)cancel->store(true);statusBar()->showMessage(trText("Cancelling…","正在取消…","中止中…"));e->ignore();return;}
        if(confirmDiscard())e->accept();else e->ignore();
    }

    void Window::showExportSettings() {
        QDialog dialog(this);
        dialog.setWindowTitle(trText("Export Settings"));
        dialog.setObjectName("exportSettingsDialog");
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
        auto globalFfmpeg=new QLineEdit(defaultFfmpegExecutable());globalFfmpeg->setObjectName("defaultFfmpegPath");
        globalFfmpeg->setPlaceholderText(trText("Automatic: bundled tools, then PATH"));
        auto overrideTool=new QCheckBox(trText("Override for this project"));overrideTool->setObjectName("overrideFfmpeg");
        overrideTool->setChecked(s.ffmpeg!="ffmpeg"&&s.ffmpeg!="ffmpeg.exe");
        auto ffmpeg=new QLineEdit(overrideTool->isChecked()?s.ffmpeg:QString{});ffmpeg->setObjectName("projectFfmpegPath");
        auto toolRow=[&](QLineEdit *field,const char *label,const char *name){
            auto row=new QWidget;auto h=new QHBoxLayout(row);h->setContentsMargins(0,0,0,0);h->addWidget(field);
            auto browse=new QPushButton(trText("Browse…"));browse->setObjectName(name);h->addWidget(browse);
            connect(browse,&QPushButton::clicked,&dialog,[&,field]{
                auto path=QFileDialog::getOpenFileName(&dialog,trText("FFmpeg executable"),field->text());
                if(!path.isEmpty())field->setText(QDir::toNativeSeparators(path));
            });
            form->addRow(trText(label),row);return row;
        };
        toolRow(globalFfmpeg,"Default FFmpeg (this computer)","browseDefaultFfmpeg");
        form->addRow(overrideTool);
        auto projectToolRow=toolRow(ffmpeg,"FFmpeg executable","browseProjectFfmpeg");projectToolRow->setEnabled(overrideTool->isChecked());
        connect(overrideTool,&QCheckBox::toggled,projectToolRow,&QWidget::setEnabled);
        auto toolHint=new QLabel(trText("Choose ffmpeg; ffprobe and shared DLLs should stay beside it. The default is saved only on this computer when you press OK."));
        toolHint->setWordWrap(true);layout->addWidget(toolHint);
        auto check=new QPushButton(trText("Check media tools"));check->setObjectName("checkMediaTools");layout->addWidget(check);
        auto report=new QPlainTextEdit;report->setObjectName("mediaToolsReport");report->setReadOnly(true);report->setMaximumHeight(160);report->hide();layout->addWidget(report);
        auto inspector=new QFutureWatcher<MediaToolsInfo>(&dialog);auto checkCancel=std::make_shared<std::atomic_bool>(false);
        connect(&dialog,&QDialog::finished,&dialog,[checkCancel]{checkCancel->store(true);});
        connect(check,&QPushButton::clicked,&dialog,[&]{
            auto selected=(overrideTool->isChecked()?ffmpeg->text():globalFfmpeg->text()).trimmed();
            // Resolve automatic discovery without the old saved global default while editing it.
            if(selected.isEmpty()||selected=="ffmpeg"||selected=="ffmpeg.exe")selected=automaticFfmpegExecutable();
            check->setEnabled(false);report->show();report->setPlainText(trText("Checking…"));checkCancel->store(false);
            inspector->setFuture(QtConcurrent::run([selected,checkCancel]{return inspectMediaTools(selectMediaTools(selected),*checkCancel);}));
        });
        connect(inspector,&QFutureWatcher<MediaToolsInfo>::finished,&dialog,[&]{
            auto info=inspector->result();check->setEnabled(true);QStringList lines;
            lines<<"FFmpeg: "+info.ffmpeg<<info.ffmpegVersion<<"ffprobe: "+info.ffprobe<<info.ffprobeVersion;
            for(const auto &message:info.errors)lines<<messageText(uiLanguage,message);
            if(!info.missingEncoders.isEmpty())lines<<trText("Missing encoders")+": "+info.missingEncoders.join(", ");
            if(!info.missingFilters.isEmpty())lines<<trText("Missing filters")+": "+info.missingFilters.join(", ");
            if(info.ready())lines<<trText("Tool startup and listed capabilities passed. Export/playback still require a real media test.");
            report->setPlainText(lines.join('\n'));
        });
        auto estimate=new QLabel;
        layout->addWidget(estimate);
        auto updateEstimate=[&] {
            if(bitrate->value()==0)estimate->setText(trText("Quality mode: size varies"));
            else estimate->setText(trText("Estimated size")+QString(": ~%1 MB\n").arg(bitrate->value()*((evaluation?evaluation->duration:project.duration()))/8000.,0,'f',2)+trText("Size estimate excludes audio/container"));
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
                s.ffmpeg=overrideTool->isChecked()?ffmpeg->text().trimmed():QString("ffmpeg");
                if(s.ffmpeg.isEmpty())throw Failure("FFmpeg project path is empty");
                setDefaultFfmpegExecutable(globalFfmpeg->text());
                waveformKey.clear();
                change(actionText("Export Settings"),[=](Project&p) {
                    p.output=s;
                });
                dialog.accept();
            }
            catch(const std::exception&e) {
                error(exceptionMessage(e));
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
                if(answer==QMessageBox::Yes)changeSubtitles(actionText("Animation duration"),[end](Project&p){p.output.duration=end;});
            }
        }
        try {
            validateExport(project);
        }
        catch(const std::exception&e) {
            error(exceptionMessage(e));
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
        std::optional<MediaTools> tools;
        try{tools.emplace(selectMediaTools(project.output.ffmpeg));}catch(const std::exception &e){error(exceptionMessage(e));return;}
        activePath=path;
        cancel=std::make_shared<std::atomic_bool>(false);
        auto token=cancel;
        auto snapshot=project;
        setBusy(true);auto report=taskProgress();
        exportWatcher.setFuture(QtConcurrent::run([snapshot,path,token,overwrite,tools,report] {
            return exportVideo(snapshot,*tools,path,*token,report,overwrite);
        }));
    }
    bool Window::runSmokeWorkflow(const QString&directory) {
        try {
            // Original synthetic content, kept separate from private user projects.
            QByteArray source=R"({"version":196,"time":{"tempo":[{"position":0,"bpm":120}],"meter":[{"index":0,"numerator":4,"denominator":4}]},"tracks":[{"name":"Original smoke","mainGroup":{"uuid":"smoke","notes":[{"uuid":"a","onset":0,"duration":705600000,"pitch":60,"lyrics":"あ","phonemes":"a"},{"uuid":"i","onset":705600000,"duration":705600000,"pitch":60,"lyrics":"い","phonemes":"i"}]},"mainRef":{"database":{"language":"japanese"}}}]})";
            Project p;
            p.output.ffmpeg=automaticFfmpegExecutable();
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
            if(QFileInfo(p.output.ffmpeg).isFile()&&QFileInfo(p.output.ffmpeg).isExecutable())result=exportVideo(project,selectMediaTools(project.output.ffmpeg),QDir(directory).filePath("smoke.mp4"),token);
            if(!result.success) {
                qWarning()<<result.error.text();
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
