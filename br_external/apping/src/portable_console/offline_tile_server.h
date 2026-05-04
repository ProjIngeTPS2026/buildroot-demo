#pragma once

#include "common/simple_http_server.h"

#include <QString>

namespace apping {

class OfflineTileServer : public QObject {
    Q_OBJECT

public:
    explicit OfflineTileServer(QObject* parent = nullptr);

    bool start(const QString& tileRootPath, QString* error = nullptr);
    QString baseUrl() const;
    QString tileRootPath() const;

private:
    QString m_tileRootPath;
    SimpleHttpServer m_server;

    HttpResponse handleRequest(const HttpRequest& request) const;
};

} // namespace apping
