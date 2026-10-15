// SPDX-License-Identifier: GPL-3.0-or-later
#include "render.h"
#include "core/executable.h"
#include <QPainter>
#include <QImageReader>
#include <QFontInfo>
#include <QTextCharFormat>
#include <cmath>
#include <cstdio>
#include <cerrno>
#include <cstring>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace chosuta {
    void Scene::setTiming(const Project &p,std::shared_ptr<const Evaluation> view){evaluation=view?std::move(view):std::make_shared<Evaluation>(p);project=p;events=evaluation->resolved;appearance=evaluation->appearance;prepareSubtitles();}
    Scene::Scene(const Project &p,std::shared_ptr<const Evaluation> view):evaluation(view?std::move(view):std::make_shared<Evaluation>(p)),project(p),events(evaluation->resolved),appearance(evaluation->appearance) {
        configureImageReaderLimit();
        const double decodeScale=std::max(1.,p.canvas.characterScale);
        imageCacheFit={std::max(1,qRound(p.canvas.width*decodeScale)),std::max(1,qRound(p.canvas.height*decodeScale))};
        for(auto it=p.assets.cbegin();it!=p.assets.cend();++it){
            QImageReader reader(it.value());const auto size=reader.size();
            if(!reader.canRead()||!size.isValid()||qint64(size.width())*size.height()*4>256LL*1024*1024)
                diagnostics.append(Message::format("Missing, invalid or oversized image: %1",{it.key()}));
            else {
                imageSizes[it.key()]=size;available.insert(it.key());
                // Resolve aliases once per scene, never on each rendered frame.
                const QFileInfo file(it.value());const auto canonical=file.canonicalFilePath();
                imagePaths[it.key()]=canonical.isEmpty()?file.absoluteFilePath():canonical;
            }
        }
        qint64 bytes=0;
        if(!p.canvas.backgroundImage.isEmpty()){
            QImageReader reader(p.canvas.backgroundImage);background=reader.read();
            if(background.isNull())diagnostics.append(Message("Background image cannot be read").withDetail(reader.errorString()));
            else bytes=background.sizeInBytes();
        }
        images.setMaxCost(int(std::max<qint64>(256LL*1024*1024,512LL*1024*1024-bytes)));
        // Decode just the fallback and initial frame. Other used images enter a bounded,
        // path-deduplicated cache on demand; inactive combinations are never decoded.
        if(available.contains(p.fallback))imageFor(p.fallback);
        const auto first=selectionAt(0);if(!first.missing)imageFor(first.id);
        if(appearance.plan())diagnostics.append(appearance.plan()->diagnostics);
        prepareSubtitles();
    }
    QImage Scene::imageFor(const QString &id)const {
        if(!available.contains(id))return {};
        const auto path=imagePaths.value(id);
        auto target=imageSizes.value(id);
        target.scale(imageCacheFit,Qt::KeepAspectRatio);
        if(target.width()>imageSizes.value(id).width())target=imageSizes.value(id);
        const QString key=path+QString("/%1x%2").arg(target.width()).arg(target.height());
        if(auto *image=images.object(key))return *image;
        QImageReader reader(path);reader.setScaledSize(target);auto image=reader.read();++decodeCount;
        if(image.isNull()){
            available.remove(id);const auto error=Message::format("Image cannot be read: %1",{id}).withDetail(reader.errorString());if(!diagnostics.contains(error))diagnostics.append(error);return {};
        }
        const auto cost=image.sizeInBytes();
        if(cost>images.maxCost()){
            available.remove(id);const auto error=Message::format("Image exceeds cache budget: %1",{id});if(!diagnostics.contains(error))diagnostics.append(error);return {};
        }
        images.insert(key,new QImage(image),int(cost));return image;
    }
    AssetSelection Scene::selectionAt(double seconds)const{return appearance.at(project,events,seconds,true,&available);}
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
        QImage img;
        for(int attempt=0;attempt<10&&img.isNull();++attempt){const auto selection=selectionAt(seconds);if(selection.missing)break;img=imageFor(selection.id);}
        if(!img.isNull()){
            QSizeF scaled(img.size());scaled.scale(size,Qt::KeepAspectRatio);scaled*=c.characterScale;
            QRectF rect(size.width()*c.characterX-scaled.width()/2,size.height()*c.characterY-scaled.height()/2,scaled.width(),scaled.height());
            painter.drawImage(rect,img);
        }
        if(project.subtitlesEnabled){
            painter.save();
            painter.scale(double(size.width())/c.width,double(size.height())/c.height);
            for(int rowIndex=0;rowIndex<subtitleRows.size();++rowIndex){const auto&row=subtitleRows[rowIndex];
                auto it=std::upper_bound(row.begin(),row.end(),seconds,[](double t,const RenderCue &v){return t<v.interval.start;});
                // Normal UI edits reject overlap. Imported/tempo-shifted collisions
                // still retain every cue and render in stable order, with diagnostics.
                const auto&ends=subtitlePrefixEnds[rowIndex];auto first=std::upper_bound(ends.begin(),ends.begin()+(it-row.begin()),seconds);
                for(auto v=row.begin()+(first-ends.begin());v!=it;++v)if(seconds<v->interval.end&&!v->cue.text.isEmpty()){
                    auto layout=textLayout(v->style,v->cue.text);
                    painter.setPen(v->style.color);
                    layout->text->draw(&painter,QPointF(c.width*v->style.x-layout->size.width()/2,c.height*v->style.y-layout->size.height()/2));
                }
            }
            painter.restore();
        }
        painter.end();
        return out;
    }
    QRectF Scene::characterRect(double seconds)const {
        const auto selection=selectionAt(seconds);if(selection.missing||!imageSizes.contains(selection.id))return {};
        const auto&c=project.canvas;QSizeF size(imageSizes.value(selection.id));size.scale(QSizeF(c.width,c.height),Qt::KeepAspectRatio);size*=c.characterScale;
        return {c.width*c.characterX-size.width()/2,c.height*c.characterY-size.height()/2,size.width(),size.height()};
    }
    std::shared_ptr<Scene::TextLayout> Scene::textLayout(const SubtitleStyle &s,const QString &text)const {
        const auto&c=project.canvas;
        QString key=text+QChar(0)+s.family+QChar(0)+s.alignment+QString("/%1/%2/%3/%4/%5/%6/%7/%8").arg(c.width).arg(c.height).arg(s.width,0,'g',17).arg(s.fontHeight,0,'g',17).arg(s.bold).arg(s.italic).arg(s.outline).arg(s.color.rgba());
        auto found=textCache.constFind(key);if(found!=textCache.cend())return found.value();
        auto result=std::make_shared<TextLayout>();QFont font;if(!s.family.isEmpty())font.setFamily(s.family);font.setPixelSize(std::max(1,qRound(c.height*s.fontHeight)));font.setBold(s.bold);font.setItalic(s.italic);
        QString rendered=text;rendered.replace("\r\n","\n");rendered.replace('\n',QChar::LineSeparator);
        result->text=std::make_unique<QTextLayout>(rendered,font);
        QTextOption option;option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);option.setAlignment(s.alignment=="left"?Qt::AlignLeft:s.alignment=="right"?Qt::AlignRight:Qt::AlignHCenter);result->text->setTextOption(option);
        QTextCharFormat format;format.setForeground(s.color);QColor outlineColor(0,0,0,s.color.alpha());if(s.outline)format.setTextOutline(QPen(outlineColor,std::max(.5,font.pixelSize()*.065),Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        result->text->setFormats({QTextLayout::FormatRange{0,int(text.size()),format}});
        result->text->setCacheEnabled(true);result->text->beginLayout();double y=0;
        while(true){auto line=result->text->createLine();if(!line.isValid())break;line.setLineWidth(c.width*s.width);line.setPosition({0,y});y+=line.height();}
        result->text->endLayout();result->size={c.width*s.width,std::max(y,double(font.pixelSize()))};
        // Bound cache by characters as well as entries; no per-cue full-frame image.
        if(textCache.size()>=64)textCache.clear();
        textCache.insert(key,result);return result;
    }
    QRectF Scene::subtitleRect(const SubtitleStyle &s,const QString &text)const {
        auto layout=textLayout(s,text);return {project.canvas.width*s.x-layout->size.width()/2,project.canvas.height*s.y-layout->size.height()/2,layout->size.width(),layout->size.height()};
    }
    void Scene::prepareSubtitles() {
        subtitleRows.clear();subtitlePrefixEnds.clear();subtitleTimes.clear();if(!project.subtitlesEnabled)return;
        QSet<QString> warned;
        for(const auto&t:project.subtitles)if(t.enabled){QVector<RenderCue> row;
            for(const auto&c:t.cues){auto v=evaluation->subtitles.value(c.id);subtitleTimes.insert(c.id,v);auto s=c.ownStyle?c.style:t.style;row.append({c,v,s});
                if(v.orphan)diagnostics.append(Message::format("Subtitle source missing; retained timing: %1 / %2",{t.name,c.id}));
                if(!s.family.isEmpty()&&!warned.contains(s.family)){warned.insert(s.family);QFont f(s.family);auto actual=QFontInfo(f).family();if(actual.compare(s.family,Qt::CaseInsensitive)!=0)diagnostics.append(Message::format("Subtitle font substituted: %1 → %2",{s.family,actual}));}
                if(project.output.duration>0&&v.end>project.output.duration)diagnostics.append(Message::format("Subtitle exceeds fixed animation duration: %1 / %2",{t.name,c.id}));
            }
            std::stable_sort(row.begin(),row.end(),[](const auto&a,const auto&b){return a.interval.start==b.interval.start?a.cue.id<b.cue.id:a.interval.start<b.interval.start;});
            for(int i=1;i<row.size();++i)if(row[i].interval.start<row[i-1].interval.end)diagnostics.append(Message::format("Subtitle overlap: %1",{t.name}));
            QVector<double> ends;double end=-std::numeric_limits<double>::infinity();for(const auto&v:row){end=std::max(end,v.interval.end);ends.append(end);}
            subtitleRows.append(row);subtitlePrefixEnds.append(ends);
        }
    }
    std::optional<SubtitleInterval> Scene::cueInterval(const QString &id)const {auto it=subtitleTimes.constFind(id);return it==subtitleTimes.cend()?std::nullopt:std::optional<SubtitleInterval>(it.value());}
    void Scene::setLayout(const CanvasSettings &c,const QVector<SubtitleTrack> &tracks,bool enabled) {
        project.canvas=c;project.subtitles=tracks;project.subtitlesEnabled=enabled;
        // Drag changes geometry/style, never cue time or source. Reuse intervals.
        int rowIndex=0;for(const auto&t:tracks)if(enabled&&t.enabled&&rowIndex<subtitleRows.size()){
            QHash<QString,SubtitleStyle> styles;for(const auto&cue:t.cues)if(cue.ownStyle)styles.insert(cue.id,cue.style);
            auto&row=subtitleRows[rowIndex++];for(auto &v:row)v.style=styles.value(v.cue.id,t.style);
        }
    }
    void Scene::setSubtitleText(const QString &id,const QString &text) {
        for(auto&t:project.subtitles)for(auto&c:t.cues)if(c.id==id)c.text=text;
        for(auto&row:subtitleRows)for(auto&v:row)if(v.cue.id==id)v.cue.text=text;
    }
    void validateExport(const Project&p) {
        validateOutputSettings(p.output);
        const auto&s=p.output;
        validateCanvas(p.canvas);
        validateSubtitles(p);
        const auto &c=p.canvas;
        if(!QStringList {
            "mp4","webm","mov"
        }
        .contains(s.format))throw Failure("Unsupported video format");
        if(s.format!="mov"&&((c.width%2)||(c.height%2)))throw Failure("MP4/WebM dimensions must be even");
        if(c.transparent&&s.format!="mov")throw Failure("Transparency requires MOV/QTRLE");
        double d=p.duration();
        if(!std::isfinite(d)||d<=0||d>21600||!std::isfinite(s.syncOffset)||!std::isfinite(s.audioOffset))throw Failure("Export duration must be positive and no more than 6 hours");
        if(p.selected.isEmpty())throw Failure("Select a track and generate events first");
        if(!p.audioPath.isEmpty()&&!QFileInfo::exists(p.audioPath))throw Failure(Message::format("Missing audio: %1",{p.audioPath}));
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
    ExportResult exportVideo(const Project&inputProject,const MediaTools&tools,const QString&path,const std::atomic_bool&cancel,Progress progress,bool overwrite) {
        ExportResult r;
        Project p=inputProject;
        try {
            const bool stale=p.timing.enabled&&!p.timing.sources.isEmpty()&&!timingCorrectionCurrent(p);
            validateExport(p);
            QFileInfo target(path);
            if(target.exists()&&!overwrite)throw Failure("Output exists; explicit overwrite required");
            Scene scene(p);
            r.diagnostics=scene.diagnostics;
            if(stale)r.diagnostics<<"Waveform correction is stale; using score timing and manual edits.";
            if(!scene.hasBackground())throw Failure(Message::format("Background image is missing or invalid: %1",{p.canvas.backgroundImage}));
            if(p.assets.isEmpty())throw Failure("Import PNG assets or create demo assets first");
            QTemporaryDir temp(target.absoluteDir().filePath(".chosuta-export-XXXXXX"));
            if(!temp.isValid())throw Failure("Cannot create output staging directory");
            QString movie=temp.filePath("encoded."+p.output.format);
            QProcess proc;
            proc.start(tools.ffmpeg,encoderArguments(p,movie),QIODevice::ReadWrite);
            if(!proc.waitForStarted(5000))throw Failure(Message("Cannot start FFmpeg").withDetail(proc.errorString()));
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
                const double frameTime=double(i)*p.output.fpsDen/p.output.fpsNum;
                auto image=scene.frame(frameTime);
                const auto selection=scene.selectionAt(frameTime);
                if(selection.missing){stop();throw Failure(Message::format("Missing or unreadable asset: %1",{selection.id}));}
                for(const auto &message:scene.diagnostics)if(!r.diagnostics.contains(message))r.diagnostics.append(message);
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
                        throw Failure(Message::format("FFmpeg terminated: %1",{QString::fromUtf8(log)}));
                    }
                    if(remaining>0&&proc.bytesToWrite()<1024*1024) {
                        qint64 chunk=std::min<qint64>(remaining,256*1024);
                        auto wrote=proc.write(reinterpret_cast<const char*>(image.constBits())+offset,chunk);
                        if(wrote<0) {
                            stop();
                            throw Failure(Message("FFmpeg input failed").withDetail(proc.errorString()));
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
            if(proc.exitStatus()!=QProcess::NormalExit||proc.exitCode()!=0)throw Failure(Message::format("Encoding failed: %1",{QString::fromUtf8(log)}));
            if(cancel.load()){r.cancelled=true;return r;}
            if(!overwrite&&QFileInfo::exists(path))throw Failure("Output exists; explicit overwrite required");
#ifdef Q_OS_WIN
            if(!MoveFileExW(reinterpret_cast<LPCWSTR>(movie.utf16()),reinterpret_cast<LPCWSTR>(path.utf16()),MOVEFILE_REPLACE_EXISTING))
                throw Failure(Message("Cannot commit encoded output").withDetail(QString::number(GetLastError())));
#else
            if(std::rename(QFile::encodeName(movie).constData(),QFile::encodeName(path).constData())!=0)
                throw Failure(Message("Cannot commit encoded output").withDetail(QString::fromLocal8Bit(std::strerror(errno))));
#endif
            r.success=true;
            if(progress)progress(100);
        }
        catch(const std::exception&e) {
            r.cancelled=cancel.load();r.error=exceptionMessage(e);
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
