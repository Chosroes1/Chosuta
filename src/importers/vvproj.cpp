// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include <cmath>
namespace chosuta {
namespace {
// VOICEVOX Engine 0.25 uses 24000 Hz / 256 samples per synthesis frame.
// These are decoder-frame estimates from a stored query, not measured boundaries.
constexpr double FramesPerSecond=93.75;
[[noreturn]] void invalid(const QString &path,const char *reason){
    throw Failure(Message::format(reason,{path}));
}
double length(const QJsonObject &o,const char *key,const QString &path){
    auto v=o.value(QLatin1String(key));
    if(!v.isDouble()||!std::isfinite(v.toDouble())||v.toDouble()<0)invalid(path+"."+key,"VVPROJ %1: expected a nonnegative duration or scale");
    return v.toDouble();
}
QString symbol(const QJsonObject &o,const char *key,const QString &path){
    const auto v=o.value(QLatin1String(key));
    if(!v.isString()||v.toString().isEmpty()||v.toString().size()>128)invalid(path+"."+key,"VVPROJ %1: expected a phoneme symbol");
    return v.toString();
}
qint64 frameCount(double seconds,const QString &path){
    const double value=seconds*FramesPerSecond;
    if(!std::isfinite(value)||value>864000*FramesPerSecond)invalid(path,"VVPROJ %1: time exceeds supported range");
    // Nearest integer, ties to even; independent of the process rounding mode.
    const double lower=std::floor(value),fraction=value-lower;
    const auto n=qint64(lower);
    return n+(fraction>.5||(fraction==.5&&(n%2))?1:0);
}
bool hasSongNotes(const QJsonObject &song){
    for(const auto &v:song["tracks"].toObject())if(!v.toObject()["notes"].toArray().isEmpty())return true;
    for(const auto &v:song["tracks"].toArray())if(!v.toObject()["notes"].toArray().isEmpty())return true;
    return false;
}
}
Score parseVvproj(const QByteArray &bytes,const QString &path,const std::atomic_bool *cancel,Progress report){
    auto check=[&]{if(cancel&&cancel->load())throw Failure("Cancelled");};check();
    const auto root=checkedJson(bytes).object();
    if(!root["talk"].isObject())throw Failure("VVPROJ talk requires a talk section; select song to import the score");
    const auto talk=root["talk"].toObject();
    if(!talk["audioKeys"].isArray()||!talk["audioItems"].isObject())throw Failure("VVPROJ requires talk.audioKeys and talk.audioItems");
    const auto keys=talk["audioKeys"].toArray();const auto items=talk["audioItems"].toObject();
    if(keys.isEmpty())throw Failure("VVPROJ has no talk items; select song to import the score");
    Score score;score.hasMusic=false;score.time.tempos.clear();score.time.meters.clear();score.sourceResolution=0;
    score.sourceVersion=root["appVersion"].toString();
    score.diagnostics.append({"$.talk","query-timing",Message("VVPROJ talk timing is derived from stored queries using VOICEVOX Engine 0.25 frame rules and default interrogative upspeak; other engine or export settings may differ.")});
    if(score.sourceVersion!="0.25.1")score.diagnostics.append({"$.appVersion","unverified-version",Message::format("VVPROJ app version not independently verified: %1",{score.sourceVersion})});
    if(hasSongNotes(root["song"].toObject()))score.diagnostics.append({"$.song","other-section",Message("VVPROJ song is retained in the source; only the selected talk section is imported")});
    Track track;track.id="vvproj/talk";track.name=QFileInfo(path).completeBaseName();if(track.name.isEmpty())track.name="VOICEVOX talk";
    qint64 cursor=0;QSet<QString> seen,engines;
    for(int ki=0;ki<keys.size();++ki){
        check();if(report)report(ki*90/std::max(1,int(keys.size())));
        const auto key=keys[ki].toString();const auto itemPath=QString("$.talk.audioItems[%1]").arg(key);
        if(!keys[ki].isString()||key.isEmpty()||seen.contains(key)||!items[key].isObject())invalid(QString("$.talk.audioKeys[%1]").arg(ki),"VVPROJ %1: missing or duplicate talk item");
        seen.insert(key);const auto item=items[key].toObject();
        if(!item["query"].isObject())invalid(itemPath+".query","VVPROJ %1: complete stored query required; open in VOICEVOX and save after generating the query");
        const auto query=item["query"].toObject();const auto queryPath=itemPath+".query";
        const auto engine=item["voice"].toObject()["engineId"].toString();
        if(engine!="074fc39e-678b-4c13-8916-ffca8d505d1d"&&!engines.contains(engine)){
            engines.insert(engine);score.diagnostics.append({itemPath+".voice.engineId","unverified-engine",Message::format("VVPROJ engine timing is unverified; using VOICEVOX Engine 0.25 rules: %1",{engine})});
        }
        const double speed=length(query,"speedScale",queryPath);
        if(speed==0)invalid(queryPath+".speedScale","VVPROJ %1: speedScale must be positive");
        const double pre=length(query,"prePhonemeLength",queryPath),post=length(query,"postPhonemeLength",queryPath);
        const double pauseScale=query.contains("pauseLengthScale")?length(query,"pauseLengthScale",queryPath):1;
        const bool replacePause=query.contains("pauseLength")&&!query["pauseLength"].isNull();
        const double pauseLength=replacePause?length(query,"pauseLength",queryPath):0;
        if(!query["accentPhrases"].isArray())invalid(queryPath+".accentPhrases","VVPROJ %1: expected accent phrases and mora arrays");
        // Keep all utterances in display order on one talk track, including voices.
        const QString instance=track.id+"/item:"+QString::fromLatin1(QUrl::toPercentEncoding(key));
        auto append=[&](const QString &phone,double rawLength,const QString &suffix,const QString &location,QJsonObject original,QString text={}){
            check();if(track.timedPhones.size()>=200000)throw Failure("More than 200000 source phonemes");
            double adjusted=rawLength;
            if(phone=="pau")adjusted=(replacePause?pauseLength:rawLength)*pauseScale;
            const double scaled=adjusted/speed;const auto frames=frameCount(scaled,location);
            if(cursor+frames>864000*FramesPerSecond)invalid(location,"VVPROJ %1: time exceeds supported range");
            TimedPhone v;v.id=instance+"/"+suffix;v.groupInstance=instance;v.language="ja";v.phoneset="voicevox";v.phone=phone;v.text=text;
            v.start=cursor/FramesPerSecond;cursor+=frames;v.end=cursor/FramesPerSecond;
            original["path"]=location;original["queryLength"]=rawLength;original["scaledLength"]=scaled;original["frames"]=double(frames);original["itemKey"]=key;
            v.original=original;track.timedPhones.append(std::move(v));
        };
        append("sil",pre,"pre",queryPath+".prePhonemeLength",{});
        const auto phrases=query["accentPhrases"].toArray();
        for(int pi=0;pi<phrases.size();++pi){
            check();if(report)report((ki*90+pi*90/std::max(1,int(phrases.size())))/std::max(1,int(keys.size())));
            const auto phrasePath=queryPath+QString(".accentPhrases[%1]").arg(pi);const auto phrase=phrases[pi].toObject();
            if(!phrases[pi].isObject()||!phrase["moras"].isArray())invalid(phrasePath,"VVPROJ %1: expected accent phrases and mora arrays");
            const auto moras=phrase["moras"].toArray();
            auto mora=[&](const QJsonValue &value,const QString &suffix,const QString &location){
                if(!value.isObject())invalid(location,"VVPROJ %1: expected a mora object");
                const auto m=value.toObject();const auto consonant=m["consonant"];
                if(!consonant.isUndefined()&&!consonant.isNull()&&consonant!="")append(symbol(m,"consonant",location),length(m,"consonantLength",location),suffix+"/consonant",location+".consonantLength",m,m["text"].toString());
                append(symbol(m,"vowel",location),length(m,"vowelLength",location),suffix+"/vowel",location+".vowelLength",m,m["text"].toString());
            };
            for(int mi=0;mi<moras.size();++mi)mora(moras[mi],QString("phrase:%1/mora:%2").arg(pi).arg(mi),phrasePath+QString(".moras[%1]").arg(mi));
            if(phrase["isInterrogative"].toBool()&&!moras.isEmpty()){
                const auto last=moras.last().toObject();
                if(!last["pitch"].isDouble()||!std::isfinite(last["pitch"].toDouble()))invalid(phrasePath+".moras.last.pitch","VVPROJ %1: interrogative timing requires the last mora pitch");
                if(last["pitch"].toDouble()>0)append(symbol(last,"vowel",phrasePath),.15,QString("phrase:%1/upspeak").arg(pi),phrasePath+".isInterrogative",{{"derived","default-interrogative-upspeak"}},last["text"].toString());
            }
            if(phrase.contains("pauseMora")&&!phrase["pauseMora"].isNull())mora(phrase["pauseMora"],QString("phrase:%1/pause").arg(pi),phrasePath+".pauseMora");
        }
        append("sil",post,"post",queryPath+".postPhonemeLength",{});
    }
    score.endSeconds=cursor/FramesPerSecond;score.tracks.append(std::move(track));return score;
}
}
