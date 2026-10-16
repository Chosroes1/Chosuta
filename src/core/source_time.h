// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
namespace chosuta {
struct SourceSlice {QString source,track;qint64 start=0,end=0;};
QVector<SourceSlice> routeSources(const Project &,const std::atomic_bool *cancel=nullptr);
struct TimedSourceSlice {QString source,track;double start=0,end=0;};
QVector<TimedSourceSlice> routeTimedSources(const Project &,const std::atomic_bool *cancel=nullptr);
QVector<SourceFragment> eventSourceFragments(const Project &,const Event &);
class SourceTimeMap {
public:
    explicit SourceTimeMap(const Project &);
    bool contains(const QString &source)const{return sources.contains(source);}
    std::optional<double> forward(const QString &source,double raw,bool end=false)const;
    std::optional<double> inverse(const QString &source,double effective,bool end=false)const;
    bool ambiguous(const QString &source,double raw,bool end=false)const;
    QVector<Message> diagnostics;
private:
    struct Piece {double rawStart,rawEnd,start,end;};
    QHash<QString,QVector<Piece>> sources;
    QHash<QString,QVector<QPair<double,double>>> ambiguousRanges;
};
}
