#include "common/roc_integration.h"

#include <QFileInfo>
#include <QUrl>

namespace apping {

QString toRocFileUri(const QString& filePath) {
    return QUrl::fromLocalFile(QFileInfo(filePath).absoluteFilePath()).toString();
}

QStringList buildRocSendArguments(const QStringList& sourceEndpoints,
                                  const QString& inputUri) {
    QStringList args{
        QStringLiteral("-v"),
        QStringLiteral("--packet-encoding=opus-mono"),
        QStringLiteral("--packet-len=40ms"),
        QStringLiteral("--opus-bitrate=16000"),
        QStringLiteral("--opus-complexity=5"),
        QStringLiteral("--opus-application=voip"),
        QStringLiteral("--opus-vbr=constrained"),
        QStringLiteral("--target-latency=240ms"),
        QStringLiteral("--latency-profile=responsive"),
        QStringLiteral("--frame-len=10ms"),
    };

    if (inputUri.startsWith(QStringLiteral("pulse://"))) {
        args << QStringLiteral("--io-latency=80ms")
             << QStringLiteral("--rate=48000");
    }

    if (!inputUri.isEmpty()) {
        args << QStringLiteral("-i") << inputUri;
    }

    for (const QString& sourceEndpoint : sourceEndpoints) {
        args << QStringLiteral("-s") << sourceEndpoint;
    }

    return args;
}

QStringList buildRocReceiveArguments(quint16 listenPort,
                                     const QString& outputUri,
                                     const QString& listenAddress,
                                     const QString& multicastInterface) {
    QStringList args{
        QStringLiteral("-v"),
        QStringLiteral("--packet-encoding=opus-mono"),
        QStringLiteral("--target-latency=240ms"),
        QStringLiteral("--io-latency=80ms"),
        QStringLiteral("--latency-profile=responsive"),
        QStringLiteral("--frame-len=10ms"),
        QStringLiteral("--rate=48000"),
        QStringLiteral("--reuseaddr"),
    };

    if (!multicastInterface.trimmed().isEmpty()) {
        args << QStringLiteral("--miface=%1").arg(multicastInterface.trimmed());
    }

    args << QStringLiteral("-s")
         << QStringLiteral("rtp://%1:%2").arg(listenAddress, QString::number(listenPort));

    if (!outputUri.isEmpty()) {
        args << QStringLiteral("-o") << outputUri;
    }

    return args;
}

} // namespace apping
