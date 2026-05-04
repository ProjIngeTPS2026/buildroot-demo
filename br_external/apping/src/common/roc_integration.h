#pragma once

#include <QString>
#include <QStringList>

namespace apping {

QString toRocFileUri(const QString& filePath);
QStringList buildRocSendArguments(const QStringList& sourceEndpoints,
                                  const QString& inputUri = {});
QStringList buildRocReceiveArguments(quint16 listenPort,
                                     const QString& outputUri = {},
                                     const QString& listenAddress = QStringLiteral("0.0.0.0"),
                                     const QString& multicastInterface = {});

} // namespace apping
