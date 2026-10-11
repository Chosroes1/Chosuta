// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore>
#include "core/model.h"
namespace chosuta {
struct Preferences {
    QString language="auto";
    bool returnOnStop=false;
    QString cursorSnap="off";
    PronunciationOptions pronunciation;
    PerformanceSettings performanceSettings;
    int performanceLaneHeight=56;
    bool performanceLaneVisible=true;
    int mouthLaneHeight=96,subtitleLaneHeight=60,timelineHeight=250,waveformLaneHeight=80;
};
QString resolveUiLanguage(const QString &choice,const QStringList &systemLanguages);
Preferences loadPreferences();
void savePreferences(const Preferences &);
}
