// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
#include "render/render.h"
#include "render/waveform.h"
#include <QGuiApplication>
#include <csignal>
#include <iostream>
using namespace chosuta;
static std::atomic_bool cancelled=false;
static void cancelSignal(int) {
    cancelled.store(true);
}
static Project readInput(const QString&file) {
    if(file.endsWith(".chosuta",Qt::CaseInsensitive))return loadProject(file);
    Project p;
    p.score=importSvp(file);
    if(!p.score.tracks.isEmpty())p.selected= {
        p.score.tracks.front().id
    };
    return p;
}
int main(int argc,char**argv) {
    if(qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))qputenv("QT_QPA_PLATFORM","offscreen");
    QGuiApplication app(argc,argv);
    QCoreApplication::setApplicationName("chosuta-cli");
    QCoreApplication::setApplicationVersion(CHOSUTA_VERSION);
    QCommandLineParser parser;
    parser.setApplicationDescription("Chosuta: inspect/generate/export SVP or .chosuta projects; create original demo assets.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command","inspect | generate | export | correct | revert | demo");
    parser.addPositionalArgument("input","SVP/project input; demo uses output directory");
    parser.addPositionalArgument("output","Project/video output for generate/export","[output]");
    auto option=[&](QString name,QString description,QString value,QString def={}) {
        parser.addOption(QCommandLineOption(name,description,value,def));
    };
    option("tracks","Comma-separated zero-based track indexes; default first track","indexes");
    option("language","auto | ja | zh | en","language");
    option("dictionary","External CMU-format English dictionary (total dictionary data <= 3 MB)","path");
    option("custom-dictionary","Chosuta custom pronunciation settings JSON","path");
    parser.addOption({"japanese-kanji","Enable optional estimated Japanese kanji readings"});
    parser.addOption({"no-japanese-kanji","Disable optional Japanese kanji readings"});
    option("assets","PNG directory: A.png, I.png, U.png, E.png, O.png, closed.png, etc.","directory");
    option("size","WIDTHxHEIGHT","resolution");
    option("fps","Integer or NUM/DEN","rate");
    option("duration","Output seconds; default score duration","seconds");
    option("format","mp4 | webm | mov","format");
    option("background","Background color (#RRGGBB)","color");
    option("background-image","Background image path","image");
    option("background-fit","cover|contain|stretch","fit");
    option("character-scale","Character scale (1 = fit canvas)","factor");
    option("character-x","Character center X (0..1 = canvas width)","position");
    option("character-y","Character center Y (0..1 = canvas height)","position");
    option("ffmpeg","FFmpeg executable/path","path");
    option("audio","Optional audio file","path");
    option("audio-offset","Audio start offset in seconds (negative trims)","seconds");
    option("sync-offset","Animation offset in seconds","seconds");
    option("max-shift","Maximum local waveform timing shift (0..1000 ms; default 100)","milliseconds");
    option("max-duration-change","Maximum relative duration change (0..50 percent; default 25)","percent");
    option("crf","Constant quality (0..51)","quality");
    option("bitrate","Target video kbps, 0 = quality mode","kbps");
    parser.addOption( {
        "transparent","Transparent MOV/QTRLE output"
    });
    parser.addOption( {
        "force","Explicitly permit replacing an existing output"
    });
    parser.process(app);
    std::signal(SIGINT,cancelSignal);
    std::signal(SIGTERM,cancelSignal);
    try {
        auto args=parser.positionalArguments();
        if(args.size()<2)parser.showHelp(1);
        QString command=args[0],input=args[1];
        if(command=="demo") {
            createDemoAssets(input);
            QJsonArray notes;
            QStringList words= {
                "あ","い","う","え","お","cl"
            };
            for(int i=0;i<words.size();++i)notes.append(QJsonObject {
                {
                    "uuid",QString("demo-%1").arg(i)
                }, {
                    "onset",qint64(i)*Blick
                }, {
                    "duration",Blick
                }, {
                    "pitch",60
                }, {
                    "lyrics",words[i]
                }, {
                    "phonemes",""
                }
            });
            QJsonObject group {
                {
                    "uuid","original-demo"
                }, {
                    "notes",notes
                }
            };
            QJsonObject root {
                {
                    "version",196
                }, {
                    "time",QJsonObject {
                        {
                            "tempo",QJsonArray {
                                QJsonObject {
                                    {
                                        "position",0
                                    }, {
                                        "bpm",120
                                    }
                                }
                            }
                        }, {
                            "meter",QJsonArray {
                                QJsonObject {
                                    {
                                        "index",0
                                    }, {
                                        "numerator",4
                                    }, {
                                        "denominator",4
                                    }
                                }
                            }
                        }
                    }
                }, {
                    "tracks",QJsonArray {
                        QJsonObject {
                            {
                                "name","Original demo"
                            }, {
                                "mainGroup",group
                            }, {
                                "mainRef",QJsonObject {
                                    {
                                        "blickOffset",0
                                    }
                                }
                            }
                        }
                    }
                }
            };
            QString file=QDir(input).filePath("demo.svp");
            if(QFileInfo::exists(file)&&!parser.isSet("force"))throw Failure("Demo SVP exists; use --force to replace it");
            QSaveFile f(file);
            auto b=QJsonDocument(root).toJson();
            if(!f.open(QIODevice::WriteOnly)||f.write(b)!=b.size()||!f.commit())throw Failure(f.errorString());
            std::cout<<file.toStdString()<<'\n';
            return 0;
        }
        Project p=readInput(input);
        if(parser.isSet("tracks")) {
            p.selected.clear();
            for(const auto&s:parser.value("tracks").split(',')) {
                bool ok=false;
                int i=s.toInt(&ok);
                if(!ok||i<0||i>=p.score.tracks.size())throw Failure("Invalid track index");
                if(!p.selected.contains(p.score.tracks[i].id)) {
                    p.priorities[p.score.tracks[i].id]=p.selected.size();
                    p.selected.append(p.score.tracks[i].id);
                }
            }
        }
        if(parser.isSet("language"))p.rules.language=parser.value("language");
        if(!QStringList {
            "auto","ja","zh","en"
        }
        .contains(p.rules.language))throw Failure("Invalid language");
        if(parser.isSet("dictionary"))p.rules.englishDictionary=QFileInfo(parser.value("dictionary")).absoluteFilePath();
        if(parser.isSet("custom-dictionary")) {
            QFile file(parser.value("custom-dictionary"));
            if(!file.open(QIODevice::ReadOnly))throw Failure(file.errorString());
            if(file.size()>DictionaryByteLimit)throw Failure("Dictionary exceeds 3 MB");
            p.rules.pronunciation=pronunciationOptionsRead(checkedJson(file.readAll()).object());
        }
        if(parser.isSet("japanese-kanji")&&parser.isSet("no-japanese-kanji"))throw Failure("Conflicting Japanese reading options");
        if(parser.isSet("japanese-kanji"))p.rules.pronunciation.japaneseKanji=true;
        if(parser.isSet("no-japanese-kanji"))p.rules.pronunciation.japaneseKanji=false;
        if(command=="inspect") {
            QJsonObject summary {
                {
                    "version",p.score.version
                }, {
                    "sourceHash",p.score.hash
                }, {
                    "sourceOrigin",p.score.sourceOrigin
                }
            };
            QJsonArray tracks,tempos,meters,diagnostics;
            for(const auto&t:p.score.tracks)tracks.append(QJsonObject {
                {
                    "id",t.id
                }, {
                    "name",t.name
                }, {
                    "notes",t.notes.size()
                }, {
                    "muted",t.muted
                }
            });
            for(const auto&t:p.score.time.tempos)tempos.append(QJsonObject {
                {
                    "position",QString::number(t.position)
                }, {
                    "bpm",t.bpm
                }
            });
            for(const auto&m:p.score.time.meters)meters.append(QJsonObject {
                {
                    "bar",m.bar
                }, {
                    "numerator",m.numerator
                }, {
                    "denominator",m.denominator
                }
            });
            for(const auto&d:p.score.diagnostics)diagnostics.append(QJsonObject {
                {
                    "path",d.path
                }, {
                    "code",d.code
                }, {
                    "message",d.message
                }
            });
            summary["tracks"]=tracks;
            summary["tempo"]=tempos;
            summary["meter"]=meters;
            summary["diagnostics"]=diagnostics;
            summary["timingCorrection"]=QJsonObject{{"enabled",p.timing.enabled},{"current",timingCorrectionCurrent(p)},{"sources",p.timing.sources.size()},{"maxShift",p.timing.maxShift},{"maxDurationChange",p.timing.maxDurationChange}};
            std::cout<<QJsonDocument(summary).toJson().constData();
            return 0;
        }
        if(command!="generate"&&command!="export"&&command!="correct"&&command!="revert")throw Failure("Unknown command");
        if(args.size()!=3)throw Failure("Output path is required");
        if(!input.endsWith(".chosuta",Qt::CaseInsensitive)||parser.isSet("tracks")||parser.isSet("language")||parser.isSet("dictionary")||parser.isSet("custom-dictionary")||parser.isSet("japanese-kanji")||parser.isSet("no-japanese-kanji"))p.regenerate();
        if(parser.isSet("assets")) {
            QDir dir(parser.value("assets"));
            for(const auto&file:dir.entryList( {
                "*.png","*.PNG"
            },QDir::Files))p.assets[QFileInfo(file).completeBaseName()]=dir.absoluteFilePath(file);
        }
        auto integer=[&](QString name,int&v) {
            if(parser.isSet(name)) {
                bool ok=false;
                int x=parser.value(name).toInt(&ok);
                if(!ok)throw Failure("Invalid "+name);
                v=x;
            }
        };
        auto real=[&](QString name,double&v) {
            if(parser.isSet(name)) {
                bool ok=false;
                double x=parser.value(name).toDouble(&ok);
                if(!ok||!std::isfinite(x))throw Failure("Invalid "+name);
                v=x;
            }
        };
        if(parser.isSet("size")) {
            auto size=parser.value("size").split('x');
            if(size.size()!=2)throw Failure("Invalid size");
            bool a=false,b=false;
            p.canvas.width=size[0].toInt(&a);
            p.canvas.height=size[1].toInt(&b);
            if(!a||!b)throw Failure("Invalid size");
        }
        if(parser.isSet("fps")) {
            auto fps=parser.value("fps").split('/');
            if(fps.size()>2)throw Failure("Invalid fps");
            bool a=false,b=true;
            p.output.fpsNum=fps[0].toInt(&a);
            p.output.fpsDen=fps.size()==2?fps[1].toInt(&b):1;
            if(!a||!b)throw Failure("Invalid fps");
        }
        integer("crf",p.output.crf);
        integer("bitrate",p.output.bitrateKbps);
        real("duration",p.output.duration);
        real("audio-offset",p.output.audioOffset);
        real("sync-offset",p.output.syncOffset);
        if(parser.isSet("format"))p.output.format=parser.value("format");
        else if(command=="export")p.output.format=QFileInfo(args[2]).suffix().toLower();
        if(parser.isSet("background"))p.canvas.background=QColor(parser.value("background"));
        if(parser.isSet("background-image"))p.canvas.backgroundImage=QFileInfo(parser.value("background-image")).absoluteFilePath();
        if(parser.isSet("background-fit"))p.canvas.backgroundFit=parser.value("background-fit");
        real("character-scale",p.canvas.characterScale);real("character-x",p.canvas.characterX);real("character-y",p.canvas.characterY);
        validateCanvas(p.canvas);
        if(parser.isSet("ffmpeg"))p.output.ffmpeg=parser.value("ffmpeg");
        if(parser.isSet("audio"))p.audioPath=QFileInfo(parser.value("audio")).absoluteFilePath();
        if(parser.isSet("transparent"))p.canvas.transparent=true;
        if(command=="correct"){
            p.timing.sources.clear();p.timing.enabled=false;
            double shift=p.timing.maxShift*1000,stretch=p.timing.maxDurationChange*100;real("max-shift",shift);real("max-duration-change",stretch);
            p.timing.maxShift=shift/1000;p.timing.maxDurationChange=stretch/100;validateTimingCorrection(p);
            auto decoded=readWaveform(p.audioPath,p.output.ffmpeg,cancelled,{},p.audioDuration);
            if(decoded.cancelled)return 130;if(!decoded.error.isEmpty())throw Failure(decoded.error);
            std::cout<<"Waveform: "<<decoded.wave->bins.size()<<" bins, "<<decoded.wave->storageBytes()<<" summary bytes, "<<decoded.wave->duration()<<" seconds\n";
            auto result=correctWaveform(p,*decoded.wave,cancelled);
            if(result.cancelled)return 130;if(!result.error.isEmpty())throw Failure(result.error);
            p.timing=result.timing;p.audioDuration=decoded.wave->duration();
            std::cout<<result.accepted<<" corrected score sources; "<<result.protectedSources<<" protected, "<<result.unclear<<" unclear, "<<result.conflicts<<" conflicting\n";
        }
        if(command=="revert"){p.timing.sources.clear();p.timing.enabled=false;}
        if(command=="generate"||command=="correct"||command=="revert") {
            if(QFileInfo::exists(args[2])&&!parser.isSet("force"))throw Failure("Output exists; use --force");
            saveProject(p,args[2]);
            std::cout<<p.generated.size()<<" estimated events\n";
            return 0;
        }
        if(!verifyTimingAudio(p,cancelled))return 130;
        if(p.timing.enabled&&!p.timing.sources.isEmpty()&&!timingCorrectionCurrent(p))std::cerr<<"Chosuta: waveform correction is stale; using score timing and manual edits.\n";
        auto result=exportVideo(p,args[2],cancelled,[](int n) {
            std::cerr<<"\r"<<n<<"%   "<<std::flush;
        },parser.isSet("force"));
        std::cerr<<'\n';
        for(const auto&message:result.diagnostics)std::cerr<<"Chosuta: "<<message.toStdString()<<'\n';
        if(result.cancelled)return 130;
        if(!result.success)throw Failure(result.error);
        std::cout<<result.frames<<" frames: "<<args[2].toStdString()<<'\n';
        return 0;
    }
    catch(const std::exception&e) {
        std::cerr<<"Chosuta: "<<e.what()<<'\n';
        return 1;
    }
}
