// SPDX-License-Identifier: GPL-3.0-or-later
#include "performance.h"
#include <cmath>
namespace chosuta {
namespace {
QMap<QString,double *> timingNumbers(PerformanceTiming &t){return {{"window",&t.window},{"background",&t.background},{"lookahead",&t.lookahead},{"support",&t.support},{"confirm",&t.confirm},{"minimum",&t.minimum},{"emphasis",&t.emphasis},{"exit",&t.exit},{"breath",&t.breath},{"phrase",&t.phrase},{"reset",&t.reset},{"release",&t.release}};}
QMap<QString,double *> numbers(PerformanceSettings &s){return {{"efficiency",&s.efficiency},{"highShare",&s.highShare},{"motifShare",&s.motifShare},{"activityRatio",&s.activityRatio},{"wideningRatio",&s.wideningRatio},{"motifTolerance",&s.motifTolerance}};}
QMap<QString,int *> integers(PerformanceSettings &s){return {{"leapSemitones",&s.leapSemitones},{"leapDegrees",&s.leapDegrees},{"progressionSemitones",&s.progressionSemitones},{"progressionDegrees",&s.progressionDegrees},{"nearSemitones",&s.nearSemitones},{"nearDegrees",&s.nearDegrees},{"contrastSemitones",&s.contrastSemitones},{"contrastDegrees",&s.contrastDegrees}};}
QMap<QString,bool *> flags(PerformanceSettings &s){return {{"leap",&s.leap},{"progression",&s.progression},{"motif",&s.motif},{"section",&s.section},{"release",&s.release},{"continuity",&s.continuity}};}
QString str(const QJsonObject &o,const QString &k){if(!o[k].isString())throw Failure("Invalid performance text: "+k);return o[k].toString();}
double num(const QJsonObject &o,const QString &k){if(!o[k].isDouble()||!std::isfinite(o[k].toDouble()))throw Failure("Invalid performance number: "+k);return o[k].toDouble();}
bool flag(const QJsonObject &o,const QString &k){if(!o[k].isBool())throw Failure("Invalid performance flag: "+k);return o[k].toBool();}
qint64 tick(const QJsonObject &o,const QString &k){bool ok=false;auto v=str(o,k).toLongLong(&ok);if(!ok||v < -MusicLimit||v>MusicLimit)throw Failure("Invalid performance music position");return v;}
QJsonArray array(const QJsonObject &o,const QString &k){if(!o[k].isArray())throw Failure("Invalid performance list: "+k);return o[k].toArray();}
QJsonObject object(const QJsonValue &v){if(!v.isObject())throw Failure("Invalid performance object");return v.toObject();}
void validateSettings(PerformanceSettings s){
    const auto ns=numbers(s);for(auto it=ns.cbegin();it!=ns.cend();++it)if(!std::isfinite(*it.value())||*it.value()<0||*it.value()>64)throw Failure("Invalid performance setting: "+it.key());
    if(s.timeBasis!=PerformanceTimeBasis::Music&&s.timeBasis!=PerformanceTimeBasis::Seconds)throw Failure("Invalid performance time basis");
    for(auto t:{s.music,s.seconds}){for(auto v:timingNumbers(t))if(!std::isfinite(*v)||*v<=0||*v>128)throw Failure("Invalid performance duration");
        if(t.background<t.window||t.lookahead<t.support||t.reset<t.phrase||t.emphasis<t.minimum)throw Failure("Inconsistent performance timing ranges");}
    if(s.efficiency<=0||s.efficiency>1||s.highShare<=0||s.highShare>1||s.motifShare<=0||s.motifShare>1||s.motifTolerance>1||s.activityRatio<1||s.wideningRatio<1)throw Failure("Invalid performance ratio");
    const auto ints=integers(s);for(auto it=ints.cbegin();it!=ints.cend();++it)if(*it.value()<0||*it.value()>48)throw Failure("Invalid performance interval threshold");
    if(s.leapDegrees<2||s.progressionDegrees<2||s.contrastDegrees<2||s.nearDegrees<1||s.contrastSemitones<1||s.leapSemitones<1||s.progressionSemitones<1)throw Failure("Invalid performance leap threshold");
    if(s.rules.size()>1024)throw Failure("Too many performance rules");
    for(const auto &r:s.rules){for(double v:{r.thresholdSeconds,r.thresholdDegrees,r.minSeconds})if(!std::isfinite(v)||v<0||v>128)throw Failure("Invalid alternate rule threshold");if(r.thresholdDegrees<1||r.thresholdDegrees!=std::floor(r.thresholdDegrees))throw Failure("Invalid written degree threshold");if(!QStringList{"leap","up","down","long","degree","outside","tonic","strong","high"}.contains(r.condition)||!std::isfinite(r.threshold)||r.threshold<0||r.threshold>127||!std::isfinite(r.minBeats)||r.minBeats<0||r.minBeats>64||r.priority<0||r.priority>999||(r.condition=="degree"&&(r.threshold<1||r.threshold>7||r.threshold!=std::floor(r.threshold))))throw Failure("Invalid custom performance rule");}
}
}
QJsonObject performanceSettingsJson(const PerformanceSettings &value){
    validateSettings(value);auto s=value;QJsonObject o{{"timeBasis",s.timeBasis==PerformanceTimeBasis::Music?"music":"seconds"}};
    auto timingJson=[](PerformanceTiming t){QJsonObject v;auto fields=timingNumbers(t);for(auto it=fields.cbegin();it!=fields.cend();++it)v[it.key()]=*it.value();return v;};o["music"]=timingJson(s.music);o["seconds"]=timingJson(s.seconds);
    const auto ns=numbers(s);for(auto it=ns.cbegin();it!=ns.cend();++it)o[it.key()]=*it.value();
    const auto is=integers(s);for(auto it=is.cbegin();it!=is.cend();++it)o[it.key()]=*it.value();
    const auto fs=flags(s);for(auto it=fs.cbegin();it!=fs.cend();++it)o[it.key()]=*it.value();
    QJsonArray rules;for(const auto &r:s.rules)rules.append(QJsonObject{{"enabled",r.enabled},{"condition",r.condition},{"target",r.target},{"track",r.track},{"threshold",r.threshold},{"minBeats",r.minBeats},{"priority",r.priority},{"thresholdSeconds",r.thresholdSeconds},{"thresholdDegrees",r.thresholdDegrees},{"minSeconds",r.minSeconds}});o["rules"]=rules;return o;
}
PerformanceSettings readPerformanceSettings(const QJsonValue &v){
    auto o=object(v);PerformanceSettings s;auto basis=str(o,"timeBasis");if(basis!="music"&&basis!="seconds")throw Failure("Invalid performance time basis");s.timeBasis=basis=="music"?PerformanceTimeBasis::Music:PerformanceTimeBasis::Seconds;
    auto readTiming=[&](const QString &key,PerformanceTiming &t){auto fields=timingNumbers(t);auto values=object(o[key]);for(auto it=fields.cbegin();it!=fields.cend();++it)*it.value()=num(values,it.key());};readTiming("music",s.music);readTiming("seconds",s.seconds);
    const auto ns=numbers(s);for(auto it=ns.cbegin();it!=ns.cend();++it)*it.value()=num(o,it.key());
    const auto is=integers(s);for(auto it=is.cbegin();it!=is.cend();++it){double n=num(o,it.key());if(n<0||n>48||n!=int(n))throw Failure("Invalid integer performance setting");*it.value()=int(n);}
    const auto fs=flags(s);for(auto it=fs.cbegin();it!=fs.cend();++it)*it.value()=flag(o,it.key());
    for(auto item:array(o,"rules")){auto r=object(item);auto priority=num(r,"priority");if(priority<0||priority>999||priority!=int(priority))throw Failure("Invalid rule priority");
        s.rules.append({flag(r,"enabled"),str(r,"condition"),str(r,"target"),str(r,"track"),num(r,"threshold"),num(r,"minBeats"),int(priority),num(r,"thresholdSeconds"),num(r,"thresholdDegrees"),num(r,"minSeconds")});}
    validateSettings(s);return s;
}
void validatePerformance(const Project &p){
    const auto &v=p.performance;validateSettings(v.settings);
    if(v.groups.isEmpty()||v.groups.size()>1024||v.generated.size()+v.manual.size()>500000||v.boundaries.size()>500000||v.suggestions.size()>500000||v.tonalities.size()>10000)throw Failure("Invalid performance data size");
    QSet<QString> groups;
    static const QRegularExpression id("^[A-Za-z0-9-]{1,80}$");
    for(const auto &g:v.groups){if(!id.match(g.id).hasMatch()||g.name.trimmed().isEmpty()||g.name.size()>128||groups.contains(g.id))throw Failure("Invalid or duplicate variant group");groups.insert(g.id);}
    if(!groups.contains("base"))throw Failure("Missing base variant group");
    if(v.roles.size()!=3)throw Failure("Invalid performance roles");
    for(const auto &r:QStringList{"relaxed","normal","emphasis"})if(!groups.contains(v.roles.value(r)))throw Failure("Unknown group in role mapping");
    auto target=[&](const QString &s){return v.roles.contains(s)||(s.startsWith("group:")&&groups.contains(s.mid(6)));};
    for(const auto &r:v.settings.rules)if(!target(r.target))throw Failure("Unknown rule target");
    QSet<QString> ids;
    for(const auto *list:{&v.generated,&v.manual})for(const auto &s:*list){
        if(s.id.isEmpty()||s.id.size()>1024||ids.contains(s.id))throw Failure("Duplicate or invalid performance segment ID");
        ids.insert(s.id);
        if(!QStringList{"beats","seconds"}.contains(s.anchor)||!QStringList{"hold","expand","contract","arch","custom"}.contains(s.mode))throw Failure("Invalid performance anchor/template");
        if(!std::isfinite(s.start)||!std::isfinite(s.end)||s.end<=s.start||std::abs(s.start)>864000||std::abs(s.end)>864000||s.startBlick < -MusicLimit||s.endBlick>MusicLimit||(s.anchor=="beats"&&s.endBlick<=s.startBlick))throw Failure("Invalid performance time");
        if(!std::isfinite(s.phaseStart)||!std::isfinite(s.phaseEnd)||s.phaseStart<0||s.phaseEnd>1||s.phaseEnd<=s.phaseStart||s.targets.isEmpty()||s.targets.size()!=s.cuts.size()+1||s.targets.size()>64)throw Failure("Invalid performance template phase");
        double previous=0;for(double cut:s.cuts){if(!std::isfinite(cut)||cut<=previous||cut>=1)throw Failure("Invalid performance cut");previous=cut;}
        for(const auto &t:s.targets)if(!target(t))throw Failure("Unknown performance target");
    }
    QSet<QString> boundaries;for(const auto &b:v.boundaries){if(b.source.isEmpty()||boundaries.contains(b.source)||!QStringList{"break","join","section-break","section-join"}.contains(b.mode))throw Failure("Invalid performance boundary");boundaries.insert(b.source);}
    qint64 end=-MusicLimit;for(const auto &k:v.tonalities){if(k.start<end||k.start < -MusicLimit||k.end>MusicLimit||k.end<=k.start||(!k.key.isEmpty()&&!parseTonality(k.key)))throw Failure("Invalid or overlapping tonal regions");end=k.end;}
}
QJsonObject performanceJson(const Project &p){
    validatePerformance(p);const auto &v=p.performance;QJsonObject o{{"settings",performanceSettingsJson(v.settings)},{"basisHash",v.basisHash}};
    QJsonArray groups;for(const auto &g:v.groups)groups.append(QJsonObject{{"id",g.id},{"name",g.name}});o["groups"]=groups;
    QJsonObject roles;for(auto it=v.roles.cbegin();it!=v.roles.cend();++it)roles[it.key()]=it.value();o["roles"]=roles;
    QJsonArray boundaries;for(const auto &b:v.boundaries)boundaries.append(QJsonObject{{"source",b.source},{"mode",b.mode}});o["boundaries"]=boundaries;
    QJsonArray suggestions;for(const auto &b:v.suggestions)suggestions.append(QJsonObject{{"source",b.source},{"mode",b.mode}});o["suggestions"]=suggestions;
    QJsonArray tonalities;for(const auto &k:v.tonalities)tonalities.append(QJsonObject{{"start",QString::number(k.start)},{"end",QString::number(k.end)},{"key",k.key}});o["tonalities"]=tonalities;
    auto segments=[](const QVector<PerformanceSegment> &list){QJsonArray a;for(const auto &s:list){QJsonArray cuts;for(auto c:s.cuts)cuts.append(c);
        a.append(QJsonObject{{"id",s.id},{"anchor",s.anchor},{"track",s.track},{"firstSource",s.firstSource},{"lastSource",s.lastSource},{"startBlick",QString::number(s.startBlick)},{"endBlick",QString::number(s.endBlick)},{"start",s.start},{"end",s.end},{"mode",s.mode},{"targets",QJsonArray::fromStringList(s.targets)},{"cuts",cuts},{"phaseStart",s.phaseStart},{"phaseEnd",s.phaseEnd},{"protect",s.protect},{"reason",s.reason}});}return a;};
    o["generated"]=segments(v.generated);o["manual"]=segments(v.manual);return o;
}
void readPerformance(Project &p,const QJsonValue &value){
    auto o=object(value);Performance v;v.settings=readPerformanceSettings(o["settings"]);v.basisHash=str(o,"basisHash");v.groups.clear();
    for(auto item:array(o,"groups")){auto g=object(item);v.groups.append({str(g,"id"),str(g,"name")});}
    auto roles=object(o["roles"]);v.roles.clear();for(auto it=roles.cbegin();it!=roles.cend();++it)v.roles[it.key()]=str(roles,it.key());
    for(auto item:array(o,"boundaries")){auto b=object(item);v.boundaries.append({str(b,"source"),str(b,"mode")});}
    for(auto item:array(o,"suggestions")){auto b=object(item);auto mode=str(b,"mode");if(!QStringList{"break","section-break","motif"}.contains(mode))throw Failure("Invalid boundary suggestion");v.suggestions.append({str(b,"source"),mode});}
    for(auto item:array(o,"tonalities")){auto k=object(item);v.tonalities.append({tick(k,"start"),tick(k,"end"),str(k,"key")});}
    auto segments=[&](const QString &name){QVector<PerformanceSegment> result;for(auto item:array(o,name)){auto a=object(item);PerformanceSegment s;
        s.id=str(a,"id");s.anchor=str(a,"anchor");s.track=str(a,"track");s.firstSource=str(a,"firstSource");s.lastSource=str(a,"lastSource");
        s.startBlick=tick(a,"startBlick");s.endBlick=tick(a,"endBlick");s.start=num(a,"start");s.end=num(a,"end");s.mode=str(a,"mode");s.targets.clear();
        for(auto t:array(a,"targets")){if(!t.isString())throw Failure("Invalid target");s.targets.append(t.toString());}
        for(auto c:array(a,"cuts")){if(!c.isDouble())throw Failure("Invalid cut");s.cuts.append(c.toDouble());}
        s.phaseStart=num(a,"phaseStart");s.phaseEnd=num(a,"phaseEnd");s.protect=flag(a,"protect");s.reason=str(a,"reason");result.append(s);
    }return result;};
    v.generated=segments("generated");v.manual=segments("manual");p.performance=std::move(v);validatePerformance(p);
}
}
