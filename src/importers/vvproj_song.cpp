// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include <algorithm>
#include <cmath>
namespace chosuta {
namespace {
[[noreturn]] void invalid(const QString &path){throw Failure(Message::format("VVPROJ song %1: invalid or missing field",{path}));}
QJsonObject object(const QJsonValue &v,const QString &path){if(!v.isObject())invalid(path);return v.toObject();}
QJsonArray array(const QJsonValue &v,const QString &path){if(!v.isArray())invalid(path);return v.toArray();}
qint64 integer(const QJsonValue &v,const QString &path,qint64 low=0,qint64 high=9007199254740991LL){
    const double x=v.toDouble(-1);if(!v.isDouble()||!std::isfinite(x)||std::floor(x)!=x||x<low||x>high)invalid(path);return qint64(x);
}
QString text(const QJsonValue &v,const QString &path){if(!v.isString())invalid(path);return v.toString();}
bool boolean(const QJsonValue &v,const QString &path){if(!v.isBool())invalid(path);return v.toBool();}
}
Score parseVvprojSong(const QByteArray &bytes,const QString &,const std::atomic_bool *cancel,Progress report){
    auto check=[&]{if(cancel&&cancel->load())throw Failure("Cancelled");};check();
    const auto document=checkedJson(bytes);const auto root=object(document.isObject()?QJsonValue(document.object()):QJsonValue(),"$");
    const auto song=object(root["song"],"$.song");Score score;score.sourceFormat="vvproj-song";
    score.sourceVersion=text(root["appVersion"],"$.appVersion");
    if(score.sourceVersion!="0.25.1")score.diagnostics.append({"$.appVersion","unverified-version",Message::format("VVPROJ app version not independently verified: %1",{score.sourceVersion})});
    score.sourceResolution=integer(song["tpqn"],"$.song.tpqn",1);
    score.time.tempos.clear();QSet<qint64> positions;
    const auto tempos=array(song["tempos"],"$.song.tempos");if(tempos.isEmpty()||tempos.size()>10000)invalid("$.song.tempos");
    for(int i=0;i<tempos.size();++i){
        check();const auto path=QString("$.song.tempos[%1]").arg(i);const auto t=object(tempos[i],path);
        const auto pos=integer(t["position"],path+".position");const double bpm=t["bpm"].toDouble(-1);
        if(!t["bpm"].isDouble()||!std::isfinite(bpm)||bpm<=0||bpm>10000||positions.contains(pos))invalid(path);
        positions.insert(pos);score.time.tempos.append({sourceBlick(pos,score.sourceResolution),bpm});
    }
    score.time.meters.clear();QSet<int> bars;
    const auto meters=array(song["timeSignatures"],"$.song.timeSignatures");if(meters.isEmpty()||meters.size()>10000)invalid("$.song.timeSignatures");
    for(int i=0;i<meters.size();++i){
        check();const auto path=QString("$.song.timeSignatures[%1]").arg(i);const auto m=object(meters[i],path);
        const int bar=int(integer(m["measureNumber"],path+".measureNumber",1,10000001))-1;
        const int n=int(integer(m["beats"],path+".beats",1,64)),d=int(integer(m["beatType"],path+".beatType",1,64));
        if((d&(d-1))||bars.contains(bar))invalid(path);
        bars.insert(bar);score.time.meters.append({bar,n,d});
    }
    const auto tracks=object(song["tracks"],"$.song.tracks");const auto order=array(song["trackOrder"],"$.song.trackOrder");
    QSet<QString> seen;bool anySolo=false;
    for(const auto &v:order){check();const auto id=text(v,"$.song.trackOrder");if(id.isEmpty()||seen.contains(id)||!tracks[id].isObject())invalid("$.song.trackOrder");seen.insert(id);anySolo=boolean(tracks[id].toObject()["solo"],"$.song.tracks["+id+"].solo")||anySolo;}
    if(seen.size()!=tracks.size()||order.isEmpty())invalid("$.song.trackOrder");
    int count=0;bool expressions=false,overlap=false;
    for(int ti=0;ti<order.size();++ti){
        check();const auto id=order[ti].toString(),path="$.song.tracks["+id+"]";const auto raw=tracks[id].toObject();
        Track track;track.id="vvproj/song/track:"+QString::fromLatin1(QUrl::toPercentEncoding(id));track.name=text(raw["name"],path+".name");
        const bool solo=boolean(raw["solo"],path+".solo"),mute=boolean(raw["mute"],path+".mute");
        // VOICEVOX solo takes precedence over mute, including on the solo track.
        track.muted=anySolo?!solo:mute;
        const auto notes=array(raw["notes"],path+".notes");QSet<QString> noteIds;
        for(int ni=0;ni<notes.size();++ni){
            check();if(report)report((ti*90+ni*90/std::max(1,int(notes.size())))/order.size());
            if(++count>200000)throw Failure("More than 200000 source notes");
            const auto np=path+QString(".notes[%1]").arg(ni);const auto rawNote=object(notes[ni],np);
            const auto noteId=text(rawNote["id"],np+".id");if(noteId.isEmpty()||noteIds.contains(noteId))invalid(np+".id");noteIds.insert(noteId);
            Note note;note.groupInstance=track.id;note.id=track.id+"/note:"+QString::fromLatin1(QUrl::toPercentEncoding(noteId));
            note.sourceOnset=integer(rawNote["position"],np+".position");note.sourceDuration=integer(rawNote["duration"],np+".duration",1);
            note.onset=sourceBlick(note.sourceOnset,score.sourceResolution);note.duration=sourceBlick(note.sourceOnset+note.sourceDuration,score.sourceResolution)-note.onset;
            note.pitch=int(integer(rawNote["noteNumber"],np+".noteNumber",0,127));
            note.lyrics=rawNote["lyric"].isNull()?QString():text(rawNote["lyric"],np+".lyric");note.language="ja";note.muted=track.muted;note.original=rawNote;
            // keyRangeAdjustment changes the synthesis guide, not the score pitch.
            // Relative phoneme edits require an engine prediction absent here.
            score.endPosition=std::max(score.endPosition,note.onset+note.duration);track.notes.append(std::move(note));
        }
        std::stable_sort(track.notes.begin(),track.notes.end(),[](const auto &a,const auto &b){return a.onset<b.onset;});
        qint64 end=0;for(const auto &n:track.notes){overlap=overlap||n.onset<end;end=std::max(end,n.onset+n.duration);}
        expressions=expressions||!raw["pitchEditData"].toArray().isEmpty()||!raw["volumeEditData"].toArray().isEmpty()||!raw["phonemeTimingEditData"].toObject().isEmpty();
        score.tracks.append(std::move(track));
    }
    if(report)report(95);
    score.diagnostics.append({"$.song","score-timing","VVPROJ song uses stored notes and lyrics; phoneme timing is estimated without a singing engine"});
    if(expressions)score.diagnostics.append({"$.song.tracks","synthesis-edits","VVPROJ song pitch, volume and relative phoneme edits are retained in the source; synthesis timing is not reproduced"});
    if(overlap)score.diagnostics.append({"$.song.tracks","overlap","VVPROJ song contains overlapping notes; mouth shapes use the existing track priority rules"});
    if(!root["talk"].toObject()["audioKeys"].toArray().isEmpty())score.diagnostics.append({"$.talk","other-section","VVPROJ talk is retained in the source; only the selected song section is imported"});
    check();return score;
}
Score parseVvprojAuto(const QByteArray &bytes,const QString &path,const std::atomic_bool *cancel,Progress report){
    if(cancel&&cancel->load())throw Failure("Cancelled");
    const auto root=checkedJson(bytes).object();const auto song=root["song"].toObject();bool notes=false;
    for(const auto &v:song["tracks"].toObject()){if(cancel&&cancel->load())throw Failure("Cancelled");notes=notes||!v.toObject()["notes"].toArray().isEmpty();}
    const bool talk=!root["talk"].toObject()["audioKeys"].toArray().isEmpty();
    if(notes&&talk)throw Failure("VVPROJ contains both talk and song; choose a section to import");
    if(notes||(!talk&&root["song"].isObject()))return parseVvprojSong(bytes,path,cancel,report);
    return parseVvproj(bytes,path,cancel,report);
}
}
