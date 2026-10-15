// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore>
#include "message.h"
#include <QColor>
#include <functional>
#include <atomic>
#include <optional>
namespace chosuta {
    constexpr qint64 Blick = 705600000;
    constexpr qint64 MusicLimit = 9000000000000000LL;
    struct Tempo {
        qint64 position=0;
        double bpm=120;
    };
    struct Meter {
        int bar=0, numerator=4, denominator=4;
    };
    class TimeMap {
        public:
        QVector<Tempo> tempos {
            {
                0,120
            }
        };
        QVector<Meter> meters {
            {
                0,4,4
            }
        };
        void validate();
        double seconds(qint64 blick) const;
        qint64 blicks(double seconds) const;
        QString beatLabel(qint64 blick) const;
    };
    struct Diagnostic {
        QString path, code;Message message;
    };
    struct Note {
        QString id, lyrics, phonemes, language="ja", phoneset, groupInstance;
        qint64 onset=0, duration=0;
        int pitch=60;
        bool muted=false;
        QJsonObject original;
        QString lyricReading; // Derived UTAU alias reading; lyrics retain the original alias.
        qint64 sourceOnset=0, sourceDuration=0;
    };
    struct Track {
        QString id,name;
        bool muted=false;
        QVector<Note> notes;
    };
    struct Score {
        TimeMap time;
        QVector<Track> tracks;
        QVector<Diagnostic> diagnostics;
        QByteArray raw;
        QString sourcePath, hash;
        int version=0;
        double sourceOrigin=0;
        QString sourceFormat="svp", sourceVersion;
        qint64 sourceResolution=Blick, sourceTickOrigin=0, endPosition=0;
    };
    using Progress=std::function<void(int)>;
    Score importSource(const QString &path,const std::atomic_bool *cancel=nullptr,Progress progress={});
    Score parseSource(QByteArray bytes,const QString &format,const QString &path={},const std::atomic_bool *cancel=nullptr,Progress progress={});
    Score importSvp(const QString &path, const std::atomic_bool *cancel=nullptr, Progress progress={});
    Score parseSvp(QByteArray bytes,const QString &path={},const std::atomic_bool *cancel=nullptr,Progress progress={});
    QJsonDocument checkedJson(QByteArray bytes,bool allowNul=false);
    struct Policy {
        QString mode="shape", shape="closed";
        double holdSeconds=.12;
    };
    constexpr qint64 DictionaryByteLimit=3000000;
    struct DictionaryReading {
        QString text;
        bool phonemes=false;
        bool operator==(const DictionaryReading &)const=default;
    };
    struct PronunciationOptions {
        bool japaneseKanji=false;
        QMap<QString,QMap<QString,DictionaryReading>> words;
        bool operator==(const PronunciationOptions &)const=default;
    };
    QString dictionaryKey(const QString &word,const QString &language);
    QJsonObject pronunciationOptionsJson(const PronunciationOptions &);
    PronunciationOptions pronunciationOptionsRead(const QJsonValue &);
    qint64 builtinDictionaryBytes();
    qint64 pronunciationDictionaryBytes(const PronunciationOptions &);
    void validatePronunciationOptions(const PronunciationOptions &);
    struct Rules {
        struct Consonants {
            QMap<QString,QString> overrides; // language:phoneset:phone -> open / closed
            bool operator==(const Consonants &)const=default;
        } consonants;
        QString language="auto";
        bool harmonyTakeover=true;
        double consonantRatio=.18;
        // A shared budget for each leading/trailing consonant group. Zero disables the fixed time cap.
        double consonantMaxSeconds=.08;
        QMap<QString,Policy> special {
            {
                "rest", {
                    "shape","rest",.12
                }
            }, {
                "empty", {
                    "shape","unknown",.12
                }
            },
            {
                "cl", {
                    "shape","closed",.12
                }
            }, {
                "br", {
                    "shape","breath",.12
                }
            }, {
                "nasal", {
                    "shape","closed",.12
                }
            },
            {
                "extend", {
                    "hold","closed",.12
                }
            }
        };
        QMap<QString,QString> readings;
        // note source identity -> user pronunciation
        QString englishDictionary;
        PronunciationOptions pronunciation;
    };
    struct Event {
        QString id, source, track, shape, provenance, text;
        double start=0,end=0;
        bool unknown=false;
        QString phone,articulation;
        bool appearanceFixed=false;
    };
    // Normalized intervals in the generated parent and in the edited event.
    // Kept independently so moving/resizing a child never rewrites its origin.
    struct SourceFragment {
        QString parentId;
        double sourceStart=0,sourceEnd=1,start=0,end=1;
        bool operator==(const SourceFragment &)const=default;
    };
    struct Override {
        Event event;
        QString anchor="seconds";
        qint64 startBlick=0,endBlick=0;
        bool locked=true, deleted=false, standalone=false;
        QStringList parentIds;
        QVector<SourceFragment> sourceFragments;
    };
    struct CanvasSettings {
        int width=1280,height=720;
        QColor background=QColor("#ffffff");
        bool transparent=false;
        QString backgroundImage,backgroundFit="cover";
        double characterScale=1,characterX=.5,characterY=.5;
    };
    void validateCanvas(const CanvasSettings &);
    struct ExportSettings {
        int fpsNum=30,fpsDen=1,crf=20,bitrateKbps=0;
        QString format="mp4",ffmpeg="ffmpeg";
        double duration=0,syncOffset=0,audioOffset=0;
    };
    struct SubtitleStyle {
        QString family, alignment="center";
        QColor color=Qt::white;
        double x=.5,y=.86,width=.85,fontHeight=.055;
        bool bold=false,italic=false,outline=true;
        bool operator==(const SubtitleStyle &)const=default;
    };
    struct SubtitleCue {
        QString id,text,anchor="seconds",sourceTrack;
        QStringList sourceNotes;
        double start=0,end=2; // Last valid video interval; also orphan fallback.
        qint64 startBlick=0,endBlick=0;
        bool ownStyle=false;
        SubtitleStyle style;
        bool operator==(const SubtitleCue &)const=default;
    };
    struct SubtitleTrack {
        QString id,name,sourceTrack;
        bool enabled=true,alignLyrics=false;
        SubtitleStyle style;
        QVector<SubtitleCue> cues;
        bool operator==(const SubtitleTrack &)const=default;
    };
    struct SubtitleInterval {double start=0,end=0;bool orphan=false;};
    struct LyricSpan {
        QString text,groupInstance;
        QStringList notes;
        qint64 start=0,end=0;
    };
    struct SourceTiming {
        double originalStart=0,originalEnd=0,start=0,end=0;
    };
    struct TimingCorrection {
        double maxShift=.1,maxDurationChange=.25;
        bool enabled=false;
        QString basisHash;
        qint64 audioBytes=-1,audioModified=0;
        QMap<QString,SourceTiming> sources;
    };
    struct VariantGroup {
        QString id="base",name="Base";
        bool operator==(const VariantGroup &)const=default;
    };
    struct PerformanceSegment {
        QString id,anchor="beats",track,firstSource,lastSource;
        qint64 startBlick=0,endBlick=0;
        double start=0,end=0; // Video-time snapshot, or an absolute interval.
        QString mode="hold";
        QStringList targets={"normal"}; // Role names, or group:<stable group ID>.
        QVector<double> cuts;
        double phaseStart=0,phaseEnd=1; // Preserve template phase when splitting/cropping.
        bool protect=true;
        QVector<Message> reasons;
        bool operator==(const PerformanceSegment &)const=default;
    };
    struct PerformanceBoundary {
        QString source,mode="break"; // break / join / section-break / section-join; source is the following note identity.
        bool operator==(const PerformanceBoundary &)const=default;
    };
    struct TonalityRegion {
        qint64 start=0,end=0;
        QString key; // Empty explicitly disables tonal assistance in this region.
        bool operator==(const TonalityRegion &)const=default;
    };
    struct PerformanceRule {
        bool enabled=true;
        QString condition="leap",target="emphasis",track;
        double threshold=5,minBeats=1;
        int priority=0;
        double thresholdSeconds=2.5,thresholdDegrees=4,minSeconds=0;
        bool operator==(const PerformanceRule &)const=default;
    };
    enum class PerformanceTimeBasis { Music, Seconds };
    struct PerformanceTiming {
        double window=4,background=16,lookahead=2,support=1,confirm=.5;
        double minimum=1,emphasis=2,exit=1,breath=2,phrase=1,reset=4,release=2;
        bool operator==(const PerformanceTiming &)const=default;
    };
    struct PerformanceSettings {
        PerformanceTimeBasis timeBasis=PerformanceTimeBasis::Music;
        PerformanceTiming music,seconds={2,8,1,.5,.25,.5,1,.5,1,.5,2,1};
        bool leap=true,progression=true,motif=true,section=true,release=true,continuity=true;
        int leapSemitones=5,leapDegrees=4,progressionSemitones=4,progressionDegrees=3;
        int nearSemitones=2,nearDegrees=2,contrastSemitones=3,contrastDegrees=3;
        double efficiency=.5,highShare=.6,motifShare=.3,activityRatio=1.5,wideningRatio=1.5,motifTolerance=.2;
        QVector<PerformanceRule> rules;
        const PerformanceTiming &timing()const{return timeBasis==PerformanceTimeBasis::Music?music:seconds;}
        bool operator==(const PerformanceSettings &)const=default;
    };
    struct AnalysisSettings {
            bool tonalityEnabled=false;
            QString intervalUnit="semitones";
            QString degreeTonic="C",degreeMode="major";
            QVector<int> customDegreeScale={0,2,4,5,7,9,11};
            QMap<QString,QString> noteSpellings; // Expanded Note identity -> written pitch, e.g. C#4 / Db4.
            QMap<QString,QVector<int>> strongBeats; // signature -> 1-based denominator beats
            bool operator==(const AnalysisSettings &)const=default;
    };
    struct Performance {
        AnalysisSettings analysis;
        QVector<VariantGroup> groups={{"base","Base"}};
        QMap<QString,QString> roles={{"relaxed","base"},{"normal","base"},{"emphasis","base"}};
        PerformanceSettings settings;
        QVector<PerformanceSegment> generated,manual;
        QVector<PerformanceBoundary> boundaries,suggestions;
        QVector<TonalityRegion> tonalities;
        QString basisHash; // Accepted automatic plan basis; never regenerated during rendering.
    };
    struct Project {
        struct Appearance { bool enabled=false; bool operator==(const Appearance &)const=default; } appearance;
        Performance performance;
        Score score;
        QStringList selected;
        QMap<QString,int> priorities;
        Rules rules;
        QVector<Event> generated;
        QMap<QString,Override> overrides;
        TimingCorrection timing;
        QMap<QString,QString> assets;
        QString fallback="closed",audioPath;
        CanvasSettings canvas;
        ExportSettings output;
        bool subtitlesEnabled=false;
        QVector<SubtitleTrack> subtitles;
        double audioDuration=0;
        double duration() const;
        QVector<Event> effective() const;
        QStringList orphanOverrides() const;
        void regenerate(const std::atomic_bool *cancel=nullptr,Progress progress={});
        void edit(const Event &event,bool locked=true,QString anchor="seconds");
        void erase(const Event &event);
        void split(const Event &event,double position);
        void merge(const QVector<Event> &events);
    };
    void validateOutputSettings(const ExportSettings &);
    void validateSavedEvent(const Event &);
    QSet<QString> subtitleSources(const Project &);
    SubtitleInterval subtitleInterval(const Project &,const SubtitleCue &,const QSet<QString> *sources=nullptr);
    QVector<LyricSpan> lyricSpans(const Project &,const QString &track);
    void setSubtitleTime(const Project &,SubtitleCue &,double start,double end,bool beats=false);
    void alignSubtitle(const Project &,SubtitleCue &,const QString &track,const LyricSpan &first,const LyricSpan &last);
    void validateSubtitleStyle(const SubtitleStyle &);
    void validateSubtitles(const Project &,bool checkOverlap=false);
    QJsonObject subtitlesJson(const Project &);
    void readSubtitles(Project &,const QJsonValue &);
    QVector<Event> generate(const Project &project,const std::atomic_bool *cancel=nullptr,Progress progress={});
    QVector<Event> resolveEvents(const QVector<Event> &events);
    const Event *eventAt(const QVector<Event> &events,double time,bool disjoint=false);
    enum class SegmentRole {Consonant,Vowel,Special};
    struct Pronunciation {
        QVector<QStringList> syllables;
        QVector<QVector<SegmentRole>> roles;
        QString provenance;
        bool unknown=false;
        QVector<QStringList> phones;
    };
    QStringList consonantKeys(const QString &language);
    QString consonantMode(const QString &key);
    QString consonantMode(const QString &key,const Rules::Consonants &);
    void validateConsonants(const Rules::Consonants &);
    QJsonObject consonantsJson(const Rules::Consonants &);
    Rules::Consonants readConsonants(const QJsonValue &);
    Pronunciation pronounce(const QString &text,const QString &language,bool explicitPhones=false,const QString &dictionary={},const QString &phoneset={},const PronunciationOptions &options={},const Rules::Consonants &consonants={});
    void saveProject(const Project &p,const QString &path);
    Project loadProject(const QString &path);
    // Providers propose a separate layer, never mutate a project or its edits.
    struct TimingProposal {
        QString eventId,source,provider,version,inputHash;
        double start=0,end=0,confidence=0;
    };
    struct TimingRequest {
        QVector<Event> events;
        QString audioPath,inputHash;
        double origin=0,begin=0,end=0;
    };
    class TimingRefiner {
        public: virtual ~TimingRefiner()=default;
        virtual QVector<TimingProposal> propose(const TimingRequest &,const std::atomic_bool &) const=0;
    };
    QVector<Event> applyTimingProposals(const Project &,const QVector<TimingProposal> &,const QString &inputHash);
    QString timingBasisHash(const Project &,const std::atomic_bool *cancel=nullptr);
    bool timingCorrectionCurrent(const Project &);
    void validateTimingCorrection(const Project &);
    QJsonObject timingCorrectionJson(const Project &);
    void readTimingCorrection(Project &,const QJsonValue &);
}
