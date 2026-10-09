// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace chosuta {
    QVector<Event> generate(const Project&p,const std::atomic_bool*cancel,Progress progress) {
        validatePronunciationOptions(p.rules.pronunciation);
        validateConsonants(p.rules.consonants);
        if(!std::isfinite(p.rules.consonantRatio)||p.rules.consonantRatio<0||p.rules.consonantRatio>.8||!std::isfinite(p.rules.consonantMaxSeconds)||p.rules.consonantMaxSeconds<0||p.rules.consonantMaxSeconds>1)throw Failure("Invalid consonant timing settings");
        if(!p.rules.englishDictionary.isEmpty()&&QFileInfo(p.rules.englishDictionary).size()+pronunciationDictionaryBytes(p.rules.pronunciation)>DictionaryByteLimit)
            throw Failure("Pronunciation dictionaries exceed 3 MB");
        struct Candidate {
            Event event;
            int priority=0,order=0;
        };
        QVector<Candidate>candidates;
        for(int ti=0;ti<p.score.tracks.size();++ti) {
            const auto&t=p.score.tracks[ti];
            if(!p.selected.contains(t.id)||t.muted)continue;
            struct Context {QString previous="closed",vowel;Pronunciation word;int continuation=0;};
            QMap<QString,Context>contexts;
            auto normalized=[](const Note&note) {return note.lyrics.normalized(QString::NormalizationForm_KC).trimmed();};
            // Count continuation notes in each reference instance in linear time.
            // Notes from another group must not consume this group's syllables.
            QVector<int>following(t.notes.size());
            QMap<QString,int>remaining;
            for(int i=int(t.notes.size())-1;i>=0;--i) {
                const auto &note=t.notes[i];
                following[i]=remaining.value(note.groupInstance);
                const auto lyric=normalized(note);
                if(note.muted)continue;
                if(!p.rules.readings.value(note.id).isEmpty()||!note.phonemes.trimmed().isEmpty())remaining[note.groupInstance]=0;
                else if(lyric=="+")++remaining[note.groupInstance];
                else if(lyric!="-"&&lyric!="ー")remaining[note.groupInstance]=0;
            }
            for(int ni=0;ni<t.notes.size();++ni) {
                if(cancel&&cancel->load())throw Failure("Cancelled");
                if(progress&&ni%128==0)progress((ti*100+ni*100/std::max(1,int(t.notes.size())))*80/std::max(1,int(p.score.tracks.size()))/100);
                const auto&n=t.notes[ni];
                if(n.muted)continue;
                auto &context=contexts[n.groupInstance];
                auto &previous=context.previous;
                auto &word=context.word;
                auto &continuation=context.continuation;
                double start=p.score.time.seconds(n.onset),end=p.score.time.seconds(n.onset+n.duration);
                QString emittedPhone,emittedArticulation;
                auto emitPart=[&](QString shape,double a,double b,int part,QString provenance,bool unknown=false) {
                    if(b<=a)return;
                    if(candidates.size()>=500000)throw Failure("More than 500000 generated segments");
                    Event e;
                    e.id=n.id+QString("/segment:%1").arg(part);
                    e.source=n.id;
                    e.track=t.id;
                    e.shape=shape;
                    e.start=a;
                    e.end=b;
                    e.provenance=provenance;
                    e.text=n.lyrics;
                    e.unknown=unknown||shape=="unknown";
                    e.phone=emittedPhone;e.articulation=emittedArticulation;
                    candidates.append( {
                        e,p.priorities.value(t.id,ti),ti
                    });
                    previous=shape;
                    if(QStringList{"A","I","U","E","O"}.contains(shape))context.vowel=shape;
                };
                QString text=normalized(n),special;
                auto reading=p.rules.readings.value(n.id);
                bool explicitPhone=!reading.isEmpty()||!n.phonemes.trimmed().isEmpty();
                QString phones=reading.isEmpty()?n.phonemes:reading;
                if(!explicitPhone) {
                    if(text=="br")special="br";
                    else if(text=="cl")special="cl";
                    else if(text.isEmpty())special="empty";
                    else if(text=="-"||text=="ー")special="extend";
                }
                if(!special.isEmpty()) {
                    emittedArticulation=special;
                    auto policy=p.rules.special.value(special);
                    QString hold=special=="extend"&&!context.vowel.isEmpty()?context.vowel:previous;
                    if(special!="extend") {word={};continuation=0;context.vowel.clear();}
                    if(policy.mode=="hold")emitPart(hold,start,end,0,"special/"+special+"/estimated");
                    else if(policy.mode=="timed") {
                        double cut=std::min(end,start+policy.holdSeconds);
                        emitPart(hold,start,cut,0,"special/"+special+"/estimated");
                        emitPart(policy.shape,cut,end,1,"special/"+special+"/estimated",policy.shape=="unknown");
                    }
                    else emitPart(policy.shape,start,end,0,"special/"+special+"/estimated",policy.shape=="unknown");
                    continue;
                }
                Pronunciation result;
                if(!explicitPhone&&text=="+") {
                    if(continuation>=word.syllables.size()) {
                        result.unknown=true;
                        result.provenance="unresolved-continuation/estimated";
                    }
                    else {
                        const int take=following[ni]>0?1:int(word.syllables.size())-continuation;
                        result.syllables=word.syllables.mid(continuation,take);
                        result.roles=word.roles.mid(continuation,take);result.phones=word.phones.mid(continuation,take);
                        continuation+=take;
                        result.provenance=word.provenance+"/continuation";
                    }
                }
                else {
                    QString language=p.rules.language=="auto"?n.language:p.rules.language;
                    bool phoneNotation=explicitPhone;
                    if(!reading.isEmpty()) {
                        if(reading.contains(QRegularExpression("[\\x{3040}-\\x{30ff}\\x{3400}-\\x{9fff}]")))phoneNotation=false;
                        else if(language=="en"&&reading==reading.toLower())phoneNotation=false;
                        else if(language=="zh"&&reading.split(' ').value(0).size()>1)phoneNotation=false;
                    }
                    result=pronounce(explicitPhone?phones:text,language,phoneNotation,p.rules.englishDictionary,reading.isEmpty()?n.phoneset:QString(),reading.isEmpty()?p.rules.pronunciation:PronunciationOptions{},p.rules.consonants);
                    {
                        word=result;
                        continuation=following[ni]>0?1:int(result.syllables.size());
                        if(following[ni]>0&&!result.syllables.isEmpty()){result.syllables={result.syllables.front()};result.roles={result.roles.front()};result.phones={result.phones.front()};}
                        context.vowel.clear();
                    }
                }
                if(result.unknown||result.syllables.isEmpty()) {
                    emittedArticulation="unknown";
                    emitPart("unknown",start,end,0,result.provenance,true);
                    continue;
                }
                int part=0;
                int syllableCount=result.syllables.size();
                for(int si=0;si<syllableCount;++si) {
                    auto shapes=result.syllables[si];
                    double a=start+(end-start)*si/syllableCount,b=start+(end-start)*(si+1)/syllableCount;
                    int cons=0;
                    while(cons<shapes.size()&&(shapes[cons]=="closed"||shapes[cons]=="open"))++cons;
                    double prefix=cons==shapes.size()?1.:std::clamp(p.rules.consonantRatio,0.,.8);
                    int vowels=shapes.size()-cons;
                    int leading=0,trailing=0;
                    double onset=0,coda=0;
                    const auto roles=result.roles.value(si);
                    const bool bounded=p.rules.consonantMaxSeconds>0&&roles.contains(SegmentRole::Vowel)&&!(roles.front()==SegmentRole::Special&&shapes.front()=="closed");
                    if(bounded) {
                        while(leading<roles.size()&&roles[leading]==SegmentRole::Consonant)++leading;
                        while(trailing<roles.size()-leading&&roles[roles.size()-1-trailing]==SegmentRole::Consonant)++trailing;
                        const double budget=std::min((b-a)*p.rules.consonantRatio,p.rules.consonantMaxSeconds);
                        onset=leading?budget:0;coda=trailing?budget:0;
                        // Both groups together must leave at least 20% for the vowel/special body.
                        if(onset+coda>(b-a)*.8){double factor=(b-a)*.8/(onset+coda);onset*=factor;coda*=factor;}
                    }
                    for(int j=0;j<shapes.size();++j) {
                        double x,y;
                        if(j<cons) {
                            x=a+(b-a)*prefix*j/cons;
                            y=a+(b-a)*prefix*(j+1)/cons;
                        }
                        else {
                            x=a+(b-a)*(prefix+(1-prefix)*(j-cons)/std::max(1,vowels));
                            y=a+(b-a)*(prefix+(1-prefix)*(j-cons+1)/std::max(1,vowels));
                            if(cons==0) {
                                x=a+(b-a)*j/shapes.size();
                                y=a+(b-a)*(j+1)/shapes.size();
                            }
                        }
                        if(bounded) {
                            if(j<leading){x=a+onset*j/leading;y=a+onset*(j+1)/leading;}
                            else if(j>=shapes.size()-trailing){int k=j-(shapes.size()-trailing);x=b-coda+coda*k/trailing;y=b-coda+coda*(k+1)/trailing;}
                            else {int count=shapes.size()-leading-trailing,k=j-leading;double body=b-a-onset-coda;x=a+onset+body*k/count;y=a+onset+body*(k+1)/count;}
                        }
                        emittedPhone=result.phones.value(si).value(j);
                        const auto role=roles.value(j,SegmentRole::Special);
                        emittedArticulation=role==SegmentRole::Consonant?"consonant":role==SegmentRole::Vowel?"vowel":shapes[j]=="nasal"?"nasal":shapes[j]=="breath"?"br":shapes[j]=="rest"?"rest":"cl";
                        QString shape=shapes[j];
                        if(shape=="open"){
                            shape="I"; // neutral open fallback for a consonant without a vowel
                            bool found=false;
                            for(int k=j+1;k<shapes.size();++k)if(QStringList{"A","I","U","E","O"}.contains(shapes[k])){shape=shapes[k];found=true;break;}
                            if(!found)for(int k=j-1;k>=0;--k)if(QStringList{"A","I","U","E","O"}.contains(shapes[k])){shape=shapes[k];break;}
                        }
                        if(shape=="nasal") {
                            auto policy=p.rules.special.value("nasal");
                            if(policy.mode=="timed") {
                                double cut=std::min(y,x+policy.holdSeconds);
                                emitPart(previous,x,cut,part++,result.provenance);
                                emitPart(policy.shape,cut,y,part++,result.provenance);
                                continue;
                            }
                            shape=policy.mode=="hold"?previous:policy.shape;
                        }
                        emitPart(shape,x,y,part++,result.provenance);
                    }
                }
            }
        }
        // Sweep only active singing intervals: rest holding is evaluated after source routing.
        struct Boundary {
            double time;
            int index;
            bool start;
        };
        QVector<Boundary>boundaries;
        for(int i=0;i<candidates.size();++i) {
            boundaries.append( {
                candidates[i].event.start,i,true
            });
            boundaries.append( {
                candidates[i].event.end,i,false
            });
        }
        std::sort(boundaries.begin(),boundaries.end(),[](auto a,auto b) {
            return a.time==b.time?a.start<b.start:a.time<b.time;
        });
        auto less=[&](int a,int b) {
            const auto&x=candidates[a];
            const auto&y=candidates[b];
            if(x.priority!=y.priority)return x.priority<y.priority;
            if(x.order!=y.order)return x.order<y.order;
            if(x.event.start!=y.event.start)return x.event.start>y.event.start;
            return x.event.id<y.event.id;
        };
        std::set<int,decltype(less)>active(less);
        QVector<Event>result;
        double previousEnd=-std::numeric_limits<double>::infinity();
        int primary=-1;
        for(int ti=0;ti<p.score.tracks.size();++ti)if(p.selected.contains(p.score.tracks[ti].id)&&!p.score.tracks[ti].muted)
        if(primary<0||p.priorities.value(p.score.tracks[ti].id,ti)<p.priorities.value(p.score.tracks[primary].id,primary))primary=ti;
        for(int i=0;i<boundaries.size();) {
            if(cancel&&cancel->load())throw Failure("Cancelled");
            if(progress&&i%1024==0)progress(80+i*20/std::max(1,int(boundaries.size())));
            double time=boundaries[i].time;
            while(i<boundaries.size()&&boundaries[i].time==time) {
                auto b=boundaries[i++];
                if(b.start)active.insert(b.index);
                else active.erase(b.index);
            }
            if(i==boundaries.size()||active.empty())continue;
            const auto&c=candidates[*active.begin()];
            if(!p.rules.harmonyTakeover&&primary!=c.order)continue;
            double next=boundaries[i].time;
            if(next<=time)continue;
            if(!result.isEmpty()&&result.last().id.section("/route:",0,0)==c.event.id&&previousEnd==time) {
                result.last().end=next;
                previousEnd=next;
                continue;
            }
            auto e=c.event;
            if(time!=e.start||next!=e.end)e.id+=QString("/route:%1").arg(p.score.time.blicks(time));
            e.start=time;
            e.end=next;
            result.append(e);
            previousEnd=next;
        }
        if(progress)progress(100);
        return result;
    }
}
