// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model.h"
namespace chosuta {
    qint64 sourceBlick(qint64 ticks,qint64 resolution);
    Score parseUst(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseVsqx(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
}
