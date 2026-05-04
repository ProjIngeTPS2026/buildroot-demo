#include "common/app_paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>

namespace apping {

namespace {

QStringList rootCandidates() {
    const QString applicationDir = QCoreApplication::applicationDirPath();
    const QString currentDir = QDir::currentPath();

    QStringList candidates{
        QProcessEnvironment::systemEnvironment().value(QStringLiteral("APPING_ROOT")),
        QStringLiteral("/usr/share/apping"),
        QStringLiteral("/usr/local/share/apping"),
        applicationDir,
        QDir(applicationDir).filePath(QStringLiteral("..")),
        QDir(applicationDir).filePath(QStringLiteral("../..")),
        currentDir,
        QDir(currentDir).filePath(QStringLiteral("..")),
    };

    candidates.removeAll(QString());
    for (QString& candidate : candidates) {
        const QString originalCandidate = candidate;
        candidate = QFileInfo(originalCandidate).canonicalFilePath();
        if (candidate.isEmpty()) {
            candidate = QDir(originalCandidate).absolutePath();
        }
    }
    candidates.removeDuplicates();
    return candidates;
}

QString findProjectRoot() {
    for (const QString& candidate : rootCandidates()) {
        if (QFileInfo::exists(QDir(candidate).filePath(QStringLiteral("assets/map")))) {
            return candidate;
        }
    }
    return QDir::currentPath();
}

} // namespace

QString AppPaths::rootDirectory() {
    static const QString root = findProjectRoot();
    return root;
}

QString AppPaths::assetsDirectory() {
    return QDir(rootDirectory()).filePath(QStringLiteral("assets"));
}

QString AppPaths::resolvePath(const QString& relativeOrAbsolutePath) {
    const QFileInfo fileInfo(relativeOrAbsolutePath);
    if (fileInfo.isAbsolute()) {
        return relativeOrAbsolutePath;
    }
    return QDir(rootDirectory()).filePath(relativeOrAbsolutePath);
}

QString AppPaths::findBinary(const QString& executableName,
                             const QStringList& preferredRelativePaths) {
    const QString upperName = executableName.toUpper().replace(QLatin1Char('-'),
                                                               QLatin1Char('_'));
    const QString envOverride =
        QProcessEnvironment::systemEnvironment().value(upperName);
    if (!envOverride.isEmpty() && QFileInfo::exists(envOverride)) {
        return envOverride;
    }

    for (const QString& preferred : preferredRelativePaths) {
        const QString candidate = resolvePath(preferred);
        if (QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return executableName;
}

QString AppPaths::rocSendBinary() {
    return findBinary(QStringLiteral("roc-send"),
                      {QStringLiteral("/usr/bin/roc-send"),
                       QStringLiteral("/usr/local/bin/roc-send"),
                       QStringLiteral("roc-toolkit-opus-master/bin/x86_64-pc-linux-gnu/roc-send"),
                       QStringLiteral("roc-toolkit-opus-master/build/src/x86_64-pc-linux-gnu/gcc-15.2.0-release/roc-send"),
                       QStringLiteral("roc-toolkit-opus-master/bin/roc-send"),
                       QStringLiteral("roc-toolkit-opus-master/build/roc-send"),
                       QStringLiteral("roc-toolkit-opus-master/roc-send")});
}

QString AppPaths::rocRecvBinary() {
    return findBinary(QStringLiteral("roc-recv"),
                      {QStringLiteral("/usr/bin/roc-recv"),
                       QStringLiteral("/usr/local/bin/roc-recv"),
                       QStringLiteral("roc-toolkit-opus-master/bin/x86_64-pc-linux-gnu/roc-recv"),
                       QStringLiteral("roc-toolkit-opus-master/build/src/x86_64-pc-linux-gnu/gcc-15.2.0-release/roc-recv"),
                       QStringLiteral("roc-toolkit-opus-master/bin/roc-recv"),
                       QStringLiteral("roc-toolkit-opus-master/build/roc-recv"),
                       QStringLiteral("roc-toolkit-opus-master/roc-recv")});
}

} // namespace apping
