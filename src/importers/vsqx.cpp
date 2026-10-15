// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>
namespace chosuta {
namespace {
struct Singer {qint64 position=0;int language=0;};
struct Part {qint64 position=0,length=0;QVector<Note> notes;QVector<Singer> singers;};
struct VocalTrack {int number=0;QString name;QVector<Part> parts;};
QString vocalLanguage(int id) {
    if(id==0)return "ja";
    if(id==1)return "en";
    if(id==4)return "zh";
    return QString("vocaloid:%1").arg(id);
}
class VsqxReader {
public:
    QXmlStreamReader xml;
    Score score;
    const std::atomic_bool *cancel;
    Progress progress;
    qsizetype byteCount;
    bool v4=false;
    int preMeasure=0;
    int reported=-1;
    QMap<int,Meter> meters;
    QMap<qint64,double> tempos;
    QMap<int,bool> muted,solo;
    QVector<VocalTrack> tracks;
    VsqxReader(const QByteArray &bytes,const std::atomic_bool *c,Progress p):xml(bytes),cancel(c),progress(p),byteCount(bytes.size()) {}
    bool next() {
        if(cancel&&cancel->load())throw Failure("Cancelled");
        const int percent=int(std::min<qint64>(99,xml.characterOffset()*100/std::max<qsizetype>(1,byteCount)));
        if(progress&&percent!=reported){reported=percent;progress(percent);}
        return xml.readNextStartElement();
    }
    QString text(){return xml.readElementText();}
    bool tag(const char *v3,const char *v4Name)const{return xml.name()==QLatin1String(v4?v4Name:v3);}
    void master() {
        while(next()) {
            if(xml.name()==QLatin1String("resolution"))score.sourceResolution=text().toLongLong();
            else if(xml.name()==QLatin1String("preMeasure"))preMeasure=text().toInt();
            else if(xml.name()==QLatin1String("timeSig")) {
                Meter m;
                while(next()) {
                    if(tag("posMes","m"))m.bar=text().toInt();
                    else if(tag("nume","nu"))m.numerator=text().toInt();
                    else if(tag("denomi","de"))m.denominator=text().toInt();
                    else xml.skipCurrentElement();
                }
                meters[m.bar]=m;
            } else if(xml.name()==QLatin1String("tempo")) {
                qint64 position=0;double bpm=120;
                while(next()) {
                    if(tag("posTick","t"))position=text().toLongLong();
                    else if(tag("bpm","v"))bpm=text().toDouble()/100.;
                    else xml.skipCurrentElement();
                }
                tempos[position]=bpm;
            } else xml.skipCurrentElement();
        }
    }
    Note note() {
        Note n;
        while(next()) {
            if(tag("posTick","t"))n.sourceOnset=text().toLongLong();
            else if(tag("durTick","dur"))n.sourceDuration=text().toLongLong();
            else if(tag("noteNum","n"))n.pitch=text().toInt();
            else if(tag("lyric","y"))n.lyrics=text();
            else if(tag("phnms","p"))n.phonemes=text();
            else xml.skipCurrentElement();
        }
        return n;
    }
    Part part() {
        Part p;
        while(next()) {
            if(tag("posTick","t"))p.position=text().toLongLong();
            else if(xml.name()==QLatin1String("playTime"))p.length=text().toLongLong();
            else if(xml.name()==QLatin1String("note"))p.notes.append(note());
            else if(xml.name()==QLatin1String("singer")) {
                Singer singer;
                while(next()) {
                    if(tag("posTick","t"))singer.position=text().toLongLong();
                    else if(tag("vBS","bs"))singer.language=text().toInt();
                    else xml.skipCurrentElement();
                }
                p.singers.append(singer);
            } else xml.skipCurrentElement();
        }
        std::stable_sort(p.singers.begin(),p.singers.end(),[](const auto &a,const auto &b){return a.position<b.position;});
        return p;
    }
    VocalTrack track() {
        VocalTrack t;
        while(next()) {
            if(tag("vsTrackNo","tNo"))t.number=text().toInt();
            else if(tag("trackName","name"))t.name=text();
            else if(tag("musicalPart","vsPart"))t.parts.append(part());
            else xml.skipCurrentElement();
        }
        return t;
    }
    void mixer() {
        while(next()) {
            if(xml.name()!=QLatin1String("vsUnit")){xml.skipCurrentElement();continue;}
            int number=0;bool mute=false,only=false;
            while(next()) {
                if(tag("vsTrackNo","tNo"))number=text().toInt();
                else if(tag("mute","m"))mute=text().toInt()!=0;
                else if(tag("solo","s"))only=text().toInt()!=0;
                else xml.skipCurrentElement();
            }
            muted[number]=mute;solo[number]=only;
        }
    }
    Score read() {
        if(!next())throw Failure("Not a VSQX project");
        v4=xml.name()==QLatin1String("vsq4");
        if((!v4&&xml.name()!=QLatin1String("vsq3"))||xml.namespaceUri()!=QString("http://www.yamaha.co.jp/vocaloid/schema/vsq%1/").arg(v4?4:3))throw Failure("Unsupported VSQX version");
        score.version=v4?4:3;score.sourceResolution=480;
        while(next()) {
            if(xml.name()==QLatin1String("version"))score.sourceVersion=text();
            else if(xml.name()==QLatin1String("masterTrack"))master();
            else if(xml.name()==QLatin1String("vsTrack"))tracks.append(track());
            else if(xml.name()==QLatin1String("mixer"))mixer();
            else xml.skipCurrentElement();
        }
        // Finish the document so Qt also reports incomplete/trailing XML.
        while(!xml.atEnd())xml.readNext();
        if(xml.hasError())throw Failure(Message("Cannot read VSQX XML").withDetail(xml.errorString()));
        if(score.sourceResolution<=0)throw Failure("Invalid source resolution");
        if(meters.isEmpty())meters[0]={0,4,4};
        // The source pre-roll is expressed in bars, including meter changes.
        Meter current;
        int bar=0;
        long double prefix=0;
        for(auto it=meters.cbegin();it!=meters.cend()&&it.key()<=preMeasure;++it) {
            if(current.denominator<=0||it.value().denominator<=0)throw Failure("Invalid meter");
            prefix+=static_cast<long double>(it.key()-bar)*score.sourceResolution*4*current.numerator/current.denominator;
            bar=it.key();current=it.value();
        }
        prefix+=static_cast<long double>(preMeasure-bar)*score.sourceResolution*4*current.numerator/current.denominator;
        score.sourceTickOrigin=qint64(std::llround(prefix));
        QMap<int,Meter> normalizedMeters;
        normalizedMeters[0]={0,4,4};
        for(auto m:meters){m.bar=std::max(0,m.bar-preMeasure);normalizedMeters[m.bar]=m;}
        score.time.meters=normalizedMeters.values();
        QMap<qint64,double> normalizedTempos{{0,120}};
        for(auto it=tempos.cbegin();it!=tempos.cend();++it)
            normalizedTempos[sourceBlick(std::max<qint64>(0,it.key()-score.sourceTickOrigin),score.sourceResolution)]=it.value();
        score.time.tempos.clear();
        for(auto it=normalizedTempos.cbegin();it!=normalizedTempos.cend();++it)score.time.tempos.append({it.key(),it.value()});
        bool anySolo=false;for(bool value:solo)anySolo=anySolo||value;
        for(int ti=0;ti<tracks.size();++ti) {
            const auto &raw=tracks[ti];
            Track t;
            t.id=QString("vsqx/track:%1").arg(ti);t.name=raw.name;
            t.muted=muted.value(raw.number)||(anySolo&&!solo.value(raw.number));
            for(int pi=0;pi<raw.parts.size();++pi) {
                const auto &p=raw.parts[pi];
                const QString instance=t.id+QString("/part:%1").arg(pi);
                score.endPosition=std::max(score.endPosition,sourceBlick(p.position+p.length-score.sourceTickOrigin,score.sourceResolution));
                for(int ni=0;ni<p.notes.size();++ni) {
                    if(cancel&&cancel->load())throw Failure("Cancelled");
                    Note n=p.notes[ni];
                    const auto local=n.sourceOnset;
                    qint64 length=n.sourceDuration;
                    if(p.length>0)length=std::min(length,p.length-local);
                    if(length<=0)continue;
                    n.sourceOnset+=p.position;
                    n.id=instance+QString("/note:%1").arg(ni);n.groupInstance=instance;
                    n.onset=sourceBlick(n.sourceOnset-score.sourceTickOrigin,score.sourceResolution);
                    n.duration=sourceBlick(n.sourceOnset+length-score.sourceTickOrigin,score.sourceResolution)-n.onset;
                    n.language="auto";
                    for(const auto &s:p.singers)if(s.position<=local)n.language=vocalLanguage(s.language);else break;
                    n.phoneset="vocaloid-xsampa";n.muted=t.muted;
                    n.original={{"partPosition",p.position},{"position",local},{"duration",n.sourceDuration},{"lyrics",n.lyrics},{"phonemes",n.phonemes}};
                    score.endPosition=std::max(score.endPosition,n.onset+n.duration);
                    t.notes.append(n);
                }
            }
            std::stable_sort(t.notes.begin(),t.notes.end(),[](const auto &a,const auto &b){return a.onset==b.onset?a.id<b.id:a.onset<b.onset;});
            score.tracks.append(t);
        }
        score.diagnostics.append({"VSQX","score-timing","VSQX timing uses notes and explicit phonemes, not rendered phoneme boundaries"});
        return score;
    }
};
}
Score parseVsqx(const QByteArray &bytes,const QString &,const std::atomic_bool *cancel,Progress progress) {
    return VsqxReader(bytes,cancel,progress).read();
}
}
