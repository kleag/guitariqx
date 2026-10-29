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

// DirModel reading local folders (no Machine: the engine is local). The
// remote listing is tested in tst_machine.

#include "dirmodel.h"

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class TestDirModel : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void listsFoldersFirst();
    void suffixFilter();
    void hiddenFiles();
    void navigation();
    void missingFolder();
    void recentFolders();
    void helpers();

private:
    QStringList names(const DirModel& m) const;
    QTemporaryDir m_dir;
};

void TestDirModel::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    // QCollator ignores the numeric mode in the C locale of a bare container
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    QCoreApplication::setOrganizationName(QStringLiteral("guitariqx-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_dirmodel"));
    QSettings().clear();
    QVERIFY(m_dir.isValid());
    QDir d(m_dir.path());
    QVERIFY(d.mkpath(QStringLiteral("models/sub")));
    QVERIFY(d.mkpath(QStringLiteral("Amps")));
    for (const char *f : {"models/clean.nam", "models/Crunch.NAM", "models/lead10.nam", "models/lead9.nam",
                          "models/readme.txt", "models/.hidden.nam", "zeta.wav"}) {
        QFile file(d.filePath(QString::fromLatin1(f)));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
    }
}

QStringList TestDirModel::names(const DirModel& m) const {
    QStringList l;
    for (int i = 0; i < m.rowCount(); ++i) {
        l.append(m.data(m.index(i), DirModel::NameRole).toString());
    }
    return l;
}

void TestDirModel::listsFoldersFirst() {
    DirModel m;
    m.setPath(m_dir.path());
    QCOMPARE(names(m), (QStringList{QStringLiteral("Amps"), QStringLiteral("models"), QStringLiteral("zeta.wav")}));
    QVERIFY(m.data(m.index(0), DirModel::IsDirRole).toBool());
    QCOMPARE(m.data(m.index(2), DirModel::PathRole).toString(), m_dir.filePath(QStringLiteral("zeta.wav")));
    QCOMPARE(m.data(m.index(2), DirModel::SizeRole).toLongLong(), 1);
    QVERIFY(m.error().isEmpty());
}

void TestDirModel::suffixFilter() {
    DirModel m;
    m.setSuffixes({QStringLiteral(".NAM")});
    m.setPath(m_dir.filePath(QStringLiteral("models")));
    // natural order, case insensitive suffixes, folders always shown
    QCOMPARE(names(m), (QStringList{QStringLiteral("sub"), QStringLiteral("clean.nam"), QStringLiteral("Crunch.NAM"),
                                    QStringLiteral("lead9.nam"), QStringLiteral("lead10.nam")}));
    m.setShowAll(true);
    QVERIFY(names(m).contains(QStringLiteral("readme.txt")));
    // filters ignored
    QVERIFY(m.contains(QStringLiteral(".hidden.nam")));
}

void TestDirModel::hiddenFiles() {
    DirModel m;
    m.setPath(m_dir.filePath(QStringLiteral("models")));
    QVERIFY(!names(m).contains(QStringLiteral(".hidden.nam")));
    m.setShowHidden(true);
    QVERIFY(names(m).contains(QStringLiteral(".hidden.nam")));
}

void TestDirModel::navigation() {
    DirModel m;
    m.setPath(m_dir.filePath(QStringLiteral("models/sub/")));
    QCOMPARE(m.path(), m_dir.filePath(QStringLiteral("models/sub")));
    QVERIFY(m.canGoUp());
    m.cdUp();
    QCOMPARE(m.path(), m_dir.filePath(QStringLiteral("models")));
    const QStringList parts = m.pathParts();
    QCOMPARE(parts.last(), QStringLiteral("models"));
    QCOMPARE(m.partPath(parts.size() - 1), m_dir.path());
    m.setPath(QStringLiteral("/"));
    QVERIFY(!m.canGoUp());
    QCOMPARE(m.pathParts(), QStringList());

    // a dialog opened again reads the folder again
    m.setPath(m_dir.path());
    QSignalSpy listed(&m, &DirModel::listed);
    QFile f(m_dir.filePath(QStringLiteral("new.wav")));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.close();
    m.openAt(m_dir.path());
    QCOMPARE(listed.count(), 1);
    QVERIFY(names(m).contains(QStringLiteral("new.wav")));
    QVERIFY(f.remove());
}

void TestDirModel::missingFolder() {
    DirModel m;
    m.setPath(m_dir.filePath(QStringLiteral("nothing")));
    QCOMPARE(m.rowCount(), 0);
    QVERIFY(!m.error().isEmpty());
}

void TestDirModel::recentFolders() {
    DirModel m;
    m.setKind(QStringLiteral("nam"));
    const QString file = m_dir.filePath(QStringLiteral("models/clean.nam"));
    QSignalSpy places(&m, &DirModel::placesChanged);
    m.addRecent(file);
    QCOMPARE(places.count(), 1);
    bool found = false;
    for (const QVariant& v : m.places()) {
        const QVariantMap p = v.toMap();
        if (p.value(QStringLiteral("section")) == QStringLiteral("Recent")) {
            QCOMPARE(p.value(QStringLiteral("path")).toString(), m_dir.filePath(QStringLiteral("models")));
            found = true;
        }
    }
    QVERIFY(found);
    // per kind
    DirModel other;
    other.setKind(QStringLiteral("ir"));
    for (const QVariant& v : other.places()) {
        QVERIFY(v.toMap().value(QStringLiteral("section")) != QStringLiteral("Recent"));
    }
}

void TestDirModel::helpers() {
    QCOMPARE(DirModel::fileName(QStringLiteral("/a/b/c.nam")), QStringLiteral("c.nam"));
    QCOMPARE(DirModel::dirName(QStringLiteral("/a/b/c.nam")), QStringLiteral("/a/b"));
    QCOMPARE(DirModel::dirName(QStringLiteral("/c.nam")), QStringLiteral("/"));
    QCOMPARE(DirModel::formatSize(512), QStringLiteral("512 B"));
    QCOMPARE(DirModel::formatSize(1536), QStringLiteral("1.5 KiB"));
    QCOMPARE(DirModel::formatSize(3 * 1024 * 1024), QStringLiteral("3.0 MiB"));
}

QTEST_GUILESS_MAIN(TestDirModel)
#include "tst_dirmodel.moc"
