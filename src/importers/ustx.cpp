// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include <yaml.h>
#include <algorithm>
#include <cmath>
namespace chosuta {
namespace {
// LibYAML parses syntax only. Preserve scalar text (including "true"/"null"
// lyrics); convert numeric/boolean values only at their defined USTX fields.
class YamlReader {
    yaml_parser_t parser{};
    yaml_event_t event{};
    bool hasEvent=false;
    const std::atomic_bool *cancel;
    Progress progress;
    int reported=-1;
    qint64 nodes=0;
    struct Anchor {QJsonValue value;qint64 nodes;};
    QHash<QString,Anchor> anchors;
    QString string(const yaml_char_t *s)const{return s?QString::fromUtf8(reinterpret_cast<const char *>(s)):QString();}
    [[noreturn]] void fail(const char *reason)const {
        throw Failure(Message::format("Cannot read USTX YAML at line %1, column %2",{QString::number(parser.mark.line+1),QString::number(parser.mark.column+1)}).withDetail(QString::fromUtf8(reason)));
    }
    void next(){
        if(cancel&&cancel->load())throw Failure("Cancelled");
        if(hasEvent){yaml_event_delete(&event);hasEvent=false;}
        if(!yaml_parser_parse(&parser,&event))fail(parser.problem?parser.problem:"YAML parser failure");
        hasEvent=true;
        const auto total=parser.input.string.end-parser.input.string.start;
        const int percent=int(std::min<qint64>(85,parser.mark.index*85/std::max<qint64>(1,total)));
        if(progress&&percent!=reported){reported=percent;progress(percent);}
    }
    QJsonValue value(int depth=0){
        if(depth>64||nodes>=1000000)fail("YAML nesting or expanded node limit exceeded");
        const auto before=nodes++;
        QString anchor;
        QJsonValue result;
        if(event.type==YAML_SCALAR_EVENT){
            anchor=string(event.data.scalar.anchor);
            result=QString::fromUtf8(reinterpret_cast<const char *>(event.data.scalar.value),event.data.scalar.length);
            next();
        }else if(event.type==YAML_ALIAS_EVENT){
            const auto key=string(event.data.alias.anchor);
            if(!anchors.contains(key))fail("Unknown or recursive YAML alias");
            const auto a=anchors.value(key);nodes+=a.nodes;
            if(nodes>1000000)fail("YAML expanded node limit exceeded");
            result=a.value;next();
        }else if(event.type==YAML_SEQUENCE_START_EVENT){
            anchor=string(event.data.sequence_start.anchor);next();QJsonArray array;
            while(event.type!=YAML_SEQUENCE_END_EVENT)array.append(value(depth+1));
            result=array;next();
        }else if(event.type==YAML_MAPPING_START_EVENT){
            anchor=string(event.data.mapping_start.anchor);next();QJsonObject object;
            while(event.type!=YAML_MAPPING_END_EVENT){
                if(event.type!=YAML_SCALAR_EVENT)fail("USTX mapping keys must be scalars");
                const auto key=value(depth+1).toString();
                if(key=="<<"||object.contains(key))fail("YAML merge keys or duplicate keys are not supported");
                object[key]=value(depth+1);
            }
            result=object;next();
        }else fail("Expected a YAML scalar, sequence or mapping");
        if(!anchor.isEmpty())anchors.insert(anchor,{result,nodes-before});
        return result;
    }
public:
    YamlReader(const QByteArray &bytes,const std::atomic_bool *c,Progress p):cancel(c),progress(p){
        if(!yaml_parser_initialize(&parser))throw Failure("Cannot initialize USTX YAML parser");
        yaml_parser_set_input_string(&parser,reinterpret_cast<const unsigned char *>(bytes.constData()),bytes.size());
    }
    ~YamlReader(){if(hasEvent)yaml_event_delete(&event);yaml_parser_delete(&parser);}
    QJsonObject read(){
        next();if(event.type!=YAML_STREAM_START_EVENT)fail("Expected one YAML document");
        next();if(event.type!=YAML_DOCUMENT_START_EVENT)fail("Expected one YAML document");
        next();const auto root=value();
        if(!root.isObject()||event.type!=YAML_DOCUMENT_END_EVENT)fail("Expected one USTX mapping document");
        next();if(event.type!=YAML_STREAM_END_EVENT)fail("Multiple YAML documents are not supported");
        return root.toObject();
    }
};
[[noreturn]] void invalid(const QString &path){throw Failure(Message::format("USTX %1: invalid or missing field",{path}));}
qint64 integer(const QJsonObject &object,const char *key,const QString &path,qint64 low,qint64 high){
    bool ok=false;const auto v=object[QLatin1String(key)];const auto n=v.toString().toLongLong(&ok);
    if(!v.isString()||!ok||n<low||n>high)invalid(path+"."+key);
    return n;
}
QJsonArray array(const QJsonObject &object,const char *key,const QString &path,bool optional=false){
    const auto v=object[QLatin1String(key)];
    if(optional&&v.isUndefined())return {};
    if(!v.isArray())invalid(path+"."+key);
    return v.toArray();
}
bool boolean(const QJsonObject &object,const char *key,const QString &path){
    if(!object.contains(QLatin1String(key)))return false;
    const auto s=object[QLatin1String(key)].toString().toLower();
    if(s!="true"&&s!="false")invalid(path+"."+key);
    return s=="true";
}
constexpr qint64 TickLimit=MusicLimit/Blick*480;
}
Score parseUstx(const QByteArray &bytes,const QString &,const std::atomic_bool *cancel,Progress progress){
    if(bytes.size()>64*1024*1024)throw Failure("Cannot read project (limit 64 MiB)");
    const auto root=YamlReader(bytes,cancel,progress).read();
    Score score;score.sourceResolution=480;score.sourceVersion=root["ustx_version"].toString();
    qsizetype versionEnd=0;
    if(QVersionNumber::fromString(score.sourceVersion,&versionEnd).normalized()!=QVersionNumber(0,6)||versionEnd!=score.sourceVersion.size())throw Failure(Message::format("Unsupported USTX version: %1",{score.sourceVersion}));
    if(root.contains("resolution")&&integer(root,"resolution","$",1,TickLimit)!=480)invalid("$.resolution");
    const auto tracks=array(root,"tracks","$");
    bool anySolo=false;QVector<bool> solos;
    for(int ti=0;ti<tracks.size();++ti){
        const auto path=QString("$.tracks[%1]").arg(ti);
        if(!tracks[ti].isObject())invalid(path);
        const auto raw=tracks[ti].toObject();Track t;t.id=QString("ustx/track:%1").arg(ti);t.name=raw["track_name"].toString(QString("Track %1").arg(ti+1));
        t.muted=boolean(raw,"mute",path);const auto solo=boolean(raw,"solo",path);solos.append(solo);anySolo|=solo;score.tracks.append(t);
    }
    for(int ti=0;ti<score.tracks.size();++ti)score.tracks[ti].muted=(score.tracks[ti].muted&&!solos[ti])||(anySolo&&!solos[ti]);
    QMap<qint64,double> tempos{{0,120}};
    const auto tempoValues=array(root,"tempos","$");QSet<qint64> tempoPositions;
    for(int i=0;i<tempoValues.size();++i){
        const auto path=QString("$.tempos[%1]").arg(i);const auto o=tempoValues[i].toObject();
        const auto position=integer(o,"position",path,0,TickLimit);bool ok=false;const double bpm=o["bpm"].toString().toDouble(&ok);
        if(!ok||!std::isfinite(bpm)||bpm<=0)invalid(path+".bpm");
        if(tempoPositions.contains(position))invalid(path+".position");
        tempoPositions.insert(position);tempos[position]=bpm;
    }
    score.time.tempos.clear();for(auto it=tempos.cbegin();it!=tempos.cend();++it)score.time.tempos.append({sourceBlick(it.key(),480),it.value()});
    QMap<int,Meter> meters{{0,{0,4,4}}};const auto meterValues=array(root,"time_signatures","$");QSet<int> meterBars;
    for(int i=0;i<meterValues.size();++i){
        const auto path=QString("$.time_signatures[%1]").arg(i);const auto o=meterValues[i].toObject();Meter m;
        m.bar=int(integer(o,"bar_position",path,0,100000000));m.numerator=int(integer(o,"beat_per_bar",path,1,128));m.denominator=int(integer(o,"beat_unit",path,1,128));
        if(meterBars.contains(m.bar))invalid(path+".bar_position");
        meterBars.insert(m.bar);meters[m.bar]=m;
    }
    score.time.meters=meters.values();
    const auto parts=array(root,"voice_parts","$");int noteCount=0;
    static const QRegularExpression vcv("^(?:-|[aiueoAIUEONn])\\s+([\\x{3041}-\\x{3096}\\x{30a1}-\\x{30fa}ー]+)$");
    static const QRegularExpression hint("\\[.*\\]");
    for(int pi=0;pi<parts.size();++pi){
        if(cancel&&cancel->load())throw Failure("Cancelled");
        if(progress)progress(85+pi*14/std::max(1,int(parts.size())));
        const auto path=QString("$.voice_parts[%1]").arg(pi);const auto part=parts[pi].toObject();
        const auto trackNo=integer(part,"track_no",path,0,score.tracks.size()-1);auto &track=score.tracks[int(trackNo)];
        const auto position=integer(part,"position",path,0,TickLimit);
        const auto duration=part.contains("duration")?integer(part,"duration",path,0,TickLimit-position):0;
        score.endPosition=std::max(score.endPosition,sourceBlick(position+duration,480));
        const auto notes=array(part,"notes",path);const auto instance=track.id+QString("/part:%1").arg(pi);
        for(int ni=0;ni<notes.size();++ni){
            if(cancel&&cancel->load())throw Failure("Cancelled");
            if(++noteCount>200000)throw Failure("More than 200000 notes");
            const auto location=path+QString(".notes[%1]").arg(ni);const auto raw=notes[ni].toObject();Note n;
            const auto local=integer(raw,"position",location,0,TickLimit-position);
            n.sourceOnset=position+local;n.sourceDuration=integer(raw,"duration",location,1,TickLimit-n.sourceOnset);
            n.pitch=int(integer(raw,"tone",location,0,127));if(!raw["lyric"].isString())invalid(location+".lyric");n.lyrics=raw["lyric"].toString();
            n.id=instance+QString("/note:%1").arg(ni);n.groupInstance=instance;n.language="auto";n.muted=track.muted;
            n.onset=sourceBlick(n.sourceOnset,480);n.duration=sourceBlick(n.sourceOnset+n.sourceDuration,480)-n.onset;
            n.original=raw;n.original["partPosition"]=QString::number(position);
            // Phonetic hints and partial overrides require the selected phonemizer.
            // Strip a hint only when ordinary lyrics remain; never claim it is a
            // complete phone sequence or apply its offsets to score time.
            auto reading=n.lyrics;const bool hasHint=hint.match(reading).hasMatch();if(hasHint)reading.remove(hint);
            reading=reading.normalized(QString::NormalizationForm_KC).trimmed();const auto alias=vcv.match(reading);
            if(reading.startsWith('+'))n.lyricReading="ー"; // OpenUtau extension, not SVP's next-syllable '+'.
            else if(alias.hasMatch())n.lyricReading=alias.captured(1);else if(hasHint&&!reading.isEmpty())n.lyricReading=reading;
            if(hasHint||!array(raw,"phoneme_overrides",location,true).isEmpty())score.diagnostics.append({location,"phonemizer-data",Message("USTX phonetic hints and partial overrides are retained; phonemizer-dependent phonemes and offsets are not applied")});
            score.endPosition=std::max(score.endPosition,n.onset+n.duration);track.notes.append(std::move(n));
        }
    }
    for(auto &track:score.tracks)std::stable_sort(track.notes.begin(),track.notes.end(),[](const auto &a,const auto &b){return a.onset==b.onset?a.id<b.id:a.onset<b.onset;});
    if(!array(root,"wave_parts","$",true).isEmpty())score.diagnostics.append({"$.wave_parts","wave-parts",Message("USTX wave parts are retained in the source; attach audio separately")});
    score.diagnostics.append({"USTX","score-timing",Message("USTX timing uses notes and lyrics; voicebanks, phonemizers and rendered phoneme boundaries are not reproduced")});
    return score;
}
}
