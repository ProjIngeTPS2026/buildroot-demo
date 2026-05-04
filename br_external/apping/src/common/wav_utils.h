#pragma once

#include <optional>

#include <QAudioFormat>
#include <QFile>
#include <QIODevice>
#include <QObject>
#include <QVector>

namespace apping {

struct WavFileInfo {
    int sampleRate = 0;
    int channelCount = 0;
    int bitsPerSample = 0;
    int byteRate = 0;
    int blockAlign = 0;
    qint64 dataOffset = 0;
    qint64 dataBytes = 0;
    qint64 durationMs = 0;

    bool isValid() const;
};

std::optional<WavFileInfo> probeWavFile(const QString& path);
QVector<qreal> readWavPeakEnvelope(const QString& path, int bucketCount);
bool cropWavFile(const QString& sourcePath,
                 const QString& targetPath,
                 qint64 startMs,
                 qint64 endMs,
                 QString* error = nullptr);

class WaveCaptureDevice : public QIODevice {
    Q_OBJECT

public:
    explicit WaveCaptureDevice(QObject* parent = nullptr);
    ~WaveCaptureDevice() override;

    bool startCapture(const QString& filePath, const QAudioFormat& format, QString* error = nullptr);
    void finalize();
    qint64 audioBytesWritten() const;
    int inputLevelPercent() const;
    bool saturated() const;

signals:
    void inputLevelChanged();

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

private:
    QFile m_file;
    QAudioFormat m_format;
    qint64 m_audioBytesWritten = 0;
    int m_inputLevelPercent = 0;
    bool m_saturated = false;

    void writeHeader(qint64 dataBytes);
};

} // namespace apping
