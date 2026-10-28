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

// Preset and bank editing of the Machine (see the comment in machine.h).
//
// Engine behaviour this relies on (checked against guitarix 0.47):
// - the bank and preset procedures look the bank up without a null check,
// - rename_preset accepts a name that already exists in the bank,
// - reorder_preset and bank_reorder need the complete list,
// - only bank_insert_new broadcasts presetlist_changed, and changes of the
//   current preset caused by these calls are not broadcast at all.

#include "machine.h"

namespace {

QString clean(const QString& name) {
    return name.simplified();
}

} // anonymous namespace

QVariantMap Machine::bank(const QString& name) const {
    for (const QVariant& b : m_banks) {
        const QVariantMap m = b.toMap();
        if (m.value(QStringLiteral("name")).toString() == name) {
            return m;
        }
    }
    return QVariantMap();
}

bool Machine::bankMutable(const QString& name) const {
    const QVariantMap b = bank(name);
    return !b.isEmpty() && b.value(QStringLiteral("mutable")).toInt() != 0;
}

QStringList Machine::writableBanks() const {
    QStringList l;
    for (const QVariant& b : m_banks) {
        const QVariantMap m = b.toMap();
        if (m.value(QStringLiteral("mutable")).toInt()) {
            l.append(m.value(QStringLiteral("name")).toString());
        }
    }
    return l;
}

bool Machine::presetExists(const QString& bankName, const QString& preset) const {
    return bank(bankName).value(QStringLiteral("presets")).toStringList().contains(preset);
}

void Machine::enqueueEdit(std::function<void(EditDone)> op) {
    m_editQueue.append(std::move(op));
    if (!m_editRunning) {
        runNextEdit();
    }
}

void Machine::runNextEdit() {
    if (m_editQueue.isEmpty() || !m_rpc.isConnected()) {
        m_editQueue.clear();
        m_editRunning = false;
        return;
    }
    m_editRunning = true;
    auto op = m_editQueue.takeFirst();
    op([this]() { runNextEdit(); });
}

void Machine::refreshPresetState() {
    requestBanks();
    m_rpc.call(QStringLiteral("get"), {QStringLiteral("system.current_bank"), QStringLiteral("system.current_preset")},
               [this](const QJsonValue& r) {
        const QJsonObject o = r.toObject();
        for (const char *id : {"system.current_bank", "system.current_preset"}) {
            if (Parameter *par = m_params.value(QString::fromLatin1(id))) {
                par->updateFromEngine(o.value(QString::fromLatin1(id)));
            }
        }
        const QString b = o.value(QStringLiteral("system.current_bank")).toString();
        const QString p = o.value(QStringLiteral("system.current_preset")).toString();
        if (b != m_currentBank || p != m_currentPreset) {
            m_currentBank = b;
            m_currentPreset = p;
            emit presetChanged();
        }
    });
}

QString Machine::saveCurrent() {
    if (m_currentPreset.isEmpty() || !presetExists(m_currentBank, m_currentPreset)) {
        return tr("No preset is loaded");
    }
    if (!bankMutable(m_currentBank)) {
        return tr("Bank \"%1\" is read only, use Save as").arg(m_currentBank);
    }
    enqueueEdit([this](EditDone done) {
        m_rpc.notify(QStringLiteral("save_current"));
        refreshPresetState();
        done();
    });
    return QString();
}

QString Machine::savePreset(const QString& bankName, const QString& name) {
    const QString n = clean(name);
    if (n.isEmpty()) {
        return tr("The preset name is empty");
    }
    // save_preset would create an unknown bank, but newBank() is used for
    // that so that the UI knows the name the engine chose
    if (!bankMutable(bankName)) {
        return tr("Bank \"%1\" is read only").arg(bankName);
    }
    // an existing preset is overwritten, the UI asks first
    enqueueEdit([this, bankName, n](EditDone done) {
        m_rpc.notify(QStringLiteral("save_preset"), {bankName, n});
        refreshPresetState();
        done();
    });
    return QString();
}

QString Machine::renamePreset(const QString& bankName, const QString& oldName, const QString& newName) {
    const QString n = clean(newName);
    if (!bankMutable(bankName) || !presetExists(bankName, oldName)) {
        return tr("Preset \"%1\" cannot be renamed").arg(oldName);
    }
    if (n.isEmpty()) {
        return tr("The preset name is empty");
    }
    if (n == oldName) {
        return QString();
    }
    if (presetExists(bankName, n)) {
        return tr("A preset named \"%1\" already exists in this bank").arg(n);
    }
    enqueueEdit([this, bankName, oldName, n](EditDone done) {
        m_rpc.call(QStringLiteral("rename_preset"), {bankName, oldName, n}, [this, done](const QJsonValue&) {
            refreshPresetState();
            done();
        });
    });
    return QString();
}

QString Machine::erasePreset(const QString& bankName, const QString& name) {
    if (!bankMutable(bankName) || !presetExists(bankName, name)) {
        return tr("Preset \"%1\" cannot be deleted").arg(name);
    }
    enqueueEdit([this, bankName, name](EditDone done) {
        m_rpc.notify(QStringLiteral("erase_preset"), {bankName, name});
        refreshPresetState();
        done();
    });
    return QString();
}

QString Machine::movePreset(const QString& bankName, const QString& name, int delta) {
    if (!bankMutable(bankName) || !presetExists(bankName, name)) {
        return tr("Preset \"%1\" cannot be moved").arg(name);
    }
    // reorder_preset needs exactly the presets of the bank: another client
    // may have changed them, so get the current list first
    enqueueEdit([this, bankName, name, delta](EditDone done) {
        m_rpc.call(QStringLiteral("presets"), {bankName}, [this, bankName, name, delta, done](const QJsonValue& r) {
            QStringList l;
            for (const QJsonValue& v : r.toArray()) {
                l.append(v.toString());
            }
            const int i = l.indexOf(name);
            const int j = i + delta;
            if (i >= 0 && j >= 0 && j < l.size() && l.count(name) == 1) {
                l.move(i, j);
                QJsonArray params{bankName};
                for (const QString& p : l) {
                    params.append(p);
                }
                m_rpc.notify(QStringLiteral("reorder_preset"), params);
            }
            refreshPresetState();
            done();
        });
    });
    return QString();
}

QString Machine::copyPreset(const QString& fromBank, const QString& preset,
                            const QString& toBank, const QString& newName) {
    const QString n = clean(newName);
    if (bank(fromBank).isEmpty() || !presetExists(fromBank, preset)) {
        return tr("Preset \"%1\" not found").arg(preset);
    }
    if (!bankMutable(toBank)) {
        return tr("Bank \"%1\" is read only").arg(toBank);
    }
    if (n.isEmpty()) {
        return tr("The preset name is empty");
    }
    if (presetExists(toBank, n)) {
        return tr("A preset named \"%1\" already exists in \"%2\"").arg(n, toBank);
    }
    enqueueEdit([this, fromBank, preset, toBank, n](EditDone done) {
        m_rpc.notify(QStringLiteral("pf_append"), {fromBank, preset, toBank, n});
        refreshPresetState();
        done();
    });
    return QString();
}

QString Machine::newBank(const QString& name) {
    const QString n = clean(name);
    if (n.isEmpty()) {
        return tr("The bank name is empty");
    }
    // the engine makes the name unique
    enqueueEdit([this, n](EditDone done) {
        m_rpc.call(QStringLiteral("bank_insert_new"), {n}, [this, done](const QJsonValue& r) {
            refreshPresetState();
            const QString created = r.toObject().value(QStringLiteral("name")).toString();
            if (!created.isEmpty()) {
                emit bankCreated(created);
            }
            done();
        });
    });
    return QString();
}

QString Machine::renameBank(const QString& oldName, const QString& newName) {
    const QString n = clean(newName);
    if (!bankMutable(oldName)) {
        return tr("Bank \"%1\" cannot be renamed").arg(oldName);
    }
    if (n.isEmpty()) {
        return tr("The bank name is empty");
    }
    if (n == oldName) {
        return QString();
    }
    if (!bank(n).isEmpty()) {
        return tr("A bank named \"%1\" already exists").arg(n);
    }
    enqueueEdit([this, oldName, n](EditDone done) {
        m_rpc.call(QStringLiteral("rename_bank"), {oldName, n}, [this, done](const QJsonValue&) {
            refreshPresetState();
            done();
        });
    });
    return QString();
}

QString Machine::removeBank(const QString& name) {
    const QVariantMap b = bank(name);
    // only user banks: not the factory banks nor the scratch pad
    if (b.isEmpty() || b.value(QStringLiteral("type")).toString() != QLatin1String("file")) {
        return tr("Bank \"%1\" cannot be deleted").arg(name);
    }
    enqueueEdit([this, name](EditDone done) {
        m_rpc.call(QStringLiteral("bank_remove"), {name}, [this, done](const QJsonValue&) {
            refreshPresetState();
            done();
        });
    });
    return QString();
}

QString Machine::moveBank(const QString& name, int delta) {
    if (bank(name).isEmpty()) {
        return tr("Bank \"%1\" not found").arg(name);
    }
    // like movePreset: bank_reorder needs the complete, current list
    enqueueEdit([this, name, delta](EditDone done) {
        m_rpc.call(QStringLiteral("banks"), {}, [this, name, delta, done](const QJsonValue& r) {
            QStringList l;
            for (const QJsonValue& v : r.toArray()) {
                l.append(v.toObject().value(QStringLiteral("name")).toString());
            }
            const int i = l.indexOf(name);
            const int j = i + delta;
            if (i >= 0 && j >= 0 && j < l.size()) {
                l.move(i, j);
                QJsonArray params;
                for (const QString& b : l) {
                    params.append(b);
                }
                m_rpc.notify(QStringLiteral("bank_reorder"), params);
            }
            refreshPresetState();
            done();
        });
    });
    return QString();
}
