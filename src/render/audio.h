// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model.h"
namespace chosuta {
struct AudioInfo {QString path,error,warning;double duration=0;bool cancelled=false;};
AudioInfo probeAudio(const QString &path,const QString &ffmpeg,const std::atomic_bool &cancel);
struct MediaToolsInfo {
    QString ffmpeg,ffprobe,ffmpegVersion,ffprobeVersion;
    QStringList errors,missingEncoders,missingFilters;
    bool cancelled=false;
    bool ready()const{return !cancelled&&errors.isEmpty()&&missingEncoders.isEmpty()&&missingFilters.isEmpty();}
};
MediaToolsInfo inspectMediaTools(const QString &ffmpeg,const std::atomic_bool &cancel);
}
