#pragma once

#include "common/models.h"

#include <optional>

#include <QJsonDocument>
#include <QJsonObject>

namespace apping {

QJsonObject toJson(const AudioAssetMetadata& asset);
QJsonObject toJson(const BaseSnapshot& snapshot);
QJsonObject toJson(const PlaybackRequest& request);
QJsonObject toJson(const MapMetadata& mapMetadata);

std::optional<AudioAssetMetadata> audioAssetFromJson(const QJsonObject& object);
std::optional<BaseSnapshot> baseSnapshotFromJson(const QJsonObject& object);
std::optional<PlaybackRequest> playbackRequestFromJson(const QJsonObject& object);
std::optional<UploadRequest> uploadRequestFromJson(const QJsonObject& object);
std::optional<MapMetadata> mapMetadataFromJson(const QJsonObject& object);

QJsonDocument jsonDocumentFromFile(const QString& path, QString* error = nullptr);
bool writeJsonDocumentToFile(const QString& path,
                             const QJsonDocument& document,
                             QString* error = nullptr);

} // namespace apping
