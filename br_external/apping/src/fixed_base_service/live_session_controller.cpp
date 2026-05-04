#include "fixed_base_service/live_session_controller.h"

#include "common/app_paths.h"
#include "common/roc_integration.h"

#include <QDebug>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcessEnvironment>

namespace apping {

namespace {

QString resolvedRocMulticastInterface(const QString& configuredInterface) {
    const QString trimmed = configuredInterface.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QHostAddress address(trimmed);
    if (!address.isNull()) {
        return trimmed;
    }

    const QNetworkInterface networkInterface = QNetworkInterface::interfaceFromName(trimmed);
    if (!networkInterface.isValid()) {
        qWarning().noquote()
            << QStringLiteral("[roc_recv] multicast interface %1 introuvable").arg(trimmed);
        return {};
    }

    for (const QNetworkAddressEntry& entry : networkInterface.addressEntries()) {
        const QHostAddress candidate = entry.ip();
        if (candidate.protocol() != QAbstractSocket::IPv4Protocol || candidate.isNull()) {
            continue;
        }

        const QString resolved = candidate.toString();
        qInfo().noquote()
            << QStringLiteral("[roc_recv] multicast interface %1 résolue en %2")
                   .arg(trimmed, resolved);
        return resolved;
    }

    qWarning().noquote()
        << QStringLiteral("[roc_recv] aucune adresse IPv4 utilisable sur %1").arg(trimmed);
    return {};
}

} // namespace

LiveSessionController::LiveSessionController(const BaseConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config) {
    m_process.setProcessChannelMode(QProcess::MergedChannels);

    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString output = QString::fromUtf8(m_process.readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) {
            qInfo().noquote() << QStringLiteral("[roc_recv %1]").arg(m_config.baseId) << output;
        }
    });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_lastError = m_process.errorString();
        m_prepared = false;
        m_active = false;
        emit stateChanged();
    });
    connect(&m_process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                if (exitStatus != QProcess::NormalExit || exitCode != 0) {
                    m_lastError = QStringLiteral("roc-recv arrêté avec code %1").arg(exitCode);
                    qWarning().noquote()
                        << QStringLiteral("[roc_recv %1] %2").arg(m_config.baseId, m_lastError);
                }
                m_prepared = false;
                m_active = false;
                emit stateChanged();
            });
}

bool LiveSessionController::prepare(QString* error) {
    if (m_process.state() != QProcess::NotRunning) {
        m_prepared = true;
        if (error) {
            error->clear();
        }
        emit stateChanged();
        return true;
    }

    const QString program = AppPaths::rocRecvBinary();
    QString configuredMulticastInterface = m_config.multicastInterface.trimmed();
    if (configuredMulticastInterface.isEmpty()) {
        configuredMulticastInterface =
            QProcessEnvironment::systemEnvironment()
                .value(QStringLiteral("APPING_ROC_MULTICAST_IFACE"),
                       QProcessEnvironment::systemEnvironment().value(
                           QStringLiteral("APPING_ROC_MIFACE")))
                .trimmed();
    }
    const QString multicastInterface = resolvedRocMulticastInterface(configuredMulticastInterface);
    const QStringList arguments =
        buildRocReceiveArguments(m_config.multicastPort,
                                 m_config.audioOutputUri,
                                 m_config.multicastAddress,
                                 multicastInterface);

    qInfo().noquote()
        << QStringLiteral("[roc_recv %1] starting %2 %3")
               .arg(m_config.baseId, program, arguments.join(QLatin1Char(' ')));

    m_process.start(program, arguments);
    if (!m_process.waitForStarted(3000)) {
        m_lastError = m_process.errorString();
        if (error) {
            *error = m_lastError;
        }
        return false;
    }

    m_lastError.clear();
    m_prepared = true;
    m_active = false;
    emit stateChanged();
    return true;
}

bool LiveSessionController::markStarted(QString* error) {
    if (!m_prepared && !prepare(error)) {
        return false;
    }
    m_active = true;
    emit stateChanged();
    return true;
}

void LiveSessionController::stop() {
    const bool hadState = m_prepared || m_active || m_process.state() != QProcess::NotRunning;
    m_prepared = false;
    m_active = false;
    if (m_process.state() != QProcess::NotRunning) {
        m_process.terminate();
        if (!m_process.waitForFinished(1000)) {
            m_process.kill();
            m_process.waitForFinished(1000);
        }
    }
    if (hadState) {
        emit stateChanged();
    }
}

QString LiveSessionController::audioState() const {
    if (m_active) {
        return QStringLiteral("live_megaphone");
    }
    if (m_prepared) {
        return QStringLiteral("megaphone_ready");
    }
    return QStringLiteral("idle");
}

QString LiveSessionController::lastError() const {
    return m_lastError;
}

} // namespace apping
