// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio.h"
#include "core/executable.h"
#include <cmath>
namespace chosuta {
    AudioInfo probeAudio(const QString&path,const QString&ffmpeg,const std::atomic_bool&cancel) {
        AudioInfo r;
        r.path=path;
        if(!QFileInfo::exists(path)) {
            r.error="Missing audio: "+path;
            return r;
        }
        QString probe;
        auto executable=resolveExecutable(ffmpeg);
        if(QFileInfo(executable).isAbsolute()) {
            auto sibling=QFileInfo(executable).absoluteDir().filePath("ffprobe"+QString(QFileInfo(executable).suffix().toLower()=="exe"?".exe":""));
            if(QFileInfo::exists(sibling))probe=sibling;
        }
        if(probe.isEmpty()){
            const auto candidate=resolveExecutable("ffprobe");
            probe=QFileInfo(candidate).isAbsolute()?candidate:QStandardPaths::findExecutable(candidate);
        }
        if(probe.isEmpty()) {
            r.warning="ffprobe unavailable; set animation duration manually";
            return r;
        }
        QProcess process;
        process.start(probe, {
            "-v","error","-show_entries","format=duration:stream=codec_type,duration","-of","json",path
        });
        if(!process.waitForStarted(3000)) {
            r.error=process.errorString();
            return r;
        }
        QElapsedTimer timer;
        timer.start();
        while(!process.waitForFinished(100)) {
            if(cancel.load()||timer.elapsed()>30000) {
                process.kill();
                process.waitForFinished(1000);
                r.cancelled=cancel.load();
                if(!r.cancelled)r.error="Audio metadata probe timed out";
                return r;
            }
        }
        if(cancel.load()) {
            r.cancelled=true;
            return r;
        }
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0) {
            r.error=QString::fromUtf8(process.readAllStandardError()).left(4096);
            return r;
        }
        auto root=QJsonDocument::fromJson(process.readAllStandardOutput()).object();
        bool hasAudio=false;
        for(const auto&v:root["streams"].toArray()) {
            auto stream=v.toObject();
            if(stream["codec_type"].toString()!="audio")continue;
            hasAudio=true;
            bool ok=false;
            double duration=stream["duration"].toString().toDouble(&ok);
            if(ok&&std::isfinite(duration)&&duration>0)r.duration=std::max(r.duration,duration);
        }
        if(!hasAudio) {
            r.error="No audio stream found";
            return r;
        }
        if(r.duration==0) {
            bool ok=false;
            auto duration=root["format"].toObject()["duration"].toString().toDouble(&ok);
            if(ok&&std::isfinite(duration)&&duration>0)r.duration=duration;
        }
        if(r.duration<=0)r.warning="Audio duration unknown; set animation duration manually";
        return r;
    }
}
