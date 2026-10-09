// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
static void initDictionaryResources() {Q_INIT_RESOURCE(data);}
namespace chosuta {
    QString dictionaryKey(const QString&word,const QString&language) {
        auto key=word.normalized(QString::NormalizationForm_KC).trimmed();
        if(language=="en")key=key.toLower().replace(u'’',u'\'');
        return key;
    }
    QJsonObject pronunciationOptionsJson(const PronunciationOptions&o) {
        QJsonArray entries;
        for(auto language=o.words.cbegin();language!=o.words.cend();++language)
            for(auto word=language->cbegin();word!=language->cend();++word)
                entries.append(QJsonObject{{"language",language.key()},{"word",word.key()},
                    {"reading",word->text},{"notation",word->phonemes?"phonemes":"reading"}});
        return {{"version",1},{"japaneseKanji",o.japaneseKanji},{"entries",entries}};
    }
    qint64 builtinDictionaryBytes() {
        static const qint64 bytes=[] {
            initDictionaryResources();qint64 total=0;
            for(const auto &name:QStringList{"english.tsv","japanese.tsv","chinese.tsv"}) {
                QFile file(":/chosuta/"+name);
                if(!file.open(QIODevice::ReadOnly))throw Failure("Missing pronunciation resource: "+name);
                total+=file.size();
            }
            return total;
        }();
        return bytes;
    }
    qint64 pronunciationDictionaryBytes(const PronunciationOptions&o) {
        return builtinDictionaryBytes()+QJsonDocument(pronunciationOptionsJson(o)).toJson(QJsonDocument::Compact).size();
    }
    void validatePronunciationOptions(const PronunciationOptions&o) {
        if(pronunciationDictionaryBytes(o)>DictionaryByteLimit)throw Failure("Pronunciation dictionaries exceed 3 MB");
        int count=0;
        for(auto language=o.words.cbegin();language!=o.words.cend();++language) {
            if(!QStringList{"en","zh","ja"}.contains(language.key()))throw Failure("Invalid dictionary language: "+language.key());
            for(auto word=language->cbegin();word!=language->cend();++word) {
                if(++count>20000)throw Failure("Dictionary exceeds 20000 entries");
                if(word.key().isEmpty()||word.key().size()>128||word.key()!=dictionaryKey(word.key(),language.key())||
                    QStringList{"+","-","ー","br","cl"}.contains(word.key())||word.key().contains(QRegularExpression("[\\p{Cc}]")))
                    throw Failure("Invalid dictionary word: "+word.key());
                if(word->text.trimmed().isEmpty()||word->text.size()>4096)throw Failure("Invalid dictionary reading: "+word.key());
                const auto parsed=pronounce(word->text,language.key(),word->phonemes);
                if(parsed.unknown)throw Failure("Unrecognized dictionary reading: "+language.key()+" / "+word.key());
            }
        }
    }
    PronunciationOptions pronunciationOptionsRead(const QJsonValue&value) {
        PronunciationOptions options;
        if(!value.isObject())throw Failure("Invalid pronunciation settings");
        const auto object=value.toObject();
        if(object["version"].toInt()!=1||!object["entries"].isArray()||!object["japaneseKanji"].isBool())
            throw Failure("Invalid pronunciation settings version/fields");
        const auto entries=object["entries"].toArray();
        if(entries.size()>20000)throw Failure("Dictionary exceeds 20000 entries");
        options.japaneseKanji=object["japaneseKanji"].toBool();
        for(const auto &value:entries) {
            if(!value.isObject())throw Failure("Invalid dictionary entry");
            const auto entry=value.toObject();
            const auto language=entry["language"].toString();
            const auto word=dictionaryKey(entry["word"].toString(),language);
            const auto notation=entry["notation"].toString();
            if(notation!="reading"&&notation!="phonemes")throw Failure("Invalid dictionary notation");
            if(options.words[language].contains(word))throw Failure("Duplicate dictionary word: "+language+" / "+word);
            options.words[language][word]={entry["reading"].toString().trimmed(),notation=="phonemes"};
        }
        validatePronunciationOptions(options);
        return options;
    }
}
