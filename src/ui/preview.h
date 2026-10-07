// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include "render/render.h"
namespace chosuta {
class PreviewCanvas:public QWidget {
    Q_OBJECT
public:
    explicit PreviewCanvas(QWidget *parent=nullptr);
    void setState(const Project &,Scene *,double seconds,bool editable);
    void setTarget(const QString &track={},const QString &cue={});
    QRectF canvasRect()const;
    QRectF selectionRect()const;
    void cancelDrag();
signals:
    void layoutEdited(chosuta::Project layout);
    void deleteRequested();
    void blankClicked();
protected:
    void paintEvent(QPaintEvent *)override;
    void mousePressEvent(QMouseEvent *)override;
    void mouseMoveEvent(QMouseEvent *)override;
    void mouseReleaseEvent(QMouseEvent *)override;
    void keyPressEvent(QKeyEvent *)override;
    void resizeEvent(QResizeEvent *)override;
private:
    Project project,before;
    Scene *scene=nullptr;
    QString trackId,cueId;
    double seconds=0;
    bool editable=true,dragging=false,changed=false;
    int mode=0;
    QPointF origin;
    QRectF originalRect;
    const SubtitleStyle *targetStyle()const;
    QString targetText()const;
    SubtitleStyle *editStyle();
    QPointF canvasPoint(const QPointF &)const;
};
}
