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

#include "pluginmanager.h"
#include "machine.h"
#include "rpcclient.h"

#include <QJsonObject>
#include <algorithm>

namespace {

// ladspa::DisplayType (ladspaback.h)
enum DisplayType {
    tp_scale, tp_scale_log, tp_toggle, tp_enum, tp_display, tp_display_toggle, tp_none, tp_int, tp_enabled,
};

// ladspa::ChangeableValues set_flags
const int dflt_set = 0x02, low_set = 0x04, up_set = 0x08, tp_set = 0x10;

// ladspa::step_mode stp_normal
const int stepNormal = 1;

const int LADSPA_HINT_SAMPLE_RATE = 0x8;

int intOf(const QJsonObject& o, const char *key) {
    return o.value(QLatin1String(key)).toInt();
}

QString stringOf(const QJsonObject& o, const char *key) {
    return o.value(QLatin1String(key)).toString();
}

// ChangeableValues::get_excl_flags: the values a display type ignores
int exclFlags(int tp) {
    switch (tp) {
    case tp_enabled:
    case tp_toggle:
        return low_set | up_set;
    case tp_display:
        return dflt_set;
    case tp_display_toggle:
    case tp_none:
        return dflt_set | low_set | up_set;
    default:
        return 0;
    }
}

// PortDesc::get_tp
int portType(const QJsonObject& port) {
    const QJsonObject user = port.value(QLatin1String("user")).toObject();
    if (intOf(user, "set_flags") & tp_set) {
        return intOf(user, "tp");
    }
    return intOf(port.value(QLatin1String("factory")).toObject(), "tp");
}

QString portName(const QJsonObject& port) {
    const QJsonObject user = port.value(QLatin1String("user")).toObject();
    if (intOf(user, "set_flags") & 0x01) {
        return stringOf(user, "name");
    }
    return stringOf(port.value(QLatin1String("factory")).toObject(), "name");
}

// PortDesc::has_settings
bool portHasSettings(const QJsonObject& port) {
    if (!intOf(port, "has_caption") || intOf(port, "step") != stepNormal || intOf(port, "use_sr")) {
        return true;
    }
    const QJsonObject user = port.value(QLatin1String("user")).toObject();
    return (intOf(user, "set_flags") & ~exclFlags(portType(port)))
        || !user.value(QLatin1String("enumdict")).toObject().isEmpty();
}

// a ChangeableValues without settings
QJsonObject noUserValues() {
    return QJsonObject{
        {QStringLiteral("name"), QString()}, {QStringLiteral("dflt"), 0}, {QStringLiteral("low"), 0},
        {QStringLiteral("up"), 0}, {QStringLiteral("tp"), 0}, {QStringLiteral("newrow"), 0},
        {QStringLiteral("set_flags"), 0}, {QStringLiteral("enumdict"), QJsonObject()},
    };
}

} // anonymous namespace

PluginManager::PluginManager(Machine& machine, RpcClient& rpc)
    : QAbstractListModel(&machine), m_machine(machine), m_rpc(rpc) {
    // answers in flight are lost with the connection
    connect(&m_machine, &Machine::connectedChanged, this, [this]() {
        if (!m_machine.connected()) {
            setBusy(false);
        }
    });
}

QStringList PluginManager::categories() const {
    // the categories of gx_head's plugin window (ladspaliste.glade)
    return {
        QStringLiteral("Tone Control"), QStringLiteral("Distortion"), QStringLiteral("Fuzz"),
        QStringLiteral("Reverb"), QStringLiteral("Echo / Delay"), QStringLiteral("Modulation"),
        QStringLiteral("Guitar Effects"), QStringLiteral("Misc"), QStringLiteral("External"),
    };
}

int PluginManager::activeCount() const {
    int n = 0;
    for (const QJsonValue& v : m_plugins) {
        n += intOf(v.toObject(), "active") ? 1 : 0;
    }
    return n;
}

void PluginManager::setBusy(bool b) {
    if (b != m_busy) {
        m_busy = b;
        emit busyChanged();
    }
}

void PluginManager::setFilter(const QString& f) {
    if (f != m_filter) {
        m_filter = f;
        refilter();
        emit filterChanged();
    }
}

void PluginManager::setView(int v) {
    if (v != m_view) {
        m_view = v;
        refilter();
        emit filterChanged();
    }
}

void PluginManager::setChannels(int c) {
    if (c != m_channels) {
        m_channels = c;
        refilter();
        emit filterChanged();
    }
}

bool PluginManager::accepts(const QJsonObject& p) const {
    switch (m_view) {
    case ActivePlugins:
        if (!intOf(p, "active")) {
            return false;
        }
        break;
    case ChangedPlugins:
        if (!intOf(p, "has_settings")) {
            return false;
        }
        break;
    case LadspaPlugins:
        if (intOf(p, "is_lv2")) {
            return false;
        }
        break;
    case Lv2Plugins:
        if (!intOf(p, "is_lv2")) {
            return false;
        }
        break;
    default:
        break;
    }
    // PluginDesc::tp: 0 mono, 1 stereo
    if ((m_channels == MonoPlugins && intOf(p, "tp") != 0) || (m_channels == StereoPlugins && intOf(p, "tp") != 1)) {
        return false;
    }
    const QString f = m_filter.trimmed();
    if (f.isEmpty()) {
        return true;
    }
    for (const char *key : {"Name", "shortname", "Maker", "category"}) {
        if (stringOf(p, key).contains(f, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

void PluginManager::refilter() {
    beginResetModel();
    m_rows.clear();
    for (int i = 0; i < m_plugins.size(); ++i) {
        if (accepts(m_plugins.at(i).toObject())) {
            m_rows.append(i);
        }
    }
    endResetModel();
}

int PluginManager::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant PluginManager::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size()) {
        return {};
    }
    const int i = m_rows.at(index.row());
    const QJsonObject p = m_plugins.at(i).toObject();
    switch (role) {
    case IndexRole:
        return i;
    case Qt::DisplayRole:
    case NameRole:
        return stringOf(p, "Name");
    case ShortnameRole:
        return stringOf(p, "shortname");
    case MakerRole:
        return stringOf(p, "Maker");
    case Lv2Role:
        return bool(intOf(p, "is_lv2"));
    case StereoRole:
        return intOf(p, "tp") == 1;
    case ActiveRole:
        return bool(intOf(p, "active"));
    case CategoryRole:
        return stringOf(p, "category");
    case HasSettingsRole:
        return bool(intOf(p, "has_settings"));
    case ChangedRole:
        return m_changed.contains(i);
    default:
        return {};
    }
}

QHash<int, QByteArray> PluginManager::roleNames() const {
    return {
        {IndexRole, "index"}, {NameRole, "name"}, {ShortnameRole, "shortname"}, {MakerRole, "maker"},
        {Lv2Role, "lv2"}, {StereoRole, "stereo"}, {ActiveRole, "active"}, {CategoryRole, "category"},
        {HasSettingsRole, "hasSettings"}, {ChangedRole, "changed"},
    };
}

void PluginManager::load() {
    if (m_busy || !m_machine.ready()) {
        return;
    }
    setBusy(true);
    m_rpc.call(QStringLiteral("load_ladspalist"), {}, [this](const QJsonValue& r) {
        setBusy(false);
        // [plugins not found any more, plugin descriptions] (see
        // RpcClient::handleLine for the repair of the engine's answer)
        const QJsonArray a = r.toArray();
        if (a.size() < 2 || !a.at(1).isArray()) {
            emit loadFailed();
            return;
        }
        m_notFound.clear();
        for (const QJsonValue& v : a.at(0).toArray()) {
            m_notFound.append(v.toVariant().toString());
        }
        m_plugins = a.at(1).toArray();
        m_saved = m_plugins;
        m_changed.clear();
        m_loaded = true;
        refilter();
        emit loadedChanged();
        emit modifiedChanged();
    });
}

void PluginManager::unload() {
    beginResetModel();
    m_plugins = QJsonArray();
    m_saved = QJsonArray();
    m_changed.clear();
    m_rows.clear();
    m_notFound.clear();
    endResetModel();
    if (m_loaded) {
        m_loaded = false;
        emit loadedChanged();
        emit modifiedChanged();
    }
}

void PluginManager::update(int index, const QJsonObject& p) {
    m_plugins[index] = p;
    if (p == m_saved.at(index).toObject()) {
        m_changed.remove(index);
    } else {
        m_changed.insert(index);
    }
    const int row = int(m_rows.indexOf(index));
    if (row >= 0) {
        emit dataChanged(createIndex(row, 0), createIndex(row, 0));
    }
    emit pluginChanged(index);
    emit modifiedChanged();
}

void PluginManager::setActive(int index, bool active) {
    if (index < 0 || index >= m_plugins.size() || m_busy) {
        return;
    }
    QJsonObject p = m_plugins.at(index).toObject();
    if (bool(intOf(p, "active")) == active) {
        return;
    }
    p.insert(QStringLiteral("active"), active ? 1 : 0);
    update(index, p);
}

void PluginManager::setAllActive(bool active) {
    const QList<int> rows = m_rows;
    for (int i : rows) {
        setActive(i, active);
    }
}

QVariantMap PluginManager::plugin(int index) const {
    if (index < 0 || index >= m_plugins.size()) {
        return {};
    }
    const QJsonObject p = m_plugins.at(index).toObject();
    const bool lv2 = intOf(p, "is_lv2");
    QVariantList ports;
    for (const QJsonValue& v : p.value(QLatin1String("ctrl_ports")).toArray()) {
        const QJsonObject port = v.toObject();
        if (intOf(port, "is_output") || portType(port) == tp_none) {
            continue;
        }
        ports.append(QVariantMap{
            {QStringLiteral("pos"), intOf(port, "pos")},
            {QStringLiteral("name"), portName(port)},
            {QStringLiteral("factoryName"), stringOf(port.value(QLatin1String("factory")).toObject(), "name")},
        });
    }
    return QVariantMap{
        {QStringLiteral("index"), index},
        {QStringLiteral("name"), stringOf(p, "Name")},
        {QStringLiteral("shortname"), stringOf(p, "shortname")},
        {QStringLiteral("maker"), stringOf(p, "Maker")},
        {QStringLiteral("lv2"), lv2},
        {QStringLiteral("stereo"), intOf(p, "tp") == 1},
        {QStringLiteral("source"), lv2 ? stringOf(p, "path")
                                       : QStringLiteral("%1: %2[%3]").arg(p.value(QLatin1String("UniqueID")).toInteger())
                                             .arg(stringOf(p, "path")).arg(intOf(p, "index"))},
        {QStringLiteral("ladspaCategory"), stringOf(p, "ladspa_category")},
        {QStringLiteral("category"), stringOf(p, "category")},
        {QStringLiteral("deducedCategory"), stringOf(p, "deduced_category")},
        {QStringLiteral("quirks"), intOf(p, "quirks")},
        {QStringLiteral("quirksDefault"), intOf(p, "quirks_default")},
        {QStringLiteral("masterIdx"), intOf(p, "MasterIdx")},
        {QStringLiteral("masterLabel"), stringOf(p, "MasterLabel")},
        {QStringLiteral("ports"), ports},
        {QStringLiteral("addWetDry"), bool(intOf(p, "add_wet_dry"))},
        {QStringLiteral("stereoToMono"), bool(intOf(p, "stereo_to_mono"))},
        {QStringLiteral("active"), bool(intOf(p, "active"))},
        {QStringLiteral("hasSettings"), bool(intOf(p, "has_settings"))},
        {QStringLiteral("changed"), m_changed.contains(index)},
    };
}

// PluginDesc::_i_check_has_settings: the settings file of a plugin is kept
// when it is not active
bool PluginManager::computeHasSettings(const QJsonObject& p) {
    if (stringOf(p, "shortname") != stringOf(p, "Name") || stringOf(p, "category") != stringOf(p, "deduced_category")
        || intOf(p, "quirks") != intOf(p, "quirks_default") || intOf(p, "MasterIdx") != -1) {
        return true;
    }
    const QJsonArray ports = p.value(QLatin1String("ctrl_ports")).toArray();
    for (int i = 0; i < ports.size(); ++i) {
        const QJsonObject port = ports.at(i).toObject();
        if (intOf(port, "pos") != i || portHasSettings(port)) {
            return true;
        }
    }
    return false;
}

void PluginManager::setPlugin(int index, const QVariantMap& settings) {
    if (index < 0 || index >= m_plugins.size() || m_busy) {
        return;
    }
    QJsonObject p = m_plugins.at(index).toObject();
    if (settings.contains(QStringLiteral("shortname"))) {
        // PluginDisplay::save_current: no name is the plugin's name
        const QString s = settings.value(QStringLiteral("shortname")).toString().trimmed();
        p.insert(QStringLiteral("shortname"), s.isEmpty() ? stringOf(p, "Name") : s);
    }
    if (settings.contains(QStringLiteral("category"))) {
        p.insert(QStringLiteral("category"), settings.value(QStringLiteral("category")).toString());
    }
    if (settings.contains(QStringLiteral("quirks"))) {
        // only the quirks of the window, never the is_lv2 flag the engine adds
        p.insert(QStringLiteral("quirks"), settings.value(QStringLiteral("quirks")).toInt() & (NeedActivate | NoCleanup));
    }
    if (settings.contains(QStringLiteral("masterIdx"))) {
        p.insert(QStringLiteral("MasterIdx"), std::max(-1, settings.value(QStringLiteral("masterIdx")).toInt()));
    }
    if (settings.contains(QStringLiteral("masterLabel"))) {
        p.insert(QStringLiteral("MasterLabel"), settings.value(QStringLiteral("masterLabel")).toString().trimmed());
    }
    if (intOf(p, "MasterIdx") < 0) {
        p.insert(QStringLiteral("MasterLabel"), QString());
    }
    if (settings.contains(QStringLiteral("addWetDry"))) {
        p.insert(QStringLiteral("add_wet_dry"), settings.value(QStringLiteral("addWetDry")).toBool() ? 1 : 0);
    }
    if (settings.contains(QStringLiteral("stereoToMono"))) {
        p.insert(QStringLiteral("stereo_to_mono"), settings.value(QStringLiteral("stereoToMono")).toBool() ? 1 : 0);
    }
    p.insert(QStringLiteral("has_settings"), computeHasSettings(p) ? 1 : 0);
    update(index, p);
}

void PluginManager::resetPlugin(int index) {
    if (index < 0 || index >= m_plugins.size() || m_busy) {
        return;
    }
    // PluginDesc::reset and PortDesc::reset
    QJsonObject p = m_plugins.at(index).toObject();
    p.insert(QStringLiteral("shortname"), stringOf(p, "Name"));
    p.insert(QStringLiteral("MasterIdx"), -1);
    p.insert(QStringLiteral("MasterLabel"), QString());
    p.insert(QStringLiteral("add_wet_dry"), 0);
    p.insert(QStringLiteral("category"), stringOf(p, "deduced_category"));
    p.insert(QStringLiteral("quirks"), intOf(p, "quirks_default"));
    p.insert(QStringLiteral("has_settings"), 0);
    const QJsonArray ports = p.value(QLatin1String("ctrl_ports")).toArray();
    QList<QJsonObject> sorted;
    for (const QJsonValue& v : ports) {
        sorted.append(v.toObject());
    }
    std::stable_sort(sorted.begin(), sorted.end(), [](const QJsonObject& a, const QJsonObject& b) {
        return intOf(a, "pos") < intOf(b, "pos");
    });
    QJsonArray reset;
    for (QJsonObject port : std::as_const(sorted)) {
        port.insert(QStringLiteral("step"), stepNormal);
        port.insert(QStringLiteral("use_sr"), 0);
        port.insert(QStringLiteral("has_sr"), (intOf(port, "hint_desc") & LADSPA_HINT_SAMPLE_RATE) ? 1 : 0);
        port.insert(QStringLiteral("has_caption"), 1);
        port.insert(QStringLiteral("user"), noUserValues());
        reset.append(port);
    }
    p.insert(QStringLiteral("ctrl_ports"), reset);
    update(index, p);
}

void PluginManager::discard() {
    if (m_busy) {
        return;
    }
    m_plugins = m_saved;
    m_changed.clear();
    refilter();
    emit modifiedChanged();
}

void PluginManager::commit(std::function<void()> done) {
    m_machine.updatePlugins(std::move(done));
}

void PluginManager::apply() {
    if (m_busy || !m_loaded || !m_machine.ready()) {
        return;
    }
    // a plugin changing between stereo and mono must be removed and loaded
    // again (PluginDisplay::on_stereo_to_mono)
    QList<int> reload;
    for (int i : std::as_const(m_changed)) {
        const QJsonObject p = m_plugins.at(i).toObject(), s = m_saved.at(i).toObject();
        if (intOf(p, "active") && intOf(s, "active") && intOf(p, "stereo_to_mono") != intOf(s, "stereo_to_mono")) {
            reload.append(i);
        }
    }
    setBusy(true);
    auto finish = [this]() {
        // PluginDisplay::do_save: the state saved is the new reference
        for (int i = 0; i < m_plugins.size(); ++i) {
            QJsonObject p = m_plugins.at(i).toObject();
            if (intOf(p, "active_set") != intOf(p, "active")) {
                p.insert(QStringLiteral("active_set"), intOf(p, "active"));
                m_plugins[i] = p;
            }
        }
        m_saved = m_plugins;
        m_changed.clear();
        if (!m_rows.isEmpty()) {
            emit dataChanged(createIndex(0, 0), createIndex(int(m_rows.size()) - 1, 0));
        }
        setBusy(false);
        emit modifiedChanged();
        emit applied();
    };
    auto saveAll = [this, finish]() {
        m_rpc.notify(QStringLiteral("save_ladspalist"), {QJsonValue(m_plugins)});
        commit(finish);
    };
    if (reload.isEmpty()) {
        saveAll();
        return;
    }
    QJsonArray without = m_plugins;
    for (int i : std::as_const(reload)) {
        QJsonObject p = without.at(i).toObject();
        p.insert(QStringLiteral("active"), 0);
        without[i] = p;
    }
    m_rpc.notify(QStringLiteral("save_ladspalist"), {QJsonValue(without)});
    commit(saveAll);
}
