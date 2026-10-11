// SPDX-License-Identifier: GPL-3.0-or-later
#include "performance.h"
#include "appearance.h"
#include "intervals.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace chosuta {
QString variantAssetId(const QString &group,const QString &shape){return group=="base"?shape:group+"/"+shape;}
QString variantGroupName(const Project &p,const QString &id){for(const auto &g:p.performance.groups)if(g.id==id)return g.name;return id;}
std::optional<QPair<QString,QString>> parseTonality(QString text){
    text=text.trimmed().replace(QChar(0x266f),'#').replace(QChar(0x266d),'b');
    static const QRegularExpression rx("^([A-Ga-g](?:#{1,3}|b{1,3})?)\\s+([A-Za-z][A-Za-z -]*)$");
    const auto m=rx.match(text);if(!m.hasMatch())return std::nullopt;
    QString tonic=m.captured(1);tonic[0]=tonic[0].toUpper();
    QString mode=m.captured(2).trimmed().toLower().replace(' ','-');
    if(mode=="maj"||mode=="ionian")mode="major";
    if(mode=="min"||mode=="aeolian"||mode=="natural-minor")mode="minor";
    if(!parseWrittenPitch(tonic+"4")||(!degreeModes().contains(mode)&&mode!="custom"))return std::nullopt;
    return QPair{tonic,mode};
}
QString performanceBasisHash(const Project &p,const std::atomic_bool *cancel){
    QCryptographicHash h(QCryptographicHash::Sha256);
    auto add=[&](const QByteArray &b){h.addData(QByteArray::number(b.size())+':');h.addData(b);};
    add("performance-v2.0");add(QByteArray::number(p.rules.harmonyTakeover));add(p.score.hash.toUtf8());auto appearance=appearanceJson(p.appearance);appearance.remove("enabled");add(QJsonDocument(appearance).toJson(QJsonDocument::Compact));
    auto settings=performanceSettingsJson(p.performance.settings);settings.remove(p.performance.settings.timeBasis==PerformanceTimeBasis::Music?"seconds":"music");
    const bool degrees=p.appearance.intervalUnit=="diatonic";for(const auto &prefix:{"leap","progression","near","contrast"})settings.remove(QString(prefix)+(degrees?"Semitones":"Degrees"));
    auto rules=settings["rules"].toArray();for(int i=0;i<rules.size();++i){auto rule=rules[i].toObject();auto condition=rule["condition"].toString();bool seconds=p.performance.settings.timeBasis==PerformanceTimeBasis::Seconds;
        rule.remove(seconds?"minBeats":"minSeconds");if(condition=="long"){rule.remove(seconds?"threshold":"thresholdSeconds");rule.remove("thresholdDegrees");}else{rule.remove("thresholdSeconds");if(QStringList{"leap","up","down"}.contains(condition))rule.remove(degrees?"threshold":"thresholdDegrees");else rule.remove("thresholdDegrees");}rules[i]=rule;}settings["rules"]=rules;
    add(QJsonDocument(settings).toJson(QJsonDocument::Compact));
    for(const auto &t:p.score.tracks)for(const auto &n:t.notes){if(cancel&&cancel->load())throw Failure("Cancelled");add(t.id.toUtf8());add(QByteArray::number(t.muted));add(QByteArray::number(n.muted));add(n.phonemes.toUtf8());add(n.lyrics.toUtf8());add(n.id.toUtf8());add(n.groupInstance.toUtf8());add(QByteArray::number(n.onset));add(QByteArray::number(n.duration));add(QByteArray::number(n.pitch));}
    // Image mappings, timing correction, playback mode and manual performance are not analysis inputs.
    for(const auto &id:p.selected){add(id.toUtf8());add(QByteArray::number(p.priorities.value(id)));}
    for(const auto &b:p.performance.boundaries){add(b.source.toUtf8());add(b.mode.toUtf8());}
    for(const auto &k:p.performance.tonalities){add(QByteArray::number(k.start));add(QByteArray::number(k.end));add(k.key.toUtf8());}
    return QString::fromLatin1(h.result().toHex());
}
bool performanceCurrent(const Project &p){return !p.performance.basisHash.isEmpty()&&p.performance.basisHash==performanceBasisHash(p);}
static QString groupFor(const Project &p,const QString &target){return target.startsWith("group:")?target.mid(6):p.performance.roles.value(target,"base");}
PerformanceResolver::PerformanceResolver(const Project &p,bool buildPlan):project(p){
    for(const auto &track:project.score.tracks)for(const auto &n:track.notes){notes[n.id]=&n;sourceTracks[n.id]=track.id;tracks[track.id].append(&n);}
    for(auto &list:tracks)std::sort(list.begin(),list.end(),[](auto a,auto b){return a->onset<b->onset;});
    for(const auto &e:resolveEvents(project.effective())){
        auto it=effectiveTimes.find(e.source);
        if(it==effectiveTimes.end())effectiveTimes[e.source]={e.start,e.end};
        else {it->first=std::min(it->first,e.start);it->second=std::max(it->second,e.end);}
    }
    if(!buildPlan)return;
    struct Candidate {PerformanceSlice slice;int priority;};
    QVector<Candidate> candidates;
    auto append=[&](const PerformanceSegment &s,bool manual,int priority){
        const auto v=interval(s);
        if(v.orphan){diagnostics<<"Performance source missing; retained interval: "+s.id; if(!manual)return;}
        QVector<double> boundaries={s.phaseStart};
        for(double cut:s.cuts)if(cut>s.phaseStart&&cut<s.phaseEnd)boundaries.append(cut);
        boundaries.append(s.phaseEnd);
        auto time=[&](double phase,bool end){
            const double ratio=(phase-s.phaseStart)/(s.phaseEnd-s.phaseStart);
            if(ratio==0)return v.start;
            if(ratio==1)return v.end;
            if(s.anchor=="beats"&&!v.orphan)return mappedBlick(s.startBlick+qRound64(double(s.endBlick-s.startBlick)*ratio),s.track,end);
            return v.start+(v.end-v.start)*ratio;
        };
        for(int i=0;i+1<boundaries.size();++i){
            const double phase=(boundaries[i]+boundaries[i+1])/2;
            const int slot=int(std::upper_bound(s.cuts.cbegin(),s.cuts.cend(),phase)-s.cuts.cbegin());
            PerformanceSlice x{time(boundaries[i],false),time(boundaries[i+1],true),groupFor(p,s.targets[slot]),s.id,s.reason,manual,s.protect,v.orphan};
            if(x.end>x.start)candidates.append({x,priority});
        }
    };
    int priority=0;
    for(const auto &s:p.performance.generated)append(s,false,priority++);
    // Manual entries win even when unprotected; unprotecting permits the next generation to replace them.
    for(const auto &s:p.performance.manual)append(s,true,priority++);
    struct Edge {double time;int index;bool start;};QVector<Edge> edges;
    for(int i=0;i<candidates.size();++i){edges.append({candidates[i].slice.start,i,true});edges.append({candidates[i].slice.end,i,false});}
    std::sort(edges.begin(),edges.end(),[](const Edge &a,const Edge &b){return a.time==b.time?a.start<b.start:a.time<b.time;});
    std::set<int> active;
    for(int i=0;i<edges.size();){const double t=edges[i].time;while(i<edges.size()&&edges[i].time==t){auto e=edges[i++];if(e.start)active.insert(e.index);else active.erase(e.index);}
        if(i==edges.size()||active.empty()||edges[i].time<=t)continue;
        auto x=candidates[*active.rbegin()].slice;x.start=t;x.end=edges[i].time;
        if(!plan.isEmpty()&&plan.back().end==x.start&&plan.back().id==x.id&&plan.back().group==x.group)plan.back().end=x.end;
        else plan.append(x);
    }
}
double PerformanceResolver::mappedBlick(qint64 tick,const QString &track,bool end)const{
    const double raw=project.score.time.seconds(tick);
    auto map=[&](const QVector<const Note *> &list)->std::optional<double>{
        auto it=std::upper_bound(list.cbegin(),list.cend(),tick,[](qint64 t,const Note *n){return t<n->onset;});
        // At shared boundaries the ending interval belongs to the previous note.
        if(end&&it!=list.cbegin()&&(*(it-1))->onset==tick)--it;
        if(it==list.cbegin())return std::nullopt;
        const Note &n=**(--it);
        if(tick<n.onset||tick>n.onset+n.duration||!effectiveTimes.contains(n.id))return std::nullopt;
        const auto v=effectiveTimes.value(n.id);const double a=project.score.time.seconds(n.onset),b=project.score.time.seconds(n.onset+n.duration);
        return v.first+(raw-a)/(b-a)*(v.second-v.first);
    };
    if(!track.isEmpty()){auto t=map(tracks.value(track));if(t)return *t+project.output.syncOffset;}
    return raw+project.output.syncOffset;
}
SubtitleInterval PerformanceResolver::interval(const PerformanceSegment &s)const{
    const bool orphan=(!s.firstSource.isEmpty()&&(!notes.contains(s.firstSource)||!effectiveTimes.contains(s.firstSource)))||(!s.lastSource.isEmpty()&&(!notes.contains(s.lastSource)||!effectiveTimes.contains(s.lastSource)));
    if(s.anchor=="seconds"||orphan)return {s.start,s.end,orphan};
    double a=mappedBlick(s.startBlick,sourceTracks.value(s.firstSource,s.track)),b=mappedBlick(s.endBlick,sourceTracks.value(s.lastSource,s.track),true);
    return b>a?SubtitleInterval{a,b,false}:SubtitleInterval{s.start,s.end,true};
}
const PerformanceSlice *PerformanceResolver::at(double time)const{
    auto it=std::upper_bound(plan.cbegin(),plan.cend(),time,[](double t,const auto &s){return t<s.start;});
    if(it==plan.cbegin())return nullptr;
    --it;return time<it->end?&*it:nullptr;
}
QString PerformanceResolver::groupAt(double time)const{const auto s=at(time);return s?s->group:project.performance.roles.value("normal","base");}
void setPerformanceTime(const Project &p,PerformanceSegment &s,double a,double b,bool beats){
    if(!std::isfinite(a)||!std::isfinite(b)||a<0||b<=a||b>21600)throw Failure("Invalid performance interval");
    s.start=a;s.end=b;s.anchor=beats?"beats":"seconds";
    s.startBlick=p.score.time.blicks(a-p.output.syncOffset);s.endBlick=p.score.time.blicks(b-p.output.syncOffset);
    s.track.clear();s.firstSource.clear();s.lastSource.clear();
    // Select actual routed sources; map corrected boundaries back to the original score.
    const auto events=resolveEvents(p.effective());QHash<QString,const Note *> notes;
    for(const auto &t:p.score.tracks)for(const auto &n:t.notes)notes[n.id]=&n;
    QHash<QString,QPair<double,double>> times;
    for(const auto &e:events){if(!times.contains(e.source))times[e.source]={e.start,e.end};else{times[e.source].first=std::min(times[e.source].first,e.start);times[e.source].second=std::max(times[e.source].second,e.end);}}
    for(const auto &e:events)if(e.end+p.output.syncOffset>a&&e.start+p.output.syncOffset<b&&notes.contains(e.source)){
        if(s.firstSource.isEmpty()){s.firstSource=e.source;s.track=e.track;}
        s.lastSource=e.source;
    }
    auto inverse=[&](const QString &id,double time,qint64 fallback){
        if(!notes.contains(id)||!times.contains(id))return fallback;
        const auto v=times.value(id);const double t=time-p.output.syncOffset;
        if(t<v.first||t>v.second||v.second<=v.first)return fallback;
        const auto n=notes[id];const double x=p.score.time.seconds(n->onset),y=p.score.time.seconds(n->onset+n->duration);
        return p.score.time.blicks(x+(t-v.first)/(v.second-v.first)*(y-x));
    };
    if(beats){s.startBlick=inverse(s.firstSource,a,s.startBlick);s.endBlick=inverse(s.lastSource,b,s.endBlick);}
}
static PerformanceSegment crop(const Project &p,const PerformanceResolver &resolver,const PerformanceSegment &s,double a,double b){
    auto result=s;const auto v=resolver.interval(s);
    if(a<=v.start&&b>=v.end)return result;
    result.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    setPerformanceTime(p,result,a,b,s.anchor=="beats");
    const double left=s.anchor=="beats"?double(result.startBlick-s.startBlick)/double(s.endBlick-s.startBlick):(a-v.start)/(v.end-v.start);
    const double right=s.anchor=="beats"?double(result.endBlick-s.startBlick)/double(s.endBlick-s.startBlick):(b-v.start)/(v.end-v.start);
    result.phaseStart=s.phaseStart+(s.phaseEnd-s.phaseStart)*std::clamp(left,0.,1.);
    result.phaseEnd=s.phaseStart+(s.phaseEnd-s.phaseStart)*std::clamp(right,0.,1.);
    return result;
}
static QVector<PerformanceSegment> subtract(const Project &p,const PerformanceResolver &resolver,const QVector<PerformanceSegment> &list,double a,double b,bool protectedToo){
    QVector<PerformanceSegment> out;
    for(const auto &s:list){const auto v=resolver.interval(s);
        if((s.protect&&!protectedToo)||v.end<=a||v.start>=b){out.append(s);continue;}
        if(v.start<a)out.append(crop(p,resolver,s,v.start,a));
        if(v.end>b)out.append(crop(p,resolver,s,b,v.end));
    }return out;
}
void putPerformanceSegment(Project &p,PerformanceSegment s){
    if(s.id.isEmpty())s.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
    Project check=p;check.performance.generated.clear();check.performance.manual={s};validatePerformance(check);
    // Moving an existing entry removes its old location first.
    for(int i=int(p.performance.manual.size())-1;i>=0;--i)if(p.performance.manual[i].id==s.id)p.performance.manual.removeAt(i);
    PerformanceResolver resolver(p,false);const auto v=resolver.interval(s);
    p.performance.manual=subtract(p,resolver,p.performance.manual,v.start,v.end,true);
    p.performance.manual.append(s);validatePerformance(p);
}
void erasePerformanceRange(Project &p,double a,double b,bool protectedToo){if(!std::isfinite(a)||!std::isfinite(b)||b<=a)throw Failure("Invalid performance range");PerformanceResolver resolver(p,false);p.performance.manual=subtract(p,resolver,p.performance.manual,a,b,protectedToo);}
void applyPerformanceDraft(Project &p,const PerformanceDraft &draft,double a,double b,bool restore){
    if(draft.cancelled||!draft.error.isEmpty())throw Failure("Cannot apply failed performance draft");
    if(draft.basisHash!=performanceBasisHash(p))throw Failure("Performance draft is stale; generate again");
    if(!std::isfinite(a)||!std::isfinite(b)||(a>=0&&b<=a))throw Failure("Invalid performance range");
    Project check=p;check.performance.generated=draft.segments;validatePerformance(check);
    PerformanceResolver resolver(p,false);
    if(a<0){p.performance.suggestions=draft.suggestions;p.performance.generated=draft.segments;a=-864000;b=864000;p.performance.basisHash=draft.basisHash;}
    else {
        p.performance.generated=subtract(p,resolver,p.performance.generated,a,b,true);
        for(const auto &s:draft.segments){const auto v=resolver.interval(s);if(v.end>a&&v.start<b)p.performance.generated.append(crop(p,resolver,s,std::max(a,v.start),std::min(b,v.end)));}
        // A local regeneration must not falsely mark untouched old regions current.
        if(p.performance.basisHash==draft.basisHash)p.performance.basisHash=draft.basisHash;
    }
    p.performance.manual=subtract(p,resolver,p.performance.manual,a,b,restore);
    validatePerformance(p);
}
void splitPerformance(Project &p,const QString &id,double time){
    PerformanceResolver resolver(p,false);
    for(int i=0;i<p.performance.manual.size();++i)if(p.performance.manual[i].id==id){const auto s=p.performance.manual[i];const auto v=resolver.interval(s);
        if(time<=v.start||time>=v.end)throw Failure("Split must be inside performance interval");
        p.performance.manual.removeAt(i);p.performance.manual.append(crop(p,resolver,s,v.start,time));p.performance.manual.append(crop(p,resolver,s,time,v.end));return;}
    throw Failure("Select a manual performance interval");
}
void removeVariantGroup(Project &p,const QString &id,const QString &replacement){
    if(id=="base"||id==replacement)throw Failure("Cannot remove base group or replace a group with itself");
    bool found=false;for(const auto &g:p.performance.groups)if(g.id==replacement)found=true;if(!found)throw Failure("Unknown replacement group");
    for(int i=int(p.performance.groups.size())-1;i>=0;--i)if(p.performance.groups[i].id==id)p.performance.groups.removeAt(i);
    for(auto &value:p.performance.roles)if(value==id)value=replacement;
    for(auto *list:{&p.performance.generated,&p.performance.manual})for(auto &s:*list)for(auto &v:s.targets)if(v=="group:"+id)v="group:"+replacement;
    for(auto &rule:p.performance.settings.rules)if(rule.target=="group:"+id)rule.target="group:"+replacement;
    for(auto it=p.assets.begin();it!=p.assets.end();)if(it.key().startsWith(id+"/"))it=p.assets.erase(it);else ++it;
}
}
