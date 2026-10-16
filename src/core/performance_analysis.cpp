// SPDX-License-Identifier: GPL-3.0-or-later
#include "performance_analysis.h"
#include "intervals.h"
#include "source_time.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace chosuta {
namespace {
AnalysisSettings tonalAt(const Project &p,qint64 tick){auto a=p.performance.analysis;
    auto it=std::upper_bound(p.performance.tonalities.cbegin(),p.performance.tonalities.cend(),tick,[](qint64 t,const TonalityRegion &k){return t<k.start;});
    if(it!=p.performance.tonalities.cbegin()){--it;if(tick<it->end){a.tonalityEnabled=!it->key.isEmpty();if(auto key=parseTonality(it->key)){a.degreeTonic=key->first;a.degreeMode=key->second;}}}return a;
}
}
PerformanceAnalysis analyzePerformance(const Project &p,const std::atomic_bool *cancel,Progress report){
    auto check=[&]{if(cancel&&cancel->load())throw Failure("Cancelled");};
    PerformanceAnalysis out;const auto &s=p.performance.settings;const auto &t=s.timing();bool music=s.timeBasis==PerformanceTimeBasis::Music,diatonic=p.performance.analysis.intervalUnit=="diatonic";
    check();if(!p.score.hasMusic)return out;
    QHash<QString,const Note *> notes;for(const auto &track:p.score.tracks)for(const auto &n:track.notes)notes[n.id]=&n;
    bool breath=false;
    for(const auto &source:routeSources(p,cancel)){check();const auto &n=*notes[source.source];const qint64 at=source.start;
        auto word=n.phonemes.trimmed().isEmpty()?n.lyrics.trimmed():n.phonemes.trimmed();
        if(word=="br"){breath=true;continue;}if(word=="cl"||word=="っ"||word=="ッ"||word.isEmpty())continue;
        qint64 end=source.end;
        if(!out.notes.isEmpty()&&out.notes.last().note==&n&&out.notes.last().b==at){auto &last=out.notes.last();last.b=end;last.end=p.score.time.seconds(end);last.y=music?double(end)/Blick:last.end;continue;}
        PerformanceInput v;v.note=&n;v.track=source.track;v.a=at;v.b=end;v.start=p.score.time.seconds(at);v.end=p.score.time.seconds(end);v.x=music?double(at)/Blick:v.start;v.y=music?double(end)/Blick:v.end;v.breath=breath;breath=false;
        const auto tone=tonalAt(p,at);v.tone=tone.tonalityEnabled?tone.degreeTonic+" "+tone.degreeMode:QString{};
        auto written=(tone.tonalityEnabled||tone.noteSpellings.contains(n.id))?writtenPitch(tone,n):std::nullopt;
        v.named=!diatonic||written.has_value();v.pitch=diatonic?(written?written->staffPosition:0):n.pitch;
        if(tone.tonalityEnabled){auto root=parseWrittenPitch(tone.degreeTonic+"4");auto scale=tone.degreeMode=="custom"?tone.customDegreeScale:degreeModes().value(tone.degreeMode);if(root){auto pc=(n.pitch-root->midi+120)%12;v.degree=scale.indexOf(pc)+1;if(written){int pos=(written->staffPosition-root->staffPosition+140)%7;v.degree=scale.value(pos,-1)==pc?pos+1:0;}}}
        out.notes.append(v);
    }
    QHash<QString,QString> boundaries;for(auto b:p.performance.boundaries)boundaries[b.source]=b.mode;
    QMap<double,double> histogram;double total=0,named=0;
    for(int i=0;i<out.notes.size();++i){check();auto &n=out.notes[i];auto mode=boundaries.value(n.note->id);bool firstInstance=i==0||out.notes[i-1].note!=n.note;
        double gap=i?n.x-out.notes[i-1].y:0;bool track=i&&n.track!=out.notes[i-1].track;
        n.hard=i==0||gap>=t.reset||track;
        n.phrase=n.hard||n.breath||gap>=t.phrase;
        if(firstInstance){if(mode=="section-join"){n.joined=true;n.hard=i==0;n.phrase=n.hard;}else if(mode=="join"&&!track&&gap<t.reset)n.phrase=n.hard;
            if(mode=="break"){n.phrase=true;}if(mode=="section-break"){n.hard=true;n.phrase=true;}}
        if(i){out.notes[i-1].ending=n.phrase;if(n.phrase)out.suggestions.append({n.note->id,n.hard?"section-break":"break"});}
        total+=n.y-n.x;if(n.named){named+=n.y-n.x;histogram[n.pitch]+=n.y-n.x;}
    }
    if(!out.notes.isEmpty())out.notes.last().ending=true;
    auto quantile=[&](double q){double accumulated=0;for(auto it=histogram.cbegin();it!=histogram.cend();++it){accumulated+=it.value();if(accumulated>=q*named)return it.key();}return histogram.isEmpty()?0:histogram.lastKey();};
    out.pitched=total>0&&named>=.9*total;out.q10=quantile(.1);out.q50=quantile(.5);out.q75=quantile(.75);out.q90=quantile(.9);
    out.motifEnd.fill(-1,out.notes.size());
    // Index exact relative pitch contours, then verify unquantized rhythm. Bounded per lookup.
    if(s.motif){QHash<QString,QVector<int>> index;const double range=music?128:64;
        for(int i=0;i<out.notes.size();++i){check();if(report&&i%256==0)report(10+i*25/std::max(1,int(out.notes.size())));
            QString key;for(int length=1;length<=12&&i+length<=out.notes.size();++length){const auto &n=out.notes[i+length-1];if(!n.named||(length>1&&(n.hard||n.track!=out.notes[i].track||(diatonic&&n.tone!=out.notes[i].tone))))break;
                key+=QString::number(n.pitch-out.notes[i].pitch)+",";if(length<4)continue;
                auto &candidates=index[key];int checks=0;
                for(int k=int(candidates.size())-1;k>=0;--k){int j=candidates[k];if(out.notes[i].x-out.notes[j].x>range)break;if(j+length>i)continue;if(++checks>32){out.degraded=true;break;}
                    if(out.notes[j].track!=out.notes[i].track||(diatonic&&out.notes[j].tone!=out.notes[i].tone))continue;
                    double span=out.notes[i+length-1].y-out.notes[i].x,other=out.notes[j+length-1].y-out.notes[j].x;if(span<=0||other<=0)continue;bool match=true;
                    for(int r=0;r<length&&match;++r){auto &a=out.notes[i+r];auto &b=out.notes[j+r];double da=(a.y-a.x)/span,db=(b.y-b.x)/other;
                        if(std::abs(da-db)>s.motifTolerance*std::max(da,db)+1e-9)match=false;
                        if(r){da=(a.x-out.notes[i+r-1].x)/span;db=(b.x-out.notes[j+r-1].x)/other;if(std::abs(da-db)>s.motifTolerance*std::max(da,db)+1e-9)match=false;}}
                    if(match){out.motifEnd[i]=std::max(out.motifEnd[i],i+length-1);out.motifEnd[j]=std::max(out.motifEnd[j],j+length-1);break;}
                }
                candidates.append(i);if(candidates.size()>64){candidates.remove(0,candidates.size()-64);out.degraded=true;}
            }
        }
        for(int i=0;i<out.motifEnd.size();++i)if(out.motifEnd[i]>=0)out.suggestions.append({out.notes[i].note->id,"motif"});
    }
    return out;
}
QJsonObject performanceAnalysisJson(const Project &p,const std::atomic_bool *cancel){
    validatePerformance(p);auto a=analyzePerformance(p,cancel,{});QJsonArray notes;
    for(int i=0;i<a.notes.size();++i){const auto &n=a.notes[i];notes.append(QJsonObject{{"source",n.note->id},{"track",n.track},{"startBlick",QString::number(n.a)},{"endBlick",QString::number(n.b)},{"position",p.score.time.beatLabel(n.a)},{"midi",n.note->pitch},{"coordinate",n.named?QJsonValue(n.pitch):QJsonValue()},{"tonality",n.tone},{"degree",n.degree},{"phrase",n.phrase},{"section",n.hard},{"ending",n.ending},{"motifEndIndex",a.motifEnd[i]}});}
    return {{"available",p.score.hasMusic},{"algorithm","performance-v2.0"},{"basisHash",performanceBasisHash(p,cancel)},{"timeBasis",p.performance.settings.timeBasis==PerformanceTimeBasis::Music?"music":"seconds"},{"distanceBasis",p.performance.analysis.intervalUnit},{"boundedMotifSearch",a.degraded},{"pitchCoverageSufficient",a.pitched},{"q10",a.q10},{"q50",a.q50},{"q75",a.q75},{"q90",a.q90},{"notes",notes}};
}

}
