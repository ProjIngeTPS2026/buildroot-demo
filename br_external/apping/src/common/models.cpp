#include "common/models.h"

namespace apping {

QUrl controlUrl(const BaseSnapshot& snapshot) {
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(snapshot.host);
    url.setPort(static_cast<int>(snapshot.controlPort));
    return url;
}

bool isOnline(const BaseSnapshot& snapshot) {
    return snapshot.status == QStringLiteral("online")
        || snapshot.status == QStringLiteral("busy");
}

} // namespace apping
