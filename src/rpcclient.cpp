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

#include "rpcclient.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

// The server's JSON parser has no boolean tokens: send 0/1 instead.
QJsonValue sanitize(const QJsonValue& v) {
    switch (v.type()) {
    case QJsonValue::Bool:
        return v.toBool() ? 1 : 0;
    case QJsonValue::Array: {
        QJsonArray out;
        for (const QJsonValue& e : v.toArray()) {
            out.append(sanitize(e));
        }
        return out;
    }
    case QJsonValue::Object: {
        QJsonObject out;
        const QJsonObject o = v.toObject();
        for (auto i = o.begin(); i != o.end(); ++i) {
            out.insert(i.key(), sanitize(i.value()));
        }
        return out;
    }
    default:
        return v;
    }
}

} // anonymous namespace

RpcClient::RpcClient(QObject *parent)
    : QObject(parent) {
    connect(&m_socket, &QTcpSocket::readyRead, this, &RpcClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::connected, this, &RpcClient::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &RpcClient::onDisconnected);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &RpcClient::onSocketError);
    m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_reconnectTimer.setInterval(2000);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_socket.state() == QAbstractSocket::UnconnectedState) {
            m_socket.connectToHost(m_host, m_port);
        }
    });
}

RpcClient::~RpcClient() {
    // the socket emits disconnected() while it is destroyed, after the
    // members declared below it (and the owner) are gone
    m_socket.disconnect(this);
    m_socket.abort();
}

void RpcClient::connectToServer(const QString& host, quint16 port) {
    m_host = host;
    m_port = port;
    m_wantConnected = true;
    m_reconnectTimer.stop();
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.abort();
    }
    m_socket.connectToHost(host, port);
}

void RpcClient::disconnectFromServer() {
    m_wantConnected = false;
    m_reconnectTimer.stop();
    m_socket.disconnectFromHost();
}

bool RpcClient::isConnected() const {
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void RpcClient::call(const QString& method, const QJsonArray& params, ResultHandler handler) {
    const qint64 id = m_nextId++;
    m_pending.insert(id, std::move(handler));
    QJsonObject msg{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params},
        {QStringLiteral("id"), id},
    };
    send(msg);
}

void RpcClient::notify(const QString& method, const QJsonArray& params) {
    QJsonObject msg{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params},
    };
    send(msg);
}

void RpcClient::send(const QJsonObject& msg) {
    if (!isConnected()) {
        return;
    }
    QByteArray data = QJsonDocument(sanitize(msg).toObject()).toJson(QJsonDocument::Compact);
    data.append('\n');
    m_socket.write(data);
}

void RpcClient::onReadyRead() {
    m_buffer.append(m_socket.readAll());
    int start = 0;
    for (;;) {
        int nl = m_buffer.indexOf('\n', start);
        if (nl < 0) {
            break;
        }
        QByteArray line = m_buffer.mid(start, nl - start).trimmed();
        start = nl + 1;
        if (!line.isEmpty()) {
            handleLine(line);
        }
    }
    m_buffer.remove(0, start);
}

void RpcClient::handleLine(const QByteArray& line) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line, &err);
    if (!doc.isObject()) {
        qWarning() << "guitariqx: bad JSON from server:" << err.errorString() << line.left(200);
        return;
    }
    const QJsonObject msg = doc.object();
    const QJsonValue method = msg.value(QStringLiteral("method"));
    if (method.isString()) {
        emit notification(method.toString(), msg.value(QStringLiteral("params")).toArray());
        return;
    }
    // the server echoes ids back as strings
    const QJsonValue idv = msg.value(QStringLiteral("id"));
    qint64 id = idv.isString() ? idv.toString().toLongLong() : idv.toInteger();
    if (msg.contains(QStringLiteral("error"))) {
        const QJsonObject e = msg.value(QStringLiteral("error")).toObject();
        const QString text = QStringLiteral("RPC error %1: %2")
            .arg(e.value(QStringLiteral("code")).toInt())
            .arg(e.value(QStringLiteral("message")).toString());
        qWarning() << "guitariqx:" << text;
        emit errorOccurred(text);
    }
    auto it = m_pending.find(id);
    if (it == m_pending.end()) {
        return;
    }
    ResultHandler handler = std::move(it.value());
    m_pending.erase(it);
    if (handler) {
        handler(msg.value(QStringLiteral("result")));
    }
}

void RpcClient::onConnected() {
    m_buffer.clear();
    emit connected();
}

void RpcClient::onDisconnected() {
    m_pending.clear();
    emit disconnected();
    if (m_autoReconnect && m_wantConnected) {
        m_reconnectTimer.start();
    }
}

void RpcClient::onSocketError(QAbstractSocket::SocketError) {
    emit errorOccurred(m_socket.errorString());
    if (m_autoReconnect && m_wantConnected && m_socket.state() == QAbstractSocket::UnconnectedState) {
        m_reconnectTimer.start();
    }
}
