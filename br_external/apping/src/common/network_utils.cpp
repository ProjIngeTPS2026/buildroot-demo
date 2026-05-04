#include "common/network_utils.h"

#include <QHostAddress>
#include <QNetworkInterface>

namespace apping {

QString firstUsableIpv4Address(bool allowLoopbackFallback) {
    QString loopbackFallback;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& interface : interfaces) {
        if (!(interface.flags() & QNetworkInterface::IsUp)
            || !(interface.flags() & QNetworkInterface::IsRunning)) {
            continue;
        }
        for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
            const QHostAddress address = entry.ip();
            if (address.protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }
            if (address.isLoopback()) {
                if (loopbackFallback.isEmpty()) {
                    loopbackFallback = address.toString();
                }
                continue;
            }
            return address.toString();
        }
    }
    return allowLoopbackFallback ? loopbackFallback : QString();
}

} // namespace apping
