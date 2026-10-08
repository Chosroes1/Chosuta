// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore>
#include "core/model.h"
namespace chosuta {
struct Preferences {
    QString language="auto";
    bool returnOnPause=false;
    PronunciationOptions pronunciation;
    int mouthLaneHeight=96,subtitleLaneHeight=60,timelineHeight=250,waveformLaneHeight=80;
};
QString resolveUiLanguage(const QString &choice,const QStringList &systemLanguages);
Preferences loadPreferences();
void savePreferences(const Preferences &);
}
