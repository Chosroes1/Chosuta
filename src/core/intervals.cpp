// SPDX-License-Identifier: GPL-3.0-or-later
#include "intervals.h"
#include <array>
#include <cmath>
namespace chosuta {
void validateAnalysisSettings(const AnalysisSettings &a){
    if(a.intervalUnit!="semitones"&&a.intervalUnit!="diatonic")throw Failure("Invalid interval unit");
    validateDegreeSettings(a);
    if(a.strongBeats.size()>64)throw Failure("Too many accent templates");
    for(auto it=a.strongBeats.cbegin();it!=a.strongBeats.cend();++it){const auto f=it.key().split('/');bool no=false,de=false;int n=f.value(0).toInt(&no),d=f.value(1).toInt(&de);
        if(f.size()!=2||!no||!de||n<1||n>64||d<1||d>64||(d&(d-1))||it->isEmpty())throw Failure("Invalid accent template");
        QSet<int> seen;for(int b:*it){if(b<1||b>n||seen.contains(b))throw Failure("Invalid accent beat");seen.insert(b);}}
}
QJsonObject analysisSettingsJson(const AnalysisSettings &a){
    validateAnalysisSettings(a);QJsonObject spellings,accents;QJsonArray scale;
    for(auto it=a.noteSpellings.cbegin();it!=a.noteSpellings.cend();++it)spellings[it.key()]=it.value();
    for(int n:a.customDegreeScale)scale.append(n);
    for(auto it=a.strongBeats.cbegin();it!=a.strongBeats.cend();++it){QJsonArray v;for(int n:*it)v.append(n);accents[it.key()]=v;}
    return {{"tonalityEnabled",a.tonalityEnabled},{"intervalUnit",a.intervalUnit},{"degreeTonic",a.degreeTonic},{"degreeMode",a.degreeMode},{"customDegreeScale",scale},{"noteSpellings",spellings},{"strongBeats",accents}};
}
AnalysisSettings readAnalysisSettings(const QJsonValue &v){
    if(!v.isObject())throw Failure("Invalid performance analysis settings");
    auto o=v.toObject();AnalysisSettings a;
    for(const auto &k:{"tonalityEnabled"})if(!o[k].isBool())throw Failure("Invalid performance analysis switch");
    for(const auto &k:{"intervalUnit","degreeTonic","degreeMode"})if(!o[k].isString())throw Failure("Invalid performance analysis text");
    a.tonalityEnabled=o["tonalityEnabled"].toBool();a.intervalUnit=o["intervalUnit"].toString();a.degreeTonic=o["degreeTonic"].toString();a.degreeMode=o["degreeMode"].toString();
    if(!o["customDegreeScale"].isArray()||!o["noteSpellings"].isObject()||!o["strongBeats"].isObject())throw Failure("Invalid degree settings");
    a.customDegreeScale.clear();for(auto value:o["customDegreeScale"].toArray()){if(!value.isDouble()||value.toDouble()!=value.toInt())throw Failure("Invalid scale offset");a.customDegreeScale.append(value.toInt());}
    const auto names=o["noteSpellings"].toObject();for(auto it=names.cbegin();it!=names.cend();++it){if(!it->isString())throw Failure("Invalid written pitch");a.noteSpellings[it.key()]=it->toString();}
    const auto accents=o["strongBeats"].toObject();for(auto it=accents.cbegin();it!=accents.cend();++it){if(!it->isArray())throw Failure("Invalid accent list");auto &beats=a.strongBeats[it.key()];for(auto value:it->toArray()){if(!value.isDouble()||value.toDouble()!=value.toInt())throw Failure("Invalid accent beat");beats.append(value.toInt());}}
    validateAnalysisSettings(a);return a;
}

    static constexpr std::array<int,7> natural={0,2,4,5,7,9,11};
    static const QString letters="CDEFGAB";
    std::optional<WrittenPitch> parseWrittenPitch(QString name){
        name=name.trimmed().replace(QChar(0x266f),'#').replace(QChar(0x266d),'b');
        if(!name.isEmpty())name[0]=name[0].toUpper();
        static const QRegularExpression pattern("^([A-G])((?:#{1,9}|b{1,9})?)(-?[0-9]{1,2})$");
        const auto match=pattern.match(name);
        if(!match.hasMatch())return std::nullopt;
        const int letter=letters.indexOf(match.captured(1));
        const auto accidental=match.captured(2);
        const int alteration=accidental.startsWith('#')?accidental.size():-accidental.size();
        const int octave=match.captured(3).toInt(),midi=12*(octave+1)+natural[letter]+alteration;
        if(midi<0||midi>127)return std::nullopt;
        return WrittenPitch{midi,octave*7+letter,match.captured(1)+accidental+QString::number(octave)};
    }
    const QMap<QString,QVector<int>> &degreeModes(){
        static const QMap<QString,QVector<int>> modes={
            {"major",{0,2,4,5,7,9,11}}, {"dorian",{0,2,3,5,7,9,10}},
            {"phrygian",{0,1,3,5,7,8,10}}, {"lydian",{0,2,4,6,7,9,11}},
            {"mixolydian",{0,2,4,5,7,9,10}}, {"minor",{0,2,3,5,7,8,10}},
            {"locrian",{0,1,3,5,6,8,10}}, {"harmonic-minor",{0,2,3,5,7,8,11}},
            {"melodic-minor",{0,2,3,5,7,9,11}}, {"harmonic-major",{0,2,4,5,7,8,11}}
        };
        return modes;
    }
    void validateDegreeSettings(const AnalysisSettings &a){
        static const QRegularExpression tonicPattern("^[A-G](?:#{1,3}|b{1,3})?$");
        if(!tonicPattern.match(a.degreeTonic).hasMatch()||!parseWrittenPitch(a.degreeTonic+"4"))throw Failure("Invalid tonic spelling");
        if(a.degreeMode!="custom"&&!degreeModes().contains(a.degreeMode))throw Failure("Invalid diatonic mode");
        if(a.customDegreeScale.size()!=7||a.customDegreeScale.front()!=0)throw Failure("Custom scale requires seven ascending offsets starting at zero");
        for(int i=0;i<7;++i)if(a.customDegreeScale[i]<0||a.customDegreeScale[i]>11||(i&&a.customDegreeScale[i]<=a.customDegreeScale[i-1]))throw Failure("Invalid custom scale offsets");
        for(auto it=a.noteSpellings.cbegin();it!=a.noteSpellings.cend();++it)if(it.key().isEmpty()||!parseWrittenPitch(it.value()))throw Failure("Invalid written note spelling");
    }
    std::optional<WrittenPitch> writtenPitch(const AnalysisSettings &a,const Note &n){
        const auto manual=a.noteSpellings.constFind(n.id);
        if(manual!=a.noteSpellings.cend()){
            auto result=parseWrittenPitch(*manual);
            return result&&result->midi==n.pitch?result:std::nullopt;
        }
        const auto tonic=parseWrittenPitch(a.degreeTonic+"4");
        if(!tonic)return std::nullopt;
        const auto scale=a.degreeMode=="custom"?a.customDegreeScale:degreeModes().value(a.degreeMode);
        const int firstLetter=letters.indexOf(tonic->name.front());
        for(int i=0;i<scale.size();++i){
            const int target=tonic->midi+scale[i];
            if((target%12+12)%12!=n.pitch%12)continue;
            const int letter=(firstLetter+i)%7;
            const int writtenNatural=60+natural[letter]+(firstLetter+i>=7?12:0);
            const int alteration=target-writtenNatural;
            const int octave=(n.pitch-natural[letter]-alteration)/12-1;
            const auto accidental=QString(std::abs(alteration),alteration>=0?'#':'b');
            return WrittenPitch{n.pitch,octave*7+letter,QString(letters[letter])+accidental+QString::number(octave)};
        }
        return std::nullopt;
    }
}
