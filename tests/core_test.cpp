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
    void consonantDefaults() {
        for(const auto &phone:QStringList{"B","P","M","T","D","K","G","CH","JH"})
            QCOMPARE(pronounce(phone+" AA","en",true).syllables[0][0],QString("closed"));
        for(const auto &phone:QStringList{"F","V","S","Z","SH","ZH","TH","DH","HH","R","L","W","Y","N","NG"}){
            QCOMPARE(pronounce(phone+" AA","en",true).syllables[0][0],QString("open"));
            auto p=basic();p.score.tracks[0].notes[0].language="en";p.score.tracks[0].notes[0].phonemes=phone+" AA";p.regenerate();
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
        QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();QCOMPARE(root["schema"].toInt(),2);
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
