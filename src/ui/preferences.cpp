// SPDX-License-Identifier: GPL-3.0-or-later
#include "preferences.h"
#include "core/performance.h"
#include <algorithm>
#include "core/model.h"
namespace chosuta {
    static void validatePerformanceDefaults(const PerformanceSettings &settings) {
        // Defaults cannot carry project-specific groups or tracks into another score.
        performanceSettingsJson(settings);
        for(const auto &rule:settings.rules)
            if(!QStringList{"relaxed","normal","emphasis"}.contains(rule.target)||!rule.track.isEmpty())
                throw Failure("New-project defaults require all-track role rules");
    }
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
        p.performanceLaneHeight=std::clamp(s.value("timeline/performanceHeight",56).toInt(),40,120);
        p.performanceLaneVisible=s.value("timeline/performanceVisible",true).toBool();
        auto optional=[&](const QString &key,auto read){
            const auto data=s.value(key).toByteArray();if(data.isEmpty())return;
            try {read(data);}catch(const std::exception &e){
                p.invalidFields.insert(key);p.diagnostics.append({key,exceptionMessage(e)});
            }
        };
        optional("performance/v2-defaults",[&](const QByteArray &data){auto value=readPerformanceSettings(checkedJson(data).object());validatePerformanceDefaults(value);p.performanceSettings=std::move(value);});
        p.cursorSnap=s.value("timeline/cursorSnap","off").toString();
        if(!QStringList{"off","beat","syllable"}.contains(p.cursorSnap))p.cursorSnap="off";
        p.returnOnStop=s.value("playback/returnOnStop",false).toBool();
        p.mouthLaneHeight=std::clamp(s.value("timeline/mouthHeight",p.mouthLaneHeight).toInt(),96,360);
        p.subtitleLaneHeight=std::clamp(s.value("timeline/subtitleHeight",60).toInt(),48,240);
        p.timelineHeight=std::clamp(s.value("timeline/viewportHeight",250).toInt(),180,900);
        p.waveformLaneHeight=std::clamp(s.value("timeline/waveformHeight",80).toInt(),40,200);
        optional("pronunciation/settings",[&](const QByteArray &data){
            if(data.size()>DictionaryByteLimit)throw Failure("Saved dictionary exceeds 3 MB");
            p.pronunciation=pronunciationOptionsRead(checkedJson(data).object());
        });
        return p;
    }
    void savePreferences(const Preferences&p) {
        validatePronunciationOptions(p.pronunciation);
        if(!p.invalidFields.contains("performance/v2-defaults"))validatePerformanceDefaults(p.performanceSettings);
        QSettings s(QSettings::IniFormat,QSettings::UserScope,"Chosuta","Chosuta");
        s.setValue("ui/language",p.language);
        s.setValue("playback/returnOnStop",p.returnOnStop);
        s.setValue("timeline/cursorSnap",p.cursorSnap);
        if(!p.invalidFields.contains("performance/v2-defaults"))s.setValue("performance/v2-defaults",QJsonDocument(performanceSettingsJson(p.performanceSettings)).toJson(QJsonDocument::Compact));
        s.setValue("timeline/performanceHeight",p.performanceLaneHeight);
        s.setValue("timeline/performanceVisible",p.performanceLaneVisible);
        if(!p.invalidFields.contains("pronunciation/settings"))s.setValue("pronunciation/settings",QJsonDocument(pronunciationOptionsJson(p.pronunciation)).toJson(QJsonDocument::Compact));
        s.setValue("timeline/mouthHeight",p.mouthLaneHeight);
        s.setValue("timeline/subtitleHeight",p.subtitleLaneHeight);
        s.setValue("timeline/viewportHeight",p.timelineHeight);
        s.setValue("timeline/waveformHeight",p.waveformLaneHeight);
        s.sync();
        if(s.status()!=QSettings::NoError)throw Failure(Message::format("Cannot save application preferences: %1",{s.fileName()}));
    }
}
