// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"
#include "appearance.h"
#include "performance.h"
#include <cmath>
#include <algorithm>
#include <set>
namespace chosuta {
    void TimeMap::validate() {
        if(tempos.size()>10000||meters.size()>10000)throw Failure("Tempo/meter map exceeds 10000 markers");
        if(tempos.isEmpty()) tempos= {
            {
                0,120
            }
        };
        std::sort(tempos.begin(),tempos.end(),[](auto a,auto b) {
            return a.position<b.position;
        });
        if(tempos.front().position!=0) throw Failure("Tempo map must start at zero");
        for(int i=0;i<tempos.size();++i) if(!std::isfinite(tempos[i].bpm)||tempos[i].bpm<=0||tempos[i].bpm>10000||tempos[i].position<0||tempos[i].position>MusicLimit||(i&&tempos[i].position==tempos[i-1].position)) throw Failure("Invalid tempo map");
        if(meters.isEmpty()) meters= {
            {
                0,4,4
            }
        };
        std::sort(meters.begin(),meters.end(),[](auto a,auto b) {
            return a.bar<b.bar;
        });
        if(meters.front().bar!=0) throw Failure("Meter map must start at bar zero");
        for(int i=0;i<meters.size();++i) {
            auto m=meters[i];
            if(m.bar<0||m.bar>10000000||m.numerator<1||m.numerator>64||m.denominator<1||m.denominator>64||(m.denominator&(m.denominator-1))||(i&&m.bar==meters[i-1].bar))throw Failure("Invalid meter map");
        }
        for(int i=0;i<measureStarts.size();++i)
            if(measureStarts[i]<0||measureStarts[i]>MusicLimit||(!i&&measureStarts[i]!=0)||(i&&measureStarts[i]<=measureStarts[i-1]))throw Failure("Invalid source measure boundaries");
        if(!measureStarts.isEmpty()&&meters.last().bar>=measureStarts.size())throw Failure("Invalid source measure boundaries");
        if(pickupOffset<0||(pickupOffset&&(measureStarts.size()<2||pickupOffset>=meters.front().numerator*(Blick*4/meters.front().denominator))))throw Failure("Invalid source measure boundaries");
    }
    double TimeMap::seconds(qint64 pos) const {
        long double result=0;
        if(pos<0)return double((long double)pos/Blick*60/tempos.front().bpm);
        for(int i=0;i<tempos.size();++i) {
            auto t=tempos[i];
            qint64 end=(i+1<tempos.size()?std::min(pos,tempos[i+1].position):pos);
            if(end>t.position)result+=(long double)(end-t.position)/Blick*60/t.bpm;
            if(end==pos)break;
        }
        return double(result);
    }
    qint64 TimeMap::blicks(double s) const {
        if(!std::isfinite(s))throw Failure("Invalid time");
        long double pos=0,remaining=s;
        if(s<0)pos=(long double)s*Blick*tempos.front().bpm/60;
        else for(int i=0;i<tempos.size();++i) {
            auto t=tempos[i];
            double span=i+1<tempos.size()?seconds(tempos[i+1].position)-seconds(t.position):std::numeric_limits<double>::infinity();
            if(remaining<=span) {
                pos=t.position+remaining*Blick*t.bpm/60;
                break;
            }
            remaining-=span;
        }
        if(std::abs(pos)>MusicLimit)throw Failure("Music position exceeds limit");
        return std::llround(pos);
    }
    QString TimeMap::beatLabel(qint64 pos)const {
        qint64 base=-pickupOffset;
        Meter current=meters.front();
        for(int i=1;i<meters.size();++i) {
            qint64 next=measureStarts.isEmpty()?base+qint64(meters[i].bar-current.bar)*current.numerator*(Blick*4/current.denominator):measureStarts[meters[i].bar];
            if(pos<next)break;
            base=next;
            current=meters[i];
        }
        qint64 unit=Blick*4/current.denominator, barLength=unit*current.numerator;
        qint64 relative=pos-base;
        qint64 bar=qint64(std::floor(double(relative)/barLength));
        qint64 inBar=relative-bar*barLength;
        return QString("%1:%2:%3").arg(current.bar+bar+1).arg(inBar/unit+1).arg(double(inBar%unit)/unit,0,'f',3);
    }
    void validateCanvas(const CanvasSettings&c){
        if(c.width<16||c.height<16||c.width>7680||c.height>4320||qint64(c.width)*c.height>33177600)throw Failure("Resolution must be 16..7680 x 16..4320");
        if(!c.background.isValid()||!QStringList{"cover","contain","stretch"}.contains(c.backgroundFit))throw Failure("Invalid canvas background");
        if(!std::isfinite(c.characterScale)||c.characterScale<.01||c.characterScale>10||!std::isfinite(c.characterX)||!std::isfinite(c.characterY)||c.characterX < -2||c.characterX>3||c.characterY < -2||c.characterY>3)throw Failure("Invalid character layout");
    }
    double Project::duration()const {
        if(output.duration>0)return output.duration;
        double d=std::max(0.,score.sourceDuration());
        for(const auto&t:score.tracks)for(const auto&n:t.notes)d=std::max(d,score.time.seconds(n.onset+n.duration));
        for(const auto&e:effective())d=std::max(d,e.end);
        d=std::max(.1,d+std::max(0.,output.syncOffset));
        if(subtitlesEnabled){const auto sources=subtitleSources(*this);for(const auto&t:subtitles)if(t.enabled)for(const auto&c:t.cues)d=std::max(d,subtitleInterval(*this,c,&sources).end);}
        if(appearance.enabled&&!performance.manual.isEmpty()){PerformanceResolver resolver(*this,false);for(const auto &segment:performance.manual)d=std::max(d,resolver.interval(segment).end);}
        return d;
    }
    QVector<Event> Project::effective()const {
        QVector<Event> result;
        const bool corrected=timing.enabled&&timingCorrectionCurrent(*this);
        QSet<QString> sources;
        for(const auto&e:generated) {
            sources.insert(e.id);
            if(!overrides.contains(e.id)){
                Event v=e;
                if(corrected&&timing.sources.contains(e.source)){
                    const auto &s=timing.sources[e.source];
                    if(e.start>=s.originalStart-1e-9&&e.end<=s.originalEnd+1e-9&&s.originalEnd>s.originalStart){
                        double factor=(s.end-s.start)/(s.originalEnd-s.originalStart);
                        v.start=s.start+(e.start-s.originalStart)*factor;v.end=s.start+(e.end-s.originalStart)*factor;
                        v.provenance+="/waveform-estimated";
                    }
                }
                result.append(v);
            }
        }
        for(auto it=overrides.begin();it!=overrides.end();++it) {
            const auto&o=it.value();
            if(o.deleted)continue;
            bool active=sources.contains(it.key());
            if(o.standalone) {
                active=true;
                for(const auto&id:o.parentIds)if(!sources.contains(id))active=false;
            }
            if(!active)continue;
            Event e=o.event;
            if(o.anchor=="beats") {
                e.start=score.time.seconds(o.startBlick);
                e.end=score.time.seconds(o.endBlick);
            }
            result.append(e);
        }
        std::stable_sort(result.begin(),result.end(),[](const Event&a,const Event&b) {
            return a.start==b.start?a.id<b.id:a.start<b.start;
        });
        return result;
    }
    QStringList Project::orphanOverrides()const {
        QSet<QString>ids;
        for(const auto&e:generated)ids.insert(e.id);
        QStringList r;
        for(auto it=overrides.begin();it!=overrides.end();++it) {
            bool orphan=!ids.contains(it.key());
            if(it.value().standalone) {
                orphan=false;
                for(const auto&id:it.value().parentIds)if(!ids.contains(id))orphan=true;
            }
            if(orphan)r<<it.key();
        }
        return r;
    }
    void Project::regenerate(const std::atomic_bool *cancel,Progress progress) {
        auto result=generate(*this,cancel,progress);
        // Commit only after successful generation; cancellation leaves every layer intact.
        for(auto it=overrides.begin();it!=overrides.end();) {
            if(!it.value().locked)it=overrides.erase(it);
            else ++it;
        }
        generated=std::move(result);
        timing.sources.clear();timing.enabled=false;
    }
    void Project::edit(const Event&e,bool locked,QString anchor) {
        if(!std::isfinite(e.start)||!std::isfinite(e.end)||e.end<=e.start)throw Failure("Invalid event interval");
        Override o;
        o.event=e;
        if(!QStringList{"A","I","U","E","O","closed","rest","breath","unknown"}.contains(e.shape))o.event.appearanceFixed=true;
        
        o.sourceFragments=eventSourceFragments(*this,e);
        if(overrides.contains(e.id))o.parentIds=overrides[e.id].parentIds;
        o.standalone=overrides.contains(e.id)?overrides[e.id].standalone:(e.provenance=="manual/split"||e.provenance=="manual/merge");
        if(!o.event.provenance.startsWith("manual"))o.event.provenance="manual/edit";
        o.locked=locked;
        if(!score.hasMusic)anchor="seconds";
        o.anchor=anchor;
        if(anchor=="beats") {
            o.startBlick=score.time.blicks(e.start);
            o.endBlick=score.time.blicks(e.end);
        }
        overrides[e.id]=o;
    }
    void Project::erase(const Event&e) {
        edit(e);
        overrides[e.id].deleted=true;
    }
    static QVector<SourceFragment> cropFragments(const QVector<SourceFragment> &fragments,double start,double end) {
        QVector<SourceFragment> result;
        for(const auto &f:fragments){
            const double a=std::max(start,f.start),b=std::min(end,f.end);
            if(b<=a)continue;
            const double scale=(f.sourceEnd-f.sourceStart)/(f.end-f.start);
            result.append({f.parentId,f.sourceStart+(a-f.start)*scale,f.sourceStart+(b-f.start)*scale,
                (a-start)/(end-start),(b-start)/(end-start)});
        }
        return result;
    }
    void Project::split(const Event&e,double p) {
        if(!std::isfinite(p)||p<=e.start||p>=e.end)throw Failure("Split must be inside event");
        const auto fragments=eventSourceFragments(*this,e);
        const double cut=(p-e.start)/(e.end-e.start);
        erase(e);
        Event a=e,b=e;
        a.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        b.id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        a.end=p;b.start=p;
        a.provenance=b.provenance="manual/split";
        edit(a);edit(b);
        const auto parents=overrides[e.id].standalone?overrides[e.id].parentIds:QStringList{e.id};
        overrides[a.id].parentIds=parents;overrides[b.id].parentIds=parents;
        overrides[a.id].sourceFragments=cropFragments(fragments,0,cut);
        overrides[b.id].sourceFragments=cropFragments(fragments,cut,1);
    }
    void Project::merge(const QVector<Event>&events) {
        if(events.size()<2)throw Failure("Select two or more events");
        Event e=events.front();
        QStringList parents;
        QHash<QString,Event> selected,originals;
        QHash<QString,QVector<SourceFragment>> fragments;
        for(const auto &v:generated)originals[v.id]=v;
        for(const auto &v:events){
            e.start=std::min(e.start,v.start);e.end=std::max(e.end,v.end);
            selected[v.id]=v;fragments[v.id]=eventSourceFragments(*this,v);
            parents.append(overrides.contains(v.id)&&overrides[v.id].standalone?overrides[v.id].parentIds:QStringList{v.id});
        }
        QVector<SourceFragment> merged;
        QSet<QString> sources;bool known=true;
        for(const auto &visible:resolveEvents(events)){
            const auto v=selected.value(visible.id);
            auto pieces=cropFragments(fragments.value(v.id),(visible.start-v.start)/(v.end-v.start),(visible.end-v.start)/(v.end-v.start));
            if(pieces.isEmpty())known=false;
            for(auto f:pieces){
                if(!originals.contains(f.parentId))known=false;
                else sources.insert(originals.value(f.parentId).source);
                f.start=(visible.start+(visible.end-visible.start)*f.start-e.start)/(e.end-e.start);
                f.end=(visible.start+(visible.end-visible.start)*f.end-e.start)/(e.end-e.start);
                merged.append(f);
            }
        }
        // Preserve an exact, potentially piecewise single-source mapping. Genuine
        // multi-source merges remain explicitly ambiguous; never encode IDs in text.
        e.source=known&&sources.size()==1?*sources.cbegin():QString{};
        if(e.source.isEmpty())merged.clear();
        for(const auto &v:events)erase(v);
        e.id=QUuid::createUuid().toString(QUuid::WithoutBraces);e.provenance="manual/merge";
        edit(e);parents.removeDuplicates();
        overrides[e.id].parentIds=parents;overrides[e.id].sourceFragments=merged;
    }
    QVector<Event> resolveEvents(const QVector<Event>&events) {
        struct Boundary {
            double time;
            int index;
            bool start;
        };
        QVector<Boundary>boundaries;
        for(int i=0;i<events.size();++i) {
            boundaries.append( {
                events[i].start,i,true
            });
            boundaries.append( {
                events[i].end,i,false
            });
        }
        std::sort(boundaries.begin(),boundaries.end(),[](const auto&a,const auto&b) {
            return a.time==b.time?a.start<b.start:a.time<b.time;
        });
        auto less=[&](int a,int b) {
            auto&x=events[a];
            auto&y=events[b];
            bool xm=x.provenance.startsWith("manual"),ym=y.provenance.startsWith("manual");
            if(xm!=ym)return xm>ym;
            if(x.start!=y.start)return x.start>y.start;
            if(x.id!=y.id)return x.id>y.id;
            return a<b;
        };
        std::set<int,decltype(less)>active(less);
        QVector<Event>result;
        for(int i=0;i<boundaries.size();) {
            double time=boundaries[i].time;
            while(i<boundaries.size()&&boundaries[i].time==time) {
                auto b=boundaries[i++];
                if(b.start)active.insert(b.index);
                else active.erase(b.index);
            }
            if(i==boundaries.size()||active.empty())continue;
            auto e=events[*active.begin()];
            double end=boundaries[i].time;
            if(!result.isEmpty()&&result.last().id==e.id&&result.last().end==time) {
                result.last().end=end;
                continue;
            }
            e.start=time;
            e.end=end;
            result.append(e);
        }
        return result;
    }
    const Event *eventAt(const QVector<Event>&events,double time,bool disjoint) {
        // Edits may overlap: manual wins, otherwise stable latest-start/id order.
        const Event *found=nullptr;
        auto end=std::upper_bound(events.begin(),events.end(),time,[](double t,const Event&e) {
            return t<e.start;
        });
        if(disjoint) {
            if(end==events.begin())return nullptr;
            --end;
            return time<end->end?&*end:nullptr;
        }
        for(auto it=events.begin();it!=end;++it)if(time>=it->start&&time<it->end) {
            if(!found||!found->provenance.startsWith("manual")||it->provenance.startsWith("manual"))found=&*it;
        }
        return found;
    }
    QVector<Event> applyTimingProposals(const Project&p,const QVector<TimingProposal>&proposals,const QString&hash) {
        auto result=p.effective();
        for(const auto&v:proposals) {
            if(v.inputHash!=hash||!std::isfinite(v.start)||!std::isfinite(v.end)||v.end<=v.start||!std::isfinite(v.confidence)||v.confidence<0||v.confidence>1)continue;
            if(p.overrides.contains(v.eventId))continue;
            for(auto&e:result)if(e.id==v.eventId&&e.source==v.source) {
                e.start=v.start;
                e.end=v.end;
                e.provenance="refiner/"+v.provider+"/"+v.version;
            }
        }
        std::stable_sort(result.begin(),result.end(),[](const auto&a,const auto&b) {
            return a.start==b.start?a.id<b.id:a.start<b.start;
        });
        return result;
    }
}
