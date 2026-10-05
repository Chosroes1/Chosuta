// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore>
namespace chosuta {
struct Preferences {
    QString language="auto";
    bool returnOnPause=false;
};
QString resolveUiLanguage(const QString &choice,const QStringList &systemLanguages);
Preferences loadPreferences();
void savePreferences(const Preferences &);
}
