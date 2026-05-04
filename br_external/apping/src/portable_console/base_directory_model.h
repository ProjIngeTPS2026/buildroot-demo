#pragma once

#include "common/models.h"

#include <QAbstractListModel>
#include <QVector>

#include <optional>

namespace apping {

class BaseDirectoryModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        BaseIdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        LatitudeRole,
        LongitudeRole,
        HostRole,
        ControlPortRole,
        MegaphonePortRole,
        AudioStateRole,
        StatusRole,
        LastSeenRole,
        LastSeenTextRole,
        SelectedRole,
    };

    explicit BaseDirectoryModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void upsert(BaseSnapshot snapshot);
    void expireStale(int staleMs = 5000, int offlineMs = 12000);
    void toggleSelection(int row);
    void clearSelection();
    void selectAllOnline();
    int selectedCount() const;
    int onlineCount() const;
    bool hasSelected() const;

    QVector<BaseSnapshot> selectedSnapshots() const;
    QVector<BaseSnapshot> selectedOnlineSnapshots() const;
    QVector<BaseSnapshot> onlineSnapshots() const;
    BaseSnapshot snapshotAt(int row) const;
    std::optional<BaseSnapshot> snapshotByBaseId(const QString& baseId) const;

private:
    QVector<BaseSnapshot> m_items;
    QHash<QString, bool> m_selectedById;
};

} // namespace apping
