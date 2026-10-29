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

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <QTcpServer>

class QTcpSocket;

/*
 * A guitarix engine for the tests: a JSON-RPC server answering with the
 * results recorded from a real engine (fixtures/engine.json, see
 * fixtures/capture.py), and keeping the state the client changes (current
 * preset, banks, rack, parameter values, MIDI map).
 *
 * It records the requests that would crash or mislead the real engine in
 * violations(): JSON booleans, unknown methods, parameter ids, plugin ids or
 * bank names where the engine does not check them, and the MIDI calls only
 * valid in config mode. Tests check that this list stays empty.
 */
class FakeEngine : public QObject {
    Q_OBJECT
public:
    // interfaceMinor: 1 like guitarix 0.47.0, 2 with list_directory and
    // get_file_places
    explicit FakeEngine(int interfaceMinor = 2, QObject *parent = nullptr);
    ~FakeEngine() override;
    void setInterfaceMajor(int major) { m_major = major; }

    bool listen(quint16 port = 0);
    quint16 port() const { return m_server.serverPort(); }
    void close();
    // drop the connections, the server keeps listening
    void disconnectClients();
    int clientCount() const { return m_clients.size(); }

    struct Message {
        QString method;
        QJsonArray params;
        bool isCall;
    };
    QList<Message> received() const { return m_received; }
    // received messages with this method
    QList<Message> received(const QString& method) const;
    void clearReceived() { m_received.clear(); }
    QStringList violations() const { return m_violations; }

    // send a notification to all clients that called "listen"
    void notifyClients(const QString& method, const QJsonArray& params);

    QJsonValue value(const QString& id) const { return m_values.value(id); }
    QJsonArray banks() const { return m_banks; }
    QStringList rack(bool stereo) const { return m_rack[stereo ? 1 : 0]; }
    bool midiConfigMode() const { return m_midiConfig; }
    bool midiFeedback() const { return m_midiFeedback; }
    bool switcherActive() const { return m_switcher; }
    // the switcher ends as when a note was played: tuner selected or not
    // (TunerSwitcher::deactivate)
    void finishSwitcher(bool tuner);
    // files listed by list_directory, path -> entries [name, is_dir, size, mtime]
    void setDirectory(const QString& path, const QJsonArray& entries) { m_dirs.insert(path, entries); }
    // unit presets: name -> parameter values; graphiceq has two factory
    // presets
    using UnitPresets = QList<QPair<QString, QJsonObject>>;
    UnitPresets unitPresets(const QString& unit, bool factory) const { return m_unitPresets[factory ? 1 : 0].value(unit); }
    void setUnitPresets(const QString& unit, bool factory, const UnitPresets& presets) {
        m_unitPresets[factory ? 1 : 0].insert(unit, presets);
    }

signals:
    void messageReceived(const QString& method);

private:
    void onNewConnection();
    void onReadyRead(QTcpSocket *s);
    void handle(QTcpSocket *s, const QJsonObject& msg);
    QJsonValue dispatch(QTcpSocket *s, const QString& method, const QJsonArray& params, bool& known);
    void send(QTcpSocket *s, const QJsonObject& msg);
    void violation(const QString& text);
    void checkNoBool(const QJsonValue& v, const QString& where);
    bool checkParam(const QString& method, const QJsonValue& id);
    bool checkBank(const QString& method, const QJsonValue& name);
    bool checkPlugin(const QString& method, const QString& id);
    QJsonObject unitValues(const QString& unit) const;
    int bankIndex(const QString& name) const;
    QStringList presets(const QString& bank) const;
    void setPresets(const QString& bank, const QStringList& presets);
    QString uniqueBankName(const QString& name) const;
    QJsonValue paramValue(const QString& id) const;
    QJsonArray oscilloscopeInfo() const;
    void checkSpecialValue(const QString& id, const QJsonValue& v);
    void loadParameterValues(const QJsonArray& list);

    QTcpServer m_server;
    QList<QTcpSocket*> m_clients;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    QList<QTcpSocket*> m_listeners;
    int m_major = 1;
    int m_minor;
    QJsonObject m_fixture;
    QHash<QString, QJsonValue> m_values;
    QHash<QString, QJsonObject> m_paramEntries;
    // parameterlist type tag of each parameter: Float, Bool, JConv, Osc...
    QHash<QString, QString> m_tags;
    int m_oscMul = 1;
    QHash<QString, QJsonArray> m_dirs;
    QSet<QString> m_plugins;
    // user (0) and factory (1) unit presets by unit
    QHash<QString, UnitPresets> m_unitPresets[2];
    QJsonArray m_banks;
    QStringList m_rack[2];
    QJsonArray m_midiMap;
    bool m_midiConfig = false;
    int m_midiControl = -1;
    bool m_midiFeedback = false;
    void setState(const QString& state);
    bool m_switcher = false;
    QString m_switcherOldState;
    bool m_switcherOldTuner = false;
    QList<Message> m_received;
    QStringList m_violations;
};
