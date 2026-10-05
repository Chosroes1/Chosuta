// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QtWidgets>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QFutureWatcher>
#include "core/model.h"
#include "render/render.h"
#include "timeline.h"
#include "preferences.h"
#include "render/audio.h"
namespace chosuta {
    struct LoadResult {
        Project project;
        QString error;
        bool cancelled=false;
    };
    class Window:public QMainWindow {
        Q_OBJECT
        public:
        explicit Window();
        void openPath(const QString &path);
        void attachAudioPath(const QString &path);
        const Project &currentProject() const {
            return project;
        }
        bool runSmokeWorkflow(const QString &directory);
        protected:
        bool eventFilter(QObject *,QEvent *)override;
        void closeEvent(QCloseEvent *)override;
        void resizeEvent(QResizeEvent *)override;
        private:
        Project project;
        QString projectPath,uiLanguage;
        Preferences preferences;
        QScrollArea *timelineScroll=nullptr;
        QWidget *canvasPage=nullptr;
        QLabel *canvasLabel=nullptr;
        QSpinBox *canvasWidth=nullptr,*canvasHeight=nullptr;
        QDoubleSpinBox *canvasDuration=nullptr,*characterScale=nullptr,*characterX=nullptr,*characterY=nullptr;
        QComboBox *canvasAspect=nullptr,*backgroundFit=nullptr;
        QCheckBox *canvasTransparent=nullptr;
        QLineEdit *backgroundPath=nullptr;
        QPushButton *backgroundColor=nullptr;
        QUndoStack history;
        Timeline *timeline=nullptr;
        QTableWidget *tracks=nullptr,*assets=nullptr,*special=nullptr;
        QTabWidget *tabs=nullptr;
        QLabel *preview=nullptr,*timeLabel=nullptr,*selectionLabel=nullptr;
        QComboBox *language=nullptr,*fallback=nullptr,*shape=nullptr,*anchor=nullptr;
        QCheckBox *takeover=nullptr,*lock=nullptr;
        QDoubleSpinBox *start=nullptr,*end=nullptr,*consonant=nullptr,*position=nullptr;
        QPlainTextEdit *diagnostics=nullptr;
        QPushButton *playButton=nullptr,*cancelButton=nullptr;
        QProgressBar *progress=nullptr;
        std::unique_ptr<QMediaPlayer> player;
        std::unique_ptr<QAudioOutput> audioOutput;
        QTimer playbackTimer;
        QElapsedTimer playbackClock;
        double playTime=0,playOrigin=0;
        bool refreshing=false,playing=false,busy=false,loadIsRegenerate=false;
        std::unique_ptr<Scene>scene;
        std::shared_ptr<std::atomic_bool>cancel;
        QFutureWatcher<LoadResult>loadWatcher;
        QFutureWatcher<ExportResult>exportWatcher;
        QFutureWatcher<AudioInfo>audioWatcher;
        QString activePath;
        bool audioStarted=false;
        QString trText(const char *key)const;
        void buildUi();
        void refresh();
        void refreshPreview();
        void refreshSelection();
        void collectRules(Project &p)const;
        void change(const QString &label,const std::function<void(Project &)> &fn);
        void assignProject(Project p);
        void setBusy(bool value);
        void error(const QString &message);
        bool confirmDiscard();
        bool save(bool choosePath=false);
        void openDialog();
        void regenerate();
        void importImages();
        void createDemo();
        void relocateAssets();
        void addCustom();
        void attachAudio();
        void editSelection();
        void splitSelection();
        void mergeSelection();
        void offsetSelection();
        void correctReading();
        void resetOverrides();
        void showExportSettings();
        void showPreferences();
        QWidget *buildCanvasPanel();
        void refreshCanvas();
        void followCursor(bool force=false);
        void startExport();
        void seek(double time);
        void togglePlayback();
        void stopPlayback();
        void syncAudio();
        void ensureAudio();
        friend class ProjectCommand;
    };
}
