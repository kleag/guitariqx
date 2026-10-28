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

#include "dirmodel.h"
#include "machine.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QStorageInfo>
#include <algorithm>

namespace {

const int maxRecent = 8;

QString settingsKey(const QString& kind) {
    return QStringLiteral("files/recent_%1").arg(kind.isEmpty() ? QStringLiteral("any") : kind);
}

} // anonymous namespace

DirModel::DirModel(QObject *parent)
    : QAbstractListModel(parent) {
    if (Machine *m = Machine::instance()) {
        connect(m, &Machine::readyChanged, this, [this]() {
            emit availableChanged();
            updatePlaces();
        });
        connect(m, &Machine::filePlacesChanged, this, &DirModel::updatePlaces);
    }
    updatePlaces();
}

int DirModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant DirModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_entries.size()) {
        return QVariant();
    }
    const Entry& e = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return e.name;
    case PathRole:
        return m_path == QLatin1String("/") ? m_path + e.name : m_path + QLatin1Char('/') + e.name;
    case IsDirRole:
        return e.isDir;
    case SizeRole:
        return e.size;
    case ModifiedRole:
        return e.modified;
    }
    return QVariant();
}

QHash<int, QByteArray> DirModel::roleNames() const {
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {IsDirRole, "isDir"},
        {SizeRole, "size"},
        {ModifiedRole, "modified"},
    };
}

bool DirModel::available() const {
    Machine *m = Machine::instance();
    return m_local || !m || m->engineLocal() || (m->ready() && m->remoteFiles());
}

void DirModel::setPath(const QString& p) {
    QString np = QDir::cleanPath(p.isEmpty() ? QStringLiteral("/") : p);
    if (np == m_path && !m_all.isEmpty()) {
        return;
    }
    m_path = np;
    emit pathChanged();
    list();
}

void DirModel::setSuffixes(const QStringList& s) {
    QStringList l;
    for (const QString& x : s) {
        l.append(x.toLower().remove(QLatin1Char('.')));
    }
    if (l == m_suffixes) {
        return;
    }
    m_suffixes = l;
    emit filterChanged();
    setEntries(m_all, m_error);
}

void DirModel::setShowAll(bool on) {
    if (on == m_showAll) {
        return;
    }
    m_showAll = on;
    emit filterChanged();
    setEntries(m_all, m_error);
}

void DirModel::setShowHidden(bool on) {
    if (on == m_showHidden) {
        return;
    }
    m_showHidden = on;
    emit filterChanged();
    setEntries(m_all, m_error);
}

void DirModel::setKind(const QString& k) {
    if (k == m_kind) {
        return;
    }
    m_kind = k;
    emit kindChanged();
    updatePlaces();
}

void DirModel::setLocal(bool on) {
    if (on == m_local) {
        return;
    }
    m_local = on;
    emit kindChanged();
    updatePlaces();
    if (!m_path.isEmpty()) {
        list();
    }
}

QStringList DirModel::pathParts() const {
    return m_path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
}

QString DirModel::partPath(int n) const {
    const QStringList parts = pathParts();
    return QLatin1Char('/') + parts.mid(0, n).join(QLatin1Char('/'));
}

bool DirModel::canGoUp() const {
    return m_path != QLatin1String("/") && !m_path.isEmpty();
}

void DirModel::cdUp() {
    if (canGoUp()) {
        setPath(dirName(m_path));
    }
}

QString DirModel::fileName(const QString& path) {
    return path.section(QLatin1Char('/'), -1);
}

QString DirModel::dirName(const QString& path) {
    const int i = path.lastIndexOf(QLatin1Char('/'));
    return i <= 0 ? QStringLiteral("/") : path.left(i);
}

QString DirModel::formatSize(qint64 size) {
    if (size < 1024) {
        return tr("%1 B").arg(size);
    }
    if (size < 1024 * 1024) {
        return tr("%1 KiB").arg(size / 1024.0, 0, 'f', 1);
    }
    return tr("%1 MiB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
}

void DirModel::refresh() {
    list();
}

void DirModel::openAt(const QString& p) {
    // recent folders may have been added by another chooser of this kind
    updatePlaces();
    const QString np = QDir::cleanPath(p.isEmpty() ? QStringLiteral("/") : p);
    if (np != m_path) {
        m_path = np;
        emit pathChanged();
    }
    list();
}

bool DirModel::contains(const QString& name) const {
    for (const Entry& e : m_all) {
        if (e.name == name) {
            return true;
        }
    }
    return false;
}

void DirModel::list() {
    Machine *m = Machine::instance();
    const int request = ++m_request;
    if (m && !m->engineLocal() && !m_local) {
        if (!m->ready() || !m->remoteFiles()) {
            setEntries({}, tr("The engine runs on another computer and is too old to list its files "
                              "(it needs JSON-RPC interface 1.2)."));
            return;
        }
        m_busy = true;
        emit busyChanged();
        m->listDirectory(m_path, [this, request](const QString& path, const QVector<Entry>& entries,
                                                 const QString& err) {
            if (request != m_request) {
                return; // a newer listing was requested
            }
            m_busy = false;
            emit busyChanged();
            if (!path.isEmpty() && path != m_path) {
                m_path = path;
                emit pathChanged();
            }
            setEntries(entries, err);
        });
        return;
    }
    QDir dir(m_path);
    if (!dir.exists()) {
        setEntries({}, tr("Folder not found: %1").arg(m_path));
        return;
    }
    QVector<Entry> entries;
    const QFileInfoList infos = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& fi : infos) {
        if (!fi.isDir() && !fi.isFile()) {
            continue;
        }
        entries.append({fi.fileName(), fi.isDir(), fi.isDir() ? 0 : fi.size(), fi.lastModified()});
    }
    setEntries(entries, dir.isReadable() ? QString() : tr("Folder not readable: %1").arg(m_path));
}

bool DirModel::accepted(const Entry& e) const {
    if (!m_showHidden && e.name.startsWith(QLatin1Char('.'))) {
        return false;
    }
    if (e.isDir || m_showAll || m_suffixes.isEmpty()) {
        return true;
    }
    return m_suffixes.contains(e.name.section(QLatin1Char('.'), -1).toLower());
}

void DirModel::setEntries(QVector<Entry> entries, const QString& error) {
    QCollator coll;
    coll.setNumericMode(true);
    coll.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(entries.begin(), entries.end(), [&coll](const Entry& a, const Entry& b) {
        if (a.isDir != b.isDir) {
            return a.isDir;
        }
        return coll.compare(a.name, b.name) < 0;
    });
    beginResetModel();
    m_all = entries;
    m_entries.clear();
    for (const Entry& e : std::as_const(m_all)) {
        if (accepted(e)) {
            m_entries.append(e);
        }
    }
    m_error = error;
    endResetModel();
    emit listed();
}

void DirModel::addRecent(const QString& file) {
    const QString dir = dirName(file);
    QSettings s;
    QStringList l = s.value(settingsKey(m_kind)).toStringList();
    l.removeAll(dir);
    l.prepend(dir);
    while (l.size() > maxRecent) {
        l.removeLast();
    }
    s.setValue(settingsKey(m_kind), l);
    updatePlaces();
}

void DirModel::updatePlaces() {
    QVariantList places;
    auto add = [&places](const QString& section, const QString& name, const QString& path) {
        for (const QVariant& p : std::as_const(places)) {
            if (p.toMap().value(QStringLiteral("path")).toString() == path) {
                return;
            }
        }
        places.append(QVariantMap{
            {QStringLiteral("section"), section},
            {QStringLiteral("name"), name},
            {QStringLiteral("path"), path},
        });
    };
    Machine *m = Machine::instance();
    const bool local = m_local || !m || m->engineLocal();
    const QString placesSection = tr("Places");
    const QString devicesSection = tr("Devices");
    const QString recentSection = tr("Recent");
    if (local) {
        add(placesSection, tr("Home"), QDir::homePath());
        const QStandardPaths::StandardLocation locs[] = {
            QStandardPaths::DocumentsLocation, QStandardPaths::MusicLocation, QStandardPaths::DownloadLocation,
        };
        for (auto loc : locs) {
            const QString p = QStandardPaths::writableLocation(loc);
            if (!p.isEmpty() && p != QDir::homePath() && QFileInfo::exists(p)) {
                add(placesSection, QStandardPaths::displayName(loc), p);
            }
        }
    }
    // folders the engine knows (guitarix config, IR folders, model folders)
    if (m && (!m_local || m->engineLocal())) {
        for (const QVariant& v : m->filePlaces()) {
            const QVariantMap p = v.toMap();
            add(placesSection, p.value(QStringLiteral("name")).toString(), p.value(QStringLiteral("path")).toString());
        }
    }
    add(placesSection, tr("Root"), QStringLiteral("/"));
    if (local) {
        // USB sticks and other removable media
        for (const QStorageInfo& si : QStorageInfo::mountedVolumes()) {
            const QString root = si.rootPath();
            if (si.isValid() && si.isReady() && (root.startsWith(QLatin1String("/media/"))
                                                 || root.startsWith(QLatin1String("/run/media/"))
                                                 || root.startsWith(QLatin1String("/mnt/")))) {
                add(devicesSection, si.displayName(), root);
            }
        }
    }
    const QStringList recent = QSettings().value(settingsKey(m_kind)).toStringList();
    for (const QString& r : recent) {
        add(recentSection, fileName(r).isEmpty() ? r : fileName(r), r);
    }
    m_places = places;
    emit placesChanged();
}
