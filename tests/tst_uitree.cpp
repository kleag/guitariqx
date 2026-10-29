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

#include "parameter.h"
#include "uitree.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>
#include <memory>

namespace {

QJsonObject fixture() {
    QFile f(QStringLiteral(GUITARIQX_FIXTURES "/engine.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        qFatal("cannot read %s", qPrintable(f.fileName()));
    }
    return QJsonDocument::fromJson(f.readAll()).object();
}

QJsonArray recordedUi(const QString& unit, const QString& form) {
    static const QJsonObject uis = fixture().value(QStringLiteral("plugin_load_ui")).toObject();
    return uis.value(unit).toObject().value(form).toArray();
}

QList<QVariantMap> controls(const QVariantMap& node) {
    QList<QVariantMap> out;
    if (node.value(QStringLiteral("type")).toString() == QLatin1String("control")) {
        out.append(node);
    }
    for (const QVariant& c : node.value(QStringLiteral("children")).toList()) {
        out += controls(c.toMap());
    }
    return out;
}

QStringList ids(const QVariantMap& node) {
    QStringList l;
    for (const QVariantMap& c : controls(node)) {
        l.append(c.value(QStringLiteral("id")).toString());
    }
    return l;
}

} // anonymous namespace

class TestUiTree : public QObject {
    Q_OBJECT
private slots:
    void stackOps();
    void stackFlagsAndSwitches();
    void singleChildBoxesAreRemoved();
    void recordedStackLayouts_data();
    void recordedStackLayouts();
    void recordedGladeLayouts_data();
    void recordedGladeLayouts();
    void gladeWithoutRackbox();
    void generic();
    void fixIds();
    void prependControl();
};

void TestUiTree::stackOps() {
    // recorded from the engine for fumaster
    const QVariantMap ui = uitree::fromStackOps(recordedUi(QStringLiteral("fumaster"), QStringLiteral("stack")));
    const QVariantMap mini = ui.value(QStringLiteral("mini")).toMap();
    const QVariantMap main = ui.value(QStringLiteral("main")).toMap();
    // the first hide box is the compact view of a collapsed unit
    QCOMPARE(ids(mini), QStringList{QStringLiteral("fumaster.Tone")});
    QCOMPARE(controls(mini).first().value(QStringLiteral("widget")).toString(), QStringLiteral("slider"));
    QCOMPARE(ids(main), (QStringList{QStringLiteral("fumaster.Tone"), QStringLiteral("fumaster.Level"),
                                     QStringLiteral("fumaster.Volume"), QStringLiteral("fumaster.wet_dry")}));
    const QVariantMap knob = controls(main).at(3);
    QCOMPARE(knob.value(QStringLiteral("widget")).toString(), QStringLiteral("knob"));
    QCOMPARE(knob.value(QStringLiteral("label")).toString(), QStringLiteral("dry/wet"));
    QCOMPARE(main.value(QStringLiteral("orientation")).toString(), QStringLiteral("h"));
}

void TestUiTree::stackFlagsAndSwitches() {
    const QJsonArray ops{
        QJsonArray{QStringLiteral("openFrameBox"), QStringLiteral("Mode")},
        QJsonArray{QStringLiteral("set_next_flags"), 0x08 | 0x02 | 0x03},
        QJsonArray{QStringLiteral("create_big_rackknob"), QStringLiteral("u.drive"), QStringLiteral("Drive")},
        QJsonArray{QStringLiteral("create_switch"), QStringLiteral("minitoggle"), QStringLiteral("u.on"), QStringLiteral("On")},
        QJsonArray{QStringLiteral("create_fload_switch"), QStringLiteral("button"), QStringLiteral("u.x"), QStringLiteral("u.file")},
        QJsonArray{QStringLiteral("create_unknown_widget"), QStringLiteral("u.y")},
        QJsonArray{QStringLiteral("insertSpacer")},
        QJsonArray{QStringLiteral("closeBox")},
    };
    const QVariantMap main = uitree::fromStackOps(ops).value(QStringLiteral("main")).toMap();
    QCOMPARE(main.value(QStringLiteral("label")).toString(), QStringLiteral("Mode"));
    QVERIFY(main.value(QStringLiteral("frame")).toBool());
    const QList<QVariantMap> c = controls(main);
    QCOMPARE(c.size(), 3);
    QCOMPARE(c.at(0).value(QStringLiteral("size")).toString(), QStringLiteral("big"));
    QCOMPARE(c.at(0).value(QStringLiteral("showValue")).toBool(), true);
    QCOMPARE(c.at(0).value(QStringLiteral("labelInverse")).toBool(), true);
    QCOMPARE(c.at(0).value(QStringLiteral("numPosition")).toInt(), 3);
    // flags apply to the next control only
    QVERIFY(!c.at(1).contains(QStringLiteral("showValue")));
    QCOMPARE(c.at(1).value(QStringLiteral("id")).toString(), QStringLiteral("u.on"));
    QCOMPARE(c.at(1).value(QStringLiteral("switchType")).toString(), QStringLiteral("minitoggle"));
    // the file button is about its string parameter
    QCOMPARE(c.at(2).value(QStringLiteral("widget")).toString(), QStringLiteral("file"));
    QCOMPARE(c.at(2).value(QStringLiteral("id")).toString(), QStringLiteral("u.file"));
    QCOMPARE(main.value(QStringLiteral("children")).toList().last().toMap().value(QStringLiteral("type")).toString(),
             QStringLiteral("spacer"));
}

void TestUiTree::singleChildBoxesAreRemoved() {
    const QJsonArray ops{
        QJsonArray{QStringLiteral("openVerticalBox"), QString()},
        QJsonArray{QStringLiteral("openHorizontalBox"), QString()},
        QJsonArray{QStringLiteral("create_small_rackknob"), QStringLiteral("u.a"), QStringLiteral("A")},
        QJsonArray{QStringLiteral("create_small_rackknob"), QStringLiteral("u.b"), QStringLiteral("B")},
        QJsonArray{QStringLiteral("closeBox")},
        // empty decoration frame
        QJsonArray{QStringLiteral("openFrameBox"), QString()},
        QJsonArray{QStringLiteral("closeBox")},
        QJsonArray{QStringLiteral("closeBox")},
    };
    const QVariantMap main = uitree::fromStackOps(ops).value(QStringLiteral("main")).toMap();
    QCOMPARE(main.value(QStringLiteral("orientation")).toString(), QStringLiteral("h"));
    QCOMPARE(main.value(QStringLiteral("children")).toList().size(), 2);
}

void TestUiTree::recordedStackLayouts_data() {
    QTest::addColumn<QString>("unit");
    for (const QString& u : fixture().value(QStringLiteral("plugin_load_ui")).toObject().keys()) {
        if (!recordedUi(u, QStringLiteral("stack")).isEmpty()) {
            QTest::newRow(qPrintable(u)) << u;
        }
    }
}

void TestUiTree::recordedStackLayouts() {
    QFETCH(QString, unit);
    const QVariantMap ui = uitree::fromStackOps(recordedUi(unit, QStringLiteral("stack")));
    QVERIFY(uitree::hasControls(ui.value(QStringLiteral("main")).toMap()));
}

void TestUiTree::recordedGladeLayouts_data() {
    QTest::addColumn<QString>("unit");
    for (const QString& u : fixture().value(QStringLiteral("plugin_load_ui")).toObject().keys()) {
        const QJsonArray ops = recordedUi(u, QStringLiteral("glade"));
        if (ops.at(0).toArray().at(0).toString() == QLatin1String("load_glade")) {
            QTest::newRow(qPrintable(u)) << u;
        }
    }
}

void TestUiTree::recordedGladeLayouts() {
    QFETCH(QString, unit);
    const QString xml = recordedUi(unit, QStringLiteral("glade")).at(0).toArray().at(1).toString();
    const QVariantMap ui = uitree::fromGlade(xml);
    const QVariantMap main = ui.value(QStringLiteral("main")).toMap();
    QVERIFY(uitree::hasControls(main));
    // controls of the unit (some layouts misname them: "rtneuralinput"
    // for rtneural.input, Machine fixes that)
    const QStringList glade = ids(main) + ids(ui.value(QStringLiteral("mini")).toMap());
    for (const QString& id : glade) {
        QVERIFY2(id.startsWith(unit), qPrintable(id));
    }
}

void TestUiTree::gladeWithoutRackbox() {
    QVERIFY(uitree::fromGlade(QStringLiteral("<interface><object class=\"GtkBox\" id=\"other\"/></interface>")).isEmpty());
    QVERIFY(uitree::fromGlade(QStringLiteral("not xml")).isEmpty());
}

void TestUiTree::generic() {
    const QJsonArray list = fixture().value(QStringLiteral("parameterlist")).toArray();
    std::vector<std::unique_ptr<Parameter>> owned;
    QList<Parameter*> params;
    for (int i = 0; i + 1 < list.size(); i += 2) {
        std::unique_ptr<Parameter> p(Parameter::fromJson(list.at(i).toString(), list.at(i + 1).toObject(), nullptr));
        if (p && p->id().startsWith(QLatin1String("fumaster."))) {
            params.append(p.get());
            owned.push_back(std::move(p));
        }
    }
    std::sort(params.begin(), params.end(), [](Parameter *a, Parameter *b) { return a->id() < b->id(); });
    const QVariantMap ui = uitree::generic(params);
    const QStringList main = ids(ui.value(QStringLiteral("main")).toMap());
    // no rack bookkeeping parameters
    for (const QString& id : main) {
        QVERIFY2(!id.endsWith(QLatin1String(".on_off")) && !id.endsWith(QLatin1String(".position"))
                 && !id.endsWith(QLatin1String(".pp")), qPrintable(id));
    }
    QVERIFY(main.contains(QStringLiteral("fumaster.Level")));
    const QList<QVariantMap> mini = controls(ui.value(QStringLiteral("mini")).toMap());
    QCOMPARE(mini.size(), 1);
    QCOMPARE(mini.first().value(QStringLiteral("widget")).toString(), QStringLiteral("slider"));
}

void TestUiTree::fixIds() {
    QVariantMap ui = uitree::fromStackOps(recordedUi(QStringLiteral("fumaster"), QStringLiteral("stack")));
    uitree::fixIds(ui, [](const QString& id) { return id.toUpper(); });
    QCOMPARE(ids(ui.value(QStringLiteral("mini")).toMap()), QStringList{QStringLiteral("FUMASTER.TONE")});
    QVERIFY(ids(ui.value(QStringLiteral("main")).toMap()).contains(QStringLiteral("FUMASTER.LEVEL")));
}

void TestUiTree::prependControl() {
    QVariantMap ui = uitree::fromStackOps(recordedUi(QStringLiteral("fumaster"), QStringLiteral("stack")));
    QVERIFY(!uitree::hasWidget(ui.value(QStringLiteral("main")).toMap(), QStringLiteral("ir")));
    uitree::prependControl(ui, QStringLiteral("ir"), QStringLiteral("fumaster.x"));
    const QVariantMap main = ui.value(QStringLiteral("main")).toMap();
    QVERIFY(uitree::hasWidget(main, QStringLiteral("ir")));
    QCOMPARE(ids(main).first(), QStringLiteral("fumaster.x"));
    QCOMPARE(ids(main).size(), 5);
}

QTEST_GUILESS_MAIN(TestUiTree)
#include "tst_uitree.moc"
