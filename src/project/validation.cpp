// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
#include "core/appearance.h"
#include "core/performance.h"
#include <cmath>
namespace chosuta {
void validateOutputSettings(const ExportSettings &s) {
    if(s.fpsNum<1||s.fpsNum>240000||s.fpsDen<1||s.fpsDen>10000||double(s.fpsNum)/s.fpsDen<1||double(s.fpsNum)/s.fpsDen>240)throw Failure("Frame rate must be 1..240 fps");
    if(!QStringList{"mp4","webm","mov"}.contains(s.format))throw Failure("Unsupported output format");
    if(s.crf<0||s.crf>51||s.bitrateKbps<0||s.bitrateKbps>500000)throw Failure("Invalid output quality");
    if(!std::isfinite(s.duration)||s.duration<0||s.duration>21600||!std::isfinite(s.syncOffset)||std::abs(s.syncOffset)>21600||!std::isfinite(s.audioOffset)||std::abs(s.audioOffset)>21600)throw Failure("Invalid output time settings");
    if(s.ffmpeg.trimmed().isEmpty())throw Failure("Missing media tool selection");
}
void validateSavedEvent(const Event &e) {
    if(e.id.isEmpty()||e.shape.isEmpty()||!std::isfinite(e.start)||!std::isfinite(e.end)||e.end<=e.start||std::abs(e.start)>864000||std::abs(e.end)>864000)throw Failure(Message::format("Invalid saved event: %1",{e.id}));
    if(e.phone.size()>128||!QStringList{"","consonant","vowel","cl","br","rest","nasal","extend","empty","unknown"}.contains(e.articulation))throw Failure(Message::format("Invalid event articulation: %1",{e.id}));
}
void validateProject(const Project &p) {
    if(p.score.raw.isEmpty()||p.score.hash.isEmpty())throw Failure("Import a score before saving a project");
    validateOutputSettings(p.output);validateCanvas(p.canvas);validateSubtitles(p);
    validatePronunciationOptions(p.rules.pronunciation);validateConsonants(p.rules.consonants);
    validateAnalysisSettings(p.performance.analysis);validatePerformance(p);validateTimingCorrection(p);
    if(!std::isfinite(p.audioDuration)||p.audioDuration<0||p.audioDuration>864000)throw Failure("Invalid audio duration");
    if(!QStringList{"auto","ja","zh","en"}.contains(p.rules.language))throw Failure("Invalid language");
    if(!std::isfinite(p.rules.consonantRatio)||p.rules.consonantRatio<0||p.rules.consonantRatio>.8||!std::isfinite(p.rules.consonantMaxSeconds)||p.rules.consonantMaxSeconds<0||p.rules.consonantMaxSeconds>1)throw Failure("Invalid consonant timing settings");
    const Rules defaults;
    if(p.rules.special.keys()!=defaults.special.keys())throw Failure("Invalid special policy keys");
    for(auto it=p.rules.special.cbegin();it!=p.rules.special.cend();++it){const auto &v=it.value();
        if(!QStringList{"shape","hold","timed"}.contains(v.mode)||v.shape.isEmpty()||!std::isfinite(v.holdSeconds)||v.holdSeconds<0||v.holdSeconds>60)throw Failure(Message::format("Invalid special policy: %1",{it.key()}));}
    QSet<QString> tracks,selected,ids;
    for(const auto &t:p.score.tracks)tracks.insert(t.id);
    for(const auto &id:p.selected){if(!tracks.contains(id)||selected.contains(id))throw Failure(Message::format("Unknown or duplicate selected track: %1",{id}));selected.insert(id);}
    for(auto it=p.priorities.cbegin();it!=p.priorities.cend();++it)if(!tracks.contains(it.key()))throw Failure(Message::format("Unknown priority track: %1",{it.key()}));
    if(p.generated.size()+p.overrides.size()>500000)throw Failure("Too many saved events");
    for(const auto &e:p.generated){validateSavedEvent(e);if(ids.contains(e.id))throw Failure(Message::format("Duplicate event ID: %1",{e.id}));ids.insert(e.id);}
    qsizetype fragmentCount=0;
    for(auto it=p.overrides.cbegin();it!=p.overrides.cend();++it){const auto &o=it.value();validateSavedEvent(o.event);
        if(it.key()!=o.event.id)throw Failure("Mismatched override ID");
        if(o.anchor!="seconds"&&o.anchor!="beats")throw Failure("Invalid edit anchor");
        if(o.startBlick < -MusicLimit||o.startBlick>MusicLimit||o.endBlick < -MusicLimit||o.endBlick>MusicLimit||(o.anchor=="beats"&&o.endBlick<=o.startBlick))throw Failure("Invalid edit music interval");
        fragmentCount+=o.sourceFragments.size();if(fragmentCount>500000)throw Failure("Too many source fragments");
        double end=0;
        for(const auto &f:o.sourceFragments){
            if(f.parentId.isEmpty()||(o.standalone?!o.parentIds.contains(f.parentId):f.parentId!=it.key()))throw Failure("Invalid source fragment parent");
            for(double v:{f.sourceStart,f.sourceEnd,f.start,f.end})if(!std::isfinite(v)||v<0||v>1)throw Failure("Invalid source fragment fraction");
            if(f.sourceEnd<=f.sourceStart||f.end<=f.start||f.start<end)throw Failure("Invalid or overlapping source fragments");
            end=f.end;
        }
    }
    if(p.fallback.isEmpty())throw Failure("Missing fallback ID");
}
}
