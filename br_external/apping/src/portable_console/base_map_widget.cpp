#include "portable_console/base_map_widget.h"

#include "common/app_paths.h"
#include "portable_console/base_directory_model.h"
#include "portable_console/portable_controller.h"

#include <algorithm>
#include <cmath>

#include <QAbstractItemModel>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

namespace apping {

namespace {

constexpr int TileSize = 256;
constexpr double MinLatitude = -85.05112878;
constexpr double MaxLatitude = 85.05112878;
constexpr int TapDragThreshold = 10;

double clampDouble(double value, double minimum, double maximum) {
    return std::max(minimum, std::min(value, maximum));
}

QColor statusColor(const QString& status, const QString& audioState, bool selected) {
    if (audioState == QStringLiteral("live_megaphone")) {
        return QColor(QStringLiteral("#ef4444"));
    }
    if (audioState == QStringLiteral("playing_prerecorded")) {
        return QColor(QStringLiteral("#f59e0b"));
    }
    if (selected) {
        return QColor(QStringLiteral("#2563eb"));
    }
    if (status == QStringLiteral("online") || status == QStringLiteral("busy")) {
        return QColor(QStringLiteral("#16a34a"));
    }
    if (status == QStringLiteral("stale")) {
        return QColor(QStringLiteral("#ca8a04"));
    }
    return QColor(QStringLiteral("#64748b"));
}

} // namespace

BaseMapWidget::BaseMapWidget(QWidget* parent)
    : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(180, 140);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void BaseMapWidget::setController(PortableController* controller) {
    m_controller = controller;
    if (m_controller) {
        m_zoom = clampDouble(m_controller->mapDefaultZoom(),
                             m_controller->mapMinZoom(),
                             m_controller->mapMaxZoom());
        m_centerWorld = latLonToWorld(m_controller->mapCenterLat(),
                                      m_controller->mapCenterLon(),
                                      m_zoom);
        connectModelSignals();
    }
    update();
}

void BaseMapWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(QStringLiteral("#d9e5eb")));

    if (!m_controller) {
        painter.setPen(QColor(QStringLiteral("#475569")));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Carte indisponible"));
        return;
    }

    drawTiles(painter);
    drawBases(painter);
}

void BaseMapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_pressPos = event->pos();
    m_dragging = true;
    m_lastMousePos = event->pos();
}

void BaseMapWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!m_dragging) {
        return;
    }
    const QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();
    m_centerWorld -= QPointF(delta.x(), delta.y());
    clampView();
    update();
}

void BaseMapWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }

    const bool wasDragging = m_dragging;
    m_dragging = false;
    if (!wasDragging) {
        return;
    }

    if ((event->pos() - m_pressPos).manhattanLength() > TapDragThreshold) {
        return;
    }

    const int row = baseRowAt(event->pos());
    if (row >= 0 && m_controller) {
        m_controller->toggleBaseSelection(row);
    }
}

void BaseMapWidget::wheelEvent(QWheelEvent* event) {
    const double steps = event->angleDelta().y() / 120.0;
    if (std::abs(steps) < 0.01) {
        return;
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const QPoint anchor = event->pos();
#else
    const QPoint anchor = event->position().toPoint();
#endif
    setZoomAt(m_zoom + steps * 0.5, anchor);
}

void BaseMapWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    clampView();
    update();
}

void BaseMapWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    clampView();
    update();
}

void BaseMapWidget::connectModelSignals() {
    auto* model = m_controller ? m_controller->baseModel() : nullptr;
    if (!model) {
        return;
    }
    connect(model, &QAbstractItemModel::dataChanged, this, QOverload<>::of(&BaseMapWidget::update));
    connect(model, &QAbstractItemModel::rowsInserted, this, QOverload<>::of(&BaseMapWidget::update));
    connect(model, &QAbstractItemModel::rowsRemoved, this, QOverload<>::of(&BaseMapWidget::update));
    connect(model, &QAbstractItemModel::modelReset, this, QOverload<>::of(&BaseMapWidget::update));
}

void BaseMapWidget::clampView() {
    if (!m_controller || width() <= 0 || height() <= 0) {
        return;
    }

    const QPointF minWorld = latLonToWorld(m_controller->mapMaxLat(),
                                           m_controller->mapMinLon(),
                                           m_zoom);
    const QPointF maxWorld = latLonToWorld(m_controller->mapMinLat(),
                                           m_controller->mapMaxLon(),
                                           m_zoom);
    const double halfWidth = width() / 2.0;
    const double halfHeight = height() / 2.0;
    m_centerWorld.setX(clampDouble(m_centerWorld.x(),
                                   minWorld.x() - halfWidth,
                                   maxWorld.x() + halfWidth));
    m_centerWorld.setY(clampDouble(m_centerWorld.y(),
                                   minWorld.y() - halfHeight,
                                   maxWorld.y() + halfHeight));
}

void BaseMapWidget::drawTiles(QPainter& painter) {
    const int tileZoom = static_cast<int>(std::round(m_zoom));
    const QPointF topLeft = m_centerWorld - QPointF(width() / 2.0, height() / 2.0);
    const int firstX = static_cast<int>(std::floor(topLeft.x() / TileSize));
    const int firstY = static_cast<int>(std::floor(topLeft.y() / TileSize));
    const int lastX = static_cast<int>(std::floor((topLeft.x() + width()) / TileSize));
    const int lastY = static_cast<int>(std::floor((topLeft.y() + height()) / TileSize));

    painter.setPen(QColor(QStringLiteral("#b8c7d0")));
    for (int y = firstY; y <= lastY; ++y) {
        for (int x = firstX; x <= lastX; ++x) {
            const QRect target(qRound(x * TileSize - topLeft.x()),
                               qRound(y * TileSize - topLeft.y()),
                               TileSize,
                               TileSize);
            const QPixmap pixmap = tilePixmap(tileZoom, x, y);
            if (pixmap.isNull()) {
                painter.fillRect(target, QColor(QStringLiteral("#e8eef2")));
                painter.drawRect(target);
            } else {
                painter.drawPixmap(target, pixmap);
            }
        }
    }
}

void BaseMapWidget::drawBases(QPainter& painter) {
    auto* model = qobject_cast<BaseDirectoryModel*>(m_controller ? m_controller->baseModel() : nullptr);
    if (!model) {
        return;
    }

    QFont labelFont = painter.font();
    labelFont.setPixelSize(11);
    labelFont.setBold(true);
    painter.setFont(labelFont);

    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        const double latitude = index.data(BaseDirectoryModel::LatitudeRole).toDouble();
        const double longitude = index.data(BaseDirectoryModel::LongitudeRole).toDouble();
        const bool selected = index.data(BaseDirectoryModel::SelectedRole).toBool();
        const QString status = index.data(BaseDirectoryModel::StatusRole).toString();
        const QString audioState = index.data(BaseDirectoryModel::AudioStateRole).toString();
        const QString name = index.data(BaseDirectoryModel::NameRole).toString();
        const QPointF point = worldToScreen(latLonToWorld(latitude, longitude, m_zoom));
        if (!rect().adjusted(-40, -40, 40, 40).contains(point.toPoint())) {
            continue;
        }

        const QColor fill = statusColor(status, audioState, selected);
        const int radius = selected ? 10 : 8;
        painter.setPen(QPen(Qt::white, selected ? 4 : 2));
        painter.setBrush(fill);
        painter.drawEllipse(point, radius, radius);
        painter.setPen(QColor(QStringLiteral("#0f172a")));
        painter.drawText(QPointF(point.x() + 12, point.y() - 8), name);
    }
}

QPointF BaseMapWidget::latLonToWorld(double latitude, double longitude, double zoom) const {
    const double lat = clampDouble(latitude, MinLatitude, MaxLatitude) * M_PI / 180.0;
    const double scale = TileSize * std::pow(2.0, zoom);
    const double x = (longitude + 180.0) / 360.0 * scale;
    const double y = (1.0 - std::log(std::tan(lat) + 1.0 / std::cos(lat)) / M_PI) / 2.0 * scale;
    return QPointF(x, y);
}

QPointF BaseMapWidget::worldToScreen(const QPointF& world) const {
    return world - m_centerWorld + QPointF(width() / 2.0, height() / 2.0);
}

int BaseMapWidget::baseRowAt(const QPoint& position) const {
    auto* model = qobject_cast<BaseDirectoryModel*>(m_controller ? m_controller->baseModel() : nullptr);
    if (!model) {
        return -1;
    }

    int bestRow = -1;
    qreal bestDistanceSquared = std::numeric_limits<qreal>::max();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        const double latitude = index.data(BaseDirectoryModel::LatitudeRole).toDouble();
        const double longitude = index.data(BaseDirectoryModel::LongitudeRole).toDouble();
        const bool selected = index.data(BaseDirectoryModel::SelectedRole).toBool();
        const QPointF point = worldToScreen(latLonToWorld(latitude, longitude, m_zoom));
        const qreal dx = point.x() - position.x();
        const qreal dy = point.y() - position.y();
        const qreal distanceSquared = dx * dx + dy * dy;
        const qreal hitRadius = selected ? 18.0 : 16.0;
        if (distanceSquared <= hitRadius * hitRadius && distanceSquared < bestDistanceSquared) {
            bestDistanceSquared = distanceSquared;
            bestRow = row;
        }
    }

    return bestRow;
}

QString BaseMapWidget::tilePath(int zoom, int x, int y) const {
    const QString root = AppPaths::resolvePath(QStringLiteral("assets/map/telecom_physique_strasbourg_tiles"));
    return QStringLiteral("%1/%2/%3/%4.png").arg(root).arg(zoom).arg(x).arg(y);
}

QPixmap BaseMapWidget::tilePixmap(int zoom, int x, int y) {
    const QString path = tilePath(zoom, x, y);
    if (m_tileCache.contains(path)) {
        return m_tileCache.value(path);
    }

    QPixmap pixmap(path);
    if (m_tileCache.size() > 96) {
        m_tileCache.clear();
    }
    m_tileCache.insert(path, pixmap);
    return pixmap;
}

void BaseMapWidget::setZoomAt(double nextZoom, const QPoint& anchor) {
    if (!m_controller) {
        return;
    }
    nextZoom = clampDouble(nextZoom, m_controller->mapMinZoom(), m_controller->mapMaxZoom());
    if (std::abs(nextZoom - m_zoom) < 0.01) {
        return;
    }

    const QPointF anchorWorld = m_centerWorld + QPointF(anchor.x() - width() / 2.0,
                                                       anchor.y() - height() / 2.0);
    const double factor = std::pow(2.0, nextZoom - m_zoom);
    m_centerWorld = anchorWorld * factor - QPointF(anchor.x() - width() / 2.0,
                                                   anchor.y() - height() / 2.0);
    m_zoom = nextZoom;
    clampView();
    update();
}

} // namespace apping
