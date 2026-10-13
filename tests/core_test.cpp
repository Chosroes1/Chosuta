// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include "core/model.h"
#include "core/performance.h"
#include "core/intervals.h"
#include "core/executable.h"
#include "render/render.h"
#include "render/audio.h"
#include "render/waveform.h"
#include <cmath>
#include <QPainter>
#include <QScopeGuard>
using namespace chosuta;
static MediaTools testMediaTools(const QString &path="ffmpeg"){return selectMediaTools(path,ToolSelection::Explicit);}
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
static Project appearanceProject(const QVector<int> &pitches,const QStringList &phones={}) {
    auto root=checkedJson(fixture()).object();auto track=root["tracks"].toArray()[0].toObject();
    auto group=track["mainGroup"].toObject();QJsonArray notes;
    for(int i=0;i<pitches.size();++i)notes.append(QJsonObject{{"uuid",QString("pose%1").arg(i)},{"onset",double(qint64(i)*Blick)},{"duration",double(Blick)},{"pitch",pitches[i]},{"lyrics","a"},{"phonemes",phones.value(i,"a")}});
    group["notes"]=notes;track["mainGroup"]=group;root["tracks"]=QJsonArray{track};
    Project p;p.score=parseSvp(QJsonDocument(root).toJson());p.selected={p.score.tracks[0].id};p.appearance.enabled=true;p.regenerate();return p;
}
static void writeTimingAudio(const QString &path){
    QByteArray pcm;QDataStream data(&pcm,QIODevice::WriteOnly);data.setByteOrder(QDataStream::LittleEndian);
    for(int i=0;i<32000;++i){double t=i/16000.;double local=std::fmod(t,.5);data<<qint16(local>=.05&&local<.45?(i%32<16?12000:-12000):0);}
    QFile f(path);if(!f.open(QIODevice::WriteOnly))throw Failure(f.errorString());QDataStream s(&f);s.setByteOrder(QDataStream::LittleEndian);
    s.writeRawData("RIFF",4);s<<quint32(36+pcm.size());s.writeRawData("WAVEfmt ",8);s<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(16000)<<quint32(32000)<<quint16(2)<<quint16(16);s.writeRawData("data",4);s<<quint32(pcm.size());s.writeRawData(pcm.constData(),pcm.size());
}
class CoreTest:public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    private slots:
    void initTestCase(){QVERIFY(settings.isValid());QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());}
    void structuredMessagesAndReasons() {
        const auto message=Message::format("Missing asset: %1 (%2)",{"名前 %2 <A>","detail"}).withDetail("external diagnostic");
        QCOMPARE(message.text(),QString("Missing asset: 名前 %2 <A> (detail)\nexternal diagnostic"));
        try{throw Failure(message);}catch(const std::exception &e){QCOMPARE(exceptionMessage(e),message);}
        auto p=appearanceProject({60,67,67,67});applyPerformanceDraft(p,generatePerformance(p));QVERIFY(!p.performance.generated.isEmpty());
        p.performance.generated[0].reasons={Message("A1: arrival supported by sounding notes"),Message::format("%1 degree %2",{"C %2", "3"})};
        const auto json=performanceJson(p);auto q=p;readPerformance(q,json);QCOMPARE(q.performance.generated[0].reasons,p.performance.generated[0].reasons);
        auto invalid=[&](const QJsonValue &value){auto bad=json;auto rows=bad["generated"].toArray();auto row=rows[0].toObject();row["reasons"]=value;rows[0]=row;bad["generated"]=rows;QVERIFY_EXCEPTION_THROWN(readPerformance(q,bad),Failure);};
        invalid("legacy English reason");invalid(QJsonArray{QJsonObject{{"key",""},{"arguments",QJsonArray{}}}});
        invalid(QJsonArray{QJsonObject{{"key","key"},{"arguments",QJsonArray{42}}}});
        invalid(QJsonArray{QJsonObject{{"key","key"},{"arguments",QJsonArray{QString(257,'x')}}}});
        QJsonArray many;for(int i=0;i<97;++i)many.append(QJsonObject{{"key","key"},{"arguments",QJsonArray{}}});invalid(many);
        QTemporaryDir dir;const auto path=dir.filePath("reasons.chosuta");saveProject(p,path);QCOMPARE(loadProject(path).performance.generated[0].reasons,p.performance.generated[0].reasons);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();QCOMPARE(root["schema"].toInt(),14);
        root["schema"]=13;QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);
    }
    void mediaToolResolution() {
        QTemporaryDir dir;QVERIFY(dir.isValid());
        const auto oldPath=qgetenv("PATH");auto restore=qScopeGuard([&]{qputenv("PATH",oldPath);setDefaultFfmpegExecutable({});});
#ifdef Q_OS_WIN
        const QString suffix=".exe";
#else
        const QString suffix;
#endif
        auto make=[&](const QString &relative){auto path=dir.filePath(relative+suffix);QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);if(!f.open(QIODevice::WriteOnly))return QString{};f.write("test");f.close();f.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);return path;};
        auto bundled=make("app/tools/ffmpeg/ffmpeg"),beside=make("app/ffmpeg"),system=make("path/ffmpeg");
        auto external=make("外部 tool/ffmpeg"),probe=make("外部 tool/ffprobe");
        qputenv("PATH",QFileInfo(system).absolutePath().toUtf8());
        QCOMPARE(resolveExecutable("ffmpeg",dir.filePath("app")),bundled);
        setDefaultFfmpegExecutable(external);QCOMPARE(resolveExecutable("ffmpeg",dir.filePath("app")),external);
        QCOMPARE(resolveExecutable(beside,dir.filePath("app")),beside);QCOMPARE(resolveFfprobe("ffmpeg"),probe);
        setDefaultFfmpegExecutable(dir.filePath("missing/ffmpeg"+suffix));
        QCOMPARE(resolveExecutable("ffmpeg",dir.filePath("app")),dir.filePath("missing/ffmpeg"+suffix));
        setDefaultFfmpegExecutable({});QVERIFY(QFile::remove(bundled));QCOMPARE(resolveExecutable("ffmpeg",dir.filePath("app")),beside);
        QVERIFY(QFile::remove(beside));QCOMPARE(resolveExecutable("ffmpeg",dir.filePath("app")),system);
        QCOMPARE(defaultFfmpegExecutable(),QString());
        qputenv("PATH",oldPath);
        const auto actual=QStandardPaths::findExecutable("ffmpeg");
        if(!actual.isEmpty()&&!QStandardPaths::findExecutable("ffprobe").isEmpty()){
            std::atomic_bool token=false;const auto result=inspectMediaTools(testMediaTools(actual),token);
            QVERIFY2(result.ready(),qPrintable(messageStrings(result.errors).join("; ")+result.missingEncoders.join(",")+result.missingFilters.join(",")));
            token=true;QVERIFY(inspectMediaTools(testMediaTools(actual),token).cancelled);
        }
        std::atomic_bool token=false;auto absent=inspectMediaTools(testMediaTools(dir.filePath("no-tool")),token);QVERIFY(!absent.ready());QVERIFY(!absent.errors.isEmpty());
    }

    void mediaToolTrust() {
        QTemporaryDir dir;const auto path=dir.filePath("project-tool");
        QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write("untrusted");f.close();
        QVERIFY(!mediaToolAdopted(path));QVERIFY_EXCEPTION_THROWN(selectMediaTools(path),std::runtime_error);
        QCOMPARE(selectMediaTools(path,ToolSelection::Explicit).ffmpeg,path);
        QVERIFY(!mediaToolAdopted(path));adoptMediaTool(path);QCOMPARE(selectMediaTools(path).ffmpeg,path);
        QVERIFY(f.open(QIODevice::Append));f.write("changed");f.close();QVERIFY(!mediaToolAdopted(path));
        QVERIFY_EXCEPTION_THROWN(selectMediaTools(path),std::runtime_error);
    }
    void strictProjectContract() {
        QTemporaryDir dir;const auto path=dir.filePath("strict.chosuta");auto p=basic();
        auto event=p.generated.front();p.edit(event);saveProject(p,path);
        QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));const auto original=checkedJson(f.readAll()).object();f.close();
        auto reject=[&](QJsonObject value){QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(value).toJson());f.close();QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);};
        for(const auto &key:{"generated","assets","overrides","selected"}){auto root=original;root[key]="wrong type";reject(root);}
        auto root=original;auto edits=root["overrides"].toArray();auto edit=edits[0].toObject();edit["locked"]="true";edits[0]=edit;root["overrides"]=edits;reject(root);
        p.output.duration=-1;QVERIFY_EXCEPTION_THROWN(saveProject(p,path),Failure);
        p.output.duration=0;saveProject(p,path);QCOMPARE(loadProject(path).overrides.size(),1);
    }

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
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.1).shape,QString("A"));
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.4).shape,QString("I"));
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.6).shape,QString("unknown"));
        QVERIFY(std::any_of(p.score.diagnostics.begin(),p.score.diagnostics.end(),[](const auto&d){return d.code=="language";}));
        QTemporaryDir dir;saveProject(p,dir.filePath("language.chosuta"));auto restored=loadProject(dir.filePath("language.chosuta"));
        QCOMPARE(restored.score.tracks[0].notes[0].language,QString("en"));
        restored.regenerate();QCOMPARE(AppearanceResolver(restored).at(restored,restored.effective(),.4).shape,QString("I"));
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
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.95).shape,QString("I"));
        for(const auto &event:p.generated)QVERIFY(event.shape!="open");
        // Each instance keeps its own word and extension state.
        notes[0].lyrics="hello";notes[0].groupInstance="first";
        notes[1].lyrics="-";notes[1].groupInstance="second";
        notes[2].lyrics="+";notes[2].groupInstance="first";
        notes[3].lyrics="+";notes[3].groupInstance="second";
        p.regenerate();
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.25).shape,QString("E"));
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.75).shape,QString("closed"));
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),1.4).shape,QString("U"));
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),1.75).shape,QString("unknown"));
        // Extending a syllable ending in a stop holds the vowel, not the stop.
        notes[0].phonemes="aa t";notes[1].groupInstance="first";notes[2].lyrics="cl";notes[3].lyrics="br";
        p.regenerate();QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.75).shape,QString("A"));
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
        QCOMPARE(golden.syllables[1].first(),QString("open"));
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
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.25).shape,QString("U"));
        p.rules.readings[note.id]="AA";p.regenerate();QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.25).shape,QString("A"));
        auto edit=p.generated[0];edit.shape="O";p.edit(edit,true);
        p.rules.pronunciation.words["en"]["hello"]={"IY",true};p.regenerate();QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.25).shape,QString("O"));
        // Save real embedded source: custom source modifications above are deliberately not serialized as the original fixture.
        p=basic();p.rules.pronunciation=options;
        QTemporaryDir dir;const auto path=dir.filePath("辞書.chosuta");saveProject(p,path);auto restored=loadProject(path);
        QCOMPARE(restored.rules.pronunciation,options);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();
        QCOMPARE(root["schema"].toInt(),14);
        auto rules=root["rules"].toObject();rules.remove("pronunciation");root["rules"]=rules;
        QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();
        QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);
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
    void routedSourceTime() {
        auto p=appearanceProject({60});auto high=p.score.tracks.front();high.notes.front().duration=4*Blick;
        auto low=high;low.id="low";low.notes.front().id="low-note";low.notes.front().duration=8*Blick;
        p.score.tracks={high,low};p.selected={high.id,low.id};p.regenerate();
        QCOMPARE(p.generated.size(),2);QCOMPARE(p.generated.last().start,2.);
        PerformanceResolver resolver(p,false);QCOMPARE(resolver.mappedBlick(4*Blick,low.id),2.);
        SourceTimeMap map(p);QCOMPARE(map.forward("low-note",2.).value(),2.);QCOMPARE(map.inverse("low-note",2.).value(),2.);
        PerformanceSegment segment;setPerformanceTime(p,segment,2.,4.,true);QCOMPARE(segment.startBlick,4*Blick);QCOMPARE(segment.endBlick,8*Blick);
        auto moved=p.generated.last();moved.start=2.1;moved.end=3.9;p.edit(moved);SourceTimeMap edited(p);
        QCOMPARE(edited.forward("low-note",2.).value(),2.1);QVERIFY(std::abs(edited.inverse("low-note",3.).value()-3.)<1e-9);
        p.output.syncOffset=.5;PerformanceResolver shifted(p,false);QCOMPARE(shifted.mappedBlick(4*Blick,low.id),2.6);
        setPerformanceTime(p,segment,2.6,4.4,true);QCOMPARE(segment.startBlick,4*Blick);QCOMPARE(segment.endBlick,8*Blick);
    }
    void editedSourceFragments() {
        auto p=appearanceProject({60});const auto original=p.generated.front();const auto source=original.source;
        p.split(original,.25);auto left=p.effective().front();const auto id=left.id;
        left.start=.05;left.end=.30;p.edit(left);
        SourceTimeMap moved(p);QVERIFY(std::abs(moved.forward(source,.125).value()-.175)<1e-9);QVERIFY(std::abs(moved.inverse(source,.175).value()-.125)<1e-9);
        // Resizing and a second split retain the first child's source interval.
        left.end=.45;p.edit(left);SourceTimeMap resized(p);
        QVERIFY(std::abs(resized.forward(source,.1).value()-.21)<1e-9);
        p.split(p.effective().front(),.20);SourceTimeMap split(p);
        QVERIFY(std::abs(split.forward(source,.05).value()-.13)<1e-9);
        QVERIFY(std::abs(split.forward(source,.1).value()-.21)<1e-9);
        // Moving outside the original parent's seconds is valid when provenance is known.
        auto child=p.effective().front();child.start=1.;child.end=1.15;p.edit(child);
        SourceTimeMap outside(p);QVERIFY(std::abs(outside.forward(source,.05).value()-1.08)<1e-9);
        PerformanceSegment selection;setPerformanceTime(p,selection,1.02,1.10,true);QCOMPARE(selection.anchor,QString("beats"));
        PerformanceResolver resolver(p,false);auto interval=resolver.interval(selection);
        QVERIFY(std::abs(interval.start-1.02)<1e-8);QVERIFY(std::abs(interval.end-1.10)<1e-8);
        QTemporaryDir dir;const auto file=dir.filePath("fragments.chosuta");saveProject(p,file);auto restored=loadProject(file);
        QCOMPARE(restored.overrides[child.id].sourceFragments,p.overrides[child.id].sourceFragments);
        QCOMPARE(SourceTimeMap(restored).forward(source,.05),outside.forward(source,.05));
        restored.regenerate();QCOMPARE(SourceTimeMap(restored).forward(source,.05),outside.forward(source,.05));
        QVERIFY(restored.overrides[id].deleted);QVERIFY(restored.overrides[child.id].locked);
        auto invalid=p;invalid.overrides[child.id].sourceFragments[0].end=2;QVERIFY_EXCEPTION_THROWN(saveProject(invalid,file),Failure);
        QCOMPARE(SourceTimeMap(loadProject(file)).forward(source,.05),outside.forward(source,.05));
        QFile saved(file);QVERIFY(saved.open(QIODevice::ReadOnly));auto json=checkedJson(saved.readAll()).object();saved.close();
        auto edits=json["overrides"].toArray();auto row=edits[0].toObject();row.remove("sourceFragments");edits[0]=row;json["overrides"]=edits;
        QVERIFY(saved.open(QIODevice::WriteOnly));saved.write(QJsonDocument(json).toJson());saved.close();QVERIFY_EXCEPTION_THROWN(loadProject(file),Failure);
        json["schema"]=12;QVERIFY(saved.open(QIODevice::WriteOnly));saved.write(QJsonDocument(json).toJson());saved.close();QVERIFY_EXCEPTION_THROWN(loadProject(file),Failure);
        // A split made after waveform correction captures corrected local positions,
        // while its normalized source ranges continue to refer to the raw generation.
        auto corrected=appearanceProject({60});corrected.audioPath=dir.filePath("audio.wav");writeTimingAudio(corrected.audioPath);
        corrected.timing.enabled=true;corrected.timing.basisHash=timingBasisHash(corrected);corrected.timing.audioHash=QString(64,'a');
        corrected.audioContentHash=corrected.timing.audioHash;corrected.timing.audioBytes=QFileInfo(corrected.audioPath).size();corrected.timing.audioModified=QFileInfo(corrected.audioPath).lastModified().toMSecsSinceEpoch();
        corrected.timing.sources[source]={0,.5,.05,.55};QVERIFY(timingCorrectionCurrent(corrected));
        corrected.split(corrected.effective().front(),.30);auto part=corrected.effective().front();part.start=.1;part.end=.35;corrected.edit(part);
        QVERIFY(std::abs(SourceTimeMap(corrected).forward(source,.125).value()-.225)<1e-9);
    }
    void mergedSourceFragments() {
        auto p=appearanceProject({60});const auto original=p.generated.front();p.split(original,.25);p.merge(p.effective());
        QCOMPARE(p.effective().front().source,original.source);SourceTimeMap map(p);QVERIFY(map.contains(original.source));QVERIFY(map.diagnostics.isEmpty());
        PerformanceSegment s;setPerformanceTime(p,s,0,.5,true);s.id="original";p.performance.generated={s};
        PerformanceResolver resolver(p);QVERIFY(!resolver.interval(s).orphan);QCOMPARE(resolver.slices().size(),1);
        for(double t:{.05,.2,.3,.45}){QVERIFY(std::abs(map.forward(original.source,t).value()-t)<1e-9);QVERIFY(std::abs(map.inverse(original.source,t).value()-t)<1e-9);}
        QTemporaryDir dir;auto file=dir.filePath("merged.chosuta");saveProject(p,file);auto reopened=loadProject(file);QCOMPARE(PerformanceResolver(reopened).slices().size(),1);
        // Preserve different piecewise stretches through merge and later split.
        auto q=appearanceProject({60});q.split(q.generated.front(),.25);auto pieces=q.effective();pieces[0].end=.2;q.edit(pieces[0]);pieces[1].start=.2;q.edit(pieces[1]);
        auto before=SourceTimeMap(q);q.merge(q.effective());auto after=SourceTimeMap(q);
        for(double t:{.05,.125,.3,.4})QVERIFY(std::abs(before.forward(original.source,t).value()-after.forward(original.source,t).value())<1e-9);
        q.split(q.effective().front(),.3);auto again=SourceTimeMap(q);for(double t:{.05,.125,.3,.4})QVERIFY(std::abs(before.forward(original.source,t).value()-again.forward(original.source,t).value())<1e-9);
        for(int i=0;i<24;++i){q.merge(q.effective());q.split(q.effective().front(),.1234567+i*.009);validateProject(q);}
        auto repeated=SourceTimeMap(q);for(double t:{.05,.125,.3,.4})QVERIFY(std::abs(before.forward(original.source,t).value()-repeated.forward(original.source,t).value())<1e-8);
        // Same note's different phoneme parents are also one true source.
        auto phones=appearanceProject({60},{"m a"});QVERIFY(phones.generated.size()>1);auto source=phones.generated.front().source;phones.merge(phones.effective());QVERIFY(SourceTimeMap(phones).contains(source));
        // Genuine cross-note merges remain explicit ambiguous intervals.
        auto mixed=appearanceProject({60,62});const auto first=mixed.generated.front().source;mixed.merge(mixed.effective());
        auto ambiguous=SourceTimeMap(mixed);QVERIFY(!ambiguous.diagnostics.isEmpty());QVERIFY(!ambiguous.forward(first,.1));
        PerformanceSegment seconds;setPerformanceTime(mixed,seconds,.1,.9,true);QCOMPARE(seconds.anchor,QString("seconds"));
    }
    void strictAccentTemplates() {
        auto settings=analysisSettingsJson(AnalysisSettings{});
        for(const auto &key:QStringList{"4/4","invalid"}){auto invalid=settings;invalid["strongBeats"]=QJsonObject{{key,QJsonArray{}}};QVERIFY_EXCEPTION_THROWN(readAnalysisSettings(invalid),Failure);}
        QJsonObject tooMany;for(int n=1;n<=65;++n)tooMany[QString::number(n)+"/4"]=QJsonArray{};settings["strongBeats"]=tooMany;QVERIFY_EXCEPTION_THROWN(readAnalysisSettings(settings),Failure);
        settings["strongBeats"]=QJsonObject{{"4/4",QJsonArray{1,3}}};QCOMPARE(analysisSettingsJson(readAnalysisSettings(settings)),settings);
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
        auto uncapped=sequence("P AA T",2);uncapped.rules.consonantMaxSeconds=0;uncapped.regenerate();QVERIFY(std::abs(uncapped.generated[0].end-.36)<1e-9);QVERIFY(std::abs(uncapped.generated.last().start-1.64)<1e-9);
        QTemporaryDir dir;auto path=dir.filePath("timing.chosuta");saveProject(uncapped,path);QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();auto rules=root["rules"].toObject();rules.remove("consonantMaxSeconds");root["rules"]=rules;
        auto write=[&]{if(!file.open(QIODevice::WriteOnly))return false;file.write(QJsonDocument(root).toJson());file.close();return true;};QVERIFY(write());QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);rules["consonantMaxSeconds"]=0;root["rules"]=rules;QVERIFY(write());auto restored=loadProject(path);QCOMPARE(restored.rules.consonantMaxSeconds,0.);QVERIFY(std::abs(restored.generated[0].end-.36)<1e-9);restored.regenerate();QVERIFY(std::abs(restored.generated[0].end-.36)<1e-9);
        auto locked=restored.generated[0];locked.shape="E";locked.start=.01;locked.end=.02;restored.edit(locked);restored.rules.consonantMaxSeconds=.08;restored.regenerate();QCOMPARE(AppearanceResolver(restored).at(restored,restored.effective(),.015).shape,QString("E"));QCOMPARE(restored.overrides[locked.id].event.end,.02);QVERIFY(std::abs(restored.generated[0].end-.08)<1e-9);saveProject(restored,path);QCOMPARE(loadProject(path).rules.consonantMaxSeconds,.08);
        rules["consonantMaxSeconds"]="80";root["rules"]=rules;QVERIFY(write());QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);rules["consonantMaxSeconds"]=1.1;root["rules"]=rules;QVERIFY(write());QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);
        restored.rules.consonantMaxSeconds=std::numeric_limits<double>::quiet_NaN();QVERIFY_EXCEPTION_THROWN(restored.regenerate(),Failure);QVERIFY_EXCEPTION_THROWN(saveProject(restored,path),Failure);
    }
    void consonantDefaults() {
        for(const auto &phone:QStringList{"B","P","M"})
            QCOMPARE(pronounce(phone+" AA","en",true).syllables[0][0],QString("closed"));
        for(const auto &phone:QStringList{"T","D","K","G","CH","JH","F","V","S","Z","SH","ZH","TH","DH","HH","R","L","W","Y","N","NG"}){
            QCOMPARE(pronounce(phone+" AA","en",true).syllables[0][0],QString("open"));
            auto p=basic();p.score.tracks[0].notes[0].language="en";p.score.tracks[0].notes[0].phoneset="arpabet";p.score.tracks[0].notes[0].phonemes=phone+" AA";p.regenerate();
            QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.02).shape,QString("A"));
            for(const auto&e:p.generated)QVERIFY(e.shape!="open");
        }
        for(const auto &lyric:QStringList{"ま","ぱ","ma","pa"})QCOMPARE(pronounce(lyric,"ja").syllables[0][0],QString("closed"));
        for(const auto &lyric:QStringList{"か","た","だ","さ","ら","は","ka","ta","da","sa","ra","ha"})QCOMPARE(pronounce(lyric,"ja").syllables[0][0],QString("open"));
        for(const auto &lyric:QStringList{"ba","pa","ma"})QCOMPARE(pronounce(lyric,"zh").syllables[0][0],QString("closed"));
        for(const auto &lyric:QStringList{"ka","da","ta","shi","hao","ri","li"})QCOMPARE(pronounce(lyric,"zh").syllables[0][0],QString("open"));
        auto p=basic();p.score.tracks[0].notes[0].phonemes="a s i";p.regenerate();
        const auto segments=p.generated;QVERIFY(segments.size()>4);QCOMPARE(segments[1].shape,QString("I"));
    }
    void configurableConsonants() {
        Rules::Consonants custom;custom.overrides={{"en:arpabet:D","closed"},{"ja:romaji:t","closed"},{"ja:romaji:ky","closed"},{"zh:xsampa:d","closed"},{"zh:xsampa:m","open"}};
        validateConsonants(custom);
        for(const auto &text:QStringList{"D AA","day"}){
            const auto r=pronounce(text,"en",text=="D AA",{},{},{},custom);
            QVERIFY(!r.unknown);QCOMPARE(r.syllables[0][0],QString("closed"));QCOMPARE(r.phones[0][0],QString("en:arpabet:D"));
        }
        for(const auto &text:QStringList{"た","タ","ta","t a"}){
            const auto r=pronounce(text,"ja",text=="t a",{},{},{},custom);
            QCOMPARE(r.syllables[0][0],QString("closed"));QCOMPARE(r.phones[0][0],QString("ja:romaji:t"));
        }
        for(const auto &text:QStringList{"きゃ","キャ","kya","ky a"}){
            const auto r=pronounce(text,"ja",text=="ky a",{},{},{},custom);
            QCOMPARE(r.syllables[0][0],QString("closed"));QCOMPARE(r.phones[0][0],QString("ja:romaji:ky"));
        }
        for(const auto &text:QStringList{"da","大","d a"})QCOMPARE(pronounce(text,"zh",text=="d a",{},{},{},custom).syllables[0][0],QString("closed"));
        QCOMPARE(pronounce("m","zh",false,{},{},{},custom).syllables[0][0],QString("open"));
        PronunciationOptions dictionary;dictionary.words["ja"]["試験"]={"た",false};
        const auto word=pronounce("試験","ja",false,{},{},dictionary,custom);QCOMPARE(word.phones[0][0],QString("ja:romaji:t"));QCOMPARE(word.syllables[0][0],QString("closed"));
        const auto cl=pronounce("っか","ja");QCOMPARE(cl.roles[0][0],SegmentRole::Special);QCOMPARE(cl.phones[0][0],QString("ja:romaji:cl"));
        auto p=appearanceProject({60,62,64},{"d a","a | d i","p a"});p.rules.consonants=custom;p.regenerate();
        QVERIFY(std::any_of(p.generated.cbegin(),p.generated.cend(),[](const Event &e){return e.phone=="ja:romaji:d"&&e.articulation=="consonant";}));
        QCOMPARE(readConsonants(consonantsJson(custom)),custom);
        auto invalid=custom;invalid.overrides["en:arpabet:D"]="half";QVERIFY_EXCEPTION_THROWN(validateConsonants(invalid),Failure);
        invalid=custom;invalid.overrides["xx:arpabet:D"]="closed";QVERIFY_EXCEPTION_THROWN(validateConsonants(invalid),Failure);
        auto json=consonantsJson(custom);json["preset"]="unknown";QVERIFY_EXCEPTION_THROWN(readConsonants(json),Failure);
    }
    void performanceLookupScale(){
        QVector<int> pitches;for(int i=0;i<2000;++i)pitches.append(i%2?67:60);auto p=appearanceProject(pitches);p.performance.groups.append(VariantGroup{"power","Power"});p.performance.roles["emphasis"]="power";
        p.performance.settings.music.emphasis=1;p.performance.settings.rules={{true,"high","emphasis",{},65,0,2},{true,"high","normal",{},0,0,1}};auto d=generatePerformance(p);QVERIFY(d.error.isEmpty());QCOMPARE(d.segments.size(),2000);applyPerformanceDraft(p,d);QElapsedTimer timer;timer.start();PerformanceResolver r(p);auto indexing=timer.nsecsElapsed();timer.restart();quint32 seed=7;
        for(int i=0;i<100000;++i){seed=1664525U*seed+1013904223U;int note=seed%2000;QCOMPARE(r.groupAt(note*.5+.25),note%2?QString("power"):QString("base"));}
        qInfo()<<"Performance lookup: 2000 segments,"<<indexing/1000000.<<"ms indexing,"<<timer.nsecsElapsed()/1000000.<<"ms for 100000 random queries";
    }
    void performanceCorrectionAndMelisma(){
        auto p=appearanceProject({60,67,67,67});auto draft=generatePerformance(p);applyPerformanceDraft(p,draft);PerformanceSegment manual;manual.id="first";setPerformanceTime(p,manual,0,.5,true);putPerformanceSegment(p,manual);const auto hash=performanceBasisHash(p);
        if(QFileInfo(resolveExecutable("ffmpeg")).isExecutable()){QTemporaryDir dir;p.audioPath=dir.filePath("timing.wav");writeTimingAudio(p.audioPath);std::atomic_bool cancel=false;auto wave=readWaveform(p.audioPath,testMediaTools("ffmpeg"),cancel);QVERIFY(wave.wave);p.audioContentHash=wave.wave->hash;p.timing=correctWaveform(p,*wave.wave,cancel).timing;QCOMPARE(p.timing.sources.size(),4);auto v=PerformanceResolver(p).interval(p.performance.manual[0]);QVERIFY(std::abs(v.start-.05)<1e-6);QVERIFY(std::abs(v.end-.45)<1e-6);QCOMPARE(performanceBasisHash(p),hash);setPerformanceTime(p,manual,v.start,v.end,true);QCOMPARE(manual.startBlick,qint64(0));QCOMPARE(manual.endBlick,Blick);p.timing.sources.clear();QCOMPARE(PerformanceResolver(p).interval(manual).start,0.);}
        auto q=appearanceProject({60,67,90,67},{"a","a","cl","a"});q.score.tracks[0].notes[2].duration=Blick/5;q.score.tracks[0].notes[3].onset=2*Blick+Blick/5;q.regenerate();auto d=generatePerformance(q);QVERIFY(d.error.isEmpty());int emphasis=0;for(auto s:d.segments)if(s.targets==QStringList{"emphasis"})++emphasis;QCOMPARE(emphasis,1);
        q.score.tracks[0].notes[2].phonemes="br";q.regenerate();d=generatePerformance(q);QCOMPARE(d.segments.last().targets,QStringList{"emphasis"});
        q=appearanceProject({60,67,67},{"a","a i",""});q.score.tracks[0].notes[2].phonemes="";q.score.tracks[0].notes[2].lyrics="+";q.regenerate();d=generatePerformance(q);QCOMPARE(d.segments.last().targets,QStringList{"emphasis"});
    }
    void performanceContinuity(){
        auto p=appearanceProject({60,67,67,66,67,67});p.performance.groups.append(VariantGroup{"power","Power"});p.performance.roles["emphasis"]="power";
        auto draft=generatePerformance(p);QVERIFY2(draft.error.isEmpty(),qPrintable(draft.error.text()));QCOMPARE(draft.segments.size(),2);applyPerformanceDraft(p,draft);PerformanceResolver r(p);QCOMPARE(r.groupAt(.1),QString("base"));for(double t:{.5,1.,1.5,2.,2.99})QCOMPARE(r.groupAt(t),QString("power"));QVERIFY(performanceCurrent(p));
        // Alternation can support one gesture, but may not flicker with every pitch.
        auto alternating=appearanceProject({60,67,60,67,60,67});auto alternation=generatePerformance(alternating);QVERIFY(alternation.error.isEmpty());QVERIFY(alternation.segments.size()<=3);
        auto narrow=generatePerformance(appearanceProject({60,62,60,62}));for(auto segment:narrow.segments)QCOMPARE(segment.targets,QStringList{"normal"});
        p.performance.boundaries={{p.score.tracks[0].notes[3].id,"section-break"}};draft=generatePerformance(p);QVERIFY(!draft.suggestions.isEmpty());applyPerformanceDraft(p,draft);QCOMPARE(PerformanceResolver(p).groupAt(1.6),QString("base"));
        auto a=generatePerformance(p),b=generatePerformance(p);QCOMPARE(a.segments,b.segments);std::atomic_bool cancel=true;auto c=generatePerformance(p,&cancel);QVERIFY(c.cancelled);QVERIFY(c.segments.isEmpty());
    }
    void performanceProgressionAndRelease(){
        auto p=appearanceProject({60,62,64,63,66});p.score.tracks[0].notes.last().duration=2*Blick;p.regenerate();p.performance.settings.music.window=3;
        auto d=generatePerformance(p);QVERIFY(d.error.isEmpty());QCOMPARE(d.segments.last().targets,QStringList{"emphasis"});
        QVector<int> pitches(20,70);pitches<<68<<66<<64<<62;auto q=appearanceProject(pitches);auto &notes=q.score.tracks[0].notes;notes[21].duration=2*Blick;notes[22].onset=23*Blick;notes[22].duration=2*Blick;notes[23].onset=25*Blick;notes[23].duration=2*Blick;q.regenerate();auto &settings=q.performance.settings;settings.leap=settings.progression=settings.motif=settings.section=false;
        d=generatePerformance(q);QVERIFY(d.error.isEmpty());bool release=false;for(auto segment:d.segments)if(segment.targets==QStringList{"relaxed"}){release=true;QVERIFY(std::any_of(segment.reasons.begin(),segment.reasons.end(),[](const auto &r){return r.key.startsWith("R1");}));QVERIFY(segment.startBlick>=25*Blick);}QVERIFY(release);
        settings.release=false;d=generatePerformance(q);for(auto segment:d.segments)QVERIFY(segment.targets!=QStringList{"relaxed"});
    }
    void performanceTonalityAndRules(){
        QCOMPARE(parseTonality("C maj").value(),qMakePair(QString("C"),QString("major")));QCOMPARE(parseTonality("D Dorian").value(),qMakePair(QString("D"),QString("dorian")));QVERIFY(!parseTonality("H major"));
        auto p=appearanceProject({62,64,65,67});p.performance.settings.leap=p.performance.settings.progression=p.performance.settings.motif=p.performance.settings.section=false;p.performance.settings.rules={{true,"degree","emphasis",{},1,0,10}};
        auto d=generatePerformance(p);for(auto s:d.segments)QCOMPARE(s.targets,QStringList{"normal"});
        p.performance.analysis.tonalityEnabled=true;p.performance.analysis.degreeTonic="D";p.performance.analysis.degreeMode="dorian";d=generatePerformance(p);QCOMPARE(d.segments[0].targets,QStringList{"emphasis"});
        p.performance.tonalities={{0,4*Blick,{}}};d=generatePerformance(p);for(auto s:d.segments)QCOMPARE(s.targets,QStringList{"normal"});
        p.performance.settings.rules={{true,"long","relaxed",{},1,0,2},{true,"high","emphasis",{},60,0,3}};d=generatePerformance(p);QCOMPARE(d.segments[0].targets,QStringList{"emphasis"});
        auto strict=appearanceProject({60,67,67});strict.performance.analysis.intervalUnit="diatonic";d=generatePerformance(strict);for(auto s:d.segments)QCOMPARE(s.targets,QStringList{"normal"});strict.performance.analysis.tonalityEnabled=true;d=generatePerformance(strict);QCOMPARE(d.segments.last().targets,QStringList{"emphasis"});
    }
    void performanceV2UnitsAndStability(){
        auto p=appearanceProject({60,62,64,63,67,67,67});auto baseline=generatePerformance(p);QVERIFY(baseline.error.isEmpty());QVERIFY(std::any_of(baseline.segments.begin(),baseline.segments.end(),[](auto s){return s.targets==QStringList{"emphasis"};}));
        auto roles=[](const PerformanceDraft &d){QVector<QPair<qint64,QString>> r;for(auto s:d.segments)r.append({s.startBlick,s.targets.front()});return r;};
        auto fast=p;fast.score.time.tempos={{0,220}};fast.score.time.validate();fast.regenerate();QCOMPARE(roles(generatePerformance(fast)),roles(baseline));
        auto transpose=p;for(auto &n:transpose.score.tracks[0].notes)n.pitch+=5;transpose.regenerate();QCOMPARE(roles(generatePerformance(transpose)),roles(baseline));
        auto hash=performanceBasisHash(p);p.performance.settings.seconds.support=100;p.performance.settings.seconds.lookahead=100;QCOMPARE(performanceBasisHash(p),hash);QCOMPARE(roles(generatePerformance(p)),roles(baseline));p.performance.settings.timeBasis=PerformanceTimeBasis::Seconds;QVERIFY(performanceBasisHash(p)!=hash);auto seconds=generatePerformance(p);QVERIFY(seconds.error.isEmpty());for(auto s:seconds.segments)QVERIFY(s.targets!=QStringList{"emphasis"});
        auto settings=performanceSettingsJson(p.performance.settings);QCOMPARE(readPerformanceSettings(settings),p.performance.settings);settings["timeBasis"]="both";QVERIFY_EXCEPTION_THROWN(readPerformanceSettings(settings),Failure);
        auto unknown=appearanceProject({60,67,67},{"m","?","m"});auto d=generatePerformance(unknown);QVERIFY(d.error.isEmpty());QVERIFY(!d.segments.isEmpty());QCOMPARE(d.segments.last().targets,QStringList{"emphasis"});
        auto tonic=appearanceProject({60,67,72});tonic.score.tracks[0].notes.last().duration=4*Blick;tonic.performance.analysis.tonalityEnabled=true;tonic.regenerate();d=generatePerformance(tonic);QCOMPARE(d.segments.last().targets,QStringList{"emphasis"});
        auto degree=appearanceProject({60,64,64});degree.performance.analysis.tonalityEnabled=true;degree.performance.analysis.intervalUnit="diatonic";degree.performance.settings.leapDegrees=4;degree.performance.settings.progression=degree.performance.settings.motif=degree.performance.settings.section=false;d=generatePerformance(degree);for(auto x:d.segments)QCOMPARE(x.targets,QStringList{"normal"});degree.performance.settings.leapDegrees=3;d=generatePerformance(degree);QCOMPARE(d.segments.last().targets,QStringList{"emphasis"});
        auto q=p;q.performance.settings=PerformanceSettings{};auto original=generatePerformance(q);applyPerformanceDraft(q,original);auto manual=q;PerformanceSegment m;m.id="protect";m.anchor="seconds";m.start=1;m.end=2;m.targets={"relaxed"};putPerformanceSegment(manual,m);applyPerformanceDraft(manual,generatePerformance(manual),.75,2.25);QCOMPARE(manual.performance.manual.size(),1);QCOMPARE(manual.performance.manual.front(),m);
    }
    void performanceV2MotifAndRoute(){
        QVector<int> pitches(24,60);pitches<<65<<68<<70<<77<<65<<68<<70<<77;auto p=appearanceProject(pitches);auto &notes=p.score.tracks[0].notes;for(int i=24;i<32;++i){notes[i].onset=24*Blick+(i-24)*Blick/4;notes[i].duration=Blick/4;}p.performance.settings.leap=p.performance.settings.progression=p.performance.settings.section=false;p.regenerate();auto d=generatePerformance(p);QVERIFY(d.error.isEmpty());QVERIFY(std::any_of(d.segments.begin(),d.segments.end(),[](auto s){return std::any_of(s.reasons.begin(),s.reasons.end(),[](const auto &r){return r.key.startsWith("A2");})&&s.targets==QStringList{"emphasis"};}));
        auto narrow=p;for(auto &n:narrow.score.tracks[0].notes)n.pitch=60;narrow.regenerate();auto nd=generatePerformance(narrow);for(auto s:nd.segments)QCOMPARE(s.targets,QStringList{"normal"});
        p.score.time.meters={{0,6,8}};p.score.time.validate();QCOMPARE(generatePerformance(p).segments,d.segments);
        auto control=appearanceProject({60,100,60},{"a","br","a"});auto cd=generatePerformance(control);for(auto s:cd.segments)QCOMPARE(s.targets,QStringList{"normal"});
        auto track=p.score.tracks.front();track.id="secondary";for(auto &n:track.notes){n.id="second-"+n.id;n.pitch=100;}p.score.tracks.append(track);p.selected.append(track.id);p.priorities[track.id]=99;p.regenerate();auto routed=generatePerformance(p);QCOMPARE(routed.segments,d.segments);
    }
    void performanceV2BoundariesAndPartial(){
        auto p=appearanceProject({60,67,67,66,64,62,60,60});p.performance.groups.append(VariantGroup{"power","Power"});p.performance.roles["emphasis"]="power";auto d=generatePerformance(p);QVERIFY(d.error.isEmpty());applyPerformanceDraft(p,d);auto full=PerformanceResolver(p);QCOMPARE(full.groupAt(.75),QString("power"));QCOMPARE(full.groupAt(3.75),QString("base"));
        auto partial=p;partial.performance.generated.clear();partial.performance.basisHash.clear();applyPerformanceDraft(partial,generatePerformance(partial),.75,3.75);auto cut=PerformanceResolver(partial);for(int i=75;i<375;++i)QCOMPARE(cut.groupAt(i*.01),full.groupAt(i*.01));QCOMPARE(cut.groupAt(.5),QString("base"));
        auto perturbed=p;perturbed.score.tracks[0].notes[1].onset-=Blick/480;perturbed.regenerate();auto perturbedDraft=generatePerformance(perturbed);QVERIFY(perturbedDraft.error.isEmpty());applyPerformanceDraft(perturbed,perturbedDraft);QCOMPARE(PerformanceResolver(perturbed).groupAt(.75),QString("power"));
        auto soft=p;soft.performance.boundaries={{soft.score.tracks[0].notes[2].id,"break"}};auto sd=generatePerformance(soft);applyPerformanceDraft(soft,sd);QCOMPARE(PerformanceResolver(soft).groupAt(1.25),QString("power"));soft.performance.boundaries[0].mode="section-break";sd=generatePerformance(soft);applyPerformanceDraft(soft,sd);QCOMPARE(PerformanceResolver(soft).groupAt(1.25),QString("base"));
        auto joined=appearanceProject({60,67,67});joined.performance.groups.append(VariantGroup{"power","Power"});joined.performance.roles["emphasis"]="power";joined.score.tracks[0].notes[2].onset=6*Blick;joined.regenerate();auto separated=generatePerformance(joined);QCOMPARE(separated.segments.last().targets,QStringList{"normal"});joined.performance.boundaries={{joined.score.tracks[0].notes[2].id,"section-join"}};auto connected=generatePerformance(joined);QVERIFY(connected.error.isEmpty());applyPerformanceDraft(joined,connected);QCOMPARE(PerformanceResolver(joined).groupAt(2.),QString("power"));QCOMPARE(connected.segments.last().targets,QStringList{"emphasis"});
        auto duration=p;duration.performance.settings=PerformanceSettings{};duration.performance.settings.leap=duration.performance.settings.progression=duration.performance.settings.motif=duration.performance.settings.section=false;PerformanceRule rule;rule.condition="long";rule.target="emphasis";rule.threshold=2;rule.thresholdSeconds=.4;rule.minBeats=0;duration.performance.settings.rules={rule};auto musical=generatePerformance(duration);for(auto s:musical.segments)QCOMPARE(s.targets,QStringList{"normal"});duration.performance.settings.timeBasis=PerformanceTimeBasis::Seconds;auto timed=generatePerformance(duration);QCOMPARE(timed.segments.front().targets,QStringList{"emphasis"});
        QVector<int> settling(20,60);settling<<60<<68<<67<<65;auto highLanding=appearanceProject(settling);highLanding.score.tracks[0].notes.last().duration=4*Blick;highLanding.regenerate();auto landing=generatePerformance(highLanding);QVERIFY(landing.error.isEmpty());QCOMPARE(landing.segments.last().targets,QStringList{"emphasis"});
        auto json=performanceSettingsJson(duration.performance.settings);auto profile=json["music"].toObject();profile.remove("support");json["music"]=profile;QVERIFY_EXCEPTION_THROWN(readPerformanceSettings(json),Failure);
    }
    void performanceManualLayers(){
        auto p=appearanceProject({60,67,67,67});p.performance.groups.append(VariantGroup{"power","Power"});p.performance.roles["emphasis"]="power";auto d=generatePerformance(p);applyPerformanceDraft(p,d);
        PerformanceSegment m;m.id="manual";m.anchor="seconds";m.start=0;m.end=2;m.mode="arch";m.targets={"normal","emphasis","normal"};m.cuts={.35,.7};putPerformanceSegment(p,m);
        auto before=PerformanceResolver(p);QCOMPARE(before.groupAt(.8),QString("power"));splitPerformance(p,"manual",1);QCOMPARE(p.performance.manual.size(),2);auto split=PerformanceResolver(p);for(int i=0;i<200;++i)QCOMPARE(split.groupAt(i*.01),before.groupAt(i*.01));
        applyPerformanceDraft(p,d);QCOMPARE(p.performance.manual.size(),2);p.performance.manual[0].protect=false;applyPerformanceDraft(p,d);QCOMPARE(p.performance.manual.size(),1);
        erasePerformanceRange(p,1.25,1.75,true);QCOMPARE(p.performance.manual.size(),2);QCOMPARE(PerformanceResolver(p).groupAt(1.5),QString("power"));
        PerformanceSegment replacement;m={};replacement.id="new";replacement.anchor="seconds";replacement.start=1.1;replacement.end=1.9;replacement.targets={"group:base"};putPerformanceSegment(p,replacement);auto r=PerformanceResolver(p);QCOMPARE(r.groupAt(1.5),QString("base"));QCOMPARE(p.performance.manual.size(),3);
        const auto mouth=p.generated;const auto timing=timingCorrectionJson(p);p.regenerate();QCOMPARE(p.generated.size(),mouth.size());for(int i=0;i<mouth.size();++i){QCOMPARE(p.generated[i].id,mouth[i].id);QCOMPARE(p.generated[i].shape,mouth[i].shape);QCOMPARE(p.generated[i].start,mouth[i].start);QCOMPARE(p.generated[i].end,mouth[i].end);}QCOMPARE(timingCorrectionJson(p),timing);QCOMPARE(p.performance.manual.size(),3);
    }
    void performancePartialRegenerationAndPersistence(){
        auto p=appearanceProject({60,67,67,67});auto d=generatePerformance(p);applyPerformanceDraft(p,d);auto initial=p.performance.generated;
        PerformanceSegment m;m.id="whole";m.anchor="seconds";m.start=0;m.end=2;m.protect=false;putPerformanceSegment(p,m);applyPerformanceDraft(p,d,.5,1.5);QCOMPARE(p.performance.manual.size(),2);auto r=PerformanceResolver(p);QVERIFY(r.at(.1)->manual);QVERIFY(!r.at(1)->manual);QVERIFY(r.at(1.9)->manual);
        auto json=performanceJson(p);auto q=p;readPerformance(q,json);QCOMPARE(performanceJson(q),json);
        auto bad=json;bad.remove("groups");QVERIFY_EXCEPTION_THROWN(readPerformance(q,bad),Failure);auto settings=performanceSettingsJson(p.performance.settings);settings["leapDegrees"]=1e200;QVERIFY_EXCEPTION_THROWN(readPerformanceSettings(settings),Failure);
        QTemporaryDir dir;auto file=dir.filePath("performance.chosuta");saveProject(p,file);q=loadProject(file);QCOMPARE(performanceJson(q),json);QFile f(file);QVERIFY(f.open(QIODevice::ReadOnly));auto root=checkedJson(f.readAll()).object();f.close();QCOMPARE(root["schema"].toInt(),14);root["schema"]=9;QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(root).toJson());f.close();QVERIFY_EXCEPTION_THROWN(loadProject(file),Failure);
        auto hash=performanceBasisHash(p);p.output.syncOffset=.2;p.output.audioOffset=2;p.assets["A"]="image.png";QCOMPARE(performanceBasisHash(p),hash);p.performance.settings.leap=false;QVERIFY(performanceBasisHash(p)!=hash);
    }
    void performanceAnchorsAndOrphans(){
        auto p=appearanceProject({60,67,67});p.output.syncOffset=.25;p.output.audioOffset=10;PerformanceSegment m;m.id="anchored";setPerformanceTime(p,m,.75,1.75,true);QCOMPARE(m.startBlick,Blick);QCOMPARE(m.endBlick,3*Blick);putPerformanceSegment(p,m);QCOMPARE(PerformanceResolver(p).interval(m).start,.75);
        p.score.time.tempos={{0,120},{Blick,60}};p.score.time.validate();p.regenerate();QCOMPARE(PerformanceResolver(p).interval(m).end,2.75);
        p.score.tracks[0].notes.removeLast();p.regenerate();auto r=PerformanceResolver(p);QVERIFY(r.interval(m).orphan);QCOMPARE(r.interval(m).end,1.75);QVERIFY(!r.diagnostics.isEmpty());
        setPerformanceTime(p,m,3,4,false);p.output.syncOffset=7;QCOMPARE(PerformanceResolver(p).interval(m).start,3.);
    }
    void groupReferencesAndImportTransactions(){
        auto p=basic();p.performance.groups.append(VariantGroup{"old","Same"});p.performance.groups.append(VariantGroup{"new","Same"});
        p.generated[0].shape="old/A";p.generated[0].appearanceFixed=true;p.edit(p.generated[0]);
        auto orphan=p.generated[0];orphan.id="orphan";orphan.shape="old/I";p.edit(orphan);
        p.fallback="old/rest";p.rules.special["br"].shape="old/breath";p.assets["old/A"]="old.png";
        const auto basis=timingBasisHash(p);removeVariantGroup(p,"old","new");
        QCOMPARE(p.generated[0].shape,QString("new/A"));QVERIFY(p.generated[0].appearanceFixed);
        QCOMPARE(p.overrides[orphan.id].event.shape,QString("new/I"));QVERIFY(p.overrides[orphan.id].locked);
        QCOMPARE(p.fallback,QString("new/rest"));QCOMPARE(p.rules.special["br"].shape,QString("new/breath"));QCOMPARE(timingBasisHash(p),basis);
        QTemporaryDir directory;QDir root(directory.path());QVERIFY(root.mkdir("extra"));
        QImage png(16,16,QImage::Format_RGBA8888);png.fill(Qt::red);QVERIFY(png.save(root.filePath("extra/unknown.png")));
        auto q=basic();auto plan=scanVariantDirectory(directory.path(),q,"base");QCOMPARE(plan.groups.size(),1);QCOMPARE(plan.entries.size(),1);
        q.performance.groups+=plan.groups;applyAssetImport(q,plan.entries);const auto group=plan.groups[0].id;
        q.assets["unknown"]=plan.entries[0].path;q.assets[group+"/hat"]=plan.entries[0].path;
        q.generated[0].shape=group+"/unknown";q.generated[0].appearanceFixed=true;q.edit(q.generated[0]);q.fallback=group+"/unknown";q.rules.special["empty"].shape=q.fallback;
        removeVariantGroup(q,group,"base");QCOMPARE(q.fallback,QString("unknown"));QCOMPARE(q.overrides.first().event.shape,QString("unknown"));QCOMPARE(q.rules.special["empty"].shape,QString("unknown"));
        QVERIFY(q.assets.contains(group+"/hat"));QVERIFY(!q.assets.contains(group+"/unknown"));QVERIFY(!Scene(q).selectionAt(.1).missing);QVERIFY(QFileInfo::exists(plan.entries[0].path));
        auto saved=root.filePath("group.chosuta");saveProject(q,saved);auto loaded=loadProject(saved);QCOMPARE(loaded.fallback,QString("unknown"));QVERIFY(loaded.assets.contains(group+"/hat"));
        QVector<AssetImportEntry> rows={{"first.png","A","new",{},{}},{"second.png","A","new",{},{}}};
        auto assets=p.assets;QVERIFY_EXCEPTION_THROWN(applyAssetImport(p,rows),Failure);QCOMPARE(p.assets,assets);
        rows.removeLast();applyAssetImport(p,rows);QVERIFY_EXCEPTION_THROWN(applyAssetImport(p,rows),Failure);applyAssetImport(p,rows,{"A"});
    }
    void exportPreflight(){
        if(!QFileInfo(resolveExecutable("ffmpeg")).isExecutable())QSKIP("FFmpeg absent");
        QTemporaryDir dir;auto p=basic();p.canvas.width=p.canvas.height=32;p.output.duration=.1;p.output.fpsNum=10;p.output.format="mov";
        QImage red(32,32,QImage::Format_RGBA8888);red.fill(Qt::red);const auto image=dir.filePath("A.png");QVERIFY(red.save(image));p.assets["A"]=image;
        std::atomic_bool cancel=false;auto result=exportVideo(p,testMediaTools(),dir.filePath("no-fallback.mov"),cancel);QVERIFY2(result.success,qPrintable(result.error.text()));
        p.output.duration=2.2;QVERIFY(!Scene(p).missingAppearanceAssets().isEmpty());p.output.duration=.1;
        p.audioPath=dir.filePath("audio.wav");writeTimingAudio(p.audioPath);auto wave=readWaveform(p.audioPath,testMediaTools(),cancel);QVERIFY(wave.wave);p.timing=correctWaveform(p,*wave.wave,cancel).timing;
        const auto before=p.timing.audioHash;const auto stamp=QFileInfo(p.audioPath).lastModified();QFile audio(p.audioPath);QVERIFY(audio.open(QIODevice::ReadWrite));QVERIFY(audio.seek(audio.size()-1));QVERIFY(audio.write("x",1)==1);QVERIFY(audio.setFileTime(stamp,QFileDevice::FileModificationTime));audio.close();
        result=exportVideo(p,testMediaTools(),dir.filePath("rechecked.mov"),cancel);QVERIFY2(result.success,qPrintable(result.error.text()));QVERIFY(result.audioContentHash!=before);QVERIFY(messageStrings(result.diagnostics).join(" ").contains("stale"));
        QProcess decoder;decoder.start(resolveExecutable("ffmpeg"),{"-v","error","-i",dir.filePath("rechecked.mov"),"-frames:v","1","-f","rawvideo","-pix_fmt","rgba","-"});QVERIFY(decoder.waitForFinished(10000));QCOMPARE(decoder.exitCode(),0);const auto pixels=decoder.readAllStandardOutput();QCOMPARE(pixels.size(),32*32*4);QCOMPARE(uchar(pixels[0]),uchar(255));QCOMPARE(uchar(pixels[1]),uchar(0));
    }
    void variantFallbackAndIndependentRendering(){
        QTemporaryDir dir;auto p=appearanceProject({60,60});p.canvas.width=p.canvas.height=32;p.output.duration=1;p.output.fpsNum=10;p.output.format="mov";
        p.performance.groups.append(VariantGroup{"power","Power"});p.performance.roles["emphasis"]="power";
        QImage image(32,32,QImage::Format_RGBA8888);image.fill(Qt::red);auto red=dir.filePath("A.png");QVERIFY(image.save(red));image.fill(Qt::blue);auto blue=dir.filePath("blue.png");QVERIFY(image.save(blue));p.assets={{"A",red},{"power/A",blue}};p.fallback="A";
        PerformanceSegment m;m.id="body";m.anchor="seconds";m.start=.1;m.end=.4;m.targets={"emphasis"};putPerformanceSegment(p,m);Scene scene(p);
        QCOMPARE(scene.frame(.05).pixelColor(16,16),QColor(Qt::red));QCOMPARE(scene.frame(.2).pixelColor(16,16),QColor(Qt::blue));QCOMPARE(scene.frame(.4).pixelColor(16,16),QColor(Qt::red));QCOMPARE(scene.frame(.2),scene.frame(.2));QVERIFY(scene.missingAppearanceAssets().isEmpty());
        // A mouth edit keeps following the body; explicitly fixed images bypass it.
        auto e=p.generated[0];e.shape="I";p.edit(e);QVERIFY(!p.effective()[0].appearanceFixed);p.assets["I"]=red;p.assets["power/I"]=blue;QCOMPARE(Scene(p).frame(.2).pixelColor(16,16),QColor(Qt::blue));e.appearanceFixed=true;p.edit(e);QCOMPARE(Scene(p).frame(.2).pixelColor(16,16),QColor(Qt::red));
        p.overrides.clear();p.assets.remove("power/A");QCOMPARE(Scene(p).frame(.2).pixelColor(16,16),QColor(Qt::red));p.assets["power/A"]=blue;
        p.appearance.enabled=false;QCOMPARE(Scene(p).frame(.2).pixelColor(16,16),QColor(Qt::red));p.appearance.enabled=true;
        auto movieProject=p;
        removeVariantGroup(p,"power","base");QCOMPARE(p.performance.roles["emphasis"],QString("base"));QVERIFY(!p.assets.contains("power/A"));QVERIFY_EXCEPTION_THROWN(removeVariantGroup(p,"base","base"),Failure);
        if(!QFileInfo(resolveExecutable(p.output.ffmpeg)).isExecutable())QSKIP("FFmpeg absent");std::atomic_bool cancel=false;auto movie=dir.filePath("performance.mov");auto result=exportVideo(movieProject,testMediaTools(movieProject.output.ffmpeg),movie,cancel);QVERIFY2(result.success,qPrintable(result.error.text()));QCOMPARE(result.frames,10);
        QProcess decoder;decoder.start(resolveExecutable("ffmpeg"),{"-v","error","-i",movie,"-f","rawvideo","-pix_fmt","rgba","-"});QVERIFY(decoder.waitForFinished(10000));QCOMPARE(decoder.exitCode(),0);auto pixels=decoder.readAllStandardOutput();QCOMPARE(pixels.size(),32*32*4*10);auto color=[&](int frame){auto offset=frame*32*32*4+(16*32+16)*4;return QColor(uchar(pixels[offset]),uchar(pixels[offset+1]),uchar(pixels[offset+2]));};QCOMPARE(color(0),QColor(Qt::red));QCOMPARE(color(2),QColor(Qt::blue));QCOMPARE(color(4),QColor(Qt::red));
    }
    void variantFolderAndCache(){
        QTemporaryDir dir;QDir root(dir.path());QVERIFY(root.mkpath("Power"));QVERIFY(root.mkpath("Soft"));QImage image(512,512,QImage::Format_RGBA8888);image.fill(Qt::cyan);QVERIFY(image.save(root.filePath("Power/A.png")));QVERIFY(image.save(root.filePath("Power/closed.png")));QVERIFY(image.save(root.filePath("Soft/I.png")));QFile bad(root.filePath("Soft/E.png"));QVERIFY(bad.open(QIODevice::WriteOnly));bad.write("not image");bad.close();
        auto p=appearanceProject({60,60});auto plan=scanVariantDirectory(root.path(),p,"base");QVERIFY(plan.error.isEmpty());QCOMPARE(plan.groups.size(),2);QCOMPARE(plan.entries.size(),4);int valid=0;for(auto e:plan.entries)if(e.status=="new")++valid;QCOMPARE(valid,3);
        std::atomic_bool cancelled=true;auto abort=scanVariantDirectory(root.path(),p,"base",&cancelled);QVERIFY(abort.cancelled);QVERIFY(abort.entries.isEmpty());
        p.canvas.width=p.canvas.height=32;p.assets["A"]=root.filePath("Power/A.png");p.performance.groups.append(VariantGroup{"power","Power"});p.assets["power/A"]=p.assets["A"];PerformanceSegment m;m.id="manual";m.anchor="seconds";m.start=0;m.end=1;m.targets={"group:power"};putPerformanceSegment(p,m);Scene scene(p);scene.frame(.2);QCOMPARE(scene.imageDecodeCount(),1);QVERIFY(scene.imageCacheBytes()<=32*32*4);
        auto arbitrary=root.filePath("任意画像.png");QVERIFY(image.save(arbitrary));auto invalid=root.filePath("壊れた画像.png");QFile broken(invalid);QVERIFY(broken.open(QIODevice::WriteOnly));broken.write("bad");broken.close();
        auto files=scanAssetFiles({arbitrary,arbitrary,invalid},{});QCOMPARE(files.entries.size(),2);QCOMPARE(files.entries[0].status,QString("unknown"));QCOMPARE(files.entries[1].status,QString("invalid"));
        auto duplicate=root.filePath("Power/a.png");QVERIFY(image.save(duplicate));auto conflict=scanAssetFiles({root.filePath("Power/A.png"),duplicate},{});QCOMPARE(conflict.entries[0].status,QString("conflict"));QCOMPARE(conflict.entries[1].status,QString("conflict"));
        auto aborted=scanAssetFiles({arbitrary},{},&cancelled);QVERIFY(aborted.cancelled);QVERIFY(aborted.entries.isEmpty());
        QVERIFY(!normalizedAssetId("A_strong_up_step"));QCOMPARE(normalizedAssetId("a").value(),QString("A"));
    }
    void diatonicClassification(){
        auto augmented=appearanceProject({68,71});
        augmented.performance.analysis.intervalUnit="diatonic";augmented.performance.analysis.degreeMode="harmonic-minor";const auto &notes=augmented.score.tracks[0].notes;
        QCOMPARE(writtenPitch(augmented.performance.analysis,notes[0])->name,QString("Ab4"));QCOMPARE(writtenPitch(augmented.performance.analysis,notes[1])->name,QString("B4"));QCOMPARE((std::abs(writtenPitch(augmented.performance.analysis,notes[1])->staffPosition-writtenPitch(augmented.performance.analysis,notes[0])->staffPosition)+1),2);
        auto enharmonic=appearanceProject({61,63});enharmonic.performance.analysis.intervalUnit="diatonic";const auto &pair=enharmonic.score.tracks[0].notes;
        enharmonic.performance.analysis.noteSpellings[pair[0].id]="C#4";enharmonic.performance.analysis.noteSpellings[pair[1].id]="D#4";QCOMPARE((std::abs(writtenPitch(enharmonic.performance.analysis,pair[1])->staffPosition-writtenPitch(enharmonic.performance.analysis,pair[0])->staffPosition)+1),2);
        enharmonic.performance.analysis.noteSpellings[pair[1].id]="Eb4";QCOMPARE((std::abs(writtenPitch(enharmonic.performance.analysis,pair[1])->staffPosition-writtenPitch(enharmonic.performance.analysis,pair[0])->staffPosition)+1),3);
        auto chromatic=appearanceProject({60,61,62});chromatic.performance.analysis.intervalUnit="diatonic";const auto &chromaticNotes=chromatic.score.tracks[0].notes;
        chromatic.performance.analysis.noteSpellings[chromaticNotes[1].id]="C#4";
        chromatic.performance.analysis.noteSpellings[chromaticNotes[1].id]="C5";QVERIFY(!writtenPitch(chromatic.performance.analysis,chromaticNotes[1]));
        for(const auto &tonic:QStringList{"C","C#","Db","D","Eb","E","F#","Gb","G","Ab","A","Bb","B","B#","Cb","E#","Fb"})for(auto it=degreeModes().cbegin();it!=degreeModes().cend();++it){
            AnalysisSettings a;a.degreeTonic=tonic;a.degreeMode=it.key();const int base=parseWrittenPitch(tonic+"4")->midi;Note first;first.pitch=base;first.id="root";
            for(int i=0;i<7;++i){Note n;n.pitch=base+it.value()[i];n.id=QString("degree%1").arg(i);QVERIFY(writtenPitch(a,n));QCOMPARE((std::abs(writtenPitch(a,n)->staffPosition-writtenPitch(a,first)->staffPosition)+1),i+1);}
            Note octave;octave.pitch=base+12;octave.id="octave";QCOMPARE((std::abs(writtenPitch(a,octave)->staffPosition-writtenPitch(a,first)->staffPosition)+1),8);
        }
        AnalysisSettings custom;custom.degreeMode="custom";custom.customDegreeScale={0,1,3,5,7,8,11};validateDegreeSettings(custom);Note n;n.pitch=61;n.id="custom";QCOMPARE(writtenPitch(custom,n)->name,QString("Db4"));
        custom.degreeTonic="Cb";custom.customDegreeScale={0,1,2,3,4,5,6};n.pitch=65;const auto unusual=writtenPitch(custom,n);QVERIFY(unusual);QCOMPARE(parseWrittenPitch(unusual->name)->midi,n.pitch);
        custom.customDegreeScale={0,2,2,5,7,9,11};QVERIFY_EXCEPTION_THROWN(validateDegreeSettings(custom),Failure);custom=AnalysisSettings{};custom.degreeMode="unknown";QVERIFY_EXCEPTION_THROWN(validateDegreeSettings(custom),Failure);
        QVERIFY(parseWrittenPitch(QString::fromUtf8("c♯4")));QCOMPARE(parseWrittenPitch("B#3")->midi,60);QCOMPARE(parseWrittenPitch("Cb4")->midi,59);QVERIFY(!parseWrittenPitch("C#b4"));QVERIFY(!parseWrittenPitch("H4"));QVERIFY(!parseWrittenPitch("C20"));
        const auto basis=timingBasisHash(augmented);augmented.performance.analysis.degreeTonic="D";augmented.performance.analysis.intervalUnit="semitones";QCOMPARE(timingBasisHash(augmented),basis);
        auto json=analysisSettingsJson(enharmonic.performance.analysis);QCOMPARE(readAnalysisSettings(json),enharmonic.performance.analysis);json["degreeTonic"]="H";QVERIFY_EXCEPTION_THROWN(readAnalysisSettings(json),Failure);
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
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();QCOMPARE(root["schema"].toInt(),14);
        root.remove("subtitles");QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);
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
        q=p;q.subtitles[0].style.family="Chosuta absent font 6a71";QVERIFY(messageStrings(Scene(q).diagnostics).join('\n').contains("font substituted"));
        q=p;q.subtitles[0].style.color=QColor(0,255,0,0);q.subtitles[0].style.outline=true;q.subtitles[1].enabled=false;QCOMPARE(Scene(q).frame(.25),empty);
        q=p;q.output.duration=.5;QVERIFY(messageStrings(Scene(q).diagnostics).join('\n').contains("fixed animation duration"));
        q=p;auto collision=q.subtitles[0].cues[0];collision.id="overlapping";collision.text="Overlap retained";q.subtitles[0].cues.append(collision);Scene overlapping(q);QVERIFY(messageStrings(overlapping.diagnostics).join('\n').contains("Subtitle overlap"));QVERIFY(overlapping.frame(.25)!=Scene(p).frame(.25));
        QTemporaryDir dir;QImage transparent(32,32,QImage::Format_RGBA8888);transparent.fill(Qt::transparent);auto asset=dir.filePath("closed.png");QVERIFY(transparent.save(asset));p.assets[p.fallback]=asset;
        if(QStandardPaths::findExecutable("ffmpeg").isEmpty())QSKIP("FFmpeg absent");
        std::atomic_bool cancel=false;auto movie=dir.filePath("original-subtitle.mov");auto result=exportVideo(p,testMediaTools(p.output.ffmpeg),movie,cancel);QVERIFY2(result.success,qPrintable(result.error.text()));QCOMPARE(result.frames,4);
        QProcess decode;decode.start("ffmpeg",{"-v","error","-i",movie,"-f","rawvideo","-pix_fmt","rgba","pipe:1"});QVERIFY(decode.waitForFinished());QCOMPARE(decode.exitCode(),0);auto rgba=decode.readAllStandardOutput();QCOMPARE(rgba.size(),320*180*4*4);
        Scene expected(p);for(int i=0;i<4;++i){auto image=expected.frame(i/4.);QCOMPARE(rgba.mid(i*320*180*4,320*180*4),QByteArray(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes()));}
        if(qEnvironmentVariableIsSet("CHOSUTA_SUBTITLE_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_SUBTITLE_ARTIFACTS"));QVERIFY(out.mkpath("."));QVERIFY(transparent.save(out.filePath("closed.png")));p.assets[p.fallback]=out.filePath("closed.png");saveProject(p,out.filePath("original-subtitle.chosuta"));QVERIFY(expected.frame(.25).save(out.filePath("subtitle-frame.png")));QFile source(movie);QVERIFY(source.open(QIODevice::ReadOnly));QSaveFile destination(out.filePath("original-subtitle.mov"));QVERIFY(destination.open(QIODevice::WriteOnly));auto bytes=source.readAll();QCOMPARE(destination.write(bytes),bytes.size());QVERIFY(destination.commit());}
    }
    void canvasAndPersistence() {
        QTemporaryDir dir;auto p=basic();
        QImage bg(80,40,QImage::Format_ARGB32);bg.fill(Qt::blue);{QPainter paint(&bg);paint.fillRect(0,0,20,40,Qt::yellow);}QVERIFY(bg.save(dir.filePath("背景.png")));
        QImage character(40,40,QImage::Format_ARGB32);character.fill(Qt::red);QVERIFY(character.save(dir.filePath("立绘.png")));
        p.canvas.width=64;p.canvas.height=64;p.canvas.background=Qt::green;p.canvas.backgroundImage=dir.filePath("背景.png");
        p.assets["A"]=p.assets[p.fallback]=dir.filePath("立绘.png");p.canvas.characterScale=.25;p.canvas.characterX=.75;p.canvas.characterY=.25;
        p.output.duration=3;p.audioDuration=4;
        Scene scene(p);auto image=scene.frame(0);QCOMPARE(image.pixelColor(48,16),QColor(Qt::red));QCOMPARE(image.pixelColor(32,32),QColor(Qt::blue));QCOMPARE(image.pixelColor(4,0),QColor(Qt::blue));
        p.canvas.backgroundFit="contain";Scene contain(p);image=contain.frame(0);QCOMPARE(image.pixelColor(0,0),QColor(Qt::green));QCOMPARE(image.pixelColor(32,32),QColor(Qt::blue));
        p.canvas.backgroundFit="stretch";Scene stretch(p);QCOMPARE(stretch.frame(0).pixelColor(0,0),QColor(Qt::yellow));
        auto path=dir.filePath("画布.chosuta");saveProject(p,path);auto restored=loadProject(path);
        QCOMPARE(restored.canvas.backgroundImage,p.canvas.backgroundImage);QCOMPARE(restored.canvas.characterScale,.25);QCOMPARE(restored.canvas.characterX,.75);QCOMPARE(restored.canvas.backgroundFit,QString("stretch"));QCOMPARE(restored.duration(),3.);QCOMPARE(restored.audioDuration,4.);
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();QCOMPARE(root["schema"].toInt(),14);
        root.remove("canvas");QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();QVERIFY_EXCEPTION_THROWN(loadProject(path),Failure);
        p.canvas.characterScale=0;QVERIFY_EXCEPTION_THROWN(validateCanvas(p.canvas),Failure);p.canvas.characterScale=.25;
        if(QStandardPaths::findExecutable("ffmpeg").isEmpty())QSKIP("FFmpeg absent");
        p.output.format="mov";p.output.duration=3;p.output.fpsNum=2;std::atomic_bool cancel=false;
        auto video=dir.filePath("canvas.mov");auto result=exportVideo(p,testMediaTools(p.output.ffmpeg),video,cancel);QVERIFY2(result.success,qPrintable(result.error.text()));
        QProcess decode;decode.start("ffmpeg",{"-v","error","-i",video,"-f","rawvideo","-pix_fmt","rgba","pipe:1"});QVERIFY(decode.waitForFinished());
        auto rgba=decode.readAllStandardOutput();QCOMPARE(rgba.size(),64*64*4*6);auto expected=Scene(p).frame(0);QCOMPARE(rgba.left(64*64*4),QByteArray(reinterpret_cast<const char*>(expected.constBits()),expected.sizeInBytes()));
        p.canvas.backgroundImage=dir.filePath("missing.png");result=exportVideo(p,testMediaTools(p.output.ffmpeg),dir.filePath("invalid.mov"),cancel);QVERIFY(!result.success);QVERIFY(result.error.text().contains("Background"));
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
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),0).shape,QString("A"));
        p.priorities[t.id]=-1;
        p.regenerate();
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),0).shape,QString("U"));
        p.rules.special["rest"]= {
            "hold","closed",.1
        };
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),2.25).shape,QString("U"));
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
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),2.1).shape,QString("U"));
        p.rules.harmonyTakeover=false;
        p.regenerate();
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),2.1).shape,QString("E"));
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
    void waveformTiming(){
        if(!QFileInfo::exists(resolveExecutable("ffmpeg"))&&QStandardPaths::findExecutable("ffmpeg").isEmpty())QSKIP("FFmpeg unavailable");
        QTemporaryDir dir;auto audio=dir.filePath("波形 ; $ test.wav");writeTimingAudio(audio);auto p=basic();p.audioPath=audio;std::atomic_bool cancel=false;
        auto decoded=readWaveform(audio,testMediaTools("ffmpeg"),cancel,{},2);QVERIFY2(decoded.error.isEmpty(),qPrintable(decoded.error.text()));QVERIFY(decoded.wave);const auto &wave=*decoded.wave;
        QCOMPARE(wave.bins.size(),200);QCOMPARE(wave.duration(),2.);QCOMPARE(wave.peaks(0,.04).high,0.f);QVERIFY(wave.peaks(.04,.1).high>.3f);QVERIFY(wave.peaks(.04,.1).low<-.3f);QVERIFY(wave.storageBytes()<8192);QCOMPARE(wave.peaks(-2,-1).high,0.f);
        // Importing/decoding only creates display data: no project event is changed.
        QCOMPARE(p.effective()[0].start,0.);QVERIFY(p.timing.sources.isEmpty());auto raw=p.score.raw;
        auto result=correctWaveform(p,wave,cancel);QVERIFY2(result.error.isEmpty(),qPrintable(result.error.text()));QCOMPARE(result.accepted,4);p.timing=result.timing;
        auto events=p.effective();QVERIFY(std::abs(events[0].start-.05)<1e-9);QVERIFY(std::abs(events[0].end-.45)<1e-9);QCOMPARE(p.generated[0].start,0.);QCOMPARE(p.score.raw,raw);QCOMPARE(events[0].shape,QString("A"));
        // Repeated runs always compare to the score rather than the last corrected events.
        auto again=correctWaveform(p,wave,cancel);QCOMPARE(again.accepted,4);QCOMPARE(again.timing.sources.first().start,result.timing.sources.first().start);
        // A group is retimed as a whole: no phoneme or mouth shape is recognized/replaced.
        auto scoreJson=checkedJson(fixture()).object();auto track=scoreJson["tracks"].toArray()[0].toObject();auto group=track["mainGroup"].toObject();auto notes=group["notes"].toArray();
        for(int i=0;i<notes.size();++i){auto n=notes[i].toObject();n["phonemes"]=i==0?"P AA T":"IY";notes[i]=n;}group["notes"]=notes;track["mainGroup"]=group;track["mainRef"]=QJsonObject{{"uuid","main"},{"database",QJsonObject{{"language","english"},{"phoneset","arpabet"}}}};scoreJson["tracks"]=QJsonArray{track};
        Project phones;phones.score=parseSvp(QJsonDocument(scoreJson).toJson());phones.selected={phones.score.tracks[0].id};phones.audioPath=audio;phones.regenerate();auto segments=phones.generated;auto retimed=correctWaveform(phones,wave,cancel);phones.timing=retimed.timing;auto mapped=phones.effective();QCOMPARE(mapped.size(),segments.size());
        for(int i=0;i<3;++i){QCOMPARE(mapped[i].id,segments[i].id);QCOMPARE(mapped[i].shape,segments[i].shape);QVERIFY(std::abs(mapped[i].start-(.05+segments[i].start*.8))<1e-9);QVERIFY(std::abs(mapped[i].end-(.05+segments[i].end*.8))<1e-9);}
        auto stored=dir.filePath("timing.chosuta");saveProject(p,stored);auto reopened=loadProject(stored);QVERIFY(timingCorrectionCurrent(reopened));QCOMPARE(reopened.effective()[0].start,events[0].start);
        reopened.canvas.width=64;reopened.canvas.height=64;QImage red(64,64,QImage::Format_ARGB32);red.fill(Qt::red);QImage blue(64,64,QImage::Format_ARGB32);blue.fill(Qt::blue);QVERIFY(red.save(dir.filePath("A.png")));QVERIFY(blue.save(dir.filePath("rest.png")));reopened.assets["A"]=dir.filePath("A.png");reopened.assets["rest"]=dir.filePath("rest.png");reopened.assets["closed"]=dir.filePath("rest.png");
        Scene scene(reopened);QCOMPARE(AppearanceResolver(reopened).at(reopened,reopened.effective(),.02).shape,QString("rest"));QCOMPARE(AppearanceResolver(reopened).at(reopened,reopened.effective(),.07).shape,QString("A"));QCOMPARE(scene.frame(.02).pixelColor(32,32),QColor(Qt::blue));QCOMPARE(scene.frame(.07).pixelColor(32,32),QColor(Qt::red));
        auto hashMismatch=reopened;hashMismatch.audioContentHash=QString(64,'0');QVERIFY(blue.save(dir.filePath("A.png")));scene.setTiming(hashMismatch);QCOMPARE(scene.frame(.02).pixelColor(32,32),QColor(Qt::red)); // Retains decoded red asset, refreshes timing only.
        reopened.timing.enabled=false;QCOMPARE(reopened.effective()[0].start,0.);reopened.timing.enabled=true;
        auto edited=events[0];edited.shape="O";edited.start=.12;edited.end=.21;reopened.edit(edited);
        auto locked=correctWaveform(reopened,wave,cancel);QCOMPARE(locked.protectedSources,1);QCOMPARE(locked.accepted,3);reopened.timing=locked.timing;QCOMPARE(AppearanceResolver(reopened).at(reopened,reopened.effective(),.15).shape,QString("O"));reopened.timing.sources.clear();QCOMPARE(AppearanceResolver(reopened).at(reopened,reopened.effective(),.15).shape,QString("O"));
        auto limited=basic();limited.audioPath=audio;limited.timing.maxShift=.02;auto small=correctWaveform(limited,wave,cancel);QCOMPARE(small.accepted,0);
        auto partial=wave;for(int i=0;i<50;++i)partial.bins[i]={};partial.buildLevels();auto local=correctWaveform(limited,partial,cancel);QCOMPARE(local.accepted,0);
        limited.timing.maxShift=.1;local=correctWaveform(limited,partial,cancel);QCOMPARE(local.accepted,3);QVERIFY(!local.timing.sources.contains(limited.generated[0].source));QVERIFY(std::abs(local.timing.sources[limited.generated[1].source].start-.55)<1e-9);
        auto continuous=wave;for(auto &bin:continuous.bins)bin={-.3f,.3f,.3f};continuous.buildLevels();QCOMPARE(correctWaveform(limited,continuous,cancel).accepted,0);
        // A real anticipation may cross the old MIDI edge if both independent
        // estimates remain ordered; do not clamp all starts to the old neighbour.
        auto anticipation=wave;for(int i=0;i<100;++i)anticipation.bins[i]=((i>=2&&i<44)||(i>=46&&i<94))?WaveBin{-.3f,.3f,.3f}:WaveBin{};anticipation.buildLevels();auto early=correctWaveform(limited,anticipation,cancel);QCOMPARE(early.accepted,4);QVERIFY(std::abs(early.timing.sources[limited.generated[1].source].start-.46)<1e-9);QVERIFY(std::abs(early.timing.sources[limited.generated[0].source].end-.44)<1e-9);
        auto stretched=wave;for(int i=0;i<50;++i)stretched.bins[i]=(i>=10&&i<40)?WaveBin{-.3f,.3f,.3f}:WaveBin{};stretched.buildLevels();auto tooShort=correctWaveform(limited,stretched,cancel);QVERIFY(!tooShort.timing.sources.contains(limited.generated[0].source));QVERIFY(tooShort.conflicts>0);
        auto offset=limited;offset.output.syncOffset=.25;offset.output.audioOffset=.2;auto synced=correctWaveform(offset,wave,cancel);QVERIFY(synced.timing.sources.contains(offset.generated[0].source));QVERIFY(std::abs(synced.timing.sources[offset.generated[0].source].start)<1e-9);QVERIFY(std::abs(synced.timing.sources[offset.generated[0].source].end-.4)<1e-9);
        auto soloJson=checkedJson(fixture()).object();auto soloTrack=soloJson["tracks"].toArray()[0].toObject();auto soloGroup=soloTrack["mainGroup"].toObject();auto soloNote=soloGroup["notes"].toArray()[0].toObject();soloNote["onset"]=Blick/5;soloGroup["notes"]=QJsonArray{soloNote};soloTrack["mainGroup"]=soloGroup;soloJson["tracks"]=QJsonArray{soloTrack};
        Project solo;solo.score=parseSvp(QJsonDocument(soloJson).toJson());solo.selected={solo.score.tracks[0].id};solo.audioPath=audio;solo.timing.maxDurationChange=0;solo.regenerate();auto translated=wave;for(int i=0;i<200;++i)translated.bins[i]=(i>=18&&i<68)?WaveBin{-.3f,.3f,.3f}:WaveBin{};translated.buildLevels();auto moved=correctWaveform(solo,translated,cancel);QCOMPARE(moved.accepted,1);QVERIFY(std::abs(moved.timing.sources.first().start-.18)<1e-9);QVERIFY(std::abs(moved.timing.sources.first().end-.68)<1e-9);
        soloNote["phonemes"]="ｃｌ";soloGroup["notes"]=QJsonArray{soloNote};soloTrack["mainGroup"]=soloGroup;soloJson["tracks"]=QJsonArray{soloTrack};solo.score=parseSvp(QJsonDocument(soloJson).toJson());solo.selected={solo.score.tracks[0].id};solo.regenerate();QCOMPARE(correctWaveform(solo,translated,cancel).accepted,0);
        auto ambiguous=wave;for(int i=2;i<4;++i)ambiguous.bins[i]={-.3f,.3f,.3f};ambiguous.bins[4]={};ambiguous.bins[5]={};ambiguous.buildLevels();auto uncertain=correctWaveform(limited,ambiguous,cancel);QVERIFY(!uncertain.timing.sources.contains(limited.generated[0].source)||uncertain.timing.sources[limited.generated[0].source].start==0.);
        auto stale=p;stale.output.audioOffset=.2;QVERIFY(!timingCorrectionCurrent(stale));QCOMPARE(stale.effective()[0].start,0.);stale=p;stale.output.syncOffset=.2;QVERIFY(!timingCorrectionCurrent(stale));
        stale=p;QVERIFY(verifyTimingAudio(stale,cancel));QVERIFY(timingCorrectionCurrent(stale));stale.audioContentHash=QString(64,'0');QVERIFY(!timingCorrectionCurrent(stale));QCOMPARE(stale.effective()[0].start,0.);
        stale=p;stale.rules.consonantRatio=.4;QVERIFY(!timingCorrectionCurrent(stale));stale=p;stale.regenerate();QVERIFY(stale.timing.sources.isEmpty());
        // Limits are checked when saving/reading, not merely by the UI.
        auto invalid=p;invalid.timing.sources.first().start=-.5;QVERIFY_EXCEPTION_THROWN(saveProject(invalid,stored),Failure);invalid=p;invalid.timing.maxShift=std::numeric_limits<double>::quiet_NaN();QVERIFY_EXCEPTION_THROWN(saveProject(invalid,stored),Failure);
        invalid=p;invalid.timing.sources.first().originalStart=.01;QVERIFY_EXCEPTION_THROWN(saveProject(invalid,stored),Failure);
        QFile saved(stored);QVERIFY(saved.open(QIODevice::ReadOnly));auto root=checkedJson(saved.readAll()).object();saved.close();auto timing=root["timingCorrection"].toObject();timing["maxShift"]="100";root["timingCorrection"]=timing;QVERIFY(saved.open(QIODevice::WriteOnly));saved.write(QJsonDocument(root).toJson());saved.close();QVERIFY_EXCEPTION_THROWN(loadProject(stored),Failure);
        cancel=true;QVERIFY(readWaveform(audio,testMediaTools("ffmpeg"),cancel).cancelled);QVERIFY(correctWaveform(p,wave,cancel).cancelled);cancel=false;
        auto failure=readWaveform(audio,testMediaTools("chosuta-no-such-decoder"),cancel);QVERIFY(!failure.wave);QVERIFY(!failure.error.isEmpty());
        QFile changed(audio);QVERIFY(changed.open(QIODevice::Append));changed.write("x");changed.close();QVERIFY(!timingCorrectionCurrent(p));QCOMPARE(p.effective()[0].start,0.);
    }
    void eventResolution() {
        auto p=basic();
        Event e=p.generated[0];
        e.shape="O";
        e.start=.1;
        e.end=.7;
        p.edit(e);
        auto raw=p.effective(),resolved=resolveEvents(raw);
        QCOMPARE(AppearanceResolver(p).at(p,resolved,.55,true).shape,QString("O"));
        QCOMPARE(AppearanceResolver(p).at(p,resolved,.7,true).shape,QString("I"));
        for(int i=0;i<250;++i) {
            double t=i*.01;
            QCOMPARE(AppearanceResolver(p).at(p,raw,t).shape,AppearanceResolver(p).at(p,resolved,t,true).shape);
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
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.25).shape,QString("E"));
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.8).shape,QString("U"));
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),1.2).shape,QString("U"));
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),1.8).shape,QString("unknown"));
        p=basic();
        p.rules.readings[p.score.tracks[0].notes[0].id]="きょ";
        p.regenerate();
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.3).shape,QString("O"));
        QVERIFY(!pronounce("月光下学习音乐","zh").unknown);
        p=basic();
        p.score.tracks[0].notes[0].phonemes="N";
        p.rules.special["nasal"]= {
            "timed","A",.1
        };
        p.regenerate();
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.05).shape,QString("closed"));
        QCOMPARE(AppearanceResolver(p).at(p,p.effective(),.2).shape,QString("A"));
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
        auto result=exportVideo(p,testMediaTools(p.output.ffmpeg),path,cancel);
        QVERIFY2(result.success,qPrintable(result.error.text()));
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
        result=exportVideo(p,testMediaTools(p.output.ffmpeg),path,cancel);
        QVERIFY2(result.success,qPrintable(result.error.text()));
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
        auto metadata=probeAudio(audio,testMediaTools("ffmpeg"),cancel);QVERIFY2(metadata.error.isEmpty(),qPrintable(metadata.error.text()));QVERIFY(std::abs(metadata.duration-1)<.001);
        auto bad=probeAudio(dir.filePath("absent.wav"),testMediaTools("ffmpeg"),cancel);QVERIFY(!bad.error.isEmpty());
        std::atomic_bool cancelled=true;QVERIFY(probeAudio(audio,testMediaTools("ffmpeg"),cancelled).cancelled);
        p.output.format="mp4";
        p.output.fpsNum=30;
        p.output.fpsDen=1;
        p.output.duration=2;
        p.output.audioOffset=.25;
        p.audioPath=audio;
        path=dir.filePath("audio-positive.mp4");
        result=exportVideo(p,testMediaTools(p.output.ffmpeg),path,cancel);
        QVERIFY2(result.success,qPrintable(result.error.text()));
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
        result=exportVideo(p,testMediaTools(p.output.ffmpeg),path,cancel);
        QVERIFY2(result.success,qPrintable(result.error.text()));
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
        auto r=exportVideo(p,testMediaTools(p.output.ffmpeg),path,cancel);
        QVERIFY2(r.success,qPrintable(r.error.text()));
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
        r=exportVideo(p,testMediaTools(p.output.ffmpeg),path,cancel,[&](int n) {
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
