// SPDX-License-Identifier: GPL-3.0-or-later
#include "evaluation.h"
namespace chosuta {
Evaluation::Evaluation(const Project &p):project(p),events(p.effective()),resolved(resolveEvents(events)),performance(std::make_shared<PerformanceResolver>(project)),appearance(project,performance){
    const auto sources=subtitleSources(p);
    for(const auto &track:p.subtitles)for(const auto &cue:track.cues)subtitles[cue.id]=subtitleInterval(p,cue,&sources);
    if(p.output.duration>0){duration=p.output.duration;return;}
    for(const auto &track:p.score.tracks)for(const auto &note:track.notes)duration=std::max(duration,p.score.time.seconds(note.onset+note.duration));
    for(const auto &e:events)duration=std::max(duration,e.end);
    duration=std::max(.1,duration+std::max(0.,p.output.syncOffset));
    if(p.subtitlesEnabled)for(const auto &track:p.subtitles)if(track.enabled)for(const auto &cue:track.cues)duration=std::max(duration,subtitles.value(cue.id).end);
    if(p.appearance.enabled)for(const auto &segment:p.performance.manual)duration=std::max(duration,performance->interval(segment).end);
}
}
