// SPDX-License-Identifier: GPL-3.0-or-later
#include <QtTest>
#include "ui/window.h"
#include "core/executable.h"
#include <QScopeGuard>
#include <QRawFont>
using namespace chosuta;
static MediaTools testMediaTools(const QString &path="ffmpeg"){return selectMediaTools(path,ToolSelection::Explicit);}
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
using DropPoint=QPointF;
#else
using DropPoint=QPoint;
#endif
static void writeWaveformFixture(const QString &path){
    QByteArray pcm;QDataStream data(&pcm,QIODevice::WriteOnly);data.setByteOrder(QDataStream::LittleEndian);
    for(int i=0;i<32000;++i){double local=std::fmod(i/16000.,.5);data<<qint16(local>=.05&&local<.45?(i%32<16?12000:-12000):0);}
    QFile f(path);if(!f.open(QIODevice::WriteOnly))throw Failure(f.errorString());QDataStream s(&f);s.setByteOrder(QDataStream::LittleEndian);
    s.writeRawData("RIFF",4);s<<quint32(36+pcm.size());s.writeRawData("WAVEfmt ",8);s<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(16000)<<quint32(32000)<<quint16(2)<<quint16(16);s.writeRawData("data",4);s<<quint32(pcm.size());s.writeRawData(pcm.constData(),pcm.size());
}
static Project mouthUiProject(bool consonants=false){
    QFile file(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");if(!file.open(QIODevice::ReadOnly))throw Failure(file.errorString());
    auto root=checkedJson(file.readAll()).object();auto track=root["tracks"].toArray()[0].toObject();auto group=track["mainGroup"].toObject();auto notes=group["notes"].toArray();
    for(int i=0;i<notes.size();++i){auto note=notes[i].toObject();note["pitch"]=QVector<int>{60,62,66,60}[i];note["phonemes"]=consonants&&i==0?"d a":"a";notes[i]=note;}
    group["notes"]=notes;track["mainGroup"]=group;root["tracks"]=QJsonArray{track};Project p;p.score=parseSvp(QJsonDocument(root).toJson());p.selected={p.score.tracks[0].id};p.regenerate();p.canvas.width=p.canvas.height=64;return p;
}
class UiTest:public QObject {
    Q_OBJECT
    QTemporaryDir settings;
    private slots:
    void initTestCase(){
        QVERIFY(settings.isValid());QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        const auto font=QRawFont::fromFont(QApplication::font());
        qInfo().noquote()<<"UI test platform:"<<QGuiApplication::platformName()<<"font:"<<font.familyName();
        // Qt's missing-font box engine can report valid=true, but has no family.
        QVERIFY2(font.isValid()&&!font.familyName().isEmpty(),"UI layout tests require a usable font. On Windows run CTest with the native windows platform in a desktop session; offscreen may lack system fonts.");
    }
    void init(){setDefaultFfmpegExecutable({});Preferences p;const auto language=qEnvironmentVariable("CHOSUTA_TEST_UI_LANGUAGE");if(!language.isEmpty())p.language=language;savePreferences(p);}
    void localizedControlsAndErrors(){
        for(const auto &language:QStringList{"zh","en","ja"}){
            auto prefs=loadPreferences();prefs.language=language;savePreferences(prefs);Window w;w.show();
            const QString cancel=language=="zh"?"取消":language=="ja"?"キャンセル":"Cancel";
            QTimer::singleShot(0,&w,[&]{auto d=w.findChild<QDialog *>("preferencesDialog");QVERIFY(d);auto guard=qScopeGuard([d]{d->reject();});auto buttons=d->findChild<QDialogButtonBox *>();QVERIFY(buttons);QCOMPARE(buttons->button(QDialogButtonBox::Cancel)->text().remove('&'),cancel);});
            w.findChild<QAction *>("preferencesAction")->trigger();
            QLineEdit edit("text");edit.selectAll();auto menu=std::unique_ptr<QMenu>(edit.createStandardContextMenu());bool copy=false;
            for(auto action:menu->actions())if(action->text().remove('&').section('\t',0,0)==(language=="zh"?"复制":language=="ja"?"コピー":"Copy"))copy=true;QVERIFY(copy);
            QTemporaryDir dir;auto p=mouthUiProject();const auto path=dir.filePath("old.chosuta");saveProject(p,path);QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto root=checkedJson(file.readAll()).object();file.close();root["schema"]=13;QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(root).toJson());file.close();
            bool captured=false;QTimer poll;poll.setInterval(10);connect(&poll,&QTimer::timeout,&w,[&]{auto box=w.findChild<QMessageBox *>();if(!box||!box->isVisible())return;poll.stop();captured=true;const auto body=box->text();box->accept();QVERIFY(body.contains(language=="zh"?"不支持此工程格式":language=="ja"?"このプロジェクト形式":"Unsupported project format/schema"));});
            poll.start();w.openPath(path);QTRY_VERIFY(captured);QVERIFY(w.currentProject().generated.isEmpty());
            w.openPath(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QTRY_COMPARE(w.currentProject().generated.size(),4);w.findChild<QDoubleSpinBox *>("consonantLimit")->setValue(40);
            QTimer::singleShot(0,&w,[&]{auto box=w.findChild<QMessageBox *>();QVERIFY(box);const auto text=box->button(QMessageBox::Discard)->text().remove('&');box->button(QMessageBox::Cancel)->click();QCOMPARE(text,QString(language=="zh"?"不保存":language=="ja"?"保存しない":"Discard"));});QVERIFY(!w.close());QCOMPARE(w.currentProject().rules.consonantMaxSeconds,.04);
        }
        const auto message=Message::format("Missing audio: %1",{"名前 %2 <A>"}).withDetail("external stderr");
        QVERIFY(messageText("zh",message,false).contains("名前 %2 <A>"));QVERIFY(!messageText("zh",message,false).contains("external stderr"));QCOMPARE(messageDetails("zh",message),QString("external stderr"));
        Message unknown("Unrecognized future diagnostic");QCOMPARE(messageDetails("ja",unknown),unknown.text());QVERIFY(messageText("ja",unknown).contains(unknown.key));
    }
    void localizedIdsAndUndo(){
        auto prefs=loadPreferences();prefs.language="zh";savePreferences(prefs);QTemporaryDir dir;auto p=mouthUiProject();const auto path=dir.filePath("language.chosuta");saveProject(p,path);
        Window w;w.show();w.openPath(path);QTRY_COMPARE(w.currentProject().generated.size(),4);w.findChild<Timeline *>()->selectIds({p.generated[0].id});
        auto shapes=w.findChild<QComboBox *>("eventShape");QVERIFY(shapes);QCOMPARE(shapes->itemText(shapes->findData("closed")),QString("闭口"));shapes->setCurrentIndex(shapes->findData("closed"));w.findChild<QPushButton *>("apply")->click();QCOMPARE(w.currentProject().overrides.first().event.shape,QString("closed"));
        auto action=[&](QKeySequence::StandardKey key)->QAction *{for(auto a:w.findChildren<QAction *>())if(a->shortcut()==QKeySequence(key))return a;return nullptr;};
        QVERIFY(action(QKeySequence::Undo));QVERIFY(action(QKeySequence::Undo)->text().contains("应用到所选事件"));
        QTimer::singleShot(0,&w,[&]{auto d=w.findChild<QDialog *>("preferencesDialog");QVERIFY(d);auto choice=d->findChild<QComboBox *>("uiLanguageChoice");choice->setCurrentIndex(choice->findData("ja"));d->accept();});w.findChild<QAction *>("preferencesAction")->trigger();
        QVERIFY(w.isWindowModified());QVERIFY(!action(QKeySequence::Undo)->text().contains("应用"));QVERIFY(action(QKeySequence::Undo)->text().contains("選択"));action(QKeySequence::Undo)->trigger();QVERIFY(w.currentProject().overrides.isEmpty());QVERIFY(!w.isWindowModified());action(QKeySequence::Redo)->trigger();QCOMPARE(w.currentProject().overrides.first().event.shape,QString("closed"));
        w.findChild<Timeline *>()->selectIds({p.generated[0].id});shapes=w.findChild<QComboBox *>("eventShape");QCOMPARE(shapes->itemText(shapes->findData("closed")),QString("閉口"));
        shapes->setEditText("custom %2 帽子");w.findChild<QPushButton *>("apply")->click();QCOMPARE(w.currentProject().overrides.first().event.shape,QString("custom %2 帽子"));
        const auto saved=dir.filePath("saved.chosuta");saveProject(w.currentProject(),saved);QCOMPARE(loadProject(saved).overrides.first().event.shape,QString("custom %2 帽子"));
    }
    void localizedPerformanceReasons(){
        QTemporaryDir dir;auto p=mouthUiProject();p.appearance.enabled=true;applyPerformanceDraft(p,generatePerformance(p));QVERIFY(!p.performance.generated.isEmpty());
        p.performance.generated[0].reasons={Message("A1: arrival supported by sounding notes"),Message::format("%1 degree %2",{"C %2","3"})};const auto path=dir.filePath("reasons.chosuta");saveProject(p,path);
        for(const auto &lang:QStringList{"zh","en","ja"}){auto prefs=loadPreferences();prefs.language=lang;savePreferences(prefs);Window w;w.show();w.openPath(path);QTRY_COMPARE(w.currentProject().generated.size(),4);
            const auto seg=w.currentProject().performance.generated[0];w.findChild<Timeline *>()->performanceSelected(seg.id,seg.start,seg.end);const auto text=w.findChild<QPlainTextEdit *>("performanceReasons")->toPlainText();
            QVERIFY(text.contains("A1"));QVERIFY(text.contains("C %2"));if(lang!="en")QVERIFY(!text.contains("arrival supported by sounding notes"));QCOMPARE(w.currentProject().performance.generated[0].reasons,p.performance.generated[0].reasons);
            w.findChild<QPushButton *>("generatePerformance")->click();QTRY_VERIFY(w.findChild<QWidget *>("performanceDraftBar")->isVisible());const auto differences=w.findChild<QPlainTextEdit *>("performanceReasons")->toPlainText();if(lang!="en"){QVERIFY(!differences.contains("insufficient structural evidence"));QVERIFY(!differences.contains("Limited preceding background"));}w.findChild<QPushButton *>("performanceReview3")->click();QCOMPARE(w.currentProject().performance.generated[0].reasons,p.performance.generated[0].reasons);
        }
    }
    void unknownCopyAndImport(){
        QTemporaryDir dir;auto p=mouthUiProject();p.performance.groups.append(VariantGroup{"g","Original"});QImage image(16,16,QImage::Format_RGBA8888);image.fill(Qt::red);const auto red=dir.filePath("red.png");QVERIFY(image.save(red));image.fill(Qt::green);const auto green=dir.filePath("green.png");QVERIFY(image.save(green));
        p.assets["g/unknown"]=red;p.assets["unknown"]=green;const auto path=dir.filePath("copy.chosuta");saveProject(p,path);Window w;w.show();w.openPath(path);QTRY_COMPARE(w.currentProject().generated.size(),4);
        QTimer::singleShot(0,&w,[&]{auto d=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(d);auto guard=qScopeGuard([d]{if(d->isVisible())d->reject();});d->findChild<QListWidget *>("variantGroups")->setCurrentRow(1);
            QTimer::singleShot(0,d,[d]{auto input=d->findChild<QInputDialog *>();QVERIFY(input);input->setTextValue("Copy");input->accept();});d->findChild<QPushButton *>("copyVariantGroup")->click();d->findChild<QDialogButtonBox *>("mouthSettingsButtons")->button(QDialogButtonBox::Apply)->click();});w.findChild<QAction *>("advancedMouthAction")->trigger();
        QCOMPARE(w.currentProject().performance.groups.size(),3);const auto group=w.currentProject().performance.groups.back().id;QCOMPARE(w.currentProject().assets.value(variantAssetId(group,"unknown")),red);auto copied=w.currentProject();copied.appearance.enabled=true;auto selected=AppearanceResolver(copied).select(copied,"unknown",AppearanceContext{group});QCOMPARE(copied.assets[selected.id],red);
        QDir folder(dir.filePath("import"));QVERIFY(folder.mkpath("."));const auto imported=folder.filePath("unknown.png");QVERIFY(image.save(imported));
        QTimer::singleShot(0,&w,[&]{auto d=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(d);auto guard=qScopeGuard([d]{if(d->isVisible())d->reject();});d->findChild<QLineEdit *>("mouthImportFolder")->setText(folder.path());d->findChild<QPushButton *>("mouthScanFolder")->click();auto table=d->findChild<QTableWidget *>("mouthImportTable");QTRY_COMPARE(table->rowCount(),1);
            auto mouth=qobject_cast<QComboBox *>(table->cellWidget(0,2));QCOMPARE(mouth->currentData().toString(),QString("unknown"));auto choice=qobject_cast<QComboBox *>(table->cellWidget(0,1));choice->setCurrentIndex(choice->findData(group));table->item(0,0)->setCheckState(Qt::Checked);d->findChild<QPushButton *>("mouthApplyImport")->click();QCOMPARE(w.currentProject().assets.value(variantAssetId(group,"unknown")),red);d->findChild<QDialogButtonBox *>("mouthSettingsButtons")->button(QDialogButtonBox::Apply)->click();});w.findChild<QAction *>("advancedMouthAction")->trigger();
        QCOMPARE(w.currentProject().assets.value(variantAssetId(group,"unknown")),imported);saveProject(w.currentProject(),path);QCOMPARE(loadProject(path).assets.value(variantAssetId(group,"unknown")),imported);
        for(auto a:w.findChildren<QAction *>())if(a->shortcut()==QKeySequence(QKeySequence::Undo)){a->trigger();break;}QCOMPARE(w.currentProject().assets.value(variantAssetId(group,"unknown")),red);
    }
    void projectToolDoesNotExecuteOnOpen(){
#ifdef Q_OS_UNIX
        QTemporaryDir dir;const auto marker=dir.filePath("executed"),tool=dir.filePath("project-tool");
        QFile script(tool);QVERIFY(script.open(QIODevice::WriteOnly));script.write(("#!/bin/sh\nprintf executed > '"+marker+"'\n").toUtf8());script.close();QVERIFY(script.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        auto p=mouthUiProject();p.output.ffmpeg=tool;p.audioPath=dir.filePath("audio.wav");writeWaveformFixture(p.audioPath);const auto file=dir.filePath("untrusted.chosuta");saveProject(p,file);
        Window w;w.show();w.openPath(file);QTRY_COMPARE(w.currentProject().generated.size(),4);QTest::qWait(80);QVERIFY(!QFileInfo::exists(marker));
        QTimer::singleShot(0,&w,[&]{auto d=w.findChild<QDialog *>("exportSettingsDialog");QVERIFY(d);auto adopt=d->findChild<QCheckBox *>("adoptProjectMediaTool");QVERIFY(!adopt->isChecked());d->findChild<QPushButton *>("checkMediaTools")->click();QVERIFY(!QFileInfo::exists(marker));d->reject();});w.findChild<QAction *>("exportSettingsAction")->trigger();
        std::atomic_bool cancel=false;inspectMediaTools(selectMediaTools(tool,ToolSelection::Explicit),cancel);QVERIFY(QFileInfo::exists(marker));QVERIFY(!mediaToolAdopted(tool));
#else
        QSKIP("POSIX marker fixture; native Windows execution needs platform test");
#endif
    }
    void damagedPreferences() {
        QSettings settings(QSettings::IniFormat,QSettings::UserScope,"Chosuta","Chosuta");
        settings.setValue("performance/v2-defaults",QByteArray("{bad"));settings.setValue("pronunciation/settings",QByteArray("[]"));settings.sync();
        auto p=loadPreferences();QCOMPARE(p.invalidFields.size(),2);QCOMPARE(p.diagnostics.size(),2);
        p.language="ja";savePreferences(p);settings.sync();QCOMPARE(settings.value("performance/v2-defaults").toByteArray(),QByteArray("{bad"));
        Window window;QCOMPARE(window.currentProject().generated.size(),0);
        p.invalidFields.clear();savePreferences(p);QVERIFY(loadPreferences().invalidFields.isEmpty());
    }
    void sourceFragmentUndo() {
        QTemporaryDir dir;auto p=mouthUiProject();const auto source=p.generated[0].source;p.split(p.generated[0],.25);const auto id=p.effective()[0].id;
        const auto file=dir.filePath("split.chosuta");saveProject(p,file);Window w;w.show();w.openPath(file);QTRY_COMPARE(w.currentProject().overrides.size(),3);
        auto timeline=w.findChild<Timeline *>();timeline->selectIds({id});
        w.findChild<QDoubleSpinBox *>("eventStart")->setValue(.05);w.findChild<QDoubleSpinBox *>("eventEnd")->setValue(.30);w.findChild<QPushButton *>("apply")->click();
        auto mapped=[&]{return SourceTimeMap(w.currentProject()).forward(source,.125).value_or(-1);};QVERIFY(std::abs(mapped()-.175)<1e-9);
        auto history=[&](QKeySequence::StandardKey key){for(auto action:w.findChildren<QAction *>())if(action->shortcut()==QKeySequence(key)&&action->isEnabled()){action->trigger();return true;}return false;};
        QVERIFY(history(QKeySequence::Undo));QVERIFY(std::abs(mapped()-.125)<1e-9);QVERIFY(history(QKeySequence::Redo));QVERIFY(std::abs(mapped()-.175)<1e-9);
        const auto saved=dir.filePath("moved.chosuta");saveProject(w.currentProject(),saved);QVERIFY(std::abs(SourceTimeMap(loadProject(saved)).forward(source,.125).value()-.175)<1e-9);
    }
    void variantUnknownRemovalUndo() {
        QTemporaryDir dir;auto p=mouthUiProject();p.performance.groups.append(VariantGroup{"g","Group"});
        const auto image=dir.filePath("unknown.png");QImage png(16,16,QImage::Format_RGBA8888);png.fill(Qt::red);QVERIFY(png.save(image));
        p.assets["g/unknown"]=image;p.assets["unknown"]=image;p.assets["g/hat"]=image;p.fallback="g/unknown";
        p.generated[0].shape=p.fallback;p.generated[0].appearanceFixed=true;p.edit(p.generated[0]);
        const auto file=dir.filePath("group.chosuta");saveProject(p,file);Window w;w.show();w.openPath(file);QTRY_COMPARE(w.currentProject().performance.groups.size(),2);
        QTimer::singleShot(0,&w,[&]{
            auto dialog=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(dialog);auto list=dialog->findChild<QListWidget *>("variantGroups");
            for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole).toString()=="g")list->setCurrentRow(i);
            QTimer::singleShot(0,dialog,[&w]{auto combo=w.findChild<QComboBox *>("replacementGroup");QVERIFY(combo);combo->setCurrentIndex(combo->findData("base"));auto choice=qobject_cast<QDialog *>(combo->parentWidget());QVERIFY(choice);choice->accept();});
            dialog->findChild<QPushButton *>("removeVariantGroup")->click();dialog->findChild<QDialogButtonBox *>("mouthSettingsButtons")->button(QDialogButtonBox::Apply)->click();
        });
        w.findChild<QAction *>("advancedMouthAction")->trigger();QCOMPARE(w.currentProject().fallback,QString("unknown"));QCOMPARE(w.currentProject().overrides.first().event.shape,QString("unknown"));QVERIFY(w.currentProject().assets.contains("g/hat"));
        bool undone=false;for(auto action:w.findChildren<QAction *>())if(action->shortcut()==QKeySequence::Undo&&action->isEnabled()){action->trigger();undone=true;break;}QVERIFY(undone);
        QCOMPARE(w.currentProject().fallback,QString("g/unknown"));QCOMPARE(w.currentProject().overrides.first().event.shape,QString("g/unknown"));QVERIFY(w.currentProject().assets.contains("g/unknown"));QVERIFY(w.currentProject().assets.contains("g/hat"));QCOMPARE(w.currentProject().performance.groups.size(),2);
    }
    void invalidPerformanceDefaults() {
        QSettings store(QSettings::IniFormat,QSettings::UserScope,"Chosuta","Chosuta");
        for(const auto &target:QStringList{"group:missing","not-a-role","normal"}){
            PerformanceSettings settings;PerformanceRule rule;rule.target=target;if(target=="normal")rule.track="old-track";settings.rules={rule};
            const auto raw=QJsonDocument(performanceSettingsJson(settings)).toJson();store.setValue("performance/v2-defaults",raw);store.sync();
            auto prefs=loadPreferences();QVERIFY(prefs.invalidFields.contains("performance/v2-defaults"));QVERIFY(!prefs.diagnostics.isEmpty());QCOMPARE(prefs.performanceSettings,PerformanceSettings{});
            prefs.language="ja";savePreferences(prefs);store.sync();QCOMPARE(store.value("performance/v2-defaults").toByteArray(),raw);
            auto invalid=prefs;invalid.invalidFields.clear();invalid.performanceSettings=settings;QVERIFY_EXCEPTION_THROWN(savePreferences(invalid),Failure);
            Window window;window.openPath(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QTRY_COMPARE(window.currentProject().generated.size(),4);
            QTemporaryDir dir;const auto file=dir.filePath("new.chosuta");saveProject(window.currentProject(),file);QCOMPARE(loadProject(file).generated.size(),4);
        }
        Preferences valid;PerformanceRule rule;rule.target="emphasis";valid.performanceSettings.rules={rule};savePreferences(valid);
        auto loaded=loadPreferences();QVERIFY(loaded.invalidFields.isEmpty());QCOMPARE(loaded.performanceSettings,valid.performanceSettings);
    }
    void mediaToolSettings() {
        auto cleanup=qScopeGuard([]{setDefaultFfmpegExecutable({});});
        Window window;window.show();window.openPath(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QTRY_COMPARE(window.currentProject().generated.size(),4);
        QTemporaryDir dir;const auto first=dir.filePath("工具 one/ffmpeg"),other=dir.filePath("工程 two/ffmpeg");
        auto open=[&](bool accept,bool projectOverride){
            QTimer::singleShot(0,&window,[&]{
                auto d=window.findChild<QDialog*>("exportSettingsDialog");QVERIFY(d);
                d->findChild<QLineEdit*>("defaultFfmpegPath")->setText(first);
                d->findChild<QCheckBox*>("overrideFfmpeg")->setChecked(projectOverride);
                d->findChild<QLineEdit*>("projectFfmpegPath")->setText(other);
                if(accept)d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();else d->reject();
            });
            window.findChild<QAction*>("exportSettingsAction")->trigger();
        };
        open(false,true);QCOMPARE(defaultFfmpegExecutable(),QString());QCOMPARE(window.currentProject().output.ffmpeg,QString("ffmpeg"));
        open(true,false);QCOMPARE(defaultFfmpegExecutable(),first);QCOMPARE(window.currentProject().output.ffmpeg,QString("ffmpeg"));
        QCOMPARE(resolveExecutable(window.currentProject().output.ffmpeg),first);
        open(true,true);QCOMPARE(resolveExecutable(window.currentProject().output.ffmpeg),other);
        auto saved=dir.filePath("tools.chosuta");saveProject(window.currentProject(),saved);QCOMPARE(loadProject(saved).output.ffmpeg,other);
        Window second;second.openPath(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QTRY_COMPARE(second.currentProject().generated.size(),4);
        QCOMPARE(resolveExecutable(second.currentProject().output.ffmpeg),first);
        // Async check tests the entered value without committing it and remains cancellable.
        QTimer::singleShot(0,&window,[&]{
            auto d=window.findChild<QDialog*>("exportSettingsDialog");QVERIFY(d);auto guard=qScopeGuard([d]{d->reject();});
            auto report=d->findChild<QPlainTextEdit*>("mediaToolsReport");auto check=d->findChild<QPushButton*>("checkMediaTools");
            check->click();QVERIFY(!check->isEnabled());QTRY_VERIFY_WITH_TIMEOUT(check->isEnabled(),5000);
            QVERIFY(report->toPlainText().contains(other));QVERIFY(report->toPlainText().contains(messageText(widgetLanguage(),Message("FFmpeg: no version information"))));d->reject();
        });window.findChild<QAction*>("exportSettingsAction")->trigger();
        QCOMPARE(defaultFfmpegExecutable(),first);
    }
    void compactWorkspace(){
        QTemporaryDir dir;auto p=mouthUiProject();p.appearance.enabled=true;p.audioPath=dir.filePath("audio.wav");writeWaveformFixture(p.audioPath);const auto file=dir.filePath("compact.chosuta");saveProject(p,file);
        for(const auto &language:QStringList{"zh","en","ja"})for(int width:{1024,1366}){
            auto preferences=loadPreferences();preferences.language=language;savePreferences(preferences);
            Window w;w.resize(width,768);w.show();w.openPath(file);QTRY_COMPARE(w.currentProject().generated.size(),4);QTRY_VERIFY(w.centralWidget()->isEnabled());
            w.resize(width,768);QTest::qWait(30);QCOMPARE(w.size(),QSize(width,768));
            auto pages=w.findChild<QComboBox *>("workspacePageChoice");QVERIFY(pages);auto rules=w.findChild<QScrollArea *>("rulesPage");QVERIFY(rules);
            auto tabs=qobject_cast<QTabWidget *>(rules->parentWidget()->parentWidget());QVERIFY(tabs);tabs->setCurrentWidget(rules);QTest::qWait(10);QVERIFY(rules->isVisible());
            rules->ensureWidgetVisible(w.findChild<QDoubleSpinBox *>("consonantLimit"));QVERIFY(w.findChild<QDoubleSpinBox *>("consonantLimit")->isVisible());
            if(qEnvironmentVariableIsSet("CHOSUTA_COMPACT_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_COMPACT_ARTIFACTS"));QVERIFY(out.mkpath("."));QVERIFY(w.grab().save(out.filePath(language+QString("-%1-rules.png").arg(width))));}
            auto timeline=w.findChild<Timeline *>();timeline->selectIds({w.currentProject().generated[0].id});QVERIFY(w.findChild<QWidget *>("mouthProperties")->isVisible());
        }
    }
    void ruleTransactionAndStablePlayback(){
        Window w;w.show();w.openPath(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QTRY_COMPARE(w.currentProject().generated.size(),4);
        auto limit=w.findChild<QDoubleSpinBox *>("consonantLimit");const auto generated=w.currentProject().generated[0].end;
        limit->setValue(40);QCOMPARE(w.currentProject().rules.consonantMaxSeconds,.04);QVERIFY(w.isWindowModified());QCOMPARE(w.currentProject().generated[0].end,generated);
        QTemporaryDir dir;auto file=dir.filePath("rules.chosuta");saveProject(w.currentProject(),file);QCOMPARE(loadProject(file).rules.consonantMaxSeconds,.04);
        auto timeline=w.findChild<Timeline *>();timeline->setFocus();w.activateWindow();QTest::qWait(10);QTest::keySequence(timeline,QKeySequence::Undo);QCOMPARE(w.currentProject().rules.consonantMaxSeconds,.08);
        auto count=w.evaluationBuildCount();w.findChild<QPushButton *>("play")->click();QTest::qWait(120);w.findChild<QPushButton *>("play")->click();QCOMPARE(w.evaluationBuildCount(),count);
    }
    void draftCloseKeepsReview(){
        QTemporaryDir dir;auto p=mouthUiProject();p.appearance.enabled=true;const auto file=dir.filePath("review.chosuta");saveProject(p,file);
        Window w;w.show();w.openPath(file);QTRY_COMPARE(w.currentProject().generated.size(),4);w.findChild<QPushButton *>("generatePerformance")->click();
        auto bar=w.findChild<QWidget *>("performanceDraftBar");QTRY_VERIFY(bar->isVisible());
        QTimer::singleShot(0,&w,[&]{auto box=w.findChild<QMessageBox *>();QVERIFY(box);box->button(QMessageBox::Cancel)->click();});QVERIFY(!w.close());QVERIFY(bar->isVisible());
        w.findChild<QPushButton *>("performanceReview3")->click();QVERIFY(!bar->isVisible());QVERIFY(w.close());
    }
    void preferencesAndTransport() {
        QCOMPARE(loadPreferences().language,QString("auto"));QVERIFY(!loadPreferences().returnOnStop);
        QCOMPARE(resolveUiLanguage("auto",{"ja-JP"}),QString("ja"));QCOMPARE(resolveUiLanguage("auto",{"zh-Hant-TW"}),QString("zh"));QCOMPARE(resolveUiLanguage("auto",{"de-DE"}),QString("en"));QCOMPARE(resolveUiLanguage("en",{"zh-CN"}),QString("en"));
        Window window;window.show();window.openPath(QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp");QTRY_COMPARE(window.currentProject().generated.size(),4);
        auto position=window.findChild<QDoubleSpinBox*>("playPosition");auto apply=window.findChild<QPushButton*>("apply");apply->setFocus();
        QTest::keyClick(apply,Qt::Key_Space);QTRY_VERIFY(position->value()>.03);
        QTest::keyClick(apply,Qt::Key_Space);const double paused=position->value();QTest::qWait(80);QCOMPARE(position->value(),paused);QCOMPARE(window.currentProject().overrides.size(),0);
        QKeyEvent repeat(QEvent::KeyPress,Qt::Key_Space,Qt::NoModifier,QString(),true,2);QApplication::sendEvent(apply,&repeat);QTest::qWait(40);QCOMPARE(position->value(),paused);
        QTest::keyClick(apply,Qt::Key_Space);QTRY_VERIFY(position->value()>paused+.03);QTest::keyClick(apply,Qt::Key_Space);
        // Preferences are persisted, and changing language rebuilds a translated UI.
        QTimer::singleShot(0,&window,[&]{auto dialog=window.findChild<QDialog*>("preferencesDialog");QVERIFY(dialog);dialog->findChild<QComboBox*>("uiLanguageChoice")->setCurrentIndex(1);QVERIFY(!dialog->findChild<QComboBox*>("pauseMode"));dialog->accept();});
        window.findChild<QAction*>("preferencesAction")->trigger();QCOMPARE(loadPreferences().language,QString("zh"));auto returning=window.findChild<QAction *>("returnOnStopAction");QVERIFY(returning);returning->setChecked(true);QVERIFY(loadPreferences().returnOnStop);QVERIFY(window.findChild<QMenu *>("transportMenu"));
        position=window.findChild<QDoubleSpinBox*>("playPosition");apply=window.findChild<QPushButton*>("apply");
        auto play=window.findChild<QPushButton *>("play");auto transport=window.findChild<QAction *>("transportToggleAction");QVERIFY(play);QVERIFY(transport);
        QVERIFY(!window.findChild<QPushButton *>("stop"));QVERIFY(!window.findChild<QAction *>("transportStopAction"));
        // All three entry points share both return modes, including mixed start/stop inputs.
        QVector<std::function<void()>> inputs={
            [&]{QTest::keyClick(apply,Qt::Key_Space);},
            [&]{QTest::mouseClick(play,Qt::LeftButton);},
            [&]{transport->trigger();}
        };
        for(bool enabled:{false,true}){
            returning->setChecked(enabled);QCOMPARE(loadPreferences().returnOnStop,enabled);
            for(int i=0;i<inputs.size();++i){
                position->setValue(.2);inputs[i]();QTRY_VERIFY(position->value()>.23);
                position->setValue(.6);QTRY_VERIFY(position->value()>.63);
                inputs[(i+1)%inputs.size()]();double stopped=position->value();
                if(enabled)QCOMPARE(stopped,.2);else QVERIFY(stopped>=.63);
                QTest::qWait(40);QCOMPARE(position->value(),stopped);
            }
        }
        position->setValue(.4);
        // Space remains an ordinary character in editable lyric/path text fields.
        QLineEdit text(&window);text.setFocus();QTest::keyClick(&text,Qt::Key_Space);QCOMPARE(text.text(),QString(" "));QCOMPARE(position->value(),.4);
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
        QCOMPARE(AppearanceResolver(window.currentProject()).at(window.currentProject(),window.currentProject().effective(),.2).shape,QString("unknown"));
        QTimer::singleShot(0,&window,[&] {
            auto dialog=window.findChild<QDialog*>("advancedSettingsDialog");QVERIFY(dialog);
            QCOMPARE(dialog->findChild<QTabWidget*>("advancedSettingsTabs")->currentIndex(),0);
            QVERIFY(!dialog->findChild<QCheckBox*>("japaneseKanji")->isChecked());
            dialog->findChild<QPushButton*>("dictionaryAdd")->click();
            auto table=dialog->findChild<QTableWidget*>("dictionaryTable");QCOMPARE(table->rowCount(),1);
            table->item(0,1)->setText("testword");table->item(0,3)->setText("UW");
            dialog->findChild<QCheckBox*>("japaneseKanji")->setChecked(true);
            QVERIFY(!dialog->findChild<QCheckBox*>("dictionaryDefaults")->isChecked());dialog->findChild<QCheckBox*>("dictionaryDefaults")->setChecked(true);
            dialog->findChild<QDialogButtonBox*>("advancedSettingsButtons")->button(QDialogButtonBox::Save)->click();
        });
        window.findChild<QAction*>("advancedSettingsAction")->trigger();
        QCOMPARE(window.currentProject().rules.pronunciation.words["en"]["testword"].text,QString("UW"));
        QCOMPARE(loadPreferences().pronunciation,window.currentProject().rules.pronunciation);
        window.findChild<QPushButton*>("generate")->click();
        QTRY_COMPARE(AppearanceResolver(window.currentProject()).at(window.currentProject(),window.currentProject().effective(),.2).shape,QString("U"));
        const auto saved=loadPreferences().pronunciation;
        QTimer::singleShot(0,&window,[&] {
            auto dialog=window.findChild<QDialog*>("advancedSettingsDialog");QVERIFY(dialog);
            auto table=dialog->findChild<QTableWidget*>("dictionaryTable");QCOMPARE(table->rowCount(),1);
            table->item(0,3)->setText("invalid-phone");
            auto buttons=dialog->findChild<QDialogButtonBox*>("advancedSettingsButtons");buttons->button(QDialogButtonBox::Save)->click();
            auto guard=qScopeGuard([dialog]{dialog->reject();});
            QVERIFY(dialog->isVisible());QVERIFY(dialog->findChild<QLabel*>("dictionaryStatus")->text().contains(messageText(widgetLanguage(),Message::format("Unrecognized dictionary reading: %1",{"en / testword"}))));
            buttons->button(QDialogButtonBox::Cancel)->click();
        });
        window.findChild<QAction*>("advancedSettingsAction")->trigger();QCOMPARE(loadPreferences().pronunciation,saved);
        const auto projectPath=dir.filePath("dictionary.chosuta");saveProject(window.currentProject(),projectPath);
        auto changed=loadPreferences();changed.pronunciation.words["en"]["testword"]={"IY",true};savePreferences(changed);
        Window restored;restored.show();restored.openPath(projectPath);
        QTRY_COMPARE(restored.currentProject().rules.pronunciation,saved);
        restored.findChild<QPushButton*>("generate")->click();QTRY_COMPARE(AppearanceResolver(restored.currentProject()).at(restored.currentProject(),restored.currentProject().effective(),.2).shape,QString("U"));
        Window fresh;fresh.show();fresh.openPath(path);
        QTRY_COMPARE(AppearanceResolver(fresh.currentProject()).at(fresh.currentProject(),fresh.currentProject().effective(),.2).shape,QString("I"));
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
        QTest::mouseDClick(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(timeline->xAtTime(3)),qRound(timeline->subtitleLaneRect(0).top()+25)));QCOMPARE(window.currentProject().subtitles[0].cues.size(),1);QCOMPARE(window.currentProject().subtitles[0].cues[0].start,3.);QCOMPARE(window.currentProject().subtitles[0].cues[0].end,5.);QVERIFY(window.findChild<QWidget*>("mouthProperties")->isHidden());
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
        t->selectIds({w.currentProject().generated[0].id});auto eventShape=w.findChild<QComboBox*>("eventShape");QVERIFY(eventShape);eventShape->setCurrentText("O");apply->click();correct->click();QTRY_VERIFY(w.centralWidget()->isEnabled());QVERIFY(!w.currentProject().timing.sources.contains(w.currentProject().generated[0].source));revert->click();QCOMPARE(AppearanceResolver(w.currentProject()).at(w.currentProject(),w.currentProject().effective(),.2).shape,QString("O"));
        QTest::mouseDClick(t,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(t->xAtTime(.3)),qRound(t->subtitleLaneRect(0).top()+22)));QVERIFY(t->editingText());auto editor=w.findChild<QPlainTextEdit*>("subtitleText");QVERIFY(editor->isVisible());QVERIFY(editor->cursorRect().intersects(editor->viewport()->rect()));QTest::keyClick(editor,Qt::Key_Escape);
        w.findChild<QPushButton*>("resetTimelineHeights")->click();QCOMPARE(t->waveformLaneRect().height(),80.);QCOMPARE(loadPreferences().waveformLaneHeight,80);
        QCOMPARE(t->mouthLaneRect().height(),96.);QCOMPARE(loadPreferences().mouthLaneHeight,96);
        if(qEnvironmentVariableIsSet("CHOSUTA_WAVEFORM_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_WAVEFORM_ARTIFACTS"));QVERIFY(out.mkpath("."));QVERIFY(w.grab().save(out.filePath("waveform-ui.png")));}
    }
    void acceptedSettingsAndStableLayout(){
        QTemporaryDir dir;auto p=mouthUiProject();p.appearance.enabled=true;p.performance.groups.append(VariantGroup{"power","Power"});p.performance.roles["emphasis"]="power";
        PerformanceSegment manual;manual.id="protected";manual.anchor="seconds";manual.start=.1;manual.end=.3;manual.targets={"emphasis"};putPerformanceSegment(p,manual);
        auto file=dir.filePath("settings.chosuta");saveProject(p,file);Window w;w.resize(1280,1000);w.show();w.openPath(file);QTRY_COMPARE(w.currentProject().generated.size(),4);
        auto edit=[&](double value){QTimer::singleShot(0,&w,[&,value]{auto d=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(d);d->findChild<QDoubleSpinBox *>("performancesupport")->setValue(value);d->findChild<QDialogButtonBox *>("mouthSettingsButtons")->button(QDialogButtonBox::Apply)->click();});w.findChild<QAction *>("advancedMouthAction")->trigger();};
        edit(1.25);QTRY_VERIFY(w.centralWidget()->isEnabled());QVERIFY(performanceCurrent(w.currentProject()));QCOMPARE(w.currentProject().performance.settings.music.support,1.25);QCOMPARE(w.currentProject().performance.manual.size(),1);QVERIFY(!w.findChild<QWidget *>("performanceDraftBar")->isVisible());
        auto timeline=w.findChild<Timeline *>("timeline");timeline->setFocus();w.activateWindow();QTest::qWait(10);QTest::keySequence(timeline,QKeySequence::Undo);QCOMPARE(w.currentProject().performance.settings.music.support,1.);QVERIFY(w.currentProject().performance.generated.isEmpty());QTest::keySequence(timeline,QKeySequence::Redo);QVERIFY(performanceCurrent(w.currentProject()));
        // A pending Apply can be cancelled without committing settings or results.
        auto before=performanceJson(w.currentProject());edit(2.);w.findChild<QPushButton *>("cancelTask")->click();QTRY_VERIFY(w.centralWidget()->isEnabled());QCOMPARE(performanceJson(w.currentProject()),before);
        auto h=w.findChild<QSplitter *>("workspaceSplitter"),v=w.findChild<QSplitter *>("timelineSplitter");h->setSizes({410,800});v->setSizes({610,290});QTest::qWait(20);auto hs=h->sizes(),vs=v->sizes();auto dimensions=w.size();
        w.findChild<QPushButton *>("generatePerformance")->click();QTRY_VERIFY(w.findChild<QWidget *>("performanceDraftBar")->isVisible());QCOMPARE(h->sizes(),hs);QCOMPARE(v->sizes(),vs);QCOMPARE(w.size(),dimensions);
        w.findChild<QPushButton *>("performanceReview2")->click();QTest::qWait(20);QCOMPARE(h->sizes(),hs);QCOMPARE(v->sizes(),vs);
        // Automatic selections prefill their group instead of retaining a prior manual template.
        w.findChild<QComboBox *>("performanceMode")->setCurrentIndex(3);
        QTest::mouseClick(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(timeline->xAtTime(.6)),qRound(timeline->performanceLaneRect().center().y())));
        QCOMPARE(w.findChild<QComboBox *>("performanceMode")->currentData().toString(),QString("hold"));
        QCOMPARE(w.findChild<QComboBox *>("performanceTarget0")->currentData().toString(),"group:"+PerformanceResolver(w.currentProject()).groupAt(.6));
        QVERIFY(w.findChild<QLabel *>("performanceEditHelp"));
        QTest::qWait(30);auto rangeStart=w.findChild<QDoubleSpinBox *>("performanceStart");QVERIFY(rangeStart->height()>=rangeStart->minimumSizeHint().height());
        if(qEnvironmentVariableIsSet("CHOSUTA_ADVANCED_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_ADVANCED_ARTIFACTS"));out.mkpath(".");w.grab().save(out.filePath("manual-and-transport.png"));}
    }
    void cursorSnapping(){
        auto p=mouthUiProject();p.score.time.tempos={{0,120},{2*Blick,60}};p.score.time.meters={{0,6,8},{1,3,4}};p.output.syncOffset=.1;p.regenerate();
        Timeline t;t.resize(1000,300);t.setProject(p);t.show();QSignalSpy seeks(&t,&Timeline::seek);t.setCursorSnap("beat");
        QCOMPARE(t.snappedCursor(.36),.35);QCOMPARE(t.snappedCursor(1.38),1.6);QCOMPARE(t.snappedCursor(2.7),3.1);QCOMPARE(t.snappedCursor(.36,true),.36);
        auto point=QPoint(qRound(t.xAtTime(.38)),qRound(t.rulerRect().center().y()));QTest::mouseClick(&t,Qt::LeftButton,Qt::NoModifier,point);QCOMPARE(seeks.last()[0].toDouble(),.35);
        QTest::mouseClick(&t,Qt::LeftButton,Qt::AltModifier,point);QVERIFY(std::abs(seeks.last()[0].toDouble()-.35)>.01);
        t.setCursorSnap("syllable");PerformanceResolver resolver(p);auto spans=lyricSpans(p,p.score.tracks.front().id);QVERIFY(!spans.isEmpty());auto edge=resolver.mappedBlick(spans.front().end,p.score.tracks.front().id,true);QCOMPARE(t.snappedCursor(edge-.01),edge);
        t.setCursorSnap("off");QCOMPARE(t.snappedCursor(.38),.38);
        Window w;w.show();auto combo=w.findChild<QComboBox *>("cursorSnap");combo->setCurrentIndex(combo->findData("syllable"));QCOMPARE(loadPreferences().cursorSnap,QString("syllable"));
    }
    void arbitraryPngDropAndMapping(){
        QTemporaryDir dir;auto p=mouthUiProject();p.performance.groups.append(VariantGroup{"power","Power"});auto file=dir.filePath("drop.chosuta");saveProject(p,file);QImage image(48,48,QImage::Format_RGBA8888);image.fill(Qt::magenta);QString one=dir.filePath("任意名前 1.png"),two=dir.filePath("任意名前 2.png");QVERIFY(image.save(one));QVERIFY(image.save(two));Window w;w.show();w.openPath(file);QTRY_COMPARE(w.currentProject().generated.size(),4);
        auto drop=[](QWidget *target,const QStringList &paths){QMimeData mime;QList<QUrl> urls;for(const auto &path:paths)urls.append(QUrl::fromLocalFile(path));mime.setUrls(urls);QDragEnterEvent enter(DropPoint(10,10),Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(target,&enter);if(!enter.isAccepted())return false;QDropEvent event({10,10},Qt::CopyAction,&mime,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(target,&event);return event.isAccepted();};
        QTimer::singleShot(0,&w,[&]{auto d=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(d);auto guard=qScopeGuard([d]{if(d->isVisible())d->reject();});auto slot=d->findChild<QToolButton *>("mouthSlotA");QVERIFY(drop(slot,{one}));QCOMPARE(d->findChild<QLineEdit *>("mouthImagePath")->text(),one);
            auto table=d->findChild<QTableWidget *>("mouthImportTable");QVERIFY(drop(table->viewport(),{one,two}));QTRY_COMPARE(table->rowCount(),2);
            for(int row=0;row<2;++row){auto group=qobject_cast<QComboBox *>(table->cellWidget(row,1));group->setCurrentIndex(group->findData("power"));auto mouth=qobject_cast<QComboBox *>(table->cellWidget(row,2));mouth->setCurrentIndex(mouth->findData("A"));}
            d->findChild<QPushButton *>("mouthApplyImport")->click();QVERIFY(d->findChild<QLabel *>("mouthSettingsStatus")->text().contains(QString::fromUtf8("同一"))||d->findChild<QLabel *>("mouthSettingsStatus")->text().contains("same slot")||d->findChild<QLabel *>("mouthSettingsStatus")->text().contains(QString::fromUtf8("同じ")));
            auto mouth=qobject_cast<QComboBox *>(table->cellWidget(1,2));mouth->setCurrentIndex(mouth->findData("O"));d->findChild<QPushButton *>("mouthApplyImport")->click();QVERIFY(w.currentProject().assets.isEmpty());
            if(qEnvironmentVariableIsSet("CHOSUTA_ADVANCED_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_ADVANCED_ARTIFACTS"));out.mkpath(".");d->grab().save(out.filePath("mapped-import.png"));}
            d->findChild<QDialogButtonBox *>("mouthSettingsButtons")->button(QDialogButtonBox::Apply)->click();});w.findChild<QAction *>("advancedMouthAction")->trigger();QCOMPARE(w.currentProject().assets.value("power/A"),one);QCOMPARE(w.currentProject().assets.value("power/O"),two);QCOMPARE(w.currentProject().assets.value("A"),one);
        auto timeline=w.findChild<Timeline *>("timeline");timeline->setFocus();w.activateWindow();QTest::qWait(10);QTest::keySequence(timeline,QKeySequence::Undo);QVERIFY(w.currentProject().assets.isEmpty());
        // Main-window drops route to the same review; cancellation leaves the project alone.
        QVERIFY(drop(&w,{one}));QTimer::singleShot(0,&w,[&]{auto d=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(d);d->reject();});QTest::qWait(50);QVERIFY(w.currentProject().assets.isEmpty());
        // Dropping a project observes the same unsaved-edit confirmation as File/Open.
        w.findChild<QCheckBox *>("advancedMouthEnabled")->setChecked(true);auto originalScore=w.currentProject().score.raw;
        auto svp=QString(CHOSUTA_SOURCE_DIR)+"/tests/fixtures/basic.svp";QVERIFY(drop(&w,{svp}));
        QTimer::singleShot(0,&w,[&]{auto question=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(question);question->button(QMessageBox::Cancel)->click();});QTest::qWait(50);QCOMPARE(w.currentProject().score.raw,originalScore);QVERIFY(w.currentProject().appearance.enabled);
        Window opened;opened.show();QVERIFY(drop(&opened,{svp}));QTRY_COMPARE(opened.currentProject().generated.size(),4);QCOMPARE(opened.findChild<QDoubleSpinBox *>("playPosition")->value(),0.);
        QMimeData remote;remote.setUrls({QUrl("https://example.invalid/image.png")});QDragEnterEvent rejected(DropPoint(10,10),Qt::CopyAction,&remote,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(&w,&rejected);QVERIFY(!rejected.isAccepted());
    }
    void performanceTemplateEditing(){
        QTemporaryDir dir;auto p=mouthUiProject();p.appearance.enabled=true;p.performance.groups.append(VariantGroup{"power","Power"});p.performance.roles["emphasis"]="power";
        PerformanceSegment m;m.id="arch";m.anchor="seconds";m.start=0;m.end=2;m.mode="arch";m.targets={"normal","emphasis","normal"};m.cuts={.35,.7};putPerformanceSegment(p,m);auto before=PerformanceResolver(p);auto file=dir.filePath("template.chosuta");saveProject(p,file);Window w;w.show();w.openPath(file);QTRY_COMPARE(w.currentProject().performance.manual.size(),1);auto timeline=w.findChild<Timeline *>("timeline");
        auto select=[&]{QTest::mouseClick(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(timeline->xAtTime(.2)),qRound(timeline->performanceLaneRect().center().y())));};select();w.findChild<QDoubleSpinBox *>("playPosition")->setValue(1);w.findChild<QPushButton *>("splitPerformance")->click();QCOMPARE(w.currentProject().performance.manual.size(),2);
        select();w.findChild<QPushButton *>("mergePerformance")->click();QCOMPARE(w.currentProject().performance.manual.size(),1);QCOMPARE(w.currentProject().performance.manual[0].mode,QString("custom"));auto merged=PerformanceResolver(w.currentProject());for(int i=0;i<200;++i)QCOMPARE(merged.groupAt(i*.01),before.groupAt(i*.01));
        select();w.findChild<QPushButton *>("restorePerformance")->click();QTRY_VERIFY(w.findChild<QWidget *>("performanceDraftBar")->isVisible());QCOMPARE(w.currentProject().performance.manual.size(),1);w.findChild<QPushButton *>("performanceReview2")->click();QVERIFY(w.currentProject().performance.manual.isEmpty());QVERIFY(!w.currentProject().performance.generated.isEmpty());
    }
    void performanceV2Controls(){
        QTemporaryDir dir;auto p=mouthUiProject();p.appearance.enabled=true;auto file=dir.filePath("controls.chosuta");saveProject(p,file);Window w;w.resize(1200,900);w.show();w.openPath(file);QTRY_COMPARE(w.currentProject().generated.size(),4);
        QTimer::singleShot(0,&w,[&]{auto dialog=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(dialog);auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});dialog->findChild<QTabWidget *>("mouthSettingsPages")->setCurrentIndex(1);
            auto state=dialog->findChild<QComboBox *>("tonalityState"),tonic=dialog->findChild<QComboBox *>("tonalityTonic"),mode=dialog->findChild<QComboBox *>("tonalityMode");state->setCurrentIndex(state->findData("specified"));tonic->setCurrentIndex(tonic->findData("Bb"));mode->setCurrentIndex(mode->findData("minor"));
            auto basis=dialog->findChild<QComboBox *>("performanceTimeBasis");auto support=dialog->findChild<QDoubleSpinBox *>("performancesupport");QCOMPARE(support->value(),1.);basis->setCurrentIndex(1);QCOMPARE(support->value(),.5);support->setValue(.8);basis->setCurrentIndex(0);QCOMPARE(support->value(),1.);support->setValue(1.25);basis->setCurrentIndex(1);QCOMPARE(support->value(),.8);
            auto unit=dialog->findChild<QComboBox *>("mouthIntervalUnit");auto leap=dialog->findChild<QSpinBox *>("performanceleapDistance");QCOMPARE(leap->value(),5);unit->setCurrentIndex(1);QCOMPARE(leap->value(),4);leap->setValue(5);unit->setCurrentIndex(0);QCOMPARE(leap->value(),5);
            dialog->findChild<QPushButton *>("addPerformanceRule")->click();auto rules=dialog->findChild<QTableWidget *>("performanceRules");rules->item(0,3)->setText("7");unit->setCurrentIndex(1);QCOMPARE(rules->item(0,3)->text(),QString("4"));rules->item(0,3)->setText("6");unit->setCurrentIndex(0);QCOMPARE(rules->item(0,3)->text(),QString("7"));basis->setCurrentIndex(0);
            if(qEnvironmentVariableIsSet("CHOSUTA_ADVANCED_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_ADVANCED_ARTIFACTS"));out.mkpath(".");QVERIFY(dialog->grab().save(out.filePath("v2-controls.png")));}
            dialog->findChild<QDialogButtonBox *>("mouthSettingsButtons")->button(QDialogButtonBox::Apply)->click();});
        w.findChild<QAction *>("advancedMouthAction")->trigger();QTRY_VERIFY(w.centralWidget()->isEnabled());auto &a=w.currentProject().performance.analysis;QCOMPARE(a.degreeTonic,QString("Bb"));QCOMPARE(a.degreeMode,QString("minor"));auto settings=w.currentProject().performance.settings;QCOMPARE(settings.music.support,1.25);QCOMPARE(settings.seconds.support,.8);QCOMPARE(settings.leapDegrees,5);QCOMPARE(settings.rules[0].threshold,7.);QCOMPARE(settings.rules[0].thresholdDegrees,6.);
        auto display=w.findChild<QComboBox *>("performanceTimeDisplay");auto start=w.findChild<QDoubleSpinBox *>("performanceStart"),end=w.findChild<QDoubleSpinBox *>("performanceEnd");start->setValue(.5);end->setValue(1.5);display->setCurrentIndex(1);QCOMPARE(start->value(),.5);QCOMPARE(end->value(),1.5);
        auto text=w.findChild<QLineEdit *>("performanceEndMusic");QTest::keyClick(text,Qt::Key_A,Qt::ControlModifier);QTest::keyClicks(text,"1:3:0");w.findChild<QPushButton *>("applyPerformance")->click();QCOMPARE(w.currentProject().performance.manual.size(),1);QCOMPARE(w.currentProject().performance.manual[0].start,.5);QCOMPARE(w.currentProject().performance.manual[0].end,1.);display->setCurrentIndex(0);QCOMPARE(end->value(),1.);
        QTimer::singleShot(0,&w,[&]{auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());QVERIFY(dialog);auto state=dialog->findChild<QComboBox *>("tonalityState");state->setCurrentIndex(state->findData("disabled"));dialog->accept();});w.findChild<QPushButton *>("performanceTonality")->click();QCOMPARE(w.currentProject().performance.tonalities.size(),1);QVERIFY(w.currentProject().performance.tonalities[0].key.isEmpty());
        auto boundary=w.findChild<QComboBox *>("performanceBoundaryMode");boundary->setCurrentIndex(boundary->findData("section-break"));w.findChild<QPushButton *>("applyPerformanceBoundary")->click();QCOMPARE(w.currentProject().performance.boundaries.size(),1);QCOMPARE(w.currentProject().performance.boundaries[0].mode,QString("section-break"));
        auto saved=dir.filePath("saved.chosuta");saveProject(w.currentProject(),saved);auto reopened=loadProject(saved);QCOMPARE(reopened.performance.settings,settings);QCOMPARE(reopened.performance.analysis.degreeTonic,QString("Bb"));
    }
    void performanceWorkflow(){
        QTemporaryDir dir;auto p=mouthUiProject();const auto source=dir.filePath("performance.chosuta");saveProject(p,source);Window w;w.resize(1200,850);w.show();w.openPath(source);QTRY_COMPARE(w.currentProject().generated.size(),4);
        auto timing=timingCorrectionJson(w.currentProject());
        QTimer::singleShot(0,&w,[&]{auto dialog=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(dialog);auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});dialog->findChild<QCheckBox *>("mouthEnabled")->setChecked(true);
            QTimer::singleShot(0,dialog,[&]{auto prompt=dialog->findChild<QInputDialog *>();QVERIFY(prompt);prompt->setTextValue("Power");prompt->accept();});dialog->findChild<QPushButton *>("addVariantGroup")->click();QCOMPARE(dialog->findChild<QListWidget *>("variantGroups")->count(),2);
            auto role=dialog->findChild<QComboBox *>("performanceRoleemphasis");role->setCurrentIndex(1);
            auto tabs=dialog->findChild<QTabWidget *>("mouthSettingsPages");tabs->setCurrentIndex(1);dialog->findChild<QComboBox *>("tonalityState")->setCurrentIndex(1);auto tonic=dialog->findChild<QComboBox *>("tonalityTonic");tonic->setCurrentIndex(tonic->findData("D"));auto mode=dialog->findChild<QComboBox *>("tonalityMode");mode->setCurrentIndex(mode->findData("dorian"));dialog->findChild<QCheckBox *>("performanceSaveDefaults")->setChecked(true);
            if(qEnvironmentVariableIsSet("CHOSUTA_ADVANCED_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_ADVANCED_ARTIFACTS"));out.mkpath(".");dialog->grab().save(out.filePath("performance-rules.png"));tabs->setCurrentIndex(0);dialog->grab().save(out.filePath("variant-groups.png"));}
            dialog->findChild<QDialogButtonBox *>("mouthSettingsButtons")->button(QDialogButtonBox::Apply)->click();});
        w.findChild<QAction *>("advancedMouthAction")->trigger();QTRY_VERIFY(w.currentProject().appearance.enabled);QCOMPARE(w.currentProject().performance.analysis.degreeMode,QString("dorian"));QCOMPARE(w.currentProject().performance.groups.size(),2);QCOMPARE(timingCorrectionJson(w.currentProject()),timing);
        auto timeline=w.findChild<Timeline *>("timeline");QCOMPARE(timeline->performanceLaneRect().height(),56.);auto generated=w.currentProject().generated.size();
        w.findChild<QPushButton *>("generatePerformance")->click();QTRY_VERIFY(w.findChild<QWidget *>("performanceDraftBar")->isVisible());QVERIFY(!w.currentProject().performance.generated.isEmpty());
        w.findChild<QPushButton *>("performanceReview0")->click();w.findChild<QPushButton *>("performanceReview1")->click();w.findChild<QPushButton *>("performanceReview3")->click();QVERIFY(!w.currentProject().performance.generated.isEmpty());
        w.findChild<QPushButton *>("generatePerformance")->click();QTRY_VERIFY(w.findChild<QWidget *>("performanceDraftBar")->isVisible());w.findChild<QPushButton *>("performanceReview2")->click();QVERIFY(!w.currentProject().performance.generated.isEmpty());QVERIFY(performanceCurrent(w.currentProject()));QCOMPARE(w.currentProject().generated.size(),generated);QCOMPARE(timingCorrectionJson(w.currentProject()),timing);
        w.findChild<QDoubleSpinBox *>("performanceStart")->setValue(.1);w.findChild<QDoubleSpinBox *>("performanceEnd")->setValue(1.8);w.findChild<QComboBox *>("performanceMode")->setCurrentIndex(3);w.findChild<QPushButton *>("applyPerformance")->click();QCOMPARE(w.currentProject().performance.manual.size(),1);auto before=performanceJson(w.currentProject());
        // Protected manual segments survive explicit generation and one-step undo.
        w.findChild<QPushButton *>("generatePerformance")->click();QTRY_VERIFY(w.findChild<QWidget *>("performanceDraftBar")->isVisible());w.findChild<QPushButton *>("performanceReview2")->click();QCOMPARE(w.currentProject().performance.manual.size(),1);
        // Restoring automatic is also a draft; cancellation retains the manual layer.
        w.findChild<QPushButton *>("restorePerformance")->click();QTRY_VERIFY(w.findChild<QWidget *>("performanceDraftBar")->isVisible());QCOMPARE(w.currentProject().performance.manual.size(),1);w.findChild<QPushButton *>("performanceReview3")->click();QCOMPARE(w.currentProject().performance.manual.size(),1);
        auto saved=dir.filePath("edited.chosuta");saveProject(w.currentProject(),saved);QCOMPARE(performanceJson(loadProject(saved)),performanceJson(w.currentProject()));
        const double position=w.findChild<QDoubleSpinBox *>("playPosition")->value();QTest::mouseClick(timeline,Qt::LeftButton,Qt::NoModifier,QPoint(qRound(timeline->xAtTime(.2)),qRound(timeline->performanceLaneRect().center().y())));QCOMPARE(w.findChild<QDoubleSpinBox *>("playPosition")->value(),position);
        w.findChild<QPushButton *>("deletePerformance")->click();QCOMPARE(w.currentProject().performance.manual.size(),0);timeline->setFocus();w.activateWindow();QTest::qWait(10);QTest::keySequence(timeline,QKeySequence::Undo);QCOMPARE(w.currentProject().performance.manual.size(),1);
        if(qEnvironmentVariableIsSet("CHOSUTA_ADVANCED_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_ADVANCED_ARTIFACTS"));w.grab().save(out.filePath("performance-workspace.png"));}
        auto advanced=w.findChild<QCheckBox *>("advancedMouthEnabled");advanced->setChecked(false);QCOMPARE(timeline->performanceLaneRect().height(),0.);QCOMPARE(w.currentProject().performance.manual.size(),1);advanced->setChecked(true);QCOMPARE(timeline->performanceLaneRect().height(),56.);
    }
    void consonantRuleWorkflow(){
        QTemporaryDir dir;auto p=mouthUiProject(true);auto edited=p.generated[0];edited.shape="O";p.edit(edited);
        if(!QStandardPaths::findExecutable("ffmpeg").isEmpty()){p.audioPath=dir.filePath("timing.wav");writeWaveformFixture(p.audioPath);std::atomic_bool cancel=false;const auto wave=readWaveform(p.audioPath,testMediaTools("ffmpeg"),cancel);QVERIFY(wave.wave);p.audioContentHash=wave.wave->hash;p.timing=correctWaveform(p,*wave.wave,cancel).timing;QCOMPARE(p.timing.sources.size(),3);}
        const auto source=dir.filePath("consonants.chosuta");saveProject(p,source);Window w;w.show();w.openPath(source);QTRY_COMPARE(w.currentProject().generated.size(),5);if(!p.audioPath.isEmpty())QTRY_VERIFY(w.findChild<QPushButton *>("correctWaveformTiming")->isEnabled());
        QTimer::singleShot(0,&w,[&]{
            auto dialog=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(dialog);auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});dialog->findChild<QTabWidget *>("mouthSettingsPages")->setCurrentIndex(2);auto editor=dialog->findChild<QWidget *>("consonantEditor");QVERIFY(editor->isHidden());QTest::mouseClick(dialog->findChild<QToolButton *>("consonantCustomize"),Qt::LeftButton);QVERIFY(editor->isVisible());
            QVERIFY(!dialog->findChild<QComboBox *>("consonantPreset"));
            auto language=dialog->findChild<QComboBox *>("consonantLanguage");language->setCurrentIndex(language->findData("ja"));auto table=dialog->findChild<QTableWidget *>("consonantTable");
            auto rowFor=[&](const QString &phone){for(int i=0;i<table->rowCount();++i)if(table->item(i,0)->text()==phone)return i;return -1;};
            int row=rowFor("d");QVERIFY(row>=0);auto choice=qobject_cast<QComboBox *>(table->cellWidget(row,2));QCOMPARE(choice->currentData().toString(),QString("open"));choice->setCurrentIndex(choice->findData("closed"));table->setCurrentCell(row,0);QTest::mouseClick(dialog->findChild<QPushButton *>("consonantResetOne"),Qt::LeftButton);row=rowFor("d");QCOMPARE(qobject_cast<QComboBox *>(table->cellWidget(row,2))->currentData().toString(),QString("open"));
            choice=qobject_cast<QComboBox *>(table->cellWidget(row,2));choice->setCurrentIndex(choice->findData("closed"));QTest::mouseClick(dialog->findChild<QPushButton *>("consonantResetAll"),Qt::LeftButton);row=rowFor("d");choice=qobject_cast<QComboBox *>(table->cellWidget(row,2));QCOMPARE(choice->currentData().toString(),QString("open"));choice->setCurrentIndex(choice->findData("closed"));
            if(qEnvironmentVariableIsSet("CHOSUTA_ADVANCED_ARTIFACTS")){QTest::qWait(20);QDir out(qEnvironmentVariable("CHOSUTA_ADVANCED_ARTIFACTS"));QVERIFY(out.mkpath("."));QVERIFY(dialog->grab().save(out.filePath("consonant-settings-ui.png")));}
            auto apply=dialog->findChild<QDialogButtonBox *>("mouthSettingsButtons")->button(QDialogButtonBox::Apply);QVERIFY(apply->text().contains("regenerate")||apply->text().contains(QString::fromUtf8("重新生成"))||apply->text().contains(QString::fromUtf8("再生成")));QTest::mouseClick(apply,Qt::LeftButton);
        });
        w.findChild<QAction *>("advancedMouthAction")->trigger();QTRY_COMPARE(w.currentProject().rules.consonants.overrides.value("ja:romaji:d"),QString("closed"));QCOMPARE(w.currentProject().generated[0].shape,QString("closed"));QVERIFY(w.currentProject().timing.sources.isEmpty());QCOMPARE(w.currentProject().overrides[edited.id].event.shape,QString("O"));QCOMPARE(AppearanceResolver(w.currentProject()).at(w.currentProject(),w.currentProject().effective(),.02).shape,QString("O"));
        const auto current=w.currentProject().rules.consonants;
        QTimer::singleShot(0,&w,[&]{auto dialog=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(dialog);auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});dialog->findChild<QTabWidget *>("mouthSettingsPages")->setCurrentIndex(2);QTest::mouseClick(dialog->findChild<QToolButton *>("consonantCustomize"),Qt::LeftButton);QTest::mouseClick(dialog->findChild<QPushButton *>("consonantResetAll"),Qt::LeftButton);dialog->reject();});w.findChild<QAction *>("advancedMouthAction")->trigger();QCOMPARE(w.currentProject().rules.consonants,current);
        const auto saved=dir.filePath("saved.chosuta");saveProject(w.currentProject(),saved);QCOMPARE(loadProject(saved).rules.consonants,current);
        auto timeline=w.findChild<Timeline *>("timeline");timeline->setFocus();w.activateWindow();QTest::qWait(10);QTest::keySequence(timeline,QKeySequence::Undo);QVERIFY(w.currentProject().rules.consonants.overrides.isEmpty());QCOMPARE(w.currentProject().generated[0].shape,QString("A"));QCOMPARE(w.currentProject().timing.sources.size(),p.timing.sources.size());
    }
    void mouthFolderImportWorkflow(){
        QTemporaryDir dir;auto p=mouthUiProject();QImage image(64,64,QImage::Format_RGBA8888);image.fill(Qt::cyan);QDir folder(dir.filePath("PNG 中文 $(literal)"));QVERIFY(folder.mkpath("Power"));QVERIFY(folder.mkpath("Soft"));QVERIFY(image.save(folder.filePath("Power/A.png")));QVERIFY(image.save(folder.filePath("Soft/O.png")));
        const auto source=dir.filePath("folder.chosuta");saveProject(p,source);Window w;w.show();w.openPath(source);QTRY_COMPARE(w.currentProject().generated.size(),4);bool commit=false;
        auto editFolder=[&]{auto dialog=w.findChild<QDialog *>("mouthSettingsDialog");QVERIFY(dialog);auto close=qScopeGuard([dialog]{if(dialog->isVisible())dialog->reject();});dialog->findChild<QTabWidget *>("mouthSettingsPages")->setCurrentIndex(3);dialog->findChild<QLineEdit *>("mouthImportFolder")->setText(folder.path());dialog->findChild<QPushButton *>("mouthScanFolder")->click();auto table=dialog->findChild<QTableWidget *>("mouthImportTable");QTRY_COMPARE(table->rowCount(),2);dialog->findChild<QPushButton *>("mouthApplyImport")->click();QCOMPARE(w.currentProject().assets,p.assets);
            if(qEnvironmentVariableIsSet("CHOSUTA_ADVANCED_ARTIFACTS")){QDir out(qEnvironmentVariable("CHOSUTA_ADVANCED_ARTIFACTS"));out.mkpath(".");dialog->grab().save(out.filePath("group-import.png"));}
            if(commit)dialog->findChild<QDialogButtonBox *>("mouthSettingsButtons")->button(QDialogButtonBox::Apply)->click();else dialog->reject();};
        QTimer::singleShot(0,&w,editFolder);w.findChild<QAction *>("advancedMouthAction")->trigger();QCOMPARE(w.currentProject().performance.groups.size(),1);
        commit=true;QTimer::singleShot(0,&w,editFolder);w.findChild<QAction *>("advancedMouthAction")->trigger();QCOMPARE(w.currentProject().performance.groups.size(),3);QCOMPARE(w.currentProject().assets.size(),2);auto timeline=w.findChild<Timeline *>("timeline");timeline->setFocus();w.activateWindow();QTest::qWait(10);QTest::keySequence(timeline,QKeySequence::Undo);QCOMPARE(w.currentProject().assets,p.assets);
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
