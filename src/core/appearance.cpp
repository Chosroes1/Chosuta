// SPDX-License-Identifier: GPL-3.0-or-later
#include "appearance.h"
#include "intervals.h"
#include <QImageReader>
#include <algorithm>
#include <bit>
#include <cmath>
#include <mutex>
namespace chosuta {
    void configureImageReaderLimit(){
        static std::once_flag initialized;
        std::call_once(initialized,[]{
            QImageReader::setAllocationLimit(256);
        });
    }
    static bool state(const QString &s){
        return QStringList{
            "A","I","U","E","O","closed","rest","breath"
        }
        .contains(s);
    }
    static QString beatAt(const Project &p,qint64 position) {
        qint64 base=0;
        auto meter=p.score.time.meters.front();
        for(int i=1;i<p.score.time.meters.size();++i){
            const auto next=p.score.time.meters[i];
            const qint64 boundary=base+qint64(next.bar-meter.bar)*meter.numerator*(Blick*4/meter.denominator);
            if(position<boundary)break;
            base=boundary;
            meter=next;
        }
        const qint64 unit=Blick*4/meter.denominator,length=unit*meter.numerator;
        const qint64 within=((position-base)%length+length)%length;
        if(within%unit)return "weak";
        const int beat=int(within/unit)+1;
        const QString signature=QString("%1/%2").arg(meter.numerator).arg(meter.denominator);
        const auto custom=p.appearance.strongBeats.constFind(signature);
        if(custom!=p.appearance.strongBeats.cend())return custom->contains(beat)?"strong":"weak";
        if(meter.numerator>3&&meter.numerator%3==0)return (beat-1)%3==0?"strong":"weak";
        if(meter.numerator==4)return beat==1||beat==3?"strong":"weak";
        return beat==1?"strong":"weak";
    }
    void validateAppearance(const Project::Appearance &a) {
        if(a.stepMax<1||a.stepMax>24||a.smallMax<=a.stepMax||a.smallMax>48)throw Failure("Invalid semitone thresholds");
        if(a.intervalUnit!="semitones"&&a.intervalUnit!="diatonic")throw Failure("Invalid interval unit");
        validateDegreeSettings(a);
        for(const auto &anchor:{
            a.closureAnchor,a.breathAnchor,a.restAnchor
        }) if(!QStringList{
            "previous","next","neutral"
        }
        .contains(anchor))throw Failure("Invalid appearance anchor");
        if(a.strongBeats.size()>64)throw Failure("Too many accent templates");
        for(auto it=a.strongBeats.cbegin();it!=a.strongBeats.cend();++it){
            const auto fields=it.key().split('/');
            bool nOk=false,dOk=false;
            const int n=fields.value(0).toInt(&nOk),d=fields.value(1).toInt(&dOk);
            if(fields.size()!=2||!nOk||!dOk||n<1||n>64||d<1||d>64||(d&(d-1))||it.key()!=QString("%1/%2").arg(n).arg(d))throw Failure("Invalid accent signature");
            QSet<int> seen;
            if(it->isEmpty())throw Failure("Empty accent template");
            for(int beat:*it){
                if(beat<1||beat>n||seen.contains(beat))throw Failure("Invalid accent beat");
                seen.insert(beat);
            }
        }
    }
    QJsonObject appearanceJson(const Project::Appearance &a) {
        validateAppearance(a);
        QJsonObject accents;
        QJsonObject spellings;
        for(auto it=a.noteSpellings.cbegin();it!=a.noteSpellings.cend();++it)spellings[it.key()]=it.value();
        QJsonArray customScale;
        for(int offset:a.customDegreeScale)customScale.append(offset);
        for(auto it=a.strongBeats.cbegin();it!=a.strongBeats.cend();++it){
            QJsonArray v;
            for(int n:*it)v.append(n);
            accents[it.key()]=v;
        }
        return {
            {
                "enabled",a.enabled
            },{
                "direction",a.direction
            },{
                "interval",a.interval
            },{
                "beat",a.beat
            }, {
                "stepMax",a.stepMax
            },{
                "smallMax",a.smallMax
            },{
                "intervalUnit",a.intervalUnit
            },{
                "stepDegreeMax",a.stepDegreeMax
            },{
                "smallDegreeMax",a.smallDegreeMax
            },{
                "degreeTonic",a.degreeTonic
            },{
                "degreeMode",a.degreeMode
            },{
                "customDegreeScale",customScale
            },{
                "noteSpellings",spellings
            }, {
                "closureAnchor",a.closureAnchor
            },{
                "breathAnchor",a.breathAnchor
            },{
                "restAnchor",a.restAnchor
            },{
                "strongBeats",accents
            }
        };
    }
    Project::Appearance readAppearance(const QJsonValue &v) {
        Project::Appearance a;
        if(!v.isObject())throw Failure("Invalid appearance settings");
        const auto o=v.toObject();
        auto flag=[&](const char *key){
            if(!o[key].isBool())throw Failure("Invalid appearance switch");
            return o[key].toBool();
        };
        auto integer=[&](const char *key){
            const auto n=o[key];
            if(!n.isDouble()||n.toDouble()!=n.toInt())throw Failure("Invalid appearance integer");
            return n.toInt();
        };
        auto text=[&](const char *key){
            if(!o[key].isString())throw Failure("Invalid appearance text");
            return o[key].toString();
        };
        a.enabled=flag("enabled");
        a.direction=flag("direction");
        a.interval=flag("interval");
        a.beat=flag("beat");
        a.stepMax=integer("stepMax");
        a.smallMax=integer("smallMax");
        a.intervalUnit=text("intervalUnit");
        a.stepDegreeMax=integer("stepDegreeMax");
        a.smallDegreeMax=integer("smallDegreeMax");
        a.degreeTonic=text("degreeTonic");
        a.degreeMode=text("degreeMode");
        if(!o["customDegreeScale"].isArray())throw Failure("Invalid custom scale");
        {
            a.customDegreeScale.clear();
            for(auto offset:o["customDegreeScale"].toArray()){
                if(!offset.isDouble()||offset.toDouble()!=offset.toInt())throw Failure("Invalid custom scale offset");
                a.customDegreeScale.append(offset.toInt());
            }
        }
        if(!o["noteSpellings"].isObject())throw Failure("Invalid note spellings");
        {
            const auto spellings=o["noteSpellings"].toObject();
            for(auto it=spellings.begin();it!=spellings.end();++it){
                if(!it.value().isString())throw Failure("Invalid note spelling value");
                a.noteSpellings[it.key()]=it.value().toString();
            }
        }
        a.closureAnchor=text("closureAnchor");
        a.breathAnchor=text("breathAnchor");
        a.restAnchor=text("restAnchor");
        if(!o["strongBeats"].isObject())throw Failure("Invalid accent templates");
        const auto accents=o["strongBeats"].toObject();
        for(auto it=accents.begin();it!=accents.end();++it){
            if(!it.value().isArray())throw Failure("Invalid accent template");
            QVector<int> beats;
            for(auto value:it.value().toArray()){
                if(!value.isDouble()||value.toDouble()!=value.toInt())throw Failure("Invalid accent beat");
                beats.append(value.toInt());
            }
            a.strongBeats[it.key()]=beats;
        }
        validateAppearance(a);
        return a;
    }
    QString appearanceId(const QString &shape,const AppearanceContext &c) {
        if(c.beat=="any"&&c.direction=="any"&&c.interval=="any")return shape;
        return shape+"_"+c.beat+"_"+c.direction+"_"+c.interval;
    }
    std::optional<QString> normalizedAssetId(const QString &name) {
        auto fields=name.split("_");
        if(fields.size()!=1&&fields.size()!=4)return std::nullopt;
        auto s=fields[0].toLower();
        if(QStringList{
            "a","i","u","e","o"
        }
        .contains(s))s=s.toUpper();
        if(!state(s)&&s!="unknown")return std::nullopt;
        if(fields.size()==1)return s;
        if(s=="unknown")return std::nullopt;
        for(int i=1;i<4;++i)fields[i]=fields[i].toLower();
        if(!QStringList{
            "any","strong","weak"
        }
        .contains(fields[1])||!QStringList{
            "any","up","down","repeat"
        }
        .contains(fields[2])||!QStringList{
            "any","step","small","large"
        }
        .contains(fields[3]))return std::nullopt;
        return appearanceId(s,{
            fields[2],fields[3],fields[1],{
            }
        });
    }
    AppearanceResolver::AppearanceResolver(const Project &p) {
        validateAppearance(p.appearance);
        if(!p.appearance.enabled)return;
        QSet<QString> singing,visible;
        QHash<QString,QString> controls;
        for(const auto &e:p.generated){
            visible.insert(e.source);
            if(e.articulation=="vowel"||e.articulation=="extend"||(e.articulation.isEmpty()&&QStringList{
                "A","I","U","E","O"
            }
            .contains(e.shape)))singing.insert(e.source);
            if(e.articulation=="cl"||e.articulation=="br"||e.articulation=="nasal")controls[e.source]=e.articulation;
        }
        for(const auto &track:p.score.tracks){
            if(track.muted||!p.selected.contains(track.id))continue;
            QMap<QString,QVector<const Note *>> groups;
            for(const auto &n:track.notes)if(!n.muted&&visible.contains(n.id)){
                groups[n.groupInstance].append(&n);
                const auto lyric=n.lyrics.normalized(QString::NormalizationForm_KC).trimmed(),phone=n.phonemes.trimmed();
                if(lyric=="cl"||lyric==QString::fromUtf8("っ")||phone=="cl")controls[n.id]="cl";
                else if(lyric=="br"||phone=="br"||phone=="breath")controls[n.id]="br";
            }
            for(auto notes:groups){
                std::sort(notes.begin(),notes.end(),[](const Note *a,const Note *b){
                    return a->onset==b->onset?a->id<b->id:a->onset<b->onset;
                });
                const Note *prior=nullptr;
                qint64 chainEnd=-MusicLimit;
                QVector<int> before(notes.size(),-1),after(notes.size(),-1);
                int last=-1;
                for(int i=0;i<notes.size();++i){
                    const auto &n=*notes[i];
                    if(i&&n.onset>notes[i-1]->onset+notes[i-1]->duration)last=-1;
                    before[i]=last;
                    if(singing.contains(n.id))last=i;
                    AppearanceContext c;
                    c.beat=beatAt(p,n.onset);
                    c.anchor=n.id;
                    ends[n.id]=n.onset+n.duration;
                    const bool unique=(i==0||notes[i-1]->onset+notes[i-1]->duration<=n.onset)&&(i+1==notes.size()||notes[i+1]->onset>=n.onset+n.duration);
                    if(singing.contains(n.id)){
                        if(prior&&chainEnd==n.onset&&unique){
                            const int delta=n.pitch-prior->pitch;
                            if(delta){
                                c.direction=delta>0?"up":"down";
                            }
                            if(p.appearance.intervalUnit=="diatonic"){
                                const auto degree=intervalDegree(p.appearance,*prior,n);
                                if(!degree)c.intervalReason="Unresolved written pitch; using shared interval";
                                else if(*degree==1&&!delta)c.direction="repeat";
                                else if(*degree>1)c.interval=*degree<=p.appearance.stepDegreeMax?"step":*degree<=p.appearance.smallDegreeMax?"small":"large";
                            }
                            else if(delta){
                                const int size=std::abs(delta);
                                c.interval=size<=p.appearance.stepMax?"step":size<=p.appearance.smallMax?"small":"large";
                            }
                            else c.direction="repeat";
                        }
                        prior=unique?&n:nullptr;
                    }
                    else if(controls.value(n.id)!="cl"&&controls.value(n.id)!="nasal")prior=nullptr;
                    chainEnd=n.onset+n.duration;
                    contexts[n.id]=c;
                }
                last=-1;
                for(int i=int(notes.size())-1;i>=0;--i){
                    if(i+1<notes.size()&&notes[i+1]->onset>notes[i]->onset+notes[i]->duration)last=-1;
                    after[i]=last;
                    if(singing.contains(notes[i]->id))last=i;
                }
                for(int i=0;i<notes.size();++i){
                    const auto &n=*notes[i];
                    if(singing.contains(n.id))continue;
                    const auto kind=controls.value(n.id);
                    const auto anchor=kind=="br"?p.appearance.breathAnchor:kind=="nasal"?p.appearance.restAnchor:p.appearance.closureAnchor;
                    if(anchor=="neutral")continue;
                    int ref=anchor=="next"?after[i]:before[i];
                    if(ref<0)ref=anchor=="next"?before[i]:after[i];
                    if(ref>=0){
                        auto c=contexts.value(notes[ref]->id);
                        c.beat=beatAt(p,n.onset);
                        c.anchor=notes[ref]->id;
                        contexts[n.id]=c;
                    }
                }
            }
        }
    }
    AssetSelection AppearanceResolver::select(const Project &p,QString shape,AppearanceContext c,bool fixed,const QSet<QString> *available)const {
        auto exists=[&](const QString &id){
            return available?available->contains(id):!p.assets.value(id).isEmpty();
        };
        AssetSelection result;
        result.shape=shape;
        result.fixed=fixed;
        if(!p.appearance.enabled||fixed||!state(shape)){
            result.id=exists(shape)?shape:p.fallback;
            result.candidates={
                shape,p.fallback
            };
            result.missing=!exists(result.id);
            return result;
        }
        if(!p.appearance.direction)c.direction="any";
        if(!p.appearance.interval){c.interval="any";c.intervalReason.clear();}
        if(!p.appearance.beat)c.beat="any";
        result.context=c;
        int mask=(c.beat!="any"?4:0)|(c.direction!="any"?2:0)|(c.interval!="any"?1:0);
        QVector<int> masks;
        for(int m=mask;m>=0;--m)if((m&mask)==m)masks.append(m);
        std::sort(masks.begin(),masks.end(),[](int a,int b){
            const auto x=std::popcount(unsigned(a)),y=std::popcount(unsigned(b));
            return x==y?a>b:x>y;
        });
        for(int m:masks){
            auto candidate=c;
            if(!(m&4))candidate.beat="any";
            if(!(m&2))candidate.direction="any";
            if(!(m&1))candidate.interval="any";
            result.candidates.append(appearanceId(shape,candidate));
        }
        result.candidates.append(p.fallback);
        result.candidates.removeDuplicates();
        for(const auto &id:result.candidates)if(exists(id)){
            result.id=id;
            return result;
        }
        result.id=appearanceId(shape,c);
        result.missing=true;
        return result;
    }
    AssetSelection AppearanceResolver::event(const Project &p,const Event &e,const QSet<QString> *available)const {
        return select(p,e.shape,contexts.value(e.source),e.appearanceFixed,available);
    }
    AssetSelection AppearanceResolver::at(const Project &p,const QVector<Event> &events,double seconds,bool disjoint,const QSet<QString> *available)const {
        const double time=seconds-p.output.syncOffset;
        if(const auto *e=eventAt(events,time,disjoint))return event(p,*e,available);
        const Event *previous=nullptr,*next=nullptr;
        if(disjoint){
            auto it=std::upper_bound(events.cbegin(),events.cend(),time,[](double t,const Event &e){
                return t<e.start;
            });
            if(it!=events.cend())next=&*it;
            if(it!=events.cbegin()){
                --it;
                if(it->end<=time)previous=&*it;
            }
        }
        else for(const auto &e:events){
            if(e.end<=time&&(!previous||e.end>previous->end))previous=&e;
            if(e.start>time&&(!next||e.start<next->start))next=&e;
        }
        const auto policy=p.rules.special.value("rest");
        if(previous&&(policy.mode=="hold"||(policy.mode=="timed"&&time-previous->end<policy.holdSeconds)))return event(p,*previous,available);
        AppearanceContext c;
        if(p.appearance.restAnchor!="neutral"){
            const auto *anchor=p.appearance.restAnchor=="next"?next:previous;
            if(!anchor)anchor=p.appearance.restAnchor=="next"?previous:next;
            if(anchor)c=contexts.value(anchor->source);
        }
        if(previous&&ends.contains(previous->source))c.beat=beatAt(p,ends.value(previous->source));
        else c.beat=beatAt(p,0);
        return select(p,policy.shape,c,false,available);
    }
    AssetImportPlan scanAssetDirectory(const QString &directory,const QMap<QString,QString> &existing,const std::atomic_bool *cancel,Progress progress) {
        configureImageReaderLimit();
        AssetImportPlan result;
        try{
            const QDir dir(directory);
            if(!dir.exists())throw Failure("Asset directory does not exist");
            const auto files=dir.entryInfoList(QDir::Files|QDir::NoSymLinks,QDir::Name);
            if(files.size()>10000)throw Failure("Asset directory exceeds 10000 files");
            QHash<QString,int> seen;
            for(int i=0;i<files.size();++i){
                if(cancel&&cancel->load()){
                    result.cancelled=true;
                    result.entries.clear();
                    return result;
                }
                const auto &file=files[i];
                if(file.suffix().compare("png",Qt::CaseInsensitive))continue;
                AssetImportEntry entry;
                entry.path=file.absoluteFilePath();
                const auto id=normalizedAssetId(file.completeBaseName());
                if(!id){
                    entry.status="unknown";
                    entry.message="Unrecognized asset filename";
                }
                else {
                    entry.id=*id;
                    entry.status=existing.contains(*id)?"replace":"new";
                    if(seen.contains(*id)){
                        entry.status="conflict";
                        entry.message="Duplicate normalized slot";
                        auto &first=result.entries[seen[*id]];
                        first.status="conflict";
                        first.message=entry.message;
                    }
                    else {
                        seen[*id]=int(result.entries.size());
                        QImageReader reader(entry.path);
                        const auto size=reader.size();
                        if(!size.isValid()||qint64(size.width())*size.height()*4>256LL*1024*1024){
                            entry.status="invalid";
                            entry.message="Invalid or oversized PNG";
                        }
                        else {
                            QSize thumb=size;
                            thumb.scale(96,96,Qt::KeepAspectRatio);
                            reader.setScaledSize(thumb);
                            if(reader.read().isNull()){
                                entry.status="invalid";
                                entry.message=reader.errorString();
                            }
                        }
                    }
                }
                result.entries.append(std::move(entry));
                if(progress)progress((i+1)*100/std::max(1,int(files.size())));
            }
        }
        catch(const std::exception &e){
            result.error=QString::fromUtf8(e.what());
            result.entries.clear();
        }
        return result;
    }
}
