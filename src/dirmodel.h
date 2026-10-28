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

#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QStringList>
#include <QVariantList>
#include <QVector>
#include <QtQml/qqmlregistration.h>

/*
 * Directory listing for the file chooser.
 *
 * Files are chosen on the computer the engine runs on. When qt_head runs
 * there too the directory is read locally, otherwise the engine lists it
 * (list_directory RPC, see Machine::remoteFiles()).
 */
class DirModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    // file name suffixes shown (without dot, lower case); empty: all
    Q_PROPERTY(QStringList suffixes READ suffixes WRITE setSuffixes NOTIFY filterChanged)
    Q_PROPERTY(bool showAll READ showAll WRITE setShowAll NOTIFY filterChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY filterChanged)
    // remembers recent folders under this name ("nam", "ir", ...)
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY kindChanged)
    // files of this computer instead of the engine's (bank import/export:
    // the contents are sent over the connection)
    Q_PROPERTY(bool local READ local WRITE setLocal NOTIFY kindChanged)
    Q_PROPERTY(QStringList pathParts READ pathParts NOTIFY pathChanged)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY pathChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY listed)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QVariantList places READ places NOTIFY placesChanged)
public:
    enum Roles { NameRole = Qt::UserRole + 1, PathRole, IsDirRole, SizeRole, ModifiedRole };
    Q_ENUM(Roles)

    explicit DirModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString path() const { return m_path; }
    void setPath(const QString& p);
    QStringList suffixes() const { return m_suffixes; }
    void setSuffixes(const QStringList& s);
    bool showAll() const { return m_showAll; }
    void setShowAll(bool on);
    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool on);
    QString kind() const { return m_kind; }
    void setKind(const QString& k);
    bool local() const { return m_local; }
    void setLocal(bool on);
    QStringList pathParts() const;
    bool canGoUp() const;
    bool busy() const { return m_busy; }
    QString error() const { return m_error; }
    bool available() const;
    QVariantList places() const { return m_places; }

    Q_INVOKABLE void cdUp();
    // path of the first n parts of the current path (breadcrumb)
    Q_INVOKABLE QString partPath(int n) const;
    Q_INVOKABLE void refresh();
    // go to a folder and read it again, also when it is the current one:
    // a dialog opened again must show the files written since
    Q_INVOKABLE void openAt(const QString& path);
    // the current folder has an entry with this name (filters ignored)
    Q_INVOKABLE bool contains(const QString& name) const;
    // remember the folder of a chosen file
    Q_INVOKABLE void addRecent(const QString& file);
    Q_INVOKABLE static QString fileName(const QString& path);
    Q_INVOKABLE static QString dirName(const QString& path);
    Q_INVOKABLE static QString formatSize(qint64 size);

    struct Entry {
        QString name;
        bool isDir;
        qint64 size;
        QDateTime modified;
    };

signals:
    void pathChanged();
    void filterChanged();
    void kindChanged();
    void busyChanged();
    void listed();
    void availableChanged();
    void placesChanged();

private:
    void list();
    void setEntries(QVector<Entry> entries, const QString& error);
    bool accepted(const Entry& e) const;
    void updatePlaces();

    QString m_path;
    QStringList m_suffixes;
    bool m_showAll = false;
    bool m_showHidden = false;
    QString m_kind;
    bool m_local = false;
    QVector<Entry> m_all;
    QVector<Entry> m_entries;
    QString m_error;
    bool m_busy = false;
    int m_request = 0;
    QVariantList m_places;
};
