// SPDX-License-Identifier: GPL-3.0-or-later
#include "source.h"
#include <QStringDecoder>
#include <unicode/ucnv.h>
#include <algorithm>
namespace chosuta {
namespace {
QString ustText(const QByteArray &bytes) {
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text=utf8(bytes);
    if(!utf8.hasError())return text;
    UErrorCode error=U_ZERO_ERROR;
    int32_t size=ucnv_convert("UTF-8","windows-932",nullptr,0,bytes.constData(),bytes.size(),&error);
    if(error!=U_BUFFER_OVERFLOW_ERROR&&U_FAILURE(error))throw Failure("Cannot decode UST text");
    error=U_ZERO_ERROR;
    QByteArray converted(size,Qt::Uninitialized);
    ucnv_convert("UTF-8","windows-932",converted.data(),size,bytes.constData(),bytes.size(),&error);
    if(U_FAILURE(error))throw Failure("Cannot decode UST text");
    return QString::fromUtf8(converted);
}
QString aliasReading(QString alias) {
    // Recognize plain Japanese CV/VCV aliases, not arbitrary voicebank suffixes.
    alias=alias.normalized(QString::NormalizationForm_KC).trimmed();
    static const QRegularExpression vcv("^(?:-|[aiueoAIUEONn])\\s+([\\x{3041}-\\x{3096}\\x{30a1}-\\x{30fa}ー]+)$");
    const auto match=vcv.match(alias);
    return match.hasMatch()?match.captured(1):QString();
}
}
Score parseUst(const QByteArray &bytes,const QString &path,const std::atomic_bool *cancel,Progress progress) {
    Score score;
    score.sourceResolution=480;
    Track track;
    track.id="ust/track:0";
    track.name=QFileInfo(path).completeBaseName();
    if(track.name.isEmpty())track.name="UTAU";
    const auto lines=ustText(bytes).split('\n');
    QString section;
    QMap<QString,QString> fields;
    QMap<qint64,double> tempos{{0,120}};
    qint64 position=0;
    int ordinal=0,phrase=0;
    bool settings=false;
    auto flush=[&] {
        if(section=="VERSION") {
            score.sourceVersion=fields.value("version");
        } else if(section=="SETTING") {
            settings=true;
            if(fields.contains("ProjectName"))track.name=fields.value("ProjectName");
            if(fields.contains("Tempo"))tempos[0]=QString(fields.value("Tempo")).replace(',','.').toDouble();
        } else if(!section.isEmpty()&&section.front().isDigit()) {
            const qint64 duration=fields.value("Length").toLongLong();
            if(fields.contains("Tempo")&&!fields.value("Tempo").isEmpty())tempos[position]=QString(fields.value("Tempo")).replace(',','.').toDouble();
            const QString lyric=fields.value("Lyric");
            if(duration>0&&lyric.compare("R",Qt::CaseInsensitive)!=0) {
                Note note;
                note.groupInstance=track.id+QString("/phrase:%1").arg(phrase);
                note.id=track.id+QString("/note:%1").arg(ordinal);
                note.sourceOnset=position;note.sourceDuration=duration;
                note.onset=sourceBlick(position,480);note.duration=sourceBlick(duration,480);
                note.pitch=fields.value("NoteNum","60").toInt();
                note.lyrics=lyric;note.lyricReading=aliasReading(lyric);note.language="ja";
                for(auto it=fields.cbegin();it!=fields.cend();++it)note.original[it.key()]=it.value();
                track.notes.append(note);
            } else if(lyric.compare("R",Qt::CaseInsensitive)==0)++phrase;
            if(duration>0)position+=duration;
            ++ordinal;
        }
        fields.clear();
    };
    for(int i=0;i<lines.size();++i) {
        if(cancel&&cancel->load())throw Failure("Cancelled");
        QString line=lines[i].trimmed();
        if(line.startsWith(QChar(0xfeff)))line.remove(0,1);
        if(line.startsWith("[#")&&line.endsWith(']')) {flush();section=line.mid(2,line.size()-3);}
        else if(section=="VERSION"&&line.startsWith("UST Version"))fields["version"]=line.mid(11).trimmed();
        else {const auto equal=line.indexOf('=');if(equal>=0)fields[line.left(equal).trimmed()]=line.mid(equal+1);}
        if(progress&&i%128==0)progress(i*100/std::max(1,int(lines.size())));
    }
    flush();
    if(!settings)throw Failure("Not a UST project: SETTING required");
    score.time.tempos.clear();
    for(auto it=tempos.cbegin();it!=tempos.cend();++it)score.time.tempos.append({sourceBlick(it.key(),480),it.value()});
    score.endPosition=sourceBlick(position,480);
    score.tracks.append(track);
    score.diagnostics.append({"UST","score-timing","UST timing uses the score; voicebank aliases and rendered timing may differ"});
    return score;
}
}
