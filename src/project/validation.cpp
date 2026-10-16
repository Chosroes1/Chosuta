// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
#include <cmath>
namespace chosuta {
void validateOutputSettings(const ExportSettings &s) {
    if(s.fpsNum<1||s.fpsNum>240000||s.fpsDen<1||s.fpsDen>10000||double(s.fpsNum)/s.fpsDen<1||double(s.fpsNum)/s.fpsDen>240)throw Failure("Frame rate must be 1..240 fps");
    if(!QStringList{"mp4","webm","mov"}.contains(s.format))throw Failure("Unsupported output format");
    if(s.crf<0||s.crf>51||s.bitrateKbps<0||s.bitrateKbps>500000)throw Failure("Invalid output quality");
    if(!std::isfinite(s.duration)||s.duration<0||s.duration>21600||!std::isfinite(s.syncOffset)||!std::isfinite(s.audioOffset))throw Failure("Invalid output time settings");
    if(s.ffmpeg.trimmed().isEmpty())throw Failure("Missing media tool selection");
}
void validateSavedEvent(const Event &e) {
    if(e.id.isEmpty()||e.shape.isEmpty()||!std::isfinite(e.start)||!std::isfinite(e.end)||e.end<=e.start||std::abs(e.start)>864000||std::abs(e.end)>864000)throw Failure(Message::format("Invalid saved event: %1",{e.id}));
    if(e.phone.size()>128||!QStringList{"","consonant","vowel","devoiced-vowel","cl","br","rest","nasal","extend","empty","unknown"}.contains(e.articulation))throw Failure(Message::format("Invalid event articulation: %1",{e.id}));
}
}
