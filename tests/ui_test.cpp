// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include "ui/window.h"
#include "core/executable.h"
using namespace chosuta;
static void writeWaveformFixture(const QString &path){
    QByteArray pcm;QDataStream data(&pcm,QIODevice::WriteOnly);data.setByteOrder(QDataStream::LittleEndian);
    for(int i=0;i<32000;++i){double local=std::fmod(i/16000.,.5);data<<qint16(local>=.05&&local<.45?(i%32<16?12000:-12000):0);}
    QFile f(path);if(!f.open(QIODevice::WriteOnly))throw Failure(f.errorString());QDataStream s(&f);s.setByteOrder(QDataStream::LittleEndian);
    s.writeRawData("RIFF",4);s<<quint32(36+pcm.size());s.writeRawData("WAVEfmt ",8);s<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(16000)<<quint32(32000)<<quint16(2)<<quint16(16);s.writeRawData("data",4);s<<quint32(pcm.size());s.writeRawData(pcm.constData(),pcm.size());
}
class UiTest:public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    private slots:
    void initTestCase(){QVERIFY(settings.isValid());QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());}
    void init(){savePreferences(Preferences{});}
    void preferencesAndTransport() {
        QCOMPARE(loadPreferences().language,QString("auto"));QVERIFY(!loadPreferences().returnOnPause);
        QCOMPARE(resolveUiLanguage("auto",{"ja-JP"}),QString("ja"));QCOMPARE(resolveUiLanguage("auto",{"zh-Hant-TW"}),QString("zh"));QCOMPARE(resolveUiLanguage("auto",{"de-DE"}),QString("en"));QCOMPARE(resolveUiLanguage("en",{"zh-CN"}),QString("en"));
        Window window;window.show();window.openPath(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QTRY_COMPARE(window.currentProject().generated.size(),4);
        auto position=window.findChild<QDoubleSpinBox*>("playPosition");auto apply=window.findChild<QPushButton*>("apply");apply->setFocus();
        QTest::keyClick(apply,Qt::Key_Space);QTRY_VERIFY(position->value()>.03);
        QTest::keyClick(apply,Qt::Key_Space);const double paused=position->value();QTest::qWait(80);QCOMPARE(position->value(),paused);QCOMPARE(window.currentProject().overrides.size(),0);
        QKeyEvent repeat(QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier,QString(),true,2);QApplication::sendEvent(apply,&repeat);QTest::qWait(40);QCOMPARE(position->value(),paused);
        QTest::keyClick(apply,Qt::Key_Space);QTRY_VERIFY(position->value()>paused+.03);QTest::keyClick(apply,Qt::Key_Space);
        // Preferences are persisted, and changing language rebuilds a translated UI.
        QTimer::singleShot(0,&window,[&]{auto dialog=window.findChild<QDialog*>("preferencesDialog");QVERIFY(dialog);dialog->findChild<QComboBox*>("uiLanguageChoice")->setCurrentIndex(1);dialog->findChild<QComboBox*>("pauseMode")->setCurrentIndex(1);dialog->findChild<QDoubleSpinBox*>("returnPosition")->setValue(.1);dialog->accept();});
        window.findChild<QAction*>("preferencesAction")->trigger();QCOMPARE(loadPreferences().language,QString("zh"));QVERIFY(loadPreferences().returnOnPause);QCOMPARE(window.currentProject().playbackReturnPosition,.1);
        position=window.findChild<QDoubleSpinBox*>("playPosition");apply=window.findChild<QPushButton*>("apply");position->setValue(.4);QTest::keyClick(apply,Qt::Key_Space);QTRY_VERIFY(position->value()>.43);QTest::keyClick(apply,Qt::Key_Space);QCOMPARE(position->value(),.1);
        // Space remains an ordinary character in editable lyric/path text fields.
        QLineEdit text(&window);text.setFocus();QTest::keyClick(&text,Qt::Key_Space);QCOMPARE(text.text(),QString(" "));QCOMPARE(position->value(),.1);
    }
    void advancedDictionaryWorkflow() {
        QTemporaryDir dir;
        QFile fixture(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QVERIFY(fixture.open(QIODevice::ReadOnly));
        auto root=checkedJson(fixture.readAll()).object();auto track=root["tracks"].toArray()[0].toObject();
        auto reference=track["mainRef"].toObject();reference["database"]=QJsonObject{{"language","english"},{"phoneset","arpabet"}};track["mainRef"]=reference;
        auto group=track["mainGroup"].toObject();auto notes=group["notes"].toArray();
        for(int i=0;i<notes.size();++i){auto note=notes[i].toObject();note["phonemes"]=i==0?"":"IY";if(i==0)note["lyrics"]="testword";notes[i]=note;}
        group["notes"]=notes;track["mainGroup"]=group;root["tracks"]=QJsonArray{track};
        const auto path=dir.filePath("custom.svp");QFile source(path);QVERIFY(source.open(QIODevice::WriteOnly));source.write(QJsonDocument(root).toJson());source.close();
        Window window;window.show();window.openPath(path);QTRY_COMPARE(window.currentProject().generated.size(),4);
        QCOMPARE(shapeAt(window.currentProject(),window.currentProject().effective(),.2),QString("unknown"));
        QTimer::singleShot(0,&window,[&] {
            auto dialog=window.findChild<QDialog*>("advancedSettingsDialog");QVERIFY(dialog);
            QCOMPARE(dialog->findChild<QTabWidget*>("advancedSettingsTabs")->currentIndex(),0);
            QVERIFY(!dialog->findChild<QCheckBox*>("japaneseKanji")->isChecked());
            dialog->findChild<QPushButton*>("dictionaryAdd")->click();
            auto table=dialog->findChild<QTableWidget*>("dictionaryTable");QCOMPARE(table->rowCount(),1);
            table->item(0,1)->setText("testword");table->item(0,3)->setText("UW");
            dialog->findChild<QCheckBox*>("japaneseKanji")->setChecked(true);
            dialog->findChild<QDialogButtonBox*>("advancedSettingsButtons")->button(QDialogButtonBox::Save)->click();
        });
        window.findChild<QAction*>("advancedSettingsAction")->trigger();
        QCOMPARE(window.currentProject().rules.pronunciation.words["en"]["testword"].text,QString("UW"));
        QCOMPARE(loadPreferences().pronunciation,window.currentProject().rules.pronunciation);
        window.findChild<QPushButton*>("generate")->click();
        QTRY_COMPARE(shapeAt(window.currentProject(),window.currentProject().effective(),.2),QString("U"));
        const auto saved=loadPreferences().pronunciation;
        QTimer::singleShot(0,&window,[&] {
            auto dialog=window.findChild<QDialog*>("advancedSettingsDialog");QVERIFY(dialog);
            auto table=dialog->findChild<QTableWidget*>("dictionaryTable");QCOMPARE(table->rowCount(),1);
            table->item(0,3)->setText("invalid-phone");
            auto buttons=dialog->findChild<QDialogButtonBox*>("advancedSettingsButtons");buttons->button(QDialogButtonBox::Save)->click();
            QVERIFY(dialog->isVisible());QVERIFY(dialog->findChild<QLabel*>("dictionaryStatus")->text().contains("Unrecognized"));
            buttons->button(QDialogButtonBox::Cancel)->click();
        });
        window.findChild<QAction*>("advancedSettingsAction")->trigger();QCOMPARE(loadPreferences().pronunciation,saved);
        const auto projectPath=dir.filePath("dictionary.chosuta");saveProject(window.currentProject(),projectPath);
        auto changed=loadPreferences();changed.pronunciation.words["en"]["testword"]={"IY",true};savePreferences(changed);
        Window restored;restored.show();restored.openPath(projectPath);
        QTRY_COMPARE(restored.currentProject().rules.pronunciation,saved);
        restored.findChild<QPushButton*>("generate")->click();QTRY_COMPARE(shapeAt(restored.currentProject(),restored.currentProject().effective(),.2),QString("U"));
        Window fresh;fresh.show();fresh.openPath(path);
        QTRY_COMPARE(shapeAt(fresh.currentProject(),fresh.currentProject().effective(),.2),QString("I"));
        // General preferences must not erase advanced defaults.
        QTimer::singleShot(0,&fresh,[&]{fresh.findChild<QDialog*>("preferencesDialog")->accept();});
        fresh.findChild<QAction*>("preferencesAction")->trigger();QCOMPARE(loadPreferences().pronunciation,changed.pronunciation);
        if(qEnvironmentVariableIsSet("CHOSUTA_ADVANCED_SCREENSHOT")) {
            QTimer::singleShot(0,&window,[&]{auto dialog=window.findChild<QDialog*>("advancedSettingsDialog");dialog->grab().save(qEnvironmentVariable("CHOSUTA_ADVANCED_SCREENSHOT"));dialog->reject();});
            window.findChild<QAction*>("advancedSettingsAction")->trigger();
        }
    }
    void canvasAndFollow() {
        Window window;window.show();window.openPath(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QTRY_COMPARE(window.currentProject().generated.size(),4);
        auto duration=window.findChild<QDoubleSpinBox*>("canvasDuration");duration->setValue(30);QMetaObject::invokeMethod(duration,"editingFinished");QCOMPARE(window.currentProject().duration(),30.);
        auto aspect=window.findChild<QComboBox*>("canvasAspect");aspect->setCurrentIndex(1);QCOMPARE(window.currentProject().canvas.width,720);QCOMPARE(window.currentProject().canvas.height,1280);
        auto scale=window.findChild<QDoubleSpinBox*>("characterScale");scale->setValue(40);window.findChild<QDoubleSpinBox*>("characterX")->setValue(70);QCOMPARE(window.currentProject().canvas.characterScale,.4);QCOMPARE(window.currentProject().canvas.characterX,.7);
        auto timeline=window.findChild<Timeline*>("timeline");auto scroll=window.findChild<QScrollArea*>("timelineScroll");auto position=window.findChild<QDoubleSpinBox*>("playPosition");timeline->setZoom(500);
        QTest::keyClick(window.findChild<QPushButton*>("apply"),Qt::Key_Space);position->setValue(12);
        QTRY_VERIFY(scroll->horizontalScrollBar()->value()>0);
        scroll->horizontalScrollBar()->setValue(0);QTRY_VERIFY(scroll->horizontalScrollBar()->value()>0);
        auto cursorVisible=[&]{int x=qRound(timeline->xAtTime(position->value()))-scroll->horizontalScrollBar()->value();return x>=0&&x<scroll->viewport()->width();};QVERIFY(cursorVisible());
        timeline->setBeats(true);QTRY_VERIFY(cursorVisible());QTest::keyClick(window.findChild<QPushButton*>("apply"),Qt::Key_Space);
        QTemporaryDir dir;auto path=dir.filePath("extended.chosuta");saveProject(window.currentProject(),path);auto p=loadProject(path);QCOMPARE(p.duration(),30.);QCOMPARE(p.canvas.characterScale,.4);QCOMPARE(p.canvas.characterX,.7);
        // Positioning beyond the score is allowed even without an audio file.
        position->setValue(25);QCOMPARE(position->value(),25.);
        duration->setValue(21600);QMetaObject::invokeMethod(duration,"editingFinished");timeline->setZoom(1000);QVERIFY(timeline->xAtTime(21600)<timeline->width());
        // Capture the new panel using only the original synthetic fixture.
        window.findChild<QPushButton*>("canvasButton")->click();
        if(qEnvironmentVariableIsSet("CHOSUTA_TEST_SCREENSHOT"))window.grab().save(qEnvironmentVariable("CHOSUTA_TEST_SCREENSHOT"));
    }
    void longerAudio() {
        if(QStandardPaths::findExecutable("ffmpeg").isEmpty()||QStandardPaths::findExecutable("ffprobe").isEmpty())QSKIP("FFmpeg/ffprobe absent");
        QTemporaryDir dir;auto audio=dir.filePath("音频 4s.wav");QProcess tone;tone.start("ffmpeg",{"-v","error","-f","lavfi","-i","sine=duration=4","-y",audio});QVERIFY(tone.waitForFinished());QCOMPARE(tone.exitCode(),0);
        for(bool extend:{false,true}){
            Window window;window.show();window.openPath(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QTRY_COMPARE(window.currentProject().generated.size(),4);const double original=window.currentProject().duration();
            QTimer answer;answer.setInterval(10);bool asked=false;connect(&answer,&QTimer::timeout,&window,[&]{auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());if(box&&box->standardButtons().testFlag(QMessageBox::Yes)){asked=true;box->button(extend?QMessageBox::Yes:QMessageBox::No)->click();}});answer.start();window.attachAudioPath(audio);
            QTRY_COMPARE_WITH_TIMEOUT(window.currentProject().audioPath,audio,5000);answer.stop();QVERIFY(asked);QVERIFY(std::abs(window.currentProject().audioDuration-4)<.001);QCOMPARE(window.currentProject().duration(),extend?4.:original);
        }
    }
    void subtitlesAndCanvasDrag() {
        QTemporaryDir dir;Project initial;QFile input(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QVERIFY(input.open(QIODevice::ReadOnly));initial.score=parseSvp(input.readAll());initial.selected={initial.score.tracks[0].id};initial.regenerate();initial.canvas.width=320;initial.canvas.height=180;
        QImage drawing(48,80,QImage::Format_RGBA8888);drawing.fill(Qt::red);auto asset=dir.filePath("closed.png");QVERIFY(drawing.save(asset));initial.assets[initial.fallback]=asset;auto path=dir.filePath("original.chosuta");saveProject(initial,path);
        Window window;window.show();window.openPath(path);QTRY_COMPARE(window.currentProject().generated.size(),4);QTest::qWait(40);
        auto enabled=window.findChild<QCheckBox*>("subtitlesEnabled");QVERIFY(enabled);QVERIFY(!enabled->isChecked());QVERIFY(!window.findChild<QWidget*>("subtitlePage"));QVERIFY(!window.findChild<QPlainTextEdit*>("subtitleText"));auto timeline=window.findChild<Timeline*>("timeline");QCOMPARE(timeline->height(),128);timeline->selectIds({window.currentProject().generated[0].id});QVERIFY(window.findChild<QLabel*>("selectionLabel")->text().contains("(1)"));
        enabled->setChecked(true);QCOMPARE(window.currentProject().subtitles.size(),1);QVERIFY(window.findChild<QWidget*>("subtitlePage"));QVERIFY(timeline->height()>128);
        QTest::mouseDClick(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(timeline->xAtTime(3)),qRound(timeline->subtitleLaneRect(0).top()+25)));QCOMPARE(window.currentProject().subtitles[0].cues.size(),1);QCOMPARE(window.currentProject().subtitles[0].cues[0].start,3.);QCOMPARE(window.currentProject().subtitles[0].cues[0].end,5.);QVERIFY(window.findChild<QLabel*>("selectionLabel")->text().contains("(0)"));
        auto text=window.findChild<QPlainTextEdit*>("subtitleText");text->setPlainText("手动字幕 / Hello\nかな");text->setFocus();double position=window.findChild<QDoubleSpinBox*>("playPosition")->value();QTest::keyClick(text,Qt::Key_Space);QCOMPARE(window.findChild<QDoubleSpinBox*>("playPosition")->value(),position);QCOMPARE(window.currentProject().subtitles[0].cues[0].text,text->toPlainText());
        // Save without Apply, then type again; undo must stop at the clean saved text.
        for(auto action:window.findChildren<QAction*>())if(action->shortcut()==QKeySequence::Save){action->trigger();break;}
        auto savedText=window.currentProject().subtitles[0].cues[0].text;QCOMPARE(loadProject(path).subtitles[0].cues[0].text,savedText);text->insertPlainText(" next");QVERIFY(window.currentProject().subtitles[0].cues[0].text!=savedText);
        for(auto action:window.findChildren<QAction*>())if(action->shortcut()==QKeySequence::Undo){action->trigger();break;}QCOMPARE(window.currentProject().subtitles[0].cues[0].text,savedText);
        text->setPlainText(QString(4097,'x'));QCOMPARE(window.currentProject().subtitles[0].cues[0].text,savedText);QCOMPARE(text->toPlainText(),savedText);
        window.findChild<QPushButton*>("applySubtitles")->click();QVERIFY(window.currentProject().subtitles[0].cues[0].text.contains("手动字幕"));
        enabled->setChecked(false);QCOMPARE(timeline->height(),128);QCOMPARE(window.currentProject().subtitles[0].cues.size(),1);enabled->setChecked(true);
        window.findChild<QPushButton*>("addSubtitleTrack")->click();QCOMPARE(window.currentProject().subtitles.size(),2);window.findChild<QCheckBox*>("subtitleAlign")->setChecked(true);
        QTest::mouseDClick(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(timeline->xAtTime(.25)),qRound(timeline->subtitleLaneRect(1).top()+25)));QCOMPARE(window.currentProject().subtitles[1].cues.size(),1);auto cue=window.currentProject().subtitles[1].cues[0];QCOMPARE(cue.anchor,QString("beats"));QCOMPARE(cue.start,0.);QCOMPARE(cue.end,.5);QCOMPARE(cue.endBlick,Blick);
        text->setPlainText("Translation");window.findChild<QPushButton*>("applySubtitles")->click();window.findChild<QComboBox*>("subtitleFirst")->setCurrentIndex(0);window.findChild<QComboBox*>("subtitleLast")->setCurrentIndex(1);window.findChild<QPushButton*>("alignSubtitleRange")->click();QCOMPARE(window.currentProject().subtitles[1].cues[0].end,1.);
        QPoint edge(qRound(timeline->xAtTime(1))-1,qRound(timeline->subtitleLaneRect(1).top()+25)),target(qRound(timeline->xAtTime(1.5))-1,qRound(timeline->subtitleLaneRect(1).top()+25));QTest::mousePress(timeline,Qt::LeftButton,Qt::NoModifier,edge);QTest::mouseMove(timeline,target);QTest::mouseRelease(timeline,Qt::LeftButton,Qt::NoModifier,target);QCOMPARE(window.currentProject().subtitles[1].cues[0].end,1.5);
        // Free timing with alignment off; movement into an occupied interval is rejected without altering either cue.
        window.findChild<QCheckBox*>("subtitleAlign")->setChecked(false);QTest::mouseDClick(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(timeline->xAtTime(2)),qRound(timeline->subtitleLaneRect(1).top()+25)));QCOMPARE(window.currentProject().subtitles[1].cues.size(),2);
        auto preserved=window.currentProject().subtitles[1].cues;QSignalSpy rejected(timeline,&Timeline::editRejected);QPoint middle(qRound(timeline->xAtTime(2.5)),qRound(timeline->subtitleLaneRect(1).top()+25)),overlap(qRound(timeline->xAtTime(.5)),qRound(timeline->subtitleLaneRect(1).top()+25));QTest::mousePress(timeline,Qt::LeftButton,Qt::NoModifier,middle);QTest::mouseMove(timeline,overlap);QTest::mouseRelease(timeline,Qt::LeftButton,Qt::NoModifier,overlap);QCOMPARE(rejected.size(),1);QCOMPARE(window.currentProject().subtitles[1].cues,preserved);
        Timeline fine;Project fineProject=initial;SubtitleTrack fineTrack;fineTrack.id="fine";fineTrack.alignLyrics=true;fineTrack.sourceTrack=initial.score.tracks[0].id;SubtitleCue fineCue;fineCue.id="fine-cue";fineCue.start=.1;fineCue.end=59./120;fineTrack.cues={fineCue};fineProject.subtitlesEnabled=true;fineProject.subtitles={fineTrack};fine.setProject(fineProject);const int fineRow=qRound(fine.subtitleLaneRect(0).top()+25);QSignalSpy fineEdits(&fine,&Timeline::subtitleEdited);QTest::mousePress(&fine,Qt::LeftButton,Qt::NoModifier,QPoint(78,fineRow));QTest::mouseMove(&fine,QPoint(79,fineRow));QTest::mouseRelease(&fine,Qt::LeftButton,Qt::NoModifier,QPoint(79,fineRow));QCOMPARE(fineEdits.size(),1);QCOMPARE(qvariant_cast<SubtitleCue>(fineEdits[0][1]).end,.5);
        auto preview=window.findChild<PreviewCanvas*>("preview");auto layout=window.findChild<QComboBox*>("layoutTarget");layout->setCurrentIndex(0);QTest::qWait(30);auto before=window.currentProject().canvas;auto box=preview->selectionRect();QVERIFY(!box.isEmpty());auto center=box.center().toPoint();
        QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseMove(preview,center+QPoint(10,5));QTest::mouseMove(preview,center+QPoint(30,15));QCOMPARE(window.currentProject().canvas.characterX,before.characterX);QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,center+QPoint(30,15));QVERIFY(window.currentProject().canvas.characterX>before.characterX);
        auto undo=[&]{for(auto a:window.findChildren<QAction*>())if(a->shortcut()==QKeySequence::Undo&&a->isEnabled()){a->trigger();return true;}return false;};QVERIFY(undo());QCOMPARE(window.currentProject().canvas.characterX,before.characterX);QCOMPARE(window.currentProject().canvas.characterY,before.characterY);
        center=preview->selectionRect().center().toPoint();QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseMove(preview,center+QPoint(20,0));QTest::keyClick(preview,Qt::Key_Escape);QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,center+QPoint(20,0));QCOMPARE(window.currentProject().canvas.characterX,before.characterX);
        // Select an active subtitle and drag its static axis layout on the same preview.
        window.findChild<QDoubleSpinBox*>("playPosition")->setValue(.25); // Selection no longer changes transport time.
        QTest::mouseClick(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(timeline->xAtTime(.25)),qRound(timeline->subtitleLaneRect(1).top()+25)));layout->setCurrentIndex(layout->findData(window.currentProject().subtitles[1].id));QTest::qWait(20);box=preview->selectionRect();QVERIFY(!box.isEmpty());center=box.center().toPoint();double previousY=window.currentProject().subtitles[1].style.y;
        QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseMove(preview,center-QPoint(0,20));QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,center-QPoint(0,20));QVERIFY(window.currentProject().subtitles[1].style.y<previousY);
        // Side and corner handles remain distinguishable even for a single short line.
        box=preview->selectionRect();auto side=QPoint(qRound(box.right()),qRound(box.center().y()));double previousWidth=window.currentProject().subtitles[1].style.width;
        QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,side);QTest::mouseMove(preview,side-QPoint(20,0));QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,side-QPoint(20,0));QVERIFY(window.currentProject().subtitles[1].style.width<previousWidth);
        box=preview->selectionRect();auto corner=box.bottomRight().toPoint();double previousFont=window.currentProject().subtitles[1].style.fontHeight;
        QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,corner);QTest::mouseMove(preview,corner+QPoint(20,10));QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,corner+QPoint(20,10));QVERIFY(window.currentProject().subtitles[1].style.fontHeight>previousFont);
        window.findChild<QCheckBox*>("subtitleOwnStyle")->setChecked(true);window.findChild<QPushButton*>("applySubtitles")->click();auto axisStyle=window.currentProject().subtitles[1].style;
        box=preview->selectionRect();center=box.center().toPoint();QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseMove(preview,center+QPoint(20,0));QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,center+QPoint(20,0));QCOMPARE(window.currentProject().subtitles[1].style,axisStyle);QVERIFY(window.currentProject().subtitles[1].cues[0].ownStyle);QVERIFY(window.currentProject().subtitles[1].cues[0].style.x>axisStyle.x);
        layout->setCurrentIndex(0);box=preview->selectionRect();corner=box.bottomRight().toPoint()-QPoint(1,1);double previousScale=window.currentProject().canvas.characterScale;
        QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,corner);QTest::mouseMove(preview,corner-QPoint(15,15));QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,corner-QPoint(15,15));QVERIFY(window.currentProject().canvas.characterScale<previousScale);
        // Resize the window/letterboxed canvas; coordinate mapping uses its actual displayed rectangle.
        window.resize(1400,1000);QTest::qWait(30);box=preview->selectionRect();center=box.center().toPoint();double oldX=window.currentProject().canvas.characterX;double expectedX=oldX+20/preview->canvasRect().width();
        QTest::mousePress(preview,Qt::LeftButton,Qt::NoModifier,center);QTest::mouseMove(preview,center+QPoint(20,0));QTest::mouseRelease(preview,Qt::LeftButton,Qt::NoModifier,center+QPoint(20,0));QVERIFY(std::abs(window.currentProject().canvas.characterX-expectedX)<1e-6);
        auto duration=window.findChild<QDoubleSpinBox*>("canvasDuration");
        for(auto answer:{QMessageBox::Cancel,QMessageBox::No,QMessageBox::Yes}){duration->setValue(1);QMetaObject::invokeMethod(duration,"editingFinished");bool asked=false;QTimer response;response.setInterval(10);connect(&response,&QTimer::timeout,&window,[&]{if(auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget())){asked=true;box->button(answer)->click();}else if(auto dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget()))dialog->reject();});response.start();for(auto action:window.findChildren<QAction*>())if(action->shortcut()==QKeySequence(Qt::CTRL|Qt::Key_E)){action->trigger();break;}response.stop();QVERIFY(asked);QCOMPARE(window.currentProject().duration(),answer==QMessageBox::Yes?5.:1.);}
        window.findChild<QPushButton*>("extendToSubtitles")->click();QCOMPARE(window.currentProject().duration(),5.);
        // Switching to playback aborts the uncommitted timeline drag.
        auto captions=window.currentProject().subtitles;middle=QPoint(qRound(timeline->xAtTime(.25)),qRound(timeline->subtitleLaneRect(1).top()+25));QTest::mousePress(timeline,Qt::LeftButton,Qt::NoModifier,middle);QTest::mouseMove(timeline,middle+QPoint(15,0));QTest::keyClick(timeline,Qt::Key_Space);QTest::mouseRelease(timeline,Qt::LeftButton,Qt::NoModifier,middle+QPoint(15,0));QTest::keyClick(timeline,Qt::Key_Space);QCOMPARE(window.currentProject().subtitles,captions);
        Scene sharedGeometry(window.currentProject());PreviewCanvas direct;direct.resize(700,300);direct.setState(window.currentProject(),&sharedGeometry,.25,true);auto &styled=window.currentProject().subtitles[1];direct.setTarget(styled.id);QVERIFY(direct.selectionRect().isEmpty());direct.setTarget(styled.id,styled.cues[0].id);QVERIFY(!direct.selectionRect().isEmpty());
        auto saved=dir.filePath("saved-subtitles.chosuta");saveProject(window.currentProject(),saved);auto restored=loadProject(saved);QCOMPARE(restored.subtitles,window.currentProject().subtitles);QVERIFY(restored.subtitlesEnabled);
        Window reopened;reopened.openPath(saved);QTRY_COMPARE(reopened.currentProject().subtitles.size(),2);QVERIFY(reopened.findChild<QCheckBox*>("subtitlesEnabled")->isChecked());
        if(qEnvironmentVariableIsSet("CHOSUTA_SUBTITLE_SCREENSHOT")){window.findChild<QComboBox*>("subtitleTrackChoice")->setCurrentIndex(1);layout->setCurrentIndex(layout->findData(window.currentProject().subtitles[1].id));window.statusBar()->clearMessage();window.grab().save(qEnvironmentVariable("CHOSUTA_SUBTITLE_SCREENSHOT"));auto page=window.findChild<QScrollArea*>("subtitlePage");page->verticalScrollBar()->setValue(page->verticalScrollBar()->maximum());QCoreApplication::processEvents();auto styles=qEnvironmentVariable("CHOSUTA_SUBTITLE_SCREENSHOT");styles.replace(".png","-style.png");window.grab().save(styles);}
    }
    void subtitleFocusCommandsAndRuler() {
        QTemporaryDir dir;Project p;QFile input(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QVERIFY(input.open(QIODevice::ReadOnly));p.score=parseSvp(input.readAll());p.selected={p.score.tracks[0].id};p.regenerate();p.canvas.width=320;p.canvas.height=180;
        QImage asset(48,80,QImage::Format_RGBA8888);asset.fill(Qt::red);auto imagePath=dir.filePath("closed.png");QVERIFY(asset.save(imagePath));p.assets[p.fallback]=imagePath;
        SubtitleTrack track;track.id="caption-track";track.name="Manual";SubtitleCue c;c.id="first";c.text="abc";c.start=.1;c.end=.8;auto second=c;second.id="second";second.start=1;second.end=1.8;track.cues={c,second};p.subtitles={track};p.subtitlesEnabled=true;auto path=dir.filePath("focus.chosuta");saveProject(p,path);
        Window w;w.resize(1200,850);w.show();w.openPath(path);QTRY_COMPARE(w.currentProject().subtitles.size(),1);QTest::qWait(30);auto t=w.findChild<Timeline*>("timeline");auto position=w.findChild<QDoubleSpinBox*>("playPosition");auto text=w.findChild<QPlainTextEdit*>("subtitleText");QVERIFY(text);QVERIFY(!text->isVisible());QSignalSpy seeks(t,&Timeline::seek);
        auto click=[&](double seconds,int y){QTest::mouseClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(seconds)),y));};int row=qRound(t->subtitleLaneRect(0).top()+21);
        position->setValue(1.9);click(.3,row);QCOMPARE(position->value(),1.9);QCOMPARE(QApplication::focusWidget(),static_cast<QWidget*>(t));QCOMPARE(seeks.size(),0);
        click(.3,qRound(t->subtitleLaneRect(0).bottom()-8));QTest::keyClick(t,Qt::Key_Delete);QCOMPARE(w.currentProject().subtitles[0].cues.size(),2); // Padding is blank, rather than an invisible extension of the block.
        click(2.5,85);click(2.5,150);click(2.5,row);QCOMPARE(position->value(),1.9);QCOMPARE(seeks.size(),0);
        click(.25,qRound(t->rulerRect().center().y()));QCOMPARE(position->value(),.25);QCOMPARE(seeks.size(),1);
        // The unmodified full-height playhead is visible in the ruler, mouth lane and subtitle lane.
        auto shot=t->grab().toImage();int x=qRound(t->xAtTime(.25));for(int y:{15,85,row})QCOMPARE(shot.pixelColor(qRound(x*shot.devicePixelRatio()),qRound(y*shot.devicePixelRatio())),QColor("#d74242"));
        QTest::mouseDClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(.3)),row));QVERIFY(t->editingText());QVERIFY(w.findChild<QWidget*>("subtitleCuePanel")->isVisible());QCOMPARE(text->parentWidget(),static_cast<QWidget*>(t));QVERIFY(text->isVisible());QCOMPARE(t->findChildren<QPlainTextEdit*>().size(),1);QCOMPARE(QApplication::focusWidget(),static_cast<QWidget*>(text));QVERIFY(text->cursorRect().intersects(text->viewport()->rect()));
        auto editorShot=text->viewport()->grab().toImage();int editorX=qRound(t->xAtTime(.25))-text->viewport()->mapTo(t,QPoint(0,0)).x();QCOMPARE(editorShot.pixelColor(qRound(editorX*editorShot.devicePixelRatio()),qRound(8*editorShot.devicePixelRatio())),QColor("#d74242"));
        QTest::keyClicks(text,"X ");QCOMPARE(w.currentProject().subtitles[0].cues[0].text,QString("abcX "));QCOMPARE(position->value(),.25);auto cursor=text->textCursor();cursor.setPosition(0);text->setTextCursor(cursor);QTest::keyClick(text,Qt::Key_Delete);QCOMPARE(w.currentProject().subtitles[0].cues[0].text,QString("bcX "));QCOMPARE(w.currentProject().subtitles[0].cues.size(),2);
        QTest::keySequence(text,QKeySequence::Undo);QCOMPARE(w.currentProject().subtitles[0].cues[0].text,QString("abc"));QTest::keySequence(text,QKeySequence::Redo);QCOMPARE(w.currentProject().subtitles[0].cues[0].text,QString("bcX "));
        QTest::keySequence(text,QKeySequence::Save);auto saved=w.currentProject().subtitles[0].cues[0].text;QCOMPARE(loadProject(path).subtitles[0].cues[0].text,saved);QTest::keyClicks(text,"Y");QTest::keySequence(text,QKeySequence::Undo);QCOMPARE(w.currentProject().subtitles[0].cues[0].text,saved);
        QInputMethodEvent preedit(QString::fromUtf8("に"),{});QApplication::sendEvent(text,&preedit);QCOMPARE(w.currentProject().subtitles[0].cues[0].text,saved);QInputMethodEvent commit;commit.setCommitString(QString::fromUtf8("日本"));QApplication::sendEvent(text,&commit);QVERIFY(w.currentProject().subtitles[0].cues[0].text.contains("日本"));auto typed=w.currentProject().subtitles[0].cues[0].text;
        click(2.5,row);QVERIFY(!t->editingText());QVERIFY(!text->isVisible());QCOMPARE(position->value(),.25);QCOMPARE(w.currentProject().subtitles[0].cues[0].text,typed);QCOMPARE(QApplication::focusWidget(),static_cast<QWidget*>(t));
        QTest::keyClick(t,Qt::Key_Space);QTRY_VERIFY(position->value()>.28);QTest::keyClick(t,Qt::Key_Delete);QCOMPARE(w.currentProject().subtitles[0].cues.size(),2);QTest::keyClick(t,Qt::Key_Space);
        click(.3,row);QTest::keyClick(t,Qt::Key_Delete);QCOMPARE(w.currentProject().subtitles[0].cues.size(),1);QTest::keySequence(t,QKeySequence::Undo);QCOMPARE(w.currentProject().subtitles[0].cues.size(),2);QTest::keySequence(t,QKeySequence::Redo);QCOMPARE(w.currentProject().subtitles[0].cues.size(),1);QTest::keySequence(t,QKeySequence::Undo);
        position->setValue(.25);click(.3,row);auto preview=w.findChild<PreviewCanvas*>("preview");preview->setFocus();QTest::keyClick(preview,Qt::Key_Backspace);QCOMPARE(w.currentProject().subtitles[0].cues.size(),1);QTest::keySequence(t,QKeySequence::Undo);
        click(.3,row);QTest::mouseClick(preview,Qt::LeftButton,Qt::NoModifier,QPoint(1,1));QCOMPARE(position->value(),.25);QTest::keyClick(preview,Qt::Key_Delete);QCOMPARE(w.currentProject().subtitles[0].cues.size(),2);
        click(.2,85);QTest::keyClick(t,Qt::Key_Delete);QCOMPARE(w.currentProject().effective().size(),3);QTest::keySequence(t,QKeySequence::Undo);QCOMPARE(w.currentProject().effective().size(),4);
        position->setValue(.25);QTest::mouseDClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(3)),row));QCOMPARE(w.currentProject().subtitles[0].cues.size(),3);QCOMPARE(position->value(),.25);QVERIFY(t->editingText());QTest::keyClick(text,Qt::Key_Escape);QVERIFY(!t->editingText());
        QPoint start(qRound(t->xAtTime(.5)),15),end(qRound(t->xAtTime(1)),15);QTest::mousePress(t,Qt::LeftButton,Qt::NoModifier,start);QTest::mouseMove(t,end);QTest::mouseRelease(t,Qt::LeftButton,Qt::NoModifier,end);QCOMPARE(position->value(),1.);
        if(qEnvironmentVariableIsSet("CHOSUTA_INTERACTION_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_INTERACTION_ARTIFACTS"));QVERIFY(out.mkpath("."));QVERIFY(w.grab().save(out.filePath("interaction-ui.png")));QTest::mouseDClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(.3)),row));QVERIFY(t->editingText());QCoreApplication::processEvents();QVERIFY(w.grab().save(out.filePath("inline-editor.png")));}
    }
    void timelineHeightsAndEditorGeometry() {
        QTemporaryDir dir;Project p;QFile input(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QVERIFY(input.open(QIODevice::ReadOnly));p.score=parseSvp(input.readAll());p.selected={p.score.tracks[0].id};p.regenerate();p.subtitlesEnabled=true;
        for(int i=0;i<4;++i){SubtitleTrack t;t.id=QString("track%1").arg(i);t.name=t.id;SubtitleCue c;c.id=QString("cue%1").arg(i);c.text="Hello 中文 かな";c.start=.1;c.end=.8;t.cues={c};p.subtitles.append(t);}auto path=dir.filePath("heights.chosuta");saveProject(p,path);Window w;w.resize(1200,1100);w.show();w.openPath(path);QTRY_COMPARE(w.currentProject().subtitles.size(),4);QTest::qWait(30);
        auto t=w.findChild<Timeline*>("timeline");auto scroll=w.findChild<QScrollArea*>("timelineScroll");auto position=w.findChild<QDoubleSpinBox*>("playPosition");auto before=w.currentProject().subtitles;
        QCOMPARE(t->mouthLaneRect().height(),96.);
        // Compact padding must remain painted and selectable with the same geometry.
        auto shot=t->grab().toImage();const int x=qRound(t->xAtTime(.35));
        for(int y:{qRound(t->mouthLaneRect().top()+10),qRound(t->mouthLaneRect().bottom()-28)}){
            QCOMPARE(shot.pixelColor(qRound(x*shot.devicePixelRatio()),qRound(y*shot.devicePixelRatio())),QColor("#ed7373"));
            QTest::mouseClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(x,y));QCOMPARE(t->selectedIds(),QStringList{w.currentProject().generated[0].id});
        }
        auto drag=[&](QPoint from,QPoint to){QTest::mousePress(t,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(t,to);QTest::mouseRelease(t,Qt::LeftButton,Qt::NoModifier,to);};
        int bottom=qRound(t->mouthLaneRect().bottom()-2);drag(QPoint(20,bottom),QPoint(20,bottom+40));QCOMPARE(t->mouthLaneRect().height(),136.);QCOMPARE(loadPreferences().mouthLaneHeight,136);
        bottom=qRound(t->subtitleLaneRect(0).bottom()-3);drag(QPoint(20,bottom),QPoint(20,bottom+50));QCOMPARE(t->subtitleLaneRect(0).height(),110.);QCOMPARE(t->subtitleLaneRect(1).height(),60.);QCOMPARE(w.currentProject().subtitles,before);QCOMPARE(position->value(),0.);
        QCoreApplication::processEvents();
        auto split=w.findChild<QSplitter*>("timelineSplitter");auto handle=split->handle(1);auto local=handle->rect().center();auto target=handle->mapToGlobal(local)+QPoint(0,-40);int oldHeight=scroll->height();QTest::mousePress(handle,Qt::LeftButton,Qt::NoModifier,local);QTest::mouseMove(handle,handle->mapFromGlobal(target));QTest::mouseRelease(handle,Qt::LeftButton,Qt::NoModifier,handle->mapFromGlobal(target));QVERIFY(scroll->height()>oldHeight);QCOMPARE(loadPreferences().timelineHeight,scroll->height());
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());QCoreApplication::processEvents();auto ruler=t->rulerRect();QTest::mouseClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(.5)),qRound(ruler.center().y())));QCOMPARE(position->value(),.5);
        QTest::mouseDClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(.3)),qRound(t->subtitleLaneRect(3).top()+22)));auto text=w.findChild<QPlainTextEdit*>("subtitleText");QVERIFY(t->editingText());QVERIFY(text->isVisible());QVERIFY(text->geometry().intersects(QRect(t->mapFrom(scroll->viewport(),QPoint(0,0)),scroll->viewport()->size())));t->setZoom(240);t->setBeats(true);QVERIFY(t->editingText());QVERIFY(text->cursorRect().intersects(text->viewport()->rect()));
        QTest::keyClicks(text,"!");QCOMPARE(w.currentProject().subtitles.last().cues[0].text,QString("Hello 中文 かな!"));QTest::keyClick(text,Qt::Key_Escape);QCOMPARE(position->value(),.5);w.findChild<QPushButton*>("resetTimelineHeights")->click();QCOMPARE(t->mouthLaneRect().height(),96.);QCOMPARE(t->subtitleLaneRect(0).height(),60.);QCOMPARE(loadPreferences().subtitleLaneHeight,60);
        Window restored;restored.openPath(path);QTRY_COMPARE(restored.currentProject().subtitles.size(),4);QCOMPARE(restored.findChild<Timeline*>("timeline")->mouthLaneRect().height(),96.);
    }
    void waveformWorkflow(){
        if(!QFileInfo::exists(resolveExecutable("ffmpeg"))&&QStandardPaths::findExecutable("ffmpeg").isEmpty())QSKIP("FFmpeg unavailable");
        QTemporaryDir dir;auto audio=dir.filePath("波形 test.wav");writeWaveformFixture(audio);
        Project p;QFile input(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QVERIFY(input.open(QIODevice::ReadOnly));p.score=parseSvp(input.readAll());p.selected={p.score.tracks[0].id};p.regenerate();p.subtitlesEnabled=true;
        SubtitleTrack track;track.id="caption";track.name="Caption";SubtitleCue cue;cue.id="text";cue.text="Waveform timing 中文 かな";cue.start=.1;cue.end=.8;track.cues={cue};p.subtitles={track};auto path=dir.filePath("wave.chosuta");saveProject(p,path);
        Window w;w.resize(1280,1100);w.show();w.openPath(path);QTRY_COMPARE(w.currentProject().generated.size(),4);
        auto t=w.findChild<Timeline*>("timeline");auto correct=w.findChild<QPushButton*>("correctWaveformTiming");auto revert=w.findChild<QPushButton*>("revertWaveformTiming");auto shift=w.findChild<QDoubleSpinBox*>("timingMaxShift");auto position=w.findChild<QDoubleSpinBox*>("playPosition");
        QVERIFY(w.findChild<QWidget*>("waveformControls")->isHidden());QCOMPARE(t->waveformLaneRect().height(),0.);
        w.attachAudioPath(audio);QTRY_VERIFY(correct->isEnabled());QCOMPARE(w.currentProject().effective()[0].start,0.);QVERIFY(w.currentProject().timing.sources.isEmpty());QCOMPARE(w.currentProject().audioPath,audio);
        QCOMPARE(t->waveformLaneRect().height(),80.);QCOMPARE(t->waveformLaneRect().top(),t->rulerRect().bottom());QCOMPARE(t->mouthLaneRect().top(),t->waveformLaneRect().bottom());auto oldSub=t->subtitleLaneRect(0).top();
        auto from=QPoint(20,qRound(t->waveformLaneRect().bottom()-2)),to=from+QPoint(0,40);QTest::mousePress(t,Qt::LeftButton,Qt::NoModifier,from);QTest::mouseMove(t,to);QTest::mouseRelease(t,Qt::LeftButton,Qt::NoModifier,to);
        QCOMPARE(t->waveformLaneRect().height(),120.);QCOMPARE(loadPreferences().waveformLaneHeight,120);QCOMPARE(t->subtitleLaneRect(0).top(),oldSub+40);QCOMPARE(position->value(),0.);QCOMPARE(w.currentProject().effective()[0].start,0.);
        // Height and waveform clicks do not change time or seek. The ruler remains the sole seek area.
        auto scroll=w.findChild<QScrollArea*>("timelineScroll");const int minimumBefore=scroll->minimumHeight();
        // Exercise the smallest allowed viewport, where a dropped wave height hides captions.
        w.resize(1280,860);w.findChild<QSplitter*>("timelineSplitter")->setSizes({10000,1});QCoreApplication::processEvents();scroll->verticalScrollBar()->setValue(0);
        const auto subtitlesBefore=w.currentProject().subtitles;
        const auto captionVisible=[&]{return t->mapTo(scroll->viewport(),QPoint(0,qRound(t->subtitleLaneRect(0).bottom()))).y()<=scroll->viewport()->height();};
        QVERIFY(captionVisible());
        QTest::mouseClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(.5)),qRound(t->waveformLaneRect().center().y())));QCOMPARE(position->value(),0.);
        QCOMPARE(scroll->minimumHeight(),minimumBefore);
        QCoreApplication::processEvents();QVERIFY(captionVisible());QVERIFY(w.currentProject().subtitlesEnabled);QCOMPARE(w.currentProject().subtitles,subtitlesBefore);
        // Selecting the caption and then the mouth lane uses the same viewport constraint.
        QTest::mouseClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(.3)),qRound(t->subtitleLaneRect(0).top()+22)));
        QCOMPARE(scroll->minimumHeight(),minimumBefore);QVERIFY(captionVisible());
        QTest::mouseClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(.3)),qRound(t->mouthLaneRect().top()+12)));
        QCOMPARE(scroll->minimumHeight(),minimumBefore);QVERIFY(captionVisible());
        QTest::mouseClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(.5)),qRound(t->rulerRect().center().y())));QCOMPARE(position->value(),.5);
        double x=t->xAtTime(.5);t->setBeats(true);QVERIFY(std::abs(t->timeAtX(t->xAtTime(.55))-.55)<1e-8);t->setBeats(false);QCOMPARE(t->xAtTime(.5),x);
        shift->setValue(20);QVERIFY(w.currentProject().timing.sources.isEmpty());correct->click();QTRY_VERIFY(w.centralWidget()->isEnabled());QVERIFY(w.currentProject().timing.sources.isEmpty());QCOMPARE(w.currentProject().effective()[0].start,0.);
        shift->setValue(100);correct->click();QTRY_COMPARE(w.currentProject().timing.sources.size(),4);QVERIFY(std::abs(w.currentProject().effective()[0].start-.05)<1e-9);QCOMPARE(w.currentProject().generated[0].start,0.);QVERIFY(revert->isEnabled());
        auto accepted=w.currentProject().effective()[0].start;correct->click();QTRY_VERIFY(w.centralWidget()->isEnabled());QCOMPARE(w.currentProject().effective()[0].start,accepted);
        // Cancelling display preparation retains the previously adopted correction.
        w.findChild<QPushButton*>("reloadWaveform")->click();w.findChild<QPushButton*>("cancelTask")->click();QTRY_VERIFY(w.centralWidget()->isEnabled());QCOMPARE(w.currentProject().effective()[0].start,accepted);QVERIFY(!correct->isEnabled());
        w.findChild<QPushButton*>("reloadWaveform")->click();QTRY_VERIFY(correct->isEnabled());QCOMPARE(w.currentProject().effective()[0].start,accepted);
        saveProject(w.currentProject(),path);Window restored;restored.resize(1280,1100);restored.show();restored.openPath(path);QTRY_VERIFY(restored.findChild<QPushButton*>("correctWaveformTiming")->isEnabled());QCOMPARE(restored.currentProject().effective()[0].start,accepted);QCOMPARE(restored.findChild<Timeline*>("timeline")->waveformLaneRect().height(),120.);
        w.activateWindow();QTRY_COMPARE(QApplication::activeWindow(),&w);revert->click();QVERIFY(w.currentProject().timing.sources.isEmpty());QCOMPARE(w.currentProject().effective()[0].start,0.);auto apply=w.findChild<QPushButton*>("apply");apply->setFocus();QTest::keyClick(apply,Qt::Key_Z,Qt::ControlModifier);QCOMPARE(w.currentProject().effective()[0].start,accepted);
        // Re-reading checks content too, even if a replacement preserves file size/time.
        QFile replacement(audio);QVERIFY(replacement.open(QIODevice::ReadOnly));auto originalBytes=replacement.readAll();replacement.close();auto stamp=QFileInfo(audio).lastModified();auto changedBytes=originalBytes;changedBytes[changedBytes.size()-1]=char(changedBytes.back()^1);
        auto replace=[&](const QByteArray &bytes){if(!replacement.open(QIODevice::WriteOnly))return false;bool ok=replacement.write(bytes)==bytes.size();ok=replacement.setFileTime(stamp,QFileDevice::FileModificationTime)&&ok;replacement.close();return ok;};QVERIFY(replace(changedBytes));
        w.findChild<QPushButton*>("reloadWaveform")->click();QTRY_VERIFY(correct->isEnabled());QVERIFY(!timingCorrectionCurrent(w.currentProject()));QCOMPARE(w.currentProject().effective()[0].start,0.);
        QVERIFY(replace(originalBytes));w.findChild<QPushButton*>("reloadWaveform")->click();QTRY_VERIFY(correct->isEnabled());QVERIFY(timingCorrectionCurrent(w.currentProject()));QCOMPARE(w.currentProject().effective()[0].start,accepted);
        // A protected manual event survives correction and reverting the independent timing layer.
        t->selectIds({w.currentProject().generated[0].id});auto eventShape=w.findChild<QComboBox*>("eventShape");QVERIFY(eventShape);eventShape->setCurrentText("O");apply->click();correct->click();QTRY_VERIFY(w.centralWidget()->isEnabled());QVERIFY(!w.currentProject().timing.sources.contains(w.currentProject().generated[0].source));revert->click();QCOMPARE(shapeAt(w.currentProject(),w.currentProject().effective(),.2),QString("O"));
        QTest::mouseDClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(.3)),qRound(t->subtitleLaneRect(0).top()+22)));QVERIFY(t->editingText());auto editor=w.findChild<QPlainTextEdit*>("subtitleText");QVERIFY(editor->isVisible());QVERIFY(editor->cursorRect().intersects(editor->viewport()->rect()));QTest::keyClick(editor,Qt::Key_Escape);
        w.findChild<QPushButton*>("resetTimelineHeights")->click();QCOMPARE(t->waveformLaneRect().height(),80.);QCOMPARE(loadPreferences().waveformLaneHeight,80);
        QCOMPARE(t->mouthLaneRect().height(),96.);QCOMPARE(loadPreferences().mouthLaneHeight,96);
        if(qEnvironmentVariableIsSet("CHOSUTA_WAVEFORM_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_WAVEFORM_ARTIFACTS"));QVERIFY(out.mkpath("."));QVERIFY(w.grab().save(out.filePath("waveform-ui.png")));}
    }
    void editWorkflow() {
        Window window;
        window.show();
        window.openPath(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");
        QTRY_COMPARE_WITH_TIMEOUT(window.currentProject().generated.size(),4,5000);
        auto timeline=window.findChild<Timeline*>("timeline");
        QVERIFY(timeline);
        auto initial=window.currentProject().effective();
        QPoint middle(qRound(timeline->xAtTime(.25)),80);
        QTest::mousePress(timeline,Qt::LeftButton,Qt::NoModifier,middle);
        QTest::mouseMove(timeline,middle+QPoint(12,0));
        QTest::mouseRelease(timeline,Qt::LeftButton,Qt::NoModifier,middle+QPoint(12,0));
        auto edited=window.currentProject().effective();
        QVERIFY(std::abs(edited[0].start-.1)<.00001);
        QVERIFY(std::abs(edited[0].end-.6)<.00001);
        QCOMPARE(window.currentProject().overrides.size(),1);
        auto eventShape=window.findChild<QComboBox*>("eventShape");
        auto apply=window.findChild<QPushButton*>("apply");
        QVERIFY(apply);
        eventShape->setCurrentText("O");
        QTest::mouseClick(apply,Qt::LeftButton);
        QCOMPARE(window.currentProject().effective()[0].shape,QString("O"));
        auto undo=[&] {
            for(auto action:window.findChildren<QAction*>())if(action->shortcut()==QKeySequence::Undo&&action->isEnabled()) {
                action->trigger();
                return true;
            }
            return false;
        };
        QVERIFY(undo());
        QCOMPARE(window.currentProject().effective()[0].shape,QString("A"));
        QVERIFY(undo());
        QCOMPARE(window.currentProject().effective()[0].start,0.);
        // Dragging an edge changes duration, independently of moving the interval.
        QPoint edge(qRound(timeline->xAtTime(.5))-2,80);
        QTest::mousePress(timeline,Qt::LeftButton,Qt::NoModifier,edge);
        QTest::mouseMove(timeline,edge-QPoint(12,0));
        QTest::mouseRelease(timeline,Qt::LeftButton,Qt::NoModifier,edge-QPoint(12,0));
        QVERIFY(std::abs(window.currentProject().effective()[0].end-.4)<.00001);
        QTemporaryDir dir;
        QString path=dir.filePath("編集 空格.chosuta");
        saveProject(window.currentProject(),path);
        auto restored=loadProject(path);
        QCOMPARE(restored.effective()[0].end,window.currentProject().effective()[0].end);
        restored.regenerate();
        QCOMPARE(restored.effective()[0].end,window.currentProject().effective()[0].end);
        auto before=restored.effective();
        timeline->setBeats(true);
        timeline->setBeats(false);
        QCOMPARE(window.currentProject().effective()[0].end,before[0].end);
        // Avoid an unsaved-close dialog in an automated test; Window destruction is local.
    }
};
QTEST_MAIN(UiTest)
#include "ui_test.moc"
