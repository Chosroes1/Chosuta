// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences.h"
#include "core/model.h"
namespace chosuta {
    QString resolveUiLanguage(const QString&choice,const QStringList&languages) {
        if(QStringList {
            "zh","ja","en"
        }.contains(choice))return choice;
        for(auto language:languages) {
            language=language.toLower().section('-',0,0).section('_',0,0);
            if(QStringList {
                "zh","ja","en"
            }.contains(language))return language;
        }
        return "en";
    }
    Preferences loadPreferences() {
        QSettings s(QSettings::IniFormat,QSettings::UserScope,"Chosuta","Chosuta");
        Preferences p;
        p.language=s.value("ui/language","auto").toString();
        if(!QStringList {
            "auto","zh","en","ja"
        }.contains(p.language))p.language="auto";
        p.returnOnPause=s.value("playback/returnOnPause",false).toBool();
        return p;
    }
    void savePreferences(const Preferences&p) {
        QSettings s(QSettings::IniFormat,QSettings::UserScope,"Chosuta","Chosuta");
        s.setValue("ui/language",p.language);
        s.setValue("playback/returnOnPause",p.returnOnPause);
        s.sync();
        if(s.status()!=QSettings::NoError)throw Failure("Cannot save application preferences: "+s.fileName());
    }
}
