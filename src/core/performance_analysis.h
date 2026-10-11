// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "performance.h"
namespace chosuta {
struct PerformanceInput {
    const Note *note=nullptr;
    QString track,tone;
    qint64 a=0,b=0;
    double start=0,end=0,x=0,y=0,pitch=0;
    int degree=0;
    bool named=false,hard=false,phrase=false,ending=false,breath=false,joined=false;
};
struct PerformanceAnalysis {
    QVector<PerformanceInput> notes;
    QVector<PerformanceBoundary> suggestions;
    QVector<int> motifEnd;
    double q10=0,q50=0,q75=0,q90=0;
    bool pitched=false,degraded=false;
};
QJsonObject performanceAnalysisJson(const Project &,const std::atomic_bool *cancel=nullptr);
PerformanceAnalysis analyzePerformance(const Project &,const std::atomic_bool *,Progress);
}
