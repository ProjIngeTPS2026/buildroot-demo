#include "fixed_base_service/discovery_broadcaster.h"

#include "common/json_protocol.h"
#include "common/models.h"

#include <QHostAddress>
#include <QJsonDocument>

namespace apping {

DiscoveryBroadcaster::DiscoveryBroadcaster(const BaseConfig& config,
                                           std::function<BaseSnapshot()> snapshotProvider,
                                           QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_snapshotProvider(std::move(snapshotProvider)) {
    connect(&m_timer, &QTimer::timeout, this, &DiscoveryBroadcaster::sendAnnouncement);
}

void DiscoveryBroadcaster::start() {
    m_timer.start(m_config.discoveryIntervalMs);
    sendAnnouncement();
}

void DiscoveryBroadcaster::stop() {
    m_timer.stop();
}

void DiscoveryBroadcaster::sendAnnouncement() {
    const BaseSnapshot snapshot = m_snapshotProvider();
    QJsonObject payload = toJson(snapshot);
    payload.insert(QStringLiteral("schema_version"), 1);
    payload.insert(QStringLiteral("kind"), QStringLiteral("apping_base_announce"));

    m_socket.writeDatagram(QJsonDocument(payload).toJson(QJsonDocument::Compact),
                           QHostAddress::Broadcast,
                           m_config.discoveryPort);
}

} // namespace apping
