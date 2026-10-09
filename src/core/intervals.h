// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "model.h"
namespace chosuta {
    struct WrittenPitch { int midi=0,staffPosition=0; QString name; };
    std::optional<WrittenPitch> parseWrittenPitch(QString name);
    const QMap<QString,QVector<int>> &degreeModes();
    void validateDegreeSettings(const Project::Appearance &);
    std::optional<WrittenPitch> writtenPitch(const Project::Appearance &,const Note &);
    std::optional<int> intervalDegree(const Project::Appearance &,const Note &from,const Note &to);
}
