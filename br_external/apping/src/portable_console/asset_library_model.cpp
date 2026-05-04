#include "portable_console/asset_library_model.h"

namespace apping {

namespace {

QString formatDuration(qint64 durationMs) {
    const qint64 totalSeconds = durationMs / 1000;
    const qint64 minutes = totalSeconds / 60;
    const qint64 seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

bool sameAsset(const AudioAssetMetadata& left, const AudioAssetMetadata& right) {
    return left.assetId == right.assetId
        && left.label == right.label
        && left.fileName == right.fileName
        && left.durationMs == right.durationMs
        && left.createdAt == right.createdAt
        && left.source == right.source
        && left.availableOn == right.availableOn;
}

} // namespace

AssetLibraryModel::AssetLibraryModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int AssetLibraryModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_assets.size();
}

QVariant AssetLibraryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_assets.size()) {
        return {};
    }

    const AudioAssetMetadata& asset = m_assets.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return QStringLiteral("%1\n%2 • disponible sur %3")
            .arg(asset.label,
                 formatDuration(asset.durationMs),
                 asset.availableOn.isEmpty()
                     ? QStringLiteral("aucune base")
                     : asset.availableOn.join(QStringLiteral(", ")));
    case Qt::ToolTipRole:
        return QStringLiteral("ID message: %1\nFichier: %2\nDurée: %3")
            .arg(asset.assetId,
                 asset.fileName,
                 formatDuration(asset.durationMs));
    case AssetIdRole:
        return asset.assetId;
    case LabelRole:
        return asset.label;
    case DurationMsRole:
        return asset.durationMs;
    case DurationTextRole:
        return formatDuration(asset.durationMs);
    case AvailableCountRole:
        return asset.availableOn.size();
    case SourcesRole:
        return asset.availableOn;
    case SourcesTextRole:
        return asset.availableOn.join(QStringLiteral(", "));
    default:
        return {};
    }
}

QHash<int, QByteArray> AssetLibraryModel::roleNames() const {
    return {
        {AssetIdRole, "assetId"},
        {LabelRole, "label"},
        {DurationMsRole, "durationMs"},
        {DurationTextRole, "durationText"},
        {AvailableCountRole, "availableCount"},
        {SourcesRole, "sources"},
        {SourcesTextRole, "sourcesText"},
    };
}

void AssetLibraryModel::setAssets(const QVector<AudioAssetMetadata>& assets) {
    if (m_assets.size() == assets.size()) {
        bool unchanged = true;
        for (int index = 0; index < m_assets.size(); ++index) {
            if (!sameAsset(m_assets.at(index), assets.at(index))) {
                unchanged = false;
                break;
            }
        }
        if (unchanged) {
            return;
        }
    }

    beginResetModel();
    m_assets = assets;
    endResetModel();
}

AudioAssetMetadata AssetLibraryModel::assetAt(int row) const {
    return (row >= 0 && row < m_assets.size()) ? m_assets.at(row) : AudioAssetMetadata{};
}

int AssetLibraryModel::findRowByAssetId(const QString& assetId) const {
    for (int row = 0; row < m_assets.size(); ++row) {
        if (m_assets.at(row).assetId == assetId) {
            return row;
        }
    }
    return -1;
}

} // namespace apping
