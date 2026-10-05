// SPDX-License-Identifier: GPL-3.0-or-later
#include "executable.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
namespace chosuta {
    QString resolveExecutable(const QString &program) {
        if(program.isEmpty())return program;
        if(program.contains('/')||program.contains('\\'))return QFileInfo(program).absoluteFilePath();
        QString name=program;
#ifdef Q_OS_WIN
        if(QFileInfo(name).suffix().isEmpty())name+=".exe";
#endif
        QFileInfo local(QDir(QCoreApplication::applicationDirPath()).filePath(name));
        if(local.isFile()&&local.isExecutable())return local.absoluteFilePath();
        return program;
    }
}
