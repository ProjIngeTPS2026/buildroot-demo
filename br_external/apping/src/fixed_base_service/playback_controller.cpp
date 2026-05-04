#include "fixed_base_service/playback_controller.h"

#include "fixed_base_service/library_manager.h"

#include <QDebug>
#include <QUrl>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioDevice>
#include <QMediaDevices>
#endif

namespace apping {

namespace {

QString audioDeviceIdFromUri(const QString& outputUri) {
    const QUrl uri(outputUri);
    if (!uri.isValid() || uri.scheme() != QStringLiteral("pulse")) {
        return QString();
    }
    return uri.host(QUrl::FullyDecoded);
}

} // namespace

PlaybackController::PlaybackController(const QString& audioOutputUri,
                                       LibraryManager* libraryManager,
                                       QObject* parent)
    : QObject(parent)
    , m_audioOutputUri(audioOutputUri)
    , m_libraryManager(libraryManager) {
    configureOutputDevice();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_player.setAudioOutput(&m_audioOutput);
    m_audioOutput.setVolume(1.0);
#else
    m_player.setVolume(100);
#endif
    m_restartTimer.setSingleShot(true);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(&m_player,
            &QMediaPlayer::errorChanged,
            this,
            [this]() {
                if (m_player.error() == QMediaPlayer::NoError) {
                    return;
                }
                qWarning().noquote()
                    << QStringLiteral("Pre-recorded playback error on %1: %2")
                           .arg(m_audioOutputUri.isEmpty() ? QStringLiteral("<default>")
                                                           : m_audioOutputUri,
                                m_player.errorString());
            });
#else
    connect(&m_player,
            QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error),
            this,
            [this](QMediaPlayer::Error error) {
                if (error == QMediaPlayer::NoError) {
                    return;
                }
                qWarning().noquote()
                    << QStringLiteral("Pre-recorded playback error on %1: %2")
                           .arg(m_audioOutputUri.isEmpty() ? QStringLiteral("<default>")
                                                           : m_audioOutputUri,
                                m_player.errorString());
            });
#endif
    connect(&m_restartTimer, &QTimer::timeout, this, &PlaybackController::startCurrentAsset);
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status != QMediaPlayer::EndOfMedia || !m_active) {
            return;
        }
        if (shouldRepeat()) {
            const int interval = qMax(m_request.intervalMs, 0);
            m_restartTimer.start(interval);
            return;
        }
        stop();
    });
}

bool PlaybackController::play(const PlaybackRequest& request, QString* error) {
    const QString resolvedPath = m_libraryManager->assetPath(request.assetId);
    if (resolvedPath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Unknown asset id");
        }
        return false;
    }

    stop();

    m_request = request;
    m_currentAssetPath = resolvedPath;
    m_deadline = request.durationMs > 0
        ? QDateTime::currentDateTimeUtc().addMSecs(request.durationMs)
        : QDateTime();
    m_active = true;
    startCurrentAsset();
    emit stateChanged();
    return true;
}

void PlaybackController::stop() {
    const bool wasActive = m_active;
    m_active = false;
    m_restartTimer.stop();
    m_player.stop();
    m_currentAssetPath.clear();
    if (wasActive) {
        emit stateChanged();
    }
}

void PlaybackController::interruptForLive() {
    stop();
}

QString PlaybackController::audioState() const {
    return m_active ? QStringLiteral("playing_prerecorded") : QStringLiteral("idle");
}

bool PlaybackController::shouldRepeat() const {
    if (!m_active || m_request.mode == QStringLiteral("once")) {
        return false;
    }
    if (!m_deadline.isValid()) {
        return true;
    }
    return QDateTime::currentDateTimeUtc() < m_deadline;
}

void PlaybackController::configureOutputDevice() {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    Q_UNUSED(m_audioOutputUri);
    return;
#else
    const QString deviceId = audioDeviceIdFromUri(m_audioOutputUri);
    if (deviceId.isEmpty() || deviceId == QStringLiteral("default")) {
        return;
    }

    const auto devices = QMediaDevices::audioOutputs();
    for (const QAudioDevice& device : devices) {
        if (QString::fromUtf8(device.id()) != deviceId) {
            continue;
        }
        m_audioOutput.setDevice(device);
        qInfo().noquote()
            << QStringLiteral("Configured pre-recorded playback output: %1 (%2)")
                   .arg(device.description(), deviceId);
        return;
    }

    qWarning().noquote()
        << QStringLiteral("Unable to match pre-recorded playback output %1, using Qt default device")
               .arg(m_audioOutputUri);
#endif
}

void PlaybackController::startCurrentAsset() {
    if (!m_active || m_currentAssetPath.isEmpty()) {
        return;
    }
    if (m_deadline.isValid() && QDateTime::currentDateTimeUtc() >= m_deadline) {
        stop();
        return;
    }
    qInfo().noquote()
        << QStringLiteral("Starting pre-recorded playback from %1 on %2")
               .arg(m_currentAssetPath,
                    m_audioOutputUri.isEmpty() ? QStringLiteral("<default>")
                                               : m_audioOutputUri);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    m_player.setMedia(QUrl::fromLocalFile(m_currentAssetPath));
#else
    m_player.setSource(QUrl::fromLocalFile(m_currentAssetPath));
#endif
    m_player.play();
}

} // namespace apping
