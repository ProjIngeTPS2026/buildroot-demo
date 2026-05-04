#include "common/wav_utils.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

#include <QDataStream>

namespace apping {

namespace {

quint16 bytesPerSample(const QAudioFormat& format) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return static_cast<quint16>(format.sampleSize() / 8);
#else
    return static_cast<quint16>(format.bytesPerSample());
#endif
}

void setError(QString* error, const QString& message) {
    if (error) {
        *error = message;
    }
}

bool writeWavHeader(QIODevice* device,
                    int sampleRate,
                    int channelCount,
                    int bitsPerSample,
                    qint64 dataBytes) {
    if (!device || sampleRate <= 0 || channelCount <= 0 || bitsPerSample <= 0
        || dataBytes < 0 || dataBytes > std::numeric_limits<quint32>::max() - 36) {
        return false;
    }

    const quint16 bytesPerSampleValue = static_cast<quint16>(bitsPerSample / 8);
    const quint16 blockAlign = static_cast<quint16>(channelCount * bytesPerSampleValue);
    const quint32 byteRate = static_cast<quint32>(sampleRate * blockAlign);
    const quint32 riffChunkSize = static_cast<quint32>(36 + dataBytes);

    QDataStream stream(device);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("RIFF", 4);
    stream << riffChunkSize;
    stream.writeRawData("WAVE", 4);
    stream.writeRawData("fmt ", 4);
    stream << quint32(16);
    stream << quint16(1);
    stream << static_cast<quint16>(channelCount);
    stream << static_cast<quint32>(sampleRate);
    stream << byteRate;
    stream << blockAlign;
    stream << static_cast<quint16>(bitsPerSample);
    stream.writeRawData("data", 4);
    stream << static_cast<quint32>(dataBytes);
    return stream.status() == QDataStream::Ok;
}

} // namespace

bool WavFileInfo::isValid() const {
    return sampleRate > 0 && channelCount > 0 && bitsPerSample > 0 && byteRate > 0
        && blockAlign > 0 && dataOffset > 0 && dataBytes > 0;
}

std::optional<WavFileInfo> probeWavFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }

    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);

    char riff[4];
    char wave[4];
    quint32 riffSize = 0;

    if (stream.readRawData(riff, 4) != 4) {
        return std::nullopt;
    }
    stream >> riffSize;
    if (stream.readRawData(wave, 4) != 4) {
        return std::nullopt;
    }
    if (QByteArray(riff, 4) != QByteArrayLiteral("RIFF")
        || QByteArray(wave, 4) != QByteArrayLiteral("WAVE")) {
        return std::nullopt;
    }

    WavFileInfo info;
    bool sawFmt = false;
    bool sawData = false;
    while (!stream.atEnd()) {
        char chunkId[4];
        quint32 chunkSize = 0;
        if (stream.readRawData(chunkId, 4) != 4) {
            break;
        }
        stream >> chunkSize;
        const QByteArray id(chunkId, 4);

        if (id == QByteArrayLiteral("fmt ")) {
            quint16 audioFormat = 0;
            quint16 channels = 0;
            quint32 sampleRate = 0;
            quint32 byteRate = 0;
            quint16 blockAlign = 0;
            quint16 bitsPerSample = 0;

            if (chunkSize < 16) {
                return std::nullopt;
            }
            stream >> audioFormat;
            stream >> channels;
            stream >> sampleRate;
            stream >> byteRate;
            stream >> blockAlign;
            stream >> bitsPerSample;
            if (audioFormat != 1) {
                return std::nullopt;
            }
            if (chunkSize > 16) {
                stream.skipRawData(static_cast<int>(chunkSize - 16));
            }

            info.sampleRate = static_cast<int>(sampleRate);
            info.channelCount = static_cast<int>(channels);
            info.bitsPerSample = static_cast<int>(bitsPerSample);
            info.byteRate = static_cast<int>(byteRate);
            info.blockAlign = static_cast<int>(blockAlign);
            sawFmt = true;
        } else if (id == QByteArrayLiteral("data")) {
            info.dataOffset = file.pos();
            info.dataBytes = static_cast<qint64>(chunkSize);
            sawData = true;
            break;
        } else {
            stream.skipRawData(static_cast<int>(chunkSize));
        }

        if (chunkSize % 2 == 1) {
            stream.skipRawData(1);
        }
    }

    if (!sawFmt || !sawData || info.dataBytes == 0 || info.byteRate == 0) {
        return std::nullopt;
    }

    info.durationMs = static_cast<qint64>((1000.0 * info.dataBytes) / info.byteRate);
    return info;
}

QVector<qreal> readWavPeakEnvelope(const QString& path, int bucketCount) {
    const auto info = probeWavFile(path);
    if (!info || !info->isValid() || info->bitsPerSample != 16 || bucketCount <= 0) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(info->dataOffset)) {
        return {};
    }

    QVector<qreal> peaks(bucketCount, 0.0);
    const qint64 frameCount = info->dataBytes / info->blockAlign;
    if (frameCount <= 0) {
        return peaks;
    }

    QByteArray chunk;
    chunk.resize(64 * 1024);
    qint64 frameIndex = 0;
    qint64 bytesRemaining = info->dataBytes;
    while (bytesRemaining > 0) {
        const qint64 readTarget = std::min<qint64>(chunk.size(), bytesRemaining);
        const qint64 bytesRead = file.read(chunk.data(), readTarget);
        if (bytesRead <= 0) {
            break;
        }

        const qint64 frameBytes = bytesRead - (bytesRead % info->blockAlign);
        const qint64 framesInChunk = frameBytes / info->blockAlign;
        const auto* samples = reinterpret_cast<const qint16*>(chunk.constData());
        const int samplesPerFrame = info->channelCount;
        for (qint64 frame = 0; frame < framesInChunk; ++frame) {
            int peak = 0;
            for (int channel = 0; channel < samplesPerFrame; ++channel) {
                const int sampleIndex = static_cast<int>(frame * samplesPerFrame + channel);
                peak = std::max(peak, std::abs(static_cast<int>(samples[sampleIndex])));
            }
            const int bucket = std::clamp(
                static_cast<int>((frameIndex * bucketCount) / frameCount), 0, bucketCount - 1);
            peaks[bucket] = std::max(peaks[bucket],
                                     std::clamp(static_cast<qreal>(peak) / 32767.0, 0.0, 1.0));
            ++frameIndex;
        }

        bytesRemaining -= bytesRead;
    }

    return peaks;
}

bool cropWavFile(const QString& sourcePath,
                 const QString& targetPath,
                 qint64 startMs,
                 qint64 endMs,
                 QString* error) {
    const auto info = probeWavFile(sourcePath);
    if (!info || !info->isValid() || info->bitsPerSample != 16) {
        setError(error, QStringLiteral("Le fichier source n'est pas un WAV PCM 16 bits valide"));
        return false;
    }
    if (targetPath == sourcePath) {
        setError(error, QStringLiteral("La source et la destination doivent être différentes"));
        return false;
    }

    const qint64 durationMs = info->durationMs;
    const qint64 boundedStartMs = std::clamp<qint64>(startMs, 0, durationMs);
    const qint64 boundedEndMs = std::clamp<qint64>(endMs, 0, durationMs);
    if (boundedEndMs <= boundedStartMs) {
        setError(error, QStringLiteral("La sélection audio est vide"));
        return false;
    }

    const qint64 totalFrames = info->dataBytes / info->blockAlign;
    if (totalFrames <= 0) {
        setError(error, QStringLiteral("Le WAV source ne contient aucune frame audio complète"));
        return false;
    }
    const qint64 startFrame = std::clamp<qint64>(
        (boundedStartMs * info->sampleRate) / 1000, 0, totalFrames - 1);
    const qint64 requestedEndFrame = (boundedEndMs * info->sampleRate + 999) / 1000;
    const qint64 endFrame = std::min(std::max(requestedEndFrame, startFrame + 1), totalFrames);
    const qint64 selectedBytes = (endFrame - startFrame) * info->blockAlign;
    if (selectedBytes <= 0) {
        setError(error, QStringLiteral("La sélection audio est vide"));
        return false;
    }

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        setError(error, source.errorString());
        return false;
    }
    if (!source.seek(info->dataOffset + startFrame * info->blockAlign)) {
        setError(error, QStringLiteral("Impossible de positionner la lecture WAV"));
        return false;
    }

    QFile target(targetPath);
    if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(error, target.errorString());
        return false;
    }
    if (!writeWavHeader(&target,
                        info->sampleRate,
                        info->channelCount,
                        info->bitsPerSample,
                        selectedBytes)) {
        setError(error, QStringLiteral("Impossible d'écrire l'en-tête WAV"));
        return false;
    }

    QByteArray buffer;
    buffer.resize(64 * 1024);
    qint64 remaining = selectedBytes;
    while (remaining > 0) {
        const qint64 readTarget = std::min<qint64>(buffer.size(), remaining);
        const qint64 bytesRead = source.read(buffer.data(), readTarget);
        if (bytesRead <= 0) {
            setError(error, QStringLiteral("Lecture incomplète du WAV source"));
            return false;
        }
        if (target.write(buffer.constData(), bytesRead) != bytesRead) {
            setError(error, target.errorString());
            return false;
        }
        remaining -= bytesRead;
    }

    target.close();
    if (target.error() != QFile::NoError) {
        setError(error, target.errorString());
        return false;
    }
    return true;
}

WaveCaptureDevice::WaveCaptureDevice(QObject* parent)
    : QIODevice(parent) {
}

WaveCaptureDevice::~WaveCaptureDevice() {
    finalize();
}

bool WaveCaptureDevice::startCapture(const QString& filePath,
                                     const QAudioFormat& format,
                                     QString* error) {
    finalize();

    if (format.sampleRate() <= 0 || format.channelCount() <= 0
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        || format.sampleSize() != 16 || format.sampleType() != QAudioFormat::SignedInt
#else
        || format.sampleFormat() != QAudioFormat::Int16
#endif
        ) {
        if (error) {
            *error = QStringLiteral("Seule la capture PCM 16 bits mono/stéréo est prise en charge");
        }
        return false;
    }

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = m_file.errorString();
        }
        return false;
    }

    m_format = format;
    m_audioBytesWritten = 0;
    QIODevice::open(QIODevice::WriteOnly);
    writeHeader(0);
    return true;
}

void WaveCaptureDevice::finalize() {
    if (!m_file.isOpen()) {
        return;
    }

    writeHeader(m_audioBytesWritten);
    m_file.close();
    QIODevice::close();
}

qint64 WaveCaptureDevice::audioBytesWritten() const {
    return m_audioBytesWritten;
}

int WaveCaptureDevice::inputLevelPercent() const {
    return m_inputLevelPercent;
}

bool WaveCaptureDevice::saturated() const {
    return m_saturated;
}

qint64 WaveCaptureDevice::readData(char* /*data*/, qint64 /*maxSize*/) {
    return -1;
}

qint64 WaveCaptureDevice::writeData(const char* data, qint64 maxSize) {
    if (!m_file.isOpen()) {
        return -1;
    }
    const qint64 written = m_file.write(data, maxSize);
    if (written > 0) {
        m_audioBytesWritten += written;

        int peak = 0;
        const auto* samples = reinterpret_cast<const qint16*>(data);
        const qint64 sampleCount = written / static_cast<qint64>(sizeof(qint16));
        for (qint64 i = 0; i < sampleCount; ++i) {
            const int absolute = std::abs(static_cast<int>(samples[i]));
            peak = std::max(peak, absolute);
        }

        const int nextLevel = std::clamp(static_cast<int>((100.0 * peak) / 32767.0), 0, 100);
        const bool nextSaturated = nextLevel >= 92;
        if (nextLevel != m_inputLevelPercent || nextSaturated != m_saturated) {
            m_inputLevelPercent = nextLevel;
            m_saturated = nextSaturated;
            emit inputLevelChanged();
        }
    }
    return written;
}

void WaveCaptureDevice::writeHeader(qint64 dataBytes) {
    if (!m_file.isOpen()) {
        return;
    }

    m_file.seek(0);
    writeWavHeader(&m_file,
                   m_format.sampleRate(),
                   m_format.channelCount(),
                   bytesPerSample(m_format) * 8,
                   dataBytes);
    m_file.seek(44 + dataBytes);
}

} // namespace apping
