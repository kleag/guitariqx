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

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>
#include <memory>

namespace {

QJsonObject base(const QString& id, int ctype, int dflags = 0) {
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), id.section(QLatin1Char('.'), -1)},
        {QStringLiteral("group"), id.section(QLatin1Char('.'), 0, 0)},
        {QStringLiteral("c_type"), ctype},
        {QStringLiteral("d_flags"), dflags},
    };
}

std::unique_ptr<Parameter> make(const QString& tag, const QJsonObject& entry) {
    return std::unique_ptr<Parameter>(Parameter::fromJson(tag, entry, nullptr));
}

std::unique_ptr<Parameter> makeFloat(double lower, double upper, double step, double value, int dflags = 0) {
    return make(QStringLiteral("Float"), QJsonObject{
        {QStringLiteral("Parameter"), base(QStringLiteral("unit.gain"), 1, dflags)},
        {QStringLiteral("lower"), lower}, {QStringLiteral("upper"), upper},
        {QStringLiteral("step"), step}, {QStringLiteral("value"), value},
        {QStringLiteral("std_value"), lower},
    });
}

std::unique_ptr<Parameter> makeEnum() {
    return make(QStringLiteral("Enum"), QJsonObject{
        {QStringLiteral("IntParameter"), QJsonObject{
            {QStringLiteral("Parameter"), base(QStringLiteral("unit.mode"), 3)},
            {QStringLiteral("lower"), 0}, {QStringLiteral("upper"), 0},
            {QStringLiteral("value"), 1}, {QStringLiteral("std_value"), 0},
        }},
        {QStringLiteral("value_names"), QJsonArray{
            QStringLiteral("soft"), QStringLiteral("Soft"),
            QStringLiteral("hard"), QStringLiteral("Hard"),
            QStringLiteral("fuzz"), QStringLiteral("Fuzz"),
        }},
    });
}

} // anonymous namespace

class TestParameter : public QObject {
    Q_OBJECT
private slots:
    void floatFromJson();
    void floatClampAndSignals();
    void engineUpdatesDoNotRequestChanges();
    void normalizedLinear();
    void normalizedLog();
    void setNormalizedRoundsToStep();
    void displayPrecision_data();
    void displayPrecision();
    void enumFromJson();
    void enumValueIdFromGet();
    void boolSentAsNumber();
    void floatSwitchIsBool();
    void reset();
    void wholeFixtureLoads();
};

void TestParameter::floatFromJson() {
    auto p = makeFloat(-20, 20, 0.1, 3);
    QVERIFY(p);
    QCOMPARE(p->id(), QStringLiteral("unit.gain"));
    QCOMPARE(p->group(), QStringLiteral("unit"));
    QCOMPARE(p->kind(), Parameter::Float);
    QCOMPARE(p->lower(), -20.0);
    QCOMPARE(p->upper(), 20.0);
    QCOMPARE(p->value().toDouble(), 3.0);
    QCOMPARE(p->controlType(), 1);
}

void TestParameter::floatClampAndSignals() {
    auto p = makeFloat(0, 10, 0, 5);
    QSignalSpy changed(p.get(), &Parameter::valueChanged);
    QSignalSpy requested(p.get(), &Parameter::changeRequested);
    p->setValue(42);
    QCOMPARE(p->value().toDouble(), 10.0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(requested.count(), 1);
    // same value: nothing sent
    p->setValue(10);
    QCOMPARE(requested.count(), 1);
}

void TestParameter::engineUpdatesDoNotRequestChanges() {
    // the engine does not echo a change back, and the client must not
    // send back what the engine reported
    auto p = makeFloat(0, 10, 0, 5);
    QSignalSpy changed(p.get(), &Parameter::valueChanged);
    QSignalSpy requested(p.get(), &Parameter::changeRequested);
    p->updateFromEngine(7.5);
    QCOMPARE(p->value().toDouble(), 7.5);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(requested.count(), 0);
}

void TestParameter::normalizedLinear() {
    auto p = makeFloat(-20, 20, 0, 0);
    QCOMPARE(p->normalized(), 0.5);
    p->setNormalized(0.75);
    QCOMPARE(p->value().toDouble(), 10.0);
    p->setNormalized(2);
    QCOMPARE(p->value().toDouble(), 20.0);
}

void TestParameter::normalizedLog() {
    auto p = makeFloat(20, 20000, 0, 200, 1);
    QVERIFY(p->logDisplay());
    QVERIFY(qAbs(p->normalized() - 1.0 / 3.0) < 1e-9);
    p->setNormalized(2.0 / 3.0);
    QVERIFY(qAbs(p->value().toDouble() - 2000) < 1e-6);
}

void TestParameter::setNormalizedRoundsToStep() {
    auto p = makeFloat(0, 1, 0.25, 0);
    p->setNormalized(0.3);
    QCOMPARE(p->value().toDouble(), 0.25);
}

void TestParameter::displayPrecision_data() {
    QTest::addColumn<double>("step");
    QTest::addColumn<double>("value");
    QTest::addColumn<QString>("text");
    QTest::newRow("step 1") << 1.0 << 3.4 << QStringLiteral("3");
    QTest::newRow("step 0.1") << 0.1 << 3.44 << QStringLiteral("3.4");
    QTest::newRow("step 0.01") << 0.01 << 3.456 << QStringLiteral("3.46");
    QTest::newRow("step 0.0001 capped") << 0.0001 << 3.45678 << QStringLiteral("3.457");
    QTest::newRow("no step") << 0.0 << 3.456 << QStringLiteral("3.46");
}

void TestParameter::displayPrecision() {
    QFETCH(double, step);
    QFETCH(double, value);
    QFETCH(QString, text);
    auto p = makeFloat(0, 10, step, value);
    QCOMPARE(p->displayValue(), text);
}

void TestParameter::enumFromJson() {
    auto p = makeEnum();
    QCOMPARE(p->kind(), Parameter::Enum);
    // upper derived from the value names
    QCOMPARE(p->upper(), 2.0);
    QCOMPARE(p->valueLabels(), (QStringList{QStringLiteral("Soft"), QStringLiteral("Hard"), QStringLiteral("Fuzz")}));
    QCOMPARE(p->displayValue(), QStringLiteral("Hard"));
    QCOMPARE(p->toJson(), QJsonValue(1));
}

void TestParameter::enumValueIdFromGet() {
    // "set" notifications carry the index, "get" results the value id
    auto p = makeEnum();
    p->updateFromEngine(QStringLiteral("fuzz"));
    QCOMPARE(p->value().toInt(), 2);
    p->updateFromEngine(0);
    QCOMPARE(p->value().toInt(), 0);
}

void TestParameter::boolSentAsNumber() {
    auto p = make(QStringLiteral("Bool"), QJsonObject{
        {QStringLiteral("Parameter"), base(QStringLiteral("unit.on_off"), 2)},
        {QStringLiteral("value"), 0}, {QStringLiteral("std_value"), 0},
    });
    QCOMPARE(p->kind(), Parameter::Bool);
    p->setValue(true);
    // the engine's JSON parser has no true/false
    QCOMPARE(p->toJson(), QJsonValue(1));
    QCOMPARE(p->displayValue(), QStringLiteral("on"));
}

void TestParameter::floatSwitchIsBool() {
    auto p = make(QStringLiteral("Float"), QJsonObject{
        {QStringLiteral("Parameter"), base(QStringLiteral("unit.switch"), 2)},
        {QStringLiteral("lower"), 0}, {QStringLiteral("upper"), 1},
        {QStringLiteral("value"), 1}, {QStringLiteral("std_value"), 0},
    });
    QCOMPARE(p->kind(), Parameter::Bool);
    QCOMPARE(p->value().toBool(), true);
}

void TestParameter::reset() {
    auto p = makeFloat(1, 10, 0, 5);
    QSignalSpy requested(p.get(), &Parameter::changeRequested);
    p->reset();
    QCOMPARE(p->value().toDouble(), 1.0);
    QCOMPARE(requested.count(), 1);
}

void TestParameter::wholeFixtureLoads() {
    QFile f(QStringLiteral(GUITARIQX_FIXTURES "/engine.json"));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonArray list = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("parameterlist")).toArray();
    QVERIFY(list.size() > 100);
    int enums = 0;
    for (int i = 0; i + 1 < list.size(); i += 2) {
        auto p = make(list.at(i).toString(), list.at(i + 1).toObject());
        QVERIFY2(p, qPrintable(QString::fromUtf8(QJsonDocument(list.at(i + 1).toObject()).toJson())));
        QVERIFY(!p->id().isEmpty());
        if (p->kind() == Parameter::Enum) {
            ++enums;
            QVERIFY2(p->valueLabels().size() == int(p->upper() - p->lower()) + 1, qPrintable(p->id()));
        }
    }
    QVERIFY(enums > 0);
}

QTEST_GUILESS_MAIN(TestParameter)
#include "tst_parameter.moc"
