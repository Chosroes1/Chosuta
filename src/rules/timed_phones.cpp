// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
#include "core/source_time.h"
#include <algorithm>
#include <cmath>
namespace chosuta {
namespace {
struct Mouth {QString shape="unknown",kind="unknown",special;bool unknown=true;};
Mouth mouth(const TimedPhone &p,const Rules &rules){
    QString phone=rules.readings.value(p.id,p.phone),language=p.language;
    if(phone.contains('/')){language=phone.section('/',0,0);phone=phone.section('/',1);}
    if(rules.language!="auto")language=rules.language;
    if(language=="jpn")language="ja";
    if(language=="cmn"||language=="zho")language="zh";
    if(language=="eng")language="en";
    if(phone=="sil"||phone=="pau"||phone=="SP")return {"rest","rest","rest",false};
    if(phone=="AP"||phone=="br"||phone=="breath")return {"breath","br","br",false};
    if(phone=="cl")return {"closed","cl","cl",false};
    if((language!="en"&&(phone=="N"||phone=="ng"))||phone=="nasal")return {"closed","nasal","nasal",false};
    if(language=="ja"&&QStringList{"A","I","U","E","O"}.contains(phone))return {phone,"devoiced-vowel",{},false};
    if(language=="zh"){
        // One supplied phone remains one interval. Compound finals use a documented
        // representative mouth; no fabricated internal phoneme timing is introduced.
        static const QMap<QString,QString> finals={
            {"a","A"},{"ai","A"},{"ao","A"},{"an","A"},{"ang","A"},
            {"o","O"},{"ou","O"},{"ong","O"},{"e","E"},{"ei","E"},{"en","E"},{"eng","E"},{"er","E"},
            {"i","I"},{"in","I"},{"ing","I"},{"ia","A"},{"ian","A"},{"iang","A"},{"iao","A"},{"ie","E"},{"iong","O"},{"iu","U"},{"iou","U"},
            {"u","U"},{"ua","A"},{"uai","A"},{"uan","A"},{"uang","A"},{"ui","I"},{"uei","I"},{"un","U"},{"uen","U"},{"uo","O"},{"ueng","E"},
            {"v","U"},{"ü","U"},{"u:","U"},{"ve","E"},{"ue","E"},{"van","A"},{"vn","U"}};
        if(finals.contains(phone))return {finals[phone],"vowel",{},false};
    }
    const QString set=language=="en"?"arpabet":language=="zh"?"xsampa":"romaji";
    auto result=pronounce(phone,language,true,{},set,{},rules.consonants);
    if(result.unknown||result.syllables.size()!=1||result.syllables[0].isEmpty())return {};
    auto shape=result.syllables[0][0];auto role=result.roles[0][0];
    return {shape,role==SegmentRole::Consonant?"consonant":"vowel",{},false};
}
}
QVector<Event> generateTimedPhones(const Project &p,const std::atomic_bool *cancel,Progress report){
    auto check=[&]{if(cancel&&cancel->load())throw Failure("Cancelled");};check();
    QHash<QString,QVector<TimedSourceSlice>> routes;for(const auto &s:routeTimedSources(p,cancel))routes[s.source].append(s);
    QVector<Event> events;int total=0;for(const auto &t:p.score.tracks)total+=t.timedPhones.size();int done=0;
    for(const auto &t:p.score.tracks){
        if(t.muted||!p.selected.contains(t.id))continue;
        QVector<Mouth> mouths;for(const auto &phone:t.timedPhones){check();mouths.append(mouth(phone,p.rules));}
        // Look ahead only through contiguous consonants of the same source instance.
        QVector<QString> following(mouths.size());struct Context{QString vowel;double edge=0;};QHash<QString,Context> next,previous;
        for(int i=int(mouths.size())-1;i>=0;--i){check();const auto &phone=t.timedPhones[i];auto &c=next[phone.groupInstance];
            if(std::abs(phone.end-c.edge)>1e-9)c.vowel.clear();
            following[i]=c.vowel;
            if(mouths[i].kind=="vowel"||mouths[i].kind=="devoiced-vowel")c.vowel=mouths[i].shape;
            else if(mouths[i].kind!="consonant")c.vowel.clear();
            c.edge=phone.start;
        }
        for(int i=0;i<t.timedPhones.size();++i){check();const auto &phone=t.timedPhones[i];auto m=mouths[i];auto &c=previous[phone.groupInstance];
            if(std::abs(phone.start-c.edge)>1e-9)c.vowel.clear();
            QString old=c.vowel.isEmpty()?QString("closed"):c.vowel;
            if(m.shape=="open")m.shape=!following[i].isEmpty()?following[i]:c.vowel.isEmpty()?QString("I"):c.vowel;
            if(m.kind=="vowel"||m.kind=="devoiced-vowel")c.vowel=m.shape;else if(m.kind!="consonant")c.vowel.clear();c.edge=phone.end;
            const QString provenance=p.score.sourceFormat+(p.score.sourceFormat=="vvproj"?"/query-time-estimated/shape-estimated":"/supplied-time/shape-estimated");
            auto append=[&](double a,double b,const QString &shape,int part){if(b<=a)return;Event e;e.id=phone.id+QString("/segment:%1").arg(part);e.source=phone.id;e.track=t.id;e.shape=shape;e.start=a;e.end=b;e.phone=phone.phone;e.text=phone.text.isEmpty()?phone.phone:phone.text;e.articulation=m.kind;e.unknown=m.unknown||shape=="unknown";e.provenance=provenance;
                for(const auto &slice:routes.value(phone.id)){auto v=e;v.start=std::max(a,slice.start);v.end=std::min(b,slice.end);if(v.end<=v.start)continue;
                    if(v.start!=a||v.end!=b)v.id+="/route:"+QString::number(slice.start,'g',17);
                    if(events.size()>=500000)throw Failure("More than 500000 generated segments");
                    events.append(v);}
            };
            if(m.special.isEmpty())append(phone.start,phone.end,m.shape,0);
            else{auto policy=p.rules.special.value(m.special);if(policy.mode=="hold")append(phone.start,phone.end,old,0);
                else if(policy.mode=="timed"){double cut=std::min(phone.end,phone.start+policy.holdSeconds);append(phone.start,cut,old,0);append(cut,phone.end,policy.shape,1);}
                else append(phone.start,phone.end,policy.shape,0);}
            if(report&&++done%128==0)report(done*100/std::max(1,total));
        }
    }
    std::sort(events.begin(),events.end(),[](const auto &a,const auto &b){return a.start==b.start?a.id<b.id:a.start<b.start;});
    if(report)report(100);
    return events;
}
}
