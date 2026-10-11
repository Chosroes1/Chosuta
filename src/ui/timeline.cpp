// SPDX-License-Identifier: GPL-3.0-or-later
#include "timeline.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <QKeyEvent>
#include <cmath>
#include <limits>
#include <QPlainTextEdit>
#include <QApplication>
#include <QInputMethod>
#include <QTimer>
#include <QFocusEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QMoveEvent>
#include <QResizeEvent>
namespace chosuta {
    // One native text editor, with the same full-height playhead painted over its viewport.
    class SubtitleEditor final:public QPlainTextEdit {
        Timeline *timeline;
    public:
        explicit SubtitleEditor(Timeline *owner):QPlainTextEdit(owner),timeline(owner){}
    protected:
        void paintEvent(QPaintEvent *event)override {
            QPlainTextEdit::paintEvent(event);
            QPainter painter(viewport());painter.setPen(QPen(QColor("#d74242"),2));
            double x=timeline->xAtTime(timeline->cursorTime())-viewport()->mapTo(timeline,QPoint(0,0)).x();
            painter.drawLine(QPointF(x,0),QPointF(x,viewport()->height()));
        }
    };
    Timeline::Timeline(QWidget*parent):QWidget(parent) {
        setMinimumHeight(baseHeight());
        setMouseTracking(true);
        setObjectName("timeline");
        setFocusPolicy(Qt::ClickFocus);
    }
    void Timeline::setProject(const Project&p) {
        cancelLaneResize();
        project=p;appearance=std::make_unique<AppearanceResolver>(p);performance=std::make_unique<PerformanceResolver>(p);performanceDrag=false;performanceMarks.clear();QSet<QString> marks;for(const auto &m:p.performance.suggestions)marks.insert(m.source);for(const auto &track:p.score.tracks)for(const auto &note:track.notes)if(marks.contains(note.id))performanceMarks.append(performance->mappedBlick(note.onset,track.id));
        beforeSubtitle.reset();dragging.clear();rulerDragging=false;
        for(auto it=laneHeights.begin();it!=laneHeights.end();) {bool found=false;for(const auto&t:p.subtitles)if(t.id==it.key())found=true;if(!found)it=laneHeights.erase(it);else ++it;}
        events=p.effective();
        syllableEdges.clear();
        for(const auto &track:p.score.tracks)if(p.selected.contains(track.id))for(const auto &span:lyricSpans(p,track.id)){
            for(double t:{performance->mappedBlick(span.start,track.id),performance->mappedBlick(span.end,track.id,true)})if(t>=0&&t<=21600)syllableEdges.append(t);
        }
        std::sort(syllableEdges.begin(),syllableEdges.end());syllableEdges.erase(std::unique(syllableEdges.begin(),syllableEdges.end()),syllableEdges.end());

        lyrics.clear();intervals.clear();availableSources.clear();
        if(p.subtitlesEnabled){availableSources=subtitleSources(p);for(const auto&t:p.subtitles){if(t.alignLyrics&&!lyrics.contains(t.sourceTrack))lyrics[t.sourceTrack]=lyricSpans(p,t.sourceTrack);for(const auto&c:t.cues)intervals[c.id]=subtitleInterval(p,c,&availableSources);}}
        bool found=false;for(const auto&t:p.subtitles)if(t.id==editingTrack)for(const auto&c:t.cues)if(c.id==editingCue){found=true;if(editor&&editor->toPlainText()!=c.text){QSignalBlocker blocker(editor);editor->setPlainText(c.text);}}
        if(!p.subtitlesEnabled||!found)finishSubtitleEditing(false);
        updateExtent();positionEditor();
        update();
    }
    void Timeline::setBeats(bool b) {
        beats=b;
        updateExtent();
        update();
    }
    void Timeline::setZoom(double zoom) {
        scale=std::clamp(zoom,5.,1000.);
        updateExtent();
        update();
    }
    void Timeline::setCursor(double t) {
        cursor=t;
        if(editor&&editor->isVisible())editor->viewport()->update();
        if(xAtTime(t)+40>width())updateExtent();
        update();
    }
    double Timeline::snappedCursor(double seconds,bool bypass)const {
        seconds=std::clamp(seconds,0.,21600.);
        if(bypass||cursorSnap=="off")return seconds;
        if(cursorSnap=="syllable"){
            auto it=std::lower_bound(syllableEdges.cbegin(),syllableEdges.cend(),seconds);
            double best=seconds,distance=std::numeric_limits<double>::max();
            for(auto candidate:{it==syllableEdges.cend()?-1.:*it,it==syllableEdges.cbegin()?-1.:*(it-1)})if(candidate>=0&&std::abs(candidate-seconds)<distance){best=candidate;distance=std::abs(candidate-seconds);}
            return best;
        }
        const auto tick=project.score.time.blicks(seconds-project.output.syncOffset);
        qint64 base=0,nextBase=MusicLimit;auto meter=project.score.time.meters.front();
        for(int i=1;i<project.score.time.meters.size();++i){auto next=project.score.time.meters[i];auto at=base+qint64(next.bar-meter.bar)*meter.numerator*(Blick*4/meter.denominator);if(at>tick){nextBase=at;break;}base=at;meter=next;}
        const qint64 unit=Blick*4/meter.denominator;
        auto nearest=base+qint64(std::llround(double(tick-base)/unit))*unit;
        nearest=std::min(nearest,nextBase);
        return std::clamp(project.score.time.seconds(nearest)+project.output.syncOffset,0.,21600.);
    }
    void Timeline::setReturnPosition(double t){returnPosition=t;update();}
    void Timeline::selectIds(const QStringList&ids) {
        selected=ids;performanceId.clear();
        subtitleTrack.clear();subtitleCue.clear();
        update();
        emit selectionChanged();
    }
    void Timeline::selectSubtitle(const QString&t,const QString&c){subtitleTrack=t;subtitleCue=c;selected.clear();performanceId.clear();update();}
    void Timeline::setSubtitleText(const QString&id,const QString&text){for(auto&t:project.subtitles)for(auto&c:t.cues)if(c.id==id)c.text=text;if(editor&&editingCue==id&&editor->toPlainText()!=text){QSignalBlocker blocker(editor);auto cursor=editor->textCursor();int at=cursor.position();editor->setPlainText(text);cursor=editor->textCursor();cursor.setPosition(std::min(at,int(text.size())));editor->setTextCursor(cursor);}update();}
    void Timeline::setEditable(bool enabled) {
        if(editable==enabled)return;
        editable=enabled;if(enabled)return;
        finishSubtitleEditing();cancelLaneResize();rulerDragging=false;performanceDrag=false;
        if(beforeSubtitle){for(auto&t:project.subtitles)for(auto&c:t.cues)if(c.id==subtitleCue){c=*beforeSubtitle;intervals[c.id]=subtitleInterval(project,c,&availableSources);}beforeSubtitle.reset();}
        if(!dragging.isEmpty()){events=beforeDrag;dragging.clear();}
        updateExtent();update();
    }
    int Timeline::stickyTop()const{return std::max(0,-pos().y());}
    int Timeline::subtitleHeight(int row)const {return laneHeights.value(project.subtitles[row].id,defaultSubtitleHeight);}
    QRectF Timeline::rulerRect()const {return {0.,double(stickyTop()),double(width()),double(RulerHeight)};}
    QRectF Timeline::mouthLaneRect()const {return {0.,double(stickyTop()+RulerHeight+waveHeight()),double(width()),double(mouthHeight)};}
    QRectF Timeline::mouthContentRect()const {return mouthLaneRect().adjusted(0,6,0,-24);}
    QRectF Timeline::waveformLaneRect()const {return {0.,double(stickyTop()+RulerHeight),double(width()),double(waveHeight())};}
    void Timeline::setWaveform(std::shared_ptr<const Waveform> wave,const QString &status){waveform=std::move(wave);waveformStatus=status;update();}
    QRectF Timeline::performanceLaneRect()const {return {0.,double(stickyTop()+RulerHeight+waveHeight()+mouthHeight),double(width()),double(performanceLaneHeight())};}
    void Timeline::setPerformanceLane(int height,bool visible){performanceHeight=std::clamp(height,40,120);performanceVisible=visible;updateExtent();update();}
    void Timeline::setWaveformHeight(int height){waveformHeight=std::clamp(height,40,200);updateExtent();positionEditor();update();}
    QRectF Timeline::subtitleLaneRect(int row)const {double y=baseHeight()+4;for(int i=0;i<row;++i)y+=subtitleHeight(i);return {0.,y,double(width()),double(subtitleHeight(row))};}
    void Timeline::setLaneHeights(int mouth,int subtitles){mouthHeight=std::clamp(mouth,96,360);defaultSubtitleHeight=std::clamp(subtitles,48,240);updateExtent();positionEditor();update();}
    void Timeline::resetLaneHeights(){laneHeights.clear();setLaneHeights(96,60);setWaveformHeight(80);setPerformanceLane(56,performanceVisible);emit laneHeightChanged("@performance",56);emit laneHeightChanged({},96);emit laneHeightChanged("*",60);emit laneHeightChanged("@waveform",80);}
    int Timeline::subtitleRow(double y)const {
        if(!project.subtitlesEnabled||y<stickyTop()+baseHeight())return -1;
        for(int row=0;row<project.subtitles.size();++row)if(subtitleLaneRect(row).contains(QPointF(1,y)))return row;
        return -1;
    }
    int Timeline::resizeHit(double y)const {
        if(waveHeight()&&std::abs(y-waveformLaneRect().bottom()+2)<=3)return -3;
        if(performanceLaneHeight()&&std::abs(y-performanceLaneRect().bottom()+2)<=3)return -4;
        if(std::abs(y-mouthLaneRect().bottom()+2)<=3)return -2;
        if(project.subtitlesEnabled)for(int row=0;row<project.subtitles.size();++row){auto r=subtitleLaneRect(row);if(r.bottom()>stickyTop()+baseHeight()+3&&std::abs(y-(r.bottom()-3))<=3)return row;}
        return -1;
    }
    void Timeline::cancelLaneResize(){if(resizeRow==-1)return;if(resizeRow==-2)mouthHeight=resizeBefore;else if(resizeRow==-3)waveformHeight=resizeBefore;else if(resizeRow==-4)performanceHeight=resizeBefore;else if(resizeRow<project.subtitles.size())laneHeights[project.subtitles[resizeRow].id]=resizeBefore;resizeRow=-1;updateExtent();}
    QPlainTextEdit *Timeline::textEditor(){if(!editor){editor=new SubtitleEditor(this);editor->setObjectName("subtitleText");editor->setUndoRedoEnabled(false);editor->setTabChangesFocus(true);editor->installEventFilter(this);editor->hide();}return editor;}
    void Timeline::beginSubtitleEditing(const QString &track,const QString &cue){
        if(!editable||!project.subtitlesEnabled)return;
        finishSubtitleEditing(false);
        for(const auto&t:project.subtitles)if(t.id==track)for(const auto&c:t.cues)if(c.id==cue){
            if(parentWidget())if(auto area=qobject_cast<QScrollArea*>(parentWidget()->parentWidget())){
                int row=0;while(row<project.subtitles.size()&&project.subtitles[row].id!=track)++row;
                auto lane=subtitleLaneRect(row);auto interval=intervals.value(cue);
                area->ensureVisible(qRound(xAtTime(interval.start)),qRound(lane.bottom()-12),40,16);
            }
            editingTrack=track;editingCue=cue;
            auto e=textEditor();{QSignalBlocker blocker(e);e->setPlainText(c.text);}positionEditor();if(editingCue.isEmpty())return;e->show();e->raise();e->setFocus();auto cursor=e->textCursor();cursor.movePosition(QTextCursor::End);e->setTextCursor(cursor);return;
        }
    }
    void Timeline::finishSubtitleEditing(bool restoreFocus){if(editingCue.isEmpty())return;QGuiApplication::inputMethod()->commit();editingTrack.clear();editingCue.clear();if(editor)editor->hide();if(restoreFocus)setFocus();emit textEditingFinished();}
    void Timeline::positionEditor(){
        if(!editor||editingCue.isEmpty())return;
        for(int row=0;row<project.subtitles.size();++row)if(project.subtitles[row].id==editingTrack)for(const auto&c:project.subtitles[row].cues)if(c.id==editingCue){
            auto lane=subtitleLaneRect(row);auto v=intervals.value(c.id);QRect view=parentWidget()?QRect(mapFrom(parentWidget(),QPoint(0,0)),parentWidget()->size()):rect();
            double top=std::max(lane.top()+21.,double(view.top()+baseHeight()+1));
            if(lane.bottom()-4<top+24||top>=view.bottom()-24){finishSubtitleEditing();return;}
            int w=std::max(80,std::min(view.width()-8,std::max(80,qRound(xAtTime(v.end)-xAtTime(v.start)))));
            int x=std::clamp(qRound(xAtTime(v.start)),view.left()+4,std::max(view.left()+4,view.right()-w-4));
            editor->setGeometry(x,qRound(top),w,std::min(std::max(32,qRound(lane.bottom()-top-4)),view.bottom()-qRound(top)-2));return;
        }
    }
    bool Timeline::eventFilter(QObject *o,QEvent *event){
        if(o==editor&&event->type()==QEvent::FocusOut&&static_cast<QFocusEvent*>(event)->reason()!=Qt::PopupFocusReason)
            QTimer::singleShot(0,this,[this]{if(editingText()&&!editor->hasFocus()&&!QApplication::activePopupWidget())finishSubtitleEditing(false);});
        if(o==editor&&event->type()==QEvent::KeyPress){auto key=static_cast<QKeyEvent*>(event);if(key->key()==Qt::Key_Escape||(key->key()==Qt::Key_Return&&key->modifiers().testFlag(Qt::ControlModifier))){finishSubtitleEditing();return true;}}
        return QWidget::eventFilter(o,event);
    }
    void Timeline::moveEvent(QMoveEvent *e){QWidget::moveEvent(e);positionEditor();update();}
    void Timeline::resizeEvent(QResizeEvent *e){QWidget::resizeEvent(e);positionEditor();}
    int Timeline::subtitleHit(int row,double x,double y)const {
        if(row<0)return -1;auto lane=subtitleLaneRect(row);if(y<lane.top()+3||y>lane.bottom()-9)return -1;const auto&t=project.subtitles[row];
        for(int i=0;i<t.cues.size();++i){auto v=intervals.value(t.cues[i].id);if(x>=xAtTime(v.start)&&x<xAtTime(v.end))return i;}return -1;
    }
    double Timeline::snapTime(const SubtitleTrack&t,double time,bool &snapped)const {
        snapped=false;if(!t.alignLyrics)return time;double best=8,result=time,x=xAtTime(time);
        for(const auto&s:lyrics.value(t.sourceTrack))for(auto tick:{s.start,s.end}){double sec=project.score.time.seconds(tick)+project.output.syncOffset;double distance=std::abs(xAtTime(sec)-x);if(sec>=0&&sec<=21600&&distance<=best){best=distance;result=sec;snapped=true;}}return result;
    }
    double Timeline::xAtTime(double t)const {
        return 20+(beats?double(project.score.time.blicks(t-project.output.syncOffset))/Blick:t)*scale;
    }
    double Timeline::timeAtX(double x)const {
        return beats?project.score.time.seconds(qRound64((x-20)/scale*Blick))+project.output.syncOffset:(x-20)/scale;
    }
    void Timeline::updateExtent() {
        double end=std::max(project.duration(),cursor);
        for(const auto&e:events)end=std::max(end,e.end+project.output.syncOffset);
        if(!project.audioPath.isEmpty())end=std::max(end,(waveform?waveform->duration():project.audioDuration)+project.output.audioOffset);
        if(project.subtitlesEnabled)for(auto v:intervals)end=std::max(end,v.end);
        double width=xAtTime(end+10);
        // QWidget has a finite maximum size. Reduce zoom for long animations
        // so their end and cursor remain reachable instead of clipping time.
        if(width>16000000){scale*=15999980/(width-20);width=xAtTime(end+10);}
        setFixedWidth(int(std::clamp(width,900.,16000000.)));
        int h=baseHeight();if(project.subtitlesEnabled){h+=4;for(int row=0;row<project.subtitles.size();++row)h+=subtitleHeight(row);}setFixedHeight(h);positionEditor();
    }
    int Timeline::hit(double x,double y)const {
        const auto content=mouthContentRect();if(y<content.top()||y>content.bottom())return -1;
        for(int i=events.size()-1;i>=0;--i) {
            auto&e=events[i];
            if(x>=xAtTime(e.start+project.output.syncOffset)&&x<xAtTime(e.end+project.output.syncOffset))return i;
        }
        return -1;
    }
    void Timeline::paintEvent(QPaintEvent*event) {
        QPainter painter(this);
        painter.fillRect(rect(),palette().base());
        painter.setRenderHint(QPainter::Antialiasing);
        double left=event->rect().left(),right=event->rect().right();
        if(project.subtitlesEnabled)for(int row=0;row<project.subtitles.size();++row){const auto&t=project.subtitles[row];auto lane=subtitleLaneRect(row);double y=lane.top();
            painter.setPen(palette().mid().color());painter.drawLine(QPointF(left,y),QPointF(right,y));
            bool labelClear=true;
            for(const auto&c:t.cues){auto v=intervals.value(c.id);double a=xAtTime(v.start),b=xAtTime(v.end);if(b<left||a>right)continue;
                if(b>left&&a<left+200)labelClear=false;
                QRectF box(a,y+3,std::max(2.,b-a),lane.height()-12);painter.setPen(QPen(c.id==subtitleCue?palette().highlight().color():palette().mid().color(),c.id==subtitleCue?3:1));painter.setBrush(t.enabled?QColor("#a5d6d0"):QColor("#cccccc"));painter.drawRoundedRect(box,3,3);
                painter.save();painter.setClipRect(box.adjusted(4,2,-4,-2));painter.setPen(Qt::black);painter.drawText(box.adjusted(5,3,-4,-2),Qt::AlignTop,t.name+"\n"+c.text);painter.restore();
            }
            painter.setPen(palette().mid().color());painter.drawLine(QPointF(left,lane.bottom()-3),QPointF(right,lane.bottom()-3));
            if(labelClear){painter.setPen(palette().text().color());painter.drawText(QRectF(left+5,y+4,200,25),t.name);}
        }
        // Keep the original mouth lane and ruler visible while subtitle rows scroll.
        int top=stickyTop();painter.fillRect(QRectF(left,top,right-left+1,baseHeight()),palette().base());
        painter.fillRect(QRectF(left,top,right-left+1,RulerHeight),palette().alternateBase());
        painter.setPen(palette().mid().color());painter.drawLine(QPointF(left,top+RulerHeight),QPointF(right,top+RulerHeight));painter.drawLine(QPointF(left,top+baseHeight()-2),QPointF(right,top+baseHeight()-2));
        double step=scale<30?5:scale<80?2:1;
        double from=std::floor((left-20)/scale/step)*step,to=(right-20)/scale;
        for(double unit=from;unit<=to;unit+=step) {
            double x=20+unit*scale;
            painter.setPen(palette().mid().color());
            painter.drawLine(QPointF(x,top+RulerHeight),QPointF(x,top+baseHeight()));
            painter.setPen(palette().text().color());
            QString label=beats?project.score.time.beatLabel(qRound64(unit*Blick)):QString::number(unit,'f',unit==std::floor(unit)?0:1)+"s";
            painter.drawText(QRectF(x+3,top+3,130,22),label);
        }
        if(waveHeight()){
            auto lane=waveformLaneRect();painter.save();painter.setClipRect(lane.adjusted(0,1,0,-4));
            double middle=lane.center().y(),amplitude=(lane.height()-12)/2;
            painter.setPen(palette().mid().color());painter.drawLine(QPointF(left,middle),QPointF(right,middle));
            if(waveform&&waveform->path==project.audioPath){
                painter.setPen(QPen(QColor("#4686a5"),1));
                for(int x=std::max(0,int(left));x<=right;++x){double a=timeAtX(x)-project.output.audioOffset,b=timeAtX(x+1)-project.output.audioOffset;
                    if(b<=0||a>=waveform->duration())continue;auto peak=waveform->peaks(a,b);
                    painter.drawLine(QPointF(x,middle-peak.high*amplitude),QPointF(x,middle-peak.low*amplitude));
                }
            }else {painter.setPen(palette().text().color());painter.drawText(QRectF(left+6,lane.top()+3,std::max(100.,right-left-12),lane.height()-8),Qt::AlignLeft|Qt::AlignTop|Qt::TextWordWrap,waveformStatus);}
            painter.restore();painter.setPen(palette().mid().color());painter.drawLine(QPointF(left,lane.bottom()-2),QPointF(right,lane.bottom()-2));
        }
        QMap<QString,QColor>colors {
            {
                "A",QColor("#ed7373")
            }, {
                "I",QColor("#78c38f")
            }, {
                "U",QColor("#76a9ef")
            }, {
                "E",QColor("#e4c660")
            }, {
                "O",QColor("#bb87de")
            }, {
                "unknown",QColor("#eead61")
            }
        };
        for(const auto&e:events) {
            double x=xAtTime(e.start+project.output.syncOffset),end=xAtTime(e.end+project.output.syncOffset);
            if(end<left||x>right)continue;
            const auto content=mouthContentRect();
            QRectF box(x,content.top(),std::max(1.,end-x),content.height());
            painter.setPen(QPen(selected.contains(e.id)?palette().highlight().color():palette().mid().color(),selected.contains(e.id)?3:1));
            painter.setBrush(colors.value(e.shape,QColor("#afbac8")));
            painter.drawRoundedRect(box.adjusted(1,1,-1,-1),3,3);
            painter.setPen(Qt::black);
            painter.save();
            painter.setClipRect(box.adjusted(3,0,-3,0));
            painter.drawText(box.adjusted(5,3,-3,-3),Qt::AlignTop,e.shape+"\n"+e.text);
            if(project.overrides.contains(e.id))painter.drawText(box.adjusted(5,std::max(10.,box.height()-29),-3,-3),QStringLiteral("●"));
            painter.restore();
        }
        if(performanceLaneHeight()) {
            auto lane=performanceLaneRect();painter.fillRect(lane,palette().alternateBase());
            for(const auto &v:performance->slices()) {
                double a=xAtTime(v.start),b=xAtTime(v.end);if(b<left||a>right)continue;
                QRectF box(a,lane.top()+4,std::max(1.,b-a),lane.height()-8);
                painter.setPen(QPen(v.id==performanceId?palette().highlight().color():palette().mid().color(),v.id==performanceId?3:1));
                painter.setBrush(v.manual?QColor("#d8b67a"):QColor("#9bbdc6"));painter.drawRoundedRect(box,3,3);
                painter.save();painter.setClipRect(box);painter.setPen(Qt::black);
                painter.drawText(box.adjusted(4,2,-2,-2),Qt::AlignVCenter,(v.manual?QStringLiteral("● "):QString{})+variantGroupName(project,v.group)+(v.protect&&v.manual?QStringLiteral(" ▣"):QString{}));painter.restore();
            }
            painter.setPen(QPen(palette().text().color(),1,Qt::DashLine));
            for(double t:performanceMarks){double x=xAtTime(t);if(x>=left&&x<=right)painter.drawLine(QPointF(x,lane.top()),QPointF(x,lane.bottom()));}
            if(performanceDrag){painter.setBrush(QColor(60,120,200,60));painter.setPen(palette().highlight().color());painter.drawRect(QRectF(xAtTime(performanceStart),lane.top(),xAtTime(performanceEnd)-xAtTime(performanceStart),lane.height()));}
        }
        painter.setPen(QPen(QColor("#3874cb"),1,Qt::DashLine));
        double marker=xAtTime(returnPosition);painter.drawLine(QPointF(marker,28),QPointF(marker,height()));
        painter.drawText(QRectF(marker+3,top+baseHeight()-23,120,20),QStringLiteral("↩"));
        painter.setPen(QPen(QColor("#d74242"),2));
        double x=xAtTime(cursor);
        painter.drawLine(QPointF(x,0),QPointF(x,height()));
    }
    void Timeline::mousePressEvent(QMouseEvent*e) {
        if(e->button()!=Qt::LeftButton)return;
        finishSubtitleEditing(false);setFocus();pressPoint=e->position();dragActive=false;
        if(rulerRect().contains(e->position())){rulerDragging=true;emit seek(snappedCursor(timeAtX(e->position().x()),e->modifiers()&Qt::AltModifier));return;}
        resizeRow=resizeHit(e->position().y());if(resizeRow!=-1){resizeBefore=resizeRow==-2?mouthHeight:resizeRow==-3?waveformHeight:resizeRow==-4?performanceHeight:subtitleHeight(resizeRow);QWidget::setCursor(Qt::SizeVerCursor);return;}
        if(performanceLaneRect().contains(e->position())&&performanceLaneHeight()) {
            selected.clear();subtitleCue.clear();subtitleTrack.clear();performanceOrigin=std::clamp(timeAtX(e->position().x()),0.,21600.);
            auto v=performance->at(performanceOrigin);performanceId=v?v->id:QString{};performanceManual=v&&v->manual;
            performanceStart=v?v->start:performanceOrigin;performanceEnd=v?v->end:performanceOrigin;
            if(v&&v->manual)for(const auto &m:project.performance.manual)if(m.id==v->id){auto range=performance->interval(m);performanceStart=range.start;performanceEnd=range.end;}
            performanceOldStart=performanceStart;performanceOldEnd=performanceEnd;
            performanceDragMode=std::abs(e->position().x()-xAtTime(performanceStart))<6?1:std::abs(e->position().x()-xAtTime(performanceEnd))<6?2:3;
            performanceDrag=editable;emit performanceSelected(performanceId,performanceStart,performanceEnd);update();return;
        }
        performanceId.clear();
        int row=subtitleRow(e->position().y());
        if(row>=0){int i=subtitleHit(row,e->position().x(),e->position().y());auto&t=project.subtitles[row];selected.clear();subtitleTrack=t.id;subtitleCue=i>=0?t.cues[i].id:QString{};
            if(i>=0&&editable){beforeSubtitle=t.cues[i];auto v=intervals.value(subtitleCue);dragOrigin=timeAtX(e->position().x());dragMode=std::abs(e->position().x()-xAtTime(v.start))<6?1:std::abs(e->position().x()-xAtTime(v.end))<6?2:3;}
            emit subtitleSelected(subtitleTrack,subtitleCue,false);if(i<0)emit blankClicked();update();return;
        }
        subtitleTrack.clear();subtitleCue.clear();
        int i=hit(e->position().x(),e->position().y());
        if(i<0) {
            if(!(e->modifiers()&Qt::ControlModifier))selected.clear();
            emit blankClicked();
            emit selectionChanged();
            update();
            return;
        }
        auto event=events[i];
        if(e->modifiers()&Qt::ControlModifier) {
            if(selected.contains(event.id))selected.removeAll(event.id);
            else selected.append(event.id);
        }
        else if(!selected.contains(event.id))selected= {
            event.id
        };
        if(!editable){emit selectionChanged();update();return;}
        dragging=event.id;
        beforeDrag=events;
        dragOrigin=timeAtX(e->position().x());
        double x=xAtTime(event.start+project.output.syncOffset),end=xAtTime(event.end+project.output.syncOffset);
        dragMode=std::abs(e->position().x()-x)<6?1:std::abs(e->position().x()-end)<6?2:3;
        emit selectionChanged();
        update();
    }
    void Timeline::mouseDoubleClickEvent(QMouseEvent*e) {
        if(e->button()!=Qt::LeftButton||!editable)return;if(performanceLaneRect().contains(e->position())&&performanceLaneHeight()){performanceDrag=false;emit performanceSelected(performanceId,performanceStart,performanceEnd);return;}if(rulerRect().contains(e->position())||resizeHit(e->position().y())!=-1)return;dragging.clear();resizeRow=-1;rulerDragging=false;int row=subtitleRow(e->position().y());if(row<0)return;beforeSubtitle.reset();
        int i=subtitleHit(row,e->position().x(),e->position().y());if(i<0&&subtitleHit(row,e->position().x(),subtitleLaneRect(row).top()+4)>=0)return;if(i<0)emit subtitleCreated(row,std::clamp(timeAtX(e->position().x()),0.,21599.999));else emit subtitleSelected(project.subtitles[row].id,project.subtitles[row].cues[i].id,true);
    }
    void Timeline::mouseMoveEvent(QMouseEvent*e) {
        if(rulerDragging){emit seek(snappedCursor(timeAtX(e->position().x()),e->modifiers()&Qt::AltModifier));return;}
        if(resizeRow!=-1){int h=resizeBefore+qRound(e->position().y()-pressPoint.y());if(resizeRow==-2)mouthHeight=std::clamp(h,96,360);else if(resizeRow==-3)waveformHeight=std::clamp(h,40,200);else if(resizeRow==-4)performanceHeight=std::clamp(h,40,120);else laneHeights[project.subtitles[resizeRow].id]=std::clamp(h,48,240);updateExtent();positionEditor();update();return;}
        if(performanceDrag){
            double t=std::clamp(timeAtX(e->position().x()),0.,21600.);
            if(!(e->modifiers()&Qt::AltModifier))for(const auto &event:events)for(double bound:{event.start+project.output.syncOffset,event.end+project.output.syncOffset})if(std::abs(xAtTime(t)-xAtTime(bound))<=6)t=bound;
            if(performanceManual){double delta=t-performanceOrigin;
                if(performanceDragMode==1)performanceStart=std::clamp(performanceOldStart+delta,0.,performanceOldEnd-.001);
                else if(performanceDragMode==2)performanceEnd=std::clamp(performanceOldEnd+delta,performanceOldStart+.001,21600.);
                else {performanceStart=std::clamp(performanceOldStart+delta,0.,21600.-(performanceOldEnd-performanceOldStart));performanceEnd=performanceStart+performanceOldEnd-performanceOldStart;}
            }else {performanceId.clear();performanceStart=std::min(t,performanceOrigin);performanceEnd=std::max(t,performanceOrigin);}
            emit performanceSelected(performanceId,performanceStart,performanceEnd);update();return;
        }
        if(beforeSubtitle||!dragging.isEmpty()){if(!dragActive&&dragMode==3&&QLineF(pressPoint,e->position()).length()<QApplication::startDragDistance())return;dragActive=true;}
        else QWidget::setCursor(resizeHit(e->position().y())!=-1?Qt::SizeVerCursor:Qt::ArrowCursor);
        if(beforeSubtitle){
            for(auto&t:project.subtitles)if(t.id==subtitleTrack)for(auto&c:t.cues)if(c.id==subtitleCue){auto old=subtitleInterval(project,*beforeSubtitle,&availableSources);double a=std::max(0.,old.start),b=std::max(a+.001,old.end),delta=timeAtX(e->position().x())-dragOrigin;bool snapped=false;
                double minimum=std::min(.001,b-a);
                if(dragMode==1)a=std::clamp(snapTime(t,a+delta,snapped),0.,b-minimum);
                else if(dragMode==2)b=std::clamp(snapTime(t,b+delta,snapped),a+minimum,21600.);
                else {double desired=std::clamp(a+delta,0.,std::max(0.,21600-(b-a)));double next=snapTime(t,desired,snapped);if(next+b-a>21600){next=desired;snapped=false;}b+=next-a;a=next;}
                c=*beforeSubtitle;setSubtitleTime(project,c,a,std::min(21600.,b),t.alignLyrics&&!t.sourceTrack.isEmpty());
                if(c.anchor=="beats"){c.sourceTrack=t.sourceTrack;for(const auto&s:lyrics.value(t.sourceTrack))if(s.end>c.startBlick&&s.start<c.endBlick)c.sourceNotes.append(s.notes);}
                intervals[c.id]=subtitleInterval(project,c,&availableSources);updateExtent();update();return;
            }
        }
        if(dragging.isEmpty()) {
            int i=hit(e->position().x(),e->position().y());
            if(i>=0) {
                const auto&v=events[i];
                const auto selected=appearance->event(project,v);
                auto text=QString("%1\n%2 – %3 s\n%4\n%5\n%6\n%7").arg(v.text).arg(v.start,0,'f',3).arg(v.end,0,'f',3).arg(v.provenance,v.source,project.appearance.enabled?selected.id:v.shape,v.phone);
                if(project.appearance.enabled&&!selected.context.intervalReason.isEmpty())text+="\n"+selected.context.intervalReason;
                QToolTip::showText(e->globalPosition().toPoint(),text,this);
            }
            return;
        }
        double delta=timeAtX(e->position().x())-dragOrigin;
        events=beforeDrag;
        for(auto&v:events)if(selected.contains(v.id)) {
            if(dragMode==1)v.start=std::min(v.end-.001,v.start+delta);
            else if(dragMode==2)v.end=std::max(v.start+.001,v.end+delta);
            else {
                v.start+=delta;
                v.end+=delta;
            }
        }
        updateExtent();
        update();
    }
    void Timeline::mouseReleaseEvent(QMouseEvent*e) {
        if(e->button()==Qt::LeftButton&&rulerDragging){rulerDragging=false;return;}
        if(e->button()==Qt::LeftButton&&resizeRow!=-1){auto id=resizeRow==-2?QString{}:resizeRow==-3?QString("@waveform"):resizeRow==-4?QString("@performance"):project.subtitles[resizeRow].id;int h=resizeRow==-2?mouthHeight:resizeRow==-3?waveformHeight:resizeRow==-4?performanceHeight:subtitleHeight(resizeRow);resizeRow=-1;unsetCursor();emit laneHeightChanged(id,h);return;}
        if(e->button()==Qt::LeftButton&&performanceDrag){performanceDrag=false;if(performanceManual&&(performanceStart!=performanceOldStart||performanceEnd!=performanceOldEnd))emit performanceEdited(performanceId,performanceStart,performanceEnd);update();return;}
        if(e->button()==Qt::LeftButton&&beforeSubtitle){SubtitleCue original=*beforeSubtitle;beforeSubtitle.reset();
            for(auto&t:project.subtitles)if(t.id==subtitleTrack)for(auto&c:t.cues)if(c.id==subtitleCue){if(c!=original){try{Project check=project;check.subtitles={t};validateSubtitles(check,true);emit subtitleEdited(t.id,c);}catch(const std::exception&ex){c=original;intervals[c.id]=subtitleInterval(project,c,&availableSources);emit editRejected(QString::fromUtf8(ex.what()));update();}}return;}
        }
        if(e->button()!=Qt::LeftButton||dragging.isEmpty())return;
        QVector<Event>changed;
        for(int i=0;i<events.size();++i)if(selected.contains(events[i].id)&&(events[i].start!=beforeDrag[i].start||events[i].end!=beforeDrag[i].end))changed.append(events[i]);
        dragging.clear();
        if(!changed.isEmpty())emit edited(changed);
        update();
    }
    void Timeline::keyPressEvent(QKeyEvent*e) {
        if(editable&&(e->key()==Qt::Key_Delete||e->key()==Qt::Key_Backspace)){emit deleteRequested();e->accept();return;}
        if(editable&&e->key()==Qt::Key_F2&&!subtitleCue.isEmpty()){emit subtitleSelected(subtitleTrack,subtitleCue,true);e->accept();return;}
        if(e->key()==Qt::Key_Escape){performanceDrag=false;performanceStart=performanceOldStart;performanceEnd=performanceOldEnd;update();rulerDragging=false;cancelLaneResize();if(beforeSubtitle){for(auto&t:project.subtitles)for(auto&c:t.cues)if(c.id==subtitleCue){c=*beforeSubtitle;intervals[c.id]=subtitleInterval(project,c,&availableSources);}beforeSubtitle.reset();update();}if(!dragging.isEmpty()){events=beforeDrag;dragging.clear();update();}e->accept();return;}QWidget::keyPressEvent(e);
    }
    void Timeline::wheelEvent(QWheelEvent*e) {
        if(e->modifiers()&Qt::ControlModifier) {
            setZoom(scale*(e->angleDelta().y()>0?1.2:1/1.2));
            e->accept();
        }
        else QWidget::wheelEvent(e);
    }
}
