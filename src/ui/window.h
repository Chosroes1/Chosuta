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
#include "preview.h"
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
        ~Window()override;
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
        QSplitter *timelineSplitter=nullptr;
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
        PreviewCanvas *preview=nullptr;
        QLabel *timeLabel=nullptr,*selectionLabel=nullptr;
        QCheckBox *subtitleEnabled=nullptr,*subtitleTrackEnabled=nullptr,*subtitleAlign=nullptr,*subtitleBold=nullptr,*subtitleItalic=nullptr,*subtitleOutline=nullptr,*subtitleOwnStyle=nullptr;
        QWidget *subtitlePage=nullptr,*subtitleCuePanel=nullptr;
        QComboBox *subtitleTrackChoice=nullptr,*subtitleSource=nullptr,*subtitleAnchor=nullptr,*subtitleFirst=nullptr,*subtitleLast=nullptr,*layoutTarget=nullptr;
        QFontComboBox *subtitleFont=nullptr;
        QLineEdit *subtitleName=nullptr;
        QPlainTextEdit *subtitleText=nullptr;
        QDoubleSpinBox *subtitleStart=nullptr,*subtitleEnd=nullptr,*subtitleX=nullptr,*subtitleY=nullptr,*subtitleSize=nullptr,*subtitleWidth=nullptr;
        QComboBox *subtitleTextAlign=nullptr;
        QPushButton *subtitleColor=nullptr;
        QLabel *subtitleHint=nullptr;
        QColor selectedSubtitleColor=Qt::white;
        QString subtitleTrackId,subtitleCueId;
        int subtitleTextSession=0;
        QComboBox *language=nullptr,*fallback=nullptr,*shape=nullptr,*anchor=nullptr;
        QCheckBox *takeover=nullptr,*lock=nullptr;
        QCheckBox *advancedMouth=nullptr;
        QDoubleSpinBox *start=nullptr,*end=nullptr,*consonant=nullptr,*consonantLimit=nullptr,*position=nullptr;
        QPlainTextEdit *diagnostics=nullptr;
        QPushButton *playButton=nullptr,*cancelButton=nullptr;
        QProgressBar *progress=nullptr;
        std::unique_ptr<QMediaPlayer> player;
        std::unique_ptr<QAudioOutput> audioOutput;
        QTimer playbackTimer;
        QElapsedTimer playbackClock;
        double playTime=0,playOrigin=0;
        bool refreshing=false,playing=false,busy=false,loadIsRegenerate=false,mouthSettingsOpen=false;
        std::unique_ptr<Scene>scene;
        std::shared_ptr<std::atomic_bool>cancel;
        QFutureWatcher<LoadResult>loadWatcher;
        QFutureWatcher<ExportResult>exportWatcher;
        QFutureWatcher<AudioInfo>audioWatcher;
        QFutureWatcher<WaveformResult>waveformWatcher;
        QFutureWatcher<WaveCorrectionResult>correctionWatcher;
        std::shared_ptr<const Waveform> waveform;
        QString waveformKey,waveformMessage;
        QWidget *waveformControls=nullptr;
        QDoubleSpinBox *timingMaxShift=nullptr,*timingMaxDuration=nullptr;
        QPushButton *correctTimingButton=nullptr,*revertTimingButton=nullptr,*reloadWaveformButton=nullptr;
        void ensureWaveform();
        void refreshWaveform();
        void correctTiming();
        void revertTiming();
        void updateTimelineMinimum();
        QString activePath;
        bool audioStarted=false;
        QString trText(const char *key)const;
        void buildUi();
        void refresh(bool timingOnly=false);
        void refreshPreview();
        void refreshSelection();
        void collectRules(Project &p)const;
        void change(const QString &label,const std::function<void(Project &)> &fn,bool timingOnly=false);
        void assignProject(Project p,bool timingOnly=false);
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
        void deleteSelection();
        void deleteSubtitleCue();
        void clearObjectSelection();
        void saveViewPreferences();
        void splitSelection();
        void mergeSelection();
        void offsetSelection();
        void correctReading();
        void resetOverrides();
        void showExportSettings();
        void showPreferences();
        void showAdvancedSettings();
        void showMouthSettings();
        QWidget *buildCanvasPanel();
        void refreshCanvas();
        QWidget *buildSubtitlePanel();
        void refreshSubtitles();
        void selectSubtitle(const QString &,const QString &,bool editText=false);
        void addSubtitleTrack();
        void addSubtitle(int track,double time);
        void applySubtitleProperties();
        void writeSubtitleText(const QString &track,const QString &cue,const QString &text);
        void alignSubtitleRange();
        void changeSubtitles(const QString &,const std::function<void(Project &)> &);
        void updateLayoutTarget();
        void followCursor(bool force=false);
        void startExport();
        void seek(double time);
        void togglePlayback();
        void stopPlayback();
        void syncAudio();
        void ensureAudio();
        friend class ProjectCommand;
        friend class SubtitleTextCommand;
    };
}
