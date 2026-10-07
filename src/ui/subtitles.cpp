// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include <cmath>
namespace chosuta {
static SubtitleTrack *findTrack(Project &p,const QString &id){for(auto&t:p.subtitles)if(t.id==id)return &t;return nullptr;}
static SubtitleCue *findCue(SubtitleTrack &t,const QString &id){for(auto&c:t.cues)if(c.id==id)return &c;return nullptr;}
class SubtitleTextCommand:public QUndoCommand {
    Window *window;
    QString track,cue,before,after;
    QElapsedTimer age;
    int session;
public:
    SubtitleTextCommand(Window*w,QString t,QString c,QString old,QString text):QUndoCommand(w->trText("Edit subtitle text")),window(w),track(std::move(t)),cue(std::move(c)),before(std::move(old)),after(std::move(text)),session(w->subtitleTextSession){age.start();}
    int id()const override{return 0x535542;}
    bool mergeWith(const QUndoCommand *other)override {
        const auto *next=dynamic_cast<const SubtitleTextCommand*>(other);
        if(!next||next->window!=window||next->track!=track||next->cue!=cue||next->session!=session||age.elapsed()>1500)return false;
        after=next->after;age.restart();return true;
    }
    void undo()override{window->writeSubtitleText(track,cue,before);}
    void redo()override{window->writeSubtitleText(track,cue,after);}
};
void Window::writeSubtitleText(const QString &track,const QString &cue,const QString &text) {
    auto t=findTrack(project,track);if(!t)return;auto c=findCue(*t,cue);if(!c)return;c->text=text;
    if(scene)scene->setSubtitleText(cue,text);
    if(timeline)timeline->setSubtitleText(cue,text);
    if(subtitlePage&&track==subtitleTrackId&&cue==subtitleCueId&&subtitleText->toPlainText()!=text){QSignalBlocker block(subtitleText);auto cursor=subtitleText->textCursor();int at=cursor.position();subtitleText->setPlainText(text);cursor=subtitleText->textCursor();cursor.setPosition(std::min(at,int(text.size())));subtitleText->setTextCursor(cursor);}
    refreshPreview();
}
void Window::changeSubtitles(const QString &label,const std::function<void(Project&)> &fn) {
    change(label,[&](Project&p){fn(p);validateCanvas(p.canvas);validateSubtitles(p);});
}
QWidget *Window::buildSubtitlePanel() {
    auto scroll=new QScrollArea;scroll->setObjectName("subtitlePage");scroll->setWidgetResizable(true);
    auto body=new QWidget;auto form=new QFormLayout(body);scroll->setWidget(body);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    subtitleTrackChoice=new QComboBox;subtitleTrackChoice->setObjectName("subtitleTrackChoice");form->addRow(trText("Subtitle track"),subtitleTrackChoice);
    auto buttons=new QHBoxLayout;form->addRow(buttons);
    auto add=new QPushButton(trText("New subtitle track"));add->setObjectName("addSubtitleTrack");buttons->addWidget(add);connect(add,&QPushButton::clicked,this,&Window::addSubtitleTrack);
    auto remove=new QPushButton(trText("Delete subtitle track"));remove->setObjectName("deleteSubtitleTrack");buttons->addWidget(remove);
    connect(remove,&QPushButton::clicked,this,[this]{auto id=subtitleTrackId;changeSubtitles(trText("Delete subtitle track"),[id](Project&p){for(int i=0;i<p.subtitles.size();++i)if(p.subtitles[i].id==id){p.subtitles.removeAt(i);break;}});});
    subtitleName=new QLineEdit;subtitleName->setMaxLength(128);subtitleName->setObjectName("subtitleName");form->addRow(trText("Name"),subtitleName);
    connect(subtitleName,&QLineEdit::editingFinished,this,[this]{if(refreshing||busy)return;auto id=subtitleTrackId,name=subtitleName->text();if(auto t=findTrack(project,id);t&&t->name!=name)changeSubtitles(trText("Subtitles"),[id,name](Project&p){if(auto t=findTrack(p,id))t->name=name;});});
    subtitleTrackEnabled=new QCheckBox(trText("Enable this track"));subtitleTrackEnabled->setObjectName("subtitleTrackEnabled");form->addRow(subtitleTrackEnabled);
    subtitleAlign=new QCheckBox(trText("Align to lyrics"));subtitleAlign->setObjectName("subtitleAlign");form->addRow(subtitleAlign);
    subtitleSource=new QComboBox;subtitleSource->setObjectName("subtitleSource");form->addRow(trText("Lyric source track"),subtitleSource);
    connect(subtitleTrackChoice,&QComboBox::currentIndexChanged,this,[this]{if(!refreshing)selectSubtitle(subtitleTrackChoice->currentData().toString(),{});});
    connect(subtitleTrackEnabled,&QCheckBox::toggled,this,[this](bool value){if(!refreshing){auto id=subtitleTrackId;changeSubtitles(trText("Subtitles"),[id,value](Project&p){if(auto t=findTrack(p,id))t->enabled=value;});}});
    connect(subtitleAlign,&QCheckBox::toggled,this,[this](bool value){if(!refreshing){auto id=subtitleTrackId;changeSubtitles(trText("Align to lyrics"),[id,value](Project&p){if(auto t=findTrack(p,id))t->alignLyrics=value;});}});
    connect(subtitleSource,&QComboBox::currentIndexChanged,this,[this]{if(!refreshing){auto id=subtitleTrackId,source=subtitleSource->currentData().toString();changeSubtitles(trText("Lyric source track"),[id,source](Project&p){if(auto t=findTrack(p,id))t->sourceTrack=source;});}});
    subtitleHint=new QLabel(trText("Double-click an empty subtitle lane to enter text. Apply confirms style and timing. Lyric alignment uses score timing."));subtitleHint->setWordWrap(true);form->addRow(subtitleHint);
    subtitleCuePanel=new QWidget;subtitleCuePanel->setObjectName("subtitleCuePanel");auto cueForm=new QFormLayout(subtitleCuePanel);form->addRow(subtitleCuePanel);
    cueForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);cueForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    subtitleText=timeline->textEditor();subtitleText->setPlaceholderText(trText("Enter subtitle text"));
    auto editText=new QPushButton(trText("Edit text in subtitle block"));editText->setObjectName("editSubtitleText");cueForm->addRow(trText("Text"),editText);connect(editText,&QPushButton::clicked,this,[this]{selectSubtitle(subtitleTrackId,subtitleCueId,true);});
    connect(subtitleText,&QPlainTextEdit::textChanged,this,[this]{
        if(refreshing||busy)return;auto t=findTrack(project,subtitleTrackId);if(!t)return;auto c=findCue(*t,subtitleCueId);if(!c)return;auto text=subtitleText->toPlainText();if(text==c->text)return;
        if(text.size()>4096){QSignalBlocker block(subtitleText);subtitleText->setPlainText(c->text);statusBar()->showMessage(trText("Subtitle text limit is 4096 characters."),7000);return;}
        history.push(new SubtitleTextCommand(this,subtitleTrackId,subtitleCueId,c->text,text));
    });
    auto spin=[&](QFormLayout *layout,const char *label,const char *name,double low,double high,int decimals=3){auto s=new QDoubleSpinBox;s->setObjectName(name);s->setRange(low,high);s->setDecimals(decimals);layout->addRow(trText(label),s);return s;};
    subtitleStart=spin(cueForm,"Start (seconds)","subtitleStart",-21600,21600,6);subtitleEnd=spin(cueForm,"End (seconds)","subtitleEnd",-21600,21600,6);
    subtitleAnchor=new QComboBox;subtitleAnchor->setObjectName("subtitleAnchor");subtitleAnchor->addItem(trText("Fixed seconds"),"seconds");subtitleAnchor->addItem(trText("Follow beats"),"beats");cueForm->addRow(trText("Anchor"),subtitleAnchor);
    subtitleFirst=new QComboBox;subtitleLast=new QComboBox;subtitleFirst->setObjectName("subtitleFirst");subtitleLast->setObjectName("subtitleLast");cueForm->addRow(trText("First lyric"),subtitleFirst);cueForm->addRow(trText("Last lyric"),subtitleLast);
    auto align=new QPushButton(trText("Use lyric range"));align->setObjectName("alignSubtitleRange");cueForm->addRow(align);connect(align,&QPushButton::clicked,this,&Window::alignSubtitleRange);
    subtitleOwnStyle=new QCheckBox(trText("Override style for this subtitle"));subtitleOwnStyle->setObjectName("subtitleOwnStyle");cueForm->addRow(subtitleOwnStyle);
    connect(subtitleOwnStyle,&QCheckBox::toggled,this,[this]{if(!refreshing)updateLayoutTarget();});
    auto removeCue=new QPushButton(trText("Delete subtitle"));removeCue->setObjectName("deleteSubtitle");cueForm->addRow(removeCue);
    connect(removeCue,&QPushButton::clicked,this,&Window::deleteSubtitleCue);
    subtitleFont=new QFontComboBox;subtitleFont->setObjectName("subtitleFont");form->addRow(trText("Font"),subtitleFont);
    subtitleFont->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);subtitleFont->setMinimumContentsLength(12);
    subtitleSize=spin(form,"Font height (% of canvas)","subtitleSize",.5,50);subtitleX=spin(form,"Center X (%)","subtitleX",-200,300);subtitleY=spin(form,"Center Y (%)","subtitleY",-200,300);subtitleWidth=spin(form,"Text width (%)","subtitleWidth",2,400);
    subtitleColor=new QPushButton(trText("Text color"));subtitleColor->setObjectName("subtitleColor");form->addRow(subtitleColor);
    connect(subtitleColor,&QPushButton::clicked,this,[this]{auto c=QColorDialog::getColor(selectedSubtitleColor,this,trText("Text color"),QColorDialog::ShowAlphaChannel);if(c.isValid()){selectedSubtitleColor=c;subtitleColor->setStyleSheet("background-color: "+c.name(QColor::HexArgb));}});
    subtitleBold=new QCheckBox(trText("Bold"));subtitleItalic=new QCheckBox(trText("Italic"));subtitleOutline=new QCheckBox(trText("Outline"));form->addRow(subtitleBold);form->addRow(subtitleItalic);form->addRow(subtitleOutline);
    subtitleTextAlign=new QComboBox;subtitleTextAlign->addItem(trText("Left"),"left");subtitleTextAlign->addItem(trText("Center"),"center");subtitleTextAlign->addItem(trText("Right"),"right");form->addRow(trText("Text alignment"),subtitleTextAlign);
    auto apply=new QPushButton(trText("Apply subtitles"));apply->setObjectName("applySubtitles");form->addRow(apply);connect(apply,&QPushButton::clicked,this,&Window::applySubtitleProperties);
    auto extend=new QPushButton(trText("Extend to subtitle end"));extend->setObjectName("extendToSubtitles");form->addRow(extend);
    connect(extend,&QPushButton::clicked,this,[this]{changeSubtitles(trText("Animation duration"),[](Project&p){double end=p.duration();const auto sources=subtitleSources(p);for(const auto&t:p.subtitles)if(t.enabled)for(const auto&c:t.cues)end=std::max(end,subtitleInterval(p,c,&sources).end);p.output.duration=std::min(21600.,end);});});
    return scroll;
}
void Window::refreshSubtitles() {
    subtitleEnabled->setChecked(project.subtitlesEnabled);
    if(project.subtitlesEnabled&&!subtitlePage){subtitlePage=buildSubtitlePanel();tabs->addTab(subtitlePage,trText("Subtitles"));}
    if(subtitlePage)tabs->setTabVisible(tabs->indexOf(subtitlePage),project.subtitlesEnabled);
    auto t=findTrack(project,subtitleTrackId);
    if(!t){subtitleTrackId=project.subtitles.isEmpty()?QString{}:project.subtitles[0].id;t=findTrack(project,subtitleTrackId);}
    if(t&&!findCue(*t,subtitleCueId))subtitleCueId.clear();
    const auto previous=layoutTarget->currentData().toString();layoutTarget->clear();layoutTarget->addItem(trText("Move character"),QString{});
    if(project.subtitlesEnabled)for(const auto&row:project.subtitles)if(row.enabled)layoutTarget->addItem(trText("Move subtitles")+": "+row.name,row.id);
    layoutTarget->setCurrentIndex(std::max(0,layoutTarget->findData(previous)));
    if(!subtitlePage){updateLayoutTarget();return;}
    subtitleTrackChoice->clear();for(const auto&row:project.subtitles)subtitleTrackChoice->addItem(row.name,row.id);
    subtitleTrackChoice->setCurrentIndex(subtitleTrackChoice->findData(subtitleTrackId));
    subtitlePage->setEnabled(!busy);subtitleName->setEnabled(t);subtitleTrackEnabled->setEnabled(t);subtitleAlign->setEnabled(t);subtitleSource->setEnabled(t);
    subtitleSource->clear();subtitleSource->addItem(trText("Choose a lyric source"),QString{});for(const auto&row:project.score.tracks)subtitleSource->addItem(row.name,row.id);
    if(!t){subtitleCuePanel->hide();updateLayoutTarget();return;}
    subtitleName->setText(t->name);subtitleTrackEnabled->setChecked(t->enabled);subtitleAlign->setChecked(t->alignLyrics);subtitleSource->setCurrentIndex(std::max(0,subtitleSource->findData(t->sourceTrack)));
    auto cue=findCue(*t,subtitleCueId);subtitleCuePanel->setVisible(cue);
    if(cue){auto v=subtitleInterval(project,*cue);if(subtitleText->toPlainText()!=cue->text){QSignalBlocker block(subtitleText);subtitleText->setPlainText(cue->text);}subtitleStart->setValue(v.start);subtitleEnd->setValue(v.end);subtitleAnchor->setCurrentIndex(subtitleAnchor->findData(cue->anchor));subtitleOwnStyle->setChecked(cue->ownStyle);
        subtitleFirst->clear();subtitleLast->clear();auto spans=lyricSpans(project,t->sourceTrack);
        for(int i=0;i<spans.size();++i){const auto&s=spans[i];auto label=QString("%1 — %2 s").arg(s.text).arg(project.score.time.seconds(s.start)+project.output.syncOffset,0,'f',3);subtitleFirst->addItem(label,i);subtitleLast->addItem(label,i);}
        for(int i=0;i<spans.size();++i)if(cue->sourceNotes.contains(spans[i].notes[0])){subtitleFirst->setCurrentIndex(i);break;}
        for(int i=int(spans.size())-1;i>=0;--i)if(cue->sourceNotes.contains(spans[i].notes[0])){subtitleLast->setCurrentIndex(i);break;}
    }
    auto s=cue&&cue->ownStyle?cue->style:t->style;
    if(!s.family.isEmpty())subtitleFont->setCurrentFont(QFont(s.family));else subtitleFont->setCurrentFont(QFont());
    selectedSubtitleColor=s.color;subtitleColor->setStyleSheet("background-color: "+s.color.name(QColor::HexArgb));subtitleSize->setValue(s.fontHeight*100);subtitleX->setValue(s.x*100);subtitleY->setValue(s.y*100);subtitleWidth->setValue(s.width*100);subtitleBold->setChecked(s.bold);subtitleItalic->setChecked(s.italic);subtitleOutline->setChecked(s.outline);subtitleTextAlign->setCurrentIndex(subtitleTextAlign->findData(s.alignment));
    if(cue&&subtitleInterval(project,*cue).orphan)subtitleHint->setText(trText("Lyric source missing; subtitle text and timing were retained."));
    else if(cue&&project.output.duration>0&&subtitleInterval(project,*cue).end>project.output.duration)subtitleHint->setText(trText("Subtitle ends after the animation. Extend the duration to include it."));
    else subtitleHint->setText(trText("Double-click an empty subtitle lane to enter text. Apply confirms style and timing. Lyric alignment uses score timing."));
    updateLayoutTarget();
    if(timelineScroll)timelineScroll->setMinimumHeight(qRound(timeline->mouthLaneRect().height())+32+(project.subtitlesEnabled?72:16));
}
void Window::updateLayoutTarget() {
    if(!preview)return;auto id=layoutTarget->currentData().toString();
    bool own=subtitlePage&&!subtitleCuePanel->isHidden()&&subtitleOwnStyle->isChecked();
    preview->setTarget(id,own&&id==subtitleTrackId?subtitleCueId:QString{});
}
void Window::selectSubtitle(const QString &id,const QString &cue,bool editText) {
    if(busy)return;
    timeline->finishSubtitleEditing(false);subtitleTrackId=id;subtitleCueId=cue;
    bool old=refreshing;refreshing=true;refreshSubtitles();refreshing=old;timeline->selectSubtitle(id,cue);refreshSelection();
    if(subtitlePage&&project.subtitlesEnabled)tabs->setCurrentWidget(subtitlePage);
    if(editText&&!playing){++subtitleTextSession;timeline->beginSubtitleEditing(id,cue);}else timeline->setFocus();
    layoutTarget->setCurrentIndex(std::max(0,layoutTarget->findData(id)));updateLayoutTarget();refreshPreview();
}
void Window::deleteSubtitleCue() {
    if(busy||playing||subtitleCueId.isEmpty())return;
    timeline->finishSubtitleEditing(false);auto id=subtitleTrackId,cue=subtitleCueId;
    changeSubtitles(trText("Delete subtitle"),[id,cue](Project&p){if(auto t=findTrack(p,id))for(int i=0;i<t->cues.size();++i)if(t->cues[i].id==cue){t->cues.removeAt(i);break;}});
}
void Window::deleteSelection() {
    if(busy||playing)return;
    if(!subtitleCueId.isEmpty()){deleteSubtitleCue();return;}
    auto ids=timeline->selectedIds();if(ids.isEmpty())return;
    change(trText("Delete selection"),[ids](Project&p){for(const auto&e:p.effective())if(ids.contains(e.id))p.erase(e);});
    timeline->selectIds({});
}
void Window::clearObjectSelection(){timeline->finishSubtitleEditing(false);subtitleCueId.clear();timeline->selectIds({});updateLayoutTarget();refreshPreview();}
void Window::saveViewPreferences(){try{savePreferences(preferences);}catch(const std::exception&e){statusBar()->showMessage(QString::fromUtf8(e.what()),7000);}}
void Window::addSubtitleTrack() {
    QString id=QUuid::createUuid().toString(QUuid::Id128);changeSubtitles(trText("New subtitle track"),[this,id](Project&p){SubtitleTrack t;t.id=id;t.name=trText("Subtitle track")+" "+QString::number(p.subtitles.size()+1);t.style.y=std::max(.15,.86-p.subtitles.size()*.10);if(p.score.tracks.size()==1)t.sourceTrack=p.score.tracks[0].id;p.subtitles.append(t);});selectSubtitle(id,{});
}
void Window::addSubtitle(int row,double time) {
    if(busy||playing||row<0||row>=project.subtitles.size())return;QString track=project.subtitles[row].id,cue=QUuid::createUuid().toString(QUuid::Id128);
    changeSubtitles(trText("Subtitles"),[this,track,cue,time](Project&p){auto t=findTrack(p,track);if(!t)return;SubtitleCue c;c.id=cue;setSubtitleTime(p,c,time,std::min(21600.,time+2));
        bool matched=false;if(t->alignLyrics&&!t->sourceTrack.isEmpty())for(const auto&s:lyricSpans(p,t->sourceTrack)){double a=p.score.time.seconds(s.start)+p.output.syncOffset,b=p.score.time.seconds(s.end)+p.output.syncOffset;if(time>=a&&time<b){alignSubtitle(p,c,t->sourceTrack,s,s);matched=true;break;}}
        if(t->alignLyrics&&!matched&&!t->sourceTrack.isEmpty()){
            double nearest=time,distance=8;const LyricSpan *selected=nullptr;auto spans=lyricSpans(p,t->sourceTrack);
            for(const auto&s:spans)for(auto tick:{s.start,s.end}){double a=p.score.time.seconds(tick)+p.output.syncOffset,d=std::abs(timeline->xAtTime(a)-timeline->xAtTime(time));if(a>=0&&a<21599.999&&d<=distance){distance=d;nearest=a;selected=&s;}}
            if(selected){setSubtitleTime(p,c,nearest,std::min(21600.,nearest+2),true);c.sourceTrack=t->sourceTrack;c.sourceNotes=selected->notes;matched=true;}
        }
        if(t->alignLyrics&&!matched)statusBar()->showMessage(trText("No nearby lyric; kept the manual interval."),5000);
        t->cues.append(c);
        Project check=p;check.subtitles={*t};validateSubtitles(check,true);
    });selectSubtitle(track,cue,true);
}
void Window::applySubtitleProperties() {
    auto t=findTrack(project,subtitleTrackId);if(!t||busy)return;auto id=subtitleTrackId,cue=subtitleCueId;
    SubtitleStyle style;style.family=subtitleFont->currentFont().family();style.alignment=subtitleTextAlign->currentData().toString();style.color=selectedSubtitleColor;style.x=subtitleX->value()/100;style.y=subtitleY->value()/100;style.width=subtitleWidth->value()/100;style.fontHeight=subtitleSize->value()/100;style.bold=subtitleBold->isChecked();style.italic=subtitleItalic->isChecked();style.outline=subtitleOutline->isChecked();
    auto text=subtitleText->toPlainText();double a=subtitleStart->value(),b=subtitleEnd->value();bool own=subtitleOwnStyle->isChecked(),beats=subtitleAnchor->currentData()=="beats";
    changeSubtitles(trText("Apply subtitles"),[id,cue,style,text,a,b,own,beats](Project&p){if(auto t=findTrack(p,id)){if(auto c=findCue(*t,cue)){c->text=text;auto v=subtitleInterval(p,*c);if(std::abs(v.start-a)>1e-6||std::abs(v.end-b)>1e-6||(c->anchor=="beats")!=beats){setSubtitleTime(p,*c,a,b,beats);Project check=p;check.subtitles={*t};validateSubtitles(check,true);}c->ownStyle=own;if(own)c->style=style;else t->style=style;}else t->style=style;}});
}
void Window::alignSubtitleRange() {
    auto t=findTrack(project,subtitleTrackId);if(!t||subtitleCueId.isEmpty())return;auto spans=lyricSpans(project,t->sourceTrack);int a=subtitleFirst->currentData().toInt(),b=subtitleLast->currentData().toInt();if(a<0||b<a||b>=spans.size())return;
    auto id=subtitleTrackId,cue=subtitleCueId;changeSubtitles(trText("Use lyric range"),[id,cue,spans,a,b](Project&p){if(auto t=findTrack(p,id))if(auto c=findCue(*t,cue)){alignSubtitle(p,*c,t->sourceTrack,spans[a],spans[b]);Project check=p;check.subtitles={*t};validateSubtitles(check,true);}});
}
}
