// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include "core/model.h"
namespace chosuta {
    class Timeline:public QWidget {
        Q_OBJECT
        public:
        explicit Timeline(QWidget *parent=nullptr);
        void setProject(const Project &p);
        void setBeats(bool beats);
        void setZoom(double zoom);
        void setCursor(double time);
        void setReturnPosition(double time);
        QStringList selectedIds()const {
            return selected;
        }
        void selectIds(const QStringList &ids);
        double cursorTime()const {
            return cursor;
        }
        double xAtTime(double seconds)const;
        double timeAtX(double x)const;
        signals:
        void selectionChanged();
        void seek(double seconds);
        void edited(QVector<chosuta::Event> events);
        protected:
        void paintEvent(QPaintEvent *)override;
        void mousePressEvent(QMouseEvent *)override;
        void mouseMoveEvent(QMouseEvent *)override;
        void mouseReleaseEvent(QMouseEvent *)override;
        void wheelEvent(QWheelEvent *)override;
        private:
        Project project;
        QVector<Event>events,beforeDrag;
        QStringList selected;
        QString dragging;
        bool beats=false;
        double scale=120,cursor=0,returnPosition=0,dragOrigin=0;
        int dragMode=0;
        int hit(double x,double y)const;
        void updateExtent();
    };
}
Q_DECLARE_METATYPE(QVector<chosuta::Event>)
