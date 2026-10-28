/*
 * Copyright (C) 2026 Gaël de Chalendar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QJsonValue>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <functional>

/*
 * JSON-RPC 2.0 client for the guitarix engine server
 * (src/gx_head/engine/jsonrpc.cpp).
 *
 * Messages are newline separated JSON objects. Requests with an id get a
 * response; requests without one are notifications (the method list in
 * jsonrpc_methods.gperf_tmpl marks which methods return a result). The
 * server sends notifications of its own ("set", "preset_changed",
 * "rack_units_changed", ...) after a "listen" call.
 */
class RpcClient : public QObject {
    Q_OBJECT
public:
    using ResultHandler = std::function<void(const QJsonValue& result)>;

    explicit RpcClient(QObject *parent = nullptr);
    ~RpcClient() override;

    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    QString host() const { return m_host; }
    QHostAddress peerAddress() const { return m_socket.peerAddress(); }
    quint16 port() const { return m_port; }
    void setAutoReconnect(bool on) { m_autoReconnect = on; }

    // request with result; the handler runs when the response arrives
    void call(const QString& method, const QJsonArray& params, ResultHandler handler);
    // request without result
    void notify(const QString& method, const QJsonArray& params = QJsonArray());

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    void notification(const QString& method, const QJsonArray& params);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void send(const QJsonObject& msg);
    void handleLine(const QByteArray& line);

    QTcpSocket m_socket;
    QTimer m_reconnectTimer;
    QByteArray m_buffer;
    QHash<qint64, ResultHandler> m_pending;
    qint64 m_nextId = 1;
    QString m_host;
    quint16 m_port = 0;
    bool m_autoReconnect = true;
    bool m_wantConnected = false;
};
