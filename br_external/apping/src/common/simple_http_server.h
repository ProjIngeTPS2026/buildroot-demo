#pragma once

#include <functional>

#include <QByteArray>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>

namespace apping {

struct HttpRequest {
    QString method;
    QUrl url;
    QMap<QString, QByteArray> headers;
    QByteArray body;

    QString path() const;
    QByteArray header(const QString& key) const;
    QJsonObject jsonBody(bool* ok = nullptr) const;
};

struct HttpResponse {
    int status = 200;
    QByteArray contentType = QByteArrayLiteral("application/json; charset=utf-8");
    QByteArray body;

    static HttpResponse json(const QJsonObject& object, int status = 200);
    static HttpResponse jsonDocument(const QJsonDocument& document, int status = 200);
    static HttpResponse text(const QString& message,
                             int status = 200,
                             const QByteArray& contentType =
                                 QByteArrayLiteral("text/plain; charset=utf-8"));
};

class SimpleHttpServer : public QObject {
    Q_OBJECT

public:
    explicit SimpleHttpServer(QObject* parent = nullptr);

    bool listen(quint16 port);
    quint16 serverPort() const;
    void setHandler(std::function<HttpResponse(const HttpRequest&)> handler);

signals:
    void errorOccurred(const QString& message);

private:
    struct ConnectionState {
        QByteArray buffer;
        qint64 contentLength = -1;
        bool handled = false;
    };

    QTcpServer m_server;
    std::function<HttpResponse(const HttpRequest&)> m_handler;
    QHash<QTcpSocket*, ConnectionState> m_connections;

    void onNewConnection();
    void onSocketReadyRead(QTcpSocket* socket);
    void onSocketDisconnected(QTcpSocket* socket);
    void writeResponse(QTcpSocket* socket, const HttpResponse& response);
};

} // namespace apping
