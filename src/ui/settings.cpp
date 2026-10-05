// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include <cmath>
namespace chosuta {
    bool Window::eventFilter(QObject *object,QEvent *event) {
        if(event->type()==QEvent::LocaleChange&&preferences.language=="auto") {
            const auto resolved=resolveUiLanguage("auto",QLocale::system().uiLanguages());
            if(resolved!=uiLanguage)QTimer::singleShot(0,this,[this,resolved] {
                stopPlayback();uiLanguage=resolved;buildUi();refresh();
            });
        }
        auto widget=qobject_cast<QWidget*>(object);
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
        Preferences next {
            choice->currentData().toString(),mode->currentData().toBool()
        };
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
