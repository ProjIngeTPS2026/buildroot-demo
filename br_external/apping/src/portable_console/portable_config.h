#pragma once

#include <optional>

#include <QString>

namespace apping {

struct PortableConfig {
    QString configPath;
    quint16 discoveryPort = 17100;
    QString mapMetadataPath = QStringLiteral("assets/map/telecom_physique_strasbourg_sector.json");
    QString recordingRoot = QStringLiteral("config/simulation/runtime/portable");
    QString captureInputUri;
    QString multicastAddress = QStringLiteral("239.42.0.10");
    quint16 multicastPort = 19100;
    int requestTimeoutMs = 2500;

    static std::optional<PortableConfig> loadFromFile(const QString& path, QString* error = nullptr);
};

} // namespace apping
