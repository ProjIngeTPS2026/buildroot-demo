#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace apping {

struct AudioAssetMetadata {
    QString assetId;
    QString label;
    QString fileName;
    qint64 durationMs = 0;
    QDateTime createdAt;
    QString source;
    QStringList availableOn;
};

struct BaseSnapshot {
    QString baseId;
    QString name;
    QString description;
    double latitude = 0.0;
    double longitude = 0.0;
    QString host;
    quint16 controlPort = 0;
    quint16 discoveryPort = 0;
    quint16 megaphonePort = 0;
    QString audioOutputUri;
    QString audioState;
    QString status;
    qint64 libraryRevision = 0;
    QStringList capabilities;
    QDateTime lastSeen;
};

struct PlaybackRequest {
    QString assetId;
    QString mode = QStringLiteral("once");
    int intervalMs = 0;
    int durationMs = -1;
};

struct UploadRequest {
    QString label;
    QString fileName;
    QString source;
    QByteArray audioBytes;
    bool playAfterUpload = false;
    PlaybackRequest playback;
};

struct MapMetadata {
    QString title;
    QString imageRelativePath;
    QString offlineTilesRelativePath;
    double minLat = 0.0;
    double maxLat = 0.0;
    double minLon = 0.0;
    double maxLon = 0.0;
    double centerLat = 0.0;
    double centerLon = 0.0;
    double defaultZoom = 14.0;
    double minZoom = 12.0;
    double maxZoom = 16.0;
};

QUrl controlUrl(const BaseSnapshot& snapshot);
bool isOnline(const BaseSnapshot& snapshot);

} // namespace apping

Q_DECLARE_METATYPE(apping::AudioAssetMetadata)
Q_DECLARE_METATYPE(apping::BaseSnapshot)
