// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include <cmath>
namespace chosuta {
    qint64 sourceBlick(qint64 ticks,qint64 resolution) {
        // Do not quantize source ticks to a common PPQ. Keep original positions on Note.
        const long double value=static_cast<long double>(ticks)*Blick/resolution;
        if(!std::isfinite(value)||std::abs(value)>MusicLimit)throw Failure("Music position exceeds limit");
        const qint64 whole=ticks/resolution,remainder=ticks%resolution;
        return whole*Blick+std::llround(static_cast<long double>(remainder)*Blick/resolution);
    }
    Score importSource(const QString &path,const std::atomic_bool *cancel,Progress progress,const QString &vvprojSection) {
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly))throw Failure(Message("File operation failed").withDetail(file.errorString()));
        auto format=QFileInfo(path).suffix().toLower();
        if(format=="vvproj"){
            if(!QStringList{"auto","talk","song"}.contains(vvprojSection))throw Failure("VVPROJ section must be auto, talk or song");
            format=vvprojSection=="song"?"vvproj-song":vvprojSection=="talk"?"vvproj":"vvproj-auto";
        }
        return parseSource(file.readAll(),format,path,cancel,progress);
    }
    Score parseSource(QByteArray bytes,const QString &format,const QString &path,const std::atomic_bool *cancel,Progress progress) {
        if(format=="svp")return parseSvp(bytes,path,cancel,progress);
        Score score;
        if(format=="ust")score=parseUst(bytes,path,cancel,progress);
        else if(format=="ustx")score=parseUstx(bytes,path,cancel,progress);
        else if(format=="vsq")score=parseVsq(bytes,path,cancel,progress);
        else if(format=="vsqx")score=parseVsqx(bytes,path,cancel,progress);
        else if(format=="musicxml"||format=="xml")score=parseMusicXml(bytes,path,cancel,progress);
        else if(format=="mxl")score=parseMxl(bytes,path,cancel,progress);
        else if(format=="vpr")score=parseVpr(bytes,path,cancel,progress);
        else if(format=="ccs")score=parseCcs(bytes,path,cancel,progress);
        else if(format=="lab")score=parseLab(bytes,path,cancel,progress);
        else if(format=="ds")score=parseDs(bytes,path,cancel,progress);
        else if(format=="vvproj")score=parseVvproj(bytes,path,cancel,progress);
        else if(format=="vvproj-song")score=parseVvprojSong(bytes,path,cancel,progress);
        else if(format=="vvproj-auto")score=parseVvprojAuto(bytes,path,cancel,progress);
        else throw Failure(Message::format("Unsupported source format: %1",{format}));
        score.raw=bytes;
        score.sourcePath=path;
        score.sourceFormat=format=="vvproj-auto"?(score.hasMusic?"vvproj-song":"vvproj"):(format=="xml"?"musicxml":format);
        score.hash=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        if(score.hasMusic)score.time.validate();
        if(progress)progress(100);
        return score;
    }
}
