// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include "ui/window.h"
using namespace chosuta;
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
