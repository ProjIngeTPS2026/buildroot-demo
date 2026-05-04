#include "portable_console/portable_config.h"

#include "common/app_paths.h"
#include "common/json_protocol.h"

namespace apping {

std::optional<PortableConfig> PortableConfig::loadFromFile(const QString& path, QString* error) {
    const QString resolvedPath = AppPaths::resolvePath(path);
    const QJsonDocument document = jsonDocumentFromFile(resolvedPath, error);
    if (!document.isObject()) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("Portable configuration JSON root must be an object");
        }
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    PortableConfig config;
    config.configPath = resolvedPath;
    config.discoveryPort =
        static_cast<quint16>(object.value(QStringLiteral("discovery_port")).toInt(17100));
    config.mapMetadataPath =
        AppPaths::resolvePath(object.value(QStringLiteral("map_metadata_path"))
                                  .toString(config.mapMetadataPath));
    config.recordingRoot =
        AppPaths::resolvePath(object.value(QStringLiteral("recording_root"))
                                  .toString(config.recordingRoot));
    config.captureInputUri =
        object.value(QStringLiteral("capture_input_uri")).toString();
    config.multicastAddress =
        object.value(QStringLiteral("multicast_address")).toString(QStringLiteral("239.42.0.10"));
    config.multicastPort =
        static_cast<quint16>(object.value(QStringLiteral("multicast_port")).toInt(19100));
    config.requestTimeoutMs =
        object.value(QStringLiteral("request_timeout_ms")).toInt(2500);
    return config;
}

} // namespace apping
