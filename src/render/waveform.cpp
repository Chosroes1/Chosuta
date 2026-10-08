// SPDX-License-Identifier: GPL-3.0-or-later
#include "waveform.h"
#include "core/executable.h"
#include <QtEndian>
#include <bit>
#include <cmath>
#include <algorithm>
namespace chosuta {
void Waveform::buildLevels(){
    reduced.clear();int count=bins.size();
    for(int level=0;count>1;++level){QVector<WavePeak> next;next.reserve((count+1)/2);
        auto get=[&](int i){return level?reduced[level-1][i]:WavePeak{bins[i].low,bins[i].high};};
        for(int i=0;i<count;i+=2){auto a=get(i);if(i+1<count){auto b=get(i+1);a.low=std::min(a.low,b.low);a.high=std::max(a.high,b.high);}next.append(a);}
        count=next.size();reduced.append(std::move(next));
    }
}
qint64 Waveform::storageBytes()const {qint64 n=bins.capacity()*qint64(sizeof(WaveBin));for(const auto &level:reduced)n+=level.capacity()*qint64(sizeof(WavePeak));return n;}
WavePeak Waveform::peaks(double begin,double end)const {
    if(bins.isEmpty()||!std::isfinite(begin)||!std::isfinite(end)||end<=begin||end<=0||begin>=duration())return {};
    int a=std::clamp(int(std::floor(std::max(0.,begin)/Step)),0,int(bins.size())-1),b=std::clamp(int(std::ceil(std::min(duration(),end)/Step)),a+1,int(bins.size()));
    WavePeak result{1,-1};
    while(a<b){int level=0;while(level<reduced.size()&&(a% (1<<(level+1)))==0&&a+(1<<(level+1))<=b)++level;
        auto peak=level?reduced[level-1][a>>level]:WavePeak{bins[a].low,bins[a].high};
        result.low=std::min(result.low,peak.low);result.high=std::max(result.high,peak.high);a+=1<<level;
    }
    return result;
}
WaveformResult readWaveform(const QString &path,const QString &ffmpeg,const std::atomic_bool &cancel,Progress progress,double expectedDuration){
    WaveformResult result;
    try {
        QFileInfo before(path);if(!before.isFile())throw Failure("Missing audio: "+path);
        auto wave=std::make_shared<Waveform>();wave->path=path;wave->fileBytes=before.size();wave->fileModified=before.lastModified().toMSecsSinceEpoch();
        QFile input(path);if(!input.open(QIODevice::ReadOnly))throw Failure(input.errorString());QCryptographicHash hash(QCryptographicHash::Sha256);
        while(!input.atEnd()){if(cancel.load()){result.cancelled=true;return result;}auto bytes=input.read(65536);if(bytes.isEmpty()&&input.error()!=QFileDevice::NoError)throw Failure(input.errorString());hash.addData(bytes);}
        wave->hash=QString::fromLatin1(hash.result().toHex());input.close();
        if(progress)progress(5);
        QProcess process;process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start(resolveExecutable(ffmpeg),{"-v","error","-nostdin","-i",path,"-map","0:a:0","-vn","-ac","1","-ar","16000","-f","f32le","pipe:1"});
        if(!process.waitForStarted(3000))throw Failure(process.errorString());
        QByteArray pending,errors;int count=0;float low=1,high=-1;double squares=0;QElapsedTimer idle;idle.start();
        auto stop=[&]{process.kill();process.waitForFinished(1000);};
        auto bin=[&]{wave->bins.append({low,high,float(std::sqrt(squares/count))});count=0;low=1;high=-1;squares=0;};
        while(true){
            if(cancel.load()){stop();result.cancelled=true;return result;}
            auto bytes=process.read(65536);
            if(!bytes.isEmpty()){
                idle.restart();pending.append(bytes);int consumed=0;
                while(consumed+4<=pending.size()){
                    quint32 bits=qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(pending.constData()+consumed));float sample=std::bit_cast<float>(bits);consumed+=4;
                    if(!std::isfinite(sample)){stop();throw Failure("Non-finite audio sample");}
                    sample=std::clamp(sample,-1.f,1.f);low=std::min(low,sample);high=std::max(high,sample);squares+=double(sample)*sample;++count;++wave->samples;
                    if(wave->samples>21600LL*16000){stop();throw Failure("Waveform exceeds six hours");}
                    if(count==160)bin();
                }
                pending.remove(0,consumed);
                if(progress&&expectedDuration>0)progress(std::clamp(5+int(wave->duration()/expectedDuration*90),5,95));
            }
            errors.append(process.readAllStandardError());if(errors.size()>4096)errors=errors.right(4096);
            if(process.state()==QProcess::NotRunning&&process.bytesAvailable()==0)break;
            if(idle.elapsed()>60000){stop();throw Failure("Audio decoding stopped responding");}
            if(bytes.isEmpty())process.waitForReadyRead(20);
        }
        if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)throw Failure(errors.isEmpty()?QString("Audio decoding failed"):QString::fromUtf8(errors));
        if(!pending.isEmpty()||wave->samples==0)throw Failure("Empty or incomplete decoded audio");
        if(count)bin();
        QFileInfo after(path);if(after.size()!=wave->fileBytes||after.lastModified().toMSecsSinceEpoch()!=wave->fileModified)throw Failure("Audio changed while reading waveform");
        wave->buildLevels();if(cancel.load()){result.cancelled=true;return result;}if(progress)progress(100);result.wave=std::move(wave);
    }catch(const std::exception &e){result.error=QString::fromUtf8(e.what());}
    return result;
}
WaveCorrectionResult correctWaveform(const Project &p,const Waveform &wave,const std::atomic_bool &cancel,Progress progress,const QStringList &selectedEvents){
    WaveCorrectionResult result;result.timing=p.timing;auto &layer=result.timing;layer.sources.clear();layer.enabled=false;
    try {
        validateTimingCorrection(p);
        QFileInfo audio(p.audioPath);
        if(wave.path!=p.audioPath||audio.size()!=wave.fileBytes||audio.lastModified().toMSecsSinceEpoch()!=wave.fileModified||wave.bins.isEmpty())throw Failure("Waveform does not match current audio");
        layer.audioHash=wave.hash;layer.audioBytes=wave.fileBytes;layer.audioModified=wave.fileModified;layer.basisHash=timingBasisHash(p,&cancel);
        struct Group {QString id;double a=0,b=0;bool skip=false,protect=false;};
        QMap<QString,Group> groups;QMap<QString,QString> eventSources;QSet<QString> chosen;
        for(const auto &e:p.generated){if(cancel.load())throw Failure("Cancelled");eventSources[e.id]=e.source;if(selectedEvents.contains(e.id))chosen.insert(e.source);
            if(!groups.contains(e.source))groups[e.source]={e.source,e.start,e.end,false,false};
            auto &g=groups[e.source];g.a=std::min(g.a,e.start);g.b=std::max(g.b,e.end);
            if(e.unknown||e.provenance.startsWith("special/")||e.shape=="breath")g.skip=true;
        }
        for(const auto &track:p.score.tracks)for(const auto &note:track.notes){if(cancel.load())throw Failure("Cancelled");
            QString control=p.rules.readings.value(note.id).trimmed();if(control.isEmpty())control=note.phonemes.trimmed();
            control=control.normalized(QString::NormalizationForm_KC).toLower();
            if(groups.contains(note.id)&&(control=="cl"||control=="br"))groups[note.id].skip=true;
        }
        for(auto i=p.overrides.begin();i!=p.overrides.end();++i){if(cancel.load())throw Failure("Cancelled");auto id=eventSources.value(i.key());if(groups.contains(id))groups[id].protect=true;
            for(const auto &parent:i->parentIds){id=eventSources.value(parent);if(groups.contains(id))groups[id].protect=true;}
        }
        QVector<Group> sorted;for(const auto &g:groups)sorted.append(g);
        std::sort(sorted.begin(),sorted.end(),[](const Group&a,const Group&b){return a.a==b.a?a.id<b.id:a.a<b.a;});
        QVector<double> prefixEnd;double latest=-std::numeric_limits<double>::infinity();for(const auto &g:sorted){latest=std::max(latest,g.b);prefixEnd.append(latest);}
        auto boundary=[&](double expected,double radius,bool onset)->std::optional<double>{
            double from=std::max(0.,expected-radius),to=std::min(wave.duration(),expected+radius);
            if(to<=from||radius<Waveform::Step)return {};
            int a=std::max(2,int(std::ceil((from-1e-9)/Waveform::Step))),b=std::min(int(wave.bins.size())-2,int(std::floor((to+1e-9)/Waveform::Step)));
            float peak=0;for(int i=std::max(0,a-2);i<std::min(int(wave.bins.size()),b+3);++i)peak=std::max(peak,wave.bins[i].rms);
            if(peak<.002f)return {};
            float threshold=std::max(.0005f,peak*.15f);std::optional<double> found;
            for(int i=a;i<=b;++i){bool before=wave.bins[i-1].rms>=threshold&&wave.bins[i-2].rms>=threshold;
                bool quietBefore=wave.bins[i-1].rms<threshold&&wave.bins[i-2].rms<threshold;
                bool after=wave.bins[i].rms>=threshold&&wave.bins[i+1].rms>=threshold;
                bool quietAfter=wave.bins[i].rms<threshold&&wave.bins[i+1].rms<threshold;
                if(onset?(quietBefore&&after):(before&&quietAfter)){if(found)return {};found=i*Waveform::Step;}
            }
            return found;
        };
        const double offset=p.output.syncOffset-p.output.audioOffset;
        for(int i=0;i<sorted.size();++i){if(cancel.load()){result.cancelled=true;layer.sources.clear();return result;}
            if(progress&&i%128==0)progress(i*90/std::max(1,int(sorted.size())));
            const auto &g=sorted[i];if(!selectedEvents.isEmpty()&&!chosen.contains(g.id))continue;
            ++result.considered;if(g.protect){++result.protectedSources;continue;}if(g.skip){++result.unclear;continue;}
            const double radius=layer.maxShift;
            auto a=boundary(g.a+offset,radius,true),b=boundary(g.b+offset,radius,false);
            if(!a&&!b){++result.unclear;continue;}
            SourceTiming s{g.a,g.b,a?*a-offset:g.a,b?*b-offset:g.b};
            // A source's internal shapes keep their score proportions and identity.
            if(s.end<=s.start||std::abs((s.end-s.start)-(g.b-g.a))>(g.b-g.a)*layer.maxDurationChange+1e-9){++result.conflicts;continue;}
            if(std::abs(s.start-g.a)<.005&&std::abs(s.end-g.b)<.005){++result.unclear;continue;}
            // Original overlapping/fragmented routing stays unchanged. Adjacent
            // suggestions may cross their old shared edge if they fit together.
            bool conflict=false;
            if(i>0&&prefixEnd[i-1]>g.a+1e-9)conflict=true;
            if(i+1<sorted.size()&&sorted[i+1].a<g.b-1e-9)conflict=true;
            if(i>0&&prefixEnd[i-1]>sorted[i-1].b+1e-9&&s.start<prefixEnd[i-1]-1e-9)conflict=true;
            if(conflict){++result.conflicts;continue;}
            layer.sources[g.id]=s;
            if(progress)progress((i+1)*90/std::max(1,int(sorted.size())));
        }
        // Reject collisions and revisit affected neighbours until stable. Nothing
        // is pushed forward: every surviving interval is an independent estimate.
        QQueue<int> pairs;QSet<int> queued;
        auto enqueue=[&](int i){if(i>0&&i<sorted.size()&&!queued.contains(i)){pairs.enqueue(i);queued.insert(i);}};
        for(int i=1;i<sorted.size();++i)enqueue(i);
        while(!pairs.isEmpty()){
            if(cancel.load())throw Failure("Cancelled");
            int i=pairs.dequeue();queued.remove(i);
            const auto &a=sorted[i-1],&b=sorted[i];double end=layer.sources.contains(a.id)?layer.sources[a.id].end:a.b;double start=layer.sources.contains(b.id)?layer.sources[b.id].start:b.a;
            if(a.b<=b.a+1e-9&&end>start+1e-9){int left=layer.sources.remove(a.id),right=layer.sources.remove(b.id);result.conflicts+=left+right;if(left)enqueue(i-1);if(right)enqueue(i+1);}
        }
        if(cancel.load()){result.cancelled=true;layer.sources.clear();return result;}
        result.accepted=layer.sources.size();layer.enabled=result.accepted>0;
        Project check=p;check.timing=layer;validateTimingCorrection(check);if(progress)progress(100);
    }catch(const std::exception &e){if(cancel.load())result.cancelled=true;else result.error=QString::fromUtf8(e.what());layer.sources.clear();layer.enabled=false;}
    return result;
}
}
