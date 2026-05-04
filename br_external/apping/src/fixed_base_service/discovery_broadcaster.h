#pragma once

#include "fixed_base_service/base_config.h"

#include <functional>

#include <QObject>
#include <QUdpSocket>
#include <QTimer>

namespace apping {

struct BaseSnapshot;

class DiscoveryBroadcaster : public QObject {
    Q_OBJECT

public:
    DiscoveryBroadcaster(const BaseConfig& config,
                         std::function<BaseSnapshot()> snapshotProvider,
                         QObject* parent = nullptr);

    void start();
    void stop();

private:
    BaseConfig m_config;
    std::function<BaseSnapshot()> m_snapshotProvider;
    QUdpSocket m_socket;
    QTimer m_timer;

    void sendAnnouncement();
};

} // namespace apping
