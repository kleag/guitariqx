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

#include "machine.h"
#include "uitree.h"

#include <QDebug>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QQmlEngine>
#include <algorithm>
#include <cmath>

namespace {

// JSON-RPC interface version this client is written against
// (InterfaceVersionMajor in jsonrpc.cpp)
const int interfaceMajor = 1;

// UiBuilder form flags (gx_plugin.h)
const int UI_FORM_STACK = 0x01;
const int UI_FORM_GLADE = 0x02;

// PluginDef::flags (gx_plugin.h)
const int PGN_STEREO = 0x0001;
const int PGN_GUI = 0x0008;
const int PGN_FIXED_GUI = 0x0800;

// notification classes requested with "listen" (token_range in
// jsonrpc.cpp); "display" and "tuner" are only used by the tuner
// switcher (MIDI controlled tuner display), which is not supported yet
const char *listenTokens[] = {
    "preset", "state", "param", "plugins_changed", "presetlist_changed",
    "logger", "units_changed", "misc", "freq", "midi",
};

} // anonymous namespace

Machine *Machine::s_instance = nullptr;

Machine::Machine(QObject *parent)
    : QObject(parent),
      m_tuner(*this, m_rpc),
      m_midi(*this, m_rpc) {
    QQmlEngine::setObjectOwnership(&m_tuner, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&m_midi, QQmlEngine::CppOwnership);
    connect(&m_rpc, &RpcClient::connected, this, &Machine::onConnected);
    connect(&m_rpc, &RpcClient::disconnected, this, &Machine::onDisconnected);
    connect(&m_rpc, &RpcClient::notification, this, &Machine::onNotification);
    connect(&m_rpc, &RpcClient::errorOccurred, this, [this](const QString& msg) {
        if (!m_rpc.isConnected()) {
            setStatus(tr("Cannot connect to %1:%2 (%3)").arg(m_rpc.host()).arg(m_rpc.port()).arg(msg));
        }
    });
    // same period as GxMachineBase::set_update_parameter
    m_pollTimer.setInterval(60);
    connect(&m_pollTimer, &QTimer::timeout, this, &Machine::pollOutputs);
    m_cpuTimer.setInterval(1000);
    connect(&m_cpuTimer, &QTimer::timeout, this, [this]() {
        m_rpc.call(QStringLiteral("jack_cpu_load"), {}, [this](const QJsonValue& r) {
            m_cpuLoad = r.toDouble();
            emit cpuLoadChanged();
        });
    });
    // the rack order is derived from ui.* and *.position parameters,
    // which change in bursts (preset load)
    m_rackTimer.setInterval(50);
    m_rackTimer.setSingleShot(true);
    connect(&m_rackTimer, &QTimer::timeout, this, &Machine::requestRack);
}

Machine::~Machine() {
    m_rpc.disconnect(this);
}

Machine *Machine::create(QQmlEngine *, QJSEngine *) {
    Q_ASSERT(s_instance);
    QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
    return s_instance;
}

void Machine::setStatus(const QString& s) {
    if (s == m_status) {
        return;
    }
    m_status = s;
    emit statusChanged();
}

void Machine::connectTo(const QString& host, int port) {
    setStatus(tr("Connecting to %1:%2 ...").arg(host).arg(port));
    m_rpc.connectToServer(host, quint16(port));
}

void Machine::onConnected() {
    // the engine's files can be read directly when it runs on this computer
    const QHostAddress peer = m_rpc.peerAddress();
    m_engineLocal = peer.isLoopback() || QNetworkInterface::allAddresses().contains(peer);
    // testing the remote file listing with a local engine
    if (qEnvironmentVariableIntValue("GUITARIQX_REMOTE_FILES")) {
        m_engineLocal = false;
    }
    emit connectedChanged();
    setStatus(tr("Connected, loading ..."));
    m_rpc.call(QStringLiteral("getversion"), {}, [this](const QJsonValue& r) {
        const QJsonArray v = r.toArray();
        if (v.size() >= 3) {
            m_serverVersion = v.at(2).toString();
        }
        m_interfaceMinor = v.at(1).toInt();
        if (v.isEmpty() || v.at(0).toInt() != interfaceMajor) {
            setStatus(tr("Unsupported engine interface version %1").arg(v.at(0).toInt()));
            m_rpc.disconnectFromServer();
            return;
        }
        for (const char *t : listenTokens) {
            m_rpc.notify(QStringLiteral("listen"), {QString::fromLatin1(t)});
        }
        m_rpc.call(QStringLiteral("parameterlist"), {}, [this](const QJsonValue& r) {
            loadParameters(r.toArray());
            m_rpc.call(QStringLiteral("pluginlist"), {}, [this](const QJsonValue& r) {
                loadPlugins(r.toArray());
                requestBanks();
                requestRack();
                // the server answers in order: banks and rack are known
                // when this result arrives
                m_rpc.call(QStringLiteral("getstate"), {}, [this](const QJsonValue& r) {
                    m_engineState = r.toString();
                    emit engineStateChanged();
                    m_ready = true;
                    m_tuner.engineReady();
                    m_midi.engineReady();
                    requestFilePlaces();
                    emit readyChanged();
                    setStatus(tr("Connected to guitarix %1 on %2:%3")
                              .arg(m_serverVersion, m_rpc.host()).arg(m_rpc.port()));
                    m_cpuTimer.start();
                    if (!m_watched.isEmpty()) {
                        m_pollTimer.start();
                    }
                });
            });
        });
    });
}

void Machine::onDisconnected() {
    m_pollTimer.stop();
    m_cpuTimer.stop();
    // requests in flight are lost
    m_unitUiPending.clear();
    m_editQueue.clear();
    m_editRunning = false;
    m_tuner.engineLost();
    m_midi.engineLost();
    if (m_ready) {
        m_ready = false;
        emit readyChanged();
    }
    emit connectedChanged();
    setStatus(tr("Disconnected from %1:%2, retrying ...").arg(m_rpc.host()).arg(m_rpc.port()));
}

void Machine::loadParameters(const QJsonArray& list) {
    // keep existing objects so QML bindings survive a reconnect
    QSet<QString> seen;
    for (int i = 0; i + 1 < list.size(); i += 2) {
        const QString tag = list.at(i).toString();
        const QJsonObject entry = list.at(i + 1).toObject();
        Parameter *np = Parameter::fromJson(tag, entry, this);
        if (!np) {
            continue;
        }
        seen.insert(np->id());
        auto it = m_params.find(np->id());
        if (it != m_params.end()) {
            Parameter *old = it.value();
            if (old->kind() == np->kind()) {
                old->updateFromEngine(np->toJson());
                delete np;
                continue;
            }
            old->deleteLater();
        }
        QQmlEngine::setObjectOwnership(np, QQmlEngine::CppOwnership);
        connect(np, &Parameter::changeRequested, this, &Machine::onParameterChangeRequested);
        m_params.insert(np->id(), np);
    }
    // the engine may have restarted with other plugins; polling an id it
    // does not know would crash it (ParamMap::operator[] does not check)
    for (auto it = m_params.begin(); it != m_params.end();) {
        if (seen.contains(it.key())) {
            ++it;
            continue;
        }
        m_watched.remove(it.key());
        it.value()->deleteLater();
        it = m_params.erase(it);
    }
    if (m_params.contains(QStringLiteral("system.current_bank"))) {
        m_currentBank = m_params.value(QStringLiteral("system.current_bank"))->value().toString();
        m_currentPreset = m_params.value(QStringLiteral("system.current_preset"))->value().toString();
        emit presetChanged();
    }
}

void Machine::loadPlugins(const QJsonArray& list) {
    m_pluginInfo.clear();
    m_pluginList.clear();
    m_unitUi.clear();
    for (const QJsonValue& v : list) {
        const QJsonObject o = v.toObject();
        const QString id = o.value(QStringLiteral("id")).toString();
        const int flags = o.value(QStringLiteral("flags")).toInt();
        QString name = o.value(QStringLiteral("name")).toString();
        QString shortname = o.value(QStringLiteral("shortname")).toString();
        QVariantMap info{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), name.isEmpty() ? id : name},
            {QStringLiteral("shortname"), shortname.isEmpty() ? (name.isEmpty() ? id : name) : shortname},
            {QStringLiteral("category"), o.value(QStringLiteral("category")).toString()},
            {QStringLiteral("description"), o.value(QStringLiteral("description")).toString()},
            {QStringLiteral("stereo"), bool(flags & PGN_STEREO)},
            {QStringLiteral("fixed"), bool(flags & PGN_FIXED_GUI)},
        };
        m_pluginInfo.insert(id, info);
        // units the user can add to the rack
        if ((flags & PGN_GUI) && !(flags & PGN_FIXED_GUI) && !info.value(QStringLiteral("category")).toString().isEmpty()) {
            m_pluginList.append(info);
        }
    }
    std::sort(m_pluginList.begin(), m_pluginList.end(), [](const QVariant& a, const QVariant& b) {
        const QVariantMap ma = a.toMap(), mb = b.toMap();
        int c = QString::compare(ma.value(QStringLiteral("category")).toString(),
                                 mb.value(QStringLiteral("category")).toString(), Qt::CaseInsensitive);
        if (c != 0) {
            return c < 0;
        }
        return QString::compare(ma.value(QStringLiteral("name")).toString(),
                                mb.value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
    });
    emit pluginsChanged();
}

void Machine::requestBanks() {
    m_rpc.call(QStringLiteral("banks"), {}, [this](const QJsonValue& r) {
        m_banks = r.toArray().toVariantList();
        emit banksChanged();
    });
}

void Machine::requestRack() {
    for (int stereo = 0; stereo < 2; ++stereo) {
        m_rpc.call(QStringLiteral("get_rack_unit_order"), {stereo}, [this, stereo](const QJsonValue& r) {
            QStringList l;
            for (const QJsonValue& v : r.toArray()) {
                l.append(v.toString());
            }
            if (l != m_rack[stereo]) {
                m_rack[stereo] = l;
                emit rackChanged();
            }
        });
    }
}

void Machine::onNotification(const QString& method, const QJsonArray& params) {
    if (method == QLatin1String("set")) {
        // id/value pairs; get_updates answers with one "set" for all ids
        bool rack = false;
        for (int i = 0; i + 1 < params.size(); i += 2) {
            const QString id = params.at(i).toString();
            Parameter *p = m_params.value(id);
            if (!p) {
                continue;
            }
            p->updateFromEngine(params.at(i + 1));
            if (id.startsWith(QLatin1String("ui.")) || id.endsWith(QLatin1String(".position"))
                || id.endsWith(QLatin1String(".pp"))) {
                rack = true;
            }
        }
        if (rack && !m_rackTimer.isActive()) {
            m_rackTimer.start();
        }
    } else if (method == QLatin1String("preset_changed")) {
        m_currentBank = params.at(0).toString();
        m_currentPreset = params.at(1).toString();
        emit presetChanged();
        m_rackTimer.start();
    } else if (method == QLatin1String("state_changed")) {
        m_engineState = params.at(0).toString();
        emit engineStateChanged();
    } else if (method == QLatin1String("rack_units_changed")) {
        const QJsonArray a = params.at(0).toArray();
        const int stereo = a.at(0).toInt() ? 1 : 0;
        QStringList l;
        for (int i = 1; i < a.size(); ++i) {
            l.append(a.at(i).toString());
        }
        m_rack[stereo] = l;
        emit rackChanged();
    } else if (method == QLatin1String("tuner_changed")) {
        m_tuner.tunerChanged(params);
    } else if (method == QLatin1String("midi_changed")) {
        m_midi.midiChanged(params);
    } else if (method == QLatin1String("presetlist_changed")) {
        requestBanks();
    } else if (method == QLatin1String("plugins_changed")) {
        m_rpc.call(QStringLiteral("pluginlist"), {}, [this](const QJsonValue& r) {
            loadPlugins(r.toArray());
        });
    } else if (method == QLatin1String("message")) {
        m_lastMessage = params.at(1).toString();
        emit messageReceived(params.at(0).toString(), m_lastMessage);
    } else if (method == QLatin1String("server_shutdown")) {
        setStatus(tr("Engine shut down"));
    }
}

void Machine::onParameterChangeRequested(Parameter *p) {
    // the server does not echo a change back to the connection it came from
    m_rpc.notify(QStringLiteral("set"), {p->id(), p->toJson()});
    if (p->id().startsWith(QLatin1String("ui."))) {
        m_rackTimer.start();
    }
}

Parameter *Machine::param(const QString& id) const {
    return m_params.value(id, nullptr);
}

QVariantMap Machine::pluginInfo(const QString& id) const {
    auto it = m_pluginInfo.constFind(id);
    if (it != m_pluginInfo.constEnd()) {
        return it.value();
    }
    return QVariantMap{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), id == QLatin1String("ampstack") ? tr("Amplifier") : id},
        {QStringLiteral("shortname"), id == QLatin1String("ampstack") ? tr("Amp") : id},
        {QStringLiteral("category"), QString()},
        {QStringLiteral("stereo"), false},
        {QStringLiteral("fixed"), true},
    };
}

QList<Parameter*> Machine::unitParameters(const QString& id) const {
    QList<Parameter*> l;
    const QString prefix = id + QLatin1Char('.');
    for (Parameter *p : m_params) {
        if (p->id().startsWith(prefix)) {
            l.append(p);
        }
    }
    std::sort(l.begin(), l.end(), [](Parameter *a, Parameter *b) { return a->id() < b->id(); });
    return l;
}

QVariantMap Machine::unitUi(const QString& id) {
    auto it = m_unitUi.constFind(id);
    if (it != m_unitUi.constEnd()) {
        return it.value();
    }
    if (m_unitUiPending.contains(id) || !m_rpc.isConnected()) {
        return QVariantMap();
    }
    m_unitUiPending.insert(id);
    // like gx_head: the Glade layout when the unit has one, else the stack
    // layout (GUITARIQX_FORM_STACK=1 forces it, like GUITARIX_FORM_STACK)
    const bool glade = !qEnvironmentVariableIntValue("GUITARIQX_FORM_STACK");
    m_rpc.call(QStringLiteral("plugin_load_ui"), {id, glade ? UI_FORM_GLADE | UI_FORM_STACK : UI_FORM_STACK},
               [this, id](const QJsonValue& r) {
        const QJsonArray ops = r.toArray();
        if (!ops.isEmpty() && ops.at(0).toArray().at(0).toString() == QLatin1String("load_glade")) {
            QVariantMap ui = uitree::fromGlade(ops.at(0).toArray().at(1).toString());
            if (uitree::hasControls(ui.value(QStringLiteral("main")).toMap())) {
                finishUnitUi(id, ui);
                return;
            }
            // unusable Glade layout: try the stack layout
            m_rpc.call(QStringLiteral("plugin_load_ui"), {id, UI_FORM_STACK}, [this, id](const QJsonValue& r) {
                finishUnitUi(id, r.isArray() ? uitree::fromStackOps(r.toArray()) : QVariantMap());
            });
            return;
        }
        finishUnitUi(id, r.isArray() ? uitree::fromStackOps(ops) : QVariantMap());
    });
    return QVariantMap();
}

void Machine::finishUnitUi(const QString& id, QVariantMap ui) {
    m_unitUiPending.remove(id);
    {
        if (!uitree::hasControls(ui.value(QStringLiteral("main")).toMap())) {
            ui = uitree::generic(unitParameters(id));
        }
        // some layouts name parameters wrongly ("rtneuralloadfile" for
        // rtneural.loadfile); gx_head then shows no control either
        const QString prefix = id + QLatin1Char('.');
        uitree::fixIds(ui, [this, &id, &prefix](const QString& pid) {
            if (m_params.contains(pid) || !pid.startsWith(id)) {
                return pid;
            }
            const QString fixed = prefix + pid.mid(id.size());
            return m_params.contains(fixed) ? fixed : pid;
        });
        // the convolvers choose their impulse response in a separate
        // window in gx_head; here it is a control of the unit
        if (m_params.contains(prefix + QStringLiteral("convolver"))
            && !uitree::hasWidget(ui.value(QStringLiteral("main")).toMap(), QStringLiteral("ir"))) {
            uitree::prependControl(ui, QStringLiteral("ir"), prefix + QStringLiteral("convolver"));
        }
        m_unitUi.insert(id, ui);
        emit unitUiReady(id);
    }
}

void Machine::setPreset(const QString& bank, const QString& preset) {
    m_rpc.notify(QStringLiteral("setpreset"), {bank, preset});
}

void Machine::stepPreset(int delta) {
    for (const QVariant& b : std::as_const(m_banks)) {
        const QVariantMap bank = b.toMap();
        if (bank.value(QStringLiteral("name")).toString() != m_currentBank) {
            continue;
        }
        const QStringList presets = bank.value(QStringLiteral("presets")).toStringList();
        if (presets.isEmpty()) {
            return;
        }
        int idx = presets.indexOf(m_currentPreset);
        idx = idx < 0 ? 0 : (idx + delta + presets.size()) % presets.size();
        setPreset(m_currentBank, presets.at(idx));
        return;
    }
}

void Machine::setEngineState(const QString& state) {
    m_rpc.notify(QStringLiteral("setstate"), {state});
}

void Machine::insertUnit(const QString& id, const QString& before, bool stereo) {
    // insert_rack_unit also moves a unit that is already in the rack
    m_rpc.notify(QStringLiteral("insert_rack_unit"), {id, before, stereo ? 1 : 0});
    m_rackTimer.start();
}

void Machine::removeUnit(const QString& id, bool stereo) {
    m_rpc.notify(QStringLiteral("remove_rack_unit"), {id, stereo ? 1 : 0});
    m_rackTimer.start();
}

void Machine::moveUnit(const QString& id, int delta, bool stereo) {
    const QStringList& rack = m_rack[stereo ? 1 : 0];
    int idx = rack.indexOf(id);
    if (idx < 0) {
        return;
    }
    int target = idx + delta;
    if (target < 0 || target >= rack.size()) {
        return;
    }
    // insert before the unit that follows the target slot
    QString before;
    if (delta > 0) {
        before = target + 1 < rack.size() ? rack.at(target + 1) : QString();
    } else {
        before = rack.at(target);
    }
    insertUnit(id, before, stereo);
}

void Machine::watchOutput(const QString& id) {
    // get_updates only handles float outputs (it asserts on others)
    Parameter *p = m_params.value(id);
    if (!p || !(p->isOutput() || p->isMaxlevel()) || p->kind() != Parameter::Float) {
        return;
    }
    m_watched[id] += 1;
    if (m_ready && !m_pollTimer.isActive()) {
        m_pollTimer.start();
    }
}

void Machine::unwatchOutput(const QString& id) {
    auto it = m_watched.find(id);
    if (it == m_watched.end()) {
        return;
    }
    if (--it.value() <= 0) {
        m_watched.erase(it);
    }
    if (m_watched.isEmpty()) {
        m_pollTimer.stop();
    }
}

void Machine::pollOutputs() {
    if (m_watched.isEmpty() || !m_rpc.isConnected()) {
        return;
    }
    QJsonArray ids;
    for (auto it = m_watched.cbegin(); it != m_watched.cend(); ++it) {
        ids.append(it.key());
    }
    m_rpc.notify(QStringLiteral("get_updates"), ids);
}
