// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include "zip.h"
#include <algorithm>
#include <cmath>
namespace chosuta {
namespace {
[[noreturn]] void invalid(const QString &path){throw Failure(Message::format("VPR %1: invalid or unsupported field",{path}));}
QJsonObject object(const QJsonValue &v,const QString &path){if(!v.isObject())invalid(path);return v.toObject();}
QJsonArray array(const QJsonValue &v,const QString &path){if(!v.isArray())invalid(path);return v.toArray();}
qint64 integer(const QJsonValue &v,const QString &path,qint64 low=0,qint64 high=9007199254740991LL){
    const double n=v.toDouble(-1);if(!v.isDouble()||!std::isfinite(n)||n!=std::floor(n)||n<low||n>high)invalid(path);return qint64(n);
}
bool boolean(const QJsonObject &o,const char *key,const QString &path){const auto v=o.value(QLatin1String(key));if(v.isUndefined())return false;if(!v.isBool())invalid(path+"."+key);return v.toBool();}
QString language(int id){return id==0?"ja":id==1?"en":id==4?"zh":QString("vocaloid:%1").arg(id);}
}
Score parseVpr(const QByteArray &bytes,const QString &,const std::atomic_bool *cancel,Progress progress){
    auto check=[&]{if(cancel&&cancel->load())throw Failure("Cancelled");};check();
    ZipArchive archive(bytes,cancel);if(progress)progress(5);
    // Both slash styles occur in VPR. ZipArchive normalizes separators and
    // rejects ambiguous duplicates; no media member is decompressed.
    const auto json=archive.read("Project/sequence.json",64*1024*1024,[&](int n){if(progress)progress(5+n*15/100);});
    const auto document=checkedJson(json);const auto root=object(document.isObject()?QJsonValue(document.object()):QJsonValue(),"$");
    const auto version=object(root["version"],"$.version");
    const int major=int(integer(version["major"],"$.version.major",5,6));
    const auto minor=integer(version["minor"],"$.version.minor"),revision=integer(version["revision"],"$.version.revision");
    Score score;score.version=major;score.sourceResolution=480;score.sourceVersion=QString("%1.%2.%3").arg(major).arg(minor).arg(revision);
    if(score.sourceVersion!="5.0.0")score.diagnostics.append({"$.version","unverified-version",Message::format("VPR version not independently verified: %1",{score.sourceVersion})});
    if(root.contains("resolution")&&integer(root["resolution"],"$.resolution",1)!=480)invalid("$.resolution (VPR uses 480 ticks per quarter)");
    const auto master=object(root["masterTrack"],"$.masterTrack");
    const auto tempo=master["tempo"].isUndefined()?QJsonObject{}:object(master["tempo"],"$.masterTrack.tempo");
    // The relation between global mode and DAW time is not stored as a fully
    // verified standalone tempo map. Do not silently substitute its scalar.
    if(boolean(tempo["global"].toObject(),"isEnabled","$.masterTrack.tempo.global"))invalid("$.masterTrack.tempo.global.isEnabled (global/DAW time)");
    const auto tempos=tempo["events"].isUndefined()?QJsonArray{}:array(tempo["events"],"$.masterTrack.tempo.events");
    if(tempos.size()>10000)invalid("$.masterTrack.tempo.events");
    QMap<qint64,double> tempoMap;
    for(int i=0;i<tempos.size();++i){check();const auto path=QString("$.masterTrack.tempo.events[%1]").arg(i);const auto t=object(tempos[i],path);const auto pos=integer(t["pos"],path+".pos");if(tempoMap.contains(pos))invalid(path+".pos");tempoMap[pos]=integer(t["value"],path+".value",1,1000000)/100.;}
    if(!tempoMap.contains(0)){tempoMap[0]=120;score.diagnostics.append({"$.masterTrack.tempo","default-tempo","VPR has no initial tempo; using 120 BPM"});}
    score.time.tempos.clear();for(auto it=tempoMap.cbegin();it!=tempoMap.cend();++it)score.time.tempos.append({sourceBlick(it.key(),480),it.value()});
    const auto signatures=master["timeSig"].isUndefined()?QJsonObject{}:object(master["timeSig"],"$.masterTrack.timeSig");
    const auto meters=signatures["events"].isUndefined()?QJsonArray{}:array(signatures["events"],"$.masterTrack.timeSig.events");
    if(meters.size()>10000)invalid("$.masterTrack.timeSig.events");
    QMap<int,Meter> meterMap;
    for(int i=0;i<meters.size();++i){check();const auto path=QString("$.masterTrack.timeSig.events[%1]").arg(i);const auto m=object(meters[i],path);Meter meter;meter.bar=int(integer(m["bar"],path+".bar",0,10000000));meter.numerator=int(integer(m["numer"],path+".numer",1,64));meter.denominator=int(integer(m["denom"],path+".denom",1,64));if(meterMap.contains(meter.bar))invalid(path+".bar");meterMap[meter.bar]=meter;}
    if(!meterMap.contains(0)){meterMap[0]={0,4,4};score.diagnostics.append({"$.masterTrack.timeSig","default-meter","VPR has no initial meter; using 4/4"});}score.time.meters=meterMap.values();
    const auto tracks=array(root["tracks"],"$.tracks");bool anySolo=false;
    for(int i=0;i<tracks.size();++i){check();const auto t=object(tracks[i],QString("$.tracks[%1]").arg(i));anySolo=boolean(t,"isSoloMode",QString("$.tracks[%1]").arg(i))||anySolo;}
    int count=0;bool omitted=false,clipped=false;
    for(int ti=0;ti<tracks.size();++ti){
        check();const auto path=QString("$.tracks[%1]").arg(ti);const auto raw=tracks[ti].toObject();const auto type=integer(raw["type"],path+".type");const auto parts=raw["parts"].isUndefined()?QJsonArray{}:array(raw["parts"],path+".parts");
        if(type!=0){
            for(const auto &part:parts)if(!part.toObject()["notes"].toArray().isEmpty())invalid(path+".type (unsupported singing track)");
            omitted=true;continue;
        }
        Track track;track.id=QString("vpr/track:%1").arg(ti);if(raw.contains("name")&&!raw["name"].isString())invalid(path+".name");track.name=raw["name"].toString(track.id);
        track.muted=boolean(raw,"isMuted",path)||(anySolo&&!boolean(raw,"isSoloMode",path));
        for(int pi=0;pi<parts.size();++pi){
            check();const auto pp=path+QString(".parts[%1]").arg(pi);const auto part=object(parts[pi],pp);
            const auto offset=integer(part["pos"],pp+".pos"),length=integer(part["duration"],pp+".duration");
            score.endPosition=std::max(score.endPosition,sourceBlick(offset+length,480));
            const auto notes=part["notes"].isUndefined()?QJsonArray{}:array(part["notes"],pp+".notes");
            const auto voice=part["voice"].toObject();const auto lang=voice.contains("langID")?language(int(integer(voice["langID"],pp+".voice.langID",0,255))):QString("auto");
            const QString instance=track.id+QString("/part:%1").arg(pi);
            for(int ni=0;ni<notes.size();++ni){
                check();if(progress)progress(20+(ti*75+(pi*75+ni*75/std::max(1,int(notes.size())))/std::max(1,int(parts.size())))/std::max(1,int(tracks.size())));
                if(++count>200000)throw Failure("More than 200000 source notes");
                const auto np=pp+QString(".notes[%1]").arg(ni);const auto n=object(notes[ni],np);
                Note note;note.id=instance+QString("/note:%1").arg(ni);note.groupInstance=instance;
                const auto local=integer(n["pos"],np+".pos");note.sourceOnset=offset+local;note.sourceDuration=integer(n["duration"],np+".duration",1);note.pitch=int(integer(n["number"],np+".number",0,127));
                if(n.contains("lyric")&&!n["lyric"].isNull()&&!n["lyric"].isString())invalid(np+".lyric");
                note.lyrics=n["lyric"].toString();
                if(n.contains("phoneme")&&!n["phoneme"].isNull()&&!n["phoneme"].isString())invalid(np+".phoneme");
                note.phonemes=n["phoneme"].toString();
                const auto duration=std::min(note.sourceDuration,std::max<qint64>(0,length-local));
                note.original=n;note.original["partPosition"]=offset;note.original["partDuration"]=length;note.original["localPosition"]=local;
                if(duration!=note.sourceDuration)clipped=true;
                if(duration<=0)continue;
                note.onset=sourceBlick(note.sourceOnset,480);note.duration=sourceBlick(note.sourceOnset+duration,480)-note.onset;note.language=lang;note.phoneset="vocaloid-xsampa";note.muted=track.muted;
                score.endPosition=std::max(score.endPosition,note.onset+note.duration);track.notes.append(std::move(note));
            }
        }
        std::stable_sort(track.notes.begin(),track.notes.end(),[](const auto &a,const auto &b){return a.onset<b.onset;});score.tracks.append(std::move(track));
    }
    if(score.tracks.isEmpty())throw Failure("VPR has no supported vocal tracks");
    if(clipped)score.diagnostics.append({"$.tracks.parts","part-boundary","VPR notes outside part boundaries are clipped or omitted; original notes are retained"});
    if(omitted||archive.names().size()>1)score.diagnostics.append({"ZIP","retained-media","VPR audio and other archive members are retained in the source; attach audio separately"});
    score.diagnostics.append({"$.tracks","score-timing","VPR timing uses notes and explicit phonemes; synthesis effects and rendered phoneme boundaries are not reproduced"});
    if(bytes.size()>48*1024*1024)score.diagnostics.append({"ZIP","project-size","The retained VPR archive may exceed the existing 64 MiB project save limit"});
    check();return score;
}
}
