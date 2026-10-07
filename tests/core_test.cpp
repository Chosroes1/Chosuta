// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include "core/model.h"
#include "core/executable.h"
#include "render/render.h"
#include "render/audio.h"
#include <cmath>
#include <QPainter>
using namespace chosuta;
static QByteArray fixture() {
    QFile f(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");
    if(!f.open(QIODevice::ReadOnly))throw Failure("Test fixture absent");
    return f.readAll();
}
static Project basic() {
    Project p;
    p.score=parseSvp(fixture());
    p.selected= {
        p.score.tracks[0].id
    };
    p.regenerate();
    return p;
}
class CoreTest:public QObject {
    Q_OBJECT
    private slots:
    void executableResolution() {
        QTemporaryFile tool(QDir(QCoreApplication::applicationDirPath()).filePath("chosuta-tool-XXXXXX.exe"));
        QVERIFY(tool.open());QVERIFY(tool.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        tool.close();
        const auto absolute=QFileInfo(tool.fileName()).absoluteFilePath();const auto name=QFileInfo(absolute).fileName();
        QCOMPARE(resolveExecutable(name),absolute);
#ifdef Q_OS_WIN
        QCOMPARE(resolveExecutable(name.chopped(4)),absolute);
#endif
        QCOMPARE(resolveExecutable(absolute),absolute);
        auto relative=QDir::current().relativeFilePath(absolute);
        if(relative.contains('/')||relative.contains('\\'))QCOMPARE(resolveExecutable(relative),absolute);
        const auto missing="chosuta-absent-"+QUuid::createUuid().toString(QUuid::Id128);
        QCOMPARE(resolveExecutable(missing),missing);QCOMPARE(resolveExecutable(QString()),QString());
    }
    void time() {
        TimeMap t;
        t.tempos= {
            {
                0,120
            }, {
                2*Blick,60
            }
        };
        t.meters= {
            {
                0,2,4
            }, {
                1,6,8
            }
        };
        t.validate();
        QCOMPARE(t.seconds(Blick),.5);
        QCOMPARE(t.seconds(3*Blick),2.);
        QCOMPARE(t.seconds(3*Blick)-t.seconds(Blick),1.5);
        QCOMPARE(t.blicks(2),3*Blick);
        QCOMPARE(t.beatLabel(2*Blick),QString("2:1:0.000"));
        QCOMPARE(t.beatLabel(3*Blick),QString("2:3:0.000"));
        QCOMPARE(t.blicks(t.seconds(99789479)),qint64(99789479));
        t.tempos[0].bpm=0;
        QVERIFY_EXCEPTION_THROWN(t.validate(),Failure);
    }
    void parsing() {
        auto p=basic();
        QCOMPARE(p.score.tracks.size(),1);
        QCOMPARE(p.score.tracks[0].notes.size(),4);
        QCOMPARE(p.score.tracks[0].notes[0].onset,qint64(0));
        QCOMPARE(p.generated[0].shape,QString("A"));
        QVERIFY(p.generated[0].provenance.contains("explicit"));
        auto root=checkedJson(fixture()).object();
        auto track=root["tracks"].toArray()[0].toObject();
        auto group=track["mainGroup"].toObject();
        QJsonArray refs {
            QJsonObject {
                {
                    "groupID",group["uuid"]
                }, {
                    "blickOffset",Blick
                }, {
                    "pitchOffset",2
                }, {
                    "uuid","repeat"
                }
            },QJsonObject {
                {
                    "groupID",group["uuid"]
                }, {
                    "blickOffset",2*Blick
                }, {
                    "uuid","repeat"
                }
            }
        };
        track["mainGroup"]=QJsonObject {
            {
                "notes",QJsonArray{}
            }
        };
        track["groups"]=refs;
        root["tracks"]=QJsonArray {
            track
        };
        root["library"]=QJsonArray {
            group,QJsonObject {
                {
                    "uuid","unused"
                }, {
                    "notes",group["notes"]
                }
            }
        };
        auto s=parseSvp(QJsonDocument(root).toJson());
        QCOMPARE(s.tracks[0].notes.size(),8);
        QCOMPARE(s.tracks[0].notes[0].pitch,62);
        QSet<QString>ids;
        for(const auto&n:s.tracks[0].notes)ids.insert(n.id);
        QCOMPARE(ids.size(),8);
        auto bytes=fixture();
        bytes.append('\0');
        QCOMPARE(parseSvp(bytes).tracks[0].notes.size(),4);
        bytes.append('\0');
        QVERIFY_EXCEPTION_THROWN(parseSvp(bytes),Failure);
        QVERIFY_EXCEPTION_THROWN(checkedJson(QByteArray(65,'[')),Failure);
    }
    void samples() {
#ifdef CHOSUTA_PRIVATE_SAMPLES
#include "private_samples.inc"
#else
        QSKIP("Private regression data is not distributed");
#endif
    }
    void language() {
        QVERIFY(!pronounce("きょ","ja").unknown);
        QCOMPARE(pronounce("きょ","ja").syllables[0].last(),QString("O"));
        QCOMPARE(pronounce("りゅ","ja").syllables[0].last(),QString("U"));
        QCOMPARE(pronounce("アイウエオ","ja").syllables.size(),5);
        QVERIFY(pronounce("unmapped","ja").unknown);
        QVERIFY(!pronounce("hello world","en").unknown);
        QVERIFY(pronounce("zzzxq","en").unknown);
        QVERIFY(!pronounce("你好世界","zh").unknown);
        QVERIFY(!pronounce("ni3 hao3","zh").unknown);
        QCOMPARE(pronounce("a","ja",true).syllables[0][0],QString("A"));
    }
    void sourceLanguageOverrides() {
        auto root=checkedJson(fixture()).object();
        auto track=root["tracks"].toArray()[0].toObject();
        auto group=track["mainGroup"].toObject();auto notes=group["notes"].toArray();
        auto note=notes[0].toObject();
        note["phonemes"]="ay";
        note["attributes"]=QJsonObject{{"languageOverride","english"},{"phonesetOverride","arpabet"}};
        notes[0]=note;
        note=notes[1].toObject();note["attributes"]=QJsonObject{{"languageOverride","korean"}};notes[1]=note;
        group["notes"]=notes;track["mainGroup"]=group;
        root["tracks"]=QJsonArray{track};
        Project p;p.score=parseSvp(QJsonDocument(root).toJson());p.selected={p.score.tracks[0].id};p.regenerate();
        QCOMPARE(p.score.tracks[0].notes[0].language,QString("en"));
        QCOMPARE(p.score.tracks[0].notes[0].phoneset,QString("arpabet"));
        QCOMPARE(shapeAt(p,p.effective(),.1),QString("A"));
        QCOMPARE(shapeAt(p,p.effective(),.4),QString("I"));
        QCOMPARE(shapeAt(p,p.effective(),.6),QString("unknown"));
        QVERIFY(std::any_of(p.score.diagnostics.begin(),p.score.diagnostics.end(),[](const auto&d){return d.code=="language";}));
        QTemporaryDir dir;saveProject(p,dir.filePath("language.chosuta"));auto restored=loadProject(dir.filePath("language.chosuta"));
        QCOMPARE(restored.score.tracks[0].notes[0].language,QString("en"));
        restored.regenerate();QCOMPARE(shapeAt(restored,restored.effective(),.4),QString("I"));
        auto incompatible=pronounce("ay","en",true,{},"romaji");QVERIFY(incompatible.unknown);
        QVERIFY(pronounce("a","en",true).unknown);
        QVERIFY(!pronounce("hh ax l ow","en",true,{},"arpabet").unknown);
        QVERIFY(!pronounce("hh ah ll ow","auto",true).unknown);
        QCOMPARE(pronounce("sh i","auto",true).syllables,pronounce("sh i","ja",true).syllables);
        QCOMPARE(pronounce("N","auto",true).syllables,(QVector<QStringList>{{"nasal"}}));
        // An unvoiced library reference inherits the track's language/phoneset.
        track["mainGroup"]=QJsonObject{};track["groups"]=QJsonArray{QJsonObject{{"groupID",group["uuid"]}}};
        root["tracks"]=QJsonArray{track};root["library"]=QJsonArray{group};
        const auto inherited=parseSvp(QJsonDocument(root).toJson());
        QCOMPARE(inherited.tracks[0].notes[2].language,QString("ja"));
        QCOMPARE(inherited.tracks[0].notes[2].phoneset,QString("romaji"));
    }
    void normalizedReadings() {
        QCOMPARE(pronounce("ｷｮｰ","ja").syllables,pronounce("きょー","ja").syllables);
        QCOMPARE(pronounce("きょー","ja").syllables.size(),2);
        QCOMPARE(pronounce("shin'ya","ja").syllables.size(),3);
        QCOMPARE(pronounce("shin'ya","ja").syllables[1],QStringList{"nasal"});
        QVERIFY(pronounce("っゃ","ja").unknown);
        QVERIFY(pronounce("未登録語","ja").unknown);
        for(const auto &reading:QStringList{"nüè","nu:e4","nve4","lüè","jue2","yuan2","yun2","ni3hao3"})
            QVERIFY2(!pronounce(reading,"zh").unknown,qPrintable(reading));
        QCOMPARE(pronounce("ni3hao3","zh").syllables.size(),2);
        QCOMPARE(pronounce("nüè","zh").syllables[0],(QStringList{"open","U","E"}));
        QVERIFY(!pronounce("ＨＥＬＬＯ","en").unknown);
        QCOMPARE(pronounce("hello","auto").syllables,pronounce("hello","en").syllables);
    }
    void continuationInstances() {
        auto p=basic();auto &notes=p.score.tracks[0].notes;
        for(auto &note:notes){note.phonemes.clear();note.language="en";note.phoneset="arpabet";}
        notes[0].lyrics="beautiful";notes[1].lyrics="＋";notes[2].lyrics="－";notes[3].lyrics="＋";
        p.regenerate();
        for(const auto &event:p.generated)QVERIFY(!event.unknown);
        // A final continuation receives the remaining syllables, not a lost tail.
        notes[0].lyrics="yesterday";notes[2].lyrics="hello";notes[3].lyrics="-";p.regenerate();
        QCOMPARE(shapeAt(p,p.effective(),.95),QString("I"));
        for(const auto &event:p.generated)QVERIFY(event.shape!="open");
        // Each instance keeps its own word and extension state.
        notes[0].lyrics="hello";notes[0].groupInstance="first";
        notes[1].lyrics="-";notes[1].groupInstance="second";
        notes[2].lyrics="+";notes[2].groupInstance="first";
        notes[3].lyrics="+";notes[3].groupInstance="second";
        p.regenerate();
        QCOMPARE(shapeAt(p,p.effective(),.25),QString("E"));
        QCOMPARE(shapeAt(p,p.effective(),.75),QString("closed"));
        QCOMPARE(shapeAt(p,p.effective(),1.4),QString("U"));
        QCOMPARE(shapeAt(p,p.effective(),1.75),QString("unknown"));
        // Extending a syllable ending in a stop holds the vowel, not the stop.
        notes[0].phonemes="aa t";notes[1].groupInstance="first";notes[2].lyrics="cl";notes[3].lyrics="br";
        p.regenerate();QCOMPARE(shapeAt(p,p.effective(),.75),QString("A"));
    }
    void smallDictionaries() {
        QVERIFY(builtinDictionaryBytes()<DictionaryByteLimit);
        PronunciationOptions optional;optional.japaneseKanji=true;
        for(const auto &pair:QList<QPair<QString,QString>>{{"en","english.tsv"},{"zh","chinese.tsv"},{"ja","japanese.tsv"}}) {
            QFile file(":/chosuta/"+pair.second);QVERIFY(file.open(QIODevice::ReadOnly));
            QSet<QString>seen;
            while(!file.atEnd()) {
                const auto row=QString::fromUtf8(file.readLine()).trimmed();if(row.startsWith('#')||row.isEmpty())continue;
                const auto fields=row.split('\t');QCOMPARE(fields.size(),2);QVERIFY(!seen.contains(fields[0]));seen.insert(fields[0]);
                const auto result=pronounce(fields[0],pair.first,false,{},{},optional);
                QVERIFY2(!result.unknown,qPrintable(pair.first+": "+fields[0]));QVERIFY(!result.syllables.isEmpty());
            }
        }
        QVERIFY(pronounce("今日","ja").unknown);
        QVERIFY(!pronounce("今日","ja",false,{},{},optional).unknown);
        QVERIFY(pronounce("未登録語","ja",false,{},{},optional).unknown);
        QCOMPARE(pronounce("かなカナ","ja").syllables,pronounce("かなカナ","ja",false,{},{},optional).syllables);
        QCOMPARE(pronounce("golden","en").syllables.size(),2);
        const auto golden=pronounce("golden","en");QCOMPARE(golden.syllables[0].last(),QString("open"));
        QCOMPARE(golden.syllables[1].first(),QString("closed"));
        QVERIFY(!pronounce("age","en").unknown);
        QCOMPARE(pronounce("银行","zh").syllables[1][1],QString("A"));
    }
    void customDictionaryPersistence() {
        PronunciationOptions options;
        options.words["en"]["hello"]={"UW",true};
        options.words["zh"]["行"]={"hang2",false};
        options.words["ja"]["銀河"]={"ぎんが",false};
        validatePronunciationOptions(options);
        QCOMPARE(pronounce("HELLO","en",false,{},{},options).syllables,(QVector<QStringList>{{"U"}}));
        QCOMPARE(pronounce("AA","en",true,{},{},options).syllables,(QVector<QStringList>{{"A"}}));
        QVERIFY(!pronounce("銀河","ja",false,{},{},options).unknown);
        QCOMPARE(pronounce("行","zh",false,{},{},options).syllables[0][1],QString("A"));
        QCOMPARE(pronounce("行走","zh",false,{},{},options).syllables[0][1],QString("A"));
        QCOMPARE(pronunciationOptionsRead(pronunciationOptionsJson(options)),options);
        auto p=basic();p.rules.pronunciation=options;
        auto &note=p.score.tracks[0].notes[0];note.phonemes.clear();note.lyrics="hello";note.language="en";note.phoneset="arpabet";p.regenerate();
        QCOMPARE(shapeAt(p,p.effective(),.25),QString("U"));
        p.rules.readings[note.id]="AA";p.regenerate();QCOMPARE(shapeAt(p,p.effective(),.25),QString("A"));
        auto edit=p.generated[0];edit.shape="O";p.edit(edit,true);
        p.rules.pronunciation.words["en"]["hello"]={"IY",true};p.regenerate();QCOMPARE(shapeAt(p,p.effective(),.25),QString("O"));
        // Save real embedded source: custom source modifications above are deliberately not serialized as the original fixture.
        p=basic();p.rules.pronunciation=options;
        QTemporaryDir dir;const auto path=dir.filePath("辞書.chosuta");saveProject(p,path);auto restored=loadProject(path);
        QCOMPARE(restored.rules.pronunciation,options);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();
        QCOMPARE(root["schema"].toInt(),4);
        root["schema"]=2;auto rules=root["rules"].toObject();rules.remove("pronunciation");root["rules"]=rules;
        QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();
        restored=loadProject(path);QCOMPARE(restored.rules.pronunciation,PronunciationOptions{});
    }
    void dictionaryValidation() {
        PronunciationOptions oversized;
        for(int i=0;i<1200;++i)oversized.words["en"][QString("word%1").arg(i)]={QString("AA ").repeated(1024),true};
        QVERIFY(pronunciationDictionaryBytes(oversized)>DictionaryByteLimit);
        QVERIFY_EXCEPTION_THROWN(validatePronunciationOptions(oversized),Failure);
        QJsonObject row{{"language","en"},{"word","X"},{"reading","AA"},{"notation","phonemes"}};
        auto duplicate=row;duplicate["word"]="x";
        const QJsonObject object{{"version",1},{"japaneseKanji",false},{"entries",QJsonArray{row,duplicate}}};
        QVERIFY_EXCEPTION_THROWN(pronunciationOptionsRead(object),Failure);
        auto invalid=object;invalid["entries"]=QJsonArray{QJsonObject{{"language","en"},{"word","x"},{"reading","not-a-phone"},{"notation","phonemes"}}};
        QVERIFY_EXCEPTION_THROWN(pronunciationOptionsRead(invalid),Failure);
        QTemporaryDir dir;QFile file(dir.filePath("oversized.dict"));QVERIFY(file.open(QIODevice::WriteOnly));QVERIFY(file.resize(DictionaryByteLimit+1));file.close();
        auto p=basic();p.rules.englishDictionary=file.fileName();QVERIFY_EXCEPTION_THROWN(p.regenerate(),Failure);
    }
    void boundedConsonantTiming() {
        auto sequence=[](const QString &phones,double seconds,double ratio=.18){auto p=basic();auto root=checkedJson(p.score.raw).object();auto track=root["tracks"].toArray()[0].toObject();auto group=track["mainGroup"].toObject();auto n=group["notes"].toArray()[0].toObject();n["phonemes"]=phones;n["duration"]=double(p.score.time.blicks(seconds));group["notes"]=QJsonArray{n};track["mainGroup"]=group;auto ref=track["mainRef"].toObject();ref["database"]=QJsonObject{{"language","english"},{"phoneset","arpabet"}};track["mainRef"]=ref;root["tracks"]=QJsonArray{track};p.score=parseSvp(QJsonDocument(root).toJson());p.selected={p.score.tracks[0].id};p.rules.consonantRatio=ratio;p.regenerate();return p;};
        auto longNote=sequence("P AA T",4);QCOMPARE(longNote.generated.size(),3);QVERIFY(std::abs(longNote.generated[0].end-.08)<1e-9);QVERIFY(std::abs(longNote.generated[2].start-3.92)<1e-9);QCOMPARE(longNote.generated[2].end,4.);
        auto shortNote=sequence("P AA T",.05);QVERIFY(std::abs(shortNote.generated[0].end-.009)<1e-9);QVERIFY(std::abs(shortNote.generated[2].start-.041)<1e-9);
        auto veryShort=sequence("P AA T",.01,.8);QVERIFY(std::abs(veryShort.generated[0].end-.004)<1e-9);QVERIFY(std::abs(veryShort.generated[2].start-.006)<1e-9);
        auto cluster=sequence("P R AA",4);QVERIFY(std::abs(cluster.generated[0].end-.04)<1e-9);QVERIFY(std::abs(cluster.generated[1].end-.08)<1e-9);
        auto coda=sequence("AA T",2);QVERIFY(std::abs(coda.generated.last().start-1.92)<1e-9);
        auto multiple=sequence("P AA P IY",4);QCOMPARE(multiple.generated.size(),4);QVERIFY(std::abs(multiple.generated[2].end-2.08)<1e-9);
        for(auto phone:QStringList{"AA","P","cl","br","nasal"}){auto p=sequence(phone,2);QCOMPARE(p.generated.size(),1);QCOMPARE(p.generated[0].start,0.);QCOMPARE(p.generated[0].end,2.);}
        auto special=sequence("cl AA",2);QVERIFY(std::abs(special.generated[0].end-1.)<1e-9); // Explicit closure keeps its separate semantics.
        auto roles=pronounce("P AA T","en",true);QVERIFY(roles.roles[0]==QVector<SegmentRole>({SegmentRole::Consonant,SegmentRole::Vowel,SegmentRole::Consonant}));
        QVERIFY(pronounce("cl AA","en",true).roles[0][0]==SegmentRole::Special);
        auto varied=sequence("P AA T",1.5);varied.score.time.tempos={{0,120},{Blick,60}};varied.score.time.validate();varied.regenerate();QCOMPARE(varied.generated.last().end,2.5);QVERIFY(std::abs(varied.generated.last().start-2.42)<1e-9);
        auto legacy=sequence("P AA T",2);legacy.rules.consonantMaxSeconds=0;legacy.regenerate();QVERIFY(std::abs(legacy.generated[0].end-.36)<1e-9);QVERIFY(std::abs(legacy.generated.last().start-1.18)<1e-9);
        QTemporaryDir dir;auto path=dir.filePath("timing.chosuta");saveProject(legacy,path);QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();auto rules=root["rules"].toObject();rules.remove("consonantMaxSeconds");root["rules"]=rules;
        auto write=[&]{if(!file.open(QIODevice::WriteOnly))return false;file.write(QJsonDocument(root).toJson());file.close();return true;};QVERIFY(write());auto old=loadProject(path);QCOMPARE(old.rules.consonantMaxSeconds,0.);QVERIFY(std::abs(old.generated[0].end-.36)<1e-9);old.regenerate();QVERIFY(std::abs(old.generated[0].end-.36)<1e-9);
        auto locked=old.generated[0];locked.shape="E";locked.start=.01;locked.end=.02;old.edit(locked);old.rules.consonantMaxSeconds=.08;old.regenerate();QCOMPARE(shapeAt(old,old.effective(),.015),QString("E"));QCOMPARE(old.overrides[locked.id].event.end,.02);QVERIFY(std::abs(old.generated[0].end-.08)<1e-9);saveProject(old,path);QCOMPARE(loadProject(path).rules.consonantMaxSeconds,.08);
        rules["consonantMaxSeconds"]="80";root["rules"]=rules;QVERIFY(write());QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);rules["consonantMaxSeconds"]=1.1;root["rules"]=rules;QVERIFY(write());QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);
        old.rules.consonantMaxSeconds=std::numeric_limits<double>::quiet_NaN();QVERIFY_EXCEPTION_THROWN(old.regenerate(),Failure);QVERIFY_EXCEPTION_THROWN(saveProject(old,path),Failure);
    }
    void consonantDefaults() {
        for(const auto &phone:QStringList{"B","P","M","T","D","K","G","CH","JH"})
            QCOMPARE(pronounce(phone+" AA","en",true).syllables[0][0],QString("closed"));
        for(const auto &phone:QStringList{"F","V","S","Z","SH","ZH","TH","DH","HH","R","L","W","Y","N","NG"}){
            QCOMPARE(pronounce(phone+" AA","en",true).syllables[0][0],QString("open"));
            auto p=basic();p.score.tracks[0].notes[0].language="en";p.score.tracks[0].notes[0].phoneset="arpabet";p.score.tracks[0].notes[0].phonemes=phone+" AA";p.regenerate();
            QCOMPARE(shapeAt(p,p.effective(),.02),QString("A"));
            for(const auto&e:p.generated)QVERIFY(e.shape!="open");
        }
        for(const auto &lyric:QStringList{"か","ま","ぱ","ka","ma","pa"})QCOMPARE(pronounce(lyric,"ja").syllables[0][0],QString("closed"));
        for(const auto &lyric:QStringList{"さ","ら","は","sa","ra","ha"})QCOMPARE(pronounce(lyric,"ja").syllables[0][0],QString("open"));
        for(const auto &lyric:QStringList{"ba","pa","ma","ka"})QCOMPARE(pronounce(lyric,"zh").syllables[0][0],QString("closed"));
        for(const auto &lyric:QStringList{"shi","hao","ri","li"})QCOMPARE(pronounce(lyric,"zh").syllables[0][0],QString("open"));
        auto p=basic();p.score.tracks[0].notes[0].phonemes="a s i";p.regenerate();
        const auto segments=p.generated;QVERIFY(segments.size()>4);QCOMPARE(segments[1].shape,QString("I"));
    }
    void subtitleTimingAndSources() {
        auto p=basic();p.output.syncOffset=.25;p.output.audioOffset=7;
        auto id=p.score.tracks[0].id;auto spans=lyricSpans(p,id);QCOMPARE(spans.size(),4);
        SubtitleCue cue;cue.id="cue";cue.text="手动字幕";alignSubtitle(p,cue,id,spans[0],spans[2]);
        auto v=subtitleInterval(p,cue);QCOMPARE(v.start,.25);QCOMPARE(v.end,1.75);QCOMPARE(cue.startBlick,qint64(0));QCOMPARE(cue.endBlick,3*Blick);
        p.score.time.tempos={{0,120},{Blick,60}};p.score.time.validate();v=subtitleInterval(p,cue);QCOMPARE(v.start,.25);QCOMPARE(v.end,2.75);
        SubtitleTrack track;track.id="t";track.cues={cue};p.subtitles={track};p.subtitlesEnabled=true;
        auto preserved=p.subtitles;p.regenerate();auto e=p.generated[0];p.split(e,(e.start+e.end)/2);p.regenerate();QCOMPARE(p.subtitles,preserved);
        p.score.tracks[0].notes.removeAt(0);v=subtitleInterval(p,cue);QVERIFY(v.orphan);QCOMPARE(v.start,.25);QCOMPARE(v.end,1.75);QCOMPARE(cue.text,QString("手动字幕"));
        setSubtitleTime(p,cue,3,8,false);QCOMPARE(subtitleInterval(p,cue).start,3.);p.output.syncOffset=10;QCOMPARE(subtitleInterval(p,cue).start,3.);
        p=basic();auto&notes=p.score.tracks[0].notes;notes[1].lyrics="+";notes[2].lyrics="ー";spans=lyricSpans(p,p.score.tracks[0].id);QCOMPARE(spans.size(),2);QCOMPARE(spans[0].end,3*Blick);QCOMPARE(spans[0].notes.size(),3);
        notes[1].groupInstance="other";spans=lyricSpans(p,p.score.tracks[0].id);QCOMPARE(spans[0].end,Blick); // A different instance cannot continue the word.
        notes[1].groupInstance=notes[0].groupInstance;notes[1].muted=true;spans=lyricSpans(p,p.score.tracks[0].id);QCOMPARE(spans[0].end,Blick);
        notes[1].muted=false;notes[1].lyrics="br";spans=lyricSpans(p,p.score.tracks[0].id);QCOMPARE(spans[0].end,Blick);
    }
    void subtitlePersistenceAndValidation() {
        auto p=basic();QTemporaryDir dir;SubtitleTrack t;t.id="sub-track";t.name="訳 / Translation";t.style.color=QColor(12,180,220,190);t.style.x=.3;t.style.fontHeight=.07;t.alignLyrics=true;t.sourceTrack=p.score.tracks[0].id;
        SubtitleCue c;c.id="sub-cue";c.text="<literal> 中文\nかな English";auto spans=lyricSpans(p,t.sourceTrack);alignSubtitle(p,c,t.sourceTrack,spans[1],spans[2]);c.ownStyle=true;c.style.x=.7;c.style.bold=true;t.cues={c};p.subtitles={t};p.subtitlesEnabled=true;
        auto path=dir.filePath("字幕.chosuta");saveProject(p,path);auto q=loadProject(path);QVERIFY(q.subtitlesEnabled);QCOMPARE(q.subtitles,p.subtitles);q.regenerate();QCOMPARE(q.subtitles,p.subtitles);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();QCOMPARE(root["schema"].toInt(),4);
        root["schema"]=3;root.remove("subtitles");QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();q=loadProject(path);QVERIFY(!q.subtitlesEnabled);QVERIFY(q.subtitles.isEmpty());
        p.subtitles[0].cues.append(c);QVERIFY_EXCEPTION_THROWN(validateSubtitles(p),Failure);p.subtitles[0].cues.last().id="another";QVERIFY_EXCEPTION_THROWN(validateSubtitles(p,true),Failure);
        p.subtitles[0].cues.removeLast();p.subtitles[0].cues[0].text=QString(4097,'x');QVERIFY_EXCEPTION_THROWN(validateSubtitles(p),Failure);p.subtitles[0].cues[0]=c;
        p.subtitles[0].style.fontHeight=std::numeric_limits<double>::quiet_NaN();QVERIFY_EXCEPTION_THROWN(validateSubtitles(p),Failure);p.subtitles[0].style=t.style;
        auto data=subtitlesJson(p);auto tracks=data["tracks"].toArray();auto row=tracks[0].toObject();auto cues=row["cues"].toArray();auto invalid=cues[0].toObject();invalid["startBlick"]="-9223372036854775808";cues[0]=invalid;row["cues"]=cues;tracks[0]=row;data["tracks"]=tracks;
        q=basic();QVERIFY_EXCEPTION_THROWN(readSubtitles(q,data),Failure);
        data=subtitlesJson(p);data["enabled"]="false";q=basic();QVERIFY_EXCEPTION_THROWN(readSubtitles(q,data),Failure);
        p.subtitles[0].cues[0]=c;p.subtitles[0].cues[0].anchor="seconds";p.subtitles[0].cues[0].start=3;p.subtitles[0].cues[0].end=5;QCOMPARE(p.duration(),5.);p.output.duration=2;QCOMPARE(p.duration(),2.);p.output.duration=0;p.subtitlesEnabled=false;QCOMPARE(p.duration(),2.);
    }
    void subtitleRenderingAndExport() {
        auto p=basic();p.canvas.width=320;p.canvas.height=180;p.canvas.transparent=true;p.output.format="mov";p.output.fpsNum=4;p.output.duration=1;
        SubtitleTrack t;t.id="text";t.name="Original text";t.style.color=Qt::green;t.style.outline=false;t.style.fontHeight=.13;t.style.y=.35;t.style.width=.8;
        SubtitleCue c;c.id="caption";c.text="Hello 中文\nかな";c.start=.25;c.end=.75;t.cues={c};p.subtitles={t};p.subtitlesEnabled=true;
        Scene scene(p);auto empty=scene.frame(0),visible=scene.frame(.25);QVERIFY(visible!=empty);QCOMPARE(scene.frame(.75),empty);QCOMPARE(scene.frame(.749),visible);
        auto box=scene.subtitleRect(t.style,c.text);auto single=scene.subtitleRect(t.style,"Hello 中文 かな");QVERIFY(box.height()>single.height());
        bool green=false;for(int y=0;y<visible.height();++y)for(int x=0;x<visible.width();++x){auto pixel=visible.pixelColor(x,y);if(pixel.alpha()>0&&pixel.green()>pixel.red())green=true;}QVERIFY(green);
        auto q=p;q.subtitlesEnabled=false;QCOMPARE(Scene(q).frame(.25),empty);q=p;q.subtitles[0].enabled=false;QCOMPARE(Scene(q).frame(.25),empty);
        auto second=t;second.id="translation";second.style.y=.8;second.style.color=Qt::red;second.cues[0].id="translated";second.cues[0].text="Translation";p.subtitles.append(second);QVERIFY(Scene(p).frame(.25)!=visible);
        q=p;q.subtitles[0].style.family="Chosuta absent font 6a71";QVERIFY(Scene(q).diagnostics.join('\n').contains("font substituted"));
        q=p;q.subtitles[0].style.color=QColor(0,255,0,0);q.subtitles[0].style.outline=true;q.subtitles[1].enabled=false;QCOMPARE(Scene(q).frame(.25),empty);
        q=p;q.output.duration=.5;QVERIFY(Scene(q).diagnostics.join('\n').contains("fixed animation duration"));
        q=p;auto collision=q.subtitles[0].cues[0];collision.id="overlapping";collision.text="Overlap retained";q.subtitles[0].cues.append(collision);Scene overlapping(q);QVERIFY(overlapping.diagnostics.join('\n').contains("Subtitle overlap"));QVERIFY(overlapping.frame(.25)!=Scene(p).frame(.25));
        QTemporaryDir dir;QImage transparent(32,32,QImage::Format_RGBA8888);transparent.fill(Qt::transparent);auto asset=dir.filePath("closed.png");QVERIFY(transparent.save(asset));p.assets[p.fallback]=asset;
        if(QStandardPaths::findExecutable("ffmpeg").isEmpty())QSKIP("FFmpeg absent");
        std::atomic_bool cancel=false;auto movie=dir.filePath("original-subtitle.mov");auto result=exportVideo(p,movie,cancel);QVERIFY2(result.success,qPrintable(result.error));QCOMPARE(result.frames,4);
        QProcess decode;decode.start("ffmpeg",{"-v","error","-i",movie,"-f","rawvideo","-pix_fmt","rgba","pipe:1"});QVERIFY(decode.waitForFinished());QCOMPARE(decode.exitCode(),0);auto rgba=decode.readAllStandardOutput();QCOMPARE(rgba.size(),320*180*4*4);
        Scene expected(p);for(int i=0;i<4;++i){auto image=expected.frame(i/4.);QCOMPARE(rgba.mid(i*320*180*4,320*180*4),QByteArray(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()));}
        if(qEnvironmentVariableIsSet("CHOSUTA_SUBTITLE_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_SUBTITLE_ARTIFACTS"));QVERIFY(out.mkpath("."));QVERIFY(transparent.save(out.filePath("closed.png")));p.assets[p.fallback]=out.filePath("closed.png");saveProject(p,out.filePath("original-subtitle.chosuta"));QVERIFY(expected.frame(.25).save(out.filePath("subtitle-frame.png")));QFile source(movie);QVERIFY(source.open(QIODevice::ReadOnly));QSaveFile destination(out.filePath("original-subtitle.mov"));QVERIFY(destination.open(QIODevice::WriteOnly));auto bytes=source.readAll();QCOMPARE(destination.write(bytes),bytes.size());QVERIFY(destination.commit());}
    }
    void canvasAndMigration() {
        QTemporaryDir dir;auto p=basic();
        QImage bg(80,40,QImage::Format_ARGB32);bg.fill(Qt::blue);{QPainter paint(&bg);paint.fillRect(0,0,20,40,Qt::yellow);}QVERIFY(bg.save(dir.filePath("背景.png")));
        QImage character(40,40,QImage::Format_ARGB32);character.fill(Qt::red);QVERIFY(character.save(dir.filePath("立绘.png")));
        p.canvas.width=64;p.canvas.height=64;p.canvas.background=Qt::green;p.canvas.backgroundImage=dir.filePath("背景.png");
        p.assets["A"]=p.assets[p.fallback]=dir.filePath("立绘.png");p.canvas.characterScale=.25;p.canvas.characterX=.75;p.canvas.characterY=.25;
        p.output.duration=3;p.audioDuration=4;p.playbackReturnPosition=.25;
        Scene scene(p);auto image=scene.frame(0);QCOMPARE(image.pixelColor(48,16),QColor(Qt::red));QCOMPARE(image.pixelColor(32,32),QColor(Qt::blue));QCOMPARE(image.pixelColor(4,0),QColor(Qt::blue));
        p.canvas.backgroundFit="contain";Scene contain(p);image=contain.frame(0);QCOMPARE(image.pixelColor(0,0),QColor(Qt::green));QCOMPARE(image.pixelColor(32,32),QColor(Qt::blue));
        p.canvas.backgroundFit="stretch";Scene stretch(p);QCOMPARE(stretch.frame(0).pixelColor(0,0),QColor(Qt::yellow));
        auto path=dir.filePath("画布.chosuta");saveProject(p,path);auto restored=loadProject(path);
        QCOMPARE(restored.canvas.backgroundImage,p.canvas.backgroundImage);QCOMPARE(restored.canvas.characterScale,.25);QCOMPARE(restored.canvas.characterX,.75);QCOMPARE(restored.canvas.backgroundFit,QString("stretch"));QCOMPARE(restored.duration(),3.);QCOMPARE(restored.audioDuration,4.);QCOMPARE(restored.playbackReturnPosition,.25);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();QCOMPARE(root["schema"].toInt(),4);
        auto legacy=root["output"].toObject();auto canvas=root["canvas"].toObject();for(auto key:QStringList{"width","height","background","transparent"})legacy[key]=canvas[key];
        root["schema"]=1;root["output"]=legacy;root.remove("canvas");root.remove("audioDuration");root.remove("playbackReturnPosition");
        QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();restored=loadProject(path);
        QCOMPARE(restored.canvas.width,64);QCOMPARE(restored.canvas.background,QColor(Qt::green));QCOMPARE(restored.canvas.characterScale,1.);QVERIFY(restored.canvas.backgroundImage.isEmpty());
        p.canvas.characterScale=0;QVERIFY_EXCEPTION_THROWN(validateCanvas(p.canvas),Failure);p.canvas.characterScale=.25;
        if(QStandardPaths::findExecutable("ffmpeg").isEmpty())QSKIP("FFmpeg absent");
        p.output.format="mov";p.output.duration=3;p.output.fpsNum=2;std::atomic_bool cancel=false;
        auto video=dir.filePath("canvas.mov");auto result=exportVideo(p,video,cancel);QVERIFY2(result.success,qPrintable(result.error));
        QProcess decode;decode.start("ffmpeg",{"-v","error","-i",video,"-f","rawvideo","-pix_fmt","rgba","pipe:1"});QVERIFY(decode.waitForFinished());
        auto rgba=decode.readAllStandardOutput();QCOMPARE(rgba.size(),64*64*4*6);auto expected=Scene(p).frame(0);QCOMPARE(rgba.left(64*64*4),QByteArray(reinterpret_cast<const char*>(expected.constBits()),expected.sizeInBytes()));
        p.canvas.backgroundImage=dir.filePath("missing.png");result=exportVideo(p,dir.filePath("invalid.mov"),cancel);QVERIFY(!result.success);QVERIFY(result.error.contains("Background"));
    }
    void edits() {
        QTemporaryDir tmp;
        auto p=basic();
        auto e=p.generated[0];
        e.shape="O";
        e.start=.1;
        p.edit(e,true,"beats");
        p.regenerate();
        QCOMPARE(p.effective()[0].shape,QString("O"));
        saveProject(p,tmp.filePath("工程.chosuta"));
        auto q=loadProject(tmp.filePath("工程.chosuta"));
        QCOMPARE(q.effective()[0].shape,QString("O"));
        QCOMPARE(q.score.raw,p.score.raw);
        q.score.time.tempos[0].bpm=60;
        QCOMPARE(q.effective()[0].start,.2);
        q.selected.clear();
        q.regenerate();
        QCOMPARE(q.orphanOverrides().size(),1);
        QVERIFY(q.effective().isEmpty());
        p=basic();
        p.split(p.generated[0],.25);
        QCOMPARE(p.effective().size(),5);
        auto events=p.effective();
        p.merge( {
            events[0],events[1]
        });
        QCOMPARE(p.effective().size(),4);
        saveProject(p,tmp.filePath("split.chosuta"));
        p=loadProject(tmp.filePath("split.chosuta"));
        QCOMPARE(p.effective().size(),4);
        p.selected.clear();
        p.regenerate();
        QVERIFY(p.effective().isEmpty());
        QVERIFY(!p.orphanOverrides().isEmpty());
    }
    void routing() {
        auto p=basic();
        auto t=p.score.tracks[0];
        t.id="harmony";
        for(auto&n:t.notes) {
            n.id="harmony/"+n.id;
            n.phonemes="u";
        }
        p.score.tracks.append(t);
        p.selected.append(t.id);
        p.priorities[t.id]=10;
        p.regenerate();
        QCOMPARE(shapeAt(p,p.effective(),0),QString("A"));
        p.priorities[t.id]=-1;
        p.regenerate();
        QCOMPARE(shapeAt(p,p.effective(),0),QString("U"));
        p.rules.special["rest"]= {
            "hold","closed",.1
        };
        QCOMPARE(shapeAt(p,p.effective(),2.25),QString("U"));
        // Primary rest/hold must not block harmony takeover.
        p=basic();
        t=p.score.tracks[0];
        t.id="harmony";
        t.notes= {
            t.notes[0]
        };
        t.notes[0].id="harmony-note";
        t.notes[0].onset=4*Blick;
        t.notes[0].phonemes="u";
        p.score.tracks.append(t);
        p.selected.append(t.id);
        p.rules.special["rest"]= {
            "hold","closed",.1
        };
        p.regenerate();
        QCOMPARE(shapeAt(p,p.effective(),2.1),QString("U"));
        p.rules.harmonyTakeover=false;
        p.regenerate();
        QCOMPARE(shapeAt(p,p.effective(),2.1),QString("E"));
    }
    void refiner() {
        auto p=basic();
        auto e=p.generated[0];
        TimingProposal v {
            e.id,e.source,"test","1","hash",.1,.6,.8
        };
        struct FakeRefiner:TimingRefiner {
            TimingProposal proposal;
            QVector<TimingProposal>propose(const TimingRequest&,const std::atomic_bool&cancel)const override {
                return cancel.load()?QVector<TimingProposal>{}:QVector<TimingProposal> {
                    proposal
                };
            }
        }
        provider;
        provider.proposal=v;
        TimingRequest request;
        request.events=p.generated;
        request.inputHash="hash";
        std::atomic_bool cancel=false;
        auto proposed=applyTimingProposals(p,provider.propose(request,cancel),"hash");
        QCOMPARE(proposed[0].start,.1);
        QCOMPARE(applyTimingProposals(p, {
            v
        },"stale")[0].start,0.);
        p.edit(e);
        QCOMPARE(applyTimingProposals(p, {
            v
        },"hash")[0].start,0.);
    }
    void eventResolution() {
        auto p=basic();
        Event e=p.generated[0];
        e.shape="O";
        e.start=.1;
        e.end=.7;
        p.edit(e);
        auto raw=p.effective(),resolved=resolveEvents(raw);
        QCOMPARE(shapeAt(p,resolved,.55,true),QString("O"));
        QCOMPARE(shapeAt(p,resolved,.7,true),QString("I"));
        for(int i=0;i<250;++i) {
            double t=i*.01;
            QCOMPARE(shapeAt(p,raw,t),shapeAt(p,resolved,t,true));
        }
        QVERIFY_EXCEPTION_THROWN(checkedJson("{invalid"),Failure);
        QTemporaryDir dir;
        auto path=dir.filePath("schema.chosuta");
        saveProject(p,path);
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        auto root=QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        root["schema"]=99;
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(root).toJson());
        f.close();
        QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);
    }
    void cancellationAndSpecials() {
        auto p=basic();
        auto e=p.generated[0];
        e.shape="O";
        p.edit(e,false);
        std::atomic_bool token=true;
        QVERIFY_EXCEPTION_THROWN(p.regenerate(&token),Failure);
        QCOMPARE(p.overrides.size(),1);
        QCOMPARE(p.effective()[0].shape,QString("O"));
        QVERIFY_EXCEPTION_THROWN(parseSvp(fixture(),{},&token),Failure);
        p=basic();
        p.rules.language="en";
        auto &notes=p.score.tracks[0].notes;
        notes[0].lyrics="hello";
        notes[1].lyrics="+";
        notes[2].lyrics="-";
        notes[3].lyrics="+";
        for(auto&n:notes)n.phonemes.clear();
        p.regenerate();
        QCOMPARE(shapeAt(p,p.effective(),.25),QString("E"));
        QCOMPARE(shapeAt(p,p.effective(),.8),QString("U"));
        QCOMPARE(shapeAt(p,p.effective(),1.2),QString("U"));
        QCOMPARE(shapeAt(p,p.effective(),1.8),QString("unknown"));
        p=basic();
        p.rules.readings[p.score.tracks[0].notes[0].id]="きょ";
        p.regenerate();
        QCOMPARE(shapeAt(p,p.effective(),.3),QString("O"));
        QVERIFY(!pronounce("月光下学习音乐","zh").unknown);
        p=basic();
        p.score.tracks[0].notes[0].phonemes="N";
        p.rules.special["nasal"]= {
            "timed","A",.1
        };
        p.regenerate();
        QCOMPARE(shapeAt(p,p.effective(),.05),QString("closed"));
        QCOMPARE(shapeAt(p,p.effective(),.2),QString("A"));
    }
    void audioAndFormats() {
        if(QStandardPaths::findExecutable("ffmpeg").isEmpty()||QStandardPaths::findExecutable("ffprobe").isEmpty())QSKIP("FFmpeg/ffprobe absent");
        QTemporaryDir dir;
        createDemoAssets(dir.path());
        auto p=basic();
        for(auto shape:QStringList {
            "A","I","U","E","O","closed","rest","breath","unknown"
        })p.assets[shape]=dir.filePath(shape+".png");
        p.canvas.width=32;
        p.canvas.height=32;
        p.output.duration=1;
        p.output.fpsNum=2;
        p.output.format="mov";
        p.canvas.transparent=true;
        std::atomic_bool cancel=false;
        auto path=dir.filePath("transparent.mov");
        auto result=exportVideo(p,path,cancel);
        QVERIFY2(result.success,qPrintable(result.error));
        QCOMPARE(result.frames,2);
        QProcess decode;
        decode.start("ffmpeg", {
            "-v","error","-i",path,"-f","rawvideo","-pix_fmt","rgba","pipe:1"
        });
        QVERIFY(decode.waitForFinished());
        auto rgba=decode.readAllStandardOutput();
        QCOMPARE(rgba.size(),32*32*4*2);
        QCOMPARE((uchar)rgba[3],0);
        QCOMPARE((uchar)rgba[(16*32+16)*4+3],255);
        Scene scene(p);
        auto frame=scene.frame(0);
        QCOMPARE(rgba.left(32*32*4),QByteArray(reinterpret_cast<const char*>(frame.constBits()),frame.sizeInBytes()));
        p.output.format="webm";
        p.canvas.transparent=false;
        p.output.fpsNum=30000;
        p.output.fpsDen=1001;
        path=dir.filePath("rational.webm");
        result=exportVideo(p,path,cancel);
        QVERIFY2(result.success,qPrintable(result.error));
        QCOMPARE(result.frames,30);
        QProcess probe;
        probe.start("ffprobe", {
            "-v","error","-count_frames","-select_streams","v:0","-show_entries","stream=r_frame_rate,nb_read_frames:frame=pts_time","-of","json",path
        });
        QVERIFY(probe.waitForFinished());
        auto root=QJsonDocument::fromJson(probe.readAllStandardOutput()).object();
        auto stream=root["streams"].toArray()[0].toObject();
        QCOMPARE(stream["r_frame_rate"].toString(),QString("30000/1001"));
        QCOMPARE(stream["nb_read_frames"].toString(),QString("30"));
        auto frames=root["frames"].toArray();
        QCOMPARE(frames.size(),30);
        for(int i=0;i<frames.size();++i)QVERIFY(std::abs(frames[i].toObject()["pts_time"].toString().toDouble()-double(i)*1001/30000)<.002);
        QProcess tone;
        QString audio=dir.filePath("音声.wav");
        tone.start("ffmpeg", {
            "-v","error","-f","lavfi","-i","sine=frequency=440:sample_rate=48000:duration=1","-y",audio
        });
        QVERIFY(tone.waitForFinished());
        QCOMPARE(tone.exitCode(),0);
        auto metadata=probeAudio(audio,"ffmpeg",cancel);QVERIFY2(metadata.error.isEmpty(),qPrintable(metadata.error));QVERIFY(std::abs(metadata.duration-1)<.001);
        auto bad=probeAudio(dir.filePath("absent.wav"),"ffmpeg",cancel);QVERIFY(!bad.error.isEmpty());
        std::atomic_bool cancelled=true;QVERIFY(probeAudio(audio,"ffmpeg",cancelled).cancelled);
        p.output.format="mp4";
        p.output.fpsNum=30;
        p.output.fpsDen=1;
        p.output.duration=2;
        p.output.audioOffset=.25;
        p.audioPath=audio;
        path=dir.filePath("audio-positive.mp4");
        result=exportVideo(p,path,cancel);
        QVERIFY2(result.success,qPrintable(result.error));
        auto samples=[&](const QString&file) {
            QProcess proc;
            proc.start("ffmpeg", {
                "-v","error","-i",file,"-t","2","-vn","-ac","1","-ar","48000","-f","s16le","pipe:1"
            });
            if(!proc.waitForFinished())return QByteArray{};
            return proc.readAllStandardOutput();
        };
        auto energy=[](const QByteArray&pcm,double time) {
            int index=int(time*48000);
            double sum=0;
            for(int i=index;i<index+2400;++i) {
                if(i*2+1>=pcm.size())return -1.;
                qint16 value=qint16(quint16(uchar(pcm[i*2]))|(quint16(uchar(pcm[i*2+1]))<<8));
                sum+=std::abs(int(value));
            }
            return sum/2400;
        };
        auto pcm=samples(path);
        QVERIFY(energy(pcm,.05)<20);
        QVERIFY(energy(pcm,.4)>1000);
        QVERIFY(energy(pcm,1.6)<20);
        p.output.audioOffset=-.5;
        path=dir.filePath("audio-negative.mp4");
        result=exportVideo(p,path,cancel);
        QVERIFY2(result.success,qPrintable(result.error));
        pcm=samples(path);
        QVERIFY(energy(pcm,.1)>1000);
        QVERIFY(energy(pcm,.8)<20);
    }
    void exportBoundary() {
        if(QStandardPaths::findExecutable("ffmpeg").isEmpty()||QStandardPaths::findExecutable("ffprobe").isEmpty())QSKIP("FFmpeg/ffprobe absent");
        QTemporaryDir tmp;
        auto p=basic();
        p.canvas.width=64;
        p.canvas.height=64;
        p.output.duration=2;
        p.output.fpsNum=30;
        QMap<QString,QColor>colors {
            {
                "A",Qt::red
            }, {
                "I",Qt::green
            }, {
                "U",Qt::blue
            }, {
                "E",Qt::yellow
            }, {
                "closed",Qt::black
            }
        };
        for(auto it=colors.begin();it!=colors.end();++it) {
            QImage img(64,64,QImage::Format_RGBA8888);
            img.fill(it.value());
            auto path=tmp.filePath(it.key()+" 空格.png");
            QVERIFY(img.save(path));
            p.assets[it.key()]=path;
        }
        auto path=tmp.filePath("视频 $(literal).mp4");
        std::atomic_bool cancel=false;
        auto r=exportVideo(p,path,cancel);
        QVERIFY2(r.success,qPrintable(r.error));
        QCOMPARE(r.frames,60);
        QProcess probe;
        probe.start("ffprobe", {
            "-v","error","-select_streams","v:0","-show_entries","stream=width,height,r_frame_rate,nb_frames,duration","-of","json",path
        });
        QVERIFY(probe.waitForFinished());
        auto stream=QJsonDocument::fromJson(probe.readAllStandardOutput()).object()["streams"].toArray()[0].toObject();
        QCOMPARE(stream["width"].toInt(),64);
        QCOMPARE(stream["height"].toInt(),64);
        QCOMPARE(stream["r_frame_rate"].toString(),QString("30/1"));
        QCOMPARE(stream["nb_frames"].toString(),QString("60"));
        QProcess decode;
        decode.start("ffmpeg", {
            "-v","error","-i",path,"-f","rawvideo","-pix_fmt","rgb24","pipe:1"
        });
        QVERIFY(decode.waitForFinished());
        auto pixels=decode.readAllStandardOutput();
        QCOMPARE(pixels.size(),64*64*3*60);
        auto color=[&](int frame) {
            int off=frame*64*64*3+(32*64+32)*3;
            return QColor((uchar)pixels[off],(uchar)pixels[off+1],(uchar)pixels[off+2]);
        };
        QVERIFY(color(14).red()>220);
        QVERIFY(color(15).green()>220);
        QVERIFY(color(30).blue()>220);
        QVERIFY(color(45).red()>220&&color(45).green()>220);
        QFile previous(path);
        QVERIFY(previous.open(QIODevice::ReadOnly));
        auto hash=QCryptographicHash::hash(previous.readAll(),QCryptographicHash::Sha256);
        previous.close();
        cancel=false;
        r=exportVideo(p,path,cancel,[&](int n) {
            if(n>5)cancel=true;
        },true);
        QVERIFY(r.cancelled);
        QVERIFY(previous.open(QIODevice::ReadOnly));
        QCOMPARE(QCryptographicHash::hash(previous.readAll(),QCryptographicHash::Sha256),hash);
        QVERIFY(QDir(tmp.path()).entryList( {
            ".chosuta-export-*"
        },QDir::Dirs|QDir::Hidden).isEmpty());
        p.canvas.width=63;
        QVERIFY_EXCEPTION_THROWN(validateExport(p),Failure);
    }
};
QTEST_MAIN(CoreTest)
#include "core_test.moc"
