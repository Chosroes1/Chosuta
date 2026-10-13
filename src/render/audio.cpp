// SPDX-License-Identifier: GPL-3.0-or-later
#include "audio.h"
#include "core/executable.h"
#include <cmath>
namespace chosuta {
    AudioInfo probeAudio(const QString&path,const MediaTools&tools,const std::atomic_bool&cancel) {
        AudioInfo r;
        r.path=path;
        if(!QFileInfo::exists(path)) {
            r.error=Message::format("Missing audio: %1",{path});
            return r;
        }
        const auto probe=tools.ffprobe;
        if(probe.isEmpty()) {
            r.warning="ffprobe unavailable; set animation duration manually";
            return r;
        }
        QProcess process;
        process.start(probe, {
            "-v","error","-show_entries","format=duration:stream=codec_type,duration","-of","json",path
        });
        if(!process.waitForStarted(3000)) {
            r.error=Message("Cannot start audio metadata probe").withDetail(process.errorString());
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
            r.error=Message("Audio metadata probe failed").withDetail(QString::fromUtf8(process.readAllStandardError()).left(4096));
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
    MediaToolsInfo inspectMediaTools(const MediaTools &tools,const std::atomic_bool &cancel) {
        MediaToolsInfo info;info.ffmpeg=tools.ffmpeg;info.ffprobe=tools.ffprobe;
        auto query=[&](const QString &program,const QStringList &args)->QString {
            if(cancel.load()){info.cancelled=true;return {};}
            if(program.isEmpty()){info.errors<<"ffprobe: unavailable";return {};}
            QProcess process;process.setProcessChannelMode(QProcess::MergedChannels);process.start(program,args);
            if(!process.waitForStarted(3000)){info.errors<<Message::format("Media tool cannot start: %1",{program}).withDetail(process.errorString());return {};}
            QElapsedTimer timer;timer.start();QByteArray output;
            while(true){
                process.waitForFinished(50);output+=process.readAll();
                if(cancel.load()||timer.elapsed()>10000||output.size()>2*1024*1024){
                    process.kill();process.waitForFinished(1000);info.cancelled=cancel.load();
                    if(!info.cancelled)info.errors<<Message::format("Media tool query timed out or exceeded its output limit: %1",{program});
                    return {};
                }
                if(process.state()==QProcess::NotRunning)break;
            }
            if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0){
                info.errors<<Message::format("Media tool query failed: %1",{program}).withDetail(QString::fromUtf8(output).left(1024));return {};
            }
            return QString::fromUtf8(output);
        };
        info.ffmpegVersion=query(info.ffmpeg,{"-hide_banner","-version"}).section('\n',0,0);
        info.ffprobeVersion=query(info.ffprobe,{"-hide_banner","-version"}).section('\n',0,0);
        if(!info.cancelled&&info.ffmpegVersion.isEmpty())info.errors<<"FFmpeg: no version information";
        if(!info.cancelled&&info.ffprobeVersion.isEmpty())info.errors<<"ffprobe: no version information";
        if(!info.ffmpegVersion.isEmpty()){
            auto encoders=query(info.ffmpeg,{"-hide_banner","-encoders"});
            auto filters=query(info.ffmpeg,{"-hide_banner","-filters"});
            auto contains=[](const QString &listing,const QString &name){
                for(const auto &line:listing.split('\n'))if(line.simplified().section(' ',1,1)==name)return true;
                return false;
            };
            for(const auto &name:QStringList{"libx264","aac","libvpx-vp9","libopus","qtrle","pcm_f32le"})
                if(!contains(encoders,name))info.missingEncoders<<name;
            for(const auto &name:QStringList{"asetpts","adelay","apad","scale","format","aresample","aformat"})
                if(!contains(filters,name))info.missingFilters<<name;
        }
        return info;
    }

}
