// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model.h"
#include "core/executable.h"
namespace chosuta {
struct AudioInfo {QString path;Message error,warning;double duration=0;bool cancelled=false;};
AudioInfo probeAudio(const QString &path,const MediaTools &tools,const std::atomic_bool &cancel);
struct MediaToolsInfo {
    QString ffmpeg,ffprobe,ffmpegVersion,ffprobeVersion;
    QVector<Message> errors;QStringList missingEncoders,missingFilters;
    bool cancelled=false;
    bool ready()const{return !cancelled&&errors.isEmpty()&&missingEncoders.isEmpty()&&missingFilters.isEmpty();}
};
MediaToolsInfo inspectMediaTools(const MediaTools &tools,const std::atomic_bool &cancel);
}
