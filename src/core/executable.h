// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
namespace chosuta {
    class MediaTools {
        MediaTools(QString encoder,QString probe):ffmpeg(std::move(encoder)),ffprobe(std::move(probe)){}
        friend MediaTools selectMediaTools(const QString &);
        public:
        const QString ffmpeg,ffprobe;
    };
    MediaTools selectMediaTools(const QString &program);
    // "ffmpeg" uses the machine default, then bundled tools, application dir, PATH.
    // Explicit paths never silently fall back. An empty app dir uses the running app.
    QString resolveExecutable(const QString &program,const QString &applicationDirectory={});
    QString defaultFfmpegExecutable();
    QString automaticFfmpegExecutable();
    void setDefaultFfmpegExecutable(const QString &program);
    QString resolveFfprobe(const QString &ffmpeg);
}
