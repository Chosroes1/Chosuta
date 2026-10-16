// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include <unicode/ucnv.h>
#include <unicode/ucnv_err.h>
#include <algorithm>
#include <cmath>
namespace chosuta {
namespace {
[[noreturn]] void invalid(const QString &detail){throw Failure(Message("Cannot read VSQ data").withDetail(detail));}
qint64 integer(const QString &text){bool ok=false;auto n=text.toLongLong(&ok);if(!ok||n<0||n>MusicLimit)invalid("Invalid integer: "+text);return n;}
QString decode(const QByteArray &bytes){
    // Join DM byte blocks BEFORE decoding: a CP932 character can cross blocks.
    UErrorCode error=U_ZERO_ERROR;auto codec=ucnv_open("windows-932",&error);
    if(U_FAILURE(error))invalid("CP932 converter unavailable");
    ucnv_setToUCallBack(codec,UCNV_TO_U_CALLBACK_STOP,nullptr,nullptr,nullptr,&error);
    int32_t size=ucnv_toUChars(codec,nullptr,0,bytes.constData(),bytes.size(),&error);
    if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error)){ucnv_close(codec);invalid("Invalid CP932 text");}
    error=U_ZERO_ERROR;QString text(size,Qt::Uninitialized);
    ucnv_toUChars(codec,reinterpret_cast<UChar *>(text.data()),size,bytes.constData(),bytes.size(),&error);ucnv_close(codec);
    if(U_FAILURE(error))invalid("Invalid CP932 text");
    return text;
}
QStringList csv(const QString &text){
    QStringList fields;QString value;bool quoted=false,closed=false;
    for(qsizetype i=0;i<text.size();++i){auto c=text[i];
        if(quoted){if(c=='"'){if(i+1<text.size()&&text[i+1]=='"'){value+='"';++i;}else{quoted=false;closed=true;}}else value+=c;}
        else if(c==','){fields.append(value);value.clear();closed=false;}
        else if(c=='"'&&value.isEmpty()&&!closed)quoted=true;
        else if(closed){if(!c.isSpace())invalid("Invalid lyric CSV");}
        else value+=c;
    }
    if(quoted)invalid("Unclosed lyric CSV quote");
    fields.append(value);return fields;
}
using Fields=QMap<QString,QString>;
struct TextTrack {int midiIndex=0;QMap<QString,Fields> sections;QVector<QPair<qint64,QString>> events;};
struct Cursor {
    const QByteArray &data;qsizetype pos=0,end;
    explicit Cursor(const QByteArray &b):data(b),end(b.size()){}
    void need(qint64 n)const{if(n<0||n>end-pos)invalid("Truncated MIDI chunk/event");}
    quint32 number(int n){need(n);quint32 v=0;while(n--)v=(v<<8)|uchar(data[pos++]);return v;}
    QByteArray take(qint64 n){need(n);auto b=data.mid(pos,n);pos+=n;return b;}
    quint32 variable(){quint32 v=0;for(int i=0;i<4;++i){auto b=number(1);v=(v<<7)|(b&127);if(!(b&128))return v;}invalid("Invalid MIDI variable length");}
};
class Reader {
    const QByteArray &bytes;const std::atomic_bool *cancel;Progress progress;int reported=-1;
    Score score;QVector<TextTrack> tracks;
    QMap<qint64,double> tempos{{0,120}};QMap<qint64,QPair<int,int>> meters{{0,{4,4}}};
    void check(int percent){if(cancel&&cancel->load())throw Failure("Cancelled");if(progress&&percent>reported){reported=percent;progress(percent);}if(cancel&&cancel->load())throw Failure("Cancelled");}
    void parseText(const QByteArray &raw,int index){
        TextTrack track;track.midiIndex=index;QString section;
        const auto lines=decode(raw).split('\n');
        for(int i=0;i<lines.size();++i){check(55+int(15LL*i/std::max(1,int(lines.size()))));QString line=lines[i];if(line.endsWith('\r'))line.chop(1);if(line.isEmpty())continue;
            if(line.startsWith('[')&&line.endsWith(']')){section=line.mid(1,line.size()-2);if(track.sections.contains(section))invalid("Duplicate section: "+section);track.sections[section]={};}
            else{auto equal=line.indexOf('=');if(section.isEmpty()||equal<0)invalid("Invalid VSQ text line");const auto key=line.left(equal),value=line.mid(equal+1);
                if(section=="EventList")track.events.append({integer(key),value});
                else{auto &fields=track.sections[section];if(fields.contains(key))invalid("Duplicate field: "+key);fields[key]=value;}
            }
        }
        if(track.sections.value("Common").value("Version")!="DSB301")throw Failure("Unsupported VSQ version");
        if(!track.sections.contains("EventList"))invalid("EventList required");
        std::stable_sort(track.events.begin(),track.events.end(),[](const auto &a,const auto &b){return a.first<b.first;});tracks.append(track);
    }
    void midi(){
        Cursor file(bytes);if(file.take(4)!="MThd")throw Failure("Not a VSQ MIDI project");auto headerSize=file.number(4);if(headerSize<6)invalid("Invalid MIDI header");
        auto format=file.number(2),count=file.number(2),resolution=file.number(2);file.take(headerSize-6);
        if(format!=1||count<2||(resolution&0x8000)||resolution==0)throw Failure("VSQ requires type 1 MIDI with PPQ timing");
        score.sourceResolution=resolution;
        QVector<QByteArray> texts;
        for(quint32 ti=0;ti<count;++ti){
            check(int(50LL*file.pos/std::max<qsizetype>(1,bytes.size())));if(file.take(4)!="MTrk")invalid("Expected MTrk chunk");auto length=file.number(4);file.need(length);
            Cursor track(bytes);track.pos=file.pos;track.end=file.pos+length;file.pos=track.end;
            qint64 tick=0;int running=0;bool ended=false;QMap<qint64,QByteArray> blocks;
            while(track.pos<track.end){
                check(int(50LL*track.pos/std::max<qsizetype>(1,bytes.size())));tick+=track.variable();sourceBlick(tick,resolution);
                track.need(1);int status=uchar(bytes[track.pos]);if(status&128){++track.pos;if(status<0xf0)running=status;else running=0;}else{if(!running)invalid("Missing MIDI running status");status=running;}
                if(status==0xff){int type=track.number(1);auto payload=track.take(track.variable());
                    if(type==0x2f){if(!payload.isEmpty()||track.pos!=track.end)invalid("Invalid end-of-track");ended=true;break;}
                    if(type==1&&payload.startsWith("DM:")){
                        auto colon=payload.indexOf(':',3);if(colon<4)invalid("Invalid DM block");auto idText=payload.mid(3,colon-3);for(auto c:idText)if(c<'0'||c>'9')invalid("Invalid DM index");
                        auto id=integer(QString::fromLatin1(idText));if(blocks.contains(id))invalid("Duplicate DM block");blocks[id]=payload.mid(colon+1);
                    }else if(type==0x51||type==0x58){
                        if(ti!=0)invalid("Tempo/meter outside conductor track is unsupported");
                        if(type==0x51){if(payload.size()!=3)invalid("Invalid MIDI tempo");Cursor p(payload);auto micros=p.number(3);if(!micros)invalid("Zero MIDI tempo");tempos[tick]=60000000./micros;}
                        else{if(payload.size()!=4||uchar(payload[1])>6||uchar(payload[0])<1||uchar(payload[0])>64)invalid("Invalid MIDI meter");meters[tick]={uchar(payload[0]),1<<uchar(payload[1])};}
                    }
                }else if(status==0xf0||status==0xf7)track.take(track.variable());
                else if(status<0xf0){int length=(status>>4)==0xc||(status>>4)==0xd?1:2;auto payload=track.take(length);for(uchar c:payload)if(c&128)invalid("Invalid MIDI channel data");}
                else invalid("Unsupported MIDI event status");
            }
            if(!ended)invalid("MIDI end-of-track required");
            if(!blocks.isEmpty()){if(ti==0)invalid("VSQ text in conductor track is unsupported");QByteArray raw;qint64 expected=0;for(auto it=blocks.cbegin();it!=blocks.cend();++it){if(it.key()!=expected++)invalid("Missing DM block");raw+=it.value();}texts.append(raw);}else texts.append(QByteArray());
        }
        if(file.pos!=file.end)invalid("Unexpected data after MIDI tracks");
        for(int i=0;i<texts.size();++i)if(!texts[i].isEmpty())parseText(texts[i],i);
        if(tracks.isEmpty())throw Failure("No VSQ singing data found");
    }
    void time(){
        int pre=0;bool found=false;Fields mixer;
        for(const auto &t:tracks){const auto master=t.sections.value("Master");if(master.contains("PreMeasure")){auto value=integer(master.value("PreMeasure"));if(value>10000000)invalid("PreMeasure exceeds bar range");if(found&&pre!=value)invalid("Conflicting PreMeasure");pre=int(value);found=true;}
            if(t.sections.contains("Mixer")){auto next=t.sections.value("Mixer");if(!mixer.isEmpty()&&mixer!=next)invalid("Conflicting Mixer");mixer=next;}}
        QMap<int,Meter> bars;int bar=0; qint64 previous=0;QPair<int,int> active{4,4};
        for(auto it=meters.cbegin();it!=meters.cend();++it){auto ticksPerBar=qint64(score.sourceResolution)*4*active.first;auto distance=(it.key()-previous)*active.second;
            if(distance%ticksPerBar)invalid("Meter change outside a measure boundary");
            auto nextBar=qint64(bar)+distance/ticksPerBar;if(nextBar>10000000)invalid("Meter bar exceeds limit");bar=int(nextBar);
            bars[bar]={bar,it.value().first,it.value().second};previous=it.key();active=it.value();}
        bar=0;long double origin=0;Meter current;
        for(auto m:bars){if(m.bar>pre)break;origin+=static_cast<long double>(m.bar-bar)*score.sourceResolution*4*current.numerator/current.denominator;bar=m.bar;current=m;}
        origin+=static_cast<long double>(pre-bar)*score.sourceResolution*4*current.numerator/current.denominator;
        if(origin>MusicLimit||origin!=std::floor(origin))invalid("PreMeasure has non-integer source position");
        score.sourceTickOrigin=qint64(origin);
        QMap<int,Meter> normalized;for(auto m:bars){m.bar=std::max(0,m.bar-pre);normalized[m.bar]=m;}score.time.meters=normalized.values();
        QMap<qint64,double> normalizedTempo;for(auto it=tempos.cbegin();it!=tempos.cend();++it)normalizedTempo[sourceBlick(std::max<qint64>(0,it.key()-score.sourceTickOrigin),score.sourceResolution)]=it.value();
        score.time.tempos.clear();for(auto it=normalizedTempo.cbegin();it!=normalizedTempo.cend();++it)score.time.tempos.append({it.key(),it.value()});
        bool anySolo=false;for(int i=0;i<tracks.size();++i)anySolo|=integer(mixer.value("Solo"+QString::number(i),"0"))!=0;
        for(int ti=0;ti<tracks.size();++ti){check(70+int(29LL*ti/std::max(1,int(tracks.size()))));const auto &raw=tracks[ti];Track track;track.id=QString("vsq/track:%1").arg(raw.midiIndex);track.name=raw.sections.value("Common").value("Name");
            track.muted=integer(mixer.value("MasterMute","0"))!=0||integer(mixer.value("Mute"+QString::number(ti),"0"))!=0||(anySolo&&integer(mixer.value("Solo"+QString::number(ti),"0"))==0);
            QString language="auto";int ordinal=0;
            for(const auto &[position,ids]:raw.events){check(70+int(29LL*ti/std::max(1,int(tracks.size()))));
                for(const auto &id:ids.split(',')){
                    if(id=="EOS"){score.endPosition=std::max(score.endPosition,sourceBlick(std::max<qint64>(0,position-score.sourceTickOrigin),score.sourceResolution));continue;}
                    if(!raw.sections.contains(id))invalid("Missing event: "+id);
                    const auto event=raw.sections.value(id);auto type=event.value("Type");
                    if(type=="Singer"){auto handle=raw.sections.value(event.value("IconHandle"));if(handle.isEmpty())invalid("Missing singer handle");auto lang=integer(handle.value("Language","0"));language=lang==0?"ja":lang==1?"en":QString("vocaloid:%1").arg(lang);continue;}
                    if(type!="Anote"){score.diagnostics.append({track.id+"/"+id,"unsupported-event",Message::format("VSQ event type retained without interpretation: %1",{type})});continue;}
                    Note n;n.sourceOnset=position;n.sourceDuration=integer(event.value("Length"));auto pitch=integer(event.value("Note#"));if(pitch>127||!n.sourceDuration||n.sourceDuration>MusicLimit-position)invalid("Invalid VSQ note");n.pitch=int(pitch);
                    n.groupInstance=track.id+"/sequence:0";n.id=n.groupInstance+QString("/event:%1/%2").arg(ordinal++).arg(id);n.language=language;n.phoneset="vocaloid-xsampa";n.muted=track.muted;
                    const auto handleId=event.value("LyricHandle");const auto lyricFields=raw.sections.value(handleId);if(!handleId.isEmpty()){if(!raw.sections.contains(handleId)||!lyricFields.contains("L0"))invalid("Missing lyric handle/L0");auto lyric=csv(lyricFields.value("L0"));if(lyric.size()<2)invalid("Lyric and phonemes required in L0");n.lyrics=lyric[0];n.phonemes=lyric[1];
                        for(auto it=lyricFields.cbegin();it!=lyricFields.cend();++it)if(it.key().startsWith('L')&&it.key()!="L0"&&it.key().mid(1).toInt()>0)score.diagnostics.append({n.id,"additional-lyrics","Additional VSQ lyric entries are retained; only L0 is interpreted"});}
                    for(auto it=event.cbegin();it!=event.cend();++it)n.original[it.key()]=it.value();
                    n.original["eventId"]=id;QJsonObject handle;for(auto it=lyricFields.cbegin();it!=lyricFields.cend();++it)handle[it.key()]=it.value();n.original["lyricHandle"]=handle;
                    auto start=position-score.sourceTickOrigin,end=position+n.sourceDuration-score.sourceTickOrigin;
                    if(start<0){score.diagnostics.append({n.id,"premeasure-note","VSQ note before the source origin was clipped to the playback range"});if(end<=0)continue;start=0;}
                    n.onset=sourceBlick(start,score.sourceResolution);n.duration=sourceBlick(end,score.sourceResolution)-n.onset;score.endPosition=std::max(score.endPosition,n.onset+n.duration);track.notes.append(n);
                }
            }
            std::stable_sort(track.notes.begin(),track.notes.end(),[](const auto &a,const auto &b){return a.onset<b.onset;});score.tracks.append(track);
        }
    }
public:
    Reader(const QByteArray &b,const std::atomic_bool *c,Progress p):bytes(b),cancel(c),progress(p){}
    Score read(){check(0);midi();time();score.sourceVersion="DSB301";score.diagnostics.append({"VSQ","score-timing","VSQ timing uses notes and explicit phonemes, not rendered phoneme boundaries"});check(99);return score;}
};
}
Score parseVsq(const QByteArray &bytes,const QString &,const std::atomic_bool *cancel,Progress progress){return Reader(bytes,cancel,progress).read();}
}
