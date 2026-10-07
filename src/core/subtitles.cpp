// SPDX-License-Identifier: GPL-3.0-or-later
#include "model.h"
#include <algorithm>
#include <cmath>

namespace chosuta {
static QString sourceKey(const QString &track,const QString &note){return QString::number(track.size())+":"+track+note;}
QSet<QString> subtitleSources(const Project &p) {
    QSet<QString> ids;
    for(const auto&t:p.score.tracks)for(const auto&n:t.notes)ids.insert(sourceKey(t.id,n.id));
    return ids;
}
SubtitleInterval subtitleInterval(const Project &p,const SubtitleCue &c,const QSet<QString> *known) {
    if(c.anchor!="beats")return {c.start,c.end,false};
    QSet<QString> local;
    if(!known&&!c.sourceNotes.isEmpty()){local=subtitleSources(p);known=&local;}
    for(const auto&id:c.sourceNotes)if(!known->contains(sourceKey(c.sourceTrack,id)))return {c.start,c.end,true};
    return {p.score.time.seconds(c.startBlick)+p.output.syncOffset,p.score.time.seconds(c.endBlick)+p.output.syncOffset,false};
}
QVector<LyricSpan> lyricSpans(const Project &p,const QString &id) {
    QVector<LyricSpan> result;
    for(const auto&t:p.score.tracks)if(t.id==id&&!t.muted) {
        QVector<const Note*> notes;for(const auto&n:t.notes)notes.append(&n);
        std::stable_sort(notes.begin(),notes.end(),[](auto a,auto b){return a->onset==b->onset?a->id<b->id:a->onset<b->onset;});
        QMap<QString,int> previous;
        for(const auto*n:notes) {
            auto text=n->lyrics.normalized(QString::NormalizationForm_KC).trimmed();
            if(n->muted||text.isEmpty()||text=="cl"||text=="br"){previous.remove(n->groupInstance);continue;}
            if(text=="+"||text=="-"||text=="ー") {
                auto i=previous.value(n->groupInstance,-1);
                if(i>=0&&n->onset<=result[i].end){result[i].end=std::max(result[i].end,n->onset+n->duration);result[i].notes.append(n->id);}
                else previous.remove(n->groupInstance);
                continue;
            }
            previous[n->groupInstance]=int(result.size());
            result.append({n->lyrics,n->groupInstance,{n->id},n->onset,n->onset+n->duration});
        }
    }
    return result;
}
void setSubtitleTime(const Project &p,SubtitleCue &c,double a,double b,bool beats) {
    if(!std::isfinite(a)||!std::isfinite(b)||(!beats&&a<0)||std::abs(a)>21600||b<=a||std::abs(b)>21600)throw Failure("Subtitle interval must be within 0..21600 seconds (music anchors may start before video zero)");
    c.start=a;c.end=b;c.anchor=beats?"beats":"seconds";c.sourceNotes.clear();c.sourceTrack.clear();
    if(beats){c.startBlick=p.score.time.blicks(a-p.output.syncOffset);c.endBlick=p.score.time.blicks(b-p.output.syncOffset);}
}
void alignSubtitle(const Project &p,SubtitleCue &c,const QString &track,const LyricSpan &first,const LyricSpan &last) {
    if(last.end<=first.start||last.start<first.start)throw Failure("Choose an ordered lyric range");
    setSubtitleTime(p,c,p.score.time.seconds(first.start)+p.output.syncOffset,p.score.time.seconds(last.end)+p.output.syncOffset,true);
    c.startBlick=first.start;c.endBlick=last.end;c.sourceTrack=track;
    for(const auto&s:lyricSpans(p,track))if(s.start>=first.start&&s.start<=last.start)c.sourceNotes.append(s.notes);
}
void validateSubtitleStyle(const SubtitleStyle &s) {
    if(s.family.size()>256||!s.color.isValid()||!QStringList{"left","center","right"}.contains(s.alignment)
       ||!std::isfinite(s.x)||!std::isfinite(s.y)||s.x< -2||s.x>3||s.y< -2||s.y>3
       ||!std::isfinite(s.width)||s.width<.02||s.width>4||!std::isfinite(s.fontHeight)||s.fontHeight<.005||s.fontHeight>.5)
        throw Failure("Invalid subtitle style/layout");
}
void validateSubtitles(const Project &p,bool overlap) {
    if(p.subtitles.size()>64)throw Failure("Subtitle track limit is 64");
    if(p.subtitles.isEmpty())return;
    QSet<QString> ids;const auto sources=subtitleSources(p);int count=0;
    for(const auto&t:p.subtitles) {
        if(t.id.isEmpty()||t.id.size()>4096||ids.contains(t.id)||t.name.size()>128||t.sourceTrack.size()>4096)throw Failure("Invalid/duplicate subtitle track");
        ids.insert(t.id);validateSubtitleStyle(t.style);
        QVector<SubtitleInterval> intervals;
        for(const auto&c:t.cues) {
            if(++count>100000)throw Failure("Subtitle cue limit is 100000");
            if(c.id.isEmpty()||c.id.size()>4096||ids.contains(c.id)||c.text.size()>4096||c.sourceTrack.size()>4096||c.sourceNotes.size()>200000)throw Failure("Invalid/duplicate subtitle cue");
            ids.insert(c.id);validateSubtitleStyle(c.style);
            if(c.anchor!="seconds"&&c.anchor!="beats")throw Failure("Invalid subtitle anchor");
            for(const auto&id:c.sourceNotes)if(id.isEmpty()||id.size()>4096)throw Failure("Invalid subtitle source");
            if(c.anchor=="beats"&&(c.startBlick< -MusicLimit||c.startBlick>MusicLimit||c.endBlick< -MusicLimit||c.endBlick>MusicLimit||c.endBlick<=c.startBlick))throw Failure("Invalid subtitle music interval");
            auto v=subtitleInterval(p,c,&sources);
            if(!std::isfinite(c.start)||!std::isfinite(c.end)||std::abs(c.start)>21600||c.end<=c.start||c.end>21600||!std::isfinite(v.start)||!std::isfinite(v.end)||v.end<=v.start||std::abs(v.start)>21600||std::abs(v.end)>21600)throw Failure("Invalid subtitle interval");
            intervals.append(v);
        }
        if(overlap){std::sort(intervals.begin(),intervals.end(),[](auto a,auto b){return a.start<b.start;});for(int i=1;i<intervals.size();++i)if(intervals[i].start<intervals[i-1].end-1e-9)throw Failure("Subtitles overlap on the same track; move to another subtitle track");}
    }
}
static QJsonObject styleJson(const SubtitleStyle &s) {
    return {{"family",s.family},{"alignment",s.alignment},{"color",s.color.name(QColor::HexArgb)},{"x",s.x},{"y",s.y},{"width",s.width},{"fontHeight",s.fontHeight},{"bold",s.bold},{"italic",s.italic},{"outline",s.outline}};
}
QJsonObject subtitlesJson(const Project &p) {
    QJsonArray tracks;const auto sources=p.subtitles.isEmpty()?QSet<QString>{}:subtitleSources(p);
    for(const auto&t:p.subtitles){QJsonArray cues;for(const auto&c:t.cues){const auto v=subtitleInterval(p,c,&sources);cues.append(QJsonObject{{"id",c.id},{"text",c.text},{"anchor",c.anchor},{"start",v.start},{"end",v.end},{"startBlick",QString::number(c.startBlick)},{"endBlick",QString::number(c.endBlick)},{"sourceTrack",c.sourceTrack},{"sourceNotes",QJsonArray::fromStringList(c.sourceNotes)},{"ownStyle",c.ownStyle},{"style",styleJson(c.style)}});}tracks.append(QJsonObject{{"id",t.id},{"name",t.name},{"enabled",t.enabled},{"alignLyrics",t.alignLyrics},{"sourceTrack",t.sourceTrack},{"style",styleJson(t.style)},{"cues",cues}});}
    return {{"enabled",p.subtitlesEnabled},{"tracks",tracks}};
}
static double number(const QJsonObject &o,const char *key,double fallback) {
    auto v=o.value(QLatin1String(key));if(v.isUndefined())return fallback;if(!v.isDouble())throw Failure("Invalid subtitle numeric field");return v.toDouble();
}
static bool boolean(const QJsonObject &o,const char *key,bool fallback) {
    auto v=o.value(QLatin1String(key));if(v.isUndefined())return fallback;if(!v.isBool())throw Failure("Invalid subtitle boolean field");return v.toBool();
}
static QString string(const QJsonObject &o,const char *key,QString fallback={}) {
    auto v=o.value(QLatin1String(key));if(v.isUndefined())return fallback;if(!v.isString())throw Failure("Invalid subtitle string field");return v.toString();
}
static SubtitleStyle styleRead(const QJsonValue &v) {
    SubtitleStyle s;if(v.isUndefined())return s;if(!v.isObject())throw Failure("Invalid subtitle style");auto o=v.toObject();
    s.family=string(o,"family");s.alignment=string(o,"alignment","center");s.color=QColor(string(o,"color","#ffffffff"));s.x=number(o,"x",.5);s.y=number(o,"y",.86);s.width=number(o,"width",.85);s.fontHeight=number(o,"fontHeight",.055);s.bold=boolean(o,"bold",false);s.italic=boolean(o,"italic",false);s.outline=boolean(o,"outline",true);validateSubtitleStyle(s);return s;
}
static qint64 music(const QJsonObject &o,const char *key) {
    auto v=o.value(QLatin1String(key));if(v.isUndefined())return 0;bool ok=false;auto n=v.toString().toLongLong(&ok);if(!ok||n< -MusicLimit||n>MusicLimit)throw Failure("Invalid subtitle music position");return n;
}
void readSubtitles(Project &p,const QJsonValue &v) {
    if(v.isUndefined())return;
    if(!v.isObject())throw Failure("Invalid subtitles object");
    auto o=v.toObject();
    p.subtitlesEnabled=boolean(o,"enabled",false);
    if(!o.value("tracks").isArray())throw Failure("Invalid subtitle tracks");
    auto tracks=o.value("tracks").toArray();if(tracks.size()>64)throw Failure("Subtitle track limit is 64");
    int totalCues=0;
    for(const auto&tv:tracks){if(!tv.isObject())throw Failure("Invalid subtitle track");auto t=tv.toObject();SubtitleTrack track;
        track.id=string(t,"id");track.name=string(t,"name");track.sourceTrack=string(t,"sourceTrack");track.enabled=boolean(t,"enabled",true);track.alignLyrics=boolean(t,"alignLyrics",false);track.style=styleRead(t.value("style"));
        if(!t.value("cues").isArray())throw Failure("Invalid subtitle cues");
        auto cues=t.value("cues").toArray();if(cues.size()>100000)throw Failure("Subtitle cue limit exceeded");
        for(const auto&cv:cues){if(++totalCues>100000)throw Failure("Subtitle cue limit exceeded");if(!cv.isObject())throw Failure("Invalid subtitle cue");auto c=cv.toObject();SubtitleCue cue;
            cue.id=string(c,"id");cue.text=string(c,"text");cue.anchor=string(c,"anchor","seconds");cue.start=number(c,"start",0);cue.end=number(c,"end",2);cue.startBlick=music(c,"startBlick");cue.endBlick=music(c,"endBlick");cue.sourceTrack=string(c,"sourceTrack");cue.ownStyle=boolean(c,"ownStyle",false);cue.style=styleRead(c.value("style"));
            if(!c.value("sourceNotes").isArray())throw Failure("Invalid subtitle sources");
            for(const auto&n:c.value("sourceNotes").toArray()){if(!n.isString())throw Failure("Invalid subtitle source");cue.sourceNotes.append(n.toString());}track.cues.append(cue);
        }p.subtitles.append(track);
    }validateSubtitles(p);
}
}
