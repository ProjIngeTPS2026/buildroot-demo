#pragma once

#include "common/models.h"
#include "common/wav_utils.h"
#include "portable_console/asset_library_model.h"
#include "portable_console/base_directory_model.h"
#include "portable_console/offline_tile_server.h"
#include "portable_console/portable_config.h"

#include <memory>

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QAudioInput>
#else
#include <QAudioOutput>
#include <QAudioSource>
#endif
#include <QJsonDocument>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QUdpSocket>

namespace apping {

class PortableController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractListModel* baseModel READ baseModel CONSTANT)
    Q_PROPERTY(QAbstractListModel* libraryModel READ libraryModel CONSTANT)
    Q_PROPERTY(QString mapTitle READ mapTitle CONSTANT)
    Q_PROPERTY(QString mapImageSource READ mapImageSource CONSTANT)
    Q_PROPERTY(QString mapTilesBaseUrl READ mapTilesBaseUrl CONSTANT)
    Q_PROPERTY(double mapMinLat READ mapMinLat CONSTANT)
    Q_PROPERTY(double mapMaxLat READ mapMaxLat CONSTANT)
    Q_PROPERTY(double mapMinLon READ mapMinLon CONSTANT)
    Q_PROPERTY(double mapMaxLon READ mapMaxLon CONSTANT)
    Q_PROPERTY(double mapCenterLat READ mapCenterLat CONSTANT)
    Q_PROPERTY(double mapCenterLon READ mapCenterLon CONSTANT)
    Q_PROPERTY(double mapDefaultZoom READ mapDefaultZoom CONSTANT)
    Q_PROPERTY(double mapMinZoom READ mapMinZoom CONSTANT)
    Q_PROPERTY(double mapMaxZoom READ mapMaxZoom CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool liveMegaphoneActive READ liveMegaphoneActive NOTIFY liveMegaphoneActiveChanged)
    Q_PROPERTY(bool antiFeedbackEnabled READ antiFeedbackEnabled NOTIFY antiFeedbackEnabledChanged)
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(bool hasPendingRecording READ hasPendingRecording NOTIFY pendingRecordingChanged)
    Q_PROPERTY(QString pendingRecordingPath READ pendingRecordingPath NOTIFY pendingRecordingChanged)
    Q_PROPERTY(QString pendingRecordingTitle READ pendingRecordingTitle NOTIFY pendingRecordingChanged)
    Q_PROPERTY(QString pendingRecordingDuration READ pendingRecordingDuration NOTIFY pendingRecordingChanged)
    Q_PROPERTY(int recordingInputLevel READ recordingInputLevel NOTIFY recordingInputLevelChanged)
    Q_PROPERTY(bool recordingSaturated READ recordingSaturated NOTIFY recordingInputLevelChanged)
    Q_PROPERTY(int previewProgress READ previewProgress NOTIFY previewProgressChanged)
    Q_PROPERTY(bool previewPlaying READ previewPlaying NOTIFY previewProgressChanged)
    Q_PROPERTY(qint64 previewWaveformPositionMs READ previewWaveformPositionMs NOTIFY previewProgressChanged)

public:
    explicit PortableController(const PortableConfig& config, QObject* parent = nullptr);
    ~PortableController() override;

    QAbstractListModel* baseModel();
    QAbstractListModel* libraryModel();

    QString mapTitle() const;
    QString mapImageSource() const;
    QString mapTilesBaseUrl() const;
    double mapMinLat() const;
    double mapMaxLat() const;
    double mapMinLon() const;
    double mapMaxLon() const;
    double mapCenterLat() const;
    double mapCenterLon() const;
    double mapDefaultZoom() const;
    double mapMinZoom() const;
    double mapMaxZoom() const;

    QString statusMessage() const;
    bool liveMegaphoneActive() const;
    bool antiFeedbackEnabled() const;
    bool recording() const;
    bool hasPendingRecording() const;
    QString pendingRecordingPath() const;
    QString pendingRecordingTitle() const;
    QString pendingRecordingDuration() const;
    qint64 pendingRecordingDurationMs() const;
    QVector<qreal> pendingRecordingPeaks() const;
    qint64 pendingRecordingSelectionStartMs() const;
    qint64 pendingRecordingSelectionEndMs() const;
    int recordingInputLevel() const;
    bool recordingSaturated() const;
    int previewProgress() const;
    bool previewPlaying() const;
    qint64 previewWaveformPositionMs() const;

    Q_INVOKABLE void toggleBaseSelection(int row);
    Q_INVOKABLE void selectAllOnlineBases();
    Q_INVOKABLE void clearBaseSelection();
    Q_INVOKABLE void refreshLibraries();
    Q_INVOKABLE void playAsset(const QString& assetId,
                               const QString& mode,
                               int intervalMs,
                               int durationMs);
    Q_INVOKABLE void stopSelected();
    Q_INVOKABLE void startLiveMegaphone();
    Q_INVOKABLE void stopLiveMegaphone();
    Q_INVOKABLE void setAntiFeedbackEnabled(bool enabled);
    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void setPendingRecordingTitle(const QString& title);
    Q_INVOKABLE void previewRecording();
    Q_INVOKABLE void setPendingRecordingSelection(qint64 startMs, qint64 endMs);
    Q_INVOKABLE void applyPendingRecordingCrop();
    Q_INVOKABLE void broadcastPendingRecording(bool saveToLibraryAfter);
    Q_INVOKABLE void savePendingRecordingToLibraries();
    Q_INVOKABLE void discardPendingRecording();

    QVector<AudioAssetMetadata> libraryForBase(const BaseSnapshot& base);
    bool deleteAssetFromBase(const BaseSnapshot& base, const QString& assetId, QString* error = nullptr);
    bool updateBaseInfo(const BaseSnapshot& base,
                        const QString& name,
                        const QString& description,
                        double latitude,
                        double longitude,
                        QString* error = nullptr);
    int syncMissingAssetsToBase(const BaseSnapshot& target, QString* error = nullptr);

signals:
    void statusMessageChanged();
    void liveMegaphoneActiveChanged();
    void antiFeedbackEnabledChanged();
    void recordingChanged();
    void pendingRecordingChanged();
    void recordingInputLevelChanged();
    void previewProgressChanged();

private:
    struct HttpResult {
        bool ok = false;
        int statusCode = 0;
        QJsonDocument json;
        QString error;
    };

    PortableConfig m_config;
    MapMetadata m_mapMetadata;
    QUdpSocket m_discoverySocket;
    QTimer m_staleTimer;
    QTimer m_libraryRefreshTimer;
    QNetworkAccessManager m_networkManager;
    BaseDirectoryModel m_baseModel;
    AssetLibraryModel m_assetLibraryModel;
    QMediaPlayer m_previewPlayer;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioOutput m_previewAudioOutput;
#endif
    QProcess m_megaphoneProcess;
    OfflineTileServer m_offlineTileServer;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    std::unique_ptr<QAudioInput> m_audioSource;
#else
    std::unique_ptr<QAudioSource> m_audioSource;
#endif
    std::unique_ptr<WaveCaptureDevice> m_captureDevice;
    QVector<BaseSnapshot> m_preparedBases;
    QString m_statusMessage;
    QString m_pendingRecordingPath;
    QString m_pendingRecordingTitle;
    qint64 m_pendingRecordingDurationMs = 0;
    QVector<qreal> m_pendingRecordingPeaks;
    qint64 m_pendingRecordingSelectionStartMs = 0;
    qint64 m_pendingRecordingSelectionEndMs = 0;
    qint64 m_previewSourceSelectionStartMs = 0;
    QString m_selectedRecordingCachePath;
    int m_recordingInputLevel = 0;
    bool m_recordingSaturated = false;
    int m_previewProgress = 0;
    bool m_previewPlaying = false;
    bool m_liveMegaphoneActive = false;
    bool m_antiFeedbackEnabled = false;
    bool m_recording = false;
    bool m_megaphoneAutoStop = false;
    bool m_saveRecordingAfterMegaphone = false;
    bool m_explicitStopRequested = false;
    int m_echoCancelModuleId = -1;
    bool m_echoCancelModuleOwned = false;

    void processDiscoveryDatagrams();
    void setStatusMessage(const QString& message);
    void refreshLibrariesInternal(bool announce);
    HttpResult requestJson(const QNetworkRequest& request,
                           const QByteArray& method,
                           const QJsonObject* body = nullptr);
    HttpResult getJson(const QUrl& url);
    HttpResult postJson(const QUrl& url, const QJsonObject& body);
    QVector<BaseSnapshot> selectedOnlineBases() const;
    QString uploadRecordingLabel() const;
    QString uploadRecordingFileName() const;
    QString effectiveLiveInputUri(const QString& configuredInputUri) const;
    QString pulseSourceNameFromUri(const QString& uri) const;
    QString defaultPulseSourceName() const;
    QString defaultPulseSinkName() const;
    QProcessEnvironment audioProcessEnvironment() const;
    QString runPactl(const QStringList& arguments, bool* ok = nullptr) const;
    bool pulseSourceExists(const QString& sourceName) const;
    void unloadEchoCancelModulesByName();
    bool ensureEchoCancelSourceReady(QString* error = nullptr);
    void unloadEchoCancelSource();
    bool startMegaphoneWithInput(const QString& inputUri,
                                 bool autoStop,
                                 bool saveToLibraryAfter);
    void finalizeMegaphoneSession();
    bool uploadPendingRecordingToBases(const QVector<BaseSnapshot>& bases);
    void updatePreviewProgress();
    void clearSelectedRecordingCache();
    bool selectionCoversFullRecording() const;
    void resetPendingRecordingAnalysis();
    void loadPendingRecordingAnalysis(bool resetSelection);
    QString selectedPendingRecordingPath(QString* error = nullptr);
    std::optional<QPair<AudioAssetMetadata, QByteArray>> downloadAsset(const BaseSnapshot& base,
                                                                       const QString& assetId,
                                                                       QString* error = nullptr);
    bool uploadAssetBytesToBase(const BaseSnapshot& base,
                                const AudioAssetMetadata& asset,
                                const QByteArray& bytes,
                                QString* error = nullptr);
};

} // namespace apping
