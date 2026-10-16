// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>
namespace chosuta {
namespace {
// CCS Clock/Duration are integer ticks, 960 per quarter. Keep the timeline's
// reserved first bar: converters that remove it change alignment with audio.
struct Unit {QString group,language,instance;QVector<Note> notes;QMap<qint64,double> tempos;QMap<qint64,QPair<int,int>> meters;};
struct Group {QString name;bool muted=false,solo=false;};
class CcsReader {
    QXmlStreamReader xml;
    const std::atomic_bool *cancel;
    Progress progress;
    qsizetype byteCount;
    int reported=-1,sceneCount=0,unitCount=0,noteCount=0;
    Score score;
    QVector<Unit> units;
    QMap<QString,Group> groups;
    QStringList groupOrder;
    QMap<qint64,double> tempos;
    QMap<qint64,QPair<int,int>> meters;
    QSet<QString> warned;
    QString location="CCS";
    [[noreturn]] void invalid(const QString &field){throw Failure(Message::format("CCS %1: invalid or unsupported field %2",{location,field}));}
    void check(){
        if(cancel&&cancel->load())throw Failure("Cancelled");
        const int percent=int(std::min<qint64>(99,xml.characterOffset()*100/std::max<qsizetype>(1,byteCount)));
        if(progress&&percent!=reported){reported=percent;progress(percent);}
    }
    bool next(){check();return xml.readNextStartElement();}
    // Parameter curves may be large; check cancellation while skipping them too.
    void skip(){int depth=1;while(depth&&!xml.atEnd()){check();auto token=xml.readNext();if(token==QXmlStreamReader::StartElement)++depth;else if(token==QXmlStreamReader::EndElement)--depth;}}
    QString attr(const char *name)const{return xml.attributes().value(QLatin1String(name)).toString();}
    qint64 integer(const char *name,qint64 low,qint64 high){bool ok=false;const auto n=attr(name).toLongLong(&ok);if(!ok||n<low||n>high)invalid(name);return n;}
    bool boolean(const char *name){const auto s=attr(name);if(s.isEmpty()||s=="false"||s=="0")return false;if(s=="true"||s=="1")return true;invalid(name);}
    void warn(const QString &code,const char *message){if(!warned.contains(code)){warned.insert(code);score.diagnostics.append({location,code,message});}}
    void tempo(Unit &unit){
        auto &tempos=unit.tempos;
        while(next()){
            if(xml.name()!=QLatin1String("Sound")){skip();continue;}
            const auto clock=integer("Clock",0,MusicLimit/Blick*960);
            bool ok=false;const double bpm=attr("Tempo").toDouble(&ok);
            if(!ok||!std::isfinite(bpm)||bpm<=0||bpm>10000)invalid("Tempo");
            if(tempos.contains(clock)&&tempos[clock]!=bpm)invalid("conflicting Tempo at Clock "+QString::number(clock));
            tempos[clock]=bpm;if(tempos.size()>10000)invalid("Tempo count");skip();
        }
    }
    void beat(Unit &unit){
        auto &meters=unit.meters;
        while(next()){
            if(xml.name()!=QLatin1String("Time")){skip();continue;}
            const auto clock=integer("Clock",0,MusicLimit/Blick*960);
            const int n=int(integer("Beats",1,64)),d=int(integer("BeatType",1,64));
            if(d&(d-1))invalid("BeatType");
            const QPair<int,int> value{n,d};
            if(meters.contains(clock)&&meters[clock]!=value)invalid("conflicting Beat at Clock "+QString::number(clock));
            meters[clock]=value;if(meters.size()>10000)invalid("Beat count");skip();
        }
    }
    void notes(Unit &unit){
        while(next()){
            if(xml.name()!=QLatin1String("Note")){skip();continue;}
            if(++noteCount>200000)invalid("Note count");
            const auto noteLocation=location;location+=QString("/note:%1").arg(unit.notes.size());
            Note n;n.groupInstance=unit.instance;n.id=unit.instance+QString("/note:%1").arg(unit.notes.size());
            n.sourceOnset=integer("Clock",0,MusicLimit/Blick*960);
            n.sourceDuration=integer("Duration",1,MusicLimit/Blick*960);
            n.onset=sourceBlick(n.sourceOnset,960);
            n.duration=sourceBlick(n.sourceOnset+n.sourceDuration,960)-n.onset;
            n.pitch=int(integer("PitchStep",0,11)+12*(integer("PitchOctave",-1,9)+1));if(n.pitch>127)invalid("PitchOctave/PitchStep");
            n.lyrics=attr("Lyric");n.phonemes=attr("Phonetic");
            n.phonemes.replace(u',',u' ');n.phonemes.replace(u'|',u' ');
            n.language=unit.language=="Japanese"?"ja":unit.language=="English"?"en":"ccs:"+unit.language;
            n.phoneset=n.language=="ja"?"romaji":n.language=="en"?"arpabet":"ccs";
            for(const auto &a:xml.attributes())n.original.insert(a.name().toString(),a.value().toString());
            if(boolean("Breath")||boolean("DoReMi")||boolean("Staccato")||boolean("SlurStart")||boolean("SlurStop"))warn("note-expression","CCS note expression attributes are retained; rendered breath, solfege, staccato and slur timing are not reproduced");
            unit.notes.append(n);score.endPosition=std::max(score.endPosition,n.onset+n.duration);
            skip();location=noteLocation;
        }
    }
    void song(Unit &unit){
        const auto version=attr("Version");
        if(!version.startsWith("1."))invalid("Song.Version");
        if(version!="1.02"&&version!="1.07"&&version!="1.08")warn("song-version","CCS Song version is not independently verified; importing the supported 1.x fields");
        QSet<QString> seen;
        while(next()){
            const auto name=xml.name().toString();
            if(QStringList{"Tempo","Beat","Score"}.contains(name)&&seen.contains(name))invalid(name);
            seen.insert(name);
            if(name=="Tempo")tempo(unit);else if(name=="Beat")beat(unit);else if(name=="Score")notes(unit);else skip();
        }
        if(!seen.contains("Score"))invalid("Song.Score");
    }
    void readUnits(){
        while(next()){
            if(xml.name()!=QLatin1String("Unit")){skip();continue;}
            const int index=unitCount++;
            if(attr("Category")!="SingerSong"){
                warn("non-song","CCS talk and audio units are retained in the source but are not imported; only SingerSong is supported");skip();continue;
            }
            Unit unit;unit.group=attr("Group");unit.language=attr("Language");
            location=QString("scene:0/group:%1/unit:%2").arg(unit.group).arg(index);unit.instance="ccs/"+location;
            if(unit.group.isEmpty())invalid("Group");
            // Standard song units start at zero. Do not guess whether a nonzero
            // Unit.StartTime is an offset of the Song.Clock map or an outer clip.
            if(!QRegularExpression("^(?:0\\.)?00:00:00(?:\\.0+)?$").match(attr("StartTime")).hasMatch())invalid("Unit.StartTime (requires zero)");
            bool found=false;
            while(next()){
                if(xml.name()==QLatin1String("Song")){if(found)invalid("duplicate Song");found=true;song(unit);}else skip();
            }
            if(!found)invalid("Song");
            units.append(unit);location="CCS";
        }
    }
    void readGroups(){
        while(next()){
            if(xml.name()!=QLatin1String("Group")){skip();continue;}
            if(attr("Category")=="SingerSong"){
                const auto id=attr("Id");if(id.isEmpty()||groups.contains(id))invalid("Group.Id");
                groups[id]={attr("Name"),boolean("IsMuted"),boolean("IsSolo")};groupOrder.append(id);
            }
            skip();
        }
    }
    void sequence(){
        while(next()){
            if(xml.name()!=QLatin1String("Scene")){skip();continue;}
            if(++sceneCount>1)invalid("multiple Scene timelines");
            while(next()){
                if(xml.name()==QLatin1String("Units"))readUnits();else if(xml.name()==QLatin1String("Groups"))readGroups();else skip();
            }
        }
    }
public:
    CcsReader(const QByteArray &bytes,const std::atomic_bool *c,Progress p):xml(bytes),cancel(c),progress(p),byteCount(bytes.size()){}
    Score read(){
        if(!next()||xml.name()!=QLatin1String("Scenario"))throw Failure("Not a CCS Scenario project");
        score.sourceResolution=960;
        bool found=false;
        while(next()){
            if(xml.name()==QLatin1String("Sequence")){if(found)invalid("duplicate Sequence");found=true;sequence();}
            else if(xml.name()==QLatin1String("Generation")){
                while(next()){if(xml.name()==QLatin1String("Author"))score.sourceVersion=attr("Version");skip();}
            }else skip();
        }
        while(!xml.atEnd()){check();xml.readNext();}
        if(xml.hasError())throw Failure(Message("Cannot read CCS XML").withDetail(xml.errorString()));
        if(!found||sceneCount!=1)invalid("Sequence.Scene");
        if(units.isEmpty())throw Failure("CCS has no SingerSong units; talk import is not supported");
        // One shared TimeMap cannot represent independent, conflicting track tempi.
        // Remove redundant markers before comparing explicit unit maps.
        auto compact=[](auto map){for(auto it=map.begin();it!=map.end();){if(it!=map.begin()&&it.value()==std::prev(it).value())it=map.erase(it);else ++it;}return map;};
        for(const auto &u:units){
            if(!u.tempos.isEmpty()){auto map=compact(u.tempos);if(!tempos.isEmpty()&&map!=tempos)invalid("inconsistent song Tempo maps");tempos=map;}
            if(!u.meters.isEmpty()){auto map=compact(u.meters);if(!meters.isEmpty()&&map!=meters)invalid("inconsistent song Beat maps");meters=map;}
        }
        if(!tempos.contains(0)){tempos[0]=120;warn("default-tempo","CCS has no initial numeric tempo; using 120 BPM");}
        if(!meters.contains(0)){meters[0]={4,4};warn("default-meter","CCS has no initial meter; using 4/4 for bar and beat labels");}
        score.time.tempos.clear();for(auto it=tempos.cbegin();it!=tempos.cend();++it)score.time.tempos.append({sourceBlick(it.key(),960),it.value()});
        score.time.meters.clear();qint64 previous=0,bar=0,length=3840;
        for(auto it=meters.cbegin();it!=meters.cend();++it){
            if((it.key()-previous)%length)invalid("Beat.Clock outside a bar boundary");
            bar+=(it.key()-previous)/length;if(bar>10000000)invalid("Beat.Clock");
            score.time.meters.append({int(bar),it.value().first,it.value().second});
            previous=it.key();length=960*4*it.value().first/it.value().second;
        }
        bool anySolo=false;for(const auto &g:groups)anySolo=anySolo||g.solo;
        for(const auto &u:units)if(!groupOrder.contains(u.group)){groupOrder.append(u.group);warn("missing-group","CCS song unit has no matching SingerSong group; using its group ID as track name");}
        for(const auto &id:groupOrder){
            const auto g=groups.value(id);Track t;t.id="ccs/scene:0/group:"+id;t.name=g.name.isEmpty()?id:g.name;t.muted=g.muted||(anySolo&&!g.solo);
            for(const auto &u:units)if(u.group==id)for(auto n:u.notes){n.muted=t.muted;t.notes.append(n);}
            std::stable_sort(t.notes.begin(),t.notes.end(),[](const auto &a,const auto &b){return a.onset<b.onset;});score.tracks.append(t);
        }
        warn("score-timing","CCS timing uses the 960-tick score, including its reserved first bar; phoneme boundaries and synthesis parameters are not reproduced");
        return score;
    }
};
}
Score parseCcs(const QByteArray &bytes,const QString &,const std::atomic_bool *cancel,Progress progress){return CcsReader(bytes,cancel,progress).read();}
}
