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

#include "midimap.h"
#include "machine.h"
#include "rpcclient.h"

#include <QHash>
#include <algorithm>

namespace {

enum { ctl_continuous = 1, ctl_switch = 2, ctl_enum = 3 };

// ControllerArray::array_size - 1 (gx_parameter.h)
const int maxController = 327;

// note on events are mapped to controller numbers 200..326
const int noteOnBase = 200;

// names of MidiStandardControllers (gx_paramtable.cpp)
const QHash<int, QString>& standardNames() {
    static const QHash<int, QString> names{
        {0, QStringLiteral("Bank Select MSB")}, {1, QStringLiteral("Modulation MSB")},
        {2, QStringLiteral("Breath Controller")}, {4, QStringLiteral("Foot Controller MSB")},
        {5, QStringLiteral("Portamento Time MSB")}, {6, QStringLiteral("Data Entry MSB")},
        {7, QStringLiteral("Main Volume")}, {8, QStringLiteral("Balance")},
        {10, QStringLiteral("Pan")}, {11, QStringLiteral("Expression")},
        {12, QStringLiteral("Effect Control 1")}, {13, QStringLiteral("Effect Control 2")},
        {22, QStringLiteral("Midi Beat Clock")}, {23, QStringLiteral("Clock start/stop")},
        {24, QStringLiteral("Jack Transport")}, {32, QStringLiteral("Bank Select LSB")},
        {64, QStringLiteral("Sustain")}, {65, QStringLiteral("Portamento")},
        {66, QStringLiteral("Sostenuto")}, {67, QStringLiteral("Soft Pedal")},
        {68, QStringLiteral("Legato Footswitch")}, {69, QStringLiteral("Hold 2")},
        {84, QStringLiteral("Portamento Control")},
        {96, QStringLiteral("Data Inc")}, {97, QStringLiteral("Data Dec")},
        {98, QStringLiteral("NRPN LSB")}, {99, QStringLiteral("NRPN MSB")},
        {100, QStringLiteral("RPN LSB")}, {101, QStringLiteral("RPN MSB")},
        {120, QStringLiteral("All Sounds Off")}, {121, QStringLiteral("Controller Reset")},
        {122, QStringLiteral("Local Control")}, {123, QStringLiteral("All Notes Off")},
        {124, QStringLiteral("Omni Off")}, {125, QStringLiteral("Omni On")},
        {126, QStringLiteral("Mono On (Poly Off)")}, {127, QStringLiteral("Poly On (Mono Off)")},
    };
    return names;
}

} // anonymous namespace

MidiMap::MidiMap(Machine& machine, RpcClient& rpc, QObject *parent)
    : QObject(parent), m_machine(machine), m_rpc(rpc) {
    // MidiConnect polls every 40 ms
    m_pollTimer.setInterval(100);
    connect(&m_pollTimer, &QTimer::timeout, this, &MidiMap::poll);
}

QString MidiMap::controllerName(int ctl) {
    if (ctl < 0) {
        return QStringLiteral("---");
    }
    if (ctl >= noteOnBase && ctl < noteOnBase + 127) {
        static const char *notes[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        const int n = ctl - noteOnBase;
        return QStringLiteral("Note On %1%2").arg(QString::fromLatin1(notes[n % 12])).arg(n / 12 - 1);
    }
    if (ctl >= 70 && ctl <= 79) {
        return QStringLiteral("CC %1 Sound Contr. %2").arg(ctl).arg(ctl - 69);
    }
    if (ctl >= 91 && ctl <= 95) {
        return QStringLiteral("CC %1 Eff. %2 Depth").arg(ctl).arg(ctl - 90);
    }
    auto it = standardNames().constFind(ctl);
    if (ctl == 22 || ctl == 23 || ctl == 24) {
        return it.value();
    }
    if (it != standardNames().constEnd()) {
        return QStringLiteral("CC %1 %2").arg(ctl).arg(it.value());
    }
    if (ctl < 128) {
        return QStringLiteral("CC %1").arg(ctl);
    }
    return QStringLiteral("Controller %1").arg(ctl);
}

void MidiMap::setLearnMode(bool on) {
    if (on == m_learnMode) {
        return;
    }
    m_learnMode = on;
    emit learnModeChanged();
}

int MidiMap::controllerFor(const QString& id) const {
    auto it = m_byParam.constFind(id);
    return it == m_byParam.constEnd() ? -1 : it.value().value(QStringLiteral("ctl")).toInt();
}

QVariantMap MidiMap::assignment(const QString& id) const {
    return m_byParam.value(id);
}

bool MidiMap::isLearnable(const QString& id) const {
    Parameter *p = m_machine.param(id);
    if (!p || p->isOutput() || !p->isControllable()) {
        return false;
    }
    const int t = p->controlType();
    return t == ctl_continuous || t == ctl_switch || t == ctl_enum;
}

bool MidiMap::usesRange(const QString& id) const {
    Parameter *p = m_machine.param(id);
    return p && (p->controlType() == ctl_continuous || p->controlType() == ctl_enum);
}

void MidiMap::engineReady() {
    m_rpc.call(QStringLiteral("get_midi_controller_map"), {}, [this](const QJsonValue& r) {
        load(r.toArray());
    });
    refreshFeedback();
}

void MidiMap::refreshFeedback() {
    // the engine answers {0} or {1}, which RpcClient turns into [0] or [1]
    m_rpc.call(QStringLiteral("get_midi_feedback"), {}, [this](const QJsonValue& r) {
        const QJsonValue v = r.isArray() ? r.toArray().at(0) : r;
        const bool on = v.isBool() ? v.toBool() : v.toInt() != 0;
        if (on != m_feedback) {
            m_feedback = on;
            emit feedbackChanged();
        }
    });
}

void MidiMap::setFeedback(bool on) {
    if (on == m_feedback) {
        return;
    }
    m_feedback = on;
    m_rpc.notify(QStringLiteral("set_midi_feedback"), {on ? 1 : 0});
    emit feedbackChanged();
}

// [ctl, value]: sent by the engine every 60 ms for the controllers that
// moved (MidiControllerList::check_midi_values)
void MidiMap::midiValueChanged(const QJsonArray& params) {
    const int ctl = params.at(0).toInt(-1);
    const int value = params.at(1).toInt(-1);
    if (ctl < 0 || ctl > maxController) {
        return;
    }
    if (value < 0) {
        m_values.remove(ctl);
    } else {
        m_values.insert(ctl, value);
    }
    emit controllerValueChanged(ctl, value < 0 ? -1 : value);
}

void MidiMap::engineLost() {
    const QList<int> ctls = m_values.keys();
    m_values.clear();
    for (int ctl : ctls) {
        emit controllerValueChanged(ctl, -1);
    }
    // the server leaves config mode when the connection closes
    // (CmdConnection::~CmdConnection)
    if (m_learning) {
        m_pollTimer.stop();
        m_learning = false;
        m_learnParam.clear();
        m_learnControl = -1;
        emit learnChanged();
    }
}

void MidiMap::midiChanged(const QJsonArray& params) {
    load(params.at(0).toArray());
}

// [ctl, [[id, lower, upper] | [id, toggle, behaviour], ...], ctl, [...], ...]
// (ControllerArray::writeJSON, MidiController::writeJSON)
void MidiMap::load(const QJsonArray& map) {
    m_byParam.clear();
    QVariantList list;
    for (int i = 0; i + 1 < map.size(); i += 2) {
        const int ctl = map.at(i).toInt();
        for (const QJsonValue& v : map.at(i + 1).toArray()) {
            const QJsonArray c = v.toArray();
            const QString id = c.at(0).toString();
            Parameter *p = m_machine.param(id);
            QVariantMap a{
                {QStringLiteral("ctl"), ctl},
                {QStringLiteral("ctlName"), controllerName(ctl)},
                {QStringLiteral("id"), id},
                {QStringLiteral("name"), p ? p->name() : id},
                {QStringLiteral("group"), p ? p->group() : QString()},
            };
            if (usesRange(id)) {
                a.insert(QStringLiteral("lower"), c.at(1).toDouble());
                a.insert(QStringLiteral("upper"), c.at(2).toDouble());
            } else {
                a.insert(QStringLiteral("toggle"), c.at(1).toInt() != 0);
                a.insert(QStringLiteral("behaviour"), c.at(2).toInt());
            }
            m_byParam.insert(id, a);
            list.append(a);
        }
    }
    m_assignments = list;
    ++m_revision;
    emit assignmentsChanged();
}

void MidiMap::startLearn(const QString& id) {
    if (!isLearnable(id)) {
        return;
    }
    if (m_learning) {
        // switch the dialog to another parameter, config mode stays on
        m_learnParam = id;
        emit learnChanged();
        return;
    }
    m_learning = true;
    m_learnParam = id;
    // start with the current assignment, like MidiConnect
    m_learnControl = controllerFor(id);
    m_rpc.notify(QStringLiteral("midi_set_config_mode"), {1, m_learnControl});
    m_pollTimer.start();
    emit learnChanged();
}

void MidiMap::setLearnControl(int ctl) {
    if (!m_learning) {
        return;
    }
    ctl = std::max(-1, std::min(maxController, ctl));
    m_learnControl = ctl;
    m_rpc.notify(QStringLiteral("midi_set_current_control"), {ctl});
    emit learnChanged();
}

void MidiMap::poll() {
    m_rpc.call(QStringLiteral("midi_get_config_mode"), {}, [this](const QJsonValue& r) {
        if (!m_learning) {
            return;
        }
        const QJsonArray a = r.toArray();
        if (!a.at(0).toInt()) {
            // another client ended config mode
            m_pollTimer.stop();
            m_learning = false;
            m_learnParam.clear();
            emit learnChanged();
            return;
        }
        const int ctl = a.at(1).toInt();
        if (ctl != m_learnControl) {
            m_learnControl = ctl;
            emit learnChanged();
        }
    });
}

void MidiMap::commitLearn(double lower, double upper, bool toggle, int behaviour) {
    if (!m_learning || m_learnControl < 0 || !m_machine.hasParam(m_learnParam)) {
        return;
    }
    // midi_modifyCurrent is only valid in config mode and looks the
    // parameter up without checking it (see jsonrpc.cpp)
    if (usesRange(m_learnParam)) {
        m_rpc.notify(QStringLiteral("midi_modifyCurrent"), {m_learnParam, lower, upper, 0, OnOff});
    } else {
        m_rpc.notify(QStringLiteral("midi_modifyCurrent"), {m_learnParam, 0, 0, toggle ? 1 : 0, behaviour});
    }
    stopLearn();
}

void MidiMap::cancelLearn() {
    stopLearn();
}

void MidiMap::stopLearn() {
    if (!m_learning) {
        return;
    }
    m_pollTimer.stop();
    m_rpc.notify(QStringLiteral("midi_set_config_mode"), {0, -1});
    m_learning = false;
    m_learnParam.clear();
    m_learnControl = -1;
    emit learnChanged();
}

void MidiMap::remove(const QString& id) {
    if (!m_machine.hasParam(id) || controllerFor(id) < 0) {
        return;
    }
    m_rpc.notify(QStringLiteral("midi_deleteParameter"), {id});
}
