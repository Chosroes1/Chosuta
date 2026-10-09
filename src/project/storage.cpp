// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
#include "core/appearance.h"
#include <cmath>
#include <initializer_list>
namespace chosuta {
    static void requireFields(const QJsonObject &object,std::initializer_list<const char *> keys) {
        for(const auto key:keys)if(!object.contains(key))throw Failure("Missing project field: "+QString::fromLatin1(key));
    }
    static QJsonObject eventJson(const Event&e) {
        return {
            {
                "id",e.id
            }, {
                "source",e.source
            }, {
                "track",e.track
            }, {
                "shape",e.shape
            }, {
                "start",e.start
            }, {
                "end",e.end
            }, {
                "provenance",e.provenance
            }, {
                "text",e.text
            }, {
                "phone",e.phone
            }, {
                "articulation",e.articulation
            }, {
                "appearanceFixed",e.appearanceFixed
            }, {
                "unknown",e.unknown
            }
        };
    }
    static Event eventRead(const QJsonObject&o) {
        requireFields(o,{"id","source","track","shape","start","end","provenance","text","phone","articulation","appearanceFixed","unknown"});
        Event e;
        e.id=o["id"].toString();
        e.source=o["source"].toString();
        e.track=o["track"].toString();
        e.shape=o["shape"].toString();
        e.provenance=o["provenance"].toString();
        e.text=o["text"].toString();
        e.start=o["start"].toDouble();
        e.end=o["end"].toDouble();
        e.unknown=o["unknown"].toBool();
        for(const auto &key:{"phone","articulation"})if(!o[key].isString())throw Failure("Invalid event pronunciation metadata");
        if(!o["appearanceFixed"].isBool())throw Failure("Invalid event appearance flag");
        e.phone=o["phone"].toString();e.articulation=o["articulation"].toString();
        e.appearanceFixed=o["appearanceFixed"].toBool();
        if(e.phone.size()>128||!QStringList{"","consonant","vowel","cl","br","rest","nasal","extend","empty","unknown"}.contains(e.articulation))throw Failure("Invalid event articulation");
        if(e.id.isEmpty()||e.shape.isEmpty()||!std::isfinite(e.start)||!std::isfinite(e.end)||e.end<=e.start||std::abs(e.start)>864000||std::abs(e.end)>864000)throw Failure("Invalid saved event");
        return e;
    }
    static QString relative(const QDir&base,const QString&path) {
        return path.isEmpty()?QString{}:base.relativeFilePath(QFileInfo(path).absoluteFilePath());
    }
    static QString absolute(const QDir&base,const QJsonValue&v) {
        auto path=v.toString();
        return path.isEmpty()?QString{}:QDir::cleanPath(base.absoluteFilePath(path));
    }
    void saveProject(const Project&p,const QString&path) {
        if(!std::isfinite(p.rules.consonantRatio)||p.rules.consonantRatio<0||p.rules.consonantRatio>.8||!std::isfinite(p.rules.consonantMaxSeconds)||p.rules.consonantMaxSeconds<0||p.rules.consonantMaxSeconds>1)throw Failure("Invalid consonant timing settings");
        validatePronunciationOptions(p.rules.pronunciation);
        validateConsonants(p.rules.consonants);validateAppearance(p.appearance);
        validateSubtitles(p);
        QDir base=QFileInfo(path).absoluteDir();
        QJsonObject root {
            {
                "format","Chosuta"
            }, {
                "schema",9
            }, {
                "sourceBase64",QString::fromLatin1(p.score.raw.toBase64())
            }, {
                "sourcePath",relative(base,p.score.sourcePath)
            }, {
                "sourceHash",p.score.hash
            }
        };
        QJsonArray tracks;
        for(const auto&id:p.selected)tracks.append(id);
        root["selected"]=tracks;
        QJsonObject priorities;
        for(auto it=p.priorities.begin();it!=p.priorities.end();++it)priorities[it.key()]=it.value();
        root["priorities"]=priorities;
        QJsonObject rules {
            {
                "language",p.rules.language
            }, {
                "harmonyTakeover",p.rules.harmonyTakeover
            }, {
                "consonantRatio",p.rules.consonantRatio
            }, {
                "consonantMaxSeconds",p.rules.consonantMaxSeconds
            }, {
                "englishDictionary",relative(base,p.rules.englishDictionary)
            }
        };
        QJsonObject special;
        for(auto it=p.rules.special.begin();it!=p.rules.special.end();++it)special[it.key()]=QJsonObject {
            {
                "mode",it.value().mode
            }, {
                "shape",it.value().shape
            }, {
                "holdSeconds",it.value().holdSeconds
            }
        };
        rules["consonants"]=consonantsJson(p.rules.consonants);
        root["appearance"]=appearanceJson(p.appearance);
        rules["pronunciation"]=pronunciationOptionsJson(p.rules.pronunciation);
        rules["special"]=special;
        QJsonObject readings;
        for(auto it=p.rules.readings.begin();it!=p.rules.readings.end();++it)readings[it.key()]=it.value();
        rules["readings"]=readings;
        root["rules"]=rules;
        QJsonArray generated;
        for(const auto&e:p.generated)generated.append(eventJson(e));
        root["generated"]=generated;
        QJsonArray overrides;
        for(auto it=p.overrides.begin();it!=p.overrides.end();++it) {
            const auto&o=it.value();
            overrides.append(QJsonObject {
                {
                    "key",it.key()
                }, {
                    "event",eventJson(o.event)
                }, {
                    "locked",o.locked
                }, {
                    "deleted",o.deleted
                }, {
                    "standalone",o.standalone
                }, {
                    "parentIds",QJsonArray::fromStringList(o.parentIds)
                }, {
                    "anchor",o.anchor
                }, {
                    "startBlick",QString::number(o.startBlick)
                }, {
                    "endBlick",QString::number(o.endBlick)
                }
            });
        }
        root["overrides"]=overrides;
        QJsonObject assets;
        for(auto it=p.assets.begin();it!=p.assets.end();++it)assets[it.key()]=relative(base,it.value());
        root["assets"]=assets;
        root["fallback"]=p.fallback;
        root["audioPath"]=relative(base,p.audioPath);
        const auto &c=p.canvas;
        root["canvas"]=QJsonObject{{"width",c.width},{"height",c.height},{"background",c.background.name(QColor::HexArgb)},{"transparent",c.transparent},
            {"backgroundImage",relative(base,c.backgroundImage)},{"backgroundFit",c.backgroundFit},{"characterScale",c.characterScale},{"characterX",c.characterX},{"characterY",c.characterY}};
        root["audioDuration"]=p.audioDuration;
        root["playbackReturnPosition"]=p.playbackReturnPosition;
        const auto&s=p.output;
        root["output"]=QJsonObject{{"fpsNum",s.fpsNum},{"fpsDen",s.fpsDen},{"format",s.format},{"crf",s.crf},{"bitrateKbps",s.bitrateKbps},{"ffmpeg",s.ffmpeg},{"duration",s.duration},{"syncOffset",s.syncOffset},{"audioOffset",s.audioOffset}};
        root["subtitles"]=subtitlesJson(p);
        root["timingCorrection"]=timingCorrectionJson(p);
        auto bytes=QJsonDocument(root).toJson();
        if(bytes.size()>64*1024*1024)throw Failure("Saved project exceeds 64 MiB");
        QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())throw Failure(file.errorString());
    }
    Project loadProject(const QString&path) {
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)||file.size()>64*1024*1024)throw Failure("Cannot read project (limit 64 MiB): "+file.errorString());
        auto root=checkedJson(file.readAll()).object();
        if(root["format"]!="Chosuta"||root["schema"]!=9)throw Failure("Unsupported project format/schema");
        requireFields(root,{"sourceBase64","sourcePath","sourceHash","selected","priorities","rules","appearance","generated","overrides","assets","fallback","audioPath","canvas","audioDuration","playbackReturnPosition","output","subtitles","timingCorrection"});
        auto decoded=QByteArray::fromBase64Encoding(root["sourceBase64"].toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
        if(!decoded)throw Failure("Invalid embedded source");
        Project p;
        QDir base=QFileInfo(path).absoluteDir();
        p.score=parseSvp(decoded.decoded,absolute(base,root["sourcePath"]));
        if(p.score.hash!=root["sourceHash"].toString())throw Failure("Embedded source checksum mismatch");
        for(const auto&v:root["selected"].toArray()) {
            auto id=v.toString();
            bool exists=false;
            for(const auto&t:p.score.tracks)if(t.id==id)exists=true;
            if(!exists)throw Failure("Unknown selected track");
            if(!p.selected.contains(id))p.selected.append(id);
        }
        auto priorities=root["priorities"].toObject();
        for(auto it=priorities.begin();it!=priorities.end();++it)p.priorities[it.key()]=it.value().toInt();
        auto rules=root["rules"].toObject();
        requireFields(rules,{"language","harmonyTakeover","consonantRatio","consonantMaxSeconds","englishDictionary","special","readings","consonants","pronunciation"});
        p.rules.language=rules["language"].toString();
        if(!QStringList {
            "auto","ja","zh","en"
        }
        .contains(p.rules.language))throw Failure("Invalid language");
        p.rules.harmonyTakeover=rules["harmonyTakeover"].toBool();
        p.rules.consonantRatio=rules["consonantRatio"].toDouble();
        if(!std::isfinite(p.rules.consonantRatio)||p.rules.consonantRatio<0||p.rules.consonantRatio>.8)throw Failure("Invalid consonant ratio");
        const auto cap=rules.value("consonantMaxSeconds");
        if(!cap.isDouble())throw Failure("Invalid consonant duration limit");
        p.rules.consonantMaxSeconds=cap.toDouble();
        if(!std::isfinite(p.rules.consonantMaxSeconds)||p.rules.consonantMaxSeconds<0||p.rules.consonantMaxSeconds>1)throw Failure("Invalid consonant duration limit");
        p.rules.consonants=readConsonants(rules.value("consonants"));
        p.appearance=readAppearance(root.value("appearance"));
        if(!rules["pronunciation"].isObject())throw Failure("Missing pronunciation settings");
        p.rules.englishDictionary=absolute(base,rules["englishDictionary"]);
        p.rules.pronunciation=pronunciationOptionsRead(rules.value("pronunciation"));
        auto special=rules["special"].toObject();
        for(auto it=special.begin();it!=special.end();++it) {
            auto v=it.value().toObject();
            Policy policy {
                v["mode"].toString(),v["shape"].toString(),v["holdSeconds"].toDouble()
            };
            if(!QStringList {
                "shape","hold","timed"
            }
            .contains(policy.mode)||policy.shape.isEmpty()||!std::isfinite(policy.holdSeconds)||policy.holdSeconds<0||policy.holdSeconds>60)throw Failure("Invalid special policy");
            p.rules.special[it.key()]=policy;
        }
        auto readings=rules["readings"].toObject();
        for(auto it=readings.begin();it!=readings.end();++it)p.rules.readings[it.key()]=it.value().toString();
        auto generated=root["generated"].toArray(),overrides=root["overrides"].toArray();
        if(generated.size()+overrides.size()>500000)throw Failure("Too many saved events");
        QSet<QString>ids;
        for(const auto&v:generated) {
            auto e=eventRead(v.toObject());
            if(ids.contains(e.id))throw Failure("Duplicate event ID");
            ids.insert(e.id);
            p.generated.append(e);
        }
        for(const auto&v:overrides) {
            auto o=v.toObject();
            requireFields(o,{"key","event","locked","deleted","standalone","parentIds","anchor","startBlick","endBlick"});
            Override edit;
            edit.event=eventRead(o["event"].toObject());
            QString key=o["key"].toString();
            if(key!=edit.event.id||p.overrides.contains(key))throw Failure("Duplicate or mismatched override ID");
            edit.locked=o["locked"].toBool();
            edit.deleted=o["deleted"].toBool();
            edit.standalone=o["standalone"].toBool();
            for(const auto&parent:o["parentIds"].toArray())edit.parentIds.append(parent.toString());
            edit.anchor=o["anchor"].toString();
            bool a=false,b=false;
            edit.startBlick=o["startBlick"].toString().toLongLong(&a);
            edit.endBlick=o["endBlick"].toString().toLongLong(&b);
            if(!QStringList {
                "seconds","beats"
            }
            .contains(edit.anchor)||(edit.anchor=="beats"&&(!a||!b||edit.endBlick<=edit.startBlick||edit.startBlick < -MusicLimit || edit.startBlick > MusicLimit || edit.endBlick < -MusicLimit || edit.endBlick > MusicLimit)))throw Failure("Invalid edit anchor");
            p.overrides[key]=edit;
        }
        auto assets=root["assets"].toObject();
        for(auto it=assets.begin();it!=assets.end();++it)p.assets[it.key()]=absolute(base,it.value());
        p.fallback=root["fallback"].toString();
        p.audioPath=absolute(base,root["audioPath"]);
        auto output=root["output"].toObject();
        requireFields(output,{"fpsNum","fpsDen","format","crf","bitrateKbps","ffmpeg","duration","syncOffset","audioOffset"});
        auto&s=p.output;

        s.fpsNum=output["fpsNum"].toInt();
        s.fpsDen=output["fpsDen"].toInt();
        s.format=output["format"].toString();
        s.crf=output["crf"].toInt();
        s.bitrateKbps=output["bitrateKbps"].toInt();

        s.ffmpeg=output["ffmpeg"].toString();
        s.duration=output["duration"].toDouble();
        s.syncOffset=output["syncOffset"].toDouble();
        s.audioOffset=output["audioOffset"].toDouble();
        if(!std::isfinite(s.duration)||s.duration<0||s.duration>21600||!std::isfinite(s.syncOffset)||std::abs(s.syncOffset)>21600||!std::isfinite(s.audioOffset)||std::abs(s.audioOffset)>21600)throw Failure("Invalid output time settings");
        if(!root["canvas"].isObject())throw Failure("Missing canvas settings");
        auto canvas=root["canvas"].toObject();
        requireFields(canvas,{"width","height","background","transparent","backgroundImage","backgroundFit","characterScale","characterX","characterY"});
        auto &c=p.canvas;c.width=canvas["width"].toInt();c.height=canvas["height"].toInt();
        c.background=QColor(canvas["background"].toString());c.transparent=canvas["transparent"].toBool();
        c.backgroundImage=absolute(base,canvas["backgroundImage"]);c.backgroundFit=canvas["backgroundFit"].toString();
        c.characterScale=canvas["characterScale"].toDouble();c.characterX=canvas["characterX"].toDouble();c.characterY=canvas["characterY"].toDouble();
        validateCanvas(c);
        p.audioDuration=root["audioDuration"].toDouble();p.playbackReturnPosition=root["playbackReturnPosition"].toDouble();
        if(!std::isfinite(p.audioDuration)||p.audioDuration<0||p.audioDuration>864000||!std::isfinite(p.playbackReturnPosition)||p.playbackReturnPosition<0||p.playbackReturnPosition>21600)throw Failure("Invalid audio duration/return position");
        readSubtitles(p,root.value("subtitles"));
        readTimingCorrection(p,root.value("timingCorrection"));
        return p;
    }
}
