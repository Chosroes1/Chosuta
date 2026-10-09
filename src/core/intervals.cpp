// SPDX-License-Identifier: GPL-3.0-or-later
#include "intervals.h"
#include <array>
#include <cmath>
namespace chosuta {
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
    void validateDegreeSettings(const Project::Appearance &a){
        if(a.stepDegreeMax<2||a.stepDegreeMax>15||a.smallDegreeMax<=a.stepDegreeMax||a.smallDegreeMax>29)throw Failure("Invalid diatonic degree thresholds");
        static const QRegularExpression tonicPattern("^[A-G](?:#{1,3}|b{1,3})?$");
        if(!tonicPattern.match(a.degreeTonic).hasMatch()||!parseWrittenPitch(a.degreeTonic+"4"))throw Failure("Invalid tonic spelling");
        if(a.degreeMode!="custom"&&!degreeModes().contains(a.degreeMode))throw Failure("Invalid diatonic mode");
        if(a.customDegreeScale.size()!=7||a.customDegreeScale.front()!=0)throw Failure("Custom scale requires seven ascending offsets starting at zero");
        for(int i=0;i<7;++i)if(a.customDegreeScale[i]<0||a.customDegreeScale[i]>11||(i&&a.customDegreeScale[i]<=a.customDegreeScale[i-1]))throw Failure("Invalid custom scale offsets");
        for(auto it=a.noteSpellings.cbegin();it!=a.noteSpellings.cend();++it)if(it.key().isEmpty()||!parseWrittenPitch(it.value()))throw Failure("Invalid written note spelling");
    }
    std::optional<WrittenPitch> writtenPitch(const Project::Appearance &a,const Note &n){
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
    std::optional<int> intervalDegree(const Project::Appearance &a,const Note &from,const Note &to){
        const auto first=writtenPitch(a,from),second=writtenPitch(a,to);
        if(!first||!second)return std::nullopt;
        return std::abs(second->staffPosition-first->staffPosition)+1;
    }
}
