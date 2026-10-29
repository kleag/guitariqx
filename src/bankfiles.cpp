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

// Loading and saving bank files (.gx) of the computer guitariqx runs on.
// Like GxMachineRemote in gx_head, the contents travel over the
// connection, so this works with an engine on another computer.

#include "machine.h"

#include <QFile>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUrl>

namespace {

// bank files are small JSON files; refuse anything that is clearly not one
const qint64 maxBankFileSize = 16 * 1024 * 1024;

} // anonymous namespace

QString Machine::bankFileName(const QString& name) const {
    QString f = name.simplified();
    f.replace(QLatin1Char('/'), QLatin1Char('-'));
    return f.isEmpty() ? QStringLiteral("bank.gx") : f + QStringLiteral(".gx");
}

QString Machine::importBank(const QString& file) {
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        return tr("Cannot read %1: %2").arg(file, f.errorString());
    }
    if (f.size() > maxBankFileSize) {
        return tr("%1 is too large for a bank file").arg(file);
    }
    const QByteArray content = f.readAll();
    // the engine checks the contents too (and removes a file it cannot
    // open), but a quick check gives a better message
    if (!QJsonDocument::fromJson(content).isArray()) {
        return tr("%1 is not a guitarix bank file").arg(file);
    }
    // the engine names the bank after the file (bank_insert_content, made
    // unique) and inserts it at the top
    const QString uri = QUrl::fromLocalFile(file).toString();
    const QString text = QString::fromUtf8(content);
    enqueueEdit([this, file, uri, text](EditDone done) {
        m_rpc.call(QStringLiteral("bank_insert_content"), {uri, text, 0}, [this, file, done](const QJsonValue& r) {
            const QString name = r.toObject().value(QStringLiteral("name")).toString();
            refreshPresetState();
            emit bankImported(file, name, name.isEmpty() ? tr("The engine could not load %1").arg(file) : QString());
            done();
        });
    });
    return QString();
}

QString Machine::exportBank(const QString& name, const QString& file) {
    // bank_get_contents crashes the engine for unknown banks
    if (bank(name).isEmpty()) {
        return tr("Bank \"%1\" not found").arg(name);
    }
    if (file.isEmpty()) {
        return tr("No file name");
    }
    enqueueEdit([this, name, file](EditDone done) {
        // -> [file name on the engine's computer, contents]
        m_rpc.call(QStringLiteral("bank_get_contents"), {name}, [this, name, file, done](const QJsonValue& r) {
            const QJsonArray a = r.toArray();
            QString error;
            if (a.size() < 2) {
                error = tr("The engine did not send the bank \"%1\"").arg(name);
            } else {
                QSaveFile out(file);
                const QByteArray data = a.at(1).toString().toUtf8();
                if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit()) {
                    error = tr("Cannot write %1: %2").arg(file, out.errorString());
                }
            }
            emit bankExported(name, file, error);
            done();
        });
    });
    return QString();
}
