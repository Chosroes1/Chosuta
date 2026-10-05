// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
namespace chosuta {
    // Explicit paths retain their meaning. Bare names prefer tools beside the
    // application, then let QProcess search the operating system's PATH.
    QString resolveExecutable(const QString &program);
}
