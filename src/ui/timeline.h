// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QWidget>
#include "core/model.h"
#include "core/appearance.h"
#include <memory>
#include "render/waveform.h"
class QPlainTextEdit;
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
        QRectF rulerRect()const;
        QRectF mouthLaneRect()const;
        QRectF waveformLaneRect()const;
        void setWaveform(std::shared_ptr<const Waveform> wave,const QString &status);
        void setWaveformHeight(int height);
        QRectF subtitleLaneRect(int row)const;
        void setLaneHeights(int mouth,int subtitles);
        void resetLaneHeights();
        QPlainTextEdit *textEditor();
        void beginSubtitleEditing(const QString &track,const QString &cue);
        void finishSubtitleEditing(bool restoreFocus=true);
        bool editingText()const {return !editingCue.isEmpty();}
        QString editingTrackId()const {return editingTrack;}
        QString editingCueId()const {return editingCue;}
        QStringList selectedIds()const {
            return selected;
        }
        void selectIds(const QStringList &ids);
        void selectSubtitle(const QString &track,const QString &cue);
        void setSubtitleText(const QString &id,const QString &text);
        void setEditable(bool enabled);
        double cursorTime()const {
            return cursor;
        }
        double xAtTime(double seconds)const;
        double timeAtX(double x)const;
        signals:
        void selectionChanged();
        void seek(double seconds);
        void edited(QVector<chosuta::Event> events);
        void subtitleSelected(QString track,QString cue,bool editText);
        void subtitleCreated(int track,double time);
        void subtitleEdited(QString track,chosuta::SubtitleCue cue);
        void editRejected(QString message);
        void deleteRequested();
        void blankClicked();
        void textEditingFinished();
        void laneHeightChanged(QString track,int height);
        protected:
        void paintEvent(QPaintEvent *)override;
        void mousePressEvent(QMouseEvent *)override;
        void mouseDoubleClickEvent(QMouseEvent *)override;
        void mouseMoveEvent(QMouseEvent *)override;
        void mouseReleaseEvent(QMouseEvent *)override;
        void wheelEvent(QWheelEvent *)override;
        void keyPressEvent(QKeyEvent *)override;
        bool eventFilter(QObject *,QEvent *)override;
        void moveEvent(QMoveEvent *)override;
        void resizeEvent(QResizeEvent *)override;
        private:
        Project project;
        std::unique_ptr<AppearanceResolver> appearance;
        QVector<Event>events,beforeDrag;
        QStringList selected;
        QString dragging;
        bool beats=false;
        double scale=120,cursor=0,returnPosition=0,dragOrigin=0;
        int dragMode=0;
        int hit(double x,double y)const;
        QString subtitleTrack,subtitleCue;
        std::optional<SubtitleCue> beforeSubtitle;
        QMap<QString,QVector<LyricSpan>> lyrics;
        QMap<QString,SubtitleInterval> intervals;
        QSet<QString> availableSources;
        bool editable=true,rulerDragging=false,dragActive=false;
        QPointF pressPoint;
        static constexpr int RulerHeight=32;
        int mouthHeight=96,defaultSubtitleHeight=60,resizeRow=-1,resizeBefore=0;
        int waveformHeight=80;
        std::shared_ptr<const Waveform> waveform;
        QString waveformStatus;
        QMap<QString,int> laneHeights;
        QPlainTextEdit *editor=nullptr;
        QString editingTrack,editingCue;
        int waveHeight()const {return project.audioPath.isEmpty()?0:waveformHeight;}
        int baseHeight()const {return RulerHeight+waveHeight()+mouthHeight;}
        QRectF mouthContentRect()const;
        int subtitleHeight(int row)const;
        int resizeHit(double y)const;
        void positionEditor();
        void cancelLaneResize();
        int stickyTop()const;
        int subtitleRow(double y)const;
        int subtitleHit(int row,double x,double y)const;
        double snapTime(const SubtitleTrack &,double time,bool &snapped)const;
        void updateExtent();
    };
}
Q_DECLARE_METATYPE(QVector<chosuta::Event>)
