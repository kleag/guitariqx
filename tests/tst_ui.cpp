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

// Runs guitariqx against FakeEngine in the UI states of --debug-state,
// without a display (offscreen), and fails on any QML warning: errors in
// QML only show at run time, and some only with the oldest supported Qt.
//
// GUITARIQX_SCREENSHOT_DIR=dir keeps the screenshots (used for the
// documentation).

#include "fakeengine.h"

#include <QDir>
#include <QImage>
#include <QProcess>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestUi : public QObject {
    Q_OBJECT
private slots:
    void states_data();
    void states();
};

void TestUi::states_data() {
    QTest::addColumn<QString>("debugState");
    QTest::addColumn<QString>("size");
    QTest::addColumn<int>("interfaceMinor");
    QTest::addColumn<bool>("remote");
    // --skin: the built-in gx_head skin (installed skins are hidden)
    QTest::addColumn<QString>("skin");

    QTest::newRow("main") << QString() << QStringLiteral("1280x800") << 2 << false << QString();
    QTest::newRow("main-small") << QString() << QStringLiteral("800x480") << 2 << false << QString();
    // portrait: the preset name gets its own row
    QTest::newRow("main-portrait") << QString() << QStringLiteral("480x800") << 2 << false << QString();
    // narrowest side by side racks: wide units wrap
    QTest::newRow("main-narrow") << QString() << QStringLiteral("1100x800") << 2 << false << QString();
    QTest::newRow("tuner") << QStringLiteral("tuner:108") << QStringLiteral("1280x800") << 2 << false << QString();
    QTest::newRow("tuner-small") << QStringLiteral("tuner:330") << QStringLiteral("800x480") << 2 << false << QString();
    QTest::newRow("switcher") << QStringLiteral("switcher") << QStringLiteral("1280x800") << 2 << false << QString();
    QTest::newRow("switcher-small") << QStringLiteral("switcher:wait_stop") << QStringLiteral("800x480") << 2 << false << QString();
    QTest::newRow("learnmode") << QStringLiteral("learnmode") << QStringLiteral("1280x800") << 2 << false << QString();
    QTest::newRow("learn") << QStringLiteral("learn:fumaster.Level") << QStringLiteral("1280x800") << 2 << false << QString();
    QTest::newRow("learn-switch") << QStringLiteral("learn:fumaster.on_off") << QStringLiteral("800x480") << 2 << false << QString();
    QTest::newRow("midimap") << QStringLiteral("midimap") << QStringLiteral("1280x800") << 2 << false << QString();
    QTest::newRow("saveas") << QStringLiteral("saveas") << QStringLiteral("1280x800") << 2 << false << QString();
    QTest::newRow("bankload") << QStringLiteral("bankload") << QStringLiteral("1280x800") << 2 << false << QString();
    QTest::newRow("banksave") << QStringLiteral("banksave:Scratchpad") << QStringLiteral("800x480") << 2 << false << QString();
    QTest::newRow("files-ir") << QStringLiteral("files:jconv_mono") << QStringLiteral("1280x800") << 2 << false << QString();
    QTest::newRow("files-remote") << QStringLiteral("files:nam") << QStringLiteral("1280x800") << 2 << true << QString();
    QTest::newRow("skin") << QString() << QStringLiteral("1280x800") << 2 << false << QStringLiteral("Guitarix");
    QTest::newRow("skin-small") << QString() << QStringLiteral("800x480") << 2 << false << QStringLiteral("Guitarix");
    QTest::newRow("skins") << QStringLiteral("skins") << QStringLiteral("800x480") << 2 << false << QStringLiteral("Guitarix");
    // special widgets: units not in the recorded rack are added first
    QTest::newRow("looper") << QStringLiteral("unit:dubber") << QStringLiteral("1280x800") << 2 << false << QString();
    QTest::newRow("looper-skin") << QStringLiteral("unit:dubber") << QStringLiteral("800x480") << 2 << false
                                 << QStringLiteral("Guitarix");
    QTest::newRow("oscilloscope") << QStringLiteral("unit:oscilloscope") << QStringLiteral("1280x800") << 2 << false
                                  << QString();
    QTest::newRow("iredit") << QStringLiteral("iredit:jconv") << QStringLiteral("1280x800") << 2 << false << QString();
    // the mono rack is the tab shown
    QTest::newRow("iredit-small") << QStringLiteral("iredit:jconv_mono") << QStringLiteral("800x480") << 2 << false
                                  << QString();
    QTest::newRow("sequencer") << QStringLiteral("unit:seq,sequencer") << QStringLiteral("1280x800") << 2 << false
                               << QString();
    QTest::newRow("sequencer-small") << QStringLiteral("unit:seq,sequencer") << QStringLiteral("800x480") << 2
                                     << false << QString();
    QTest::newRow("files-remote-old-engine") << QStringLiteral("files:nam") << QStringLiteral("800x480") << 1 << true << QString();
}

void TestUi::states() {
    QFETCH(QString, debugState);
    QFETCH(QString, size);
    QFETCH(int, interfaceMinor);
    QFETCH(bool, remote);
    QFETCH(QString, skin);

    FakeEngine engine(interfaceMinor);
    engine.setDirectory(QStringLiteral("/home/guitarist"),
                        QJsonArray{QJsonArray{QStringLiteral("models"), 1, 0, 1700000000},
                                   QJsonArray{QStringLiteral("notes.txt"), 0, 120, 1700000000}});
    engine.setDirectory(QStringLiteral("/home/guitarist/models"),
                        QJsonArray{QJsonArray{QStringLiteral("clean.nam"), 0, 300000, 1700000000},
                                   QJsonArray{QStringLiteral("crunch.nam"), 0, 310000, 1700000000}});
    QVERIFY(engine.listen());

    QTemporaryDir home;
    QVERIFY(home.isValid());
    const QString shot = home.filePath(QStringLiteral("shot.png"));
    QStringList args{QStringLiteral("-H"), QStringLiteral("127.0.0.1"),
                     QStringLiteral("-p"), QString::number(engine.port()),
                     QStringLiteral("--size"), size,
                     QStringLiteral("--screenshot"), shot,
                     QStringLiteral("--screenshot-delay"), QStringLiteral("2500")};
    if (!debugState.isEmpty()) {
        args << QStringLiteral("--debug-state") << debugState;
    }
    if (!skin.isEmpty()) {
        args << QStringLiteral("--skin") << skin;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    env.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
    // settings (recent folders) in a scratch folder
    env.insert(QStringLiteral("HOME"), home.path());
    env.insert(QStringLiteral("XDG_CONFIG_HOME"), home.filePath(QStringLiteral(".config")));
    // no installed gx_head skins: the same on every computer
    env.insert(QStringLiteral("XDG_DATA_HOME"), home.filePath(QStringLiteral(".local/share")));
    env.insert(QStringLiteral("XDG_DATA_DIRS"), home.filePath(QStringLiteral("share")));
    env.remove(QStringLiteral("QT_QUICK_CONTROLS_STYLE"));
    if (remote) {
        env.insert(QStringLiteral("GUITARIQX_REMOTE_FILES"), QStringLiteral("1"));
    }

    QProcess app;
    app.setProcessEnvironment(env);
    QSignalSpy finished(&app, &QProcess::finished);
    app.start(QStringLiteral(GUITARIQX_BINARY), args);
    QVERIFY2(app.waitForStarted(), qPrintable(app.errorString()));
    // the fake engine needs the event loop: no waitForFinished()
    QVERIFY(finished.wait(60000) || finished.count() == 1);

    const QString out = QString::fromLocal8Bit(app.readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(app.readAllStandardError());
    const QString log = QStringLiteral("stdout:\n%1\nstderr:\n%2").arg(out, err);
    QVERIFY2(app.exitStatus() == QProcess::NormalExit && app.exitCode() == 0, qPrintable(log));
    QVERIFY2(out.contains(QStringLiteral("screenshot saved")), qPrintable(log));

    // QML errors and warnings name their file (qrc:/Guitariqx/qml/...)
    static const QRegularExpression qmlProblem(
        QStringLiteral("qrc:/|\\.qml:|TypeError|ReferenceError|Binding loop|Cannot assign|is not a type|"
                       "module .* is not installed|Unable to assign|unknown skin"));
    QStringList problems;
    for (const QString& line : err.split(QLatin1Char('\n'))) {
        if (qmlProblem.match(line).hasMatch()) {
            problems.append(line);
        }
    }
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1Char('\n'))));
    QCOMPARE(engine.violations(), QStringList());

    QImage image(shot);
    QVERIFY(!image.isNull());
    const QStringList wh = size.split(QLatin1Char('x'));
    QCOMPARE(image.width(), wh.at(0).toInt());
    QCOMPARE(image.height(), wh.at(1).toInt());

    const QString keep = qEnvironmentVariable("GUITARIQX_SCREENSHOT_DIR");
    if (!keep.isEmpty()) {
        QDir().mkpath(keep);
        const QString target = QDir(keep).filePath(QString::fromLatin1(QTest::currentDataTag()) + QStringLiteral(".png"));
        QFile::remove(target);
        QVERIFY(QFile::copy(shot, target));
    }
}

QTEST_GUILESS_MAIN(TestUi)
#include "tst_ui.moc"
