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

// Unit presets of the Machine (see the comment in machine.h): the
// plugin_preset_list_* procedures, GxSettings in gx_preset.cpp.
//
// Engine behaviour this relies on (checked against guitarix 0.47):
// - every procedure looks the plugin up without a null check,
// - plugin_preset_list_load answers [[name, is_set], ...]: the LV2 and
//   user presets, an entry with an empty name, then the factory presets,
// - plugin_preset_list_set broadcasts a "set" for each parameter it
//   changes, to the sender too, and then an empty "set",
// - plugin_preset_list_save overwrites a preset of the same name and,
//   like plugin_preset_list_remove, broadcasts nothing,
// - plugin_preset_list_sync_set (used by gx_head for the sequencer) does
//   nothing for other units, ignores the factory flag and waits in a
//   thread for the sequencer to reach its first step: it is not used.

#include "machine.h"

#include <algorithm>

namespace {

QString clean(const QString& name) {
    return name.simplified();
}

} // anonymous namespace

void Machine::loadUnitPresets(const QString& unit, PresetsHandler handler) {
    if (!m_pluginInfo.contains(unit) || !m_rpc.isConnected()) {
        return;
    }
    m_rpc.call(QStringLiteral("plugin_preset_list_load"), {unit}, [handler](const QJsonValue& r) {
        QVariantList presets;
        bool factory = false;
        for (const QJsonValue& v : r.toArray()) {
            const QJsonArray e = v.toArray();
            const QString name = e.at(0).toString();
            if (name.isEmpty()) {
                factory = true;
                continue;
            }
            presets.append(QVariantMap{
                {QStringLiteral("name"), name},
                {QStringLiteral("factory"), factory},
                // an integer: QJsonValue(1).toBool() is false
                {QStringLiteral("active"), e.at(1).toInt() != 0},
            });
        }
        handler(presets);
    });
}

void Machine::requestUnitPresets(const QString& unit) {
    loadUnitPresets(unit, [this, unit](const QVariantList& presets) {
        emit unitPresetsReady(unit, presets);
    });
}

void Machine::setUnitPreset(const QString& unit, const QString& name, bool factory) {
    if (!m_pluginInfo.contains(unit) || name.isEmpty()) {
        return;
    }
    // the parameters come back as "set" notifications
    m_rpc.notify(QStringLiteral("plugin_preset_list_set"), {unit, factory ? 1 : 0, name});
}

QString Machine::saveUnitPreset(const QString& unit, const QString& name) {
    const QString n = clean(name);
    // an empty name would read as the start of the factory presets
    if (n.isEmpty()) {
        return tr("The preset name is empty");
    }
    if (!m_pluginInfo.contains(unit)) {
        return tr("Unknown unit %1").arg(unit);
    }
    // an existing preset is overwritten, the UI asks first
    m_rpc.notify(QStringLiteral("plugin_preset_list_save"), {unit, n});
    requestUnitPresets(unit);
    return QString();
}

QString Machine::removeUnitPreset(const QString& unit, const QString& name) {
    if (!m_pluginInfo.contains(unit) || name.isEmpty()) {
        return tr("Preset \"%1\" cannot be removed").arg(name);
    }
    m_rpc.notify(QStringLiteral("plugin_preset_list_remove"), {unit, name});
    requestUnitPresets(unit);
    return QString();
}

// SEQWindow::on_next_preset_set and on_previus_preset_set
void Machine::stepUnitPreset(const QString& unit, int delta) {
    loadUnitPresets(unit, [this, unit, delta](const QVariantList& presets) {
        QStringList names;
        int current = -1;
        for (const QVariant& v : presets) {
            const QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("factory")).toBool()) {
                break;
            }
            if (current < 0 && m.value(QStringLiteral("active")).toBool()) {
                current = names.size();
            }
            names.append(m.value(QStringLiteral("name")).toString());
        }
        if (names.isEmpty()) {
            return;
        }
        const int n = names.size();
        const int i = current < 0 ? (delta > 0 ? 0 : n - 1) : ((current + delta) % n + n) % n;
        setUnitPreset(unit, names.at(i), false);
    });
}

// the parameters of ParamMap::reset_unit, without those of other groups;
// JSON values (convolver, sequencer lines) have no standard value here
QList<Parameter*> Machine::unitPresetParameters(const QString& unit) const {
    QList<Parameter*> l;
    for (Parameter *p : unitParameters(unit)) {
        const QString key = p->id().mid(unit.size() + 1);
        if (p->isInPreset() && !p->isOutput() && p->kind() != Parameter::Other
            && key != QLatin1String("on_off") && key != QLatin1String("pp") && key != QLatin1String("position")) {
            l.append(p);
        }
    }
    return l;
}

void Machine::resetUnit(const QString& unit) {
    for (Parameter *p : unitPresetParameters(unit)) {
        if (!p->isStd()) {
            p->reset();
        }
    }
}

bool Machine::unitHasStdValues(const QString& unit) const {
    const QList<Parameter*> l = unitPresetParameters(unit);
    return std::all_of(l.begin(), l.end(), [](const Parameter *p) { return p->isStd(); });
}

// seq.npreset and seq.ppreset: buttons of the sequencer, possibly
// controlled by MIDI; gx_head's GUI loads the preset and sets them back
void Machine::onSequencerPresetStep(Parameter *p, int delta) {
    if (p->value().toDouble() == 0) {
        return;
    }
    p->setValue(0);
    stepUnitPreset(QStringLiteral("seq"), delta);
}
