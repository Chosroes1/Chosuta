// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include <QInputMethod>
#include <cmath>
#include <algorithm>
namespace chosuta {
    class DictionaryChoiceDelegate:public QStyledItemDelegate {
        QList<QPair<QString,QString>> choices;
        public:
        DictionaryChoiceDelegate(QList<QPair<QString,QString>> values,QObject *parent):QStyledItemDelegate(parent),choices(std::move(values)){}
        QWidget *createEditor(QWidget *parent,const QStyleOptionViewItem&,const QModelIndex&)const override {
            auto editor=new QComboBox(parent);for(const auto &choice:choices)editor->addItem(choice.first,choice.second);return editor;
        }
        void setEditorData(QWidget *widget,const QModelIndex &index)const override {
            auto editor=qobject_cast<QComboBox*>(widget);editor->setCurrentIndex(editor->findData(index.data(Qt::UserRole)));
        }
        void setModelData(QWidget *widget,QAbstractItemModel *model,const QModelIndex &index)const override {
            auto editor=qobject_cast<QComboBox*>(widget);model->setData(index,editor->currentText());model->setData(index,editor->currentData(),Qt::UserRole);
        }
    };
    bool Window::eventFilter(QObject *object,QEvent *event) {
        if(event->type()==QEvent::LocaleChange&&preferences.language=="auto") {
            const auto resolved=resolveUiLanguage("auto",QLocale::system().uiLanguages());
            if(resolved!=uiLanguage)QTimer::singleShot(0,this,[this,resolved] {
                stopPlayback();uiLanguage=resolved;buildUi();refresh();
            });
        }
        auto widget=qobject_cast<QWidget*>(object);
        if(widget&&widget->window()==this&&event->type()==QEvent::MouseButtonPress&&timeline&&timeline->editingText()&&widget!=subtitleText&&!(subtitleText&&subtitleText->isAncestorOf(widget)))timeline->finishSubtitleEditing(false);
        if(timelineSplitter&&event->type()==QEvent::MouseButtonRelease&&object==timelineSplitter->handle(1))saveViewPreferences();
        if(widget==subtitleText&&(event->type()==QEvent::ShortcutOverride||event->type()==QEvent::KeyPress)){
            auto key=static_cast<QKeyEvent*>(event);
            if(key->matches(QKeySequence::Undo)||key->matches(QKeySequence::Redo)){
                if(event->type()==QEvent::KeyPress){QGuiApplication::inputMethod()->commit();if(key->matches(QKeySequence::Undo))history.undo();else history.redo();}
                event->accept();return true;
            }
        }
        if(widget&&widget->window()==this&&(event->type()==QEvent::ShortcutOverride||event->type()==QEvent::KeyPress||event->type()==QEvent::KeyRelease)) {
            auto key=static_cast<QKeyEvent*>(event);
            bool textEditor=qobject_cast<QTextEdit*>(widget)||qobject_cast<QPlainTextEdit*>(widget);
            if(qobject_cast<QLineEdit*>(widget)&&!qobject_cast<QAbstractSpinBox*>(widget->parentWidget()))textEditor=true;
            if(key->key()==Qt::Key_Space&&key->modifiers()==Qt::NoModifier&&!textEditor) {
                if(event->type()==QEvent::KeyPress&&!key->isAutoRepeat())togglePlayback();
                event->accept();
                return true;
            }
        }
        return QMainWindow::eventFilter(object,event);
    }
    void Window::followCursor(bool force) {
        if(!timelineScroll||(!playing&&!force))return;
        auto bar=timelineScroll->horizontalScrollBar();
        const int x=qRound(timeline->xAtTime(playTime)),width=timelineScroll->viewport()->width();
        const int margin=std::min(40,width/5);
        if(x<bar->value()+margin||x>bar->value()+width-margin)bar->setValue(std::max(0,x-width/4));
    }
    void Window::showPreferences() {
        if(busy)return;
        stopPlayback();
        QDialog dialog(this);
        dialog.setObjectName("preferencesDialog");
        dialog.setWindowTitle(trText("Preferences"));
        auto form=new QFormLayout(&dialog);
        auto choice=new QComboBox;
        choice->setObjectName("uiLanguageChoice");
        choice->addItem(trText("System language"),"auto");
        choice->addItem("中文","zh");
        choice->addItem("English","en");
        choice->addItem("日本語","ja");
        choice->setCurrentIndex(choice->findData(preferences.language));
        form->addRow(trText("Interface language"),choice);
        auto mode=new QComboBox;
        mode->setObjectName("pauseMode");
        mode->addItem(trText("Resume from pause"),false);
        mode->addItem(trText("Return to marker"),true);
        mode->setCurrentIndex(preferences.returnOnPause?1:0);
        form->addRow(trText("Pause behavior"),mode);
        auto marker=new QDoubleSpinBox;
        marker->setObjectName("returnPosition");
        marker->setRange(0,21600);
        marker->setDecimals(3);
        marker->setValue(project.playbackReturnPosition);
        form->addRow(trText("Return position (seconds)"),marker);
        auto here=new QPushButton(trText("Set return point"));
        form->addRow(here);
        connect(here,&QPushButton::clicked,&dialog,[=,this] {
            marker->setValue(playTime);
        });
        auto buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
        form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted)return;
        Preferences next=preferences;
        next.language=choice->currentData().toString();
        next.returnOnPause=mode->currentData().toBool();
        try {
            savePreferences(next);
        }catch(const Failure&e) {
            error(QString::fromUtf8(e.what()));
            return;
        }
        preferences=next;
        if(project.playbackReturnPosition!=marker->value())change(trText("Set return point"),[value=marker->value()](Project&p) {
            p.playbackReturnPosition=value;
        });
        auto resolved=resolveUiLanguage(preferences.language,QLocale::system().uiLanguages());
        if(resolved!=uiLanguage) {
            const int tab=tabs->currentIndex();
            const auto selected=timeline->selectedIds();
            uiLanguage=resolved;
            buildUi();
            refresh();
            tabs->setCurrentIndex(tab);
            timeline->selectIds(selected);
            followCursor(true);
        }
    }
    void Window::showAdvancedSettings() {
        if(busy)return;
        stopPlayback();
        QDialog dialog(this);
        dialog.setObjectName("advancedSettingsDialog");
        dialog.setWindowTitle(trText("Advanced settings"));dialog.resize(840,560);
        auto layout=new QVBoxLayout(&dialog);
        auto pages=new QTabWidget;pages->setObjectName("advancedSettingsTabs");layout->addWidget(pages);
        auto dictionaryPage=new QWidget;auto dictionaryLayout=new QVBoxLayout(dictionaryPage);
        auto hint=new QLabel(trText("Custom readings override built-in readings. Note corrections and explicit SVP phonemes keep priority. Regenerate to apply."));
        hint->setWordWrap(true);dictionaryLayout->addWidget(hint);
        auto table=new QTableWidget(0,4);table->setObjectName("dictionaryTable");
        table->setHorizontalHeaderLabels({trText("Language"),trText("Word / phrase"),trText("Notation"),trText("Reading / phonemes")});
        table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::Stretch);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);dictionaryLayout->addWidget(table);
        table->setItemDelegateForColumn(0,new DictionaryChoiceDelegate({{"English","en"},{"中文","zh"},{"日本語","ja"}},table));
        table->setItemDelegateForColumn(2,new DictionaryChoiceDelegate({{trText("Kana / pinyin / word"),"reading"},{trText("Phonemes"),"phonemes"}},table));
        table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
        auto appendRow=[&](const QString&language,const QString&word,const DictionaryReading&reading) {
            QSignalBlocker block(table);
            const int row=table->rowCount();table->insertRow(row);
            auto languageItem=new QTableWidgetItem(language=="en"?"English":language=="zh"?"中文":"日本語");
            languageItem->setData(Qt::UserRole,language);table->setItem(row,0,languageItem);
            table->setItem(row,1,new QTableWidgetItem(word));
            auto notationItem=new QTableWidgetItem(trText(reading.phonemes?"Phonemes":"Kana / pinyin / word"));
            notationItem->setData(Qt::UserRole,reading.phonemes?"phonemes":"reading");table->setItem(row,2,notationItem);
            table->setItem(row,3,new QTableWidgetItem(reading.text));
        };
        auto fill=[&](const PronunciationOptions &options) {
            table->setRowCount(0);
            for(auto language=options.words.cbegin();language!=options.words.cend();++language)
                for(auto word=language->cbegin();word!=language->cend();++word)appendRow(language.key(),word.key(),word.value());
        };
        fill(project.rules.pronunciation);
        auto rowButtons=new QHBoxLayout;
        auto add=new QPushButton(trText("Add word"));add->setObjectName("dictionaryAdd");
        auto remove=new QPushButton(trText("Remove selected"));remove->setObjectName("dictionaryRemove");
        auto import=new QPushButton(trText("Import dictionary"));auto exportButton=new QPushButton(trText("Export dictionary"));
        for(auto button:{add,remove,import,exportButton})rowButtons->addWidget(button);
        dictionaryLayout->addLayout(rowButtons);pages->addTab(dictionaryPage,trText("Pronunciation dictionary"));
        auto japanesePage=new QWidget;auto japaneseLayout=new QVBoxLayout(japanesePage);
        auto kanji=new QCheckBox(trText("Estimate Japanese kanji readings (optional)"));kanji->setObjectName("japaneseKanji");
        kanji->setChecked(project.rules.pronunciation.japaneseKanji);japaneseLayout->addWidget(kanji);
        auto kanjiHint=new QLabel(trText("Japanese uses kana by default. This small word table cannot guarantee kanji readings; unknown words still need correction. Explicit custom readings work even with this option off."));
        kanjiHint->setWordWrap(true);japaneseLayout->addWidget(kanjiHint);japaneseLayout->addStretch();pages->addTab(japanesePage,trText("Japanese"));
        auto defaults=new QCheckBox(trText("Also save as defaults for new SVP imports"));defaults->setObjectName("dictionaryDefaults");defaults->setChecked(true);layout->addWidget(defaults);
        auto status=new QLabel;status->setObjectName("dictionaryStatus");status->setWordWrap(true);layout->addWidget(status);
        auto rawOptions=[&] {
            QJsonArray entries;
            for(int row=0;row<table->rowCount();++row) {
                auto language=table->item(row,0)->data(Qt::UserRole).toString();
                const auto word=table->item(row,1)->text().trimmed(),reading=table->item(row,3)->text().trimmed();
                if(word.isEmpty()&&reading.isEmpty())continue;
                entries.append(QJsonObject{{"language",language},{"word",word},{"reading",reading},
                    {"notation",table->item(row,2)->data(Qt::UserRole).toString()}});
            }
            return QJsonObject{{"version",1},{"japaneseKanji",kanji->isChecked()},{"entries",entries}};
        };
        auto updateSize=[&] {
            const auto bytes=builtinDictionaryBytes()+QJsonDocument(rawOptions()).toJson(QJsonDocument::Compact).size();
            status->setText(trText("Dictionary data: %1 / 3,000,000 bytes").arg(bytes));
        };
        connect(table,&QTableWidget::itemChanged,&dialog,[&]{updateSize();});
        connect(add,&QPushButton::clicked,&dialog,[&] {
            if(table->rowCount()>=20000) {status->setText(trText("Dictionary exceeds 20000 entries"));return;}
            appendRow("en",{},{{},true});table->setCurrentCell(table->rowCount()-1,1);updateSize();
        });
        connect(remove,&QPushButton::clicked,&dialog,[&] {
            QSet<int>selected;for(const auto&index:table->selectionModel()->selectedRows())selected.insert(index.row());
            for(int row=table->rowCount()-1;row>=0;--row)if(selected.contains(row))table->removeRow(row);
            updateSize();
        });
        connect(import,&QPushButton::clicked,&dialog,[&] {
            const auto path=QFileDialog::getOpenFileName(&dialog,trText("Import dictionary"),{},"JSON (*.json)");if(path.isEmpty())return;
            try {
                QFile file(path);if(!file.open(QIODevice::ReadOnly))throw Failure(file.errorString());
                if(file.size()>DictionaryByteLimit)throw Failure("Dictionary exceeds 3 MB");
                const auto options=pronunciationOptionsRead(checkedJson(file.readAll()).object());
                QSignalBlocker block(table);fill(options);kanji->setChecked(options.japaneseKanji);updateSize();
            }catch(const Failure&e){status->setText(QString::fromUtf8(e.what()));}
        });
        connect(exportButton,&QPushButton::clicked,&dialog,[&] {
            try {
                const auto options=pronunciationOptionsRead(rawOptions());
                const auto path=QFileDialog::getSaveFileName(&dialog,trText("Export dictionary"),{},"JSON (*.json)");if(path.isEmpty())return;
                QSaveFile file(path);if(!file.open(QIODevice::WriteOnly))throw Failure(file.errorString());
                const auto bytes=QJsonDocument(pronunciationOptionsJson(options)).toJson(QJsonDocument::Compact);
                if(file.write(bytes)!=bytes.size()||!file.commit())throw Failure(file.errorString());
            }catch(const Failure&e){status->setText(QString::fromUtf8(e.what()));}
        });
        auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->setObjectName("advancedSettingsButtons");layout->addWidget(buttons);
        buttons->button(QDialogButtonBox::Save)->setText(trText("Save"));
        buttons->button(QDialogButtonBox::Cancel)->setText(trText("Cancel"));
        connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,[&] {
            try {
                const auto options=pronunciationOptionsRead(rawOptions());
                if(!project.rules.englishDictionary.isEmpty()&&QFileInfo(project.rules.englishDictionary).size()+pronunciationDictionaryBytes(options)>DictionaryByteLimit)
                    throw Failure("Pronunciation dictionaries exceed 3 MB");
                if(defaults->isChecked()) {auto next=preferences;next.pronunciation=options;savePreferences(next);preferences=next;}
                if(project.rules.pronunciation!=options)change(trText("Advanced settings"),[options](Project&p){p.rules.pronunciation=options;});
                dialog.accept();
            }catch(const Failure&e){status->setText(QString::fromUtf8(e.what()));}
        });
        updateSize();dialog.exec();
    }
    QWidget *Window::buildCanvasPanel() {
        auto scroll=new QScrollArea;
        scroll->setWidgetResizable(true);
        auto page=new QWidget;
        scroll->setWidget(page);
        auto form=new QFormLayout(page);
        canvasAspect=new QComboBox;
        canvasAspect->setObjectName("canvasAspect");
        canvasAspect->addItem("16:9",QSize(1280,720));
        canvasAspect->addItem("9:16",QSize(720,1280));
        canvasAspect->addItem("4:3",QSize(960,720));
        canvasAspect->addItem("1:1",QSize(720,720));
        canvasAspect->addItem(trText("Custom"),QSize());
        form->addRow(trText("Aspect ratio"),canvasAspect);
        canvasWidth=new QSpinBox;
        canvasHeight=new QSpinBox;
        canvasWidth->setRange(16,7680);
        canvasHeight->setRange(16,4320);
        canvasWidth->setObjectName("canvasWidth");
        canvasHeight->setObjectName("canvasHeight");
        form->addRow(trText("Width"),canvasWidth);
        form->addRow(trText("Height"),canvasHeight);
        connect(canvasAspect,&QComboBox::currentIndexChanged,this,[this] {
            if(refreshing||busy)return;auto size=canvasAspect->currentData().toSize();if(!size.isEmpty())change(trText("Canvas"),[size](Project&p) {
                p.canvas.width=size.width();p.canvas.height=size.height();
            });
        });
        for(auto spin: {
            canvasWidth,canvasHeight
        })connect(spin,&QSpinBox::editingFinished,this,[this] {
            if(!refreshing&&!busy&&(project.canvas.width!=canvasWidth->value()||project.canvas.height!=canvasHeight->value()))change(trText("Canvas"),[this](Project&p) {
                p.canvas.width=canvasWidth->value();p.canvas.height=canvasHeight->value();validateCanvas(p.canvas);
            });
        });
        canvasDuration=new QDoubleSpinBox;
        canvasDuration->setObjectName("canvasDuration");
        canvasDuration->setRange(0,21600);
        canvasDuration->setDecimals(3);
        canvasDuration->setSingleStep(1);
        canvasDuration->setSpecialValueText(trText("Auto (score)"));
        form->addRow(trText("Animation duration"),canvasDuration);
        connect(canvasDuration,&QDoubleSpinBox::editingFinished,this,[this] {
            if(!refreshing&&!busy&&project.output.duration!=canvasDuration->value())change(trText("Animation duration"),[this](Project&p) {
                p.output.duration=canvasDuration->value();
            });
        });
        auto useAudio=new QPushButton(trText("Use audio duration"));
        useAudio->setObjectName("useAudioDuration");
        form->addRow(useAudio);
        connect(useAudio,&QPushButton::clicked,this,[this] {
            if(!busy&&project.audioDuration>0&&project.audioDuration+project.output.audioOffset>0&&project.audioDuration+project.output.audioOffset<=21600)change(trText("Animation duration"),[](Project&p) {
                p.output.duration=p.audioDuration+p.output.audioOffset;
            });
        });
        backgroundColor=new QPushButton;
        form->addRow(trText("Background"),backgroundColor);
        connect(backgroundColor,&QPushButton::clicked,this,[this] {
            if(busy)return;auto color=QColorDialog::getColor(project.canvas.background,this,trText("Background"));if(color.isValid())change(trText("Background"),[color](Project&p) {
                p.canvas.background=color;
            });
        });
        canvasTransparent=new QCheckBox(trText("Transparent canvas"));
        form->addRow(canvasTransparent);
        connect(canvasTransparent,&QCheckBox::toggled,this,[this](bool checked) {
            if(!refreshing&&!busy)change(trText("Canvas"),[checked](Project&p) {
                p.canvas.transparent=checked;
            });
        });
        backgroundPath=new QLineEdit;
        backgroundPath->setObjectName("backgroundPath");
        backgroundPath->setReadOnly(true);
        form->addRow(trText("Background image"),backgroundPath);
        auto choose=new QPushButton(trText("Choose image"));
        form->addRow(choose);
        connect(choose,&QPushButton::clicked,this,[this] {
            if(busy)return;auto path=QFileDialog::getOpenFileName(this,trText("Background image"), {
            },"Images (*.png *.jpg *.jpeg *.webp *.bmp)");if(!path.isEmpty())change(trText("Background image"),[path](Project&p) {
                p.canvas.backgroundImage=path;
            });
        });
        auto remove=new QPushButton(trText("Remove image"));
        form->addRow(remove);
        connect(remove,&QPushButton::clicked,this,[this] {
            if(!busy)change(trText("Background image"),[](Project&p) {
                p.canvas.backgroundImage.clear();
            });
        });
        backgroundFit=new QComboBox;
        backgroundFit->setObjectName("backgroundFit");
        backgroundFit->addItem(trText("Cover"),"cover");
        backgroundFit->addItem(trText("Contain"),"contain");
        backgroundFit->addItem(trText("Stretch"),"stretch");
        form->addRow(trText("Background fit"),backgroundFit);
        connect(backgroundFit,&QComboBox::currentIndexChanged,this,[this] {
            if(!refreshing&&!busy)change(trText("Background fit"),[this](Project&p) {
                p.canvas.backgroundFit=backgroundFit->currentData().toString();
            });
        });
        characterScale=new QDoubleSpinBox;
        characterX=new QDoubleSpinBox;
        characterY=new QDoubleSpinBox;
        characterScale->setObjectName("characterScale");
        characterX->setObjectName("characterX");
        characterY->setObjectName("characterY");
        characterScale->setRange(1,1000);
        characterX->setRange(-200,300);
        characterY->setRange(-200,300);
        for(auto spin: {
            characterScale,characterX,characterY
        }) {
            spin->setDecimals(1);
            spin->setSingleStep(1);
        }
        form->addRow(trText("Character scale (%)"),characterScale);
        form->addRow(trText("Character center X (%)"),characterX);
        form->addRow(trText("Character center Y (%)"),characterY);
        for(auto spin: {
            characterScale,characterX,characterY
        })connect(spin,&QDoubleSpinBox::valueChanged,this,[this] {
            if(!refreshing&&!busy)change(trText("Canvas"),[this](Project&p) {
                p.canvas.characterScale=characterScale->value()/100.;p.canvas.characterX=characterX->value()/100.;p.canvas.characterY=characterY->value()/100.;
            });
        });
        auto reset=new QPushButton(trText("Reset layout"));
        form->addRow(reset);
        connect(reset,&QPushButton::clicked,this,[this] {
            if(!busy)change(trText("Canvas"),[](Project&p) {
                p.canvas.characterScale=1;p.canvas.characterX=p.canvas.characterY=.5;
            });
        });
        return scroll;
    }
    void Window::refreshCanvas() {
        canvasWidth->setValue(project.canvas.width);
        canvasHeight->setValue(project.canvas.height);
        canvasDuration->setValue(project.output.duration);
        int index=canvasAspect->count()-1;
        for(int i=0;i<canvasAspect->count()-1;++i) {
            auto size=canvasAspect->itemData(i).toSize();
            if(qint64(size.width())*project.canvas.height==qint64(size.height())*project.canvas.width) {
                index=i;
                break;
            }
        }
        canvasAspect->setCurrentIndex(index);
        canvasTransparent->setChecked(project.canvas.transparent);
        backgroundPath->setText(project.canvas.backgroundImage);
        backgroundFit->setCurrentIndex(backgroundFit->findData(project.canvas.backgroundFit));
        backgroundColor->setText(project.canvas.background.name());
        backgroundColor->setStyleSheet("background-color:"+project.canvas.background.name()+";color:"+(project.canvas.background.lightness()<128?"white":"black"));
        characterScale->setValue(project.canvas.characterScale*100);
        characterX->setValue(project.canvas.characterX*100);
        characterY->setValue(project.canvas.characterY*100);
        canvasLabel->setText(QString("%1 × %2 · %3 · %4 s").arg(project.canvas.width).arg(project.canvas.height).arg(canvasAspect->currentText()).arg(project.duration(),0,'f',3));
        findChild<QPushButton*>("useAudioDuration")->setEnabled(project.audioDuration>0&&project.audioDuration+project.output.audioOffset>0&&project.audioDuration+project.output.audioOffset<=21600&&!busy);
    }
}
