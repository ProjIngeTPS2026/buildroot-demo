#include "common/app_paths.h"
#include "fixed_base_service/base_config.h"
#include "fixed_base_service/base_service.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

using namespace apping;

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("fixed-base-service"));
    app.setOrganizationName(QStringLiteral("apping"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Fixed base service for Apping field audio nodes"));
    parser.addHelpOption();

    QCommandLineOption configOption(QStringList{QStringLiteral("c"), QStringLiteral("config")},
                                    QStringLiteral("Path to the base configuration JSON"),
                                    QStringLiteral("path"),
                                    QStringLiteral("config/simulation/generated/base-alpha.json"));
    parser.addOption(configOption);
    parser.process(app);

    QString error;
    const auto config = BaseConfig::loadFromFile(parser.value(configOption), &error);
    if (!config) {
        qCritical().noquote() << QStringLiteral("Failed to load base config: %1").arg(error);
        return 1;
    }

    BaseService service(*config);
    if (!service.start(&error)) {
        qCritical().noquote() << QStringLiteral("Failed to start base service: %1").arg(error);
        return 1;
    }

    qInfo().noquote() << QStringLiteral("Base %1 listening on %2")
                             .arg(config->baseId)
                             .arg(config->controlPort);

    return app.exec();
}
