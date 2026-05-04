#include "fixed_base_service/base_service.h"

#include "common/json_protocol.h"
#include "common/network_utils.h"

#include <QFile>
#include <QJsonArray>

namespace apping {

BaseService::BaseService(const BaseConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_libraryManager(config, this)
    , m_playbackController(config.audioOutputUri, &m_libraryManager, this)
    , m_liveSessionController(config, this)
    , m_httpServer(this) {
}

bool BaseService::start(QString* error) {
    if (!m_libraryManager.initialize(error)) {
        return false;
    }

    m_hostAddress = firstUsableIpv4Address();
    if (m_hostAddress.isEmpty()) {
        m_hostAddress = QStringLiteral("127.0.0.1");
    }

    m_httpServer.setHandler([this](const HttpRequest& request) { return handleRequest(request); });
    if (!m_httpServer.listen(m_config.controlPort)) {
        if (error) {
            *error = QStringLiteral("Unable to listen on control port %1").arg(m_config.controlPort);
        }
        return false;
    }

    m_discoveryBroadcaster = std::make_unique<DiscoveryBroadcaster>(
        m_config, [this]() { return currentSnapshot(); }, this);
    m_discoveryBroadcaster->start();
    return true;
}

BaseSnapshot BaseService::currentSnapshot() const {
    BaseSnapshot snapshot;
    snapshot.baseId = m_config.baseId;
    snapshot.name = m_config.name;
    snapshot.description = m_config.description;
    snapshot.latitude = m_config.latitude;
    snapshot.longitude = m_config.longitude;
    snapshot.host = m_hostAddress;
    snapshot.controlPort = m_config.controlPort;
    snapshot.discoveryPort = m_config.discoveryPort;
    snapshot.megaphonePort = m_config.megaphonePort;
    snapshot.audioOutputUri = m_config.audioOutputUri;

    const QString liveState = m_liveSessionController.audioState();
    const QString playbackState = m_playbackController.audioState();
    snapshot.audioState =
        liveState != QStringLiteral("idle") ? liveState : playbackState;
    snapshot.status =
        snapshot.audioState == QStringLiteral("idle") ? QStringLiteral("online")
                                                      : QStringLiteral("busy");
    snapshot.libraryRevision = m_libraryManager.revision();
    snapshot.capabilities = QStringList{
        QStringLiteral("prerecorded-playback"),
        QStringLiteral("upload"),
        QStringLiteral("megaphone-live"),
        QStringLiteral("megaphone-recorded"),
    };
    snapshot.lastSeen = QDateTime::currentDateTimeUtc();
    return snapshot;
}

HttpResponse BaseService::handleRequest(const HttpRequest& request) {
    if (request.method == QStringLiteral("GET") && request.path() == QStringLiteral("/api/v1/status")) {
        return handleStatusRequest();
    }
    if (request.method == QStringLiteral("GET") && request.path() == QStringLiteral("/api/v1/library")) {
        return handleLibraryRequest();
    }
    if (request.method == QStringLiteral("GET") && request.path().startsWith(QStringLiteral("/api/v1/assets/"))) {
        return handleAssetDownloadRequest(request.path().mid(QStringLiteral("/api/v1/assets/").size()));
    }
    if (request.method == QStringLiteral("POST") && request.path() == QStringLiteral("/api/v1/library/delete")) {
        return handleLibraryDeleteRequest(request);
    }
    if (request.method == QStringLiteral("POST") && request.path() == QStringLiteral("/api/v1/config")) {
        return handleConfigRequest(request);
    }
    if (request.method == QStringLiteral("POST") && request.path() == QStringLiteral("/api/v1/play")) {
        return handlePlayRequest(request);
    }
    if (request.method == QStringLiteral("POST") && request.path() == QStringLiteral("/api/v1/stop")) {
        return handleStopRequest();
    }
    if (request.method == QStringLiteral("POST") && request.path() == QStringLiteral("/api/v1/upload")) {
        return handleUploadRequest(request);
    }
    if (request.method == QStringLiteral("POST")
        && request.path() == QStringLiteral("/api/v1/megaphone/prepare")) {
        return handleMegaphonePrepare();
    }
    if (request.method == QStringLiteral("POST")
        && request.path() == QStringLiteral("/api/v1/megaphone/start")) {
        return handleMegaphoneStart();
    }
    if (request.method == QStringLiteral("POST")
        && request.path() == QStringLiteral("/api/v1/megaphone/stop")) {
        return handleMegaphoneStop();
    }

    return HttpResponse::json(QJsonObject{
                                  {QStringLiteral("error"), QStringLiteral("Route not found")},
                              },
                              404);
}

HttpResponse BaseService::handleStatusRequest() const {
    return HttpResponse::json(QJsonObject{
        {QStringLiteral("base"), toJson(currentSnapshot())},
    });
}

HttpResponse BaseService::handleLibraryRequest() const {
    QJsonArray assets;
    for (const AudioAssetMetadata& asset : m_libraryManager.assets()) {
        assets.push_back(toJson(asset));
    }
    return HttpResponse::json(QJsonObject{
        {QStringLiteral("assets"), assets},
        {QStringLiteral("revision"), m_libraryManager.revision()},
    });
}

HttpResponse BaseService::handleAssetDownloadRequest(const QString& assetId) const {
    const auto asset = m_libraryManager.assetById(assetId);
    if (!asset) {
        return HttpResponse::json(QJsonObject{{QStringLiteral("error"), QStringLiteral("Audio introuvable")}}, 404);
    }

    QFile file(m_libraryManager.assetPath(assetId));
    if (!file.open(QIODevice::ReadOnly)) {
        return HttpResponse::json(QJsonObject{{QStringLiteral("error"), file.errorString()}}, 500);
    }

    return HttpResponse::json(QJsonObject{
        {QStringLiteral("asset"), toJson(*asset)},
        {QStringLiteral("content_base64"), QString::fromLatin1(file.readAll().toBase64())},
    });
}

HttpResponse BaseService::handleLibraryDeleteRequest(const HttpRequest& request) {
    bool ok = false;
    const QJsonObject body = request.jsonBody(&ok);
    const QString assetId = body.value(QStringLiteral("asset_id")).toString();
    if (!ok || assetId.isEmpty()) {
        return HttpResponse::json(QJsonObject{{QStringLiteral("error"), QStringLiteral("Payload invalide")}}, 400);
    }

    QString error;
    if (!m_libraryManager.deleteAsset(assetId, &error)) {
        return HttpResponse::json(QJsonObject{{QStringLiteral("error"), error}}, 404);
    }
    return HttpResponse::json(QJsonObject{{QStringLiteral("status"), QStringLiteral("deleted")}});
}

HttpResponse BaseService::handleConfigRequest(const HttpRequest& request) {
    bool ok = false;
    const QJsonObject body = request.jsonBody(&ok);
    if (!ok) {
        return HttpResponse::json(QJsonObject{{QStringLiteral("error"), QStringLiteral("Payload invalide")}}, 400);
    }

    const QString name = body.value(QStringLiteral("name")).toString().trimmed();
    const QString description = body.value(QStringLiteral("description")).toString().trimmed();
    if (!name.isEmpty()) {
        m_config.name = name;
    }
    m_config.description = description;
    if (body.contains(QStringLiteral("lat"))) {
        m_config.latitude = body.value(QStringLiteral("lat")).toDouble(m_config.latitude);
    }
    if (body.contains(QStringLiteral("lon"))) {
        m_config.longitude = body.value(QStringLiteral("lon")).toDouble(m_config.longitude);
    }

    return HttpResponse::json(QJsonObject{
        {QStringLiteral("status"), QStringLiteral("updated")},
        {QStringLiteral("base"), toJson(currentSnapshot())},
    });
}

HttpResponse BaseService::handlePlayRequest(const HttpRequest& request) {
    if (m_liveSessionController.audioState() != QStringLiteral("idle")) {
        return HttpResponse::json(QJsonObject{
                                      {QStringLiteral("error"),
                                       QStringLiteral("Megaphone session has priority")},
                                  },
                                  409);
    }

    bool ok = false;
    const auto playback = playbackRequestFromJson(request.jsonBody(&ok));
    if (!ok || !playback) {
        return HttpResponse::json(QJsonObject{
                                      {QStringLiteral("error"), QStringLiteral("Invalid playback payload")},
                                  },
                                  400);
    }

    QString error;
    if (!m_playbackController.play(*playback, &error)) {
        return HttpResponse::json(QJsonObject{
                                      {QStringLiteral("error"), error},
                                  },
                                  404);
    }

    return HttpResponse::json(QJsonObject{
        {QStringLiteral("status"), QStringLiteral("playing")},
        {QStringLiteral("base"), toJson(currentSnapshot())},
    });
}

HttpResponse BaseService::handleStopRequest() {
    m_playbackController.stop();
    m_liveSessionController.stop();
    return HttpResponse::json(QJsonObject{
        {QStringLiteral("status"), QStringLiteral("stopped")},
        {QStringLiteral("base"), toJson(currentSnapshot())},
    });
}

HttpResponse BaseService::handleUploadRequest(const HttpRequest& request) {
    bool ok = false;
    const auto upload = uploadRequestFromJson(request.jsonBody(&ok));
    if (!ok || !upload) {
        return HttpResponse::json(QJsonObject{
                                      {QStringLiteral("error"), QStringLiteral("Invalid upload payload")},
                                  },
                                  400);
    }

    QString error;
    const auto asset = m_libraryManager.storeUpload(*upload, &error);
    if (!asset) {
        return HttpResponse::json(QJsonObject{
                                      {QStringLiteral("error"), error},
                                  },
                                  400);
    }

    if (upload->playAfterUpload) {
        PlaybackRequest playback = upload->playback;
        playback.assetId = asset->assetId;
        if (playback.assetId.isEmpty()) {
            playback.assetId = asset->assetId;
        }
        m_playbackController.play(playback);
    }

    return HttpResponse::json(QJsonObject{
                                  {QStringLiteral("asset"), toJson(*asset)},
                              },
                              201);
}

HttpResponse BaseService::handleMegaphonePrepare() {
    m_playbackController.interruptForLive();
    QString error;
    if (!m_liveSessionController.prepare(&error)) {
        return HttpResponse::json(QJsonObject{
                                      {QStringLiteral("error"), error},
                                  },
                                  500);
    }
    return HttpResponse::json(QJsonObject{
        {QStringLiteral("status"), QStringLiteral("prepared")},
        {QStringLiteral("base"), toJson(currentSnapshot())},
    });
}

HttpResponse BaseService::handleMegaphoneStart() {
    QString error;
    if (!m_liveSessionController.markStarted(&error)) {
        return HttpResponse::json(QJsonObject{
                                      {QStringLiteral("error"), error},
                                  },
                                  500);
    }
    return HttpResponse::json(QJsonObject{
        {QStringLiteral("status"), QStringLiteral("live")},
        {QStringLiteral("base"), toJson(currentSnapshot())},
    });
}

HttpResponse BaseService::handleMegaphoneStop() {
    m_liveSessionController.stop();
    return HttpResponse::json(QJsonObject{
        {QStringLiteral("status"), QStringLiteral("idle")},
        {QStringLiteral("base"), toJson(currentSnapshot())},
    });
}

} // namespace apping
