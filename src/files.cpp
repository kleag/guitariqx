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

// Files on the engine's computer: places for the file chooser, remote
// directory listing and loading impulse responses.

#include "machine.h"

#include <QDateTime>
#include <QJsonObject>
#include <cmath>

namespace {

// string parameters holding the folder of the current model
const char *modelPathParams[][2] = {
    {"nam.loadpath", "NAM models"},
    {"snam.loadpath", "NAM models (stereo)"},
    {"mnam.loadapath", "NAM models A"},
    {"mnam.loadbpath", "NAM models B"},
    {"rtneural.loadpath", "RTNeural models"},
    {"srtneural.loadpath", "RTNeural models (stereo)"},
    {"mrtneural.loadapath", "RTNeural models A"},
    {"mrtneural.loadbpath", "RTNeural models B"},
};

} // anonymous namespace

void Machine::requestFilePlaces() {
    QVariantList places;
    auto add = [&places](const QString& name, const QString& path) {
        if (path.isEmpty()) {
            return;
        }
        for (const QVariant& p : std::as_const(places)) {
            if (p.toMap().value(QStringLiteral("path")).toString() == path) {
                return;
            }
        }
        places.append(QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("path"), path}});
    };
    for (const auto& mp : modelPathParams) {
        if (Parameter *p = m_params.value(QString::fromLatin1(mp[0]))) {
            add(tr(mp[1]), p->value().toString());
        }
    }
    m_filePlaces = places;
    emit filePlacesChanged();
    // IR folders of the engine (user folder and the factory sounds)
    m_rpc.call(QStringLiteral("load_impresp_dirs"), {}, [this, places](const QJsonValue& r) mutable {
        for (const QJsonValue& v : r.toArray()) {
            const QJsonArray d = v.toArray();
            const QString path = d.at(0).toString();
            const QString name = d.at(1).toString().trimmed();
            // the top level folders come first (user, then factory: the
            // "%S" of IR settings), their sub folders are named relatively
            if (name == path) {
                m_irFactoryDir = path;
            }
            bool seen = false;
            for (const QVariant& p : std::as_const(places)) {
                seen = seen || p.toMap().value(QStringLiteral("path")).toString() == path;
            }
            if (!seen) {
                places.append(QVariantMap{
                    {QStringLiteral("name"), tr("IR: %1").arg(name.section(QLatin1Char('/'), -1))},
                    {QStringLiteral("path"), path},
                });
            }
        }
        if (remoteFiles()) {
            // home and guitarix folders of the engine's computer
            m_rpc.call(QStringLiteral("get_file_places"), {}, [this, places](const QJsonValue& r) mutable {
                QVariantList front;
                for (const QJsonValue& v : r.toArray()) {
                    const QJsonArray d = v.toArray();
                    front.append(QVariantMap{
                        {QStringLiteral("name"), d.at(0).toString()},
                        {QStringLiteral("path"), d.at(1).toString()},
                    });
                }
                m_filePlaces = front + places;
                emit filePlacesChanged();
            });
            return;
        }
        m_filePlaces = places;
        emit filePlacesChanged();
    });
}

// list_directory [path] -> {"path": path, "entries": [[name, is_dir, size, mtime], ...]}
void Machine::listDirectory(const QString& path, ListHandler handler) {
    if (!m_rpc.isConnected() || !remoteFiles()) {
        handler(QString(), {}, tr("Not connected"));
        return;
    }
    m_rpc.call(QStringLiteral("list_directory"), {path}, [handler](const QJsonValue& r) {
        const QJsonObject o = r.toObject();
        if (o.isEmpty()) {
            handler(QString(), {}, tr("The engine cannot read this folder"));
            return;
        }
        QVector<DirModel::Entry> entries;
        for (const QJsonValue& v : o.value(QStringLiteral("entries")).toArray()) {
            const QJsonArray e = v.toArray();
            entries.append({e.at(0).toString(), e.at(1).toInt() != 0, qint64(e.at(2).toDouble()),
                            QDateTime::fromSecsSinceEpoch(qint64(e.at(3).toDouble()))});
        }
        handler(o.value(QStringLiteral("path")).toString(), entries, o.value(QStringLiteral("error")).toString());
    });
}

void Machine::loadImpulseResponse(const QString& unit, const QString& file) {
    const QString id = unit + QStringLiteral(".convolver");
    if (!m_params.contains(id)) {
        emit impulseResponseLoaded(unit, file, tr("No convolver in unit %1").arg(unit));
        return;
    }
    // read_audio [file] -> [size, channels, type, form, rate, [samples...]]
    m_rpc.call(QStringLiteral("read_audio"), {file}, [this, unit, file, id](const QJsonValue& r) {
        const QJsonArray a = r.toArray();
        if (a.size() < 6) {
            emit impulseResponseLoaded(unit, file, tr("Cannot read the audio file %1").arg(file));
            return;
        }
        const qint64 frames = qint64(a.at(0).toDouble());
        const int channels = a.at(1).toInt();
        const QJsonArray samples = a.at(5).toArray();
        // IRWindow::calc_normalized_gain with an empty gain line
        double sum = 0;
        for (const QJsonValue& s : samples) {
            const double v = s.toDouble();
            sum += v * v;
        }
        double gain = channels > 0 ? std::sqrt(sum / channels) : 0;
        gain = gain != 0 ? 1 / gain : 1;
        const int slash = file.lastIndexOf(QLatin1Char('/'));
        // like IRWindow::on_reset_clicked: whole file, no delay, flat gain line
        QJsonObject settings{
            {QStringLiteral("jconv.IRFile"), file.mid(slash + 1)},
            {QStringLiteral("jconv.IRDir"), file.left(slash)},
            {QStringLiteral("jconv.Gain"), gain},
            {QStringLiteral("jconv.GainCor"), 1},
            {QStringLiteral("jconv.Offset"), 0},
            {QStringLiteral("jconv.Length"), frames},
            {QStringLiteral("jconv.Delay"), 0},
            {QStringLiteral("jconv.gainline"), QJsonArray()},
        };
        m_rpc.notify(QStringLiteral("set"), {id, settings});
        if (Parameter *p = m_params.value(id)) {
            p->updateFromEngine(settings);
        }
        // like Apply in gx_head's IR window, switch the unit on
        if (Parameter *on = m_params.value(unit + QStringLiteral(".on_off"))) {
            on->setValue(true);
        }
        emit impulseResponseLoaded(unit, file, QString());
    });
}

// The neural units load the model selected by index in their own listing
// of a folder: *.loadpath is the folder (setting it makes the engine list
// it), *.flist the index; *.loadfile only records the file. Same sequence
// as StackBoxBuilder::set_neural_filelist in gx_head.
void Machine::loadModel(const QString& fileParam, const QString& file) {
    QString pathParam;
    QString listParam;
    for (const char *ab : {"", "a", "b"}) {
        const QString x = QString::fromLatin1(ab);
        const QString suffix = QStringLiteral(".load%1file").arg(x);
        if (fileParam.endsWith(suffix)) {
            const QString unit = fileParam.left(fileParam.size() - suffix.size());
            pathParam = unit + QStringLiteral(".load%1path").arg(x);
            listParam = unit + (x.isEmpty() ? QStringLiteral(".flist") : QStringLiteral(".f%1list").arg(x));
        }
    }
    if (!m_params.contains(fileParam) || !m_params.contains(pathParam) || !m_params.contains(listParam)) {
        emit modelLoaded(fileParam, file, tr("Unknown model parameter %1").arg(fileParam));
        return;
    }
    const int slash = file.lastIndexOf(QLatin1Char('/'));
    const QString name = file.mid(slash + 1);
    m_params.value(fileParam)->setValue(file);
    m_params.value(pathParam)->setValue(file.left(slash));
    m_rpc.call(QStringLiteral("get_file_list"), {listParam}, [this, fileParam, file, name, listParam](const QJsonValue& r) {
        const QJsonArray names = r.toArray();
        int index = -1;
        for (int i = 1; i < names.size(); ++i) {
            if (names.at(i).toString() == name) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            emit modelLoaded(fileParam, file, tr("The engine does not list %1 in its folder").arg(name));
            return;
        }
        Parameter *list = m_params.value(listParam);
        // the engine loads when the index changes
        if (int(list->value().toDouble()) == index) {
            list->setValue(0);
        }
        list->setValue(index);
        emit modelLoaded(fileParam, file, QString());
    });
}
