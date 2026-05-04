#include "common/simple_http_server.h"

#include <QHostAddress>
#include <QJsonDocument>

namespace apping {

namespace {

QByteArray reasonPhrase(int status) {
    switch (status) {
    case 200:
        return QByteArrayLiteral("OK");
    case 201:
        return QByteArrayLiteral("Created");
    case 400:
        return QByteArrayLiteral("Bad Request");
    case 404:
        return QByteArrayLiteral("Not Found");
    case 409:
        return QByteArrayLiteral("Conflict");
    case 500:
        return QByteArrayLiteral("Internal Server Error");
    case 503:
        return QByteArrayLiteral("Service Unavailable");
    default:
        return QByteArrayLiteral("Unknown");
    }
}

} // namespace

QString HttpRequest::path() const {
    return url.path();
}

QByteArray HttpRequest::header(const QString& key) const {
    return headers.value(key.toLower());
}

QJsonObject HttpRequest::jsonBody(bool* ok) const {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    if (ok) {
        *ok = error.error == QJsonParseError::NoError && document.isObject();
    }
    return document.object();
}

HttpResponse HttpResponse::json(const QJsonObject& object, int statusCode) {
    return jsonDocument(QJsonDocument(object), statusCode);
}

HttpResponse HttpResponse::jsonDocument(const QJsonDocument& document, int statusCode) {
    HttpResponse response;
    response.status = statusCode;
    response.body = document.toJson(QJsonDocument::Compact);
    return response;
}

HttpResponse HttpResponse::text(const QString& message,
                                int statusCode,
                                const QByteArray& type) {
    HttpResponse response;
    response.status = statusCode;
    response.contentType = type;
    response.body = message.toUtf8();
    return response;
}

SimpleHttpServer::SimpleHttpServer(QObject* parent)
    : QObject(parent) {
    connect(&m_server, &QTcpServer::newConnection, this, &SimpleHttpServer::onNewConnection);
}

bool SimpleHttpServer::listen(quint16 port) {
    if (!m_server.listen(QHostAddress::AnyIPv4, port)) {
        emit errorOccurred(m_server.errorString());
        return false;
    }
    return true;
}

quint16 SimpleHttpServer::serverPort() const {
    return m_server.serverPort();
}

void SimpleHttpServer::setHandler(std::function<HttpResponse(const HttpRequest&)> handler) {
    m_handler = std::move(handler);
}

void SimpleHttpServer::onNewConnection() {
    while (QTcpSocket* socket = m_server.nextPendingConnection()) {
        m_connections.insert(socket, ConnectionState{});
        connect(socket, &QTcpSocket::readyRead, this,
                [this, socket]() { onSocketReadyRead(socket); });
        connect(socket, &QTcpSocket::disconnected, this,
                [this, socket]() { onSocketDisconnected(socket); });
    }
}

void SimpleHttpServer::onSocketReadyRead(QTcpSocket* socket) {
    if (!m_connections.contains(socket)) {
        return;
    }

    ConnectionState& state = m_connections[socket];
    state.buffer += socket->readAll();
    if (state.handled) {
        return;
    }

    const int headerEnd = state.buffer.indexOf(QByteArrayLiteral("\r\n\r\n"));
    if (headerEnd < 0) {
        return;
    }

    const QList<QByteArray> headerLines =
        state.buffer.left(headerEnd).split('\n');
    if (headerLines.isEmpty()) {
        writeResponse(socket, HttpResponse::text(QStringLiteral("Malformed request"), 400));
        state.handled = true;
        return;
    }

    QByteArray requestLine = headerLines.first().trimmed();
    const QList<QByteArray> requestParts = requestLine.split(' ');
    if (requestParts.size() < 2) {
        writeResponse(socket, HttpResponse::text(QStringLiteral("Malformed request"), 400));
        state.handled = true;
        return;
    }

    HttpRequest request;
    request.method = QString::fromUtf8(requestParts.at(0));
    request.url = QUrl::fromEncoded(QByteArrayLiteral("http://localhost")
                                    + requestParts.at(1));

    for (int index = 1; index < headerLines.size(); ++index) {
        const QByteArray line = headerLines.at(index).trimmed();
        const int separator = line.indexOf(':');
        if (separator <= 0) {
            continue;
        }
        const QString key = QString::fromUtf8(line.left(separator)).toLower();
        const QByteArray value = line.mid(separator + 1).trimmed();
        request.headers.insert(key, value);
    }

    if (state.contentLength < 0) {
        state.contentLength = request.header(QStringLiteral("content-length")).toLongLong();
        if (state.contentLength < 0) {
            state.contentLength = 0;
        }
    }

    const qint64 totalLength = static_cast<qint64>(headerEnd) + 4 + state.contentLength;
    if (state.buffer.size() < totalLength) {
        return;
    }

    request.body = state.buffer.mid(headerEnd + 4, state.contentLength);

    const HttpResponse response =
        m_handler ? m_handler(request)
                  : HttpResponse::text(QStringLiteral("No handler registered"), 503);

    writeResponse(socket, response);
    state.handled = true;
}

void SimpleHttpServer::onSocketDisconnected(QTcpSocket* socket) {
    m_connections.remove(socket);
    socket->deleteLater();
}

void SimpleHttpServer::writeResponse(QTcpSocket* socket, const HttpResponse& response) {
    QByteArray raw;
    raw += QByteArrayLiteral("HTTP/1.1 ");
    raw += QByteArray::number(response.status);
    raw += QByteArrayLiteral(" ");
    raw += reasonPhrase(response.status);
    raw += QByteArrayLiteral("\r\nConnection: close\r\nContent-Type: ");
    raw += response.contentType;
    raw += QByteArrayLiteral("\r\nContent-Length: ");
    raw += QByteArray::number(response.body.size());
    raw += QByteArrayLiteral("\r\n\r\n");
    raw += response.body;

    socket->write(raw);
    socket->disconnectFromHost();
}

} // namespace apping
