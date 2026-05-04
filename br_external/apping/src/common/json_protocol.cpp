#include "common/json_protocol.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

namespace apping {

namespace {

QStringList stringListFromJson(const QJsonValue& value) {
    QStringList result;
    const auto array = value.toArray();
    result.reserve(array.size());
    for (const QJsonValue& entry : array) {
        result.push_back(entry.toString());
    }
    return result;
}

QJsonArray jsonArrayFromStringList(const QStringList& list) {
    QJsonArray result;
    for (const QString& item : list) {
        result.push_back(item);
    }
    return result;
}

} // namespace

QJsonObject toJson(const AudioAssetMetadata& asset) {
    return QJsonObject{
        {QStringLiteral("asset_id"), asset.assetId},
        {QStringLiteral("label"), asset.label},
        {QStringLiteral("file_name"), asset.fileName},
        {QStringLiteral("duration_ms"), asset.durationMs},
        {QStringLiteral("created_at"), asset.createdAt.toString(Qt::ISODate)},
        {QStringLiteral("source"), asset.source},
        {QStringLiteral("available_on"), jsonArrayFromStringList(asset.availableOn)},
    };
}

QJsonObject toJson(const BaseSnapshot& snapshot) {
    return QJsonObject{
        {QStringLiteral("base_id"), snapshot.baseId},
        {QStringLiteral("name"), snapshot.name},
        {QStringLiteral("description"), snapshot.description},
        {QStringLiteral("lat"), snapshot.latitude},
        {QStringLiteral("lon"), snapshot.longitude},
        {QStringLiteral("control_host"), snapshot.host},
        {QStringLiteral("control_port"), snapshot.controlPort},
        {QStringLiteral("discovery_port"), snapshot.discoveryPort},
        {QStringLiteral("megaphone_port"), snapshot.megaphonePort},
        {QStringLiteral("audio_output_uri"), snapshot.audioOutputUri},
        {QStringLiteral("audio_state"), snapshot.audioState},
        {QStringLiteral("status"), snapshot.status},
        {QStringLiteral("library_revision"), snapshot.libraryRevision},
        {QStringLiteral("capabilities"), jsonArrayFromStringList(snapshot.capabilities)},
        {QStringLiteral("last_seen"), snapshot.lastSeen.toString(Qt::ISODate)},
    };
}

QJsonObject toJson(const PlaybackRequest& request) {
    return QJsonObject{
        {QStringLiteral("asset_id"), request.assetId},
        {QStringLiteral("mode"), request.mode},
        {QStringLiteral("interval_ms"), request.intervalMs},
        {QStringLiteral("duration_ms"), request.durationMs},
    };
}

QJsonObject toJson(const MapMetadata& mapMetadata) {
    return QJsonObject{
        {QStringLiteral("title"), mapMetadata.title},
        {QStringLiteral("image_relative_path"), mapMetadata.imageRelativePath},
        {QStringLiteral("offline_tiles_relative_path"), mapMetadata.offlineTilesRelativePath},
        {QStringLiteral("min_lat"), mapMetadata.minLat},
        {QStringLiteral("max_lat"), mapMetadata.maxLat},
        {QStringLiteral("min_lon"), mapMetadata.minLon},
        {QStringLiteral("max_lon"), mapMetadata.maxLon},
        {QStringLiteral("center_lat"), mapMetadata.centerLat},
        {QStringLiteral("center_lon"), mapMetadata.centerLon},
        {QStringLiteral("default_zoom"), mapMetadata.defaultZoom},
        {QStringLiteral("min_zoom"), mapMetadata.minZoom},
        {QStringLiteral("max_zoom"), mapMetadata.maxZoom},
    };
}

std::optional<AudioAssetMetadata> audioAssetFromJson(const QJsonObject& object) {
    AudioAssetMetadata asset;
    asset.assetId = object.value(QStringLiteral("asset_id")).toString();
    asset.label = object.value(QStringLiteral("label")).toString();
    asset.fileName = object.value(QStringLiteral("file_name")).toString();
    asset.durationMs =
        static_cast<qint64>(object.value(QStringLiteral("duration_ms")).toDouble());
    asset.createdAt =
        QDateTime::fromString(object.value(QStringLiteral("created_at")).toString(),
                              Qt::ISODate);
    asset.source = object.value(QStringLiteral("source")).toString();
    asset.availableOn = stringListFromJson(object.value(QStringLiteral("available_on")));
    if (asset.assetId.isEmpty() || asset.fileName.isEmpty()) {
        return std::nullopt;
    }
    return asset;
}

std::optional<BaseSnapshot> baseSnapshotFromJson(const QJsonObject& object) {
    BaseSnapshot snapshot;
    snapshot.baseId = object.value(QStringLiteral("base_id")).toString();
    snapshot.name = object.value(QStringLiteral("name")).toString();
    snapshot.description = object.value(QStringLiteral("description")).toString();
    snapshot.latitude = object.value(QStringLiteral("lat")).toDouble();
    snapshot.longitude = object.value(QStringLiteral("lon")).toDouble();
    snapshot.host = object.value(QStringLiteral("control_host")).toString();
    snapshot.controlPort =
        static_cast<quint16>(object.value(QStringLiteral("control_port")).toInt());
    snapshot.discoveryPort =
        static_cast<quint16>(object.value(QStringLiteral("discovery_port")).toInt());
    snapshot.megaphonePort =
        static_cast<quint16>(object.value(QStringLiteral("megaphone_port")).toInt());
    snapshot.audioOutputUri = object.value(QStringLiteral("audio_output_uri")).toString();
    snapshot.audioState = object.value(QStringLiteral("audio_state")).toString();
    snapshot.status = object.value(QStringLiteral("status")).toString();
    snapshot.libraryRevision =
        static_cast<qint64>(object.value(QStringLiteral("library_revision")).toDouble());
    snapshot.capabilities =
        stringListFromJson(object.value(QStringLiteral("capabilities")));
    snapshot.lastSeen =
        QDateTime::fromString(object.value(QStringLiteral("last_seen")).toString(),
                              Qt::ISODate);

    if (snapshot.baseId.isEmpty() || snapshot.name.isEmpty() || snapshot.host.isEmpty()
        || snapshot.controlPort == 0) {
        return std::nullopt;
    }
    return snapshot;
}

std::optional<PlaybackRequest> playbackRequestFromJson(const QJsonObject& object) {
    PlaybackRequest request;
    request.assetId = object.value(QStringLiteral("asset_id")).toString();
    request.mode = object.value(QStringLiteral("mode")).toString(QStringLiteral("once"));
    request.intervalMs = object.value(QStringLiteral("interval_ms")).toInt(0);
    request.durationMs = object.value(QStringLiteral("duration_ms")).toInt(-1);
    if (request.assetId.isEmpty()) {
        return std::nullopt;
    }
    return request;
}

std::optional<UploadRequest> uploadRequestFromJson(const QJsonObject& object) {
    UploadRequest upload;
    upload.label = object.value(QStringLiteral("label")).toString();
    upload.fileName = object.value(QStringLiteral("file_name")).toString();
    upload.source = object.value(QStringLiteral("source")).toString();
    upload.audioBytes = QByteArray::fromBase64(
        object.value(QStringLiteral("content_base64")).toString().toLatin1());
    upload.playAfterUpload =
        object.value(QStringLiteral("play_after_upload")).toBool(false);
    if (const auto playback = playbackRequestFromJson(object.value(QStringLiteral("playback"))
                                                          .toObject())) {
        upload.playback = *playback;
    }
    if (upload.label.isEmpty() || upload.fileName.isEmpty() || upload.audioBytes.isEmpty()) {
        return std::nullopt;
    }
    return upload;
}

std::optional<MapMetadata> mapMetadataFromJson(const QJsonObject& object) {
    MapMetadata mapMetadata;
    mapMetadata.title = object.value(QStringLiteral("title")).toString();
    mapMetadata.imageRelativePath =
        object.value(QStringLiteral("image_relative_path")).toString();
    mapMetadata.offlineTilesRelativePath =
        object.value(QStringLiteral("offline_tiles_relative_path")).toString();
    mapMetadata.minLat = object.value(QStringLiteral("min_lat")).toDouble();
    mapMetadata.maxLat = object.value(QStringLiteral("max_lat")).toDouble();
    mapMetadata.minLon = object.value(QStringLiteral("min_lon")).toDouble();
    mapMetadata.maxLon = object.value(QStringLiteral("max_lon")).toDouble();
    mapMetadata.centerLat = object.value(QStringLiteral("center_lat"))
                                .toDouble((mapMetadata.minLat + mapMetadata.maxLat) / 2.0);
    mapMetadata.centerLon = object.value(QStringLiteral("center_lon"))
                                .toDouble((mapMetadata.minLon + mapMetadata.maxLon) / 2.0);
    mapMetadata.defaultZoom = object.value(QStringLiteral("default_zoom")).toDouble(14.0);
    mapMetadata.minZoom = object.value(QStringLiteral("min_zoom")).toDouble(12.0);
    mapMetadata.maxZoom = object.value(QStringLiteral("max_zoom")).toDouble(16.0);
    if (mapMetadata.title.isEmpty()) {
        return std::nullopt;
    }
    return mapMetadata;
}

QJsonDocument jsonDocumentFromFile(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = file.errorString();
        }
        return QJsonDocument();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError && error) {
        *error = parseError.errorString();
    }
    return document;
}

bool writeJsonDocumentToFile(const QString& path,
                             const QJsonDocument& document,
                             QString* error) {
    QFile file(path);
    QDir().mkpath(QFileInfo(file).absolutePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    if (file.write(document.toJson(QJsonDocument::Indented)) < 0) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }
    return true;
}

} // namespace apping
