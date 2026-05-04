#pragma once

#include "common/models.h"
#include "portable_console/portable_config.h"

#include <functional>
#include <memory>

#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QModelIndex>
#include <QPoint>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QTabWidget>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace apping {

class BaseMapWidget;
class BaseDirectoryModel;
class AudioWaveformWidget;
class PortableController;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const PortableConfig& config, QWidget* parent = nullptr);
    ~MainWindow() override;
    bool saveReportScreenshots(const QString& outputDirectory);

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class ConfirmationScope {
        None,
        Messages,
        Live,
        Recording,
    };

    std::unique_ptr<Ui::MainWindow> m_ui;
    std::unique_ptr<PortableController> m_controller;
    BaseMapWidget* m_mapWidget = nullptr;
    AudioWaveformWidget* m_recordingWaveformWidget = nullptr;
    QFrame* m_confirmationOverlay = nullptr;
    QFrame* m_confirmationCard = nullptr;
    QLabel* m_confirmationTitleLabel = nullptr;
    QLabel* m_confirmationTextLabel = nullptr;
    QScrollArea* m_confirmationBasesScrollArea = nullptr;
    QLabel* m_confirmationBasesLabel = nullptr;
    QPushButton* m_confirmationConfirmButton = nullptr;
    QPushButton* m_confirmationCancelButton = nullptr;
    QFrame* m_baseSettingsOverlay = nullptr;
    QFrame* m_baseSettingsCard = nullptr;
    QLabel* m_baseSettingsTitleLabel = nullptr;
    QTabWidget* m_baseSettingsTabWidget = nullptr;
    QWidget* m_baseInfoTab = nullptr;
    QWidget* m_baseAudioTab = nullptr;
    QLineEdit* m_baseNameLineEdit = nullptr;
    QLineEdit* m_baseDescriptionLineEdit = nullptr;
    QLineEdit* m_baseLatitudeLineEdit = nullptr;
    QLineEdit* m_baseLongitudeLineEdit = nullptr;
    QLabel* m_baseStatusInfoLabel = nullptr;
    QLabel* m_baseNetworkInfoLabel = nullptr;
    QListWidget* m_baseAudioListWidget = nullptr;
    QPushButton* m_baseSettingsSyncButton = nullptr;
    QPushButton* m_baseSettingsEditButton = nullptr;
    QPushButton* m_baseSettingsCloseButton = nullptr;
    QPushButton* m_baseAudioSaveButton = nullptr;
    QPushButton* m_baseAudioCloseButton = nullptr;
    QPushButton* m_baseAudioDeleteButton = nullptr;
    BaseSnapshot m_currentSettingsBase;
    QString m_selectedAssetId;
    ConfirmationScope m_confirmationScope = ConfirmationScope::None;
    std::function<void()> m_pendingConfirmationAction;
    QString m_pendingConfirmationText;
    QString m_pendingConfirmationDetail;
    QTimer m_baseLongPressTimer;
    QModelIndex m_baseLongPressIndex;
    QPoint m_baseLongPressPos;
    bool m_basePointerMoved = false;
    bool m_ignoreNextBaseClick = false;

    void installMapWidget();
    void installRecordingWaveform();
    void configureViews();
    void setupTouchScrolling();
    void setupConfirmationOverlay();
    void setupBaseSettingsOverlay();
    void connectSignals();
    void applyCompactStyle();
    void updateAdaptiveMetrics();
    void updateStatus();
    void updateLiveMegaphoneButton();
    void updateLivePanel();
    void updateRecordingButtons();
    void updatePendingRecordingPanel();
    void updateRecordingMeters();
    void updateAssetButtons();
    void updateBaseSelectionControls();
    void setupIcons();
    void rememberSelectedAsset();
    void restoreSelectedAssetSelection();
    void playSelectedAsset(const QString& mode);
    void requestBroadcastConfirmation(ConfirmationScope scope,
                                      const QString& actionText,
                                      std::function<void()> action,
                                      const QString& detailText = QString());
    void refreshBroadcastConfirmationPanel();
    void clearBroadcastConfirmation();
    void executePendingBroadcastConfirmation();
    void updateConfirmationOverlayGeometry();
    void updateBaseSettingsOverlayGeometry();
    void showBaseSettings(const QModelIndex& index);
    void hideBaseSettings();
    void populateBaseAudioList();
    void saveBaseSettings();
    void deleteSelectedBaseAudio();
    void syncCurrentBaseAudio();
    QStringList selectedBroadcastBaseNames() const;
    int playbackIntervalMs(bool* ok = nullptr) const;
    int playbackDurationMs(bool* ok = nullptr) const;
    QString selectedAssetId() const;
    BaseDirectoryModel* baseDirectoryModel() const;
};

} // namespace apping
