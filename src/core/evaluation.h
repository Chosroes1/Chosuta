// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "appearance.h"
namespace chosuta {
// A view owns its immutable source and every index derived from it. Never stored in undo.
struct Evaluation {
    const Project project;
    const QVector<Event> events,resolved;
    const std::shared_ptr<const PerformanceResolver> performance;
    const AppearanceResolver appearance;
    QHash<QString,SubtitleInterval> subtitles;
    double duration=0;
    explicit Evaluation(const Project &);
};
}
