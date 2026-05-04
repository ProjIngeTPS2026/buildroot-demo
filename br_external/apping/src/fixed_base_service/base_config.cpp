#include "fixed_base_service/base_config.h"

#include "common/app_paths.h"
#include "common/json_protocol.h"

#include <QDir>

namespace apping {

QString BaseConfig::libraryRoot() const {
    return QDir(mediaRoot).filePath(QStringLiteral("library"));
}

QString BaseConfig::libraryIndexPath() const {
    return QDir(libraryRoot()).filePath(QStringLiteral("library.json"));
}

std::optional<BaseConfig> BaseConfig::loadFromFile(const QString& path, QString* error) {
    const QString resolvedPath = AppPaths::resolvePath(path);
    const QJsonDocument document = jsonDocumentFromFile(resolvedPath, error);
    if (!document.isObject()) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("Configuration JSON root must be an object");
        }
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    BaseConfig config;
    config.configPath = resolvedPath;
    config.baseId = object.value(QStringLiteral("base_id")).toString();
    config.name = object.value(QStringLiteral("name")).toString();
    config.description = object.value(QStringLiteral("description")).toString();
    config.latitude = object.value(QStringLiteral("lat")).toDouble();
    config.longitude = object.value(QStringLiteral("lon")).toDouble();
    config.controlPort = static_cast<quint16>(object.value(QStringLiteral("control_port")).toInt());
    config.discoveryPort = static_cast<quint16>(object.value(QStringLiteral("discovery_port")).toInt());
    config.megaphonePort =
        static_cast<quint16>(object.value(QStringLiteral("megaphone_port")).toInt());
    config.multicastAddress =
        object.value(QStringLiteral("multicast_address")).toString(QStringLiteral("239.42.0.10"));
    config.multicastPort =
        static_cast<quint16>(object.value(QStringLiteral("multicast_port")).toInt(19100));
    config.multicastInterface =
        object.value(QStringLiteral("multicast_interface")).toString().trimmed();
    config.discoveryIntervalMs =
        object.value(QStringLiteral("discovery_interval_ms")).toInt(2000);
    config.mediaRoot = AppPaths::resolvePath(object.value(QStringLiteral("media_root")).toString());
    config.audioOutputUri =
        object.value(QStringLiteral("audio_output_uri")).toString(QStringLiteral("pulse://default"));

    if (config.baseId.isEmpty() || config.name.isEmpty() || config.controlPort == 0
        || config.discoveryPort == 0 || config.megaphonePort == 0 || config.multicastPort == 0
        || config.mediaRoot.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Missing required fields in base configuration");
        }
        return std::nullopt;
    }

    return config;
}

} // namespace apping
