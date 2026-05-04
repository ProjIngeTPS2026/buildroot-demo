#include "portable_console/portable_config.h"
#include "portable_console/mainwindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>

using namespace apping;

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("portable-console"));
    app.setOrganizationName(QStringLiteral("apping"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Portable control console for Apping"));
    parser.addHelpOption();

    QCommandLineOption configOption(QStringList{QStringLiteral("c"), QStringLiteral("portable-config")},
                                    QStringLiteral("Path to the portable console configuration JSON"),
                                    QStringLiteral("path"),
                                    QStringLiteral("config/simulation/generated/portable-console.json"));
    parser.addOption(configOption);
    QCommandLineOption reportScreenshotsOption(QStringLiteral("report-screenshots"),
                                               QStringLiteral("Save report screenshots to a directory and exit"),
                                               QStringLiteral("directory"));
    parser.addOption(reportScreenshotsOption);
    parser.process(app);

    QString error;
    const auto config = PortableConfig::loadFromFile(parser.value(configOption), &error);
    if (!config) {
        qCritical().noquote() << QStringLiteral("Failed to load portable config: %1").arg(error);
        return 1;
    }

    MainWindow window(*config);
    window.show();
    if (parser.isSet(reportScreenshotsOption)) {
        const bool ok = window.saveReportScreenshots(parser.value(reportScreenshotsOption));
        return ok ? 0 : 2;
    }
    return app.exec();
}
