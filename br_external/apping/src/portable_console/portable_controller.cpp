#include "portable_console/portable_controller.h"

#include "common/app_paths.h"
#include "common/json_protocol.h"
#include "common/models.h"
#include "common/roc_integration.h"

#include <algorithm>

#include <QDir>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QMediaDevices>
#else
#include <QAudioDeviceInfo>
#endif
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <QUrl>

#include <unistd.h>

namespace apping {

namespace {

QString durationToText(qint64 durationMs) {
    const qint64 totalSeconds = durationMs / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString sanitizeUploadStem(QString text) {
    text = text.trimmed();
    if (text.isEmpty()) {
        return QStringLiteral("message");
    }

    text.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("-"));
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("-"));
    text.replace(QRegularExpression(QStringLiteral("-+")), QStringLiteral("-"));
    text.remove(QRegularExpression(QStringLiteral("^-|-$")));
    return text.isEmpty() ? QStringLiteral("message") : text;
}

} // namespace

PortableController::PortableController(const PortableConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_previewPlayer(this)
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    , m_previewAudioOutput(this)
#endif
    , m_offlineTileServer(this)
    , m_staleTimer(this) {
    QString error;
    const auto mapDocument = jsonDocumentFromFile(m_config.mapMetadataPath, &error);
    if (mapDocument.isObject()) {
        const auto metadata = mapMetadataFromJson(mapDocument.object());
        if (metadata) {
            m_mapMetadata = *metadata;
        }
    }

    if (!m_mapMetadata.offlineTilesRelativePath.isEmpty()) {
        const QString tileRoot = AppPaths::resolvePath(m_mapMetadata.offlineTilesRelativePath);
        if (m_offlineTileServer.start(tileRoot, &error)) {
            qInfo().noquote()
                << QStringLiteral("Tuiles de carte hors ligne servies depuis %1 sur %2")
                       .arg(tileRoot, m_offlineTileServer.baseUrl());
        } else {
            qWarning().noquote()
                << QStringLiteral("Tuiles de carte hors ligne indisponibles : %1").arg(error);
        }
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_previewPlayer.setAudioOutput(&m_previewAudioOutput);
    m_previewAudioOutput.setVolume(1.0);
#else
    m_previewPlayer.setVolume(100);
#endif
    connect(&m_previewPlayer, &QMediaPlayer::positionChanged, this, [this]() {
        updatePreviewProgress();
    });
    connect(&m_previewPlayer, &QMediaPlayer::durationChanged, this, [this]() {
        updatePreviewProgress();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(&m_previewPlayer, &QMediaPlayer::playbackStateChanged, this, [this]() {
        const bool playing = m_previewPlayer.playbackState() == QMediaPlayer::PlayingState;
        if (m_previewPlaying != playing) {
            m_previewPlaying = playing;
            emit previewProgressChanged();
        }
        updatePreviewProgress();
    });
#else
    connect(&m_previewPlayer, &QMediaPlayer::stateChanged, this, [this](QMediaPlayer::State state) {
        const bool playing = state == QMediaPlayer::PlayingState;
        if (m_previewPlaying != playing) {
            m_previewPlaying = playing;
            emit previewProgressChanged();
        }
        updatePreviewProgress();
    });
#endif

    m_discoverySocket.bind(QHostAddress::AnyIPv4,
                           m_config.discoveryPort,
                           QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    connect(&m_discoverySocket,
            &QUdpSocket::readyRead,
            this,
            &PortableController::processDiscoveryDatagrams);

    m_staleTimer.setInterval(2000);
    connect(&m_staleTimer, &QTimer::timeout, this, [this]() { m_baseModel.expireStale(); });
    m_staleTimer.start();

    m_libraryRefreshTimer.setSingleShot(true);
    m_libraryRefreshTimer.setInterval(350);
    connect(&m_libraryRefreshTimer, &QTimer::timeout, this, [this]() {
        refreshLibrariesInternal(false);
    });

    m_megaphoneProcess.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_megaphoneProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString output = QString::fromUtf8(m_megaphoneProcess.readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) {
            qInfo().noquote() << QStringLiteral("[roc_send]") << output;
        }
    });
    connect(&m_megaphoneProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_explicitStopRequested) {
            return;
        }
        qWarning().noquote()
            << QStringLiteral("[roc_send] process error: %1").arg(m_megaphoneProcess.errorString());
    });
    connect(&m_megaphoneProcess,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                const QString output =
                    QString::fromUtf8(m_megaphoneProcess.readAllStandardOutput()).trimmed();
                if (!output.isEmpty()) {
                    qInfo().noquote() << QStringLiteral("[roc_send]") << output;
                }
                if (!m_explicitStopRequested
                    && (exitStatus != QProcess::NormalExit || exitCode != 0)) {
                    qWarning().noquote()
                        << QStringLiteral("[roc_send] stopped unexpectedly: status=%1 code=%2")
                               .arg(exitStatus == QProcess::NormalExit
                                        ? QStringLiteral("normal")
                                        : QStringLiteral("crash"),
                                    QString::number(exitCode));
                }
                finalizeMegaphoneSession();
            });
}

PortableController::~PortableController() {
    m_previewPlayer.stop();
    clearSelectedRecordingCache();
    unloadEchoCancelSource();
}

QAbstractListModel* PortableController::baseModel() {
    return &m_baseModel;
}

QAbstractListModel* PortableController::libraryModel() {
    return &m_assetLibraryModel;
}

QString PortableController::mapTitle() const {
    return m_mapMetadata.title;
}

QString PortableController::mapImageSource() const {
    if (m_mapMetadata.imageRelativePath.isEmpty()) {
        return {};
    }
    return QUrl::fromLocalFile(AppPaths::resolvePath(m_mapMetadata.imageRelativePath)).toString();
}

QString PortableController::mapTilesBaseUrl() const {
    return m_offlineTileServer.baseUrl();
}

double PortableController::mapMinLat() const {
    return m_mapMetadata.minLat;
}

double PortableController::mapMaxLat() const {
    return m_mapMetadata.maxLat;
}

double PortableController::mapMinLon() const {
    return m_mapMetadata.minLon;
}

double PortableController::mapMaxLon() const {
    return m_mapMetadata.maxLon;
}

double PortableController::mapCenterLat() const {
    return m_mapMetadata.centerLat != 0.0
        ? m_mapMetadata.centerLat
        : (m_mapMetadata.minLat + m_mapMetadata.maxLat) / 2.0;
}

double PortableController::mapCenterLon() const {
    return m_mapMetadata.centerLon != 0.0
        ? m_mapMetadata.centerLon
        : (m_mapMetadata.minLon + m_mapMetadata.maxLon) / 2.0;
}

double PortableController::mapDefaultZoom() const {
    return m_mapMetadata.defaultZoom;
}

double PortableController::mapMinZoom() const {
    return m_mapMetadata.minZoom;
}

double PortableController::mapMaxZoom() const {
    return m_mapMetadata.maxZoom;
}

QString PortableController::statusMessage() const {
    return m_statusMessage;
}

bool PortableController::liveMegaphoneActive() const {
    return m_liveMegaphoneActive;
}

bool PortableController::antiFeedbackEnabled() const {
    return m_antiFeedbackEnabled;
}

bool PortableController::recording() const {
    return m_recording;
}

bool PortableController::hasPendingRecording() const {
    return !m_pendingRecordingPath.isEmpty();
}

QString PortableController::pendingRecordingPath() const {
    return m_pendingRecordingPath;
}

QString PortableController::pendingRecordingTitle() const {
    return m_pendingRecordingTitle;
}

QString PortableController::pendingRecordingDuration() const {
    return durationToText(m_pendingRecordingDurationMs);
}

qint64 PortableController::pendingRecordingDurationMs() const {
    return m_pendingRecordingDurationMs;
}

QVector<qreal> PortableController::pendingRecordingPeaks() const {
    return m_pendingRecordingPeaks;
}

qint64 PortableController::pendingRecordingSelectionStartMs() const {
    return m_pendingRecordingSelectionStartMs;
}

qint64 PortableController::pendingRecordingSelectionEndMs() const {
    return m_pendingRecordingSelectionEndMs;
}

int PortableController::recordingInputLevel() const {
    return m_recordingInputLevel;
}

bool PortableController::recordingSaturated() const {
    return m_recordingSaturated;
}

int PortableController::previewProgress() const {
    return m_previewProgress;
}

bool PortableController::previewPlaying() const {
    return m_previewPlaying;
}

qint64 PortableController::previewWaveformPositionMs() const {
    if (!m_previewPlaying && m_previewPlayer.position() <= 0) {
        return -1;
    }
    return m_previewSourceSelectionStartMs + m_previewPlayer.position();
}

void PortableController::toggleBaseSelection(int row) {
    m_baseModel.toggleSelection(row);
}

void PortableController::selectAllOnlineBases() {
    m_baseModel.selectAllOnline();
}

void PortableController::clearBaseSelection() {
    m_baseModel.clearSelection();
}

void PortableController::refreshLibraries() {
    refreshLibrariesInternal(true);
}

void PortableController::refreshLibrariesInternal(bool announce) {
    QHash<QString, AudioAssetMetadata> merged;
    const QVector<BaseSnapshot> onlineBases = m_baseModel.onlineSnapshots();
    for (const BaseSnapshot& base : onlineBases) {
        QUrl url = controlUrl(base);
        url.setPath(QStringLiteral("/api/v1/library"));
        const HttpResult result = getJson(url);
        if (!result.ok || !result.json.isObject()) {
            continue;
        }
        for (const QJsonValue& value : result.json.object().value(QStringLiteral("assets")).toArray()) {
            const auto asset = audioAssetFromJson(value.toObject());
            if (!asset) {
                continue;
            }
            AudioAssetMetadata mergedAsset = merged.value(asset->assetId, *asset);
            if (!mergedAsset.availableOn.contains(base.name)) {
                mergedAsset.availableOn.push_back(base.name);
            }
            merged.insert(asset->assetId, mergedAsset);
        }
    }

    QVector<AudioAssetMetadata> assets = merged.values().toVector();
    std::sort(assets.begin(), assets.end(), [](const AudioAssetMetadata& left, const AudioAssetMetadata& right) {
        return left.label.localeAwareCompare(right.label) < 0;
    });
    m_assetLibraryModel.setAssets(assets);
    if (announce) {
        setStatusMessage(QStringLiteral("Bibliothèque actualisée depuis %1 base(s)").arg(onlineBases.size()));
    }
}

void PortableController::playAsset(const QString& assetId,
                                   const QString& mode,
                                   int intervalMs,
                                   int durationMs) {
    const QVector<BaseSnapshot> bases = selectedOnlineBases();
    if (bases.isEmpty()) {
        setStatusMessage(QStringLiteral("Sélectionnez au moins une base en ligne"));
        return;
    }

    const PlaybackRequest request{assetId, mode, intervalMs, durationMs};
    QJsonObject payload = toJson(request);
    int successCount = 0;
    for (const BaseSnapshot& base : bases) {
        QUrl url = controlUrl(base);
        url.setPath(QStringLiteral("/api/v1/play"));
        const HttpResult result = postJson(url, payload);
        if (result.ok) {
            ++successCount;
        }
    }
    setStatusMessage(QStringLiteral("Lecture lancée sur %1/%2 base(s)").arg(successCount).arg(bases.size()));
}

void PortableController::stopSelected() {
    const QVector<BaseSnapshot> bases = selectedOnlineBases();
    if (bases.isEmpty()) {
        setStatusMessage(QStringLiteral("Sélectionnez au moins une base en ligne"));
        return;
    }

    int successCount = 0;
    for (const BaseSnapshot& base : bases) {
        QUrl url = controlUrl(base);
        url.setPath(QStringLiteral("/api/v1/stop"));
        if (postJson(url, QJsonObject{}).ok) {
            ++successCount;
        }
    }
    setStatusMessage(QStringLiteral("Arrêt envoyé à %1/%2 base(s)").arg(successCount).arg(bases.size()));
}

void PortableController::startLiveMegaphone() {
    unloadEchoCancelModulesByName();
    startMegaphoneWithInput(m_config.captureInputUri, false, false);
}

void PortableController::stopLiveMegaphone() {
    if (m_megaphoneProcess.state() == QProcess::NotRunning) {
        finalizeMegaphoneSession();
        return;
    }
    m_explicitStopRequested = true;
    m_megaphoneProcess.terminate();
    if (!m_megaphoneProcess.waitForFinished(1000)) {
        m_megaphoneProcess.kill();
        m_megaphoneProcess.waitForFinished(1000);
    }
}

void PortableController::setAntiFeedbackEnabled(bool enabled) {
    if (!enabled && !m_antiFeedbackEnabled) {
        return;
    }

    if (enabled) {
        unloadEchoCancelModulesByName();
        m_antiFeedbackEnabled = false;
        emit antiFeedbackEnabledChanged();
        setStatusMessage(QStringLiteral("Anti-larsen désactivé : direct micro standard utilisé"));
        return;
    }

    unloadEchoCancelModulesByName();
    m_antiFeedbackEnabled = false;
    emit antiFeedbackEnabledChanged();
    setStatusMessage(QStringLiteral("Anti-larsen désactivé"));
}

void PortableController::startRecording() {
    if (m_recording) {
        return;
    }
    m_previewPlayer.stop();
    clearSelectedRecordingCache();

    QDir().mkpath(m_config.recordingRoot);
    const QString targetPath =
        QDir(m_config.recordingRoot)
            .filePath(QDateTime::currentDateTimeUtc().toString(QStringLiteral("'recording-'yyyyMMdd-hhmmss'.wav'")));

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    format.setCodec(QStringLiteral("audio/pcm"));
    format.setSampleSize(16);
    format.setSampleType(QAudioFormat::SignedInt);
    format.setByteOrder(QAudioFormat::LittleEndian);
#else
    format.setSampleFormat(QAudioFormat::Int16);
#endif

    QString error;
    m_captureDevice = std::make_unique<WaveCaptureDevice>();
    if (!m_captureDevice->startCapture(targetPath, format, &error)) {
        m_captureDevice.reset();
        m_pendingRecordingPath.clear();
        resetPendingRecordingAnalysis();
        setStatusMessage(QStringLiteral("Échec de l'enregistrement : %1").arg(error));
        emit pendingRecordingChanged();
        return;
    }
    connect(m_captureDevice.get(), &WaveCaptureDevice::inputLevelChanged, this, [this]() {
        if (!m_captureDevice) {
            return;
        }
        m_recordingInputLevel = m_captureDevice->inputLevelPercent();
        m_recordingSaturated = m_captureDevice->saturated();
        emit recordingInputLevelChanged();
    });

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_audioSource =
        std::make_unique<QAudioInput>(QAudioDeviceInfo::defaultInputDevice(), format);
#else
    m_audioSource =
        std::make_unique<QAudioSource>(QMediaDevices::defaultAudioInput(), format);
#endif
    m_audioSource->start(m_captureDevice.get());
    m_pendingRecordingPath = targetPath;
    m_pendingRecordingTitle = QFileInfo(targetPath).completeBaseName();
    resetPendingRecordingAnalysis();
    m_recordingInputLevel = 0;
    m_recordingSaturated = false;
    m_recording = true;
    emit recordingChanged();
    emit pendingRecordingChanged();
    emit recordingInputLevelChanged();
    setStatusMessage(QStringLiteral("Enregistrement en cours"));
}

void PortableController::stopRecording() {
    if (!m_recording) {
        return;
    }

    m_audioSource->stop();
    m_captureDevice->finalize();
    loadPendingRecordingAnalysis(true);
    m_audioSource.reset();
    m_captureDevice.reset();
    m_recordingInputLevel = 0;
    m_recordingSaturated = false;
    m_recording = false;
    emit recordingChanged();
    emit pendingRecordingChanged();
    emit recordingInputLevelChanged();
    setStatusMessage(QStringLiteral("Enregistrement prêt pour écoute ou diffusion"));
}

void PortableController::setPendingRecordingTitle(const QString& title) {
    const QString trimmed = title.trimmed();
    if (m_pendingRecordingTitle == trimmed) {
        return;
    }
    m_pendingRecordingTitle = trimmed;
    emit pendingRecordingChanged();
}

void PortableController::previewRecording() {
    if (!hasPendingRecording()) {
        setStatusMessage(QStringLiteral("Aucun enregistrement disponible"));
        return;
    }
    QString error;
    const QString playbackPath = selectedPendingRecordingPath(&error);
    if (playbackPath.isEmpty()) {
        setStatusMessage(QStringLiteral("Pré-écoute impossible : %1").arg(error));
        return;
    }
    m_previewSourceSelectionStartMs = m_pendingRecordingSelectionStartMs;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_previewPlayer.setMedia(QUrl::fromLocalFile(playbackPath));
#else
    m_previewPlayer.setSource(QUrl::fromLocalFile(playbackPath));
#endif
    m_previewProgress = 0;
    emit previewProgressChanged();
    m_previewPlayer.play();
}

void PortableController::setPendingRecordingSelection(qint64 startMs, qint64 endMs) {
    if (!hasPendingRecording() || m_pendingRecordingDurationMs <= 0 || m_liveMegaphoneActive) {
        return;
    }

    const qint64 minSpanMs = std::min<qint64>(80, m_pendingRecordingDurationMs);
    const qint64 nextStart = std::clamp(startMs,
                                        qint64(0),
                                        std::max<qint64>(0, m_pendingRecordingDurationMs - minSpanMs));
    const qint64 nextEnd = std::clamp(endMs, nextStart + minSpanMs, m_pendingRecordingDurationMs);
    if (m_pendingRecordingSelectionStartMs == nextStart
        && m_pendingRecordingSelectionEndMs == nextEnd) {
        return;
    }

    m_previewPlayer.stop();
    m_previewProgress = 0;
    m_previewPlaying = false;
    m_previewSourceSelectionStartMs = nextStart;
    clearSelectedRecordingCache();
    m_pendingRecordingSelectionStartMs = nextStart;
    m_pendingRecordingSelectionEndMs = nextEnd;
    emit pendingRecordingChanged();
    emit previewProgressChanged();
}

void PortableController::applyPendingRecordingCrop() {
    if (!hasPendingRecording()) {
        setStatusMessage(QStringLiteral("Aucun enregistrement disponible"));
        return;
    }
    if (m_liveMegaphoneActive) {
        setStatusMessage(QStringLiteral("Impossible de rogner pendant une diffusion"));
        return;
    }
    if (selectionCoversFullRecording()) {
        setStatusMessage(QStringLiteral("La sélection couvre déjà tout l'enregistrement"));
        return;
    }

    m_previewPlayer.stop();
    clearSelectedRecordingCache();
    const QString sourcePath = m_pendingRecordingPath;
    const QString croppedPath =
        QDir(m_config.recordingRoot)
            .filePath(QStringLiteral("%1-crop.wav").arg(QFileInfo(sourcePath).completeBaseName()));

    QString error;
    if (!cropWavFile(sourcePath,
                     croppedPath,
                     m_pendingRecordingSelectionStartMs,
                     m_pendingRecordingSelectionEndMs,
                     &error)) {
        QFile::remove(croppedPath);
        setStatusMessage(QStringLiteral("Rognage impossible : %1").arg(error));
        return;
    }
    if (!QFile::remove(sourcePath)) {
        QFile::remove(croppedPath);
        setStatusMessage(QStringLiteral("Rognage impossible : ancien fichier verrouillé"));
        return;
    }
    if (!QFile::rename(croppedPath, sourcePath)) {
        setStatusMessage(QStringLiteral("Rognage impossible : remplacement du fichier échoué"));
        return;
    }

    loadPendingRecordingAnalysis(true);
    m_previewProgress = 0;
    m_previewPlaying = false;
    m_previewSourceSelectionStartMs = 0;
    emit pendingRecordingChanged();
    emit previewProgressChanged();
    setStatusMessage(QStringLiteral("Enregistrement rogné sur la sélection"));
}

void PortableController::broadcastPendingRecording(bool saveToLibraryAfter) {
    if (!hasPendingRecording()) {
        setStatusMessage(QStringLiteral("Aucun enregistrement disponible"));
        return;
    }
    QString error;
    const QString playbackPath = selectedPendingRecordingPath(&error);
    if (playbackPath.isEmpty()) {
        setStatusMessage(QStringLiteral("Diffusion impossible : %1").arg(error));
        return;
    }
    startMegaphoneWithInput(toRocFileUri(playbackPath), true, saveToLibraryAfter);
}

void PortableController::savePendingRecordingToLibraries() {
    if (!hasPendingRecording()) {
        setStatusMessage(QStringLiteral("Aucun enregistrement disponible"));
        return;
    }
    const QVector<BaseSnapshot> bases = selectedOnlineBases();
    if (bases.isEmpty()) {
        setStatusMessage(QStringLiteral("Sélectionnez au moins une base en ligne"));
        return;
    }
    if (uploadPendingRecordingToBases(bases)) {
        refreshLibraries();
    }
}

QVector<AudioAssetMetadata> PortableController::libraryForBase(const BaseSnapshot& base) {
    QUrl url = controlUrl(base);
    url.setPath(QStringLiteral("/api/v1/library"));
    const HttpResult result = getJson(url);
    if (!result.ok || !result.json.isObject()) {
        return {};
    }

    QVector<AudioAssetMetadata> assets;
    for (const QJsonValue& value : result.json.object().value(QStringLiteral("assets")).toArray()) {
        const auto asset = audioAssetFromJson(value.toObject());
        if (asset) {
            AudioAssetMetadata hydrated = *asset;
            hydrated.availableOn = QStringList{base.name};
            assets.push_back(hydrated);
        }
    }
    return assets;
}

bool PortableController::deleteAssetFromBase(const BaseSnapshot& base,
                                             const QString& assetId,
                                             QString* error) {
    QUrl url = controlUrl(base);
    url.setPath(QStringLiteral("/api/v1/library/delete"));
    const HttpResult result = postJson(url, QJsonObject{{QStringLiteral("asset_id"), assetId}});
    if (!result.ok) {
        if (error) {
            *error = result.error.isEmpty() ? QStringLiteral("Suppression impossible") : result.error;
        }
        return false;
    }
    refreshLibraries();
    return true;
}

bool PortableController::updateBaseInfo(const BaseSnapshot& base,
                                        const QString& name,
                                        const QString& description,
                                        double latitude,
                                        double longitude,
                                        QString* error) {
    QUrl url = controlUrl(base);
    url.setPath(QStringLiteral("/api/v1/config"));
    const QJsonObject body{
        {QStringLiteral("name"), name},
        {QStringLiteral("description"), description},
        {QStringLiteral("lat"), latitude},
        {QStringLiteral("lon"), longitude},
    };
    const HttpResult result = postJson(url, body);
    if (!result.ok) {
        if (error) {
            *error = result.error.isEmpty() ? QStringLiteral("Modification impossible") : result.error;
        }
        return false;
    }
    setStatusMessage(QStringLiteral("Informations de %1 mises à jour").arg(name));
    return true;
}

int PortableController::syncMissingAssetsToBase(const BaseSnapshot& target, QString* error) {
    const QVector<AudioAssetMetadata> targetAssets = libraryForBase(target);
    QSet<QString> targetLabels;
    QSet<QString> targetIds;
    for (const AudioAssetMetadata& asset : targetAssets) {
        targetLabels.insert(asset.label);
        targetIds.insert(asset.assetId);
    }

    int uploaded = 0;
    const QVector<BaseSnapshot> bases = m_baseModel.onlineSnapshots();
    for (const BaseSnapshot& source : bases) {
        if (source.baseId == target.baseId) {
            continue;
        }
        for (const AudioAssetMetadata& asset : libraryForBase(source)) {
            if (targetIds.contains(asset.assetId) || targetLabels.contains(asset.label)) {
                continue;
            }
            const auto downloaded = downloadAsset(source, asset.assetId, error);
            if (!downloaded) {
                continue;
            }
            if (uploadAssetBytesToBase(target, downloaded->first, downloaded->second, error)) {
                ++uploaded;
                targetLabels.insert(downloaded->first.label);
                targetIds.insert(downloaded->first.assetId);
            }
        }
    }

    refreshLibraries();
    setStatusMessage(QStringLiteral("%1 audio(s) synchronisé(s) vers %2").arg(uploaded).arg(target.name));
    return uploaded;
}

void PortableController::discardPendingRecording() {
    if (m_recording) {
        stopRecording();
    }
    m_previewPlayer.stop();
    clearSelectedRecordingCache();
    if (!m_pendingRecordingPath.isEmpty()) {
        QFile::remove(m_pendingRecordingPath);
    }
    m_pendingRecordingPath.clear();
    m_pendingRecordingTitle.clear();
    resetPendingRecordingAnalysis();
    m_previewProgress = 0;
    m_previewPlaying = false;
    m_previewSourceSelectionStartMs = 0;
    emit pendingRecordingChanged();
    emit previewProgressChanged();
    setStatusMessage(QStringLiteral("Message en attente supprimé"));
}

void PortableController::processDiscoveryDatagrams() {
    while (m_discoverySocket.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_discoverySocket.pendingDatagramSize()));
        m_discoverySocket.readDatagram(datagram.data(), datagram.size());

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(datagram, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            continue;
        }
        const QJsonObject object = document.object();
        if (object.value(QStringLiteral("kind")).toString() != QStringLiteral("apping_base_announce")) {
            continue;
        }

        const auto snapshot = baseSnapshotFromJson(object);
        if (!snapshot) {
            continue;
        }

        BaseSnapshot updated = *snapshot;
        updated.lastSeen = QDateTime::currentDateTimeUtc();
        const auto previous = m_baseModel.snapshotByBaseId(updated.baseId);
        const bool libraryChanged = !previous || previous->libraryRevision != updated.libraryRevision;
        m_baseModel.upsert(updated);
        if (libraryChanged) {
            m_libraryRefreshTimer.start();
        }
    }
}

void PortableController::setStatusMessage(const QString& message) {
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusMessageChanged();
}

PortableController::HttpResult PortableController::requestJson(const QNetworkRequest& request,
                                                               const QByteArray& method,
                                                               const QJsonObject* body) {
    HttpResult result;

    QNetworkReply* reply = nullptr;
    if (method == QByteArrayLiteral("GET")) {
        reply = m_networkManager.get(request);
    } else {
        QNetworkRequest mutableRequest(request);
        mutableRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                                 QStringLiteral("application/json"));
        reply = m_networkManager.sendCustomRequest(
            mutableRequest,
            method,
            body ? QJsonDocument(*body).toJson(QJsonDocument::Compact) : QByteArray("{}"));
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(m_config.requestTimeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        result.error = QStringLiteral("Délai d'attente dépassé");
        reply->deleteLater();
        return result;
    }

    result.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.ok = reply->error() == QNetworkReply::NoError
        && result.statusCode >= 200 && result.statusCode < 300;

    const QByteArray payload = reply->readAll();
    if (!payload.isEmpty()) {
        QJsonParseError parseError;
        result.json = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError && result.error.isEmpty()) {
            result.error = parseError.errorString();
        }
    }
    if (!result.ok && result.error.isEmpty()) {
        result.error = reply->errorString();
    }

    reply->deleteLater();
    return result;
}

PortableController::HttpResult PortableController::getJson(const QUrl& url) {
    return requestJson(QNetworkRequest(url), QByteArrayLiteral("GET"), nullptr);
}

PortableController::HttpResult PortableController::postJson(const QUrl& url,
                                                            const QJsonObject& body) {
    return requestJson(QNetworkRequest(url), QByteArrayLiteral("POST"), &body);
}

QVector<BaseSnapshot> PortableController::selectedOnlineBases() const {
    return m_baseModel.selectedOnlineSnapshots();
}

QString PortableController::uploadRecordingLabel() const {
    const QString title = m_pendingRecordingTitle.trimmed();
    return title.isEmpty()
        ? QFileInfo(m_pendingRecordingPath).completeBaseName()
        : title;
}

QString PortableController::uploadRecordingFileName() const {
    return sanitizeUploadStem(uploadRecordingLabel()) + QStringLiteral(".wav");
}

QString PortableController::effectiveLiveInputUri(const QString& configuredInputUri) const {
    const QString trimmed = configuredInputUri.trimmed();
    if (!trimmed.isEmpty() && trimmed != QStringLiteral("pulse://default")) {
        return trimmed;
    }

    const QString defaultSource = defaultPulseSourceName();
    return defaultSource.isEmpty()
        ? trimmed
        : QStringLiteral("pulse://%1").arg(defaultSource);
}

QString PortableController::pulseSourceNameFromUri(const QString& uri) const {
    if (!uri.startsWith(QStringLiteral("pulse://"))) {
        return {};
    }
    return uri.mid(QStringLiteral("pulse://").size()).trimmed();
}

QString PortableController::defaultPulseSourceName() const {
    bool ok = false;
    const QString output = runPactl({QStringLiteral("info")}, &ok);
    if (!ok) {
        return {};
    }

    for (const QString& line : output.split(QLatin1Char('\n'))) {
        if (line.startsWith(QStringLiteral("Default Source:"))) {
            return line.section(QLatin1Char(':'), 1).trimmed();
        }
    }
    return {};
}

QString PortableController::defaultPulseSinkName() const {
    bool ok = false;
    const QString output = runPactl({QStringLiteral("info")}, &ok);
    if (!ok) {
        return {};
    }

    for (const QString& line : output.split(QLatin1Char('\n'))) {
        if (line.startsWith(QStringLiteral("Default Sink:"))) {
            return line.section(QLatin1Char(':'), 1).trimmed();
        }
    }
    return {};
}

QProcessEnvironment PortableController::audioProcessEnvironment() const {
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();

    QString runtimeDir = environment.value(QStringLiteral("XDG_RUNTIME_DIR")).trimmed();
    if (runtimeDir.isEmpty()) {
        runtimeDir = QStringLiteral("/run/user/%1").arg(QString::number(getuid()));
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtimeDir);
    }

    if (environment.value(QStringLiteral("DBUS_SESSION_BUS_ADDRESS")).trimmed().isEmpty()) {
        environment.insert(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                           QStringLiteral("unix:path=%1/bus").arg(runtimeDir));
    }

    if (environment.value(QStringLiteral("PULSE_SERVER")).trimmed().isEmpty()) {
        environment.insert(QStringLiteral("PULSE_SERVER"),
                           QStringLiteral("unix:%1/pulse/native").arg(runtimeDir));
    }

    const QString defaultSource = defaultPulseSourceName();
    if (!defaultSource.isEmpty()) {
        environment.insert(QStringLiteral("PULSE_SOURCE"), defaultSource);
    }

    return environment;
}

QString PortableController::runPactl(const QStringList& arguments, bool* ok) const {
    QProcess process;
    process.setProgram(QStringLiteral("pactl"));
    process.setArguments(arguments);
    process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    process.start();
    if (!process.waitForStarted(2000)) {
        if (ok) {
            *ok = false;
        }
        return {};
    }
    process.closeWriteChannel();
    process.waitForFinished(4000);

    const bool success = process.exitStatus() == QProcess::NormalExit
        && process.exitCode() == 0;
    if (ok) {
        *ok = success;
    }
    const QString stdoutText = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    if (!success) {
        const QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return stderrText.isEmpty() ? stdoutText : stderrText;
    }
    return stdoutText;
}

bool PortableController::pulseSourceExists(const QString& sourceName) const {
    if (sourceName.isEmpty()) {
        return false;
    }

    bool ok = false;
    const QString output = runPactl({QStringLiteral("list"), QStringLiteral("short"), QStringLiteral("sources")}, &ok);
    if (!ok) {
        return false;
    }

    for (const QString& line : output.split(QLatin1Char('\n'))) {
        const QStringList columns = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                               Qt::SkipEmptyParts);
        if (columns.size() >= 2 && columns.at(1) == sourceName) {
            return true;
        }
    }
    return false;
}

void PortableController::unloadEchoCancelModulesByName() {
    bool ok = false;
    const QString output = runPactl({QStringLiteral("list"), QStringLiteral("short"), QStringLiteral("modules")}, &ok);
    if (!ok) {
        return;
    }

    for (const QString& line : output.split(QLatin1Char('\n'))) {
        if (!line.contains(QStringLiteral("module-echo-cancel"))
            || !line.contains(QStringLiteral("apping_ec_source"))) {
            continue;
        }

        const QStringList columns = line.split(QRegularExpression(QStringLiteral("\\s+")),
                                               Qt::SkipEmptyParts);
        if (columns.isEmpty()) {
            continue;
        }

        bool idOk = false;
        const int moduleId = columns.first().toInt(&idOk);
        if (!idOk) {
            continue;
        }

        bool unloadOk = false;
        runPactl({QStringLiteral("unload-module"), QString::number(moduleId)}, &unloadOk);
        Q_UNUSED(unloadOk);
    }

    if (m_echoCancelModuleId >= 0) {
        m_echoCancelModuleId = -1;
        m_echoCancelModuleOwned = false;
    }
}

bool PortableController::ensureEchoCancelSourceReady(QString* error) {
    const QString sourceName = QStringLiteral("apping_ec_source");
    unloadEchoCancelModulesByName();

    QStringList args{
        QStringLiteral("load-module"),
        QStringLiteral("module-echo-cancel"),
        QStringLiteral("aec_method=webrtc"),
        QStringLiteral("aec_args=analog_gain_control=0 digital_gain_control=0 noise_suppression=1"),
        QStringLiteral("format=s16le"),
        QStringLiteral("rate=48000"),
        QStringLiteral("channels=1"),
        QStringLiteral("source_name=apping_ec_source"),
        QStringLiteral("sink_name=apping_ec_sink"),
        QStringLiteral("source_properties=device.description=Apping-AntiLarsen-Source"),
        QStringLiteral("sink_properties=device.description=Apping-AntiLarsen-Sink"),
    };

    const QString sourceMaster = pulseSourceNameFromUri(m_config.captureInputUri);
    if (!sourceMaster.isEmpty()) {
        args << QStringLiteral("source_master=%1").arg(sourceMaster);
    }

    const QString sinkMaster = defaultPulseSinkName();
    if (!sinkMaster.isEmpty()) {
        args << QStringLiteral("sink_master=%1").arg(sinkMaster);
    }

    bool ok = false;
    const QString output = runPactl(args, &ok);
    if (!ok) {
        if (error) {
            *error = output.isEmpty() ? QStringLiteral("module-echo-cancel indisponible") : output;
        }
        return false;
    }

    bool idOk = false;
    const int moduleId = output.toInt(&idOk);
    if (!idOk) {
        if (error) {
            *error = output.isEmpty() ? QStringLiteral("réponse pactl invalide") : output;
        }
        return false;
    }

    m_echoCancelModuleId = moduleId;
    m_echoCancelModuleOwned = true;
    return true;
}

void PortableController::unloadEchoCancelSource() {
    if (!m_echoCancelModuleOwned || m_echoCancelModuleId < 0) {
        return;
    }

    bool ok = false;
    runPactl({QStringLiteral("unload-module"), QString::number(m_echoCancelModuleId)}, &ok);
    Q_UNUSED(ok);
    m_echoCancelModuleId = -1;
    m_echoCancelModuleOwned = false;
}

bool PortableController::startMegaphoneWithInput(const QString& inputUri,
                                                 bool autoStop,
                                                 bool saveToLibraryAfter) {
    if (m_liveMegaphoneActive) {
        setStatusMessage(QStringLiteral("Le micro en direct est déjà actif"));
        return false;
    }

    const QVector<BaseSnapshot> bases = selectedOnlineBases();
    if (bases.isEmpty()) {
        setStatusMessage(QStringLiteral("Sélectionnez au moins une base en ligne"));
        return false;
    }

    QVector<BaseSnapshot> prepared;
    for (const BaseSnapshot& base : bases) {
        QUrl url = controlUrl(base);
        url.setPath(QStringLiteral("/api/v1/megaphone/prepare"));
        const HttpResult result = postJson(url, QJsonObject{});
        if (!result.ok) {
            for (const BaseSnapshot& preparedBase : prepared) {
                QUrl stopUrl = controlUrl(preparedBase);
                stopUrl.setPath(QStringLiteral("/api/v1/megaphone/stop"));
                postJson(stopUrl, QJsonObject{});
            }
            setStatusMessage(QStringLiteral("Impossible de préparer %1").arg(base.name));
            return false;
        }
        prepared.push_back(base);
    }

    const QStringList endpoints{
        QStringLiteral("rtp://%1:%2")
            .arg(m_config.multicastAddress, QString::number(m_config.multicastPort)),
    };

    m_megaphoneAutoStop = autoStop;
    m_saveRecordingAfterMegaphone = saveToLibraryAfter;
    m_explicitStopRequested = false;
    m_preparedBases = prepared;

    const QString program = AppPaths::rocSendBinary();
    const QString effectiveInputUri = effectiveLiveInputUri(inputUri);
    if (effectiveInputUri.isEmpty()) {
        finalizeMegaphoneSession();
        setStatusMessage(QStringLiteral("Aucune entrée micro PulseAudio disponible"));
        return false;
    }

    const QStringList arguments = buildRocSendArguments(endpoints, effectiveInputUri);
    const QProcessEnvironment audioEnvironment = audioProcessEnvironment();
    m_megaphoneProcess.setProcessEnvironment(audioEnvironment);
    qInfo().noquote()
        << QStringLiteral("[roc_send] starting %1 %2 | input=%3 PULSE_SERVER=%4 PULSE_SOURCE=%5 XDG_RUNTIME_DIR=%6")
               .arg(program,
                    arguments.join(QLatin1Char(' ')),
                    effectiveInputUri,
                    audioEnvironment.value(QStringLiteral("PULSE_SERVER")),
                    audioEnvironment.value(QStringLiteral("PULSE_SOURCE")),
                    audioEnvironment.value(QStringLiteral("XDG_RUNTIME_DIR")));
    m_megaphoneProcess.start(program, arguments);
    if (!m_megaphoneProcess.waitForStarted(3000)) {
        finalizeMegaphoneSession();
        setStatusMessage(QStringLiteral("Impossible de lancer roc_send"));
        return false;
    }
    if (!autoStop && m_megaphoneProcess.waitForFinished(500)) {
        const QString output = QString::fromUtf8(m_megaphoneProcess.readAllStandardOutput()).trimmed();
        finalizeMegaphoneSession();
        setStatusMessage(output.isEmpty()
                             ? QStringLiteral("roc_send s'est arrêté au démarrage")
                             : QStringLiteral("roc_send arrêté : %1").arg(output.left(180)));
        return false;
    }

    for (const BaseSnapshot& base : prepared) {
        QUrl url = controlUrl(base);
        url.setPath(QStringLiteral("/api/v1/megaphone/start"));
        postJson(url, QJsonObject{});
    }

    m_liveMegaphoneActive = true;
    emit liveMegaphoneActiveChanged();
    setStatusMessage(autoStop
                         ? QStringLiteral("Diffusion multicast du message enregistré")
                         : QStringLiteral("Micro multicast actif"));
    return true;
}

void PortableController::finalizeMegaphoneSession() {
    if (m_preparedBases.isEmpty() && !m_liveMegaphoneActive) {
        return;
    }

    for (const BaseSnapshot& base : std::as_const(m_preparedBases)) {
        QUrl url = controlUrl(base);
        url.setPath(QStringLiteral("/api/v1/megaphone/stop"));
        postJson(url, QJsonObject{});
    }

    const QVector<BaseSnapshot> usedBases = m_preparedBases;
    const bool shouldUploadAfter =
        m_megaphoneAutoStop && m_saveRecordingAfterMegaphone && hasPendingRecording()
        && !m_explicitStopRequested;

    m_preparedBases.clear();
    const bool wasLive = m_liveMegaphoneActive;
    m_liveMegaphoneActive = false;
    m_megaphoneAutoStop = false;
    m_saveRecordingAfterMegaphone = false;
    m_explicitStopRequested = false;

    if (wasLive) {
        emit liveMegaphoneActiveChanged();
    }

    if (!m_antiFeedbackEnabled) {
        unloadEchoCancelSource();
    }

    if (shouldUploadAfter) {
        uploadPendingRecordingToBases(usedBases);
        refreshLibraries();
    }

    setStatusMessage(QStringLiteral("Session micro terminée"));
}

bool PortableController::uploadPendingRecordingToBases(const QVector<BaseSnapshot>& bases) {
    QString error;
    const QString uploadPath = selectedPendingRecordingPath(&error);
    if (uploadPath.isEmpty()) {
        setStatusMessage(QStringLiteral("Impossible de préparer l'enregistrement : %1").arg(error));
        return false;
    }

    QFile file(uploadPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatusMessage(QStringLiteral("Impossible de lire l'enregistrement pour l'envoi"));
        return false;
    }
    const QByteArray payload = file.readAll();
    file.close();

    int successCount = 0;
    for (const BaseSnapshot& base : bases) {
        QUrl url = controlUrl(base);
        url.setPath(QStringLiteral("/api/v1/upload"));
        const QJsonObject body{
            {QStringLiteral("label"), uploadRecordingLabel()},
            {QStringLiteral("file_name"), uploadRecordingFileName()},
            {QStringLiteral("source"), QStringLiteral("portable-console")},
            {QStringLiteral("content_base64"), QString::fromLatin1(payload.toBase64())},
        };
        if (postJson(url, body).ok) {
            ++successCount;
        }
    }

    setStatusMessage(QStringLiteral("Enregistrement sauvegardé sur %1/%2 base(s)")
                         .arg(successCount)
                         .arg(bases.size()));
    return successCount > 0;
}

std::optional<QPair<AudioAssetMetadata, QByteArray>> PortableController::downloadAsset(const BaseSnapshot& base,
                                                                                       const QString& assetId,
                                                                                       QString* error) {
    QUrl url = controlUrl(base);
    url.setPath(QStringLiteral("/api/v1/assets/%1").arg(assetId));
    const HttpResult result = getJson(url);
    if (!result.ok || !result.json.isObject()) {
        if (error) {
            *error = result.error.isEmpty() ? QStringLiteral("Téléchargement impossible") : result.error;
        }
        return std::nullopt;
    }

    const auto asset = audioAssetFromJson(result.json.object().value(QStringLiteral("asset")).toObject());
    const QByteArray bytes = QByteArray::fromBase64(
        result.json.object().value(QStringLiteral("content_base64")).toString().toLatin1());
    if (!asset || bytes.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Audio distant invalide");
        }
        return std::nullopt;
    }
    return QPair<AudioAssetMetadata, QByteArray>{*asset, bytes};
}

bool PortableController::uploadAssetBytesToBase(const BaseSnapshot& base,
                                                const AudioAssetMetadata& asset,
                                                const QByteArray& bytes,
                                                QString* error) {
    QUrl url = controlUrl(base);
    url.setPath(QStringLiteral("/api/v1/upload"));
    const QJsonObject body{
        {QStringLiteral("label"), asset.label},
        {QStringLiteral("file_name"), asset.fileName},
        {QStringLiteral("source"), QStringLiteral("sync:%1").arg(asset.source)},
        {QStringLiteral("content_base64"), QString::fromLatin1(bytes.toBase64())},
    };
    const HttpResult result = postJson(url, body);
    if (!result.ok) {
        if (error) {
            *error = result.error.isEmpty() ? QStringLiteral("Envoi impossible") : result.error;
        }
        return false;
    }
    return true;
}

void PortableController::clearSelectedRecordingCache() {
    if (m_selectedRecordingCachePath.isEmpty()) {
        return;
    }
    QFile::remove(m_selectedRecordingCachePath);
    m_selectedRecordingCachePath.clear();
}

bool PortableController::selectionCoversFullRecording() const {
    return m_pendingRecordingDurationMs <= 0
        || (m_pendingRecordingSelectionStartMs <= 0
            && m_pendingRecordingSelectionEndMs >= m_pendingRecordingDurationMs);
}

void PortableController::resetPendingRecordingAnalysis() {
    m_pendingRecordingDurationMs = 0;
    m_pendingRecordingPeaks.clear();
    m_pendingRecordingSelectionStartMs = 0;
    m_pendingRecordingSelectionEndMs = 0;
}

void PortableController::loadPendingRecordingAnalysis(bool resetSelection) {
    const auto info = probeWavFile(m_pendingRecordingPath);
    if (!info || !info->isValid()) {
        resetPendingRecordingAnalysis();
        return;
    }

    m_pendingRecordingDurationMs = info->durationMs;
    m_pendingRecordingPeaks = readWavPeakEnvelope(m_pendingRecordingPath, 360);
    if (resetSelection || m_pendingRecordingSelectionEndMs <= m_pendingRecordingSelectionStartMs
        || m_pendingRecordingSelectionEndMs > m_pendingRecordingDurationMs) {
        m_pendingRecordingSelectionStartMs = 0;
        m_pendingRecordingSelectionEndMs = m_pendingRecordingDurationMs;
    }
}

QString PortableController::selectedPendingRecordingPath(QString* error) {
    if (!hasPendingRecording()) {
        if (error) {
            *error = QStringLiteral("aucun enregistrement disponible");
        }
        return {};
    }
    if (selectionCoversFullRecording()) {
        return m_pendingRecordingPath;
    }

    clearSelectedRecordingCache();
    const QString cachePath =
        QDir(m_config.recordingRoot)
            .filePath(QStringLiteral("%1-selection.wav")
                          .arg(QFileInfo(m_pendingRecordingPath).completeBaseName()));
    if (!cropWavFile(m_pendingRecordingPath,
                     cachePath,
                     m_pendingRecordingSelectionStartMs,
                     m_pendingRecordingSelectionEndMs,
                     error)) {
        QFile::remove(cachePath);
        return {};
    }
    m_selectedRecordingCachePath = cachePath;
    return m_selectedRecordingCachePath;
}

void PortableController::updatePreviewProgress() {
    const qint64 duration = m_previewPlayer.duration();
    const int nextProgress = duration > 0
        ? std::clamp(static_cast<int>((100 * m_previewPlayer.position()) / duration), 0, 100)
        : 0;
    if (m_previewProgress != nextProgress) {
        m_previewProgress = nextProgress;
    }
    emit previewProgressChanged();
}

} // namespace apping
