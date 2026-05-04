#include "portable_console/audio_waveform_widget.h"

#include <algorithm>

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

namespace apping {

namespace {

QString timeText(qint64 valueMs) {
    const qint64 secondsTotal = std::max<qint64>(0, valueMs / 1000);
    return QStringLiteral("%1:%2")
        .arg(secondsTotal / 60, 2, 10, QLatin1Char('0'))
        .arg(secondsTotal % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

AudioWaveformWidget::AudioWaveformWidget(QWidget* parent)
    : QWidget(parent) {
    setMinimumHeight(58);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("Déplace les deux bornes pour sélectionner la partie utile du message"));
}

void AudioWaveformWidget::setWaveform(const QVector<qreal>& peaks, qint64 durationMs) {
    m_peaks = peaks;
    m_durationMs = std::max<qint64>(0, durationMs);
    m_selectionStartMs = 0;
    m_selectionEndMs = m_durationMs;
    m_playbackPositionMs = -1;
    update();
}

void AudioWaveformWidget::clearWaveform() {
    m_peaks.clear();
    m_durationMs = 0;
    m_selectionStartMs = 0;
    m_selectionEndMs = 0;
    m_playbackPositionMs = -1;
    m_dragHandle = DragHandle::None;
    update();
}

void AudioWaveformWidget::setSelection(qint64 startMs, qint64 endMs) {
    if (m_durationMs <= 0) {
        clearWaveform();
        return;
    }

    const qint64 minSpanMs = std::min<qint64>(80, m_durationMs);
    const qint64 boundedStart = std::clamp(startMs, qint64(0), std::max<qint64>(0, m_durationMs - minSpanMs));
    const qint64 boundedEnd = std::clamp(endMs, boundedStart + minSpanMs, m_durationMs);
    if (m_selectionStartMs == boundedStart && m_selectionEndMs == boundedEnd) {
        return;
    }

    m_selectionStartMs = boundedStart;
    m_selectionEndMs = boundedEnd;
    update();
}

void AudioWaveformWidget::setPlaybackPosition(qint64 positionMs) {
    const qint64 bounded = positionMs < 0 || m_durationMs <= 0
        ? qint64(-1)
        : std::clamp(positionMs, qint64(0), m_durationMs);
    if (m_playbackPositionMs == bounded) {
        return;
    }
    m_playbackPositionMs = bounded;
    update();
}

qint64 AudioWaveformWidget::selectionStartMs() const {
    return m_selectionStartMs;
}

qint64 AudioWaveformWidget::selectionEndMs() const {
    return m_selectionEndMs;
}

void AudioWaveformWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF outer = rect().adjusted(1, 1, -1, -1);
    painter.setPen(QPen(QColor(QStringLiteral("#cfd8e3")), 1));
    painter.setBrush(QColor(QStringLiteral("#f8fafc")));
    painter.drawRoundedRect(outer, 8, 8);

    const QRectF waveRect = waveformRect();
    if (m_peaks.isEmpty() || m_durationMs <= 0) {
        painter.setPen(QColor(QStringLiteral("#64748b")));
        painter.drawText(outer, Qt::AlignCenter, QStringLiteral("Signal affiché après l'enregistrement"));
        return;
    }

    const qreal startX = msToX(m_selectionStartMs);
    const qreal endX = msToX(m_selectionEndMs);
    const QRectF selectedRect(QPointF(startX, waveRect.top()),
                              QPointF(endX, waveRect.bottom()));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#dbeafe")));
    painter.drawRoundedRect(selectedRect.adjusted(0, 1, 0, -1), 5, 5);

    const qreal centerY = waveRect.center().y();
    const qreal maxHalfHeight = waveRect.height() * 0.44;
    const qreal slotWidth = waveRect.width() / std::max(1, m_peaks.size());
    for (int i = 0; i < m_peaks.size(); ++i) {
        const qreal x = waveRect.left() + (i + 0.5) * slotWidth;
        const qreal peak = std::clamp(m_peaks.at(i), 0.0, 1.0);
        const qreal halfHeight = std::max<qreal>(1.0, peak * maxHalfHeight);
        const bool selected = x >= startX && x <= endX;
        painter.setPen(QPen(selected ? QColor(QStringLiteral("#1d4ed8"))
                                     : QColor(QStringLiteral("#94a3b8")),
                            std::max<qreal>(1.0, std::min<qreal>(3.0, slotWidth * 0.55)),
                            Qt::SolidLine,
                            Qt::RoundCap));
        painter.drawLine(QPointF(x, centerY - halfHeight), QPointF(x, centerY + halfHeight));
    }

    painter.setPen(QPen(QColor(QStringLiteral("#1e3a8a")), 2));
    painter.drawLine(QPointF(startX, waveRect.top()), QPointF(startX, waveRect.bottom()));
    painter.drawLine(QPointF(endX, waveRect.top()), QPointF(endX, waveRect.bottom()));

    const auto drawHandle = [&](qreal x, const QString& label) {
        const QRectF handleRect(x - 10, waveRect.top() - 1, 20, waveRect.height() + 2);
        painter.setPen(QPen(QColor(QStringLiteral("#1e3a8a")), 1));
        painter.setBrush(QColor(QStringLiteral("#ffffff")));
        painter.drawRoundedRect(handleRect.adjusted(2, 4, -2, -4), 6, 6);
        painter.setPen(QColor(QStringLiteral("#1e3a8a")));
        painter.drawText(QRectF(x - 28, outer.bottom() - 18, 56, 15),
                         Qt::AlignCenter,
                         label);
    };
    drawHandle(startX, timeText(m_selectionStartMs));
    drawHandle(endX, timeText(m_selectionEndMs));

    if (m_playbackPositionMs >= 0) {
        const qreal playbackX = msToX(m_playbackPositionMs);
        painter.setPen(QPen(QColor(QStringLiteral("#f97316")), 2));
        painter.drawLine(QPointF(playbackX, waveRect.top() - 3),
                         QPointF(playbackX, waveRect.bottom() + 3));
    }
}

void AudioWaveformWidget::mousePressEvent(QMouseEvent* event) {
    if (m_durationMs <= 0 || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const qreal x = event->pos().x();
    const qreal startDistance = std::abs(x - msToX(m_selectionStartMs));
    const qreal endDistance = std::abs(x - msToX(m_selectionEndMs));
    const qreal handleRadius = 22.0;
    if (startDistance <= handleRadius || endDistance <= handleRadius) {
        m_dragHandle = startDistance <= endDistance ? DragHandle::Start : DragHandle::End;
    } else {
        const qint64 positionMs = xToMs(x);
        const qint64 selectionMiddle = (m_selectionStartMs + m_selectionEndMs) / 2;
        m_dragHandle = positionMs <= selectionMiddle ? DragHandle::Start : DragHandle::End;
    }
    updateSelectionFromPosition(x);
}

void AudioWaveformWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragHandle == DragHandle::None) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    updateSelectionFromPosition(event->pos().x());
}

void AudioWaveformWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragHandle = DragHandle::None;
    }
    QWidget::mouseReleaseEvent(event);
}

QRectF AudioWaveformWidget::waveformRect() const {
    return rect().adjusted(12, 7, -12, -19);
}

qreal AudioWaveformWidget::msToX(qint64 valueMs) const {
    const QRectF waveRect = waveformRect();
    if (m_durationMs <= 0) {
        return waveRect.left();
    }
    const qreal ratio = static_cast<qreal>(std::clamp(valueMs, qint64(0), m_durationMs))
        / static_cast<qreal>(m_durationMs);
    return waveRect.left() + ratio * waveRect.width();
}

qint64 AudioWaveformWidget::xToMs(qreal x) const {
    const QRectF waveRect = waveformRect();
    if (m_durationMs <= 0 || waveRect.width() <= 0) {
        return 0;
    }
    const qreal ratio = std::clamp((x - waveRect.left()) / waveRect.width(), 0.0, 1.0);
    return static_cast<qint64>(ratio * m_durationMs);
}

void AudioWaveformWidget::updateSelectionFromPosition(qreal x) {
    if (m_durationMs <= 0 || m_dragHandle == DragHandle::None) {
        return;
    }

    const qint64 minSpanMs = std::min<qint64>(80, m_durationMs);
    const qint64 positionMs = xToMs(x);
    qint64 nextStart = m_selectionStartMs;
    qint64 nextEnd = m_selectionEndMs;
    if (m_dragHandle == DragHandle::Start) {
        nextStart = std::clamp(positionMs, qint64(0), std::max<qint64>(0, m_selectionEndMs - minSpanMs));
    } else {
        nextEnd = std::clamp(positionMs, std::min(m_durationMs, m_selectionStartMs + minSpanMs), m_durationMs);
    }

    if (nextStart == m_selectionStartMs && nextEnd == m_selectionEndMs) {
        return;
    }

    m_selectionStartMs = nextStart;
    m_selectionEndMs = nextEnd;
    update();
    emit selectionChanged(m_selectionStartMs, m_selectionEndMs);
}

} // namespace apping
