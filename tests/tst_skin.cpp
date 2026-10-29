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

// gx_head skins: style sheet values and images. The built-in skin is read
// from skins/ in the sources, an installed one from a temporary folder.

#include "skin.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

class TestSkin : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void parse();
    void imports();
    void colors();
    void builtinOnly();
    void installed();
    void persisted();

private:
    static void write(const QString& path, const QByteArray& data);
    const QString m_builtin = QStringLiteral(GUITARIQX_SKINS);
};

void TestSkin::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QSettings().clear();
}

void TestSkin::write(const QString& path, const QByteArray& data) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

void TestSkin::parse() {
    const skincss::Rules r = skincss::parse(QStringLiteral(
        "/* comment {a:b} */ gx-regler , gx-knob{color:#ff9000;padding:4px 8px}"
        "gx-knob{color:red}\n#rack_label,.rack_label{font:condensed 8pt \"Roboto\";color:#222222}"
        "@media print{}"));
    QCOMPARE(r.value(QStringLiteral("gx-regler")).value(QStringLiteral("color")), QStringLiteral("#ff9000"));
    // later declarations win
    QCOMPARE(r.value(QStringLiteral("gx-knob")).value(QStringLiteral("color")), QStringLiteral("red"));
    QCOMPARE(r.value(QStringLiteral("gx-knob")).value(QStringLiteral("padding")), QStringLiteral("4px 8px"));
    QCOMPARE(r.value(QStringLiteral("#rack_label")).value(QStringLiteral("font")),
             QStringLiteral("condensed 8pt \"Roboto\""));
    QVERIFY(r.contains(QStringLiteral(".rack_label")));
    QVERIFY(!r.contains(QStringLiteral("comment")));
}

void TestSkin::imports() {
    QStringList loaded;
    const skincss::Rules r = skincss::parse(
        QStringLiteral("@import url(\"base.css\");#rack_label{color:#000}"),
        [&loaded](const QString& file) {
            loaded.append(file);
            return QStringLiteral("#rack_label{color:#222;font-size:8pt}gx-regler{color:#ff9000}");
        });
    QCOMPARE(loaded, QStringList{QStringLiteral("base.css")});
    // the importing sheet overrides the imported one
    QCOMPARE(r.value(QStringLiteral("#rack_label")).value(QStringLiteral("color")), QStringLiteral("#000"));
    QCOMPARE(r.value(QStringLiteral("#rack_label")).value(QStringLiteral("font-size")), QStringLiteral("8pt"));
    QCOMPARE(r.value(QStringLiteral("gx-regler")).value(QStringLiteral("color")), QStringLiteral("#ff9000"));
}

void TestSkin::colors() {
    QCOMPARE(skincss::color(QStringLiteral("#ff9000")), QColor(255, 144, 0));
    QCOMPARE(skincss::color(QStringLiteral(" rgb(1, 2, 3) ")), QColor(1, 2, 3));
    const QColor c = skincss::color(QStringLiteral("rgba(50,50,50,0.7)"));
    QCOMPARE(c.red(), 50);
    QVERIFY(qAbs(c.alphaF() - 0.7) < 0.01);
    QCOMPARE(skincss::color(QStringLiteral("black")), QColor(Qt::black));
    QVERIFY(!skincss::color(QStringLiteral("-gtk-icontheme(\"x\")")).isValid());
}

void TestSkin::builtinOnly() {
    QTemporaryDir empty;
    Skin skin({empty.path()}, m_builtin);
    QCOMPARE(skin.names(), QStringList{QStringLiteral("Guitarix")});
    QVERIFY(!skin.active());
    QVERIFY(skin.image(QStringLiteral("bigknob")).isEmpty());

    QSignalSpy changed(&skin, &Skin::changed);
    skin.select(QStringLiteral("Guitarix"));
    QCOMPARE(changed.count(), 1);
    QVERIFY(skin.active());
    const QVariantMap style = skin.style();
    QCOMPARE(style.value(QStringLiteral("value")).value<QColor>(), QColor(0xff, 0x90, 0x00));
    QCOMPARE(style.value(QStringLiteral("label")).value<QColor>(), QColor(0x22, 0x22, 0x22));
    QCOMPARE(style.value(QStringLiteral("title")).value<QColor>(), QColor(0x22, 0x22, 0x22));
    const QVariantMap knobs = style.value(QStringLiteral("knobs")).toMap();
    QCOMPARE(knobs.size(), 4);
    const QVariantMap small = knobs.value(QStringLiteral("smallknobr")).toMap();
    QCOMPARE(small.value(QStringLiteral("ringRadius")).toInt(), 26);
    QCOMPARE(small.value(QStringLiteral("ledSize")).toInt(), 3);
    QCOMPARE(small.value(QStringLiteral("xCenter")).toInt(), -1);
    // 50.5 in the style sheet, an integer style property in GTK
    QCOMPARE(knobs.value(QStringLiteral("bigknob")).toMap().value(QStringLiteral("ringRadius")).toInt(), 50);

    // the libgxw icons
    const QUrl knob = skin.image(QStringLiteral("bigknob"));
    QVERIFY(knob.isLocalFile());
    QVERIFY(QFile::exists(knob.toLocalFile()));
    QVERIFY(!skin.image(QStringLiteral("rbutton_on")).isEmpty());
    QVERIFY(skin.image(QStringLiteral("no_such_icon")).isEmpty());

    skin.select(QStringLiteral("Nonexistent"));
    QVERIFY(!skin.active());
    QVERIFY(skin.style().isEmpty());
}

void TestSkin::installed() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // an installed skin importing the Guitarix skin, which is not installed,
    // with its own background and knob
    write(dir.filePath(QStringLiteral("gx_head_Wood.css")),
          "@import url(\"gx_head_Guitarix.css\");#rack_label,#effect_title{color:#ffffff}");
    QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("Wood")));
    write(dir.filePath(QStringLiteral("Wood/background1.png")), "png");
    write(dir.filePath(QStringLiteral("Wood/smallknobr.png")), "png");

    Skin skin({dir.path()}, m_builtin);
    QCOMPARE(skin.names(), (QStringList{QStringLiteral("Guitarix"), QStringLiteral("Wood")}));
    skin.select(QStringLiteral("Wood"));
    QCOMPARE(skin.style().value(QStringLiteral("label")).value<QColor>(), QColor(Qt::white));
    QCOMPARE(skin.style().value(QStringLiteral("value")).value<QColor>(), QColor(0xff, 0x90, 0x00));
    QCOMPARE(skin.image(QStringLiteral("background1")),
             QUrl::fromLocalFile(dir.filePath(QStringLiteral("Wood/background1.png"))));
    QCOMPARE(skin.image(QStringLiteral("smallknobr")),
             QUrl::fromLocalFile(dir.filePath(QStringLiteral("Wood/smallknobr.png"))));
    // not in the skin: libgxw's
    QCOMPARE(skin.image(QStringLiteral("bigknob")),
             QUrl::fromLocalFile(m_builtin + QStringLiteral("/gxw/bigknob.png")));
}

void TestSkin::persisted() {
    QTemporaryDir empty;
    {
        Skin skin({empty.path()}, m_builtin);
        skin.select(QStringLiteral("Guitarix"));
        // select() is for --skin: not saved
        QVERIFY(!QSettings().contains(QStringLiteral("ui/skin")));
        skin.setName(QStringLiteral("Guitarix"));
    }
    {
        Skin skin({empty.path()}, m_builtin);
        QCOMPARE(skin.name(), QStringLiteral("Guitarix"));
        skin.setName(QString());
    }
    Skin skin({empty.path()}, m_builtin);
    QVERIFY(!skin.active());
}

QTEST_GUILESS_MAIN(TestSkin)
#include "tst_skin.moc"
