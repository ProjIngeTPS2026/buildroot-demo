#pragma once

#include <QString>
#include <QStringList>

namespace apping {

class AppPaths {
public:
    static QString rootDirectory();
    static QString assetsDirectory();
    static QString resolvePath(const QString& relativeOrAbsolutePath);
    static QString findBinary(const QString& executableName,
                              const QStringList& preferredRelativePaths = {});
    static QString rocSendBinary();
    static QString rocRecvBinary();
};

} // namespace apping
