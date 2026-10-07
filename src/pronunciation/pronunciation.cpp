// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
#include <unicode/translit.h>
#include <unicode/unistr.h>
#include <memory>
#include <map>
static void initChosutaData() {
    Q_INIT_RESOURCE(data);
}
namespace chosuta {
    static QString transliterate(QString text,const char*id) {
        UErrorCode error=U_ZERO_ERROR;
        // Transliterator is mutable: cache one instance per worker thread.
        static thread_local std::map<std::string,std::unique_ptr<icu::Transliterator>> transforms;
        auto &t=transforms[id];
        if(!t)t.reset(icu::Transliterator::createInstance(id,UTRANS_FORWARD,error));
        if(U_FAILURE(error)||!t)return text;
        auto bytes=text.toUtf8();
        icu::UnicodeString s=icu::UnicodeString::fromUTF8(icu::StringPiece(bytes.constData(),bytes.size()));
        t->transliterate(s);
        std::string result;
        s.toUTF8String(result);
        return QString::fromUtf8(result);
    }
    static QMap<QString,QStringList>englishWords(const QString&path) {
        static const auto builtin=[]() {
            initChosutaData();
            QMap<QString,QStringList>m;
            QFile f(":/chosuta/english.tsv");
            if(!f.open(QIODevice::ReadOnly))throw Failure("Missing built-in word table");
            while(!f.atEnd()) {
                auto l=QString::fromUtf8(f.readLine()).trimmed();
                if(l.startsWith('#')||l.isEmpty())continue;
                auto a=l.split('\t');
                if(a.size()==2)m[a[0]]=a[1].split(' ',Qt::SkipEmptyParts);
            }
            return m;
        }
        ();
        if(path.isEmpty())return builtin;
        // Thread-local cache: external dictionary is never loaded on the GUI's paint path.
        static thread_local QString last;
        static thread_local qint64 modified=0;
        static thread_local QMap<QString,QStringList>cache;
        QFileInfo info(path);
        if(last==path&&modified==info.lastModified().toMSecsSinceEpoch())return cache;
        QFile f(path);
        if(!f.open(QIODevice::ReadOnly)||f.size()>DictionaryByteLimit-builtinDictionaryBytes())throw Failure("Cannot read English dictionary (limit 3 MB including built-in tables): "+path);
        cache=builtin;
        while(!f.atEnd()) {
            QString l=QString::fromUtf8(f.readLine()).section('#',0,0).trimmed();
            if(l.startsWith(";;;"))continue;
            auto a=l.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
            if(a.size()<2)continue;
            auto word=a.takeFirst().toLower();
            if(word.contains('('))continue;
            cache[word]=a;
        }
        last=path;
        modified=info.lastModified().toMSecsSinceEpoch();
        return cache;
    }
    static QMap<QString,QStringList>englishPhones() {
        return {
            {
                "AX", {"E"}
            }, {
                "AA", {
                    "A"
                }
            }, {
                "AE", {
                    "A"
                }
            }, {
                "AH", {
                    "A"
                }
            }, {
                "AO", {
                    "O"
                }
            }, {
                "AW", {
                    "A","U"
                }
            }, {
                "AY", {
                    "A","I"
                }
            }, {
                "EH", {
                    "E"
                }
            }, {
                "ER", {
                    "E"
                }
            }, {
                "EY", {
                    "E","I"
                }
            }, {
                "IH", {
                    "I"
                }
            }, {
                "IY", {
                    "I"
                }
            }, {
                "OW", {
                    "O","U"
                }
            }, {
                "OY", {
                    "O","I"
                }
            }, {
                "UH", {
                    "U"
                }
            }, {
                "UW", {
                    "U"
                }
            }
        };
    }
    // Binary animation defaults, not a phonetic timing model. Open consonants
    // anticipate the nearby vowel; half-closed/articulator detail is deferred.
    static QString consonantShape(QString phone,const QString&language){
        phone=phone.toLower();
        const QStringList closed=language=="zh"?QStringList{"b","p","m","d","t","g","k","j","q","zh","ch","z","c"}:
            QStringList{"b","p","m","d","t","g","k","ch","jh","j","ts","by","py","my","dy","ty","gy","ky"};
        return closed.contains(phone)?"closed":"open";
    }
    static Pronunciation phones(const QStringList&tokens,QString language) {
        Pronunciation r;
        r.provenance="explicit/estimated";
        QStringList pending;
        auto vowels=englishPhones();
        const QStringList engConsonants= {
            "B","CH","D","DH","DX","F","G","HH","JH","K","L","LL","M","N","NG","P","R","S","SH","T","TH","V","W","Y","Z","ZH"
        };
        const QStringList jpConsonants= {
            "b","by","ch","d","dy","f","g","gy","h","hy","j","k","ky","m","my","n","ny","p","py","r","ry","s","sh","t","ts","ty","v","w","y","z"
        };
        for(QString token:tokens) {
            if(token=="|") {
                if(r.syllables.isEmpty()) {r.unknown=true;return r;}
                r.syllables.last().append(pending);r.roles.last()+=QVector<SegmentRole>(pending.size(),SegmentRole::Consonant);pending.clear();continue;
            }
            if(language=="en")token.remove(QRegularExpression("[012]$"));
            QStringList shape;
            if(token=="br"||token=="breath")shape= {
                "breath"
            };
            else if(token=="cl")shape= {
                "closed"
            };
            else if(token=="sil"||token=="pau")shape= {
                "rest"
            };
            else if(language=="en"&&vowels.contains(token.toUpper()))shape=vowels[token.toUpper()];
            else if(language!="en"&&QStringList {
                "a","i","u","e","o"
            }
            .contains(token.toLower()))shape= {
                token.toUpper()
            };
            else if((language!="en"&&(token=="N"||token=="ng"))||token=="nasal")shape= {
                "nasal"
            };
            else if((language=="en"&&engConsonants.contains(token.toUpper()))||(language=="ja"&&jpConsonants.contains(token))||(language=="zh"&&QStringList{"b","p","m","f","d","t","n","l","g","k","h","j","q","x","zh","ch","sh","r","z","c","s","y","w"}.contains(token))) {
                pending<<consonantShape(token,language);
                continue;
            }
            else {
                r.unknown=true;
                return r;
            }
            r.syllables.append(pending+shape);
            QVector<SegmentRole> roles(pending.size(),SegmentRole::Consonant);
            const bool vowel=language=="en"?vowels.contains(token.toUpper()):QStringList{"a","i","u","e","o"}.contains(token.toLower());
            roles+=QVector<SegmentRole>(shape.size(),vowel?SegmentRole::Vowel:SegmentRole::Special);
            r.roles.append(roles);
            pending.clear();
        }
        if(!pending.isEmpty()) {
            if(r.syllables.isEmpty()){r.syllables.append(pending);r.roles.append(QVector<SegmentRole>(pending.size(),SegmentRole::Consonant));}
            else {r.syllables.last().append(pending);r.roles.last()+=QVector<SegmentRole>(pending.size(),SegmentRole::Consonant);}
        }
        if(r.syllables.isEmpty())r.unknown=true;
        return r;
    }
    static Pronunciation japanese(QString text) {
        Pronunciation r;
        r.provenance="japanese-rule/estimated";
        text=text.normalized(QString::NormalizationForm_KC);
        for(auto&c:text)if(c.unicode()>=0x30a1&&c.unicode()<=0x30f6)c=QChar(c.unicode()-0x60);
        // Kana rows and combinations are algorithmic, written for Chosuta; no external dictionary.
        QMap<QChar,QChar>map;
        QStringList rows= {
            "あいうえお","かきくけこ","がぎぐげご","さしすせそ","ざじずぜぞ","たちつてと","だぢづでど","なにぬねの","はひふへほ","ばびぶべぼ","ぱぴぷぺぽ","まみむめも","らりるれろ"
        };
        for(const auto&row:rows)for(int i=0;i<5;++i)map[row[i]]=QString("AIUEO")[i];
        for(auto pair:QList<QPair<QChar,QChar>> {
            {
                u'や',u'A'
            }, {
                u'ゆ',u'U'
            }, {
                u'よ',u'O'
            }, {
                u'わ',u'A'
            }, {
                u'ゐ',u'I'
            }, {
                u'ゑ',u'E'
            }, {
                u'を',u'O'
            }, {
                u'ゔ',u'U'
            }
        })map[pair.first]=pair.second;
        QString small="ぁぃぅぇぉゃゅょゎ";
        QString corresponding="AIUEOAUOA";
        bool kana=false;
        for(QChar c:text)if(c.unicode()>=0x3041&&c.unicode()<=0x3096)kana=true;
        if(kana) {
            for(QChar c:text) {
                if(c.isSpace()||(c.isPunct()&&c!=u'ー'))continue;
                if(c==u'ん') {
                    r.syllables.append( {
                        "nasal"
                    });
                    continue;
                }
                if(c==u'っ') {
                    r.syllables.append( {
                        "closed"
                    });
                    continue;
                }
                if(c==u'ー') {
                    if(r.syllables.isEmpty()) {
                        r.unknown=true;
                        return r;
                    }
                    r.syllables.append( {
                        r.syllables.last().last()
                    });
                    continue;
                }
                if(small.contains(c)) {
                    QString v(corresponding[small.indexOf(c)]);
                    if(r.syllables.isEmpty()||QStringList{"nasal","closed"}.contains(r.syllables.last().last())) {
                        if(QString("ぁぃぅぇぉ").contains(c))r.syllables.append({v});
                        else {r.unknown=true;return r;}
                    }
                    else r.syllables.last().last()=v;
                    continue;
                }
                if(!map.contains(c)) {
                    r.unknown=true;
                    return r;
                }
                QString v(map[c]);
                if(QString("あいうえお").contains(c))r.syllables.append( {
                    v
                });
                else r.syllables.append( {
                    QString::fromUtf8("かきくけこがぎぐげごたちつてとだぢづでどばびぶべぼぱぴぷぺぽまみむめもじ").contains(c)?QString("closed"):QString("open"),v
                });
            }
            return r;
        }
        // Romaji mora grammar; reject unrecognized consonant sequences instead of guessing vowels.
        QString s=text.toLower();
        s.remove(QRegularExpression("[\\s]"));
        QStringList heads= {
            "ky","gy","sh","ch","ny","hy","by","py","my","ry","ts","sy","ty","dy","zy","b","d","f","g","h","j","k","m","n","p","r","s","t","v","w","y","z",""
        };
        while(!s.isEmpty()) {
            if(s.startsWith('n')&&(s.size()==1||s[1]==u'\''||!QString("aiueoy").contains(s[1]))) {
                r.syllables.append( {
                    "nasal"
                });
                s.remove(0,s.size()>1&&s[1]==u'\''?2:1);
                continue;
            }
            if(s.size()>1&&s[0]==s[1]&&!QString("aiueo").contains(s[0])) {
                r.syllables.append( {
                    "closed"
                });
                s.remove(0,1);
                continue;
            }
            bool matched=false;
            for(const auto&head:heads)if(s.startsWith(head)&&s.size()>head.size()&&QString("aiueo").contains(s[head.size()])) {
                QString vowel=s.mid(head.size(),1).toUpper();
                r.syllables.append(head.isEmpty()?QStringList {
                    vowel
                }
                :QStringList {
                    consonantShape(head,"ja"),vowel
                });
                s.remove(0,head.size()+1);
                matched=true;
                break;
            }
            if(!matched) {
                r.unknown=true;
                return r;
            }
        }
        if(r.syllables.isEmpty())r.unknown=true;
        return r;
    }
    static Pronunciation chinese(QString text) {
        Pronunciation r;
        r.provenance="icu-han-pinyin/estimated";
        QString s=transliterate(text.normalized(QString::NormalizationForm_KC),"Han-Latin; Lower");
        // Preserve the umlaut before removing tonal combining marks.
        s=s.normalized(QString::NormalizationForm_D);
        s.replace(QStringLiteral("u\u0308"),QStringLiteral("v"));
        s.remove(QRegularExpression("[\\p{M}]"));
        s.replace("u:","v");
        s.replace(QRegularExpression("[0-5]")," ");
        auto words=s.split(QRegularExpression("[\\s\\p{P}]+"),Qt::SkipEmptyParts);
        QMap<QString,QStringList>finals= {
            {
                "a", {
                    "A"
                }
            }, {
                "o", {
                    "O"
                }
            }, {
                "e", {
                    "E"
                }
            }, {
                "i", {
                    "I"
                }
            }, {
                "u", {
                    "U"
                }
            }, {
                "v", {
                    "U"
                }
            }, {
                "ai", {
                    "A","I"
                }
            }, {
                "ei", {
                    "E","I"
                }
            }, {
                "ao", {
                    "A","O"
                }
            }, {
                "ou", {
                    "O","U"
                }
            }, {
                "an", {
                    "A","nasal"
                }
            }, {
                "en", {
                    "E","nasal"
                }
            }, {
                "ang", {
                    "A","nasal"
                }
            }, {
                "eng", {
                    "E","nasal"
                }
            }, {
                "ong", {
                    "O","nasal"
                }
            }, {
                "ia", {
                    "I","A"
                }
            }, {
                "ie", {
                    "I","E"
                }
            }, {
                "iao", {
                    "I","A","O"
                }
            }, {
                "iu", {
                    "I","U"
                }
            }, {
                "ian", {
                    "I","A","nasal"
                }
            }, {
                "in", {
                    "I","nasal"
                }
            }, {
                "iang", {
                    "I","A","nasal"
                }
            }, {
                "ing", {
                    "I","nasal"
                }
            }, {
                "iong", {
                    "I","O","nasal"
                }
            }, {
                "ua", {
                    "U","A"
                }
            }, {
                "uo", {
                    "U","O"
                }
            }, {
                "uai", {
                    "U","A","I"
                }
            }, {
                "ui", {
                    "U","I"
                }
            }, {
                "uan", {
                    "U","A","nasal"
                }
            }, {
                "un", {
                    "U","nasal"
                }
            }, {
                "uang", {
                    "U","A","nasal"
                }
            }, {
                "ueng", {
                    "U","E","nasal"
                }
            }, {
                "ve", {
                    "U","E"
                }
            }, {
                "van", {
                    "U","A","nasal"
                }
            }, {
                "vn", {
                    "U","nasal"
                }
            }, {
                "er", {
                    "E"
                }
            }
        };
        finals["ue"]={"U","E"};
        finals["iou"]={"I","O","U"};
        finals["uei"]={"U","E","I"};
        finals["uen"]={"U","E","nasal"};
        finals["m"]={"closed"};
        finals["n"]={"nasal"};
        finals["ng"]={"nasal"};
        QStringList initials= {
            "zh","ch","sh","b","p","m","f","d","t","n","l","g","k","h","j","q","x","r","z","c","s","y","w"
        };
        for(auto w:words) {
            if(QStringList{"m","n","ng"}.contains(w)) {r.syllables.append(finals[w]);continue;}
            QString initial;
            for(const auto&i:initials)if(w.startsWith(i)) {
                initial=i;
                w.remove(0,i.size());
                break;
            }
            if((initial=="j"||initial=="q"||initial=="x"||initial=="y")&&w.startsWith('u'))w[0]=u'v';
            if(!finals.contains(w)) {
                r.unknown=true;
                return r;
            }
            auto shapes=finals[w];
            if(!initial.isEmpty())shapes.prepend(consonantShape(initial,"zh"));
            r.syllables.append(shapes);
        }
        if(r.syllables.isEmpty())r.unknown=true;
        return r;
    }
    static const QMap<QString,QString>& readingWords(const QString&language) {
        static const auto tables=[] {
            initChosutaData();QMap<QString,QMap<QString,QString>> result;
            for(const auto &language:QStringList{"zh","ja"}) {
                QFile file(":/chosuta/"+(language=="zh"?QString("chinese.tsv"):QString("japanese.tsv")));
                if(!file.open(QIODevice::ReadOnly))throw Failure("Missing reading table");
                while(!file.atEnd()) {
                    const auto line=QString::fromUtf8(file.readLine()).trimmed();
                    if(line.startsWith('#')||line.isEmpty())continue;
                    const auto fields=line.split('\t');
                    if(fields.size()==2)result[language][fields[0]]=fields[1];
                }
            }
            return result;
        }();
        return tables.constFind(language).value();
    }
    static Pronunciation withRoles(Pronunciation r) {
        for(int i=0;i<r.syllables.size();++i) {
            if(i<r.roles.size()&&r.roles[i].size()==r.syllables[i].size())continue;
            QVector<SegmentRole> roles;
            for(const auto &shape:r.syllables[i])roles.append(QStringList{"A","I","U","E","O"}.contains(shape)?SegmentRole::Vowel:(shape=="closed"||shape=="open")?SegmentRole::Consonant:SegmentRole::Special);
            if(i<r.roles.size())r.roles[i]=roles;else r.roles.append(roles);
        }
        return r;
    }
    Pronunciation pronounce(const QString&input,const QString&language,bool explicitPhones,const QString&dictionary,const QString&phoneset,const PronunciationOptions&options) {
        Pronunciation r;
        if(input.size()>4096) {
            r.unknown=true;r.provenance="text-limit/estimated";return r;
        }
        QString text=input.normalized(QString::NormalizationForm_KC).trimmed();
        QString l=language;
        if(l=="japanese")l="ja";
        if(l=="mandarin"||l=="chinese")l="zh";
        if(l=="english")l="en";
        if(l=="auto") {
            if(text.contains(QRegularExpression("[\\x{3040}-\\x{30ff}]")))l="ja";
            else if(text.contains(QRegularExpression("[\\x{3400}-\\x{9fff}]")))l="zh";
            else if(explicitPhones) {
                l=phoneset=="arpabet"?"en":phoneset=="xsampa"?"zh":"ja";
                if(phoneset.isEmpty()) {
                    const auto tokens=text.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
                    bool englishVowel=false;
                    for(auto token:tokens) {
                        token.remove(QRegularExpression("[012]$"));
                        if(englishPhones().contains(token.toUpper()))englishVowel=true;
                    }
                    if(englishVowel&&!phones(tokens,"en").unknown)l="en";
                }
            } else if(options.words.value("en").contains(dictionaryKey(text,"en"))||englishWords(dictionary).contains(dictionaryKey(text,"en")))l="en";
            else l="ja";
        }
        if(!QStringList{"ja","zh","en"}.contains(l)) {
            r.unknown=true;r.provenance="unsupported-language/estimated";return r;
        }
        if(explicitPhones) {
            const QString expected=l=="en"?"arpabet":l=="ja"?"romaji":"xsampa";
            if(!phoneset.isEmpty()&&phoneset!=expected) {
                r.unknown=true;r.provenance="unsupported-phoneset/estimated";return r;
            }
            return phones(text.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts),l);
        }
        bool closure=text.startsWith(u'\'')||text.startsWith(u'’');
        if(closure)text.remove(0,1);
        const auto custom=options.words.value(l);
        auto customReading=[&](const DictionaryReading &entry) {
            auto result=pronounce(entry.text,l,entry.phonemes,dictionary);
            result.provenance="user-dictionary/estimated";
            return result;
        };
        const auto key=dictionaryKey(text,l);
        if(custom.contains(key))r=customReading(custom[key]);
        else if(l=="en") {
            r.provenance="english-word-table/estimated";
            const auto words=englishWords(dictionary);
            auto tokens=text.toLower().replace(u'’',u'\'').split(QRegularExpression("[\\s,!.?;:\"()\\-]+"),Qt::SkipEmptyParts);
            for(const auto&w:tokens) {
                Pronunciation p;
                if(custom.contains(w))p=customReading(custom[w]);
                else if(words.contains(w))p=phones(words[w],"en");
                else {r.unknown=true;return r;}
                if(p.unknown) {r.unknown=true;return r;}
                if(custom.contains(w))r.provenance="user-dictionary/estimated";
                r.syllables+=p.syllables;r.roles+=p.roles;
            }
        } else {
            const auto &builtin=readingWords(l);
            const bool allowBuiltin=l=="zh"||options.japaneseKanji;
            r.provenance=l=="zh"?"icu-han-pinyin/estimated":"japanese-rule/estimated";
            auto fallback=[&](const QString &chunk) {return l=="zh"?chinese(chunk):japanese(chunk);};
            auto append=[&](const Pronunciation &piece) {auto tagged=withRoles(piece);r.syllables+=tagged.syllables;r.roles+=tagged.roles;r.unknown=r.unknown||piece.unknown;};
            QString chunk;
            for(qsizetype i=0;i<text.size();) {
                QString found;
                bool user=false;
                for(int length=std::min(qsizetype(128),text.size()-i);length>0;--length) {
                    const auto candidate=text.mid(i,length);
                    if(custom.contains(candidate)) {found=candidate;user=true;break;}
                }
                if(found.isEmpty()&&allowBuiltin)for(int length=std::min(qsizetype(128),text.size()-i);length>0;--length) {
                    const auto candidate=text.mid(i,length);
                    if(builtin.contains(candidate)) {found=candidate;break;}
                }
                if(found.isEmpty()) {chunk+=text[i++];continue;}
                if(!chunk.trimmed().isEmpty())append(fallback(chunk));
                chunk.clear();
                append(user?customReading(custom[found]):fallback(builtin[found]));
                if(user)r.provenance="user-dictionary/estimated";
                else if(r.provenance!="user-dictionary/estimated")r.provenance=l=="zh"?"chinese-phrase-table/estimated":"japanese-kanji-table/estimated";
                i+=found.size();
            }
            if(!chunk.trimmed().isEmpty())append(fallback(chunk));
            if(r.unknown&&l=="ja"&&text.contains(QRegularExpression("[\\x{3400}-\\x{9fff}]")))
                r.provenance=options.japaneseKanji?"unresolved-japanese-kanji/estimated":"japanese-kanji-disabled/estimated";
        }
        if(r.syllables.isEmpty())r.unknown=true;
        r=withRoles(r);
        if(closure&&!r.syllables.isEmpty()){r.syllables.first().prepend("closed");r.roles.first().prepend(SegmentRole::Special);}
        return r;
    }
}
