// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include <QXmlStreamReader>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
namespace chosuta {
namespace {
[[noreturn]] void invalid(const QString &path,const QString &field){throw Failure(Message::format("MusicXML %1: invalid or unsupported field %2",{path,field}));}
[[noreturn]] void precision(){throw Failure("MusicXML musical precision exceeds the supported integer range");}
qint64 multiply(qint64 a,qint64 b){
    if(b<0){a=-a;b=-b;}
    if(b&&std::abs(a)>std::numeric_limits<qint64>::max()/b)precision();
    return a*b;
}
// Exact quarter-note fractions: divisions may change, and both divisions and
// duration are XML decimals. Never accumulate rounded blicks or normalize to 480.
struct QTime {
    qint64 n=0,d=1;
    QTime(qint64 numerator=0,qint64 denominator=1):n(numerator),d(denominator){
        if(d<=0||n==std::numeric_limits<qint64>::min())precision();
        const auto g=std::gcd(n,d);n/=g;d/=g;
    }
    bool operator==(const QTime &)const=default;
    bool operator<(const QTime &b)const {
        if((n<0)!=(b.n<0))return n<0;
        qint64 an=std::abs(n),ad=d,bn=std::abs(b.n),bd=b.d;bool reverse=n<0;
        for(;;){
            const auto x=an/ad,y=bn/bd;if(x!=y)return reverse?x>y:x<y;
            const auto ar=an%ad,br=bn%bd;if(!ar||!br)return reverse?ar>br:ar<br;
            an=ad;ad=ar;bn=bd;bd=br;reverse=!reverse;
        }
    }
    QTime operator+(const QTime &b)const {
        const auto g=std::gcd(d,b.d),a1=multiply(n,b.d/g),b1=multiply(b.n,d/g);
        if((b1>0&&a1>std::numeric_limits<qint64>::max()-b1)||(b1<0&&a1<-std::numeric_limits<qint64>::max()-b1))precision();
        return {a1+b1,multiply(d,b.d/g)};
    }
    QTime operator-(const QTime &b)const{return *this+QTime(-b.n,b.d);}
    QTime operator/(const QTime &b)const {
        if(b.n<=0)precision();
        const auto g=std::gcd(n,b.n),h=std::gcd(b.d,d);
        return {multiply(n/g,b.d/h),multiply(d/h,b.n/g)};
    }
};
QTime decimal(QString value,const QString &path,const QString &field){
    static const QRegularExpression pattern("^[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)$");
    value=value.trimmed();if(!pattern.match(value).hasMatch())invalid(path,field);
    const bool negative=value.startsWith('-');if(value.startsWith('-')||value.startsWith('+'))value.remove(0,1);
    const auto point=value.indexOf('.');int places=point<0?0:int(value.size()-point-1);if(point>=0)value.remove(point,1);
    while(places&&value.endsWith('0')){value.chop(1);--places;}
    bool ok=false;const auto n=value.toLongLong(&ok);if(!ok)precision();qint64 d=1;
    while(places--)d=multiply(d,10);
    return {negative?-n:n,d};
}
qint64 integer(const QString &value,const QString &path,const QString &field,qint64 low,qint64 high){
    bool ok=false;const auto n=value.toLongLong(&ok);if(!ok||n<low||n>high)invalid(path,field);return n;
}
struct Lyric {QString key,text,syllabic,extend,language;bool hasText=false;};
struct XNote {
    QTime at,duration;QString rawDuration,divisions,voice="1",staff="1",location,language,step,alter="0",octave;
    int ordinal=0,pitch=60,transpose=0;bool rest=false,chord=false,grace=false,cue=false,startTie=false,stopTie=false;
    QVector<Lyric> lyrics;
};
struct XTempo {QTime at;double bpm=120;QString path;};
struct Measure {QString number,path;bool implicit=false;QTime extent;QVector<XNote> notes;QVector<XTempo> tempos;std::optional<Meter> meter;};
struct Part {QString id,name,language;QVector<Measure> measures;};
class MusicXmlReader {
    QXmlStreamReader xml;Score score;QVector<Part> parts;QMap<QString,QString> names;
    const std::atomic_bool *cancel;Progress progress;qsizetype bytes;int reported=-1,notes=0;
    qint64 resolution=1;QString location="$",rootLanguage;
    QSet<QString> warnings;
    void check(){if(cancel&&cancel->load())throw Failure("Cancelled");}
    bool next(){
        check();const int p=int(std::min<qint64>(75,std::max<qint64>(0,xml.characterOffset())*75/std::max<qsizetype>(1,bytes)));
        if(progress&&reported!=p){reported=p;progress(p);}return xml.readNextStartElement();
    }
    QString text(){return xml.readElementText();}
    QString language()const{return xml.attributes().value("http://www.w3.org/XML/1998/namespace","lang").toString();}
    void unit(qint64 d){resolution=multiply(resolution/std::gcd(resolution,d),d);}
    QTime duration(const QString &s,const QTime &division){
        const auto v=decimal(s,location,"duration");if(v.n<=0||division.n<=0)invalid(location,"duration/divisions");
        const auto t=v/division;unit(t.d);return t;
    }
    void warn(const QString &code,const char *key){
        const auto id=location+"/"+code;if(warnings.contains(id))return;warnings.insert(id);score.diagnostics.append({location,code,Message(key)});
    }
    void playback(){warn("notated-order","MusicXML is imported once in notated order; repeats, endings and jumps are not expanded");}
    Lyric lyric(){
        Lyric l;l.key=xml.attributes().value("number").toString();if(l.key.isEmpty())l.key=xml.attributes().value("name").toString();if(l.key.isEmpty())l.key="1";l.language=language();
        while(next()){
            if(xml.name()==QLatin1String("text")){if(!language().isEmpty())l.language=language();l.hasText=true;l.text+=text();}
            else if(xml.name()==QLatin1String("elision")){xml.skipCurrentElement();l.text+=' ';}
            else if(xml.name()==QLatin1String("syllabic"))l.syllabic=text();
            else if(xml.name()==QLatin1String("extend")){l.extend=xml.attributes().value("type").toString();if(l.extend.isEmpty())l.extend="start";xml.skipCurrentElement();}
            else xml.skipCurrentElement();
        }
        return l;
    }
    void tie(XNote &n){const auto type=xml.attributes().value("type");n.startTie|=type==QLatin1String("start");n.stopTie|=type==QLatin1String("stop");xml.skipCurrentElement();}
    XNote note(const QTime &division,const QString &divisionText,const QMap<QString,int> &transpositions){
        if(++notes>200000)throw Failure("More than 200000 notes");
        XNote n;n.ordinal=notes;n.location=location;n.divisions=divisionText;n.language=language();
        while(next()){
            const auto tag=xml.name();
            if(tag==QLatin1String("duration"))n.rawDuration=text();
            else if(tag==QLatin1String("voice"))n.voice=text();
            else if(tag==QLatin1String("staff"))n.staff=text();
            else if(tag==QLatin1String("lyric"))n.lyrics.append(lyric());
            else if(tag==QLatin1String("pitch")){
                while(next()){
                    if(xml.name()==QLatin1String("step"))n.step=text();
                    else if(xml.name()==QLatin1String("octave"))n.octave=text();
                    else if(xml.name()==QLatin1String("alter"))n.alter=text();
                    else xml.skipCurrentElement();
                }
            }else if(tag==QLatin1String("tie"))tie(n);
            else if(tag==QLatin1String("notations")){
                while(next()){if(xml.name()==QLatin1String("tied"))tie(n);else xml.skipCurrentElement();}
            }else if(tag==QLatin1String("rest")){n.rest=true;xml.skipCurrentElement();}
            else if(tag==QLatin1String("chord")){n.chord=true;xml.skipCurrentElement();}
            else if(tag==QLatin1String("grace")){n.grace=true;xml.skipCurrentElement();}
            else if(tag==QLatin1String("cue")){n.cue=true;xml.skipCurrentElement();}
            else xml.skipCurrentElement();
        }
        if(!n.grace)n.duration=duration(n.rawDuration,division);
        if(!n.rest&&!n.grace&&!n.cue&&!n.chord){
            const int step=QString("C D EF G A B").indexOf(n.step);
            if(n.step.size()!=1||step<0||n.step==" ")invalid(location,"pitch.step/unpitched");
            const auto alteration=decimal(n.alter,location,"pitch.alter");if(alteration.d!=1||alteration.n < -127||alteration.n>127)invalid(location,"pitch.alter");
            n.transpose=transpositions.value(n.staff,transpositions.value("*"));
            const auto pitch=(integer(n.octave,location,"pitch.octave",-1,9)+1)*12+step+alteration.n+n.transpose;
            if(pitch<0||pitch>127)invalid(location,"MIDI pitch");
            n.pitch=int(pitch);
        }
        return n;
    }
    void attributes(Measure &m,const QTime &cursor,QTime &division,QString &divisionText,QMap<QString,int> &transpositions){
        while(next()){
            if(xml.name()==QLatin1String("divisions")){
                divisionText=text();division=decimal(divisionText,location,"divisions");if(division.n<=0)invalid(location,"divisions");unit(division.n);
            }else if(xml.name()==QLatin1String("time")){
                Meter meter;meter.numerator=0;QString beats;
                while(next()){
                    if(xml.name()==QLatin1String("beats"))beats=text();
                    else if(xml.name()==QLatin1String("beat-type")){
                        const int d=int(integer(text(),location,"beat-type",1,64));if(d&(d-1))invalid(location,"beat-type");
                        int n=0;for(const auto &item:beats.split('+'))n+=int(integer(item,location,"beats",1,64));
                        if(meter.numerator&&meter.denominator!=d)invalid(location,"mixed beat units");
                        meter.numerator+=n;meter.denominator=d;beats.clear();
                    }else if(xml.name()==QLatin1String("senza-misura"))invalid(location,"senza-misura");
                    else xml.skipCurrentElement();
                }
                if(cursor.n||meter.numerator<1||meter.numerator>64)invalid(location,"time signature");
                if(m.meter&&(m.meter->numerator!=meter.numerator||m.meter->denominator!=meter.denominator))invalid(location,"staff-specific meters");
                m.meter=meter;
            }else if(xml.name()==QLatin1String("transpose")){
                auto staff=xml.attributes().value("number").toString();if(staff.isEmpty())staff="*";int chromatic=0,octaves=0;
                while(next()){
                    if(xml.name()==QLatin1String("chromatic"))chromatic=int(integer(text(),location,"transpose.chromatic",-127,127));
                    else if(xml.name()==QLatin1String("octave-change"))octaves=int(integer(text(),location,"transpose.octave-change",-10,10));
                    else xml.skipCurrentElement();
                }
                transpositions[staff]=chromatic+12*octaves;
            }else if(xml.name()==QLatin1String("measure-style")){
                while(next()){if(xml.name()==QLatin1String("measure-repeat")||xml.name()==QLatin1String("beat-repeat"))playback();xml.skipCurrentElement();}
            }else xml.skipCurrentElement();
        }
    }
    std::optional<double> sound(const QTime &division,std::optional<QTime> &offset){
        const auto attrs=xml.attributes();std::optional<double> bpm;
        if(attrs.hasAttribute("tempo")){bool ok=false;const double value=attrs.value("tempo").toDouble(&ok);if(!ok||!std::isfinite(value)||value<=0||value>10000)invalid(location,"sound.tempo");bpm=value;}
        for(const auto &key:{"dacapo","dalsegno","tocoda","fine","forward-repeat"})if(attrs.hasAttribute(QLatin1String(key)))playback();
        while(next()){
            if(xml.name()==QLatin1String("offset")){
                const auto v=decimal(text(),location,"sound.offset");offset=v.n?v/division:QTime();
            }else xml.skipCurrentElement();
        }
        return bpm;
    }
    void direction(Measure &m,const QTime &cursor,const QTime &division){
        std::optional<double> explicitTempo,printedTempo;QTime offset;std::optional<QTime> soundOffset;
        while(next()){
            if(xml.name()==QLatin1String("offset")){const auto v=decimal(text(),location,"direction.offset");offset=v.n?v/division:QTime();}
            else if(xml.name()==QLatin1String("sound"))explicitTempo=sound(division,soundOffset);
            else if(xml.name()==QLatin1String("direction-type")){
                while(next()){
                    if(xml.name()!=QLatin1String("metronome")){xml.skipCurrentElement();continue;}
                    QString beat,perMinute;int dots=0,beats=0;
                    while(next()){
                        if(xml.name()==QLatin1String("beat-unit")){beat=text();++beats;}
                        else if(xml.name()==QLatin1String("beat-unit-dot")){++dots;xml.skipCurrentElement();}
                        else if(xml.name()==QLatin1String("per-minute"))perMinute=text();
                        else xml.skipCurrentElement();
                    }
                    static const QMap<QString,double> units{{"maxima",32},{"long",16},{"breve",8},{"whole",4},{"half",2},{"quarter",1},{"eighth",.5},{"16th",.25},{"32nd",.125},{"64th",.0625},{"128th",.03125}};
                    bool ok=false;const double bpm=perMinute.toDouble(&ok)*units.value(beat)*(2-std::pow(.5,dots));
                    if(ok&&beats==1&&dots<16&&bpm>0&&bpm<=10000)printedTempo=bpm;
                    else warn("metronome","MusicXML metronome text or modulation could not be converted to a numeric tempo");
                }
            }else xml.skipCurrentElement();
        }
        const auto tempo=explicitTempo?explicitTempo:printedTempo;
        if(tempo){const auto at=cursor+(explicitTempo&&soundOffset?*soundOffset:offset);unit(at.d);m.tempos.append({at,*tempo,location});}
    }
    Part part(int index){
        Part p;p.id=xml.attributes().value("id").toString();p.name=names.value(p.id,QString("Part %1").arg(index+1));p.language=language();if(p.language.isEmpty())p.language=rootLanguage;
        if(p.id.isEmpty())invalid(location,"part.id");
        for(const auto &other:parts)if(other.id==p.id)invalid(location,"duplicate part.id");
        QTime division;QString divisionText;QMap<QString,int> transpositions;
        while(next()){
            if(xml.name()!=QLatin1String("measure")){xml.skipCurrentElement();continue;}
            Measure m;m.number=xml.attributes().value("number").toString();m.implicit=xml.attributes().value("implicit")==QLatin1String("yes");
            m.path=QString("part[%1]/measure[%2] (%3)").arg(index).arg(p.measures.size()).arg(m.number);location=m.path;
            if(xml.attributes().value("non-controlling")==QLatin1String("yes"))invalid(location,"non-controlling measure");
            QTime cursor,chordStart,chordDuration;bool canChord=false;int ordinal=0;
            while(next()){
                location=m.path+QString("/item[%1]").arg(ordinal++);
                if(xml.name()==QLatin1String("attributes"))attributes(m,cursor,division,divisionText,transpositions);
                else if(xml.name()==QLatin1String("note")){
                    auto n=note(division,divisionText,transpositions);
                    if(n.grace){warn("grace","MusicXML grace notes have no independent duration and are retained only in the source");canChord=false;continue;}
                    if(n.chord){if(!canChord||chordDuration<n.duration)invalid(location,"chord");n.at=chordStart;warn("chord","MusicXML imports the first note of each same-voice chord; additional chord tones remain in the source");}
                    else{n.at=cursor;chordStart=cursor;chordDuration=n.duration;cursor=cursor+n.duration;canChord=true;}
                    m.extent=std::max(m.extent,n.at+n.duration);
                    if(n.cue)warn("cue","MusicXML cue notes are retained in the source but are not imported as singing notes");
                    if(!n.rest&&!n.chord&&!n.cue)m.notes.append(n);
                }else if(xml.name()==QLatin1String("backup")||xml.name()==QLatin1String("forward")){
                    const bool backup=xml.name()==QLatin1String("backup");QString raw;
                    while(next()){if(xml.name()==QLatin1String("duration"))raw=text();else xml.skipCurrentElement();}
                    const auto d=duration(raw,division);cursor=backup?cursor-d:cursor+d;if(cursor.n<0)invalid(location,"backup before measure");m.extent=std::max(m.extent,cursor);canChord=false;
                }else if(xml.name()==QLatin1String("direction"))direction(m,cursor,division);
                else if(xml.name()==QLatin1String("sound")){std::optional<QTime> offset;const auto bpm=sound(division,offset);if(bpm)m.tempos.append({cursor+offset.value_or(QTime()),*bpm,location});}
                else if(xml.name()==QLatin1String("barline")){
                    while(next()){if(xml.name()==QLatin1String("repeat")||xml.name()==QLatin1String("ending"))playback();xml.skipCurrentElement();}
                }else xml.skipCurrentElement();
            }
            unit(m.extent.d);p.measures.append(m);
        }
        return p;
    }
    qint64 ticks(const QTime &v)const {return multiply(v.n,resolution/v.d);}
    qint64 blicks(const QTime &v)const {return sourceBlick(ticks(v),resolution);}
public:
    MusicXmlReader(const QByteArray &data,const std::atomic_bool *c,Progress p):xml(data),cancel(c),progress(p),bytes(data.size()){}
    Score read(){
        if(!next()||xml.name()!=QLatin1String("score-partwise"))throw Failure("MusicXML requires an uncompressed score-partwise document");
        rootLanguage=language();score.sourceVersion=xml.attributes().value("version").toString();
        if(!xml.attributes().hasAttribute("version"))score.sourceVersion="1.0";
        if(!QStringList{"1.0","1.1","2.0","3.0","3.1","4.0"}.contains(score.sourceVersion))warn("unverified-version","MusicXML version is not independently verified; importing the supported partwise fields");
        while(next()){
            if(xml.name()==QLatin1String("part-list")){
                while(next()){
                    if(xml.name()!=QLatin1String("score-part")){xml.skipCurrentElement();continue;}
                    const auto id=xml.attributes().value("id").toString();QString name;
                    while(next()){if(xml.name()==QLatin1String("part-name"))name=text();else xml.skipCurrentElement();}
                    if(id.isEmpty()||names.contains(id))invalid("part-list","score-part.id");
                    names[id]=name;
                }
            }else if(xml.name()==QLatin1String("part"))parts.append(part(int(parts.size())));
            else xml.skipCurrentElement();
        }
        while(!xml.atEnd()){check();xml.readNext();}
        if(xml.hasError())throw Failure(Message::format("Cannot read MusicXML at line %1, column %2",{QString::number(xml.lineNumber()),QString::number(xml.columnNumber())}).withDetail(xml.errorString()));
        if(parts.isEmpty())invalid("$","part");
        int count=0;for(const auto &p:parts)count=std::max(count,int(p.measures.size()));
        QVector<QTime> starts{QTime()};QVector<Meter> meterMap;Meter current;
        for(int mi=0;mi<count;++mi){
            check();std::optional<Meter> specified;QTime extent;bool implicit=false;QString number;
            for(const auto &p:parts)if(mi<p.measures.size()){
                const auto &m=p.measures[mi];location=m.path;
                if(!number.isEmpty()&&number!=m.number)invalid(location,"unaligned measure numbers");
                number=m.number;
                extent=std::max(extent,m.extent);implicit|=m.implicit;
                if(m.meter){if(specified&&(specified->numerator!=m.meter->numerator||specified->denominator!=m.meter->denominator))invalid(location,"conflicting part meters");specified=m.meter;}
            }
            if(mi==0&&!specified){location="$";warn("default-meter","MusicXML has no initial meter; using 4/4 for bar and beat labels");}
            if(specified)current=*specified;
            current.bar=mi;
            if(meterMap.isEmpty()||meterMap.last().numerator!=current.numerator||meterMap.last().denominator!=current.denominator||(starts.size()>1&&starts[mi]-starts[mi-1]!=QTime(meterMap.last().numerator*4,meterMap.last().denominator)))meterMap.append(current);
            const QTime nominal(current.numerator*4,current.denominator);unit(nominal.d);
            if(mi==0&&implicit&&extent.n&&extent<nominal)score.time.pickupOffset=blicks(nominal-extent);
            if(!implicit||extent.n==0)extent=std::max(extent,nominal);
            starts.append(starts.last()+extent);unit(starts.last().d);
        }
        score.sourceResolution=resolution;score.time.meters=meterMap;if(count==0)score.time.meters={{0,4,4}};
        for(const auto &v:starts)score.time.measureStarts.append(blicks(v));
        score.endPosition=blicks(starts.last());
        QMap<QTime,double> tempos;
        for(const auto &p:parts)for(int mi=0;mi<p.measures.size();++mi)for(const auto &t:p.measures[mi].tempos){
            const auto at=starts[mi]+t.at;unit(at.d);if(at.n<0)invalid(t.path,"tempo before score");
            if(tempos.contains(at)&&std::abs(tempos.value(at)-t.bpm)>1e-8)invalid(t.path,"conflicting part tempos");
            tempos[at]=t.bpm;
        }
        if(!tempos.contains(QTime())){tempos[QTime()]=120;location="$";warn("default-tempo","MusicXML has no initial numeric tempo; using 120 BPM");}
        score.sourceResolution=resolution;score.time.tempos.clear();for(auto it=tempos.cbegin();it!=tempos.cend();++it)score.time.tempos.append({blicks(it.key()),it.value()});
        int expanded=0;
        for(int pi=0;pi<parts.size();++pi){
            check();if(progress)progress(76+pi*23/std::max(1,int(parts.size())));
            const auto &p=parts[pi];QMap<QString,QVector<XNote>> voices;QStringList voiceOrder;
            for(int mi=0;mi<p.measures.size();++mi)for(auto n:p.measures[mi].notes){
                const auto key=QString::fromLatin1(QUrl::toPercentEncoding(n.staff))+"/"+QString::fromLatin1(QUrl::toPercentEncoding(n.voice));if(!voices.contains(key))voiceOrder.append(key);
                n.at=starts[mi]+n.at;
                voices[key].append(n);
            }
            for(const auto &voice:voiceOrder){
                auto raw=voices.value(voice);std::stable_sort(raw.begin(),raw.end(),[](const auto &a,const auto &b){return a.at<b.at;});
                QStringList verses;for(const auto &n:raw)for(const auto &l:n.lyrics)if(!verses.contains(l.key))verses.append(l.key);if(verses.isEmpty())verses.append("1");
                for(const auto &verse:verses){
                    Track track;track.id=QString("musicxml/part:%1/staff:%2/voice:%3/verse:%4").arg(pi).arg(QString::fromLatin1(QUrl::toPercentEncoding(raw.front().staff)),QString::fromLatin1(QUrl::toPercentEncoding(raw.front().voice)),QString::fromLatin1(QUrl::toPercentEncoding(verse)));
                    track.name=p.name;if(voiceOrder.size()>1||verses.size()>1)track.name+=QString(" [%1; verse %2]").arg(voice,verse);
                    QTime lastEnd;bool hasPrevious=false,melisma=false,tied=false;int lastPitch=-1;
                    for(const auto &x:raw){
                        check();if(++expanded>200000)throw Failure("More than 200000 notes");
                        const bool contiguous=hasPrevious&&lastEnd==x.at;Lyric l;bool found=false;
                        for(const auto &item:x.lyrics)if(item.key==verse){if(found)invalid(x.location,"duplicate lyric number");l=item;found=true;}
                        Note n;n.id=track.id+QString("/note:%1").arg(x.ordinal);n.groupInstance=track.id;n.onset=blicks(x.at);n.duration=blicks(x.at+x.duration)-n.onset;n.pitch=x.pitch;
                        if(n.duration<=0)invalid(x.location,"duration below blick precision");
                        n.sourceOnset=ticks(x.at);n.sourceDuration=ticks(x.duration);n.lyrics=l.text;
                        n.language=!l.language.isEmpty()?l.language:!x.language.isEmpty()?x.language:p.language;
                        n.language=n.language.toLower().section('-',0,0);if(n.language.isEmpty())n.language="auto";
                        if(x.stopTie&&(!contiguous||!tied||lastPitch!=x.pitch)){location=x.location;warn("unresolved-tie","MusicXML tie has no contiguous matching start; no continuation was inferred");}
                        const bool tieContinuation=x.stopTie&&contiguous&&tied&&lastPitch==x.pitch;
                        if(tieContinuation||(!l.hasText&&contiguous&&(melisma||l.extend=="continue"||l.extend=="stop")))n.lyricReading="ー";
                        n.original={{"path",x.location},{"partId",p.id},{"staff",x.staff},{"voice",x.voice},{"verse",verse},{"duration",x.rawDuration},{"divisions",x.divisions},{"quarterNumerator",QString::number(x.at.n)},{"quarterDenominator",QString::number(x.at.d)},{"step",x.step},{"alter",x.alter},{"octave",x.octave},{"transpose",x.transpose},{"lyrics",l.text},{"syllabic",l.syllabic},{"extend",l.extend},{"tieStart",x.startTie},{"tieStop",x.stopTie}};
                        if(l.hasText||!contiguous)melisma=false;
                        if(l.extend=="start"||l.extend=="continue")melisma=true;else if(l.extend=="stop")melisma=false;
                        tied=x.startTie;lastPitch=x.pitch;lastEnd=x.at+x.duration;hasPrevious=true;track.notes.append(n);
                    }
                    score.tracks.append(track);
                }
            }
        }
        score.diagnostics.append({"MusicXML","score-timing",Message("MusicXML mouth shapes and phoneme timing are estimated from the score; choose the required staff, voice and lyric verse")});
        return score;
    }
};
}
Score parseMusicXml(const QByteArray &bytes,const QString &,const std::atomic_bool *cancel,Progress progress){
    if(bytes.size()>64*1024*1024)throw Failure("Cannot read project (limit 64 MiB)");
    return MusicXmlReader(bytes,cancel,progress).read();
}
}
