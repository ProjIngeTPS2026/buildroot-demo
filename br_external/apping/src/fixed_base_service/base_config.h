#pragma once

#include <optional>

#include <QString>

namespace apping {

struct BaseConfig {
    QString configPath;
    QString baseId;
    QString name;
    QString description;
    double latitude = 0.0;
    double longitude = 0.0;
    quint16 controlPort = 0;
    quint16 discoveryPort = 0;
    quint16 megaphonePort = 0;
    QString multicastAddress = QStringLiteral("239.42.0.10");
    quint16 multicastPort = 19100;
    QString multicastInterface;
    int discoveryIntervalMs = 2000;
    QString mediaRoot;
    QString audioOutputUri = QStringLiteral("pulse://default");

    QString libraryRoot() const;
    QString libraryIndexPath() const;

    static std::optional<BaseConfig> loadFromFile(const QString& path, QString* error = nullptr);
};

} // namespace apping
