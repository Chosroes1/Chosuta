// SPDX-License-Identifier: GPL-3.0-or-later
#include "appearance.h"
#include "intervals.h"
#include <QImageReader>
#include <algorithm>
#include <cmath>
#include <mutex>
namespace chosuta {
void configureImageReaderLimit(){static std::once_flag flag;std::call_once(flag,[]{QImageReader::setAllocationLimit(256);});}
const QStringList &mouthStates(){static const QStringList states={"A","I","U","E","O","closed","rest","breath","unknown"};return states;}
static bool state(const QString &s){return mouthStates().contains(s);}
std::optional<QString> normalizedAssetId(const QString &name){auto s=name.toLower();if(QStringList{"a","i","u","e","o"}.contains(s))s=s.toUpper();return state(s)?std::optional<QString>(s):std::nullopt;}
AppearanceResolver::AppearanceResolver(const Project &p){validateAnalysisSettings(p.performance.analysis);validatePerformance(p);if(p.appearance.enabled)performance=std::make_shared<PerformanceResolver>(p);}
AppearanceResolver::AppearanceResolver(const Project &p,std::shared_ptr<const PerformanceResolver> plan){if(p.appearance.enabled)performance=std::move(plan);}
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
    if(performance){c.group=performance->groupAt(e.start+p.output.syncOffset);if(auto s=performance->at(e.start+p.output.syncOffset))c.intervalReasons=s->reasons;}
    return select(p,e.shape,c,e.appearanceFixed,available);
}
AssetSelection AppearanceResolver::at(const Project &p,const QVector<Event> &events,double seconds,bool disjoint,const QSet<QString> *available)const{
    const double time=seconds-p.output.syncOffset;AppearanceContext c;
    if(performance){c.group=performance->groupAt(seconds);if(auto s=performance->at(seconds))c.intervalReasons=s->reasons;}
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
    void applyAssetImport(Project &p,const QVector<AssetImportEntry> &entries,const QSet<QString> &replace){
        QSet<QString> seen;
        for(const auto &e:entries){
            if(e.id.trimmed().isEmpty()||e.status=="invalid"||e.path.isEmpty())throw Failure(Message::format("Invalid asset import target: %1",{e.id}));
            if(seen.contains(e.id))throw Failure(Message::format("Duplicate asset import target: %1",{e.id}));
            seen.insert(e.id);
            if(p.assets.contains(e.id)&&!replace.contains(e.id))throw Failure(Message::format("Asset replacement needs an explicit selection: %1",{e.id}));
        }
        for(const auto &e:entries)p.assets[e.id]=e.path;
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
            else {auto thumb=size;thumb.scale(96,96,Qt::KeepAspectRatio);reader.setScaledSize(thumb);if(reader.read().isNull()){entry.status="invalid";entry.message=Message("Invalid or oversized PNG").withDetail(reader.errorString());}}
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
