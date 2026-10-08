// SPDX-License-Identifier: GPL-3.0-or-later
#include "window.h"
#include <QtConcurrent/QtConcurrentRun>
namespace chosuta {
Window::~Window(){
    if(cancel)cancel->store(true);
    waveformWatcher.waitForFinished();correctionWatcher.waitForFinished();loadWatcher.waitForFinished();audioWatcher.waitForFinished();exportWatcher.waitForFinished();
}
void Window::updateTimelineMinimum(){
    if(!timelineScroll||!timeline)return;
    const int fixedHeight=qRound(timeline->rulerRect().height()+timeline->mouthLaneRect().height()+timeline->waveformLaneRect().height());
    const int subtitleHeight=project.subtitlesEnabled&&!project.subtitles.isEmpty()?qRound(timeline->subtitleLaneRect(0).height())+4:16;
    // Reserve the viewport as well as its frame and horizontal scrollbar.
    timelineScroll->setMinimumHeight(fixedHeight+subtitleHeight+2*timelineScroll->frameWidth()+timelineScroll->horizontalScrollBar()->sizeHint().height());
}
void Window::refreshWaveform(){
    if(!timeline||!waveformControls)return;
    if(project.audioPath.isEmpty()){waveform.reset();waveformKey.clear();waveformMessage.clear();}
    waveformControls->setVisible(!project.audioPath.isEmpty());
    timeline->setWaveform(waveform,waveformMessage.isEmpty()?trText("Preparing waveform…"):waveformMessage);
    timingMaxShift->setValue(project.timing.maxShift*1000);timingMaxDuration->setValue(project.timing.maxDurationChange*100);
    QFileInfo info(project.audioPath);
    bool ready=waveform&&waveform->path==project.audioPath&&waveform->fileBytes==info.size()&&waveform->fileModified==info.lastModified().toMSecsSinceEpoch();
    correctTimingButton->setEnabled(ready&&!busy&&!playing&&!project.generated.isEmpty());
    revertTimingButton->setEnabled(!project.timing.sources.isEmpty()&&!busy&&!playing);
    reloadWaveformButton->setEnabled(!busy&&!playing);
    updateTimelineMinimum();
    if(!busy&&!project.audioPath.isEmpty())QTimer::singleShot(0,this,[this]{ensureWaveform();});
}
void Window::ensureWaveform(){
    if(busy||project.audioPath.isEmpty())return;
    QFileInfo info(project.audioPath);
    QString key=project.audioPath+"\n"+QString::number(info.size())+"\n"+QString::number(info.lastModified().toMSecsSinceEpoch())+"\n"+project.output.ffmpeg;
    if(waveformKey==key)return;
    waveformKey=key;waveform.reset();waveformMessage=trText("Preparing waveform…");timeline->setWaveform({},waveformMessage);
    cancel=std::make_shared<std::atomic_bool>(false);auto token=cancel;auto path=project.audioPath;auto executable=project.output.ffmpeg;double duration=project.audioDuration;
    setBusy(true);
    auto report=[this](int n){QMetaObject::invokeMethod(this,[this,n]{progress->setValue(n);},Qt::QueuedConnection);};
    waveformWatcher.setFuture(QtConcurrent::run([path,executable,token,report,duration]{return readWaveform(path,executable,*token,report,duration);}));
}
void Window::correctTiming(){
    if(busy||playing||!waveform||project.generated.isEmpty())return;
    auto snapshot=project;snapshot.timing.maxShift=timingMaxShift->value()/1000;snapshot.timing.maxDurationChange=timingMaxDuration->value()/100;
    cancel=std::make_shared<std::atomic_bool>(false);auto token=cancel;auto wave=waveform;
    setBusy(true);
    auto report=[this](int n){QMetaObject::invokeMethod(this,[this,n]{progress->setValue(n);},Qt::QueuedConnection);};
    correctionWatcher.setFuture(QtConcurrent::run([snapshot,wave,token,report]{
        // Limits apply to this new score-based pass, never to accumulated results.
        auto base=snapshot;base.timing.sources.clear();base.timing.enabled=false;
        return correctWaveform(base,*wave,*token,report);
    }));
}
void Window::revertTiming(){
    if(busy||playing||project.timing.sources.isEmpty())return;
    change(trText("Revert waveform correction"),[](Project &p){p.timing.sources.clear();p.timing.enabled=false;},true);
    statusBar()->showMessage(trText("Waveform correction reverted."));
}
}
