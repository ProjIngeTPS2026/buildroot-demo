#pragma once

#include "common/models.h"
#include "common/simple_http_server.h"
#include "fixed_base_service/base_config.h"
#include "fixed_base_service/discovery_broadcaster.h"
#include "fixed_base_service/library_manager.h"
#include "fixed_base_service/live_session_controller.h"
#include "fixed_base_service/playback_controller.h"

#include <memory>

#include <QObject>

namespace apping {

class BaseService : public QObject {
    Q_OBJECT

public:
    explicit BaseService(const BaseConfig& config, QObject* parent = nullptr);

    bool start(QString* error = nullptr);
    BaseSnapshot currentSnapshot() const;

private:
    BaseConfig m_config;
    LibraryManager m_libraryManager;
    PlaybackController m_playbackController;
    LiveSessionController m_liveSessionController;
    std::unique_ptr<DiscoveryBroadcaster> m_discoveryBroadcaster;
    SimpleHttpServer m_httpServer;
    QString m_hostAddress;

    HttpResponse handleRequest(const HttpRequest& request);
    HttpResponse handleStatusRequest() const;
    HttpResponse handleLibraryRequest() const;
    HttpResponse handleAssetDownloadRequest(const QString& assetId) const;
    HttpResponse handleLibraryDeleteRequest(const HttpRequest& request);
    HttpResponse handleConfigRequest(const HttpRequest& request);
    HttpResponse handlePlayRequest(const HttpRequest& request);
    HttpResponse handleStopRequest();
    HttpResponse handleUploadRequest(const HttpRequest& request);
    HttpResponse handleMegaphonePrepare();
    HttpResponse handleMegaphoneStart();
    HttpResponse handleMegaphoneStop();
};

} // namespace apping
