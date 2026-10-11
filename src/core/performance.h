// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
namespace chosuta {
    QString variantAssetId(const QString &group,const QString &shape);
    QString variantGroupName(const Project &,const QString &id);
    std::optional<QPair<QString,QString>> parseTonality(QString text);
    QString performanceBasisHash(const Project &,const std::atomic_bool *cancel=nullptr);
    bool performanceCurrent(const Project &);
    void validatePerformance(const Project &);
    QJsonObject performanceJson(const Project &);
    void readPerformance(Project &,const QJsonValue &);
    QJsonObject performanceSettingsJson(const PerformanceSettings &);
    PerformanceSettings readPerformanceSettings(const QJsonValue &);
    struct PerformanceDraft {
        QVector<PerformanceSegment> segments;
        QVector<PerformanceBoundary> suggestions;
        QString basisHash,error;
        bool cancelled=false;
    };
    PerformanceDraft generatePerformance(const Project &,const std::atomic_bool *cancel=nullptr,Progress progress={});
    struct PerformanceSlice {
        double start=0,end=0;
        QString group,id,reason;
        bool manual=false,protect=false,orphan=false;
    };
    class PerformanceResolver {
        public:
        explicit PerformanceResolver(const Project &,bool buildPlan=true);
        const QVector<PerformanceSlice> &slices()const{return plan;}
        QString groupAt(double videoTime)const;
        const PerformanceSlice *at(double videoTime)const;
        SubtitleInterval interval(const PerformanceSegment &)const;
        double mappedBlick(qint64 blick,const QString &track={},bool end=false)const;
        QStringList diagnostics;
        private:
        Project project;
        QVector<PerformanceSlice> plan;
        QHash<QString,QPair<double,double>> effectiveTimes;
        QHash<QString,const Note *> notes;
        QHash<QString,QString> sourceTracks;
        QMap<QString,QVector<const Note *>> tracks;
    };
    void setPerformanceTime(const Project &,PerformanceSegment &,double start,double end,bool beats);
    void putPerformanceSegment(Project &,PerformanceSegment); // Replace only the overlapping portion.
    void erasePerformanceRange(Project &,double start,double end,bool includeProtected);
    void applyPerformanceDraft(Project &,const PerformanceDraft &,double start=-1,double end=-1,bool restoreManual=false);
    void splitPerformance(Project &,const QString &id,double time);
    void removeVariantGroup(Project &,const QString &id,const QString &replacement);
}
