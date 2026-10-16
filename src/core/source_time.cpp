// SPDX-License-Identifier: GPL-3.0-or-later
#include "source_time.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace chosuta {
QVector<TimedSourceSlice> routeTimedSources(const Project &p,const std::atomic_bool *cancel){
    struct Source{const TimedPhone *phone;QString track;int priority,order;};QVector<Source> sources;
    struct Edge{double at;int source;bool start;};QVector<Edge> edges;int primary=-1;
    for(int ti=0;ti<p.score.tracks.size();++ti){const auto &t=p.score.tracks[ti];if(t.muted||!p.selected.contains(t.id))continue;
        if(primary<0||p.priorities.value(t.id,ti)<p.priorities.value(p.score.tracks[primary].id,primary))primary=ti;
        for(const auto &phone:t.timedPhones){if(cancel&&cancel->load())throw Failure("Cancelled");if(phone.end<=phone.start)continue;
            int i=sources.size();sources.append({&phone,t.id,p.priorities.value(t.id,ti),ti});edges.append({phone.start,i,true});edges.append({phone.end,i,false});}}
    std::sort(edges.begin(),edges.end(),[](auto a,auto b){return a.at==b.at?a.start<b.start:a.at<b.at;});
    auto less=[&](int a,int b){const auto &x=sources[a],&y=sources[b];if(x.priority!=y.priority)return x.priority<y.priority;if(x.order!=y.order)return x.order<y.order;if(x.phone->start!=y.phone->start)return x.phone->start>y.phone->start;return x.phone->id==y.phone->id?a<b:x.phone->id<y.phone->id;};
    std::set<int,decltype(less)> active(less);QVector<TimedSourceSlice> out;
    for(int i=0;i<edges.size();){if(cancel&&cancel->load())throw Failure("Cancelled");double at=edges[i].at;
        while(i<edges.size()&&edges[i].at==at){auto edge=edges[i++];if(edge.start)active.insert(edge.source);else active.erase(edge.source);}
        if(i==edges.size()||active.empty())continue;
        const auto &s=sources[*active.begin()];if(!p.rules.harmonyTakeover&&s.order!=primary)continue;
        if(!out.isEmpty()&&out.back().source==s.phone->id&&out.back().end==at)out.back().end=edges[i].at;
        else out.append({s.phone->id,s.track,at,edges[i].at});
    }return out;
}
QVector<SourceSlice> routeSources(const Project &p,const std::atomic_bool *cancel){
    struct Source{const Note *note;QString track;int priority,order;};QVector<Source> sources;
    struct Edge{qint64 at;int source;bool start;};QVector<Edge> edges;
    int primary=-1;
    for(int ti=0;ti<p.score.tracks.size();++ti){const auto &t=p.score.tracks[ti];if(t.muted||!p.selected.contains(t.id))continue;
        if(primary<0||p.priorities.value(t.id,ti)<p.priorities.value(p.score.tracks[primary].id,primary))primary=ti;
        for(const auto &n:t.notes)if(!n.muted&&n.duration>0){if(cancel&&cancel->load())throw Failure("Cancelled");int i=sources.size();sources.append({&n,t.id,p.priorities.value(t.id,ti),ti});edges.append({n.onset,i,true});edges.append({n.onset+n.duration,i,false});}}
    std::sort(edges.begin(),edges.end(),[](auto a,auto b){return a.at==b.at?a.start<b.start:a.at<b.at;});
    auto less=[&](int a,int b){const auto &x=sources[a],&y=sources[b];if(x.priority!=y.priority)return x.priority<y.priority;if(x.order!=y.order)return x.order<y.order;if(x.note->onset!=y.note->onset)return x.note->onset>y.note->onset;return x.note->id==y.note->id?a<b:x.note->id<y.note->id;};
    std::set<int,decltype(less)> active(less);QVector<SourceSlice> out;
    for(int i=0;i<edges.size();){if(cancel&&cancel->load())throw Failure("Cancelled");const auto at=edges[i].at;
        while(i<edges.size()&&edges[i].at==at){auto e=edges[i++];if(e.start)active.insert(e.source);else active.erase(e.source);}
        if(i==edges.size()||active.empty())continue;
        const auto &s=sources[*active.begin()];if(!p.rules.harmonyTakeover&&s.order!=primary)continue;
        if(!out.isEmpty()&&out.back().source==s.note->id&&out.back().end==at)out.back().end=edges[i].at;
        else out.append({s.note->id,s.track,at,edges[i].at});
    }return out;
}
QVector<SourceFragment> eventSourceFragments(const Project &p,const Event &e){
    const auto it=p.overrides.constFind(e.id);
    if(it!=p.overrides.cend()&&(it->standalone||!it->sourceFragments.isEmpty()))return it->sourceFragments;
    return {{e.id,0,1,0,1}};
}
SourceTimeMap::SourceTimeMap(const Project &p){
    QHash<QString,Event> originals,effective;
    for(const auto &e:p.generated)originals[e.id]=e;
    const auto events=p.effective();for(const auto &e:events)effective[e.id]=e;
    auto markAmbiguous=[&](const QStringList &parents){for(const auto &id:parents){auto it=originals.constFind(id);if(it!=originals.cend())ambiguousRanges[it->source].append({it->start,it->end});}};
    for(const auto &visible:resolveEvents(events)){
        const auto e=effective.value(visible.id);
        const auto fragments=eventSourceFragments(p,e);
        bool known=!fragments.isEmpty();QSet<QString> identities;
        for(const auto &f:fragments){auto parent=originals.constFind(f.parentId);if(parent==originals.cend())known=false;else identities.insert(parent->source);}
        if(!known||identities.size()!=1){
            diagnostics<<Message::format("Ambiguous manual source mapping: %1",{e.id});
            const auto o=p.overrides.constFind(e.id);if(o!=p.overrides.cend())markAmbiguous(o->parentIds);
            continue;
        }
        for(const auto &f:fragments){
            const auto parent=originals.value(f.parentId);
            const double start=e.start+(e.end-e.start)*f.start,end=e.start+(e.end-e.start)*f.end;
            const double a=std::max(start,visible.start),b=std::min(end,visible.end);
            if(b<=a)continue;
            const double rawStart=parent.start+(parent.end-parent.start)*f.sourceStart;
            const double rawEnd=parent.start+(parent.end-parent.start)*f.sourceEnd;
            const double factor=(rawEnd-rawStart)/(end-start);
            sources[parent.source].append({rawStart+(a-start)*factor,rawStart+(b-start)*factor,a,b});
        }
    }
}
bool SourceTimeMap::ambiguous(const QString &source,double raw,bool end)const{
    auto it=ambiguousRanges.constFind(source);if(it==ambiguousRanges.cend())return false;
    for(const auto &range:*it)if(end?(raw>range.first&&raw<=range.second):(raw>=range.first&&raw<range.second))return true;
    return false;
}
std::optional<double> SourceTimeMap::forward(const QString &source,double raw,bool end)const{
    if(ambiguous(source,raw,end))return {};
    auto it=sources.constFind(source);if(it==sources.cend())return {};
    std::optional<double> result;
    for(const auto &p:*it)if(raw>=p.rawStart-1e-9&&raw<=p.rawEnd+1e-9){
        if((end&&std::abs(raw-p.rawStart)<1e-9)||(!end&&std::abs(raw-p.rawEnd)<1e-9))continue;
        double value=p.start+(raw-p.rawStart)/(p.rawEnd-p.rawStart)*(p.end-p.start);
        if(result&&std::abs(*result-value)>1e-8)return {};
        result=value;
    }
    if(!result)for(const auto &p:*it)if(std::abs(raw-(end?p.rawEnd:p.rawStart))<1e-9)return end?p.end:p.start;
    return result;
}
std::optional<double> SourceTimeMap::inverse(const QString &source,double time,bool end)const{
    auto it=sources.constFind(source);if(it==sources.cend())return {};
    std::optional<double> result;
    for(const auto &p:*it)if(time>=p.start-1e-9&&time<=p.end+1e-9){
        if((end&&std::abs(time-p.start)<1e-9)||(!end&&std::abs(time-p.end)<1e-9))continue;
        double value=p.rawStart+(time-p.start)/(p.end-p.start)*(p.rawEnd-p.rawStart);
        if(ambiguous(source,value,end))return {};
        if(result&&std::abs(*result-value)>1e-8)return {};
        result=value;
    }
    if(!result)for(const auto &p:*it)if(std::abs(time-(end?p.end:p.start))<1e-9){const double value=end?p.rawEnd:p.rawStart;return ambiguous(source,value,end)?std::nullopt:std::optional<double>(value);}
    return result;
}
}
