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

#include <QDeadlineTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

// RpcClient against a bare QTcpServer, to control the framing byte by byte
class TestRpcClient : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void requestFormat();
    void booleansSentAsNumbers();
    void responseWithStringId();
    void responsesOutOfOrder();
    void splitAndCoalescedLines();
    void notification();
    void errorResponse();
    void pendingDroppedOnDisconnect();
    void reconnects();

private:
    QJsonObject readMessage();
    void write(const QByteArray& data);

    QTcpServer *m_server = nullptr;
    QTcpSocket *m_peer = nullptr;
    RpcClient *m_client = nullptr;
    QByteArray m_buffer;
};

void TestRpcClient::init() {
    m_server = new QTcpServer(this);
    QVERIFY(m_server->listen(QHostAddress::LocalHost));
    m_client = new RpcClient(this);
    QSignalSpy connected(m_client, &RpcClient::connected);
    m_client->connectToServer(QStringLiteral("127.0.0.1"), m_server->serverPort());
    QTRY_VERIFY(m_server->hasPendingConnections());
    m_peer = m_server->nextPendingConnection();
    QTRY_COMPARE(connected.count(), 1);
    m_buffer.clear();
}

void TestRpcClient::cleanup() {
    delete m_client;
    m_client = nullptr;
    delete m_server;
    m_server = nullptr;
    m_peer = nullptr;
}

// the client writes from the event loop: no blocking waits here
QJsonObject TestRpcClient::readMessage() {
    QDeadlineTimer deadline(5000);
    while (!m_buffer.contains('\n') && !deadline.hasExpired()) {
        QTest::qWait(5);
        m_buffer.append(m_peer->readAll());
    }
    if (!m_buffer.contains('\n')) {
        return QJsonObject();
    }
    const int nl = m_buffer.indexOf('\n');
    const QByteArray line = m_buffer.left(nl);
    m_buffer.remove(0, nl + 1);
    return QJsonDocument::fromJson(line).object();
}

void TestRpcClient::write(const QByteArray& data) {
    m_peer->write(data);
    m_peer->flush();
}

void TestRpcClient::requestFormat() {
    m_client->call(QStringLiteral("getversion"), {}, [](const QJsonValue&) {});
    m_client->notify(QStringLiteral("listen"), {QStringLiteral("preset")});
    const QJsonObject call = readMessage();
    QCOMPARE(call.value(QStringLiteral("jsonrpc")).toString(), QStringLiteral("2.0"));
    QCOMPARE(call.value(QStringLiteral("method")).toString(), QStringLiteral("getversion"));
    QVERIFY(call.contains(QStringLiteral("id")));
    const QJsonObject notification = readMessage();
    QCOMPARE(notification.value(QStringLiteral("method")).toString(), QStringLiteral("listen"));
    // notifications have no id: the engine does not answer them
    QVERIFY(!notification.contains(QStringLiteral("id")));
}

void TestRpcClient::booleansSentAsNumbers() {
    m_client->notify(QStringLiteral("set"), {QStringLiteral("a.on_off"), true,
                                             QStringLiteral("b"), QJsonObject{{QStringLiteral("x"), false}}});
    QTRY_VERIFY(m_peer->bytesAvailable() > 0);
    QTest::qWait(20);
    m_buffer.append(m_peer->readAll());
    QVERIFY(!m_buffer.contains("true"));
    QVERIFY(!m_buffer.contains("false"));
    const QJsonArray params = readMessage().value(QStringLiteral("params")).toArray();
    QCOMPARE(params.at(1), QJsonValue(1));
    QCOMPARE(params.at(3).toObject().value(QStringLiteral("x")), QJsonValue(0));
}

void TestRpcClient::responseWithStringId() {
    QJsonValue result;
    m_client->call(QStringLiteral("getstate"), {}, [&result](const QJsonValue& r) { result = r; });
    const QJsonObject req = readMessage();
    const QByteArray id = QByteArray::number(req.value(QStringLiteral("id")).toInteger());
    // the engine sends the id back as a string
    write("{\"jsonrpc\":\"2.0\",\"id\":\"" + id + "\",\"result\":\"running\"}\n");
    QTRY_COMPARE(result.toString(), QStringLiteral("running"));
}

void TestRpcClient::responsesOutOfOrder() {
    QStringList order;
    m_client->call(QStringLiteral("a"), {}, [&order](const QJsonValue& r) { order.append(r.toString()); });
    m_client->call(QStringLiteral("b"), {}, [&order](const QJsonValue& r) { order.append(r.toString()); });
    const qint64 ida = readMessage().value(QStringLiteral("id")).toInteger();
    const qint64 idb = readMessage().value(QStringLiteral("id")).toInteger();
    QVERIFY(ida != idb);
    write(QStringLiteral("{\"id\":%1,\"result\":\"b\"}\n{\"id\":%2,\"result\":\"a\"}\n").arg(idb).arg(ida).toUtf8());
    QTRY_COMPARE(order, (QStringList{QStringLiteral("b"), QStringLiteral("a")}));
}

void TestRpcClient::splitAndCoalescedLines() {
    QSignalSpy spy(m_client, &RpcClient::notification);
    const QByteArray data = "{\"method\":\"state_changed\",\"params\":[\"running\"]}\n"
                            "{\"method\":\"preset_changed\",\"params\":[\"Bank\",\"Pre\"]}\n";
    // byte by byte, then both at once
    for (char c : data) {
        write(QByteArray(1, c));
        QCoreApplication::processEvents();
    }
    QTRY_COMPARE(spy.count(), 2);
    write(data);
    QTRY_COMPARE(spy.count(), 4);
    QCOMPARE(spy.at(1).at(0).toString(), QStringLiteral("preset_changed"));
    QCOMPARE(spy.at(1).at(1).toJsonArray().at(1).toString(), QStringLiteral("Pre"));
}

void TestRpcClient::notification() {
    QSignalSpy spy(m_client, &RpcClient::notification);
    write("{\"jsonrpc\":\"2.0\",\"method\":\"set\",\"params\":[\"a\",1,\"b\",2]}\n");
    QTRY_COMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("set"));
    QCOMPARE(spy.at(0).at(1).toJsonArray().size(), 4);
}

void TestRpcClient::errorResponse() {
    QSignalSpy errors(m_client, &RpcClient::errorOccurred);
    bool called = false;
    m_client->call(QStringLiteral("nope"), {}, [&called](const QJsonValue&) { called = true; });
    const qint64 id = readMessage().value(QStringLiteral("id")).toInteger();
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("RPC error -32601")));
    write(QStringLiteral("{\"id\":\"%1\",\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}\n").arg(id).toUtf8());
    QTRY_COMPARE(errors.count(), 1);
    // the handler still runs, with an undefined result
    QVERIFY(called);
}

void TestRpcClient::pendingDroppedOnDisconnect() {
    bool called = false;
    QSignalSpy disconnected(m_client, &RpcClient::disconnected);
    m_client->setAutoReconnect(false);
    m_client->call(QStringLiteral("getstate"), {}, [&called](const QJsonValue&) { called = true; });
    readMessage();
    m_peer->disconnectFromHost();
    QTRY_COMPARE(disconnected.count(), 1);
    QVERIFY(!m_client->isConnected());
    QVERIFY(!called);
}

void TestRpcClient::reconnects() {
    QSignalSpy connected(m_client, &RpcClient::connected);
    m_peer->disconnectFromHost();
    // the client retries every 2 s
    QTRY_VERIFY_WITH_TIMEOUT(m_server->hasPendingConnections(), 10000);
    m_peer = m_server->nextPendingConnection();
    QTRY_COMPARE_WITH_TIMEOUT(connected.count(), 1, 5000);
    QVERIFY(m_client->isConnected());
}

QTEST_GUILESS_MAIN(TestRpcClient)
#include "tst_rpcclient.moc"
