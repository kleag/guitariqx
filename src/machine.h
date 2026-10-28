/*
 * Copyright (C) 2026 Guitarix developers
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

#include "parameter.h"
#include "rpcclient.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

/*
 * Engine proxy for the QML UI: the qt_head counterpart of
 * GxMachineRemote in src/gx_head/gui/machine.cpp.
 *
 * Exposed to QML as the singleton "Gx".
 */
class Machine : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Gx)
    QML_SINGLETON
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString host READ host NOTIFY connectedChanged)
    Q_PROPERTY(int port READ port NOTIFY connectedChanged)
    Q_PROPERTY(QString serverVersion READ serverVersion NOTIFY readyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString engineState READ engineState NOTIFY engineStateChanged)
    Q_PROPERTY(double cpuLoad READ cpuLoad NOTIFY cpuLoadChanged)
    Q_PROPERTY(QVariantList banks READ banks NOTIFY banksChanged)
    Q_PROPERTY(QString currentBank READ currentBank NOTIFY presetChanged)
    Q_PROPERTY(QString currentPreset READ currentPreset NOTIFY presetChanged)
    Q_PROPERTY(QStringList monoRack READ monoRack NOTIFY rackChanged)
    Q_PROPERTY(QStringList stereoRack READ stereoRack NOTIFY rackChanged)
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY messageReceived)
public:
    // no default argument: a default constructible class would be
    // instantiated by the QML engine instead of calling create()
    explicit Machine(QObject *parent);
    ~Machine() override;

    // used by the QML engine to get the instance created in main()
    static Machine *create(QQmlEngine *, QJSEngine *);
    static void setInstance(Machine *m) { s_instance = m; }

    bool connected() const { return m_rpc.isConnected(); }
    bool ready() const { return m_ready; }
    QString host() const { return m_rpc.host(); }
    int port() const { return m_rpc.port(); }
    QString serverVersion() const { return m_serverVersion; }
    QString status() const { return m_status; }
    QString engineState() const { return m_engineState; }
    double cpuLoad() const { return m_cpuLoad; }
    QVariantList banks() const { return m_banks; }
    QString currentBank() const { return m_currentBank; }
    QString currentPreset() const { return m_currentPreset; }
    QStringList monoRack() const { return m_rack[0]; }
    QStringList stereoRack() const { return m_rack[1]; }
    QVariantList plugins() const { return m_pluginList; }
    QString lastMessage() const { return m_lastMessage; }
    int parameterCount() const { return m_params.size(); }

    Q_INVOKABLE void connectTo(const QString& host, int port);

    Q_INVOKABLE Parameter *param(const QString& id) const;
    Q_INVOKABLE bool hasParam(const QString& id) const { return m_params.contains(id); }
    Q_INVOKABLE QVariantMap pluginInfo(const QString& id) const;

    // layout of a rack unit (see uitree.h); empty until unitUiReady(id)
    Q_INVOKABLE QVariantMap unitUi(const QString& id);

    Q_INVOKABLE void setPreset(const QString& bank, const QString& preset);
    Q_INVOKABLE void stepPreset(int delta);
    Q_INVOKABLE void setEngineState(const QString& state);

    Q_INVOKABLE void insertUnit(const QString& id, const QString& before, bool stereo);
    Q_INVOKABLE void removeUnit(const QString& id, bool stereo);
    Q_INVOKABLE void moveUnit(const QString& id, int delta, bool stereo);

    // output parameters (meters) are polled while registered
    Q_INVOKABLE void watchOutput(const QString& id);
    Q_INVOKABLE void unwatchOutput(const QString& id);

signals:
    void connectedChanged();
    void readyChanged();
    void statusChanged();
    void engineStateChanged();
    void cpuLoadChanged();
    void banksChanged();
    void presetChanged();
    void rackChanged();
    void pluginsChanged();
    void unitUiReady(const QString& id);
    void messageReceived(const QString& level, const QString& text);

private:
    void onConnected();
    void onDisconnected();
    void onNotification(const QString& method, const QJsonArray& params);
    void onParameterChangeRequested(Parameter *p);
    void setStatus(const QString& s);
    void loadParameters(const QJsonArray& list);
    void loadPlugins(const QJsonArray& list);
    void requestBanks();
    void requestRack();
    void pollOutputs();
    QList<Parameter*> unitParameters(const QString& id) const;

    static Machine *s_instance;

    RpcClient m_rpc;
    QHash<QString, Parameter*> m_params;
    QHash<QString, QVariantMap> m_pluginInfo;
    QVariantList m_pluginList;
    QHash<QString, QVariantMap> m_unitUi;
    QSet<QString> m_unitUiPending;
    QHash<QString, int> m_watched;
    QTimer m_pollTimer;
    QTimer m_cpuTimer;
    QTimer m_rackTimer;
    QVariantList m_banks;
    QStringList m_rack[2];
    QString m_currentBank;
    QString m_currentPreset;
    QString m_engineState;
    QString m_serverVersion;
    QString m_status;
    QString m_lastMessage;
    double m_cpuLoad = 0;
    bool m_ready = false;
};
