#include "common/app_paths.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QDebug>

using namespace apping;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("sim-launcher"));
    app.setOrganizationName(QStringLiteral("apping"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Launches a local multi-base simulation"));
    parser.addHelpOption();

    QCommandLineOption portableConfigOption(QStringList{QStringLiteral("p"), QStringLiteral("portable-config")},
                                            QStringLiteral("Portable console configuration"),
                                            QStringLiteral("path"),
                                            QStringLiteral("config/simulation/generated/portable-console.json"));
    QCommandLineOption baseDirOption(QStringList{QStringLiteral("b"), QStringLiteral("base-dir")},
                                     QStringLiteral("Directory containing base-*.json"),
                                     QStringLiteral("path"),
                                     QStringLiteral("config/simulation/generated"));
    parser.addOption(portableConfigOption);
    parser.addOption(baseDirOption);
    parser.process(app);

    const QString baseDirPath = AppPaths::resolvePath(parser.value(baseDirOption));
    QDir baseDir(baseDirPath);
    const QStringList baseConfigs =
        baseDir.entryList({QStringLiteral("base-*.json")}, QDir::Files, QDir::Name);

    if (baseConfigs.isEmpty()) {
        qCritical().noquote() << QStringLiteral("No base configs found in %1").arg(baseDirPath);
        return 1;
    }

    const QString baseBinary = AppPaths::findBinary(
        QStringLiteral("fixed-base-service"),
        {QStringLiteral("build/fixed-base-service"),
         QStringLiteral("fixed-base-service")});
    const QString portableBinary = AppPaths::findBinary(
        QStringLiteral("portable-console"),
        {QStringLiteral("build/portable-console"),
         QStringLiteral("portable-console")});

    if (baseBinary.isEmpty() || portableBinary.isEmpty()) {
        qCritical().noquote() << QStringLiteral("Unable to resolve one or more application binaries");
        return 1;
    }

    for (const QString& configFile : baseConfigs) {
        const QString absoluteConfigPath = baseDir.filePath(configFile);
        if (!QProcess::startDetached(baseBinary,
                                     {QStringLiteral("--config"), absoluteConfigPath},
                                     AppPaths::rootDirectory())) {
            qCritical().noquote() << QStringLiteral("Failed to start base for %1").arg(configFile);
            return 1;
        }
        qInfo().noquote() << QStringLiteral("Started base with %1").arg(configFile);
    }

    const QString portableConfigPath = AppPaths::resolvePath(parser.value(portableConfigOption));
    if (!QProcess::startDetached(portableBinary,
                                 {QStringLiteral("--portable-config"), portableConfigPath},
                                 AppPaths::rootDirectory())) {
        qCritical().noquote() << QStringLiteral("Failed to start portable console");
        return 1;
    }

    qInfo().noquote() << QStringLiteral("Started portable console with %1").arg(portableConfigPath);
    return 0;
}
