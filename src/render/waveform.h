// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "core/model.h"
#include <memory>
namespace chosuta {
struct WavePeak {float low=0,high=0;};
struct WaveBin {float low=0,high=0,rms=0;};
struct Waveform {
    QString path,hash;
    qint64 fileBytes=0,fileModified=0,samples=0;
    static constexpr double Step=.01;
    QVector<WaveBin> bins;
    QVector<QVector<WavePeak>> reduced;
    double duration()const {return samples/16000.;}
    WavePeak peaks(double begin,double end)const;
    qint64 storageBytes()const;
    void buildLevels();
};
struct WaveformResult {std::shared_ptr<const Waveform> wave;QString error;bool cancelled=false;};
WaveformResult readWaveform(const QString &path,const QString &ffmpeg,const std::atomic_bool &cancel,Progress progress={},double expectedDuration=0);
struct WaveCorrectionResult {
    TimingCorrection timing;
    QString error;
    bool cancelled=false;
    int considered=0,accepted=0,protectedSources=0,unclear=0,conflicts=0;
};
WaveCorrectionResult correctWaveform(const Project &,const Waveform &,const std::atomic_bool &,Progress progress={},const QStringList &selectedEvents={});
}
