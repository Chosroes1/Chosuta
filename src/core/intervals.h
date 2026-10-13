// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
namespace chosuta {
    void validateAnalysisSettings(const AnalysisSettings &);
    QJsonObject analysisSettingsJson(const AnalysisSettings &);
    AnalysisSettings readAnalysisSettings(const QJsonValue &);
    struct WrittenPitch { int midi=0,staffPosition=0; QString name; };
    std::optional<WrittenPitch> parseWrittenPitch(QString name);
    const QMap<QString,QVector<int>> &degreeModes();
    void validateDegreeSettings(const AnalysisSettings &);
    std::optional<WrittenPitch> writtenPitch(const AnalysisSettings &,const Note &);
}
