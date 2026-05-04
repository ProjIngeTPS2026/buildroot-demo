#pragma once

#include "common/models.h"

#include <QAbstractListModel>
#include <QVector>

namespace apping {

class AssetLibraryModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        AssetIdRole = Qt::UserRole + 1,
        LabelRole,
        DurationMsRole,
        DurationTextRole,
        AvailableCountRole,
        SourcesRole,
        SourcesTextRole,
    };

    explicit AssetLibraryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setAssets(const QVector<AudioAssetMetadata>& assets);
    AudioAssetMetadata assetAt(int row) const;
    int findRowByAssetId(const QString& assetId) const;

private:
    QVector<AudioAssetMetadata> m_assets;
};

} // namespace apping
