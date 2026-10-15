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
    Score importSource(const QString &path,const std::atomic_bool *cancel,Progress progress) {
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly))throw Failure(Message("File operation failed").withDetail(file.errorString()));
        return parseSource(file.readAll(),QFileInfo(path).suffix().toLower(),path,cancel,progress);
    }
    Score parseSource(QByteArray bytes,const QString &format,const QString &path,const std::atomic_bool *cancel,Progress progress) {
        if(format=="svp")return parseSvp(bytes,path,cancel,progress);
        Score score;
        if(format=="ust")score=parseUst(bytes,path,cancel,progress);
        else if(format=="vsqx")score=parseVsqx(bytes,path,cancel,progress);
        else throw Failure(Message::format("Unsupported source format: %1",{format}));
        score.raw=bytes;
        score.sourcePath=path;
        score.sourceFormat=format;
        score.hash=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        score.time.validate();
        if(progress)progress(100);
        return score;
    }
}
