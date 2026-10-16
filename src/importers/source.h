// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model.h"
namespace chosuta {
    qint64 sourceBlick(qint64 ticks,qint64 resolution);
    Score parseUst(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseUstx(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseVsq(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseVsqx(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseMxl(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseVpr(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseCcs(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseMusicXml(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseLab(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseDs(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseVvprojSong(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseVvprojAuto(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
    Score parseVvproj(const QByteArray &,const QString &,const std::atomic_bool *,Progress);
}
