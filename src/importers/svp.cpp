// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
#include <cmath>
#include <algorithm>
namespace chosuta {
    QJsonDocument checkedJson(QByteArray bytes,bool allowNul) {
        if(bytes.size()>64*1024*1024)throw Failure("Input exceeds 64 MiB");
        if(allowNul&&bytes.endsWith('\0'))bytes.chop(1);
        if(bytes.contains('\0'))throw Failure("Embedded NUL or multiple JSON segments are unsupported");
        int depth=0;
        bool quoted=false,escape=false;
        for(char c:bytes) {
            if(quoted) {
                if(escape)escape=false;
                else if(c=='\\')escape=true;
                else if(c=='"')quoted=false;
            }
            else if(c=='"')quoted=true;
            else if(c=='{'||c=='[') {
                if(++depth>64)throw Failure("JSON nesting exceeds 64");
            }
            else if(c=='}'||c==']')--depth;
        }
        QJsonParseError error;
        auto doc=QJsonDocument::fromJson(bytes,&error);
        if(error.error!=QJsonParseError::NoError||!doc.isObject())throw Failure(QString("Invalid JSON at %1: %2").arg(error.offset).arg(error.errorString()));
        return doc;
    }
    static qint64 integer(const QJsonObject&o,const QString&k,qint64 def=0) {
        if(!o.contains(k))return def;
        const auto v=o[k];
        double d=v.toDouble(std::numeric_limits<double>::quiet_NaN());
        if(!std::isfinite(d)||std::floor(d)!=d||std::abs(d)>MusicLimit)throw Failure("Invalid integer: "+k);
        return v.toInteger();
    }
    static QString lang(const QString&s) {
        if(s=="english"||s=="en")return "en";
        if(s=="mandarin"||s=="chinese"||s=="zh")return "zh";
        if(s=="japanese"||s=="ja")return "ja";
        return s.isEmpty()?"auto":s;
    }
    Score importSvp(const QString&path,const std::atomic_bool*cancel,Progress progress) {
        QFile f(path);
        if(!f.open(QIODevice::ReadOnly))throw Failure(f.errorString());
        if(f.size()>64*1024*1024)throw Failure("Input exceeds 64 MiB");
        return parseSvp(f.readAll(),path,cancel,progress);
    }
    Score parseSvp(QByteArray bytes,const QString&path,const std::atomic_bool*cancel,Progress progress) {
        Score s;
        s.raw=bytes;
        s.sourcePath=path;
        s.hash=QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());
        auto root=checkedJson(bytes,true).object();
        if(!root["tracks"].isArray()||!root["time"].isObject())throw Failure("Not an SVP project: time/tracks required");
        s.version=int(integer(root,"version"));
        if(s.version!=196)s.diagnostics.append( {
            "$.version","version","Unverified source version; parsed conservatively"
        });
        auto time=root["time"].toObject();
        s.sourceOrigin=time["startTimeSeconds"].toDouble();
        if(!std::isfinite(s.sourceOrigin))throw Failure("Invalid source origin");
        s.time.tempos.clear();
        for(const auto&v:time["tempo"].toArray()) {
            auto o=v.toObject();
            s.time.tempos.append( {
                integer(o,"position"),o["bpm"].toDouble(0)
            });
        }
        s.time.meters.clear();
        for(const auto&v:time["meter"].toArray()) {
            auto o=v.toObject();
            auto b=integer(o,"index");
            auto n=integer(o,"numerator",4);
            auto d=integer(o,"denominator",4);
            if(b<0||b>10000000||n<1||n>64||d<1||d>64)throw Failure("Invalid meter");
            s.time.meters.append( {
                int(b),int(n),int(d)
            });
        }
        s.time.validate();
        QMap<QString,QJsonObject>library;
        for(const auto&v:root["library"].toArray()) {
            auto o=v.toObject();
            auto id=o["uuid"].toString();
            if(id.isEmpty()||library.contains(id))throw Failure("Missing or duplicate library UUID");
            library[id]=o;
        }
        auto tracks=root["tracks"].toArray();
        if(tracks.size()>1024)throw Failure("Too many tracks");
        int total=0;
        for(int ti=0;ti<tracks.size();++ti) {
            if(cancel&&cancel->load())throw Failure("Cancelled");
            auto o=tracks[ti].toObject();
            Track t;
            t.id=o["uuid"].toString("track")+QString("@%1").arg(ti);
            t.name=o["name"].toString(QString("Track %1").arg(ti+1));
            t.muted=o["mixer"].toObject()["mute"].toBool();
            auto addGroup=[&](const QJsonObject&g,const QJsonObject&r,const QString&locator,int ri) {
                if(r["isInstrumental"].toBool()) {
                    s.diagnostics.append( {
                        locator,"instrumental","Instrumental reference does not generate singing events"
                    });
                    return;
                }
                auto notes=g["notes"].toArray();
                qint64 offset=integer(r,"blickOffset"),pitchOffset=integer(r,"pitchOffset");
                if(std::abs(pitchOffset)>127)throw Failure("Invalid pitch offset");
                auto db=r["database"].toObject();
                const auto trackDb=o["mainRef"].toObject()["database"].toObject();
                // A reference without its own voice inherits the track voice.
                auto inherited=[&](const QString&key) {
                    auto value=db[key].toString();
                    if(value.isEmpty()&&db["name"].toString().isEmpty())value=trackDb[key].toString();
                    return value;
                };
                QString languageName=inherited("languageOverride");
                if(languageName.isEmpty())languageName=inherited("language");
                QString language=lang(languageName);
                QString phoneset=inherited("phonesetOverride");
                if(phoneset.isEmpty())phoneset=inherited("phoneset");
                const QString instance=t.id+"/"+r["uuid"].toString("ref")+QString("@%1/").arg(ri)+g["uuid"].toString("group");
                for(int ni=0;ni<notes.size();++ni) {
                    if(cancel&&cancel->load())throw Failure("Cancelled");
                    if(++total>200000)throw Failure("More than 200000 referenced notes");
                    auto n=notes[ni].toObject();
                    Note note;
                    QString p=locator+QString(".notes[%1]").arg(ni);
                    qint64 onset=integer(n,"onset"),duration=integer(n,"duration"),pitch=integer(n,"pitch",60);
                    if(std::abs(onset+offset)>MusicLimit||duration>MusicLimit||std::abs(onset+offset+duration)>MusicLimit)throw Failure(p+": music position overflow");
                    if(duration<=0) {
                        s.diagnostics.append( {
                            p,"duration","Nonpositive note duration skipped"
                        });
                        continue;
                    }
                    note.groupInstance=instance;
                    note.id=instance+"/"+n["uuid"].toString("note")+QString("@%1").arg(ni);
                    note.onset=onset+offset;
                    note.duration=duration;
                    if(pitch+pitchOffset<0||pitch+pitchOffset>127)throw Failure(p+": invalid MIDI pitch");
                    note.pitch=int(pitch+pitchOffset);
                    note.lyrics=n["lyrics"].toString();
                    note.phonemes=n["phonemes"].toString();
                    auto attributes=n["attributes"].toObject();
                    QString overrideLanguage=attributes["languageOverride"].toString();
                    if(overrideLanguage.isEmpty())overrideLanguage=n["languageOverride"].toString();
                    note.language=overrideLanguage.isEmpty()?language:lang(overrideLanguage);
                    note.phoneset=attributes["phonesetOverride"].toString();
                    if(note.phoneset.isEmpty())note.phoneset=n["phonesetOverride"].toString();
                    if(note.phoneset.isEmpty())note.phoneset=overrideLanguage.isEmpty()?phoneset:QString();
                    if(!QStringList{"auto","ja","zh","en"}.contains(note.language))s.diagnostics.append({
                        p,"language","Unsupported singing language preserved: "+note.language
                    });
                    note.original=n;
                    note.muted=t.muted||r["mute"].toBool()||n["attributes"].toObject()["muted"].toBool();
                    auto attrs=n["attributes"].toObject()["phonemes"].toArray();
                    for(int ai=0;ai<attrs.size();++ai)if(attrs[ai].toObject().contains("leftOffset"))s.diagnostics.append( {
                        p+QString(".attributes.phonemes[%1].leftOffset").arg(ai),"leftOffset","Preserved raw value; unknown unit/baseline, not applied"
                    });
                    t.notes.append(note);
                }
            };
            if(o["mainGroup"].isObject())addGroup(o["mainGroup"].toObject(),o["mainRef"].toObject(),QString("$.tracks[%1].mainGroup").arg(ti),-1);
            auto refs=o["groups"].toArray();
            for(int ri=0;ri<refs.size();++ri) {
                auto r=refs[ri].toObject();
                QString key=r["groupID"].toString(),p=QString("$.tracks[%1].groups[%2]").arg(ti).arg(ri);
                if(!library.contains(key)) {
                    s.diagnostics.append( {
                        p,"missing-group","Missing library reference: "+key
                    });
                    continue;
                }
                addGroup(library[key],r,p+" -> library["+key+"]",ri);
            }
            std::stable_sort(t.notes.begin(),t.notes.end(),[](const auto&a,const auto&b) {
                return a.onset==b.onset?a.id<b.id:a.onset<b.onset;
            });
            s.tracks.append(t);
            if(progress)progress((ti+1)*100/std::max(1,int(tracks.size())));
        }
        return s;
    }
}
