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

#include "fakeengine.h"

#include <cmath>

#include <QFile>
#include <QJsonDocument>
#include <QMap>
#include <QTcpSocket>
#include <QUrl>

namespace {

QJsonObject loadFixture() {
    QFile f(QStringLiteral(GUITARIQX_FIXTURES "/engine.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        qFatal("cannot read %s", qPrintable(f.fileName()));
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}

// see Parameter::fromJson
QJsonObject parameterBody(const QJsonObject& entry) {
    if (entry.contains(QStringLiteral("value_names"))) {
        if (entry.contains(QStringLiteral("IntParameter"))) {
            return entry.value(QStringLiteral("IntParameter")).toObject();
        }
        return entry.value(QStringLiteral("FloatParameter")).toObject();
    }
    return entry;
}

QStringList toStringList(const QJsonArray& a, int from = 0) {
    QStringList l;
    for (int i = from; i < a.size(); ++i) {
        l.append(a.at(i).toString());
    }
    return l;
}

} // anonymous namespace

FakeEngine::FakeEngine(int interfaceMinor, QObject *parent)
    : QObject(parent), m_minor(interfaceMinor), m_fixture(loadFixture()) {
    connect(&m_server, &QTcpServer::newConnection, this, &FakeEngine::onNewConnection);
    loadParameterValues(m_fixture.value(QStringLiteral("parameterlist")).toArray());
    for (const QJsonValue& p : m_fixture.value(QStringLiteral("pluginlist")).toArray()) {
        m_plugins.insert(p.toObject().value(QStringLiteral("id")).toString());
    }
    m_banks = m_fixture.value(QStringLiteral("banks")).toArray();
    const QJsonArray racks = m_fixture.value(QStringLiteral("get_rack_unit_order")).toArray();
    m_rack[0] = toStringList(racks.at(0).toArray());
    m_rack[1] = toStringList(racks.at(1).toArray());
    m_midiMap = m_fixture.value(QStringLiteral("get_midi_controller_map")).toArray();
}

FakeEngine::~FakeEngine() {
    // the sockets, children of the server, are deleted after the lists
    for (QTcpSocket *s : std::as_const(m_clients)) {
        s->disconnect(this);
    }
}

void FakeEngine::loadParameterValues(const QJsonArray& list) {
    for (int i = 0; i + 1 < list.size(); i += 2) {
        const QJsonObject body = parameterBody(list.at(i + 1).toObject());
        const QString id = body.value(QStringLiteral("Parameter")).toObject().value(QStringLiteral("id")).toString();
        m_paramEntries.insert(id, body);
        m_tags.insert(id, list.at(i).toString());
        m_values.insert(id, body.value(QStringLiteral("value")));
    }
}

bool FakeEngine::listen(quint16 port) {
    return m_server.listen(QHostAddress::LocalHost, port);
}

void FakeEngine::close() {
    m_server.close();
    disconnectClients();
}

void FakeEngine::disconnectClients() {
    const QList<QTcpSocket*> clients = m_clients;
    for (QTcpSocket *s : clients) {
        s->disconnectFromHost();
    }
}

QList<FakeEngine::Message> FakeEngine::received(const QString& method) const {
    QList<Message> l;
    for (const Message& m : m_received) {
        if (m.method == method) {
            l.append(m);
        }
    }
    return l;
}

void FakeEngine::onNewConnection() {
    while (QTcpSocket *s = m_server.nextPendingConnection()) {
        m_clients.append(s);
        connect(s, &QTcpSocket::readyRead, this, [this, s]() { onReadyRead(s); });
        connect(s, &QTcpSocket::disconnected, this, [this, s]() {
            m_clients.removeAll(s);
            m_listeners.removeAll(s);
            m_buffers.remove(s);
            // the engine leaves MIDI config mode when a connection closes
            m_midiConfig = false;
            s->deleteLater();
        });
    }
}

void FakeEngine::onReadyRead(QTcpSocket *s) {
    QByteArray& buf = m_buffers[s];
    buf.append(s->readAll());
    int nl;
    while ((nl = buf.indexOf('\n')) >= 0) {
        const QByteArray line = buf.left(nl).trimmed();
        buf.remove(0, nl + 1);
        if (line.isEmpty()) {
            continue;
        }
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (!doc.isObject()) {
            violation(QStringLiteral("not a JSON object: %1").arg(QString::fromUtf8(line.left(200))));
            continue;
        }
        handle(s, doc.object());
    }
}

void FakeEngine::handle(QTcpSocket *s, const QJsonObject& msg) {
    const QString method = msg.value(QStringLiteral("method")).toString();
    const QJsonArray params = msg.value(QStringLiteral("params")).toArray();
    const bool isCall = msg.contains(QStringLiteral("id"));
    checkNoBool(msg, method);
    m_received.append({method, params, isCall});
    if (method == QLatin1String("get_midi_feedback") && isCall) {
        // the engine's answer is not valid JSON: an object without key
        s->write(QStringLiteral("{\"jsonrpc\": \"2.0\",\"id\": \"%1\",\"result\": {%2}}\n")
                     .arg(msg.value(QStringLiteral("id")).toVariant().toString())
                     .arg(m_midiFeedback ? 1 : 0).toUtf8());
        emit messageReceived(method);
        return;
    }
    bool known = true;
    const QJsonValue result = dispatch(s, method, params, known);
    if (!known) {
        violation(QStringLiteral("unknown method %1").arg(method));
    }
    if (isCall) {
        // the engine sends the id back as a string
        QJsonObject reply{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), msg.value(QStringLiteral("id")).toVariant().toString()},
        };
        if (known) {
            reply.insert(QStringLiteral("result"), result);
        } else {
            reply.insert(QStringLiteral("error"),
                         QJsonObject{{QStringLiteral("code"), -32601}, {QStringLiteral("message"), QStringLiteral("Method not found")}});
        }
        send(s, reply);
    }
    emit messageReceived(method);
}

void FakeEngine::send(QTcpSocket *s, const QJsonObject& msg) {
    s->write(QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n');
}

void FakeEngine::notifyClients(const QString& method, const QJsonArray& params) {
    const QJsonObject msg{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("method"), method},
        {QStringLiteral("params"), params},
    };
    for (QTcpSocket *s : std::as_const(m_listeners)) {
        send(s, msg);
    }
}

void FakeEngine::setState(const QString& state) {
    m_fixture.insert(QStringLiteral("getstate"), state);
    notifyClients(QStringLiteral("state_changed"), {state});
}

void FakeEngine::finishSwitcher(bool tuner) {
    m_switcher = false;
    notifyClients(QStringLiteral("set_display_state"), {QStringLiteral("normal_mode")});
    QString state = m_switcherOldState;
    if (tuner && state == QLatin1String("running")) {
        state = QStringLiteral("bypassed");
    }
    setState(state);
    notifyClients(QStringLiteral("show_tuner"), {tuner ? 1 : 0});
}

void FakeEngine::violation(const QString& text) {
    qWarning("FakeEngine: %s", qPrintable(text));
    m_violations.append(text);
}

// the engine's JSON parser has no true/false tokens
void FakeEngine::checkNoBool(const QJsonValue& v, const QString& where) {
    if (v.isBool()) {
        violation(QStringLiteral("JSON boolean in %1").arg(where));
    } else if (v.isArray()) {
        for (const QJsonValue& e : v.toArray()) {
            checkNoBool(e, where);
        }
    } else if (v.isObject()) {
        const QJsonObject o = v.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            checkNoBool(it.value(), where);
        }
    }
}

// ParamMap::operator[] does not check the id: the engine crashes
bool FakeEngine::checkParam(const QString& method, const QJsonValue& id) {
    if (m_values.contains(id.toString())) {
        return true;
    }
    violation(QStringLiteral("%1 with unknown parameter %2").arg(method, id.toString()));
    return false;
}

// the bank and preset procedures do not check banks.get_file()
bool FakeEngine::checkBank(const QString& method, const QJsonValue& name) {
    if (bankIndex(name.toString()) >= 0) {
        return true;
    }
    violation(QStringLiteral("%1 with unknown bank %2").arg(method, name.toString()));
    return false;
}

int FakeEngine::bankIndex(const QString& name) const {
    for (int i = 0; i < m_banks.size(); ++i) {
        if (m_banks.at(i).toObject().value(QStringLiteral("name")).toString() == name) {
            return i;
        }
    }
    return -1;
}

QStringList FakeEngine::presets(const QString& bank) const {
    return toStringList(m_banks.at(bankIndex(bank)).toObject().value(QStringLiteral("presets")).toArray());
}

void FakeEngine::setPresets(const QString& bank, const QStringList& presets) {
    const int i = bankIndex(bank);
    QJsonObject b = m_banks.at(i).toObject();
    b.insert(QStringLiteral("presets"), QJsonArray::fromStringList(presets));
    m_banks.replace(i, b);
}

QString FakeEngine::uniqueBankName(const QString& name) const {
    QString n = name;
    for (int i = 1; bankIndex(n) >= 0; ++i) {
        n = QStringLiteral("%1-%2").arg(name).arg(i);
    }
    return n;
}

// OscilloscopeInfo::writeJSON: [load, frames, is_rt, jack buffer size,
// size, [samples]]; a sine of a few periods
QJsonArray FakeEngine::oscilloscopeInfo() const {
    const int bs = 256;
    const int size = bs * m_oscMul;
    QJsonArray samples;
    for (int i = 0; i < size; ++i) {
        samples.append(0.6 * std::sin(i * 2 * M_PI * 3 / bs));
    }
    return QJsonArray{12, 4251, 1, bs, size, samples};
}

// JConv and Seq parameters take an object (GxJConvSettings::readJSON,
// GxSeqSettings::readJSON); the convolver asserts when its gain line does
// not start at 0 or is not strictly increasing (GxConvolver::read_sndfile)
void FakeEngine::checkSpecialValue(const QString& id, const QJsonValue& v) {
    const QString tag = m_tags.value(id);
    if (tag == QLatin1String("JConv")) {
        const QJsonObject o = v.toObject();
        if (o.isEmpty()) {
            violation(QStringLiteral("set %1 without settings object").arg(id));
            return;
        }
        for (const char *k : {"jconv.Offset", "jconv.Length", "jconv.Delay"}) {
            const double n = o.value(QLatin1String(k)).toDouble(-1);
            if (n < 0 || n != std::floor(n)) {
                violation(QStringLiteral("set %1 with %2 %3").arg(id, QLatin1String(k)).arg(n));
            }
        }
        const QJsonArray line = o.value(QStringLiteral("jconv.gainline")).toArray();
        double last = -1;
        for (const QJsonValue& pt : line) {
            const double i = pt.toArray().at(0).toDouble(-1);
            if (i != std::floor(i) || i <= last || (last < 0 && i != 0)) {
                violation(QStringLiteral("set %1 with a bad gain line point %2").arg(id).arg(i));
            }
            last = i;
        }
        if (line.size() == 1) {
            violation(QStringLiteral("set %1 with a single gain line point").arg(id));
        }
    } else if (tag == QLatin1String("Seq")) {
        const QJsonValue line = v.toObject().value(QStringLiteral("seq.seqline"));
        // an empty line makes the sequence length -1 (min_seq_size)
        if (!line.isArray() || line.toArray().isEmpty()) {
            violation(QStringLiteral("set %1 without steps").arg(id));
        }
        for (const QJsonValue& b : line.toArray()) {
            if (b.toDouble(-1) != 0 && b.toDouble(-1) != 1) {
                violation(QStringLiteral("set %1 with step value %2").arg(id, b.toVariant().toString()));
            }
        }
    } else if (tag == QLatin1String("Osc")) {
        violation(QStringLiteral("set of the output %1").arg(id));
    }
}

QJsonValue FakeEngine::paramValue(const QString& id) const {
    return m_values.value(id);
}

QJsonValue FakeEngine::dispatch(QTcpSocket *s, const QString& method, const QJsonArray& params, bool& known) {
    const auto fixture = [this](const char *key) { return m_fixture.value(QLatin1String(key)); };
    const QString p0 = params.at(0).toString();

    // connection and engine state
    if (method == QLatin1String("getversion")) {
        QJsonArray v = fixture("getversion").toArray();
        v.replace(0, m_major);
        v.replace(1, m_minor);
        return v;
    }
    if (method == QLatin1String("listen")) {
        if (!m_listeners.contains(s)) {
            m_listeners.append(s);
        }
        return {};
    }
    if (method == QLatin1String("parameterlist") || method == QLatin1String("pluginlist")
        || method == QLatin1String("load_impresp_dirs")) {
        return m_fixture.value(method);
    }
    if (method == QLatin1String("getstate")) {
        return fixture("getstate");
    }
    if (method == QLatin1String("setstate")) {
        setState(p0);
        return {};
    }
    if (method == QLatin1String("jack_cpu_load")) {
        return 12.5;
    }
    if (method == QLatin1String("tuner_used_for_display")) {
        return {};
    }
    // TunerSwitcher (tunerswitcher.cpp)
    if (method == QLatin1String("get_tuner_switcher_active")) {
        return m_switcher ? 1 : 0;
    }
    if (method == QLatin1String("tuner_switcher_activate")
        || (method == QLatin1String("tuner_switcher_toggle") && !m_switcher && params.at(0).toInt() == 0)) {
        if (!m_switcher) {
            m_switcher = true;
            m_switcherOldState = fixture("getstate").toString();
            m_switcherOldTuner = params.at(0).toInt() != 0;
            notifyClients(QStringLiteral("set_display_state"), {QStringLiteral("wait_start")});
            setState(QStringLiteral("stopped"));
        }
        return {};
    }
    if (method == QLatin1String("tuner_switcher_deactivate")
        || (method == QLatin1String("tuner_switcher_toggle") && m_switcher)) {
        if (m_switcher) {
            finishSwitcher(m_switcherOldTuner);
        }
        return {};
    }
    if (method == QLatin1String("tuner_switcher_toggle")) {
        // the tuner is shown: close it
        if (fixture("getstate").toString() == QLatin1String("bypassed")) {
            setState(QStringLiteral("running"));
        }
        notifyClients(QStringLiteral("show_tuner"), {0});
        return {};
    }

    // parameters
    if (method == QLatin1String("set")) {
        for (int i = 0; i + 1 < params.size(); i += 2) {
            if (checkParam(method, params.at(i))) {
                checkSpecialValue(params.at(i).toString(), params.at(i + 1));
                m_values.insert(params.at(i).toString(), params.at(i + 1));
            }
        }
        return {};
    }
    if (method == QLatin1String("get")) {
        QJsonObject o;
        for (const QJsonValue& id : params) {
            if (checkParam(method, id)) {
                o.insert(id.toString(), paramValue(id.toString()));
            }
        }
        return o;
    }
    if (method == QLatin1String("get_updates")) {
        QJsonArray values;
        for (const QJsonValue& id : params) {
            if (!checkParam(method, id)) {
                continue;
            }
            // get_updates asserts on anything but float and oscilloscope
            // parameters
            const QString tag = m_tags.value(id.toString());
            if (tag != QLatin1String("Float") && tag != QLatin1String("FloatEnum") && tag != QLatin1String("Osc")) {
                violation(QStringLiteral("get_updates with the %1 parameter %2").arg(tag, id.toString()));
                continue;
            }
            values.append(id);
            values.append(tag == QLatin1String("Osc") ? oscilloscopeInfo() : paramValue(id.toString()));
        }
        send(s, QJsonObject{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("method"), QStringLiteral("set")},
            {QStringLiteral("params"), values},
        });
        return {};
    }

    // presets and banks
    if (method == QLatin1String("banks")) {
        return m_banks;
    }
    if (method == QLatin1String("presets")) {
        return checkBank(method, p0) ? QJsonArray::fromStringList(presets(p0)) : QJsonArray();
    }
    if (method == QLatin1String("setpreset")) {
        if (checkBank(method, p0)) {
            m_values.insert(QStringLiteral("system.current_bank"), p0);
            m_values.insert(QStringLiteral("system.current_preset"), params.at(1));
            notifyClients(QStringLiteral("preset_changed"), params);
        }
        return {};
    }
    if (method == QLatin1String("save_current")) {
        return {};
    }
    if (method == QLatin1String("save_preset")) {
        if (checkBank(method, p0)) {
            QStringList l = presets(p0);
            if (!l.contains(params.at(1).toString())) {
                setPresets(p0, l << params.at(1).toString());
            }
            m_values.insert(QStringLiteral("system.current_bank"), p0);
            m_values.insert(QStringLiteral("system.current_preset"), params.at(1));
        }
        return {};
    }
    if (method == QLatin1String("rename_preset")) {
        if (checkBank(method, p0)) {
            QStringList l = presets(p0);
            if (l.contains(params.at(2).toString())) {
                violation(QStringLiteral("rename_preset to the existing name %1").arg(params.at(2).toString()));
            }
            const int i = l.indexOf(params.at(1).toString());
            if (i >= 0) {
                l[i] = params.at(2).toString();
                setPresets(p0, l);
            }
        }
        return 1;
    }
    if (method == QLatin1String("erase_preset")) {
        if (checkBank(method, p0)) {
            QStringList l = presets(p0);
            l.removeAll(params.at(1).toString());
            setPresets(p0, l);
        }
        return {};
    }
    if (method == QLatin1String("reorder_preset")) {
        if (checkBank(method, p0)) {
            const QStringList l = toStringList(params, 1);
            QStringList a = l, b = presets(p0);
            a.sort();
            b.sort();
            if (a != b) {
                violation(QStringLiteral("reorder_preset with other presets than those of %1").arg(p0));
            } else {
                setPresets(p0, l);
            }
        }
        return {};
    }
    if (method == QLatin1String("pf_append")) {
        if (checkBank(method, p0) && checkBank(method, params.at(2))) {
            setPresets(params.at(2).toString(), presets(params.at(2).toString()) << params.at(3).toString());
        }
        return {};
    }
    if (method == QLatin1String("bank_insert_new") || method == QLatin1String("bank_insert_content")) {
        QString name = p0;
        if (method == QLatin1String("bank_insert_content")) {
            name = QUrl(p0).fileName().section(QLatin1Char('.'), 0, 0);
        }
        const QJsonObject b{
            {QStringLiteral("name"), uniqueBankName(name)},
            {QStringLiteral("mutable"), 1},
            {QStringLiteral("type"), QStringLiteral("file")},
            {QStringLiteral("presets"), QJsonArray()},
        };
        m_banks.insert(0, b);
        return b;
    }
    if (method == QLatin1String("rename_bank")) {
        if (checkBank(method, p0)) {
            const int i = bankIndex(p0);
            QJsonObject b = m_banks.at(i).toObject();
            b.insert(QStringLiteral("name"), params.at(1));
            m_banks.replace(i, b);
        }
        return 1;
    }
    if (method == QLatin1String("bank_remove")) {
        if (checkBank(method, p0)) {
            m_banks.removeAt(bankIndex(p0));
        }
        return 1;
    }
    if (method == QLatin1String("bank_reorder")) {
        QStringList a = toStringList(params), b;
        for (const QJsonValue& v : std::as_const(m_banks)) {
            b.append(v.toObject().value(QStringLiteral("name")).toString());
        }
        QStringList sa = a, sb = b;
        sa.sort();
        sb.sort();
        if (sa != sb) {
            violation(QStringLiteral("bank_reorder with other banks than the engine's"));
            return {};
        }
        QJsonArray banks;
        for (const QString& n : a) {
            banks.append(m_banks.at(bankIndex(n)));
        }
        m_banks = banks;
        return {};
    }
    if (method == QLatin1String("bank_get_contents")) {
        if (!checkBank(method, p0)) {
            return QJsonArray();
        }
        return QJsonArray{QStringLiteral("/home/guitarist/.config/guitarix/banks/%1.gx").arg(p0),
                          QStringLiteral("[\"gx_head_file_version\", [1, 2, \"0.47.0\"]]\n")};
    }

    // rack
    if (method == QLatin1String("get_rack_unit_order")) {
        return QJsonArray::fromStringList(m_rack[params.at(0).toInt() ? 1 : 0]);
    }
    if (method == QLatin1String("insert_rack_unit") || method == QLatin1String("remove_rack_unit")) {
        if (!m_plugins.contains(p0)) {
            violation(QStringLiteral("%1 with unknown plugin %2").arg(method, p0));
            return {};
        }
        const bool insert = method == QLatin1String("insert_rack_unit");
        QStringList& rack = m_rack[params.at(insert ? 2 : 1).toInt() ? 1 : 0];
        rack.removeAll(p0);
        if (insert) {
            const int i = rack.indexOf(params.at(1).toString());
            rack.insert(i < 0 ? rack.size() : i, p0);
        }
        return {};
    }
    if (method == QLatin1String("plugin_load_ui")) {
        const QJsonObject uis = fixture("plugin_load_ui").toObject();
        if (!m_plugins.contains(p0)) {
            // lookup_plugin aborts the engine
            violation(QStringLiteral("plugin_load_ui with unknown plugin %1").arg(p0));
            return QJsonArray();
        }
        const QJsonObject ui = uis.value(p0).toObject();
        const bool glade = params.at(1).toInt() & 0x02;
        return ui.value(glade ? QStringLiteral("glade") : QStringLiteral("stack")).toArray();
    }

    // MIDI
    if (method == QLatin1String("get_midi_controller_map")) {
        return m_midiMap;
    }
    if (method == QLatin1String("midi_set_config_mode")) {
        m_midiConfig = params.at(0).toInt() != 0;
        m_midiControl = params.at(1).toInt();
        return {};
    }
    if (method == QLatin1String("set_midi_feedback")) {
        m_midiFeedback = params.at(0).toInt() != 0;
        return {};
    }
    if (method == QLatin1String("midi_set_current_control")) {
        m_midiControl = params.at(0).toInt();
        return {};
    }
    if (method == QLatin1String("midi_get_config_mode")) {
        return QJsonArray{m_midiConfig ? 1 : 0, m_midiControl};
    }
    if (method == QLatin1String("midi_modifyCurrent") || method == QLatin1String("midi_deleteParameter")) {
        if (!checkParam(method, params.at(0))) {
            return {};
        }
        const bool modify = method == QLatin1String("midi_modifyCurrent");
        if (modify && (!m_midiConfig || m_midiControl < 0)) {
            violation(QStringLiteral("midi_modifyCurrent outside of MIDI config mode"));
            return {};
        }
        // [ctl, [[id, a, b], ...], ...] without the parameter
        QMap<int, QJsonArray> byCtl;
        for (int i = 0; i + 1 < m_midiMap.size(); i += 2) {
            QJsonArray l;
            for (const QJsonValue& c : m_midiMap.at(i + 1).toArray()) {
                if (c.toArray().at(0).toString() != p0) {
                    l.append(c);
                }
            }
            if (!l.isEmpty()) {
                byCtl.insert(m_midiMap.at(i).toInt(), l);
            }
        }
        if (modify) {
            const bool range = params.at(3).toInt() == 0
                && m_paramEntries.value(p0).value(QStringLiteral("Parameter")).toObject()
                       .value(QStringLiteral("c_type")).toInt() != 2;
            byCtl[m_midiControl].append(range ? QJsonArray{p0, params.at(1), params.at(2)}
                                              : QJsonArray{p0, params.at(3), params.at(4)});
        }
        m_midiMap = QJsonArray();
        for (auto it = byCtl.cbegin(); it != byCtl.cend(); ++it) {
            m_midiMap.append(it.key());
            m_midiMap.append(it.value());
        }
        // not {m_midiMap}: that would copy the array instead of wrapping it
        QJsonArray changed;
        changed.append(m_midiMap);
        notifyClients(QStringLiteral("midi_changed"), changed);
        return {};
    }

    // files
    if (method == QLatin1String("get_file_places") && m_minor >= 2) {
        return QJsonArray{
            QJsonArray{QStringLiteral("Home"), QStringLiteral("/home/guitarist")},
            QJsonArray{QStringLiteral("guitarix"), QStringLiteral("/home/guitarist/.config/guitarix")},
        };
    }
    if (method == QLatin1String("list_directory") && m_minor >= 2) {
        if (!m_dirs.contains(p0)) {
            return QJsonObject{{QStringLiteral("path"), p0}, {QStringLiteral("entries"), QJsonArray()},
                               {QStringLiteral("error"), QStringLiteral("No such file or directory")}};
        }
        return QJsonObject{{QStringLiteral("path"), p0}, {QStringLiteral("entries"), m_dirs.value(p0)}};
    }
    if (method == QLatin1String("read_audio")) {
        if (p0.startsWith(QLatin1String("/usr/share/gx_head/sounds/"))) {
            // factory IRs: 0.1 s of decaying oscillation, mono
            const int frames = 4800;
            QJsonArray samples;
            for (int i = 0; i < frames; ++i) {
                samples.append(std::exp(-i / 600.0) * std::sin(i * 0.05) * (i % 7 == 0 ? 1 : 0.6));
            }
            return QJsonArray{frames, 1, 2, 1, 48000, samples};
        }
        // 4 frames, 2 channels; sum of squares 8, normalized gain 1/2
        return QJsonArray{4, 2, 0, 0, 48000, QJsonArray{1, 1, 1, 1, 1, 1, 1, -1}};
    }
    if (method == QLatin1String("get_oscilloscope_mul_buffer")) {
        return m_oscMul;
    }
    if (method == QLatin1String("set_oscilloscope_mul_buffer")) {
        m_oscMul = params.at(0).toInt();
        if (m_oscMul < 1 || m_oscMul > 6) {
            violation(QStringLiteral("set_oscilloscope_mul_buffer %1").arg(m_oscMul));
        }
        return {};
    }
    if (method == QLatin1String("get_file_list")) {
        if (!checkParam(method, params.at(0))) {
            return QJsonArray();
        }
        return QJsonArray{QString(), QStringLiteral("clean.nam"), QStringLiteral("crunch.nam")};
    }

    known = false;
    return {};
}
