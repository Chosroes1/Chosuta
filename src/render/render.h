// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model.h"
#include <QImage>
#include <QTextLayout>
#include <memory>
namespace chosuta {
    class Scene {
        public:
        explicit Scene(const Project &project);
        QImage frame(double seconds,QSize size={})const;
        QRectF characterRect(double seconds)const;
        QRectF subtitleRect(const SubtitleStyle &,const QString &text)const;
        std::optional<SubtitleInterval> cueInterval(const QString &id)const;
        void setLayout(const CanvasSettings &,const QVector<SubtitleTrack> &,bool enabled);
        void setSubtitleText(const QString &id,const QString &text);
        void setTiming(const Project &project); // Same assets/layout; no image decoding.
        QStringList diagnostics;
        bool hasFallback()const {
            return images.contains(project.fallback);
        }
        bool hasBackground()const{return project.canvas.backgroundImage.isEmpty()||!background.isNull();}
        private:
        Project project;
        QVector<Event>events;
        QMap<QString,QImage>images;
        QImage background;
        struct TextLayout {std::unique_ptr<QTextLayout> text;QSizeF size;};
        struct RenderCue {SubtitleCue cue;SubtitleInterval interval;SubtitleStyle style;};
        QVector<QVector<RenderCue>> subtitleRows;
        QVector<QVector<double>> subtitlePrefixEnds;
        QHash<QString,SubtitleInterval> subtitleTimes;
        mutable QMap<QString,std::shared_ptr<TextLayout>> textCache;
        std::shared_ptr<TextLayout> textLayout(const SubtitleStyle &,const QString &)const;
        void prepareSubtitles();
    };
    struct ExportResult {
        bool success=false,cancelled=false;
        QString error;
        QStringList diagnostics;
        int frames=0;
    };
    ExportResult exportVideo(const Project &,const QString &path,const std::atomic_bool &cancel,Progress progress={},bool overwrite=false);
    QStringList encoderArguments(const Project &,const QString &temporaryPath);
    void validateExport(const Project &);
    QStringList availableEncoders(const QString &ffmpeg);
    void createDemoAssets(const QString &directory);
}
