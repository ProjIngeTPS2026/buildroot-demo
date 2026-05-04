#pragma once

#include "common/models.h"

#include <QHash>
#include <QModelIndex>
#include <QPixmap>
#include <QPointer>
#include <QWidget>
#include <QResizeEvent>
#include <QShowEvent>

namespace apping {

class PortableController;

class BaseMapWidget : public QWidget {
    Q_OBJECT

public:
    explicit BaseMapWidget(QWidget* parent = nullptr);

    void setController(PortableController* controller);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QPointer<PortableController> m_controller;
    QHash<QString, QPixmap> m_tileCache;
    QPoint m_pressPos;
    QPoint m_lastMousePos;
    QPointF m_centerWorld;
    double m_zoom = 17.0;
    bool m_dragging = false;

    void connectModelSignals();
    void clampView();
    void drawTiles(QPainter& painter);
    void drawBases(QPainter& painter);
    QPointF latLonToWorld(double latitude, double longitude, double zoom) const;
    QPointF worldToScreen(const QPointF& world) const;
    int baseRowAt(const QPoint& position) const;
    QString tilePath(int zoom, int x, int y) const;
    QPixmap tilePixmap(int zoom, int x, int y);
    void setZoomAt(double nextZoom, const QPoint& anchor);
};

} // namespace apping
