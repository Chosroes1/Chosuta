// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences.h"
#include <algorithm>
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
        p.mouthLaneHeight=std::clamp(s.value("timeline/mouthHeight",p.mouthLaneHeight).toInt(),96,360);
        p.subtitleLaneHeight=std::clamp(s.value("timeline/subtitleHeight",60).toInt(),48,240);
        p.timelineHeight=std::clamp(s.value("timeline/viewportHeight",250).toInt(),180,900);
        p.waveformLaneHeight=std::clamp(s.value("timeline/waveformHeight",80).toInt(),40,200);
        const auto data=s.value("pronunciation/settings").toByteArray();
        if(!data.isEmpty()) {
            if(data.size()>DictionaryByteLimit)throw Failure("Saved dictionary exceeds 3 MB");
            p.pronunciation=pronunciationOptionsRead(checkedJson(data).object());
        }
        return p;
    }
    void savePreferences(const Preferences&p) {
        validatePronunciationOptions(p.pronunciation);
        QSettings s(QSettings::IniFormat,QSettings::UserScope,"Chosuta","Chosuta");
        s.setValue("ui/language",p.language);
        s.setValue("playback/returnOnPause",p.returnOnPause);
        s.setValue("pronunciation/settings",QJsonDocument(pronunciationOptionsJson(p.pronunciation)).toJson(QJsonDocument::Compact));
        s.setValue("timeline/mouthHeight",p.mouthLaneHeight);
        s.setValue("timeline/subtitleHeight",p.subtitleLaneHeight);
        s.setValue("timeline/viewportHeight",p.timelineHeight);
        s.setValue("timeline/waveformHeight",p.waveformLaneHeight);
        s.sync();
        if(s.status()!=QSettings::NoError)throw Failure("Cannot save application preferences: "+s.fileName());
    }
}
