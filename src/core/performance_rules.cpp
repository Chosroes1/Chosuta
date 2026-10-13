// SPDX-License-Identifier: GPL-3.0-or-later
#include "performance_analysis.h"
#include "appearance.h"
#include <algorithm>
#include <cmath>
namespace chosuta {
namespace {
struct Stats {double median=0,high=0,sounding=0,coverage=0,ioi=0;int count=0;bool valid=false;};
bool strongBeat(const Project &p,qint64 tick){
    qint64 base=0;auto meter=p.score.time.meters.front();for(int i=1;i<p.score.time.meters.size();++i){auto next=p.score.time.meters[i];qint64 at=base+qint64(next.bar-meter.bar)*meter.numerator*(Blick*4/meter.denominator);if(tick<at)break;base=at;meter=next;}
    qint64 unit=Blick*4/meter.denominator;auto within=((tick-base)%(unit*meter.numerator)+unit*meter.numerator)%(unit*meter.numerator);if(within%unit)return false;int b=int(within/unit)+1;
    QString key=QString("%1/%2").arg(meter.numerator).arg(meter.denominator);if(p.performance.analysis.strongBeats.contains(key))return p.performance.analysis.strongBeats[key].contains(b);
    return meter.numerator>3&&meter.numerator%3==0?(b-1)%3==0:meter.numerator==4?(b==1||b==3):b==1;
}
}
PerformanceDraft generatePerformance(const Project &p,const std::atomic_bool *cancel,Progress report){
    PerformanceDraft result;
    try{
        validateAnalysisSettings(p.performance.analysis);validatePerformance(p);auto check=[&]{if(cancel&&cancel->load())throw Failure("Cancelled");};check();result.basisHash=performanceBasisHash(p,cancel);
        auto analysis=analyzePerformance(p,cancel,report);auto &v=analysis.notes;result.suggestions=analysis.suggestions;
        const auto &s=p.performance.settings;const auto &t=s.timing();const bool music=s.timeBasis==PerformanceTimeBasis::Music,diatonic=p.performance.analysis.intervalUnit=="diatonic";
        const double leap=diatonic?s.leapDegrees-1:s.leapSemitones,progression=diatonic?s.progressionDegrees-1:s.progressionSemitones,near=diatonic?s.nearDegrees-1:s.nearSemitones,contrast=diatonic?s.contrastDegrees-1:s.contrastSemitones;
        QVector<int> contexts(v.size()),ends(v.size());int context=0;for(int i=0;i<v.size();++i){if(v[i].hard)context=i;contexts[i]=context;}int end=v.size();for(int i=int(v.size())-1;i>=0;--i){ends[i]=end;if(v[i].hard)end=i;}
        auto stats=[&](double a,double b,int first,int last){Stats r;QVector<QPair<double,double>> pitches;QVector<double> intervals;double named=0;
            auto it=std::lower_bound(v.cbegin()+first,v.cbegin()+last,a,[](const auto &n,double x){return n.y<=x;});int from=int(it-v.cbegin()),count=0;
            for(int k=from;k<last&&v[k].x<b;++k){if(++count>512){analysis.degraded=true;break;}double weight=std::max(0.,std::min(b,v[k].y)-std::max(a,v[k].x));if(weight<=0)continue;r.sounding+=weight;++r.count;if(k>from)intervals.append(v[k].x-v[k-1].x);
                if(v[k].named){named+=weight;pitches.append({v[k].pitch,weight});if(v[k].pitch>=analysis.q75)r.high+=weight;}}
            r.coverage=r.sounding>0?named/r.sounding:0;r.valid=r.count>=4&&named>=t.support&&r.coverage>=.9;
            std::sort(pitches.begin(),pitches.end(),[](auto a,auto b){return a.first<b.first;});double sum=0;for(auto pitch:pitches){sum+=pitch.second;if(sum>=named*.5){r.median=pitch.first;break;}}if(named>0)r.high/=named;
            if(!intervals.isEmpty()){std::sort(intervals.begin(),intervals.end());r.ioi=intervals[intervals.size()/2];}return r;};
        auto support=[&](int i,double goal,double length){double amount=0;for(int k=i;k<ends[i]&&v[k].x<v[i].x+length;++k){if(k-i>=256){analysis.degraded=true;break;}if(k>i&&v[k].phrase)break;if(v[k].named&&std::abs(v[k].pitch-goal)<=near)amount+=std::max(0.,std::min(v[k].y,v[i].x+length)-v[k].x);}return amount;};
        QVector<PerformanceRule> rules=s.rules;std::stable_sort(rules.begin(),rules.end(),[](auto a,auto b){return a.priority>b.priority;});
        QString current="normal";double since=0,target=0,motifUntil=-1,motifGoal=0;bool falling=false;double sectionSuggestionAt=-1e30;
        for(int i=0;i<v.size();++i){check();if(report&&i%128==0)report(35+i*65/std::max(1,int(v.size())));const auto &n=v[i];int first=contexts[i],last=ends[i];bool reset=n.hard,carryGap=false;
            auto local=stats(n.y-t.window,n.y,first,last);auto background=stats(n.x-t.window-t.background,n.x-t.window,first,last);
            double base=background.valid?background.median:analysis.q50;
            int from=i;while(from>first&&n.x-v[from-1].x<=t.window&&i-from<256&&!v[from].phrase)--from;
            double net=0,movement=0;bool distance=n.named;for(int k=from;k<i;++k){distance&=v[k].named;if(v[k].named&&v[k+1].named)movement+=std::abs(v[k+1].pitch-v[k].pitch);}if(distance)net=n.pitch-v[from].pitch;
            bool previous=i>first&&n.named&&v[i-1].named&&!n.phrase;double delta=previous?n.pitch-v[i-1].pitch:0;
            double goalSupport=n.named?support(i,n.pitch,t.lookahead):0;
            bool up=s.progression&&distance&&i-from>=2&&net>=progression&&movement>0&&net/movement>=s.efficiency;
            bool jump=s.leap&&previous&&std::abs(delta)>=leap;
            bool newMotif=false;int motifEnd=analysis.motifEnd.value(i,-1);double candidateGoal=n.pitch;
            if(motifEnd>=i&&analysis.pitched){double high=0,total=0;for(int k=i;k<=motifEnd;++k){double weight=v[k].y-v[k].x;total+=weight;if(v[k].named&&v[k].pitch>=analysis.q75&&v[k].pitch>=base+contrast){high+=weight;candidateGoal=std::max(candidateGoal,v[k].pitch);}}
                newMotif=total>=t.confirm&&high>=s.motifShare*total&&(n.phrase||candidateGoal>=n.pitch+contrast);
                if(newMotif){double highEnd=n.y;for(int k=i;k<=motifEnd;++k)if(v[k].named&&v[k].pitch>=candidateGoal-near)highEnd=v[k].y;motifUntil=std::max(motifUntil,highEnd);motifGoal=candidateGoal;}}
            bool a2=s.motif&&newMotif;
            if(reset){current="normal";since=n.x;target=n.pitch;falling=false;if(!a2)motifUntil=-1;}
            bool compatibleMotif=s.motif&&n.x<motifUntil;
            if(current=="emphasis"&&distance&&net<=-contrast&&!n.phrase)falling=true;
            bool a1=(jump||up)&&goalSupport>=std::max(t.support,t.confirm)&&!(jump&&delta<0&&falling&&!n.phrase&&!a2)&&!(current=="emphasis"&&n.pitch<target-near&&!n.phrase&&!a2);
            auto next=stats(n.x,n.x+t.window,first,last);
            bool elevated=analysis.pitched&&background.valid&&next.valid&&next.high>=s.highShare&&next.median>=background.median+contrast;
            bool active=elevated&&next.count>=3&&next.count/std::max(next.sounding,1e-9)>=s.activityRatio*background.count/std::max(background.sounding,1e-9);
            auto later=elevated&&!active?stats(n.x+t.window,n.x+2*t.window,first,last):Stats{};
            bool a3=s.section&&elevated&&(active||(later.valid&&later.high>=s.highShare&&later.median>=background.median+contrast));
            if(a3&&active&&n.x-sectionSuggestionAt>=t.window){result.suggestions.append({n.note->id,"section-break"});sectionSuggestionAt=n.x;}
            if(i&&n.x-v[i-1].y>1e-9&&!reset){bool compatible=n.joined||(s.continuity&&n.x-v[i-1].y<=t.breath&&(compatibleMotif||(n.named&&std::abs(n.pitch-target)<=near)));carryGap=compatible;if(!compatible){reset=true;current="normal";since=n.x;target=n.pitch;falling=false;}}
            bool same=n.named&&std::abs(n.pitch-target)<=near,returns=false;
            if(s.continuity&&!same)for(int k=i+1;k<last&&k-i<=256&&v[k].x-n.x<=t.lookahead;++k){if(v[k].phrase)break;if(v[k].named&&std::abs(v[k].pitch-target)<=near){returns=true;break;}}
            double lowSupport=0;for(int k=i;k<last&&k-i<256&&v[k].x<n.x+t.lookahead;++k){if(k>i&&v[k].phrase)break;if(v[k].named&&target-v[k].pitch>=contrast)lowSupport+=std::max(0.,std::min(v[k].y,n.x+t.lookahead)-v[k].x);}
            bool lower=n.named&&target-n.pitch>=contrast&&lowSupport>=t.exit;
            // A high, stable landing can finish an ascent a little below its crest. This is
            // continuity evidence, not a new emphasis entry or a tonic shortcut.
            bool settledHigh=n.named&&analysis.pitched&&background.valid&&n.pitch>=analysis.q75&&n.pitch>=background.median+contrast&&goalSupport>=t.support+t.exit&&distance&&net>=0;
            bool backgroundHeld=s.continuity&&(compatibleMotif||settledHigh||(elevated&&next.median>=target-near));
            bool held=current=="emphasis"&&n.x-since<t.emphasis;
            QString wanted=current;Message reason="N0: insufficient structural evidence";bool isCustom=false;
            if(a1||a2||a3){wanted="emphasis";reason=a2?"A2: repeated high-register motif":a3?"A3: elevated section and sustained activity":"A1: arrival supported by sounding notes";}
            else if(current=="emphasis"){
                if(held)reason="H1: minimum emphasis hold";
                else if(s.continuity&&(same||returns||backgroundHeld))reason=compatibleMotif?"H3: established motif":returns?"H2: brief turn returns":"H1: sustained target";
                else if(lower||!s.continuity){wanted="normal";reason="X1: confirmed departure from emphasis target";falling=true;}
                else reason="H3: no confirmed departure";
            }
            bool wider=i-first>=3&&background.ioi>0&&n.x-v[i-1].x>=s.wideningRatio*background.ioi&&v[i-1].x-v[i-2].x>=s.wideningRatio*background.ioi;
            bool release=s.release&&current!="emphasis"&&!a1&&!a2&&!a3&&!compatibleMotif&&n.ending&&background.valid&&local.coverage>=.9&&local.median<=background.median-contrast&&local.sounding>=t.release&&wider;
            if(release){wanted="relaxed";reason="R1: sustained descent + two widened onsets + ending";}
            else if(current=="relaxed"&&!a1&&!a2&&!a3&&(!n.named||std::abs(n.pitch-target)>near)){wanted="normal";reason="X1: release context ended";}
            for(const auto &r:rules){if(!r.enabled||(!r.track.isEmpty()&&r.track!=n.track))continue;double minimum=music?r.minBeats:r.minSeconds;if(n.y-n.x<minimum)continue;
                auto c=r.condition;double threshold=c=="long"&&!music?r.thresholdSeconds:diatonic&&QStringList{"leap","up","down"}.contains(c)?r.thresholdDegrees-1:r.threshold;
                bool match=c=="leap"?previous&&std::abs(delta)>=threshold:c=="up"?distance&&net>=threshold:c=="down"?distance&&-net>=threshold:c=="long"?n.y-n.x>=threshold:c=="degree"?n.degree>0&&n.degree==int(threshold):c=="tonic"?n.degree==1:c=="outside"?!n.tone.isEmpty()&&n.degree==0:c=="strong"?strongBeat(p,n.a):c=="high"?n.note->pitch>=threshold:false;
                if(match){wanted=r.target;reason=Message::format("User rule: %1",{c});isCustom=true;break;}}
            double switchAt=n.x;QString prefixTarget;
            if(wanted!=current&&!reset&&n.x-since<(current=="emphasis"?t.emphasis:t.minimum)){
                const double earliest=since+(current=="emphasis"?t.emphasis:t.minimum);
                if(earliest<n.y-1e-9){switchAt=earliest;prefixTarget=current;}
                else{wanted=current;reason="H1: minimum automatic hold";}
            }
            if(wanted!=current){current=wanted;since=switchAt;if(current!="normal")target=a2?motifGoal:a3?next.median:n.pitch;}
            else if((a2||a3||isCustom||(a1&&!same))&&current=="emphasis")target=a2?motifGoal:a3?next.median:n.pitch;
            if(current=="emphasis"&&(a1||a2||a3))falling=false;
            QVector<Message> reasons{reason};
            if(n.breath&&current=="emphasis")reasons.append(Message("H4: compatible short breath"));
            if(!n.named)reasons.append(Message("Unresolved written interval"));
            if(!n.tone.isEmpty())reasons.append(Message::format("%1 degree %2",{n.tone,QString::number(n.degree)}));
            if(!background.valid)reasons.append(Message("Limited preceding background"));
            if(analysis.degraded)reasons.append(Message("Bounded analysis limit reached"));
            PerformanceSegment segment;segment.id="auto:"+n.note->id+":"+QString::number(n.a);segment.track=n.track;segment.firstSource=segment.lastSource=n.note->id;segment.startBlick=n.a;segment.endBlick=n.b;segment.start=n.start+p.output.syncOffset;segment.end=n.end+p.output.syncOffset;segment.targets={current};segment.protect=false;segment.reasons=reasons;
            if(carryGap&&!result.segments.isEmpty()){auto &previous=result.segments.last();previous.endBlick=n.a;previous.end=n.start+p.output.syncOffset;}
            if(!prefixTarget.isEmpty()){
                const qint64 tick=music?qRound64(switchAt*Blick):p.score.time.blicks(switchAt);
                auto prefix=segment;prefix.endBlick=tick;prefix.end=p.score.time.seconds(tick)+p.output.syncOffset;prefix.targets={prefixTarget};prefix.reasons={Message("H1: minimum automatic hold before confirmed change")};
                if(!result.segments.isEmpty()&&!reset&&result.segments.last().targets==prefix.targets&&result.segments.last().track==prefix.track){auto &last=result.segments.last();last.endBlick=prefix.endBlick;last.end=prefix.end;last.lastSource=prefix.lastSource;}
                else if(prefix.endBlick>prefix.startBlick)result.segments.append(prefix);
                segment.startBlick=tick;segment.start=prefix.end;segment.id="auto:"+n.note->id+":"+QString::number(tick);
            }
            if(!result.segments.isEmpty()&&!reset&&result.segments.last().targets==segment.targets&&result.segments.last().track==segment.track){auto &back=result.segments.last();back.endBlick=n.b;back.end=segment.end;back.lastSource=n.note->id;for(const auto &item:reasons)if(!back.reasons.contains(item)&&back.reasons.size()<96)back.reasons.append(item);}
            else result.segments.append(segment);
        }
        check();if(report)report(100);
    }catch(const std::exception &e){result.cancelled=cancel&&cancel->load();result.error=result.cancelled?Message{}:exceptionMessage(e);result.segments.clear();result.suggestions.clear();}return result;
}
}
