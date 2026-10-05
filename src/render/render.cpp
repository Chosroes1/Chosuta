// SPDX-License-Identifier: GPL-3.0-or-later
#include "render.h"
#include "core/executable.h"
#include <QPainter>
#include <QImageReader>
#include <cmath>
namespace chosuta {
    Scene::Scene(const Project&p):project(p),events(resolveEvents(p.effective())) {
        QImageReader::setAllocationLimit(256);
        qint64 bytes=0;
        auto assets=p.assets;
        for(auto it=assets.begin();it!=assets.end();++it) {
            QImageReader reader(it.value());
            QImage image=reader.read();
            if(image.isNull())diagnostics.append(it.key()+": "+reader.errorString());
            else if(bytes+image.sizeInBytes()>512LL*1024*1024)diagnostics.append(it.key()+": decoded image cache exceeds 512 MiB");
            else {
                bytes+=image.sizeInBytes();
                images[it.key()]=image;
            }
        }
        if(!p.canvas.backgroundImage.isEmpty()){
            QImageReader reader(p.canvas.backgroundImage);auto image=reader.read();
            if(image.isNull())diagnostics.append("background: "+reader.errorString());
            else if(bytes+image.sizeInBytes()>512LL*1024*1024)diagnostics.append("background: decoded image cache exceeds 512 MiB");
            else background=image;
        }
    }
    QImage Scene::frame(double seconds,QSize size)const {
        if(size.isEmpty())size= {
            project.canvas.width,project.canvas.height
        };
        QImage out(size,QImage::Format_RGBA8888);
        out.fill(project.canvas.transparent?Qt::transparent:project.canvas.background);
        QPainter painter(&out);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const auto &c=project.canvas;
        
        if(!background.isNull()){
            QSizeF scaled(background.size());
            if(c.backgroundFit=="stretch")scaled=size;
            else scaled.scale(size,c.backgroundFit=="cover"?Qt::KeepAspectRatioByExpanding:Qt::KeepAspectRatio);
            painter.drawImage(QRectF((size.width()-scaled.width())/2,(size.height()-scaled.height())/2,scaled.width(),scaled.height()),background);
        }
        auto img=images.value(shapeAt(project,events,seconds,true));
        if(img.isNull())img=images.value(project.fallback);
        if(!img.isNull()){
            QSizeF scaled(img.size());scaled.scale(size,Qt::KeepAspectRatio);scaled*=c.characterScale;
            QRectF rect(size.width()*c.characterX-scaled.width()/2,size.height()*c.characterY-scaled.height()/2,scaled.width(),scaled.height());
            painter.drawImage(rect,img);
        }
        painter.end();
        return out;
    }
    void validateExport(const Project&p) {
        const auto&s=p.output;
        validateCanvas(p.canvas);
        const auto &c=p.canvas;
        if(s.fpsNum<1||s.fpsNum>240000||s.fpsDen<1||s.fpsDen>10000||double(s.fpsNum)/s.fpsDen<1||double(s.fpsNum)/s.fpsDen>240)throw Failure("Frame rate must be 1..240 fps");
        if(!QStringList {
            "mp4","webm","mov"
        }
        .contains(s.format))throw Failure("Unsupported video format");
        if(s.format!="mov"&&((c.width%2)||(c.height%2)))throw Failure("MP4/WebM dimensions must be even");
        if(c.transparent&&s.format!="mov")throw Failure("Transparency requires MOV/QTRLE");
        if(s.crf<0||s.crf>51||s.bitrateKbps<0||s.bitrateKbps>500000)throw Failure("Invalid quality/bitrate");
        double d=p.duration();
        if(!std::isfinite(d)||d<=0||d>21600||!std::isfinite(s.syncOffset)||!std::isfinite(s.audioOffset))throw Failure("Export duration must be positive and no more than 6 hours");
        if(s.ffmpeg.isEmpty())throw Failure("Invalid background or FFmpeg path");
        if(p.selected.isEmpty())throw Failure("Select a track and generate events first");
        if(!p.audioPath.isEmpty()&&!QFileInfo::exists(p.audioPath))throw Failure("Missing audio: "+p.audioPath);
    }
    QStringList availableEncoders(const QString&ffmpeg) {
        QProcess p;
        p.start(resolveExecutable(ffmpeg), {
            "-hide_banner","-encoders"
        });
        if(!p.waitForStarted(5000)||!p.waitForFinished(10000)) {
            p.kill();
            p.waitForFinished();
            throw Failure("Cannot start/query FFmpeg: "+p.errorString());
        }
        if(p.exitCode()!=0)throw Failure(QString::fromUtf8(p.readAllStandardError()));
        QStringList result;
        auto lines=QString::fromUtf8(p.readAllStandardOutput()).split('\n');
        for(const auto&line:lines) {
            auto a=line.simplified().split(' ');
            if(a.size()>1&&a[0].size()==6&&(a[0][0]=='V'||a[0][0]=='A'))result<<a[1];
        }
        return result;
    }
    QStringList encoderArguments(const Project&p,const QString&temporary) {
        const auto&s=p.output;
        QString fps=QString("%1/%2").arg(s.fpsNum).arg(s.fpsDen),duration=QString::number(p.duration(),'f',9);
        QStringList a= {
            "-hide_banner","-loglevel","warning","-nostdin","-y","-f","rawvideo","-pixel_format","rgba","-video_size",QString("%1x%2").arg(p.canvas.width).arg(p.canvas.height),"-framerate",fps,"-i","pipe:0"
        };
        if(!p.audioPath.isEmpty()) {
            if(s.audioOffset<0)a<<"-ss"<<QString::number(-s.audioOffset,'f',9);
            a<<"-i"<<p.audioPath<<"-map"<<"0:v:0";
            // Normalize timestamps and delay/pad audio to the requested video duration.
            a<<"-filter_complex"<<QString("[1:a:0]asetpts=PTS-STARTPTS,adelay=%1:all=1,apad[a]").arg(qRound64(std::max(0.,s.audioOffset)*1000))<<"-map"<<"[a]";
            a<<"-c:a"<<(s.format=="webm"?"libopus":"aac");
        }
        else a<<"-an";
        if(s.format=="mp4")a<<"-c:v"<<"libx264"<<"-pix_fmt"<<"yuv420p"<<"-preset"<<"veryfast"<<"-movflags"<<"+faststart";
        else if(s.format=="webm")a<<"-c:v"<<"libvpx-vp9"<<"-pix_fmt"<<"yuv420p"<<"-deadline"<<"good"<<"-cpu-used"<<"4";
        else a<<"-c:v"<<"qtrle"<<"-pix_fmt"<<"argb";
        if(s.format!="mov") {
            if(s.bitrateKbps)a<<"-b:v"<<QString::number(s.bitrateKbps)+"k";
            else {
                a<<"-crf"<<QString::number(s.crf);
                if(s.format=="webm")a<<"-b:v"<<"0";
            }
        }
        a<<"-t"<<duration<<"-f"<<(s.format=="mp4"?"mp4":s.format=="mov"?"mov":"webm")<<temporary;
        return a;
    }
    ExportResult exportVideo(const Project&p,const QString&path,const std::atomic_bool&cancel,Progress progress,bool overwrite) {
        ExportResult r;
        try {
            validateExport(p);
            QFileInfo target(path);
            if(target.exists()&&!overwrite)throw Failure("Output exists; explicit overwrite required");
            auto encoders=availableEncoders(p.output.ffmpeg);
            QString encoder=p.output.format=="mp4"?"libx264":p.output.format=="webm"?"libvpx-vp9":"qtrle";
            if(!encoders.contains(encoder))throw Failure("Unavailable video encoder: "+encoder);
            if(!p.audioPath.isEmpty()&&!encoders.contains(p.output.format=="webm"?"libopus":"aac"))throw Failure("Unavailable audio encoder");
            Scene scene(p);
            if(!scene.hasBackground())throw Failure("Background image is missing or invalid: "+p.canvas.backgroundImage);
            if(!scene.hasFallback())throw Failure("Fallback PNG is missing or invalid: "+p.fallback+"; "+scene.diagnostics.join("; "));
            if(p.assets.isEmpty())throw Failure("Import PNG assets or create demo assets first");
            QTemporaryDir temp(target.absoluteDir().filePath(".chosuta-export-XXXXXX"));
            if(!temp.isValid())throw Failure("Cannot create output staging directory");
            QString movie=temp.filePath("encoded."+p.output.format);
            QProcess proc;
            proc.start(resolveExecutable(p.output.ffmpeg),encoderArguments(p,movie),QIODevice::ReadWrite);
            if(!proc.waitForStarted(5000))throw Failure("FFmpeg: "+proc.errorString());
            QByteArray log;
            auto drain=[&]() {
                log+=proc.readAllStandardError();
                if(log.size()>32768)log=log.right(32768);
            };
            auto stop=[&]() {
                proc.kill();
                proc.waitForFinished(3000);
            };
            // Integer frame index evaluated at absolute time; no event/frame accumulation.
            qint64 count=qint64(std::ceil(p.duration()*p.output.fpsNum/p.output.fpsDen-1e-10));
            for(qint64 i=0;i<count;++i) {
                if(cancel.load()) {
                    stop();
                    r.cancelled=true;
                    return r;
                }
                auto image=scene.frame(double(i)*p.output.fpsDen/p.output.fpsNum);
                qint64 remaining=image.sizeInBytes(),offset=0;
                QElapsedTimer stall;
                stall.start();
                while(remaining>0||proc.bytesToWrite()>0) {
                    if(cancel.load()) {
                        stop();
                        r.cancelled=true;
                        return r;
                    }
                    drain();
                    if(proc.state()==QProcess::NotRunning) {
                        throw Failure("FFmpeg terminated: "+QString::fromUtf8(log));
                    }
                    if(remaining>0&&proc.bytesToWrite()<1024*1024) {
                        qint64 chunk=std::min<qint64>(remaining,256*1024);
                        auto wrote=proc.write(reinterpret_cast<const char*>(image.constBits())+offset,chunk);
                        if(wrote<0) {
                            stop();
                            throw Failure(proc.errorString());
                        }
                        offset+=wrote;
                        remaining-=wrote;
                    }
                    if(proc.bytesToWrite()>0) {
                        if(proc.waitForBytesWritten(100))stall.restart();
                        else if(stall.elapsed()>30000) {
                            stop();
                            throw Failure("FFmpeg pipe stalled");
                        }
                    }
                }
                r.frames=int(i+1);
                if(progress)progress(int((i+1)*95/count));
            }
            proc.closeWriteChannel();
            QElapsedTimer finish;
            finish.start();
            while(!proc.waitForFinished(100)) {
                drain();
                if(cancel.load()) {
                    stop();
                    r.cancelled=true;
                    return r;
                }
                if(finish.elapsed()>120000) {
                    stop();
                    throw Failure("FFmpeg completion timed out");
                }
            }
            drain();
            if(proc.exitStatus()!=QProcess::NormalExit||proc.exitCode()!=0)throw Failure("Encoding failed: "+QString::fromUtf8(log));
            QFile input(movie);
            QSaveFile output(path);
            if(!input.open(QIODevice::ReadOnly)||!output.open(QIODevice::WriteOnly))throw Failure("Cannot commit encoded output: "+output.errorString());
            while(!input.atEnd()) {
                if(cancel.load()) {
                    output.cancelWriting();
                    r.cancelled=true;
                    return r;
                }
                auto chunk=input.read(1024*1024);
                if(chunk.isEmpty()&&input.error()!=QFile::NoError) {
                    output.cancelWriting();
                    throw Failure(input.errorString());
                }
                if(output.write(chunk)!=chunk.size()) {
                    output.cancelWriting();
                    throw Failure(output.errorString());
                }
            }
            if(cancel.load()) {
                output.cancelWriting();
                r.cancelled=true;
                return r;
            }
            if(!overwrite&&QFileInfo::exists(path)) {
                output.cancelWriting();
                throw Failure("Output appeared during export; overwrite refused");
            }
            if(!output.commit())throw Failure(output.errorString());
            r.success=true;
            if(progress)progress(100);
        }
        catch(const std::exception&e) {
            r.error=QString::fromUtf8(e.what());
        }
        return r;
    }
    void createDemoAssets(const QString&directory) {
        QDir dir(directory);
        if(!dir.exists()&&!QDir().mkpath(directory))throw Failure("Cannot create demo directory");
        QStringList shapes= {
            "A","I","U","E","O","closed","rest","breath","unknown"
        };
        QVector<QColor>colors= {
            Qt::red,Qt::green,Qt::blue,Qt::yellow,Qt::magenta,Qt::gray,Qt::lightGray,Qt::cyan,QColor("orange")
        };
        for(int i=0;i<shapes.size();++i) {
            QString path=dir.filePath(shapes[i]+".png");
            if(QFileInfo::exists(path))continue;
            QImage img(256,256,QImage::Format_RGBA8888);
            img.fill(Qt::transparent);
            QPainter painter(&img);
            painter.setPen(QPen(Qt::black,4));
            painter.setBrush(colors[i]);
            painter.drawEllipse(24,24,208,208);
            painter.setBrush(Qt::black);
            painter.drawEllipse(75,85,12,12);
            painter.drawEllipse(169,85,12,12);
            if(i<5)painter.drawEllipse(QRectF(100-i*3,140,56+i*6,18+i*4));
            else painter.drawLine(96,160,160,160);
            painter.end();
            if(!img.save(path,"PNG"))throw Failure("Cannot save demo image");
        }
    }
}
