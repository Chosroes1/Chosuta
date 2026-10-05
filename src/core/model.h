// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtCore>
#include <QColor>
#include <functional>
#include <atomic>
#include <optional>
namespace chosuta {
    constexpr qint64 Blick = 705600000;
    constexpr qint64 MusicLimit = 9000000000000000LL;
    struct Failure : std::runtime_error {
        explicit Failure(const QString &s):std::runtime_error(s.toStdString()){}
    };
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
        QString path, code, message;
    };
    struct Note {
        QString id, lyrics, phonemes, language="ja", phoneset;
        qint64 onset=0, duration=0;
        int pitch=60;
        bool muted=false;
        QJsonObject original;
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
    };
    using Progress=std::function<void(int)>;
    Score importSvp(const QString &path, const std::atomic_bool *cancel=nullptr, Progress progress={});
    Score parseSvp(QByteArray bytes,const QString &path={},const std::atomic_bool *cancel=nullptr,Progress progress={});
    QJsonDocument checkedJson(QByteArray bytes,bool allowNul=false);
    struct Policy {
        QString mode="shape", shape="closed";
        double holdSeconds=.12;
    };
    struct Rules {
        QString language="auto";
        bool harmonyTakeover=true;
        double consonantRatio=.18;
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
    };
    struct Event {
        QString id, source, track, shape, provenance, text;
        double start=0,end=0;
        bool unknown=false;
    };
    struct Override {
        Event event;
        QString anchor="seconds";
        qint64 startBlick=0,endBlick=0;
        bool locked=true, deleted=false, standalone=false;
        QStringList parentIds;
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
    struct Project {
        Score score;
        QStringList selected;
        QMap<QString,int> priorities;
        Rules rules;
        QVector<Event> generated;
        QMap<QString,Override> overrides;
        QMap<QString,QString> assets;
        QString fallback="closed",audioPath;
        CanvasSettings canvas;
        ExportSettings output;
        double audioDuration=0,playbackReturnPosition=0;
        double duration() const;
        QVector<Event> effective() const;
        QStringList orphanOverrides() const;
        void regenerate(const std::atomic_bool *cancel=nullptr,Progress progress={});
        void edit(const Event &event,bool locked=true,QString anchor="seconds");
        void erase(const Event &event);
        void split(const Event &event,double position);
        void merge(const QVector<Event> &events);
    };
    QVector<Event> generate(const Project &project,const std::atomic_bool *cancel=nullptr,Progress progress={});
    QVector<Event> resolveEvents(const QVector<Event> &events);
    const Event *eventAt(const QVector<Event> &events,double time,bool disjoint=false);
    QString shapeAt(const Project &project,const QVector<Event> &events,double time,bool disjoint=false);
    struct Pronunciation {
        QVector<QStringList> syllables;
        QString provenance;
        bool unknown=false;
    };
    Pronunciation pronounce(const QString &text,const QString &language,bool explicitPhones=false,const QString &dictionary={});
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
}
