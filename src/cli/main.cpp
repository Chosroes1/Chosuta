// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/model.h"
#include "core/appearance.h"
#include "core/performance.h"
#include "core/performance_analysis.h"
#include "render/render.h"
#include "render/waveform.h"
#include "render/audio.h"
#include <QGuiApplication>
#include <csignal>
#include <iostream>
using namespace chosuta;
static std::atomic_bool cancelled=false;
static void cancelSignal(int) {
    cancelled.store(true);
}
static Project readInput(const QString&file,const QString &vvprojSection) {
    if(file.endsWith(".chosuta",Qt::CaseInsensitive))return loadProject(file);
    Project p;
    p.score=importSource(file,&cancelled,{},vvprojSection);
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
    parser.setApplicationDescription("Chosuta: inspect/generate/export SVP, UST, USTX, VSQ/VSQX, MusicXML/MXL, CCS song, VPR, LAB, timed DS, VVPROJ talk/song or .chosuta projects; create original demo assets.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command","inspect | generate | export | correct | revert | demo");
    parser.addPositionalArgument("input","SVP/UST/USTX/VSQ/VSQX/MusicXML/MXL/CCS song/VPR/LAB/DS/VVPROJ talk/song/project input; demo uses output directory");
    parser.addPositionalArgument("output","Project/video output for generate/export","[output]");
    auto option=[&](QString name,QString description,QString value,QString def={}) {
        parser.addOption(QCommandLineOption(name,description,value,def));
    };
    option("vvproj-section","VVPROJ content: auto | talk | song; required when both contain content","section","auto");
    option("tracks","Comma-separated zero-based track indexes; default first track","indexes");
    option("language","auto | ja | zh | en","language");
    option("dictionary","External CMU-format English dictionary (total dictionary data <= 3 MB)","path");
    option("custom-dictionary","Chosuta custom pronunciation settings JSON","path");
    parser.addOption({"japanese-kanji","Enable optional estimated Japanese kanji readings"});
    parser.addOption({"no-japanese-kanji","Disable optional Japanese kanji readings"});
    option("assets","Scan basic mouth PNGs; --force permits replacing mappings","directory");
    option("variant-assets","Import direct subfolders as variant groups; basic names in each group","directory");
    parser.addOption({"advanced-mouth","Enable score-driven advanced images"});
    parser.addOption({"simple-mouth","Use simple images (retain advanced settings)"});
    option("key","Optional tonic and mode, e.g. C maj or D Dorian","key");option("performance-json","Replace performance settings/groups/segments from JSON","file");option("generate-performance","Generate and apply automatic performance",{});option("performance-info","Print performance JSON",{});option("performance-analysis","Save score analysis diagnostics to JSON","file");
    option("consonant-settings","Consonant settings JSON; regenerate using its overrides","path");
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
        "force","Explicitly permit replacing an existing output or asset mapping"
    });
    parser.addOption({"check-media-tools","Check FFmpeg/ffprobe startup and current export capabilities; JSON output"});
    parser.process(app);
    std::signal(SIGINT,cancelSignal);
    std::signal(SIGTERM,cancelSignal);
    try {
        if(parser.isSet("check-media-tools")){
            const auto info=inspectMediaTools(selectMediaTools(parser.isSet("ffmpeg")?parser.value("ffmpeg"):QString("ffmpeg")),cancelled);
            QJsonObject result{{"ready",info.ready()},{"ffmpeg",info.ffmpeg},{"ffprobe",info.ffprobe},
                {"ffmpegVersion",info.ffmpegVersion},{"ffprobeVersion",info.ffprobeVersion},
                {"errors",QJsonArray::fromStringList(messageStrings(info.errors))},{"missingEncoders",QJsonArray::fromStringList(info.missingEncoders)},
                {"missingFilters",QJsonArray::fromStringList(info.missingFilters)},{"cancelled",info.cancelled}};
            std::cout<<QJsonDocument(result).toJson().toStdString();return info.cancelled?130:info.ready()?0:1;
        }
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
        Project p=readInput(input,parser.value("vvproj-section"));
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
        if(parser.isSet("advanced-mouth")&&parser.isSet("simple-mouth"))throw Failure("Conflicting mouth modes");
        if(parser.isSet("advanced-mouth"))p.appearance.enabled=true;
        if(parser.isSet("simple-mouth"))p.appearance.enabled=false;
        if(parser.isSet("key")){auto key=parseTonality(parser.value("key"));if(!key)throw Failure("Invalid key");p.performance.analysis.tonalityEnabled=true;p.performance.analysis.degreeTonic=key->first;p.performance.analysis.degreeMode=key->second;}
        if(parser.isSet("performance-json")){QFile f(parser.value("performance-json"));if(!f.open(QIODevice::ReadOnly)||f.size()>64*1024*1024)throw Failure("Cannot read performance JSON");readPerformance(p,checkedJson(f.readAll()).object());}
        if(parser.isSet("consonant-settings")){QFile file(parser.value("consonant-settings"));if(!file.open(QIODevice::ReadOnly)||file.size()>65536)throw Failure("Cannot read consonant settings (limit 64 KiB)");p.rules.consonants=readConsonants(checkedJson(file.readAll()).object());}
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
                }, {
                    "hasMusic",p.score.hasMusic
                }, {
                    "endSeconds",p.score.sourceDuration()
                }, {
                    "sourceFormat",p.score.sourceFormat
                }, {
                    "sourceVersion",p.score.sourceVersion
                }, {
                    "sourceResolution",QString::number(p.score.sourceResolution)
                }, {
                    "sourceTickOrigin",QString::number(p.score.sourceTickOrigin)
                }, {
                    "endPosition",QString::number(p.score.endPosition)
                }
            };
            QJsonArray tracks,tempos,meters,diagnostics;
            for(const auto&t:p.score.tracks)tracks.append(QJsonObject {
                {
                    "id",t.id
                }, {
                    "name",t.name
                }, {
                    "timedPhones",t.timedPhones.size()
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
                    "message",d.message.text()
                }
            });
            summary["tracks"]=tracks;
            summary["tempo"]=tempos;
            summary["meter"]=meters;
            if(!p.score.time.measureStarts.isEmpty()){
                QJsonArray starts;for(const auto at:p.score.time.measureStarts)starts.append(QString::number(at));summary["measureStarts"]=starts;
                summary["pickupOffset"]=QString::number(p.score.time.pickupOffset);
            }
            summary["diagnostics"]=diagnostics;
            summary["appearance"]=QJsonObject{{"enabled",p.appearance.enabled}};summary["analysis"]=analysisSettingsJson(p.performance.analysis);summary["consonants"]=consonantsJson(p.rules.consonants);
            summary["timingCorrection"]=QJsonObject{{"enabled",p.timing.enabled},{"current",timingCorrectionCurrent(p)},{"sources",p.timing.sources.size()},{"maxShift",p.timing.maxShift},{"maxDurationChange",p.timing.maxDurationChange}};
            std::cout<<QJsonDocument(summary).toJson().constData();
            return 0;
        }
        if(command!="generate"&&command!="export"&&command!="correct"&&command!="revert")throw Failure("Unknown command");
        if(args.size()!=3)throw Failure("Output path is required");
        if(!input.endsWith(".chosuta",Qt::CaseInsensitive)||parser.isSet("tracks")||parser.isSet("language")||parser.isSet("dictionary")||parser.isSet("custom-dictionary")||parser.isSet("japanese-kanji")||parser.isSet("no-japanese-kanji")||parser.isSet("consonant-settings"))p.regenerate();
        auto importPlan=[&](const AssetImportPlan &plan){
            if(!plan.error.isEmpty())throw Failure(plan.error);
            QVector<AssetImportEntry> entries;
            for(const auto &entry:plan.entries){
                const bool usable=entry.status=="new"||entry.status=="conflict"||entry.status=="replace";
                if(usable&&(!p.assets.contains(entry.id)||parser.isSet("force")))entries.append(entry);
                else std::cerr<<(entry.status+": skipped "+entry.path).toStdString()<<"\n";
            }
            applyAssetImport(p,entries);
            for(const auto &group:plan.groups)p.performance.groups.append(group);
            validatePerformance(p);
        };
        if(parser.isSet("assets")){const auto plan=scanAssetDirectory(parser.value("assets"),p.assets,&cancelled);if(plan.cancelled)return 130;importPlan(plan);}
        if(parser.isSet("variant-assets")){const auto plan=scanVariantDirectory(parser.value("variant-assets"),p,"base",&cancelled);if(plan.cancelled)return 130;importPlan(plan);}
        auto integer=[&](QString name,int &value){if(parser.isSet(name)){bool ok=false;const int n=parser.value(name).toInt(&ok);if(!ok)throw Failure(Message::format("Invalid %1",{name}));value=n;}};
        auto real=[&](QString name,double &value){if(parser.isSet(name)){bool ok=false;const double n=parser.value(name).toDouble(&ok);if(!ok||!std::isfinite(n))throw Failure(Message::format("Invalid %1",{name}));value=n;}};
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
            auto decoded=readWaveform(p.audioPath,selectMediaTools(p.output.ffmpeg),cancelled,{},p.audioDuration);
            if(decoded.cancelled)return 130;if(!decoded.error.isEmpty())throw Failure(decoded.error);
            std::cout<<"Waveform: "<<decoded.wave->bins.size()<<" bins, "<<decoded.wave->storageBytes()<<" summary bytes, "<<decoded.wave->duration()<<" seconds\n";
            auto result=correctWaveform(p,*decoded.wave,cancelled);
            if(result.cancelled)return 130;if(!result.error.isEmpty())throw Failure(result.error);
            p.timing=result.timing;p.audioDuration=decoded.wave->duration();
            std::cout<<result.accepted<<" corrected score sources; "<<result.protectedSources<<" protected, "<<result.unclear<<" unclear, "<<result.conflicts<<" conflicting\n";
        }
        if(command=="revert"){p.timing.sources.clear();p.timing.enabled=false;}
        if(parser.isSet("generate-performance")){auto draft=generatePerformance(p,&cancelled);if(draft.cancelled)return 130;if(!draft.error.isEmpty())throw Failure(draft.error);applyPerformanceDraft(p,draft);}
        if(parser.isSet("performance-analysis")){auto path=parser.value("performance-analysis");if(QFileInfo::exists(path)&&!parser.isSet("force"))throw Failure("Analysis output exists; choose another path or --force");auto data=QJsonDocument(performanceAnalysisJson(p,&cancelled)).toJson();QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())throw Failure("Cannot save performance analysis");}
        if(parser.isSet("performance-info"))QTextStream(stdout)<<QJsonDocument(performanceJson(p)).toJson();
        if(command=="generate"||command=="correct"||command=="revert") {
            if(QFileInfo::exists(args[2])&&!parser.isSet("force"))throw Failure("Output exists; use --force");
            saveProject(p,args[2]);
            std::cout<<p.generated.size()<<" estimated events\n";
            return 0;
        }
        auto result=exportVideo(p,selectMediaTools(p.output.ffmpeg),args[2],cancelled,[](int n) {
            std::cerr<<"\r"<<n<<"%   "<<std::flush;
        },parser.isSet("force"));
        std::cerr<<'\n';
        for(const auto&message:result.diagnostics)std::cerr<<"Chosuta: "<<message.text().toStdString()<<'\n';
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
