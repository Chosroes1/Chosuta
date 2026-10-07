// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore>
#include "core/model.h"
namespace chosuta {
struct Preferences {
    QString language="auto";
    bool returnOnPause=false;
    PronunciationOptions pronunciation;
    int mouthLaneHeight=138,subtitleLaneHeight=60,timelineHeight=250;
};
QString resolveUiLanguage(const QString &choice,const QStringList &systemLanguages);
Preferences loadPreferences();
void savePreferences(const Preferences &);
}
