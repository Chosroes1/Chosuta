// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model.h"
#include <QImage>
namespace chosuta {
    class Scene {
        public:
        explicit Scene(const Project &project);
        QImage frame(double seconds,QSize size={})const;
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
    };
    struct ExportResult {
        bool success=false,cancelled=false;
        QString error;
        int frames=0;
    };
    ExportResult exportVideo(const Project &,const QString &path,const std::atomic_bool &cancel,Progress progress={},bool overwrite=false);
    QStringList encoderArguments(const Project &,const QString &temporaryPath);
    void validateExport(const Project &);
    QStringList availableEncoders(const QString &ffmpeg);
    void createDemoAssets(const QString &directory);
}
