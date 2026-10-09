// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
namespace chosuta {
    QStringList consonantKeys(const QString &language) {
        static const auto all=[] {
            QMap<QString,QStringList> result{
                {
                    "en",{
                        "B","CH","D","DH","DX","F","G","HH","JH","K","L","LL","M","N","NG","P","R","S","SH","T","TH","V","W","Y","Z","ZH"
                    }
                }, {
                    "ja",{
                        "b","by","ch","d","dy","f","g","gy","h","hy","j","k","ky","m","my","n","ny","p","py","r","ry","s","sh","sy","t","ts","ty","v","w","y","z","zy"
                    }
                }, {
                    "zh",{
                        "b","p","m","f","d","t","n","l","g","k","h","j","q","x","zh","ch","sh","r","z","c","s","y","w"
                    }
                }
            };
            for(auto it=result.begin();it!=result.end();++it){
                const QString set=it.key()=="en"?"arpabet":it.key()=="ja"?"romaji":"xsampa";
                for(auto &phone:it.value())phone=it.key()+":"+set+":"+phone;
            }
            return result;
        }
        ();
        return all.value(language);
    }
    QString consonantMode(const QString &key) {
        const auto fields=key.split(':');
        if(fields.size()!=3||!consonantKeys(fields[0]).contains(key))throw Failure("Unknown consonant key: "+key);
        const auto phone=fields[2].toLower();
        return QStringList{
            "b","p","m","by","py","my"
        }
        .contains(phone)?"closed":"open";

    }
    QString consonantMode(const QString &key,const Rules::Consonants &rules) {
        return rules.overrides.value(key,consonantMode(key));
    }
    void validateConsonants(const Rules::Consonants &rules) {
        if(rules.overrides.size()>100)throw Failure("Too many consonant overrides");
        for(auto it=rules.overrides.cbegin();it!=rules.overrides.cend();++it){
            consonantMode(it.key());
            if(it.value()!="open"&&it.value()!="closed")throw Failure("Invalid consonant mode");
        }
    }
    QJsonObject consonantsJson(const Rules::Consonants &rules) {
        validateConsonants(rules);
        QJsonObject overrides;
        for(auto it=rules.overrides.cbegin();it!=rules.overrides.cend();++it)overrides[it.key()]=it.value();
        return {{"overrides",overrides}};
    }
    Rules::Consonants readConsonants(const QJsonValue &value) {
        Rules::Consonants result;
        if(!value.isObject())throw Failure("Invalid consonant settings");
        const auto o=value.toObject();
        if(o.size()!=1)throw Failure("Invalid consonant settings");
        if(!o["overrides"].isObject())throw Failure("Invalid consonant overrides");
        const auto overrides=o["overrides"].toObject();
        for(auto it=overrides.begin();it!=overrides.end();++it){
            if(!it.value().isString())throw Failure("Invalid consonant override");
            result.overrides[it.key()]=it.value().toString();
        }
        validateConsonants(result);
        return result;
    }
}
