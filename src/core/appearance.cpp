// SPDX-License-Identifier: GPL-3.0-or-later
#include "appearance.h"
#include "intervals.h"
#include <QImageReader>
#include <algorithm>
#include <cmath>
#include <mutex>
namespace chosuta {
void configureImageReaderLimit(){static std::once_flag flag;std::call_once(flag,[]{QImageReader::setAllocationLimit(256);});}
static bool state(const QString &s){return QStringList{"A","I","U","E","O","closed","rest","breath","unknown"}.contains(s);}
void validateAppearance(const Project::Appearance &a){
    if(a.intervalUnit!="semitones"&&a.intervalUnit!="diatonic")throw Failure("Invalid interval unit");
    validateDegreeSettings(a);
    if(a.strongBeats.size()>64)throw Failure("Too many accent templates");
    for(auto it=a.strongBeats.cbegin();it!=a.strongBeats.cend();++it){const auto f=it.key().split('/');bool no=false,de=false;int n=f.value(0).toInt(&no),d=f.value(1).toInt(&de);
        if(f.size()!=2||!no||!de||n<1||n>64||d<1||d>64||(d&(d-1))||it->isEmpty())throw Failure("Invalid accent template");
        QSet<int> seen;for(int b:*it){if(b<1||b>n||seen.contains(b))throw Failure("Invalid accent beat");seen.insert(b);}}
}
QJsonObject appearanceJson(const Project::Appearance &a){
    validateAppearance(a);QJsonObject spellings,accents;QJsonArray scale;
    for(auto it=a.noteSpellings.cbegin();it!=a.noteSpellings.cend();++it)spellings[it.key()]=it.value();
    for(int n:a.customDegreeScale)scale.append(n);
    for(auto it=a.strongBeats.cbegin();it!=a.strongBeats.cend();++it){QJsonArray v;for(int n:*it)v.append(n);accents[it.key()]=v;}
    return {{"enabled",a.enabled},{"tonalityEnabled",a.tonalityEnabled},{"intervalUnit",a.intervalUnit},{"degreeTonic",a.degreeTonic},{"degreeMode",a.degreeMode},{"customDegreeScale",scale},{"noteSpellings",spellings},{"strongBeats",accents}};
}
Project::Appearance readAppearance(const QJsonValue &v){
    if(!v.isObject())throw Failure("Invalid appearance settings");
    auto o=v.toObject();Project::Appearance a;
    for(const auto &k:{"enabled","tonalityEnabled"})if(!o[k].isBool())throw Failure("Invalid appearance switch");
    for(const auto &k:{"intervalUnit","degreeTonic","degreeMode"})if(!o[k].isString())throw Failure("Invalid appearance text");
    a.enabled=o["enabled"].toBool();a.tonalityEnabled=o["tonalityEnabled"].toBool();a.intervalUnit=o["intervalUnit"].toString();a.degreeTonic=o["degreeTonic"].toString();a.degreeMode=o["degreeMode"].toString();
    if(!o["customDegreeScale"].isArray()||!o["noteSpellings"].isObject()||!o["strongBeats"].isObject())throw Failure("Invalid degree settings");
    a.customDegreeScale.clear();for(auto value:o["customDegreeScale"].toArray()){if(!value.isDouble()||value.toDouble()!=value.toInt())throw Failure("Invalid scale offset");a.customDegreeScale.append(value.toInt());}
    const auto names=o["noteSpellings"].toObject();for(auto it=names.cbegin();it!=names.cend();++it){if(!it->isString())throw Failure("Invalid written pitch");a.noteSpellings[it.key()]=it->toString();}
    const auto accents=o["strongBeats"].toObject();for(auto it=accents.cbegin();it!=accents.cend();++it){if(!it->isArray())throw Failure("Invalid accent list");for(auto value:it->toArray()){if(!value.isDouble()||value.toDouble()!=value.toInt())throw Failure("Invalid accent beat");a.strongBeats[it.key()].append(value.toInt());}}
    validateAppearance(a);return a;
}
std::optional<QString> normalizedAssetId(const QString &name){auto s=name.toLower();if(QStringList{"a","i","u","e","o"}.contains(s))s=s.toUpper();return state(s)?std::optional<QString>(s):std::nullopt;}
AppearanceResolver::AppearanceResolver(const Project &p){validateAppearance(p.appearance);validatePerformance(p);if(p.appearance.enabled)performance=std::make_shared<PerformanceResolver>(p);}
AssetSelection AppearanceResolver::select(const Project &p,QString shape,AppearanceContext c,bool fixed,const QSet<QString> *available)const{
    auto exists=[&](const QString &id){return available?available->contains(id):!p.assets.value(id).isEmpty();};
    AssetSelection r;r.shape=shape;r.fixed=fixed;r.context=c;
    if(p.appearance.enabled&&!fixed&&state(shape))r.candidates.append(variantAssetId(c.group,shape));
    r.candidates.append(shape);r.candidates.append(p.fallback);r.candidates.removeDuplicates();
    for(const auto &id:r.candidates)if(exists(id)){r.id=id;return r;}
    r.id=r.candidates.front();r.missing=true;return r;
}
AssetSelection AppearanceResolver::event(const Project &p,const Event &e,const QSet<QString> *available)const{
    AppearanceContext c;c.anchor=e.source;
    if(performance){c.group=performance->groupAt(e.start+p.output.syncOffset);if(auto s=performance->at(e.start+p.output.syncOffset))c.intervalReason=s->reason;}
    return select(p,e.shape,c,e.appearanceFixed,available);
}
AssetSelection AppearanceResolver::at(const Project &p,const QVector<Event> &events,double seconds,bool disjoint,const QSet<QString> *available)const{
    const double time=seconds-p.output.syncOffset;AppearanceContext c;
    if(performance){c.group=performance->groupAt(seconds);if(auto s=performance->at(seconds))c.intervalReason=s->reason;}
    if(const auto e=eventAt(events,time,disjoint)){c.anchor=e->source;return select(p,e->shape,c,e->appearanceFixed,available);}
    const auto policy=p.rules.special.value("rest");const Event *previous=nullptr;
    if(disjoint){auto it=std::upper_bound(events.cbegin(),events.cend(),time,[](double t,const Event &e){return t<e.start;});if(it!=events.cbegin()){--it;if(it->end<=time)previous=&*it;}}
    else for(const auto &e:events)if(e.end<=time&&(!previous||e.end>previous->end))previous=&e;
    if(previous&&(policy.mode=="hold"||(policy.mode=="timed"&&time-previous->end<policy.holdSeconds))){
        if(performance&&!performance->at(seconds))c.group=performance->groupAt(std::nextafter(previous->end+p.output.syncOffset,-INFINITY));
        return select(p,previous->shape,c,previous->appearanceFixed,available);
    }
    return select(p,policy.shape,c,false,available);
}
    AssetImportPlan scanAssetFiles(const QStringList &paths,const QMap<QString,QString> &existing,const std::atomic_bool *cancel,Progress progress) {
        configureImageReaderLimit();AssetImportPlan result;
        if(paths.size()>10000){result.error="Import exceeds 10000 files";return result;}
        QHash<QString,int> seen;QSet<QString> files;
        for(int i=0;i<paths.size();++i){
            if(cancel&&cancel->load()){result.cancelled=true;result.entries.clear();return result;}
            QFileInfo file(paths[i]);if(file.isSymLink()||!file.isFile()||file.suffix().compare("png",Qt::CaseInsensitive))continue;
            const auto path=file.absoluteFilePath();if(files.contains(path))continue;files.insert(path);
            AssetImportEntry entry;entry.path=path;auto id=normalizedAssetId(file.completeBaseName());
            entry.id=id.value_or(QString{});entry.status=id?(existing.contains(*id)?"replace":"new"):"unknown";
            if(!id)entry.message="Choose a group and mouth slot";
            QImageReader reader(path);const auto size=reader.size();
            if(!size.isValid()||reader.format().toLower()!="png"||qint64(size.width())*size.height()*4>256LL*1024*1024){entry.status="invalid";entry.message="Invalid or oversized PNG";}
            else {auto thumb=size;thumb.scale(96,96,Qt::KeepAspectRatio);reader.setScaledSize(thumb);if(reader.read().isNull()){entry.status="invalid";entry.message=reader.errorString();}}
            if(id&&entry.status!="invalid"){
                if(seen.contains(*id)){entry.status="conflict";entry.message="Duplicate normalized slot; remap or select one";auto &first=result.entries[seen[*id]];first.status=entry.status;first.message=entry.message;}
                else seen[*id]=int(result.entries.size());
            }
            result.entries.append(std::move(entry));if(progress)progress((i+1)*100/std::max(1,int(paths.size())));
        }return result;
    }
    AssetImportPlan scanAssetDirectory(const QString &directory,const QMap<QString,QString> &existing,const std::atomic_bool *cancel,Progress progress) {
        QDir dir(directory);if(!dir.exists()){AssetImportPlan r;r.error="Asset directory does not exist";return r;}
        const auto files=dir.entryInfoList(QDir::Files|QDir::NoSymLinks,QDir::Name);
        if(files.size()>10000){AssetImportPlan r;r.error="Asset directory exceeds 10000 files";return r;}
        QStringList paths;for(const auto &file:files)paths.append(file.absoluteFilePath());return scanAssetFiles(paths,existing,cancel,progress);
    }
    AssetImportPlan scanVariantDirectory(const QString &path,const Project &p,const QString &singleGroup,const std::atomic_bool *cancel,Progress progress){
        AssetImportPlan result;
        if(QFileInfo(path).isSymLink()){result.error="Symlink folders are not imported";return result;}
        const QDir dir(path);const auto children=dir.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks,QDir::Name);
        bool direct=false;for(const auto &file:dir.entryInfoList(QDir::Files|QDir::NoSymLinks))if(file.suffix().compare("png",Qt::CaseInsensitive)==0)direct=true;
        QStringList folders;if(direct||children.isEmpty())folders<<path;else for(const auto &child:children)folders<<child.absoluteFilePath();
        if(folders.size()>1024){result.error="Too many variant folders";return result;}
        for(int i=0;i<folders.size();++i){
            if(cancel&&cancel->load()){result.cancelled=true;result.entries.clear();result.groups.clear();return result;}
            QString group=singleGroup;
            if(!direct&&!children.isEmpty()){
                const auto name=QFileInfo(folders[i]).fileName();QStringList matching;
                for(const auto &g:p.performance.groups)if(g.name==name)matching<<g.id;
                group=matching.size()==1?matching.front():QUuid::createUuid().toString(QUuid::WithoutBraces);
                if(matching.size()!=1)result.groups.append({group,name});
            }
            auto scan=scanAssetDirectory(folders[i],{},cancel);
            if(!scan.error.isEmpty()||scan.cancelled){result.error=scan.error;result.cancelled=scan.cancelled;result.entries.clear();result.groups.clear();return result;}
            for(auto &entry:scan.entries){entry.group=group;if(!entry.id.isEmpty())entry.id=variantAssetId(group,entry.id);
                if(entry.status=="new"&&p.assets.contains(entry.id))entry.status="replace";
                if(result.entries.size()>=10000){result.entries.clear();result.groups.clear();result.error="Import exceeds 10000 files";return result;}
                result.entries.append(entry);}
            if(progress)progress((i+1)*100/std::max(1,int(folders.size())));
        }return result;
    }

}
