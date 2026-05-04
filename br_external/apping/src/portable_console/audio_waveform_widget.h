#pragma once

#include <QVector>
#include <QWidget>

namespace apping {

class AudioWaveformWidget : public QWidget {
    Q_OBJECT

public:
    explicit AudioWaveformWidget(QWidget* parent = nullptr);

    void setWaveform(const QVector<qreal>& peaks, qint64 durationMs);
    void clearWaveform();
    void setSelection(qint64 startMs, qint64 endMs);
    void setPlaybackPosition(qint64 positionMs);

    qint64 selectionStartMs() const;
    qint64 selectionEndMs() const;

signals:
    void selectionChanged(qint64 startMs, qint64 endMs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class DragHandle {
        None,
        Start,
        End,
    };

    QVector<qreal> m_peaks;
    qint64 m_durationMs = 0;
    qint64 m_selectionStartMs = 0;
    qint64 m_selectionEndMs = 0;
    qint64 m_playbackPositionMs = -1;
    DragHandle m_dragHandle = DragHandle::None;

    QRectF waveformRect() const;
    qreal msToX(qint64 valueMs) const;
    qint64 xToMs(qreal x) const;
    void updateSelectionFromPosition(qreal x);
};

} // namespace apping
