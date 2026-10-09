// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"
#include <cmath>
#include <algorithm>
#include <limits>
namespace chosuta {
QString timingBasisHash(const Project &p,const std::atomic_bool *cancel) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    auto add=[&](const QString &s){if(cancel&&cancel->load())throw Failure("Cancelled");auto bytes=s.toUtf8();hash.addData(QByteArray::number(bytes.size()));hash.addData(":");hash.addData(bytes);};
    add(p.score.hash);add(QDir::cleanPath(p.audioPath));
    add(QString::number(p.output.syncOffset,'g',17));add(QString::number(p.output.audioOffset,'g',17));
    for(const auto &t:p.score.time.tempos){add(QString::number(t.position));add(QString::number(t.bpm,'g',17));}
    for(const auto &id:p.selected){add(id);add(QString::number(p.priorities.value(id)));}
    add(p.rules.language);add(QString::number(p.rules.consonantRatio,'g',17));add(QString::number(p.rules.consonantMaxSeconds,'g',17));add(QString::number(p.rules.harmonyTakeover));
    add(QString::fromUtf8(QJsonDocument(consonantsJson(p.rules.consonants)).toJson(QJsonDocument::Compact)));
    for(auto i=p.rules.readings.begin();i!=p.rules.readings.end();++i){add(i.key());add(i.value());}
    add(QString::fromUtf8(QJsonDocument(pronunciationOptionsJson(p.rules.pronunciation)).toJson(QJsonDocument::Compact)));
    for(auto i=p.rules.special.begin();i!=p.rules.special.end();++i){add(i.key());add(i->mode);add(i->shape);add(QString::number(i->holdSeconds,'g',17));}
    for(const auto &e:p.generated){add(e.id);add(e.source);add(e.track);add(e.shape);add(e.provenance);add(e.text);add(QString::number(e.start,'g',17));add(QString::number(e.end,'g',17));}
    return QString::fromLatin1(hash.result().toHex());
}
bool timingCorrectionCurrent(const Project &p) {
    if(p.timing.sources.isEmpty()||p.timing.basisHash!=timingBasisHash(p))return false;
    if(!p.audioContentHash.isEmpty()&&p.audioContentHash!=p.timing.audioHash)return false;
    QFileInfo audio(p.audioPath);
    return audio.isFile()&&audio.size()==p.timing.audioBytes&&audio.lastModified().toMSecsSinceEpoch()==p.timing.audioModified;
}
bool verifyTimingAudio(Project &p,const std::atomic_bool &cancel){
    if(p.timing.sources.isEmpty()||!p.timing.enabled)return true;
    QFile file(p.audioPath);if(!file.open(QIODevice::ReadOnly)){p.audioContentHash="missing";return true;}
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while(!file.atEnd()){if(cancel.load())return false;auto bytes=file.read(65536);if(bytes.isEmpty()&&file.error()!=QFileDevice::NoError){p.audioContentHash="unreadable";return true;}hash.addData(bytes);}
    p.audioContentHash=QString::fromLatin1(hash.result().toHex());return !cancel.load();
}
void validateTimingCorrection(const Project &p) {
    const auto &t=p.timing;
    if(!std::isfinite(t.maxShift)||t.maxShift<0||t.maxShift>1||!std::isfinite(t.maxDurationChange)||t.maxDurationChange<0||t.maxDurationChange>.5||t.sources.size()>500000)throw Failure("Invalid timing correction limits");
    if(!t.sources.isEmpty()&&(t.basisHash.size()!=64||t.audioHash.size()!=64||t.audioBytes<0))throw Failure("Invalid timing correction identity");
    for(auto i=t.sources.begin();i!=t.sources.end();++i){const auto &s=i.value();double duration=s.originalEnd-s.originalStart;
        if(i.key().isEmpty()||!std::isfinite(s.originalStart)||!std::isfinite(s.originalEnd)||!std::isfinite(s.start)||!std::isfinite(s.end)||duration<=0||s.end<=s.start||std::abs(s.start)>864000||std::abs(s.end)>864000||std::abs(s.start-s.originalStart)>t.maxShift+1e-9||std::abs(s.end-s.originalEnd)>t.maxShift+1e-9||std::abs((s.end-s.start)-duration)>duration*t.maxDurationChange+1e-9)throw Failure("Timing correction exceeds score limits");
    }
    if(!t.sources.isEmpty()&&t.basisHash==timingBasisHash(p)){
        QMap<QString,QPair<double,double>> groups;
        for(const auto &e:p.generated){if(!groups.contains(e.source))groups[e.source]={e.start,e.end};else {auto &g=groups[e.source];g.first=std::min(g.first,e.start);g.second=std::max(g.second,e.end);}}
        for(auto i=t.sources.begin();i!=t.sources.end();++i)if(!groups.contains(i.key())||std::abs(groups[i.key()].first-i->originalStart)>1e-9||std::abs(groups[i.key()].second-i->originalEnd)>1e-9)throw Failure("Timing correction does not match score source");
        struct Span{double originalStart,originalEnd,start,end;};QVector<Span> spans;
        for(auto i=groups.begin();i!=groups.end();++i){auto s=i.value();auto j=t.sources.constFind(i.key());spans.append({s.first,s.second,j==t.sources.end()?s.first:j->start,j==t.sources.end()?s.second:j->end});}
        std::sort(spans.begin(),spans.end(),[](const Span&a,const Span&b){return a.originalStart<b.originalStart;});
        double originalEnd=-std::numeric_limits<double>::infinity(),end=originalEnd;
        for(const auto &s:spans){if(originalEnd<=s.originalStart+1e-9&&end>s.start+1e-9)throw Failure("Timing correction introduces an overlap");originalEnd=std::max(originalEnd,s.originalEnd);end=std::max(end,s.end);}
    }
}
QJsonObject timingCorrectionJson(const Project &p) {
    validateTimingCorrection(p);const auto &t=p.timing;QJsonArray sources;
    for(auto i=t.sources.begin();i!=t.sources.end();++i){const auto &s=i.value();sources.append(QJsonObject{{"source",i.key()},{"originalStart",s.originalStart},{"originalEnd",s.originalEnd},{"start",s.start},{"end",s.end}});}
    return {{"maxShift",t.maxShift},{"maxDurationChange",t.maxDurationChange},{"enabled",t.enabled},{"basisHash",t.basisHash},{"audioHash",t.audioHash},{"audioBytes",QString::number(t.audioBytes)},{"audioModified",QString::number(t.audioModified)},{"sources",sources}};
}
void readTimingCorrection(Project &p,const QJsonValue &value) {
    if(!value.isObject())throw Failure("Invalid timing correction data");
    auto o=value.toObject();auto &t=p.timing;
    auto number=[&](const char *key){auto v=o.value(key);if(!v.isDouble())throw Failure("Invalid timing correction number");return v.toDouble();};
    t.maxShift=number("maxShift");t.maxDurationChange=number("maxDurationChange");
    if(!o.value("enabled").isBool()||!o.value("sources").isArray())throw Failure("Invalid timing correction state");
    t.enabled=o["enabled"].toBool();t.basisHash=o["basisHash"].toString();t.audioHash=o["audioHash"].toString();
    bool a=false,b=false;t.audioBytes=o["audioBytes"].toString().toLongLong(&a);t.audioModified=o["audioModified"].toString().toLongLong(&b);
    if(!a||!b||o["sources"].toArray().size()>500000)throw Failure("Invalid timing correction audio identity");
    for(const auto &v:o["sources"].toArray()){if(!v.isObject())throw Failure("Invalid timing correction source");auto s=v.toObject();
        for(auto key:{"originalStart","originalEnd","start","end"})if(!s.value(key).isDouble())throw Failure("Invalid timing correction interval");
        auto id=s["source"].toString();if(t.sources.contains(id))throw Failure("Duplicate timing correction source");
        t.sources[id]={s["originalStart"].toDouble(),s["originalEnd"].toDouble(),s["start"].toDouble(),s["end"].toDouble()};
    }
    validateTimingCorrection(p);
}
}
