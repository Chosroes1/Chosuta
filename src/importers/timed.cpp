// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include <QStringDecoder>
#include <cmath>
#include <algorithm>
namespace chosuta {
namespace {
void check(const std::atomic_bool *cancel){if(cancel&&cancel->load())throw Failure("Cancelled");}
Score absoluteScore(){
    Score s;s.hasMusic=false;s.sourceResolution=0;s.time.tempos.clear();s.time.meters.clear();
    s.diagnostics.append({"source","absolute-time",Message("Supplied phoneme timing; mouth shapes are rule estimates. No musical beat map is available.")});
    return s;
}
[[noreturn]] void labError(int line,const char *key){throw Failure(Message::format(key,{QString::number(line)}));}
[[noreturn]] void dsError(int part,const char *key){throw Failure(Message::format(key,{QString::number(part)}));}
QStringList words(const QString &s){return s.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);}
}
Score parseLab(const QByteArray &bytes,const QString &path,const std::atomic_bool *cancel,Progress progress){
    check(cancel);
    if(bytes.size()>64*1024*1024)throw Failure("Input exceeds 64 MiB");
    QStringDecoder decoder(QStringDecoder::Utf8);QString text=decoder(bytes);
    if(decoder.hasError())throw Failure("LAB requires UTF-8 text");
    auto score=absoluteScore();score.sourceResolution=10000000;score.sourceVersion="monophone-100ns";
    Track track;track.id="lab/track:0";track.name=QFileInfo(path).completeBaseName();
    if(track.name.isEmpty())track.name="LAB";
    qint64 previous=0;const auto lines=text.split('\n');int count=0;
    for(int i=0;i<lines.size();++i){
        check(cancel);if(progress&&i%128==0)progress(i*90/std::max(1,int(lines.size())));
        const auto row=words(lines[i].trimmed());if(row.isEmpty())continue;
        if(row.size()!=3||row.value(2).size()>128)labError(i+1,"LAB line %1: expected start end phone (integer 100 ns, monophone)");
        static const QRegularExpression integer("^[0-9]+$");
        bool a=false,b=false;const auto start=row[0].toLongLong(&a),end=row[1].toLongLong(&b);
        if(!a||!b||!integer.match(row[0]).hasMatch()||!integer.match(row[1]).hasMatch()||end<start||end>8640000000000LL||start<previous)
            labError(i+1,"LAB line %1: invalid, unordered or overlapping 100 ns boundaries");
        // HTS full-context labels require their own producer-specific adapter.
        if(row[2].contains('^')||row[2].contains('=')||row[2].contains("/A:"))labError(i+1,"LAB line %1: full-context labels are not supported; export monophone labels");
        if(++count>200000)throw Failure("More than 200000 source phonemes");
        TimedPhone phone;phone.id=track.id+QString("/line:%1").arg(i+1);phone.groupInstance="lab";
        phone.phone=row[2];phone.language="ja";phone.phoneset="romaji";phone.sourceStart=start;phone.sourceEnd=end;
        phone.start=double(start)/10000000;phone.end=double(end)/10000000;
        phone.original={{"line",i+1},{"start",row[0]},{"end",row[1]},{"phone",row[2]}};
        track.timedPhones.append(phone);previous=end;score.endSeconds=phone.end;
    }
    if(track.timedPhones.isEmpty())throw Failure("No timed phonemes in source");
    score.tracks.append(track);return score;
}
Score parseDs(const QByteArray &bytes,const QString &path,const std::atomic_bool *cancel,Progress progress){
    check(cancel);
    if(bytes.size()>64*1024*1024)throw Failure("Input exceeds 64 MiB");
    QJsonParseError error;auto doc=QJsonDocument::fromJson(bytes,&error);
    if(error.error!=QJsonParseError::NoError)throw Failure(Message::format("Invalid JSON at %1",{QString::number(error.offset)}).withDetail(error.errorString()));
    QJsonArray parts;
    if(doc.isArray())parts=doc.array();else if(doc.isObject())parts.append(doc.object());
    if(parts.isEmpty())throw Failure("No timed phonemes in source");
    auto score=absoluteScore();score.sourceVersion="ph_seq/ph_dur-seconds";
    Track track;track.id="ds/track:0";track.name=QFileInfo(path).completeBaseName();if(track.name.isEmpty())track.name="DS";
    int count=0;
    for(int pi=0;pi<parts.size();++pi){
        check(cancel);if(progress)progress(pi*90/std::max(1,int(parts.size())));
        if(!parts[pi].isObject())dsError(pi+1,"DS segment %1: expected an object");
        const auto o=parts[pi].toObject();
        if(!o["ph_seq"].isString()||!o["ph_dur"].isString())dsError(pi+1,"DS segment %1: complete ph_seq and ph_dur strings are required; duration prediction is not supported");
        const auto phones=words(o["ph_seq"].toString()),durations=words(o["ph_dur"].toString());
        if(phones.isEmpty()||phones.size()!=durations.size())dsError(pi+1,"DS segment %1: ph_seq and ph_dur counts differ or are empty");
        double offset=0;
        if(o.contains("offset")){
            bool ok=o["offset"].isDouble();offset=o["offset"].toDouble();
            if(o["offset"].isString())offset=o["offset"].toString().toDouble(&ok);
            if(!ok||!std::isfinite(offset)||offset<0||offset>864000)dsError(pi+1,"DS segment %1: invalid offset in seconds");
        }
        QString language=o["language"].toString("zh");
        long double elapsed=0;
        for(int i=0;i<phones.size();++i){
            check(cancel);if(progress&&i%128==0)progress((pi*90+i*90/std::max(1,int(phones.size())))/std::max(1,int(parts.size())));
            if(++count>200000)throw Failure("More than 200000 source phonemes");
            if(phones[i].size()>128)dsError(pi+1,"DS segment %1: phoneme token exceeds 128 characters");
            bool ok=false;double duration=durations[i].toDouble(&ok);
            if(!ok||!std::isfinite(duration)||duration<0||offset+elapsed+duration>864000)dsError(pi+1,"DS segment %1: invalid phoneme duration in seconds");
            TimedPhone phone;phone.groupInstance=QString("ds/segment:%1").arg(pi);phone.id=track.id+"/"+phone.groupInstance+QString("/phone:%1").arg(i);
            phone.phone=phones[i];phone.language=language;phone.phoneset="diffsinger";
            phone.start=double(offset+elapsed);elapsed+=duration;phone.end=double(offset+elapsed);
            phone.original={{"segment",pi},{"index",i},{"duration",durations[i]},{"offset",o.value("offset")},{"phone",phones[i]}};
            track.timedPhones.append(phone);score.endSeconds=std::max(score.endSeconds,phone.end);
        }
    }
    std::stable_sort(track.timedPhones.begin(),track.timedPhones.end(),[](const auto &a,const auto &b){return a.start<b.start;});
    score.tracks.append(track);return score;
}
}
