#include "portable_console/base_directory_model.h"

namespace apping {

namespace {

QString audioStateLabel(const BaseSnapshot& snapshot) {
    if (snapshot.status == QStringLiteral("offline")) {
        return QStringLiteral("Hors ligne");
    }
    if (snapshot.status == QStringLiteral("stale")) {
        return QStringLiteral("Signal ancien");
    }
    if (snapshot.audioState == QStringLiteral("playing_prerecorded")) {
        return QStringLiteral("Diffusion en cours");
    }
    if (snapshot.audioState == QStringLiteral("live_megaphone")) {
        return QStringLiteral("Micro en direct");
    }
    if (snapshot.audioState == QStringLiteral("megaphone_ready")) {
        return QStringLiteral("Micro prêt");
    }
    if (snapshot.audioState == QStringLiteral("idle")) {
        return QStringLiteral("Disponible");
    }
    return snapshot.audioState;
}

} // namespace

BaseDirectoryModel::BaseDirectoryModel(QObject* parent)
    : QAbstractListModel(parent) {
}

int BaseDirectoryModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_items.size();
}

QVariant BaseDirectoryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const BaseSnapshot& snapshot = m_items.at(index.row());
    const bool selected = m_selectedById.value(snapshot.baseId, false);
    switch (role) {
    case Qt::DisplayRole:
        return QStringLiteral("%1 %2\n%3\n%4 • vu %5")
            .arg(selected ? QStringLiteral("[x]") : QStringLiteral("[ ]"),
                 snapshot.name,
                 snapshot.description,
                 audioStateLabel(snapshot),
                 snapshot.lastSeen.isValid()
                     ? snapshot.lastSeen.toLocalTime().toString(QStringLiteral("HH:mm:ss"))
                     : QStringLiteral("--:--:--"));
    case Qt::ToolTipRole:
        return QStringLiteral("%1\nHôte: %2:%3\nPort micro: %4\nSortie: %5")
            .arg(snapshot.description,
                 snapshot.host,
                 QString::number(snapshot.controlPort),
                 QString::number(snapshot.megaphonePort),
                 snapshot.audioOutputUri);
    case BaseIdRole:
        return snapshot.baseId;
    case NameRole:
        return snapshot.name;
    case DescriptionRole:
        return snapshot.description;
    case LatitudeRole:
        return snapshot.latitude;
    case LongitudeRole:
        return snapshot.longitude;
    case HostRole:
        return snapshot.host;
    case ControlPortRole:
        return snapshot.controlPort;
    case MegaphonePortRole:
        return snapshot.megaphonePort;
    case AudioStateRole:
        return snapshot.audioState;
    case StatusRole:
        return snapshot.status;
    case LastSeenRole:
        return snapshot.lastSeen;
    case LastSeenTextRole:
        return snapshot.lastSeen.isValid()
            ? snapshot.lastSeen.toLocalTime().toString(QStringLiteral("HH:mm:ss"))
            : QString();
    case SelectedRole:
        return selected;
    default:
        return {};
    }
}

QHash<int, QByteArray> BaseDirectoryModel::roleNames() const {
    return {
        {BaseIdRole, "baseId"},
        {NameRole, "name"},
        {DescriptionRole, "description"},
        {LatitudeRole, "latitude"},
        {LongitudeRole, "longitude"},
        {HostRole, "host"},
        {ControlPortRole, "controlPort"},
        {MegaphonePortRole, "megaphonePort"},
        {AudioStateRole, "audioState"},
        {StatusRole, "status"},
        {LastSeenRole, "lastSeen"},
        {LastSeenTextRole, "lastSeenText"},
        {SelectedRole, "selected"},
    };
}

void BaseDirectoryModel::upsert(BaseSnapshot snapshot) {
    const bool selected = m_selectedById.value(snapshot.baseId, false);
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items[row].baseId == snapshot.baseId) {
            m_items[row] = snapshot;
            m_selectedById.insert(snapshot.baseId, selected);
            emit dataChanged(index(row), index(row));
            return;
        }
    }

    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.push_back(snapshot);
    m_selectedById.insert(snapshot.baseId, selected);
    endInsertRows();
}

void BaseDirectoryModel::expireStale(int staleMs, int offlineMs) {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (int row = 0; row < m_items.size(); ++row) {
        BaseSnapshot& snapshot = m_items[row];
        const qint64 ageMs = snapshot.lastSeen.msecsTo(now);
        QString newStatus = snapshot.status;
        if (ageMs >= offlineMs) {
            newStatus = QStringLiteral("offline");
        } else if (ageMs >= staleMs) {
            newStatus = QStringLiteral("stale");
        } else {
            newStatus = snapshot.audioState == QStringLiteral("idle")
                ? QStringLiteral("online")
                : QStringLiteral("busy");
        }
        if (newStatus != snapshot.status) {
            snapshot.status = newStatus;
            emit dataChanged(index(row),
                             index(row),
                             {Qt::DisplayRole, StatusRole});
        }
    }
}

void BaseDirectoryModel::toggleSelection(int row) {
    if (row < 0 || row >= m_items.size()) {
        return;
    }
    const QString baseId = m_items[row].baseId;
    m_selectedById.insert(baseId, !m_selectedById.value(baseId, false));
    emit dataChanged(index(row), index(row), {Qt::DisplayRole, SelectedRole});
}

void BaseDirectoryModel::clearSelection() {
    for (int row = 0; row < m_items.size(); ++row) {
        if (!m_selectedById.value(m_items[row].baseId, false)) {
            continue;
        }
        m_selectedById.insert(m_items[row].baseId, false);
        emit dataChanged(index(row), index(row), {Qt::DisplayRole, SelectedRole});
    }
}

void BaseDirectoryModel::selectAllOnline() {
    for (int row = 0; row < m_items.size(); ++row) {
        const bool online =
            m_items[row].status == QStringLiteral("online")
            || m_items[row].status == QStringLiteral("busy");
        if (!online || m_selectedById.value(m_items[row].baseId, false)) {
            continue;
        }
        m_selectedById.insert(m_items[row].baseId, true);
        emit dataChanged(index(row), index(row), {Qt::DisplayRole, SelectedRole});
    }
}

int BaseDirectoryModel::selectedCount() const {
    int count = 0;
    for (const BaseSnapshot& snapshot : m_items) {
        if (m_selectedById.value(snapshot.baseId, false)) {
            ++count;
        }
    }
    return count;
}

int BaseDirectoryModel::onlineCount() const {
    int count = 0;
    for (const BaseSnapshot& snapshot : m_items) {
        if (snapshot.status == QStringLiteral("online")
            || snapshot.status == QStringLiteral("busy")) {
            ++count;
        }
    }
    return count;
}

bool BaseDirectoryModel::hasSelected() const {
    return selectedCount() > 0;
}

QVector<BaseSnapshot> BaseDirectoryModel::selectedSnapshots() const {
    QVector<BaseSnapshot> result;
    for (const BaseSnapshot& snapshot : m_items) {
        if (m_selectedById.value(snapshot.baseId, false)) {
            result.push_back(snapshot);
        }
    }
    return result;
}

QVector<BaseSnapshot> BaseDirectoryModel::selectedOnlineSnapshots() const {
    QVector<BaseSnapshot> result;
    for (const BaseSnapshot& snapshot : m_items) {
        const bool selected = m_selectedById.value(snapshot.baseId, false);
        const bool online =
            snapshot.status == QStringLiteral("online")
            || snapshot.status == QStringLiteral("busy");
        if (selected && online) {
            result.push_back(snapshot);
        }
    }
    return result;
}

QVector<BaseSnapshot> BaseDirectoryModel::onlineSnapshots() const {
    QVector<BaseSnapshot> result;
    for (const BaseSnapshot& snapshot : m_items) {
        if (snapshot.status == QStringLiteral("online")
            || snapshot.status == QStringLiteral("busy")) {
            result.push_back(snapshot);
        }
    }
    return result;
}

BaseSnapshot BaseDirectoryModel::snapshotAt(int row) const {
    return (row >= 0 && row < m_items.size()) ? m_items.at(row) : BaseSnapshot{};
}

std::optional<BaseSnapshot> BaseDirectoryModel::snapshotByBaseId(const QString& baseId) const {
    for (const BaseSnapshot& snapshot : m_items) {
        if (snapshot.baseId == baseId) {
            return snapshot;
        }
    }
    return std::nullopt;
}

} // namespace apping
