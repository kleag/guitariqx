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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif

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
        whenRackSettled([this, bankName, n, done]() {
            m_rpc.notify(QStringLiteral("save_preset"), {bankName, n});
            refreshPresetState();
            done();
        });
    });
    return QString();
}

void Machine::saveNewPreset(const QString& bankName, const QString& name,
                            std::function<void(const QString&, const QString&)> done) {
    const QString base = clean(name).isEmpty() ? QStringLiteral("Generated") : clean(name);
    enqueueEdit([this, bankName, base, done](EditDone editDone) {
        // the bank may have been changed by another client: unknown bank
        // names crash the engine, duplicate preset names corrupt its state
        m_rpc.call(QStringLiteral("banks"), {}, [this, bankName, base, done, editDone](const QJsonValue& r) {
            m_banks = r.toArray().toVariantList();
            emit banksChanged();
            if (!bankMutable(bankName)) {
                done(QString(), bank(bankName).isEmpty() ? tr("Bank \"%1\" not found").arg(bankName)
                                                         : tr("Bank \"%1\" is read only").arg(bankName));
                editDone();
                return;
            }
            QString n = base;
            for (int i = 2; presetExists(bankName, n); ++i) {
                n = base + QLatin1Char(' ') + QString::number(i);
            }
            whenRackSettled([this, bankName, n, done, editDone]() {
                m_rpc.notify(QStringLiteral("save_preset"), {bankName, n});
                refreshPresetState();
                done(n, QString());
                editDone();
            });
        });
    });
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

// Last preset: with restorePreset on, the current preset is saved on every
// change and loaded again on each connection to the same engine. The engine
// saves its own state only after 10 to 30 s without change (and only with
// autosave), so after a power cut (a pedalboard unplugged) or a crash it
// would come back with an older preset.
//
// The file is replaced atomically (QSaveFile: written, flushed to disk,
// then renamed over the old one, and the directory is flushed too), so a
// power cut at any moment leaves either the previous preset or the new
// one, never a truncated file.

QString Machine::lastPresetFile() {
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QStringLiteral("/last-preset.json");
}

void Machine::onPresetChangedForSave() {
    // "" / "" when the engine settings are not a preset (preset_changed)
    if (!m_restorePreset || !m_presetSaveOpen || m_currentBank.isEmpty() || m_currentPreset.isEmpty()) {
        return;
    }
    m_pendingSaveBank = m_currentBank;
    m_pendingSavePreset = m_currentPreset;
    m_presetSaveTimer.start();
}

void Machine::saveLastPreset() {
    m_presetSaveTimer.stop();
    if (m_pendingSaveBank.isEmpty()
        || (m_pendingSaveBank == m_savedBank && m_pendingSavePreset == m_savedPreset)) {
        return;
    }
    const QString file = lastPresetFile();
    const QString dir = QFileInfo(file).absolutePath();
    QDir().mkpath(dir);
    const QJsonObject o{
        {QStringLiteral("host"), m_rpc.host()},
        {QStringLiteral("port"), int(m_rpc.port())},
        {QStringLiteral("bank"), m_pendingSaveBank},
        {QStringLiteral("preset"), m_pendingSavePreset},
    };
    const QByteArray data = QJsonDocument(o).toJson(QJsonDocument::Compact);
    QSaveFile out(file);
    // commit() flushes the data to disk before renaming
    if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit()) {
        qWarning() << "cannot save the last preset to" << file << ":" << out.errorString();
        return;
    }
#ifdef Q_OS_UNIX
    // the rename itself is only durable once the directory is flushed
    const int fd = ::open(QFile::encodeName(dir).constData(), O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        ::fsync(fd);
        ::close(fd);
    }
#endif
    m_savedBank = m_pendingSaveBank;
    m_savedPreset = m_pendingSavePreset;
}

void Machine::restoreLastPreset() {
    // the engine's current preset was read with the parameters (banks are
    // known too: the server answers in order)
    m_presetSaveOpen = true;
    if (!m_restorePreset) {
        return;
    }
    QFile in(lastPresetFile());
    QJsonObject o;
    if (in.open(QIODevice::ReadOnly)) {
        o = QJsonDocument::fromJson(in.readAll()).object();
    }
    const QString bank = o.value(QStringLiteral("bank")).toString();
    const QString preset = o.value(QStringLiteral("preset")).toString();
    if (o.value(QStringLiteral("host")).toString() != m_rpc.host()
        || o.value(QStringLiteral("port")).toInt() != int(m_rpc.port()) || bank.isEmpty()
        || preset.isEmpty()) {
        // nothing saved for this engine: start from its current preset
        onPresetChangedForSave();
        return;
    }
    m_savedBank = bank;
    m_savedPreset = preset;
    if (bank == m_currentBank && preset == m_currentPreset) {
        // loading it again would reset what was changed since
        return;
    }
    // an unknown bank name would crash the engine
    if (!presetExists(bank, preset)) {
        qWarning() << "last preset" << bank << "/" << preset << "no longer exists";
        onPresetChangedForSave();
        return;
    }
    setPreset(bank, preset);
}

// Automatic save of the current preset
//
// The engine does not know whether the current preset was changed, so a
// snapshot of what it saves in a preset (and of the MIDI map when it is
// saved with presets) is taken once a preset has loaded. The engine sends
// the loaded values as "set" notifications before preset_changed (they
// only restart m_autoSaveTimer, which preset_changed stops); the preset is
// settled when no "set" came for m_presetSettleTimer after preset_changed.
// Every later change (from this UI,
// another one, a MIDI controller, a unit preset) starts m_autoSaveTimer,
// and the preset is saved with save_current if it then differs from the
// snapshot. save_current saves the engine's state into the engine's
// current preset, so a save racing with a preset change made elsewhere
// cannot write one preset's settings into another; changes not yet saved
// when another client or a MIDI program change loads a preset are lost.

void Machine::onPresetChangedForAutoSave() {
    // the changes belonged to the previous preset (or to the one just
    // saved under a new name)
    m_autoSaveTimer.stop();
    m_presetSettling = true;
    m_presetSettleTimer.start();
}

void Machine::onPresetSettled() {
    m_presetSettling = false;
    m_presetSnapshot = presetSnapshot();
    m_snapshotBank = m_currentBank;
    m_snapshotPreset = m_currentPreset;
}

void Machine::presetEdited() {
    if (m_presetSettling) {
        // still loading
        m_presetSettleTimer.start();
        return;
    }
    if (m_autoSavePreset && m_ready) {
        m_autoSaveTimer.start();
    }
}

QHash<QString, QVariant> Machine::presetSnapshot() const {
    QHash<QString, QVariant> s;
    for (auto it = m_params.cbegin(); it != m_params.cend(); ++it) {
        const Parameter *p = it.value();
        if (p->isSavedInPreset() && !p->isOutput() && p->tag() != QLatin1String("Osc")) {
            s.insert(it.key(), p->value());
        }
    }
    if (m_midi.inPresets()) {
        // not a parameter id
        s.insert(QStringLiteral(" midi"), m_midi.assignments());
    }
    return s;
}

void Machine::autoSaveNow() {
    m_autoSaveTimer.stop();
    if (!m_autoSavePreset || !m_ready || m_presetSettling || m_currentPreset.isEmpty()
        || m_currentBank != m_snapshotBank || m_currentPreset != m_snapshotPreset
        || !presetExists(m_currentBank, m_currentPreset) || !bankMutable(m_currentBank)) {
        return;
    }
    const QHash<QString, QVariant> s = presetSnapshot();
    if (s == m_presetSnapshot) {
        // changed and changed back
        return;
    }
    // not queued with the edits: it must reach the engine before a
    // setpreset sent next, and does not depend on the bank list
    m_rpc.notify(QStringLiteral("save_current"));
    m_presetSnapshot = s;
}
