#pragma once

#include "fixed_base_service/base_config.h"

#include <QObject>
#include <QProcess>

namespace apping {

class LiveSessionController : public QObject {
    Q_OBJECT

public:
    explicit LiveSessionController(const BaseConfig& config, QObject* parent = nullptr);

    bool prepare(QString* error = nullptr);
    bool markStarted(QString* error = nullptr);
    void stop();
    QString audioState() const;
    QString lastError() const;

signals:
    void stateChanged();

private:
    BaseConfig m_config;
    QProcess m_process;
    bool m_prepared = false;
    bool m_active = false;
    QString m_lastError;
};

} // namespace apping
