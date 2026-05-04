#pragma once

#include "common/models.h"
#include "fixed_base_service/base_config.h"

#include <optional>

#include <QObject>
#include <QVector>

namespace apping {

class LibraryManager : public QObject {
    Q_OBJECT

public:
    explicit LibraryManager(const BaseConfig& config, QObject* parent = nullptr);

    bool initialize(QString* error = nullptr);
    QVector<AudioAssetMetadata> assets() const;
    std::optional<AudioAssetMetadata> assetById(const QString& assetId) const;
    QString assetPath(const QString& assetId) const;
    qint64 revision() const;

    std::optional<AudioAssetMetadata> storeUpload(const UploadRequest& upload, QString* error = nullptr);
    bool deleteAsset(const QString& assetId, QString* error = nullptr);

signals:
    void libraryChanged();

private:
    BaseConfig m_config;
    QVector<AudioAssetMetadata> m_assets;
    qint64 m_revision = 0;

    bool loadIndex(QString* error = nullptr);
    bool saveIndex(QString* error = nullptr);
};

} // namespace apping
