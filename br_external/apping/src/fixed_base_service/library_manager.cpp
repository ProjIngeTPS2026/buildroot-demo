#include "fixed_base_service/library_manager.h"

#include "common/json_protocol.h"
#include "common/wav_utils.h"

#include <QDir>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace apping {

namespace {

QString stableAssetIdForUpload(const UploadRequest& upload) {
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(upload.fileName.toUtf8());
    hash.addData("\n", 1);
    hash.addData(upload.label.toUtf8());
    hash.addData("\n", 1);
    hash.addData(upload.audioBytes);
    return QString::fromLatin1(hash.result().toHex().left(32));
}

} // namespace

LibraryManager::LibraryManager(const BaseConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config) {
}

bool LibraryManager::initialize(QString* error) {
    QDir().mkpath(m_config.libraryRoot());
    if (QFileInfo::exists(m_config.libraryIndexPath())) {
        return loadIndex(error);
    }

    m_revision = QDateTime::currentMSecsSinceEpoch();
    return saveIndex(error);
}

QVector<AudioAssetMetadata> LibraryManager::assets() const {
    return m_assets;
}

std::optional<AudioAssetMetadata> LibraryManager::assetById(const QString& assetId) const {
    for (const AudioAssetMetadata& asset : m_assets) {
        if (asset.assetId == assetId) {
            return asset;
        }
    }
    return std::nullopt;
}

QString LibraryManager::assetPath(const QString& assetId) const {
    if (const auto asset = assetById(assetId)) {
        return QDir(m_config.libraryRoot()).filePath(asset->fileName);
    }
    return {};
}

qint64 LibraryManager::revision() const {
    return m_revision;
}

std::optional<AudioAssetMetadata> LibraryManager::storeUpload(const UploadRequest& upload,
                                                              QString* error) {
    const QString assetId = stableAssetIdForUpload(upload);
    const QString fileName = QStringLiteral("%1.wav").arg(assetId);
    const QString path = QDir(m_config.libraryRoot()).filePath(fileName);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = file.errorString();
        }
        return std::nullopt;
    }
    if (file.write(upload.audioBytes) != upload.audioBytes.size()) {
        if (error) {
            *error = file.errorString();
        }
        return std::nullopt;
    }
    file.close();

    const auto wavInfo = probeWavFile(path);
    if (!wavInfo || !wavInfo->isValid()) {
        QFile::remove(path);
        if (error) {
            *error = QStringLiteral("Uploaded file is not a valid PCM WAV");
        }
        return std::nullopt;
    }

    AudioAssetMetadata asset;
    asset.assetId = assetId;
    asset.label = upload.label;
    asset.fileName = fileName;
    asset.durationMs = wavInfo->durationMs;
    asset.createdAt = QDateTime::currentDateTimeUtc();
    asset.source = upload.source;

    bool updated = false;
    for (AudioAssetMetadata& existing : m_assets) {
        if (existing.assetId != assetId) {
            continue;
        }
        existing = asset;
        updated = true;
        break;
    }
    if (!updated) {
        m_assets.push_back(asset);
    }
    m_revision = QDateTime::currentMSecsSinceEpoch();
    if (!saveIndex(error)) {
        return std::nullopt;
    }

    emit libraryChanged();
    return asset;
}

bool LibraryManager::deleteAsset(const QString& assetId, QString* error) {
    for (int index = 0; index < m_assets.size(); ++index) {
        if (m_assets.at(index).assetId != assetId) {
            continue;
        }

        const QString path = QDir(m_config.libraryRoot()).filePath(m_assets.at(index).fileName);
        m_assets.removeAt(index);
        QFile::remove(path);
        m_revision = QDateTime::currentMSecsSinceEpoch();
        if (!saveIndex(error)) {
            return false;
        }
        emit libraryChanged();
        return true;
    }

    if (error) {
        *error = QStringLiteral("Audio introuvable");
    }
    return false;
}

bool LibraryManager::loadIndex(QString* error) {
    const QJsonDocument document = jsonDocumentFromFile(m_config.libraryIndexPath(), error);
    if (!document.isObject()) {
        return false;
    }

    const QJsonObject object = document.object();
    m_revision = static_cast<qint64>(object.value(QStringLiteral("revision")).toDouble());
    m_assets.clear();

    const QJsonArray assetsArray = object.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& value : assetsArray) {
        const auto asset = audioAssetFromJson(value.toObject());
        if (asset) {
            AudioAssetMetadata hydrated = *asset;
            const auto wavInfo = probeWavFile(QDir(m_config.libraryRoot()).filePath(hydrated.fileName));
            if (wavInfo && wavInfo->isValid()) {
                hydrated.durationMs = wavInfo->durationMs;
            }
            m_assets.push_back(hydrated);
        }
    }

    return true;
}

bool LibraryManager::saveIndex(QString* error) {
    QJsonArray assetsArray;
    for (const AudioAssetMetadata& asset : m_assets) {
        assetsArray.push_back(toJson(asset));
    }

    const QJsonObject root{
        {QStringLiteral("revision"), m_revision},
        {QStringLiteral("assets"), assetsArray},
    };
    return writeJsonDocumentToFile(m_config.libraryIndexPath(), QJsonDocument(root), error);
}

} // namespace apping
