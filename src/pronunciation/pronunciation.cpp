// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
#include <unicode/translit.h>
#include <unicode/unistr.h>
#include <memory>
static void initChosutaData() {
    Q_INIT_RESOURCE(data);
}
namespace chosuta {
    static QString transliterate(QString text,const char*id) {
        UErrorCode error=U_ZERO_ERROR;
        std::unique_ptr<icu::Transliterator>t(icu::Transliterator::createInstance(id,UTRANS_FORWARD,error));
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
        if(!f.open(QIODevice::ReadOnly)||f.size()>32*1024*1024)throw Failure("Cannot read English dictionary (limit 32 MiB): "+path);
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
            "B","CH","D","DH","F","G","HH","JH","K","L","M","N","NG","P","R","S","SH","T","TH","V","W","Y","Z","ZH"
        };
        const QStringList jpConsonants= {
            "b","by","ch","d","dy","f","g","gy","h","hy","j","k","ky","m","my","n","ny","p","py","r","ry","s","sh","t","ts","ty","v","w","y","z"
        };
        for(QString token:tokens) {
            token.remove(QRegularExpression("[012]$"));
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
            else if(QStringList {
                "a","i","u","e","o"
            }
            .contains(token.toLower()))shape= {
                token.toUpper()
            };
            else if((language!="en"&&(token=="N"||token=="ng"))||token=="nasal")shape= {
                "nasal"
            };
            else if((language=="en"&&engConsonants.contains(token.toUpper()))||jpConsonants.contains(token)||(language=="zh"&&QStringList{"zh","ch","c","x","q","l"}.contains(token))) {
                pending<<consonantShape(token,language);
                continue;
            }
            else {
                r.unknown=true;
                return r;
            }
            r.syllables.append(pending+shape);
            pending.clear();
        }
        if(!pending.isEmpty()) {
            if(r.syllables.isEmpty())r.syllables.append(pending);
            else r.syllables.last().append(pending);
        }
        if(r.syllables.isEmpty())r.unknown=true;
        return r;
    }
    static Pronunciation japanese(QString text) {
        Pronunciation r;
        r.provenance="japanese-rule/estimated";
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
                if(c.isSpace()||c.isPunct())continue;
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
                    if(r.syllables.isEmpty())r.syllables.append( {
                        v
                    });
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
        s.remove(QRegularExpression("[\\s']"));
        QStringList heads= {
            "ky","gy","sh","ch","ny","hy","by","py","my","ry","ts","sy","ty","dy","zy","b","d","f","g","h","j","k","m","n","p","r","s","t","v","w","y","z",""
        };
        while(!s.isEmpty()) {
            if(s.startsWith('n')&&(s.size()==1||(!QString("aiueoy").contains(s[1])&&s[1]!='n'))) {
                r.syllables.append( {
                    "nasal"
                });
                s.remove(0,1);
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
        QString s=transliterate(text,"Han-Latin; NFD; [:Nonspacing Mark:] Remove; NFC; Lower");
        s.replace(u'ü',u'v');
        s.remove(QRegularExpression("[1-5]"));
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
        finals["ue"]= {
            "U","E"
        };
        QStringList initials= {
            "zh","ch","sh","b","p","m","f","d","t","n","l","g","k","h","j","q","x","r","z","c","s","y","w"
        };
        for(auto w:words) {
            QString initial;
            for(const auto&i:initials)if(w.startsWith(i)) {
                initial=i;
                w.remove(0,i.size());
                break;
            }
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
    Pronunciation pronounce(const QString&text,const QString&language,bool explicitPhones,const QString&dictionary) {
        if(text.size()>4096) {
            Pronunciation r;
            r.unknown=true;
            r.provenance="text-limit/estimated";
            return r;
        }
        QString l=language;
        if(l=="auto") {
            if(text.contains(QRegularExpression("[\\x{3040}-\\x{30ff}]")))l="ja";
            else if(text.contains(QRegularExpression("[\\x{3400}-\\x{9fff}]")))l="zh";
            else if(explicitPhones&&text.contains(QRegularExpression("[A-Z]{2}")))l="en";
            else l="ja";
            // Empty SV database defaults to Japanese for the supplied kana projects; explicit UI override is available.
        }
        if(explicitPhones) {
            if(l=="zh"&&text==text.toLower()&&!text.contains(' '))return chinese(text);
            return phones(text.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts),l);
        }
        if(l=="ja")return japanese(text);
        if(l=="zh")return chinese(text);
        Pronunciation r;
        r.provenance="english-word-table/estimated";
        auto words=englishWords(dictionary);
        auto tokens=text.toLower().replace(u'’',u'\'').split(QRegularExpression("[\\s,!.?;:]+"),Qt::SkipEmptyParts);
        for(const auto&w:tokens) {
            if(!words.contains(w)) {
                r.unknown=true;
                return r;
            }
            auto p=phones(words[w],"en");
            if(p.unknown) {
                r.unknown=true;
                return r;
            }
            r.syllables+=p.syllables;
        }
        if(r.syllables.isEmpty())r.unknown=true;
        return r;
    }
}
