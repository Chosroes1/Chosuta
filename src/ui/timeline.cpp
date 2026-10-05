// SPDX-License-Identifier: GPL-3.0-or-later
#include "timeline.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <cmath>
namespace chosuta {
    Timeline::Timeline(QWidget*parent):QWidget(parent) {
        setMinimumHeight(170);
        setMouseTracking(true);
        setObjectName("timeline");
    }
    void Timeline::setProject(const Project&p) {
        project=p;
        events=p.effective();
        updateExtent();
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
        if(xAtTime(t)+40>width())updateExtent();
        update();
    }
    void Timeline::setReturnPosition(double t){returnPosition=t;update();}
    void Timeline::selectIds(const QStringList&ids) {
        selected=ids;
        update();
        emit selectionChanged();
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
        double width=xAtTime(end+10);
        // QWidget has a finite maximum size. Reduce zoom for long animations
        // so their end and cursor remain reachable instead of clipping time.
        if(width>16000000){scale*=15999980/(width-20);width=xAtTime(end+10);}
        setFixedWidth(int(std::clamp(width,900.,16000000.)));
    }
    int Timeline::hit(double x,double y)const {
        if(y<46||y>120)return -1;
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
        double step=scale<30?5:scale<80?2:1;
        double from=std::floor((left-20)/scale/step)*step,to=(right-20)/scale;
        for(double unit=from;unit<=to;unit+=step) {
            double x=20+unit*scale;
            painter.setPen(palette().mid().color());
            painter.drawLine(QPointF(x,28),QPointF(x,height()));
            painter.setPen(palette().text().color());
            QString label=beats?project.score.time.beatLabel(qRound64(unit*Blick)):QString::number(unit,'f',unit==std::floor(unit)?0:1)+"s";
            painter.drawText(QRectF(x+3,3,130,22),label);
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
            QRectF box(x,46,std::max(1.,end-x),74);
            painter.setPen(QPen(selected.contains(e.id)?palette().highlight().color():palette().mid().color(),selected.contains(e.id)?3:1));
            painter.setBrush(colors.value(e.shape,QColor("#afbac8")));
            painter.drawRoundedRect(box.adjusted(1,1,-1,-1),3,3);
            painter.setPen(Qt::black);
            painter.save();
            painter.setClipRect(box.adjusted(3,0,-3,0));
            painter.drawText(box.adjusted(5,3,-3,-3),Qt::AlignTop,e.shape+"\n"+e.text);
            if(project.overrides.contains(e.id))painter.drawText(box.adjusted(5,45,-3,-3),QStringLiteral("●"));
            painter.restore();
        }
        painter.setPen(QPen(QColor("#3874cb"),1,Qt::DashLine));
        double marker=xAtTime(returnPosition);painter.drawLine(QPointF(marker,28),QPointF(marker,height()));
        painter.drawText(QRectF(marker+3,125,120,20),QStringLiteral("↩"));
        painter.setPen(QPen(QColor("#d74242"),2));
        double x=xAtTime(cursor);
        painter.drawLine(QPointF(x,0),QPointF(x,height()));
    }
    void Timeline::mousePressEvent(QMouseEvent*e) {
        if(e->button()!=Qt::LeftButton)return;
        int i=hit(e->position().x(),e->position().y());
        if(i<0) {
            if(!(e->modifiers()&Qt::ControlModifier))selected.clear();
            cursor=std::max(0.,timeAtX(e->position().x()));
            emit seek(cursor);
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
        dragging=event.id;
        beforeDrag=events;
        dragOrigin=timeAtX(e->position().x());
        double x=xAtTime(event.start+project.output.syncOffset),end=xAtTime(event.end+project.output.syncOffset);
        dragMode=std::abs(e->position().x()-x)<6?1:std::abs(e->position().x()-end)<6?2:3;
        emit selectionChanged();
        update();
    }
    void Timeline::mouseMoveEvent(QMouseEvent*e) {
        if(dragging.isEmpty()) {
            int i=hit(e->position().x(),e->position().y());
            if(i>=0) {
                const auto&v=events[i];
                QToolTip::showText(e->globalPosition().toPoint(),QString("%1\n%2 – %3 s\n%4\n%5").arg(v.text).arg(v.start,0,'f',3).arg(v.end,0,'f',3).arg(v.provenance,v.source),this);
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
        if(e->button()!=Qt::LeftButton||dragging.isEmpty())return;
        QVector<Event>changed;
        for(int i=0;i<events.size();++i)if(selected.contains(events[i].id)&&(events[i].start!=beforeDrag[i].start||events[i].end!=beforeDrag[i].end))changed.append(events[i]);
        dragging.clear();
        if(!changed.isEmpty())emit edited(changed);
        update();
    }
    void Timeline::wheelEvent(QWheelEvent*e) {
        if(e->modifiers()&Qt::ControlModifier) {
            setZoom(scale*(e->angleDelta().y()>0?1.2:1/1.2));
            e->accept();
        }
        else QWidget::wheelEvent(e);
    }
}
