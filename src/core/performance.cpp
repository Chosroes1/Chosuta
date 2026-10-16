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
    add("performance-v2.0");add(QByteArray::number(p.rules.harmonyTakeover));add(p.score.hash.toUtf8());auto appearance=analysisSettingsJson(p.performance.analysis);add(QJsonDocument(appearance).toJson(QJsonDocument::Compact));
    auto settings=performanceSettingsJson(p.performance.settings);settings.remove(p.performance.settings.timeBasis==PerformanceTimeBasis::Music?"seconds":"music");
    const bool degrees=p.performance.analysis.intervalUnit=="diatonic";for(const auto &prefix:{"leap","progression","near","contrast"})settings.remove(QString(prefix)+(degrees?"Semitones":"Degrees"));
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
PerformanceResolver::PerformanceResolver(const Project &p,bool buildPlan):project(p),sourceTimes(project){
    for(const auto &track:project.score.tracks)for(const auto &n:track.notes){notes[n.id]=&n;sourceTracks[n.id]=track.id;}
    for(const auto &slice:routeSources(project))routes[slice.track].append(slice);
    diagnostics=sourceTimes.diagnostics;
    if(!buildPlan)return;
    struct Candidate {PerformanceSlice slice;};
    QVector<Candidate> candidates;
    auto append=[&](const PerformanceSegment &s,bool manual){
        const auto v=interval(s);
        if(v.orphan){diagnostics<<Message::format("Performance source missing; retained interval: %1",{s.id}); if(!manual)return;}
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
            PerformanceSlice x{time(boundaries[i],false),time(boundaries[i+1],true),groupFor(p,s.targets[slot]),s.id,s.reasons,manual,s.protect,v.orphan};
            if(x.end>x.start)candidates.append({x});
        }
    };
    for(const auto &s:p.performance.generated)append(s,false);
    // Manual entries win even when unprotected; unprotecting permits the next generation to replace them.
    for(const auto &s:p.performance.manual)append(s,true);
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
    const auto found=routes.constFind(track);
    if(found!=routes.cend()){
        const auto &list=*found;auto it=std::upper_bound(list.cbegin(),list.cend(),tick,[](qint64 t,const SourceSlice &s){return t<s.start;});
        if(end&&it!=list.cbegin()&&(it-1)->start==tick)--it;
        if(it!=list.cbegin()){--it;if(tick>=it->start&&tick<=it->end){const auto time=sourceTimes.forward(it->source,raw,end);if(time)return *time+project.output.syncOffset;}}
    }
    return raw+project.output.syncOffset;
}
SubtitleInterval PerformanceResolver::interval(const PerformanceSegment &s)const{
    if(!project.score.hasMusic)return {s.start,s.end,s.anchor!="seconds"};
    const bool orphan=sourceTimes.ambiguous(s.firstSource,project.score.time.seconds(s.startBlick))||sourceTimes.ambiguous(s.lastSource,project.score.time.seconds(s.endBlick),true)||(!s.firstSource.isEmpty()&&(!notes.contains(s.firstSource)||!sourceTimes.contains(s.firstSource)))||(!s.lastSource.isEmpty()&&(!notes.contains(s.lastSource)||!sourceTimes.contains(s.lastSource)));
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
    if(!p.score.hasMusic){s.anchor="seconds";s.startBlick=s.endBlick=0;s.track.clear();s.firstSource.clear();s.lastSource.clear();return;}
    s.startBlick=p.score.time.blicks(a-p.output.syncOffset);s.endBlick=p.score.time.blicks(b-p.output.syncOffset);
    s.track.clear();s.firstSource.clear();s.lastSource.clear();
    // Select actual routed sources; map corrected boundaries back to the original score.
    const auto events=resolveEvents(p.effective());QHash<QString,const Note *> notes;
    for(const auto &t:p.score.tracks)for(const auto &n:t.notes)notes[n.id]=&n;
    SourceTimeMap mapping(p);
    for(const auto &e:events)if(e.end+p.output.syncOffset>a&&e.start+p.output.syncOffset<b&&notes.contains(e.source)){
        if(s.firstSource.isEmpty()){s.firstSource=e.source;s.track=e.track;}
        s.lastSource=e.source;
    }
    auto inverse=[&](const QString &id,double time,bool end)->std::optional<qint64>{
        auto raw=mapping.inverse(id,time-p.output.syncOffset,end);return raw?std::optional<qint64>(p.score.time.blicks(*raw)):std::nullopt;
    };
    if(beats){const auto x=inverse(s.firstSource,a,false),y=inverse(s.lastSource,b,true);
        if(x&&y){s.startBlick=*x;s.endBlick=*y;}else s.anchor="seconds";
    }
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
    QMap<QString,QString> members;
    for(const auto &slot:mouthStates())members[variantAssetId(id,slot)]=variantAssetId(replacement,slot);
    auto remap=[&](QString &reference){auto it=members.constFind(reference);if(it!=members.cend())reference=*it;};
    remap(p.fallback);for(auto &policy:p.rules.special)remap(policy.shape);
    for(auto &e:p.generated)remap(e.shape);
    for(auto &o:p.overrides)remap(o.event.shape);
    // Arbitrary custom IDs (including slash-containing IDs) are not group slots.
    for(auto it=members.cbegin();it!=members.cend();++it)p.assets.remove(it.key());
}
}
