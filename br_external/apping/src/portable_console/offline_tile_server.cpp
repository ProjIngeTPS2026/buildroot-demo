#include "portable_console/offline_tile_server.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>

namespace apping {

OfflineTileServer::OfflineTileServer(QObject* parent)
    : QObject(parent)
    , m_server(this) {
    m_server.setHandler([this](const HttpRequest& request) { return handleRequest(request); });
}

bool OfflineTileServer::start(const QString& tileRootPath, QString* error) {
    const QFileInfo rootInfo(tileRootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        if (error) {
            *error = QStringLiteral("Offline tile directory not found: %1").arg(tileRootPath);
        }
        return false;
    }

    m_tileRootPath = rootInfo.canonicalFilePath();
    if (m_tileRootPath.isEmpty()) {
        m_tileRootPath = rootInfo.absoluteFilePath();
    }

    if (m_server.serverPort() != 0) {
        return true;
    }

    if (!m_server.listen(0)) {
        if (error) {
            *error = QStringLiteral("Unable to listen for offline tiles");
        }
        return false;
    }

    return true;
}

QString OfflineTileServer::baseUrl() const {
    if (m_server.serverPort() == 0) {
        return {};
    }
    return QStringLiteral("http://127.0.0.1:%1/tiles/").arg(m_server.serverPort());
}

QString OfflineTileServer::tileRootPath() const {
    return m_tileRootPath;
}

HttpResponse OfflineTileServer::handleRequest(const HttpRequest& request) const {
    if (request.method != QStringLiteral("GET")) {
        return HttpResponse::text(QStringLiteral("Method not allowed"),
                                  404,
                                  QByteArrayLiteral("text/plain; charset=utf-8"));
    }

    QString relativePath = request.path();
    if (!relativePath.startsWith(QStringLiteral("/tiles/"))) {
        return HttpResponse::text(QStringLiteral("Not found"),
                                  404,
                                  QByteArrayLiteral("text/plain; charset=utf-8"));
    }

    relativePath.remove(0, QStringLiteral("/tiles/").size());
    const QString cleanRelativePath = QDir::cleanPath(relativePath);
    if (cleanRelativePath.startsWith(QStringLiteral("../"))
        || cleanRelativePath.contains(QStringLiteral("/../"))
        || cleanRelativePath == QStringLiteral("..")) {
        return HttpResponse::text(QStringLiteral("Not found"),
                                  404,
                                  QByteArrayLiteral("text/plain; charset=utf-8"));
    }

    const QString fullPath = QDir(m_tileRootPath).filePath(cleanRelativePath);
    const QFileInfo fileInfo(fullPath);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    if (!fileInfo.exists() || canonicalPath.isEmpty()
        || !canonicalPath.startsWith(m_tileRootPath + QDir::separator())) {
        return HttpResponse::text(QStringLiteral("Not found"),
                                  404,
                                  QByteArrayLiteral("text/plain; charset=utf-8"));
    }

    QFile file(canonicalPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return HttpResponse::text(QStringLiteral("Unable to read tile"),
                                  500,
                                  QByteArrayLiteral("text/plain; charset=utf-8"));
    }

    HttpResponse response;
    response.status = 200;
    response.contentType = QByteArrayLiteral("image/png");
    response.body = file.readAll();
    return response;
}

} // namespace apping
