// SPDX-License-Identifier: GPL-3.0-or-later
#include "executable.h"
#include "message.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <stdexcept>
namespace chosuta {
    QString defaultFfmpegExecutable() {
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,"Chosuta","Chosuta");
        return settings.value("tools/ffmpeg").toString();
    }
    void setDefaultFfmpegExecutable(const QString &program) {
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,"Chosuta","Chosuta");
        settings.setValue("tools/ffmpeg",program.trimmed());settings.sync();
        if(settings.status()!=QSettings::NoError)throw Failure("Cannot save FFmpeg preferences");
    }
    static QString findExecutable(const QString &selected,const QString &applicationDirectory) {
        if(selected.isEmpty())return selected;
        if(selected.contains('/')||selected.contains('\\'))return QFileInfo(selected).absoluteFilePath();
        QString name=selected;
#ifdef Q_OS_WIN
        if(QFileInfo(name).suffix().isEmpty())name+=".exe";
#endif
        QDir app(applicationDirectory.isEmpty()?QCoreApplication::applicationDirPath():applicationDirectory);
        if(selected=="ffmpeg"||selected=="ffmpeg.exe"||selected=="ffprobe"||selected=="ffprobe.exe") {
            QFileInfo bundled(app.filePath("tools/ffmpeg/"+name));
            if(bundled.isFile()&&bundled.isExecutable())return bundled.absoluteFilePath();
        }
        QFileInfo local(app.filePath(name));
        if(local.isFile()&&local.isExecutable())return local.absoluteFilePath();
        auto path=QStandardPaths::findExecutable(name);
        return path.isEmpty()?selected:path;
    }
    QString resolveExecutable(const QString &program,const QString &applicationDirectory) {
        auto selected=program;
        if(program=="ffmpeg"||program=="ffmpeg.exe"){
            const auto configured=defaultFfmpegExecutable();
            if(!configured.isEmpty())selected=configured;
        }
        return findExecutable(selected,applicationDirectory);
    }
    QString automaticFfmpegExecutable() {
        auto found=findExecutable("ffmpeg",{});
        if(QFileInfo(found).isAbsolute())return found;
#ifdef Q_OS_WIN
        const auto name="ffmpeg.exe";
#else
        const auto name="ffmpeg";
#endif
        // Absolute missing target avoids re-applying a saved override to an auto check.
        return QDir(QCoreApplication::applicationDirPath()).filePath(QString("tools/ffmpeg/")+name);
    }
    QString resolveFfprobe(const QString &ffmpeg) {
        const auto executable=resolveExecutable(ffmpeg);
        if(QFileInfo(executable).isAbsolute()) {
            const auto sibling=QFileInfo(executable).absoluteDir().filePath(
                "ffprobe"+QString(QFileInfo(executable).suffix().compare("exe",Qt::CaseInsensitive)==0?".exe":""));
            if(QFileInfo(sibling).isFile())return sibling;
        }
        const auto candidate=resolveExecutable("ffprobe");
        return QFileInfo(candidate).isAbsolute()?candidate:QStandardPaths::findExecutable(candidate);
    }
    MediaTools selectMediaTools(const QString &program){
        if(program.trimmed().isEmpty())throw Failure("Media tool path is empty");
        auto encoder=resolveExecutable(program);
        if(!QFileInfo(encoder).isAbsolute())encoder=QDir(QCoreApplication::applicationDirPath()).filePath("tools/ffmpeg/"+encoder);
        return MediaTools(encoder,resolveFfprobe(encoder));
    }
}
