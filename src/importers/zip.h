// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model.h"
#include <memory>
namespace chosuta {
// Read-only archive, backed by the preserved original bytes. Never extracts
// files to disk. VPR and subsequent MXL use the same member reader.
class ZipArchive {
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    explicit ZipArchive(const QByteArray &,const std::atomic_bool *cancel=nullptr);
    ~ZipArchive();
    bool contains(const QString &)const;
    QStringList names()const;
    QByteArray read(const QString &name,qint64 maxBytes,Progress progress={});
};
}
