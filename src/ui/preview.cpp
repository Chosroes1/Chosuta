// SPDX-License-Identifier: GPL-3.0-or-later
#include "preview.h"
#include <QMouseEvent>
#include <QPainter>
#include <QKeyEvent>
#include <cmath>
namespace chosuta {
PreviewCanvas::PreviewCanvas(QWidget *parent):QWidget(parent) {
    setObjectName("preview");setMinimumSize(320,210);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);setFocusPolicy(Qt::ClickFocus);
}
void PreviewCanvas::setState(const Project &p,Scene *s,double t,bool canEdit) {
    if(dragging&&scene==s)cancelDrag();
    dragging=false;project=p;scene=s;seconds=t;editable=canEdit;update();
}
void PreviewCanvas::setTarget(const QString &track,const QString &cue) {
    cancelDrag();trackId=track;cueId=cue;update();
}
QRectF PreviewCanvas::canvasRect()const {
    QSizeF size(project.canvas.width,project.canvas.height);size.scale(QSizeF(width(),height()),Qt::KeepAspectRatio);
    return {(width()-size.width())/2,(height()-size.height())/2,size.width(),size.height()};
}
QPointF PreviewCanvas::canvasPoint(const QPointF &p)const {
    auto r=canvasRect();return {(p.x()-r.left())*project.canvas.width/r.width(),(p.y()-r.top())*project.canvas.height/r.height()};
}
const SubtitleStyle *PreviewCanvas::targetStyle()const {
    if(!project.subtitlesEnabled)return nullptr;
    for(const auto&t:project.subtitles)if(t.id==trackId&&t.enabled){
        if(cueId.isEmpty())return &t.style;
        for(const auto&c:t.cues)if(c.id==cueId)return c.ownStyle?&c.style:&t.style;
    }return nullptr;
}
QString PreviewCanvas::targetText()const {
    for(const auto&t:project.subtitles)if(t.id==trackId)for(const auto&c:t.cues){auto v=scene?scene->cueInterval(c.id):std::nullopt;if(v&&((cueId.isEmpty()&&!c.ownStyle)||cueId==c.id)&&seconds>=v->start&&seconds<v->end)return c.text;}
    return {};
}
SubtitleStyle *PreviewCanvas::editStyle() {
    for(auto&t:project.subtitles)if(t.id==trackId){
        if(cueId.isEmpty())return &t.style;
        for(auto&c:t.cues)if(c.id==cueId){if(!c.ownStyle){c.style=t.style;c.ownStyle=true;}return &c.style;}
    }return nullptr;
}
QRectF PreviewCanvas::selectionRect()const {
    if(!scene)return {};
    QRectF rect;
    if(trackId.isEmpty())rect=scene->characterRect(seconds);
    else {auto s=targetStyle();auto text=targetText();if(!s||text.isEmpty())return {};rect=scene->subtitleRect(*s,text);}
    auto r=canvasRect();return {r.left()+rect.left()*r.width()/project.canvas.width,r.top()+rect.top()*r.height()/project.canvas.height,rect.width()*r.width()/project.canvas.width,rect.height()*r.height()/project.canvas.height};
}
void PreviewCanvas::paintEvent(QPaintEvent *) {
    QPainter painter(this);painter.fillRect(rect(),palette().dark());if(!scene)return;
    auto r=canvasRect();painter.drawImage(r,scene->frame(seconds,QSize(std::max(16,qRound(r.width())),std::max(16,qRound(r.height())))));
    if(editable){auto box=selectionRect();if(!box.isEmpty()){
        painter.setPen(QPen(palette().highlight().color(),1,Qt::DashLine));painter.setBrush(Qt::NoBrush);painter.drawRect(box);
        painter.setBrush(palette().highlight());painter.drawRect(QRectF(box.bottomRight()-QPointF(4,4),QSizeF(8,8)));
        if(!trackId.isEmpty())painter.drawRect(QRectF(QPointF(box.right()-4,box.center().y()-4),QSizeF(8,8)));
    }}
}
void PreviewCanvas::mousePressEvent(QMouseEvent *e) {
    if(e->button()!=Qt::LeftButton)return;setFocus();
    auto box=selectionRect();if(box.isEmpty()||!box.adjusted(-8,-8,8,8).contains(e->position())){emit blankClicked();return;}
    if(!editable||!scene)return;
    auto corner=QRectF(box.bottomRight()-QPointF(8,8),QSizeF(16,16));auto side=QRectF(QPointF(box.right()-8,box.center().y()-8),QSizeF(16,16));
    bool sideHit=!trackId.isEmpty()&&side.contains(e->position());
    bool cornerHit=corner.contains(e->position());
    if(sideHit&&cornerHit)cornerHit=QLineF(e->position(),box.bottomRight()).length()<=QLineF(e->position(),QPointF(box.right(),box.center().y())).length();
    mode=cornerHit?2:sideHit?3:box.contains(e->position())?1:0;
    if(!mode)return;
    before=project;originalRect=box;origin=canvasPoint(e->position());dragging=true;changed=false;setFocus();
}
void PreviewCanvas::mouseMoveEvent(QMouseEvent *e) {
    if(!dragging||!scene)return;
    auto point=canvasPoint(e->position());auto delta=point-origin;project=before;
    if(trackId.isEmpty()){
        auto&c=project.canvas;
        if(mode==1){c.characterX=std::clamp(c.characterX+delta.x()/c.width,-2.,3.);c.characterY=std::clamp(c.characterY+delta.y()/c.height,-2.,3.);}
        else {auto center=canvasPoint(originalRect.center());auto old=origin-center;auto next=point-center;auto factor=std::hypot(next.x(),next.y())/std::max(1.,std::hypot(old.x(),old.y()));c.characterScale=std::clamp(c.characterScale*factor,.01,10.);}
        changed=c.characterX!=before.canvas.characterX||c.characterY!=before.canvas.characterY||c.characterScale!=before.canvas.characterScale;
    }else if(auto s=editStyle()){
        if(mode==1){s->x=std::clamp(s->x+delta.x()/project.canvas.width,-2.,3.);s->y=std::clamp(s->y+delta.y()/project.canvas.height,-2.,3.);}
        else if(mode==3)s->width=std::clamp(s->width+2*delta.x()/project.canvas.width,.02,4.);
        else {auto center=canvasPoint(originalRect.center());auto old=origin-center;auto next=point-center;double factor=std::hypot(next.x(),next.y())/std::max(1.,std::hypot(old.x(),old.y()));s->fontHeight=std::clamp(s->fontHeight*factor,.005,.5);}
        changed=project.subtitles!=before.subtitles;
    }
    scene->setLayout(project.canvas,project.subtitles,project.subtitlesEnabled);update();
}
void PreviewCanvas::mouseReleaseEvent(QMouseEvent *e) {
    if(e->button()!=Qt::LeftButton||!dragging)return;
    dragging=false;
    if(changed)emit layoutEdited(project);
    else {scene->setLayout(before.canvas,before.subtitles,before.subtitlesEnabled);project=before;update();}
}
void PreviewCanvas::cancelDrag() {
    if(!dragging)return;dragging=false;project=before;if(scene)scene->setLayout(project.canvas,project.subtitles,project.subtitlesEnabled);update();
}
void PreviewCanvas::keyPressEvent(QKeyEvent *e) {
    if(editable&&(e->key()==Qt::Key_Delete||e->key()==Qt::Key_Backspace)){cancelDrag();emit deleteRequested();e->accept();return;}
    if(e->key()==Qt::Key_Escape&&dragging){cancelDrag();e->accept();return;}QWidget::keyPressEvent(e);
}
void PreviewCanvas::resizeEvent(QResizeEvent *e) {cancelDrag();QWidget::resizeEvent(e);}
}
