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

// Machine, Tuner and MidiMap against FakeEngine. Every test ends with a
// check that the engine saw no request that would crash the real one.

#include "fakeengine.h"
#include "impulseresponse.h"
#include "machine.h"
#include "uitree.h"

#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <memory>

class TestMachine : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanup();

    void connectsAndLoads();
    void listensToNotifications();
    void unsupportedInterface();
    void lastEngineIsSaved();
    void userChangesAreSent();
    void engineChangesAreNotSentBack();
    void presetNotifications();
    void stepPresetWraps();
    void readOnlyBanksAreRefused();
    void presetEditing();
    void bankEditing();
    void bankFiles();
    void rackEditing();
    void metersArePolled();
    void specialOutputsArePolled();
    void oscilloscope();
    void unitLayouts_data();
    void unitLayouts();
    void reconnectKeepsParameters();

    void midiAssignments();
    void midiLearn();
    void midiLearnRefusesOutputs();
    void midiFeedback();
    void midiValues();

    void tunerTemperament();
    void tunerReferencePitch();
    void tunerStrings();
    void tunerDisplayRequest();
    void tunerMuteIsSaved();
    void placeAddedUnitsIsSaved();
    void tunerMutesEngine();
    void shutdownRestoresEngine();
    void switcherDisplay();
    void switcherTunerRoundTrip_data();
    void switcherTunerRoundTrip();
    void switcherKeys();

    void filePlacesOldEngine();
    void filePlacesRemote();
    void loadImpulseResponse();
    void gainLine();
    void impulseResponseEditing();
    void sequencer();
    void unitPresets();
    void sequencerPresetSteps();
    void loadModel();
    void ladspaPlugins();
    void pluginsChangedByOtherClient();

private:
    void start(int interfaceMinor = 2);
    QStringList bankPresets(const QString& bank) const;

    std::unique_ptr<FakeEngine> m_engine;
    std::unique_ptr<Machine> m_machine;
};

void TestMachine::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
}

void TestMachine::start(int interfaceMinor) {
    m_engine = std::make_unique<FakeEngine>(interfaceMinor);
    QVERIFY(m_engine->listen());
    m_machine = std::make_unique<Machine>(nullptr);
    Machine::setInstance(m_machine.get());
    m_machine->connectTo(QStringLiteral("127.0.0.1"), m_engine->port());
    QTRY_VERIFY(m_machine->ready());
}

QStringList TestMachine::bankPresets(const QString& bank) const {
    return m_machine->bank(bank).value(QStringLiteral("presets")).toStringList();
}

void TestMachine::cleanup() {
    if (m_engine) {
        QCOMPARE(m_engine->violations(), QStringList());
    }
    m_machine.reset();
    Machine::setInstance(nullptr);
    m_engine.reset();
    qunsetenv("GUITARIQX_REMOTE_FILES");
}

void TestMachine::connectsAndLoads() {
    start();
    Machine& m = *m_machine;
    QCOMPARE(m.serverVersion(), QStringLiteral("0.47.0"));
    QVERIFY(m.connected());
    QVERIFY(m.engineLocal());
    QCOMPARE(m.engineState(), QStringLiteral("stopped"));
    QVERIFY(m.parameterCount() > 200);
    QCOMPARE(m.monoRack(), m_engine->rack(false));
    QCOMPARE(m.stereoRack(), m_engine->rack(true));
    QVERIFY(m.monoRack().contains(QStringLiteral("nam")));
    QCOMPARE(m.currentBank(), QStringLiteral("Musiclab"));
    QCOMPARE(m.currentPreset(), QStringLiteral("Distortion - Iron Man"));
    QCOMPARE(m.banks().size(), m_engine->banks().size());
    QVERIFY(!m.bankMutable(QStringLiteral("Musiclab")));
    QVERIFY(m.bankMutable(QStringLiteral("Scratchpad")));
    QCOMPARE(m.writableBanks(), QStringList{QStringLiteral("Scratchpad")});

    // units that can be added: sorted by category then name, no fixed unit
    const QVariantList plugins = m.plugins();
    QVERIFY(plugins.size() > 50);
    QString lastCategory;
    for (const QVariant& v : plugins) {
        const QVariantMap p = v.toMap();
        QVERIFY(!p.value(QStringLiteral("fixed")).toBool());
        const QString cat = p.value(QStringLiteral("category")).toString();
        QVERIFY(!cat.isEmpty());
        QVERIFY(QString::compare(lastCategory, cat, Qt::CaseInsensitive) <= 0);
        lastCategory = cat;
    }
    QCOMPARE(m.pluginInfo(QStringLiteral("jconv")).value(QStringLiteral("stereo")).toBool(), true);
    // units the engine does not list
    QCOMPARE(m.pluginInfo(QStringLiteral("nothing")).value(QStringLiteral("name")).toString(), QStringLiteral("nothing"));
}

void TestMachine::listensToNotifications() {
    start();
    QStringList tokens;
    for (const auto& msg : m_engine->received(QStringLiteral("listen"))) {
        QVERIFY(!msg.isCall);
        tokens.append(msg.params.at(0).toString());
    }
    for (const char *t : {"preset", "state", "param", "midi", "freq", "units_changed"}) {
        QVERIFY2(tokens.contains(QString::fromLatin1(t)), t);
    }
    QSignalSpy state(m_machine.get(), &Machine::engineStateChanged);
    m_engine->notifyClients(QStringLiteral("state_changed"), {QStringLiteral("running")});
    QTRY_COMPARE(m_machine->engineState(), QStringLiteral("running"));
    QSignalSpy msg(m_machine.get(), &Machine::messageReceived);
    m_engine->notifyClients(QStringLiteral("message"), {QStringLiteral("error"), QStringLiteral("out of memory")});
    QTRY_COMPARE(msg.count(), 1);
    QCOMPARE(m_machine->lastMessage(), QStringLiteral("out of memory"));
}

void TestMachine::unsupportedInterface() {
    m_engine = std::make_unique<FakeEngine>();
    m_engine->setInterfaceMajor(2);
    QVERIFY(m_engine->listen());
    m_machine = std::make_unique<Machine>(nullptr);
    m_machine->connectTo(QStringLiteral("127.0.0.1"), m_engine->port());
    QTRY_VERIFY(!m_machine->connected());
    // no retry
    QTest::qWait(2500);
    QVERIFY(!m_machine->ready());
    QCOMPARE(m_engine->received(QStringLiteral("getversion")).size(), 1);
    QVERIFY(m_engine->received(QStringLiteral("parameterlist")).isEmpty());
}

void TestMachine::lastEngineIsSaved() {
    QSettings().clear();
    // not saved for an engine that is refused
    m_engine = std::make_unique<FakeEngine>();
    m_engine->setInterfaceMajor(2);
    QVERIFY(m_engine->listen());
    m_machine = std::make_unique<Machine>(nullptr);
    m_machine->connectTo(QStringLiteral("127.0.0.1"), m_engine->port());
    QTRY_VERIFY(!m_machine->connected());
    QVERIFY(!QSettings().contains(QStringLiteral("connection/port")));
    cleanup();

    start();
    QSettings s;
    QCOMPARE(s.value(QStringLiteral("connection/host")).toString(), QStringLiteral("127.0.0.1"));
    QCOMPARE(s.value(QStringLiteral("connection/port")).toInt(), int(m_engine->port()));
}

void TestMachine::userChangesAreSent() {
    start();
    Parameter *drive = m_machine->param(QStringLiteral("fumaster.Level"));
    QVERIFY(drive);
    drive->setValue(0.8);
    Parameter *on = m_machine->param(QStringLiteral("jconv_mono.on_off"));
    QVERIFY(on);
    on->setValue(true);
    QTRY_COMPARE(m_engine->received(QStringLiteral("set")).size(), 2);
    const auto sets = m_engine->received(QStringLiteral("set"));
    QVERIFY(!sets.at(0).isCall);
    QCOMPARE(sets.at(0).params, (QJsonArray{QStringLiteral("fumaster.Level"), 0.8}));
    // no JSON booleans (the engine's parser has none): checked by cleanup()
    QCOMPARE(sets.at(1).params, (QJsonArray{QStringLiteral("jconv_mono.on_off"), 1}));
    QCOMPARE(m_engine->value(QStringLiteral("jconv_mono.on_off")), QJsonValue(1));
}

void TestMachine::engineChangesAreNotSentBack() {
    start();
    Parameter *level = m_machine->param(QStringLiteral("fumaster.Level"));
    QSignalSpy changed(level, &Parameter::valueChanged);
    m_engine->notifyClients(QStringLiteral("set"), {QStringLiteral("fumaster.Level"), 0.1,
                                                   QStringLiteral("fumaster.Tone"), 0.9,
                                                   QStringLiteral("no.such.param"), 1});
    QTRY_COMPARE(changed.count(), 1);
    QCOMPARE(level->value().toDouble(), 0.1);
    QCOMPARE(m_machine->param(QStringLiteral("fumaster.Tone"))->value().toDouble(), 0.9);
    QTest::qWait(100);
    QVERIFY(m_engine->received(QStringLiteral("set")).isEmpty());
}

void TestMachine::presetNotifications() {
    start();
    QSignalSpy preset(m_machine.get(), &Machine::presetChanged);
    m_engine->notifyClients(QStringLiteral("preset_changed"), {QStringLiteral("Musiclab"), QStringLiteral("Lead - Europa")});
    QTRY_COMPARE(m_machine->currentPreset(), QStringLiteral("Lead - Europa"));
    // the rack is read again after a preset change
    const int before = m_engine->received(QStringLiteral("get_rack_unit_order")).size();
    QTRY_VERIFY(m_engine->received(QStringLiteral("get_rack_unit_order")).size() > before);
}

void TestMachine::stepPresetWraps() {
    start();
    m_engine->notifyClients(QStringLiteral("preset_changed"),
                            {QStringLiteral("Musiclab"), QStringLiteral("Lead - Still Got The Blues")});
    QTRY_COMPARE(m_machine->currentPreset(), QStringLiteral("Lead - Still Got The Blues"));
    m_machine->stepPreset(1);
    QTRY_COMPARE(m_machine->currentPreset(), QStringLiteral("Acoustic - Angie"));
    m_machine->stepPreset(-1);
    QTRY_COMPARE(m_machine->currentPreset(), QStringLiteral("Lead - Still Got The Blues"));
}

void TestMachine::readOnlyBanksAreRefused() {
    start();
    Machine& m = *m_machine;
    m_engine->clearReceived();
    QVERIFY(!m.savePreset(QStringLiteral("Musiclab"), QStringLiteral("Mine")).isEmpty());
    QVERIFY(!m.savePreset(QStringLiteral("No such bank"), QStringLiteral("Mine")).isEmpty());
    QVERIFY(!m.savePreset(QStringLiteral("Scratchpad"), QStringLiteral("   ")).isEmpty());
    QVERIFY(!m.renamePreset(QStringLiteral("Musiclab"), QStringLiteral("Lead - Europa"), QStringLiteral("X")).isEmpty());
    QVERIFY(!m.erasePreset(QStringLiteral("Musiclab"), QStringLiteral("Lead - Europa")).isEmpty());
    QVERIFY(!m.erasePreset(QStringLiteral("Scratchpad"), QStringLiteral("No such preset")).isEmpty());
    QVERIFY(!m.movePreset(QStringLiteral("No such bank"), QStringLiteral("x"), 1).isEmpty());
    QVERIFY(!m.copyPreset(QStringLiteral("Musiclab"), QStringLiteral("Lead - Europa"),
                          QStringLiteral("Musiclab"), QStringLiteral("Copy")).isEmpty());
    QVERIFY(!m.renameBank(QStringLiteral("Musiclab"), QStringLiteral("Mine")).isEmpty());
    QVERIFY(!m.removeBank(QStringLiteral("Musiclab")).isEmpty());
    QVERIFY(!m.removeBank(QStringLiteral("Scratchpad")).isEmpty());
    QVERIFY(!m.exportBank(QStringLiteral("No such bank"), QStringLiteral("/tmp/x.gx")).isEmpty());
    // the current preset is in a factory bank
    QVERIFY(!m.saveCurrent().isEmpty());
    QTest::qWait(100);
    QCOMPARE(m_engine->received().size(), 0);
}

void TestMachine::presetEditing() {
    start();
    Machine& m = *m_machine;
    const QString bank = QStringLiteral("Scratchpad");
    QCOMPARE(m.savePreset(bank, QStringLiteral("  Clean   tone ")), QString());
    QCOMPARE(m.savePreset(bank, QStringLiteral("Lead")), QString());
    QTRY_COMPARE(bankPresets(bank), (QStringList{QStringLiteral("livebuffer1"), QStringLiteral("Clean tone"),
                                                  QStringLiteral("Lead")}));
    QTRY_COMPARE(m.currentPreset(), QStringLiteral("Lead"));
    QCOMPARE(m.saveCurrent(), QString());

    QVERIFY(!m.renamePreset(bank, QStringLiteral("Lead"), QStringLiteral("Clean tone")).isEmpty());
    QCOMPARE(m.renamePreset(bank, QStringLiteral("Lead"), QStringLiteral("Solo")), QString());
    // names are checked against the bank list, read again after each edit
    QVERIFY(!m.movePreset(bank, QStringLiteral("Solo"), -2).isEmpty());
    QTRY_VERIFY(bankPresets(bank).contains(QStringLiteral("Solo")));
    QCOMPARE(m.movePreset(bank, QStringLiteral("Solo"), -2), QString());
    QTRY_COMPARE(bankPresets(bank), (QStringList{QStringLiteral("Solo"), QStringLiteral("livebuffer1"),
                                                  QStringLiteral("Clean tone")}));
    // out of range: nothing reordered
    QCOMPARE(m.movePreset(bank, QStringLiteral("Solo"), -1), QString());
    QCOMPARE(m.copyPreset(QStringLiteral("Musiclab"), QStringLiteral("Lead - Europa"), bank, QStringLiteral("Europa")),
             QString());
    QCOMPARE(m.erasePreset(bank, QStringLiteral("livebuffer1")), QString());
    QTRY_COMPARE(bankPresets(bank), (QStringList{QStringLiteral("Solo"), QStringLiteral("Clean tone"),
                                                  QStringLiteral("Europa")}));
    QVERIFY(!m.copyPreset(QStringLiteral("Musiclab"), QStringLiteral("Lead - Europa"), bank,
                          QStringLiteral("Europa")).isEmpty());
}

void TestMachine::bankEditing() {
    start();
    Machine& m = *m_machine;
    QSignalSpy created(&m, &Machine::bankCreated);
    QCOMPARE(m.newBank(QStringLiteral("Live")), QString());
    QTRY_COMPARE(created.count(), 1);
    QCOMPARE(created.at(0).at(0).toString(), QStringLiteral("Live"));
    // the engine makes names unique
    QCOMPARE(m.newBank(QStringLiteral("Live")), QString());
    QTRY_COMPARE(created.count(), 2);
    QCOMPARE(created.at(1).at(0).toString(), QStringLiteral("Live-1"));
    QTRY_VERIFY(m.bankMutable(QStringLiteral("Live-1")));

    QVERIFY(!m.renameBank(QStringLiteral("Live-1"), QStringLiteral("Live")).isEmpty());
    QCOMPARE(m.renameBank(QStringLiteral("Live-1"), QStringLiteral("Rehearsal")), QString());
    QTRY_VERIFY(!m.bank(QStringLiteral("Rehearsal")).isEmpty());
    QCOMPARE(m.moveBank(QStringLiteral("Rehearsal"), 1), QString());
    QTRY_COMPARE(m.banks().at(1).toMap().value(QStringLiteral("name")).toString(), QStringLiteral("Rehearsal"));
    QCOMPARE(m.removeBank(QStringLiteral("Live")), QString());
    QTRY_VERIFY(m.bank(QStringLiteral("Live")).isEmpty());
    QVERIFY(!m.bank(QStringLiteral("Rehearsal")).isEmpty());
}

void TestMachine::bankFiles() {
    start();
    Machine& m = *m_machine;
    QTemporaryDir dir;
    QCOMPARE(m.bankFileName(QStringLiteral(" AC/DC  songs ")), QStringLiteral("AC-DC songs.gx"));

    const QString bad = dir.filePath(QStringLiteral("bad.gx"));
    QFile f(bad);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not json");
    f.close();
    QVERIFY(!m.importBank(bad).isEmpty());
    QVERIFY(!m.importBank(dir.filePath(QStringLiteral("missing.gx"))).isEmpty());

    const QString good = dir.filePath(QStringLiteral("Gig.gx"));
    QFile g(good);
    QVERIFY(g.open(QIODevice::WriteOnly));
    g.write("[\"gx_head_file_version\", [1, 2, \"0.47.0\"]]");
    g.close();
    QSignalSpy imported(&m, &Machine::bankImported);
    QCOMPARE(m.importBank(good), QString());
    QTRY_COMPARE(imported.count(), 1);
    QCOMPARE(imported.at(0).at(1).toString(), QStringLiteral("Gig"));
    QCOMPARE(imported.at(0).at(2).toString(), QString());
    const auto calls = m_engine->received(QStringLiteral("bank_insert_content"));
    QCOMPARE(calls.size(), 1);
    QVERIFY(calls.at(0).params.at(1).toString().contains(QStringLiteral("gx_head_file_version")));

    QSignalSpy exported(&m, &Machine::bankExported);
    const QString out = dir.filePath(QStringLiteral("out.gx"));
    QCOMPARE(m.exportBank(QStringLiteral("Scratchpad"), out), QString());
    QTRY_COMPARE(exported.count(), 1);
    QCOMPARE(exported.at(0).at(2).toString(), QString());
    QFile o(out);
    QVERIFY(o.open(QIODevice::ReadOnly));
    QVERIFY(QJsonDocument::fromJson(o.readAll()).isArray());
}

void TestMachine::rackEditing() {
    start();
    Machine& m = *m_machine;
    QStringList mono = m.monoRack();
    // fumaster after graphiceq
    const int i = mono.indexOf(QStringLiteral("fumaster"));
    QCOMPARE(mono.at(i + 1), QStringLiteral("graphiceq"));
    m.moveUnit(QStringLiteral("fumaster"), 1, false);
    mono.move(i, i + 1);
    QTRY_COMPARE(m.monoRack(), mono);
    m.moveUnit(QStringLiteral("fumaster"), -1, false);
    mono.move(i + 1, i);
    QTRY_COMPARE(m.monoRack(), mono);
    // to the end
    m.moveUnit(mono.at(mono.size() - 2), 1, false);
    mono.move(mono.size() - 2, mono.size() - 1);
    QTRY_COMPARE(m.monoRack(), mono);
    // out of range: nothing sent
    m_engine->clearReceived();
    m.moveUnit(mono.first(), -1, false);
    m.moveUnit(QStringLiteral("not_in_rack"), 1, false);
    QTest::qWait(100);
    QVERIFY(m_engine->received(QStringLiteral("insert_rack_unit")).isEmpty());

    // drag and drop: to a slot of the rack
    const QString unit = mono.at(1);
    m.moveUnitTo(unit, mono.size(), false);
    mono.move(1, mono.size() - 1);
    QTRY_COMPARE(m.monoRack(), mono);
    m.moveUnitTo(unit, 0, false);
    mono.move(mono.size() - 1, 0);
    QTRY_COMPARE(m.monoRack(), mono);
    m.moveUnitTo(unit, 3, false);
    mono.move(0, 2);
    QTRY_COMPARE(m.monoRack(), mono);
    // its own place, out of range, not in the rack: nothing sent
    m_engine->clearReceived();
    m.moveUnitTo(unit, 2, false);
    m.moveUnitTo(unit, 3, false);
    m.moveUnitTo(unit, -1, false);
    m.moveUnitTo(unit, mono.size() + 1, false);
    m.moveUnitTo(QStringLiteral("not_in_rack"), 0, false);
    QTest::qWait(100);
    QVERIFY(m_engine->received(QStringLiteral("insert_rack_unit")).isEmpty());

    m.insertUnit(QStringLiteral("chorus"), QString(), true);
    QTRY_COMPARE(m.stereoRack().last(), QStringLiteral("chorus"));
    m.removeUnit(QStringLiteral("chorus"), true);
    QTRY_VERIFY(!m.stereoRack().contains(QStringLiteral("chorus")));
    // added on top
    m.addUnit(QStringLiteral("chorus"), true);
    QTRY_COMPARE(m.stereoRack().first(), QStringLiteral("chorus"));
    m.addUnit(QStringLiteral("overdrive"), false);
    QTRY_COMPARE(m.monoRack().first(), QStringLiteral("overdrive"));
    m.removeUnit(QStringLiteral("chorus"), true);
    QTRY_VERIFY(!m.stereoRack().contains(QStringLiteral("chorus")));

    QJsonArray changed;
    changed.append(QJsonArray{1, QStringLiteral("jconv"), QStringLiteral("pre_st")});
    m_engine->notifyClients(QStringLiteral("rack_units_changed"), changed);
    QTRY_COMPARE(m.stereoRack(), (QStringList{QStringLiteral("jconv"), QStringLiteral("pre_st")}));
}

void TestMachine::metersArePolled() {
    start();
    Machine& m = *m_machine;
    // not float parameters, or unknown: never polled (get_updates would
    // crash the engine)
    m.watchOutput(QStringLiteral("fumaster.on_off"));
    m.watchOutput(QStringLiteral("jconv.convolver"));
    m.watchOutput(QStringLiteral("no.such.meter"));
    m.watchOutput(QStringLiteral("graphiceq.v1"));
    m.watchOutput(QStringLiteral("graphiceq.v1"));
    QTRY_VERIFY(m_engine->received(QStringLiteral("get_updates")).size() >= 2);
    for (const auto& msg : m_engine->received(QStringLiteral("get_updates"))) {
        QCOMPARE(msg.params, QJsonArray{QStringLiteral("graphiceq.v1")});
    }
    // still watched once
    m.unwatchOutput(QStringLiteral("graphiceq.v1"));
    const int n = m_engine->received(QStringLiteral("get_updates")).size();
    QTRY_VERIFY(m_engine->received(QStringLiteral("get_updates")).size() > n);
    m.unwatchOutput(QStringLiteral("graphiceq.v1"));
    QTest::qWait(100);
    const int stopped = m_engine->received(QStringLiteral("get_updates")).size();
    QTest::qWait(200);
    QCOMPARE(m_engine->received(QStringLiteral("get_updates")).size(), stopped);
}

void TestMachine::specialOutputsArePolled() {
    start();
    Machine& m = *m_machine;
    // a switch the engine flips (a full tape stops recording) and the
    // sequencer position, which is no output: both floats in the engine
    QCOMPARE(m.param(QStringLiteral("dubber.rec1"))->kind(), Parameter::Bool);
    m.watchOutput(QStringLiteral("dubber.rec1"));
    m.watchOutput(QStringLiteral("seq.pos"));
    QTRY_VERIFY(!m_engine->received(QStringLiteral("get_updates")).isEmpty());
    const QJsonArray ids = m_engine->received(QStringLiteral("get_updates")).first().params;
    QVERIFY(ids.contains(QStringLiteral("dubber.rec1")));
    QVERIFY(ids.contains(QStringLiteral("seq.pos")));
    m.unwatchOutput(QStringLiteral("dubber.rec1"));
    m.unwatchOutput(QStringLiteral("seq.pos"));
}

void TestMachine::oscilloscope() {
    start();
    Machine& m = *m_machine;
    Oscilloscope *osc = m.oscilloscope();
    QSignalSpy frames(osc, &Oscilloscope::frameChanged);
    // the value of the parameter list
    const QVariant before = m.param(QStringLiteral("oscilloscope.info"))->value();
    m.watchOutput(QStringLiteral("oscilloscope.info"));
    m.watchOutput(QStringLiteral("graphiceq.v1"));
    QTRY_VERIFY(frames.count() > 0);
    QCOMPARE(osc->size(), 256);
    QCOMPARE(osc->jackBufferSize(), 256);
    QVERIFY(osc->realtime());
    QVERIFY(!m_engine->received(QStringLiteral("get_oscilloscope_mul_buffer")).isEmpty());
    // the frames are not kept in the parameter
    QCOMPARE(m.param(QStringLiteral("oscilloscope.info"))->value(), before);
    // polled at half the rate of the meters
    QTRY_VERIFY(m_engine->received(QStringLiteral("get_updates")).size() >= 6);
    int withOsc = 0;
    const auto polls = m_engine->received(QStringLiteral("get_updates"));
    for (const auto& msg : polls) {
        QVERIFY(msg.params.contains(QStringLiteral("graphiceq.v1")));
        withOsc += msg.params.contains(QStringLiteral("oscilloscope.info")) ? 1 : 0;
    }
    QVERIFY(withOsc > 0 && withOsc < polls.size());

    osc->setMulBuffer(3);
    QTRY_COMPARE(m_engine->received(QStringLiteral("set_oscilloscope_mul_buffer")).size(), 1);
    QCOMPARE(m_engine->received(QStringLiteral("set_oscilloscope_mul_buffer")).first().params, QJsonArray{3});
    QTRY_COMPARE(osc->size(), 3 * 256);
    osc->setMulBuffer(10);
    QCOMPARE(osc->mulBuffer(), Oscilloscope::maxMulBuffer);
    m.unwatchOutput(QStringLiteral("oscilloscope.info"));
    m.unwatchOutput(QStringLiteral("graphiceq.v1"));
}

void TestMachine::unitLayouts_data() {
    QTest::addColumn<QString>("unit");
    QTest::addColumn<bool>("stack");
    for (const char *u : {"fumaster", "graphiceq", "biquad", "amp.tonestack", "nam", "rtneural", "jconv_mono",
                          "jconv", "pre_st", "stereoverb"}) {
        QTest::addRow("%s glade", u) << QString::fromLatin1(u) << false;
        QTest::addRow("%s stack", u) << QString::fromLatin1(u) << true;
    }
}

namespace {

void collectControls(const QVariantMap& node, QList<QVariantMap>& out) {
    if (node.value(QStringLiteral("type")).toString() == QLatin1String("control")) {
        out.append(node);
    }
    for (const QVariant& c : node.value(QStringLiteral("children")).toList()) {
        collectControls(c.toMap(), out);
    }
}

} // anonymous namespace

void TestMachine::unitLayouts() {
    QFETCH(QString, unit);
    QFETCH(bool, stack);
    if (stack) {
        qputenv("GUITARIQX_FORM_STACK", "1");
    }
    start();
    qunsetenv("GUITARIQX_FORM_STACK");
    QSignalSpy ready(m_machine.get(), &Machine::unitUiReady);
    QVERIFY(m_machine->unitUi(unit).isEmpty());
    QTRY_COMPARE(ready.count(), 1);
    QCOMPARE(ready.at(0).at(0).toString(), unit);
    const QVariantMap ui = m_machine->unitUi(unit);
    const QVariantMap main = ui.value(QStringLiteral("main")).toMap();
    QVERIFY(uitree::hasControls(main));
    QList<QVariantMap> controls;
    collectControls(main, controls);
    collectControls(ui.value(QStringLiteral("mini")).toMap(), controls);
    for (const QVariantMap& c : std::as_const(controls)) {
        const QString id = c.value(QStringLiteral("id")).toString();
        QVERIFY2(m_machine->hasParam(id), qPrintable(id));
    }
    if (unit.startsWith(QLatin1String("jconv"))) {
        // the convolvers get an impulse response chooser
        QVERIFY(uitree::hasWidget(main, QStringLiteral("ir")));
    }
    // cached
    const int calls = m_engine->received(QStringLiteral("plugin_load_ui")).size();
    QCOMPARE(m_machine->unitUi(unit), ui);
    QCOMPARE(m_engine->received(QStringLiteral("plugin_load_ui")).size(), calls);
}

void TestMachine::reconnectKeepsParameters() {
    start();
    Parameter *p = m_machine->param(QStringLiteral("fumaster.Level"));
    QSignalSpy readySpy(m_machine.get(), &Machine::readyChanged);
    m_engine->disconnectClients();
    QTRY_VERIFY(!m_machine->ready());
    QVERIFY(m_machine->status().contains(QStringLiteral("retrying")));
    // the client retries every 2 s
    QTRY_VERIFY_WITH_TIMEOUT(m_machine->ready(), 10000);
    // QML bindings to the parameter objects survive
    QCOMPARE(m_machine->param(QStringLiteral("fumaster.Level")), p);
}

void TestMachine::midiAssignments() {
    start();
    MidiMap *midi = m_machine->midi();
    QTRY_COMPARE(midi->controllerFor(QStringLiteral("amp2.stage1.Pregain")), 7);
    const QVariantMap a = midi->assignment(QStringLiteral("amp2.stage1.Pregain"));
    QCOMPARE(a.value(QStringLiteral("ctlName")).toString(), QStringLiteral("CC 7 Main Volume"));
    QCOMPARE(a.value(QStringLiteral("upper")).toDouble(), 1.0);
    QCOMPARE(midi->controllerFor(QStringLiteral("fumaster.Level")), -1);
    QCOMPARE(MidiMap::controllerName(-1), QStringLiteral("---"));
    QCOMPARE(MidiMap::controllerName(3), QStringLiteral("CC 3"));
    QCOMPARE(MidiMap::controllerName(22), QStringLiteral("Midi Beat Clock"));
    QCOMPARE(MidiMap::controllerName(260), QStringLiteral("Note On C4"));
    QCOMPARE(MidiMap::controllerName(327), QStringLiteral("Controller 327"));

    // removing: the engine broadcasts the new map
    midi->remove(QStringLiteral("fumaster.Level"));
    midi->remove(QStringLiteral("amp2.stage1.Pregain"));
    QTRY_COMPARE(midi->controllerFor(QStringLiteral("amp2.stage1.Pregain")), -1);
    QCOMPARE(m_engine->received(QStringLiteral("midi_deleteParameter")).size(), 1);
}

void TestMachine::midiLearn() {
    start();
    MidiMap *midi = m_machine->midi();
    QTRY_COMPARE(midi->assignments().size(), 1);
    midi->startLearn(QStringLiteral("fumaster.Level"));
    QVERIFY(midi->learning());
    QTRY_VERIFY(m_engine->midiConfigMode());
    // a controller number typed in the dialog
    midi->setLearnControl(12);
    QCOMPARE(midi->learnControl(), 12);
    midi->commitLearn(0.2, 0.8, false, 0);
    QVERIFY(!midi->learning());
    QTRY_COMPARE(midi->controllerFor(QStringLiteral("fumaster.Level")), 12);
    QCOMPARE(midi->assignment(QStringLiteral("fumaster.Level")).value(QStringLiteral("lower")).toDouble(), 0.2);
    // config mode left again
    QTRY_VERIFY(!m_engine->midiConfigMode());

    // a switch takes a toggle behaviour instead of a range
    QVERIFY(!midi->usesRange(QStringLiteral("fumaster.on_off")));
    midi->startLearn(QStringLiteral("fumaster.on_off"));
    midi->setLearnControl(64);
    midi->commitLearn(0, 0, true, MidiMap::Toggle);
    QTRY_COMPARE(midi->controllerFor(QStringLiteral("fumaster.on_off")), 64);
    QCOMPARE(midi->assignment(QStringLiteral("fumaster.on_off")).value(QStringLiteral("behaviour")).toInt(),
             int(MidiMap::Toggle));

    // cancel
    midi->startLearn(QStringLiteral("fumaster.Tone"));
    QTRY_VERIFY(m_engine->midiConfigMode());
    midi->cancelLearn();
    QTRY_VERIFY(!m_engine->midiConfigMode());
    QCOMPARE(midi->controllerFor(QStringLiteral("fumaster.Tone")), -1);
}

void TestMachine::midiLearnRefusesOutputs() {
    start();
    MidiMap *midi = m_machine->midi();
    QVERIFY(!midi->isLearnable(QStringLiteral("graphiceq.v1")));
    QVERIFY(!midi->isLearnable(QStringLiteral("no.such.param")));
    midi->startLearn(QStringLiteral("graphiceq.v1"));
    QVERIFY(!midi->learning());
    // committing without learning sends nothing
    midi->commitLearn(0, 1, false, 0);
    QTest::qWait(100);
    QVERIFY(m_engine->received(QStringLiteral("midi_modifyCurrent")).isEmpty());
}

void TestMachine::midiFeedback() {
    start();
    MidiMap *midi = m_machine->midi();
    // asked on connection; the answer is repaired JSON
    QTRY_COMPARE(m_engine->received(QStringLiteral("get_midi_feedback")).size(), 1);
    QTest::qWait(50);
    QVERIFY(!midi->feedback());
    QSignalSpy changed(midi, &MidiMap::feedbackChanged);
    midi->setFeedback(true);
    QCOMPARE(changed.count(), 1);
    QTRY_VERIFY(m_engine->midiFeedback());
    QCOMPARE(m_engine->received(QStringLiteral("set_midi_feedback")).at(0).params, QJsonArray{1});
    // another UI turns it off: read again
    m_engine->clearReceived();
    m_machine.reset();
    Machine::setInstance(nullptr);
    m_machine = std::make_unique<Machine>(nullptr);
    Machine::setInstance(m_machine.get());
    m_machine->connectTo(QStringLiteral("127.0.0.1"), m_engine->port());
    QTRY_VERIFY(m_machine->ready());
    QTRY_VERIFY(m_machine->midi()->feedback());
}

void TestMachine::midiValues() {
    start();
    MidiMap *midi = m_machine->midi();
    QCOMPARE(midi->controllerValue(12), -1);
    QSignalSpy spy(midi, &MidiMap::controllerValueChanged);
    m_engine->notifyClients(QStringLiteral("midi_value_changed"), {12, 64});
    QTRY_COMPARE(midi->controllerValue(12), 64);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toInt(), 12);
    QCOMPARE(spy.at(0).at(1).toInt(), 64);
    // out of range controllers are ignored
    m_engine->notifyClients(QStringLiteral("midi_value_changed"), {1000, 3});
    m_engine->notifyClients(QStringLiteral("midi_value_changed"), {12, 100});
    QTRY_COMPARE(midi->controllerValue(12), 100);
    QCOMPARE(spy.count(), 2);
    // unknown again after a disconnection
    m_engine.reset();
    QTRY_VERIFY(!m_machine->connected());
    QCOMPARE(midi->controllerValue(12), -1);
}

void TestMachine::tunerTemperament() {
    start();
    Tuner *t = m_machine->tuner();
    t->setFrequency(440);
    QVERIFY(t->hasPitch());
    QCOMPARE(t->noteName(), QStringLiteral("A"));
    QCOMPARE(t->octave(), 4);
    QVERIFY(qAbs(t->cents()) < 1e-9);
    QCOMPARE(t->range(), 50.0);
    QCOMPARE(t->stringIndex(), -1);
    // a quarter tone sharp of E2
    t->setFrequency(82.4069 * std::pow(2.0, 0.25 / 12));
    QCOMPARE(t->noteName(), QStringLiteral("E"));
    QCOMPARE(t->octave(), 2);
    QVERIFY(qAbs(t->cents() - 25) < 0.1);
    // no pitch
    t->setFrequency(0);
    QVERIFY(!t->hasPitch());
    // 24-ET: index 2 of racktuner.temperament
    m_machine->param(QStringLiteral("racktuner.temperament"))->setValue(2);
    t->setFrequency(440 * std::pow(2.0, 0.5 / 12));
    QCOMPARE(t->range(), 25.0);
    QVERIFY(qAbs(t->cents()) < 1e-6);
}

void TestMachine::tunerReferencePitch() {
    start();
    Tuner *t = m_machine->tuner();
    m_engine->notifyClients(QStringLiteral("set"), {QStringLiteral("ui.tuner_reference_pitch"), 442});
    QTRY_COMPARE(m_machine->param(QStringLiteral("ui.tuner_reference_pitch"))->value().toDouble(), 442.0);
    t->setFrequency(442);
    QCOMPARE(t->noteName(), QStringLiteral("A"));
    QVERIFY(qAbs(t->cents()) < 1e-9);
    t->setFrequency(440);
    QVERIFY(qAbs(t->cents() + 7.85) < 0.01);
}

void TestMachine::tunerStrings() {
    start();
    Tuner *t = m_machine->tuner();
    QSignalSpy strings(t, &Tuner::stringsChanged);
    QCOMPARE(t->strings(), QStringList());
    // Standard
    m_engine->notifyClients(QStringLiteral("set"), {QStringLiteral("racktuner.tuning"), 1});
    QTRY_COMPARE(t->strings(), (QStringList{QStringLiteral("E"), QStringLiteral("A"), QStringLiteral("D"),
                                            QStringLiteral("G"), QStringLiteral("B"), QStringLiteral("E")}));
    // slightly flat A string
    t->setFrequency(108);
    QCOMPARE(t->stringIndex(), 1);
    QCOMPARE(t->noteName(), QStringLiteral("A"));
    QVERIFY(t->cents() < -30 && t->cents() > -40);
    // Standard/Es uses flats
    m_engine->notifyClients(QStringLiteral("set"), {QStringLiteral("racktuner.tuning"), 2});
    QTRY_COMPARE(t->strings().first(), QStringLiteral("Eb"));
}

void TestMachine::tunerDisplayRequest() {
    start();
    Tuner *t = m_machine->tuner();
    t->setActive(true);
    QTRY_COMPARE(m_engine->received(QStringLiteral("tuner_used_for_display")).size(), 1);
    QCOMPARE(m_engine->received(QStringLiteral("tuner_used_for_display")).at(0).params, QJsonArray{1});
    m_engine->notifyClients(QStringLiteral("tuner_changed"), {329.63, 64});
    QTRY_COMPARE(t->frequency(), 329.63);
    QCOMPARE(t->noteName(), QStringLiteral("E"));
    t->setActive(false);
    QVERIFY(!t->hasPitch());
    QTRY_COMPARE(m_engine->received(QStringLiteral("tuner_used_for_display")).size(), 2);
}

void TestMachine::tunerMuteIsSaved() {
    QSettings().clear();
    start();
    QVERIFY(m_machine->tuner()->muteWhileTuning());
    m_machine->tuner()->setMuteWhileTuning(false);
    cleanup();
    // a new session reads it back
    start();
    QVERIFY(!m_machine->tuner()->muteWhileTuning());
}

void TestMachine::placeAddedUnitsIsSaved() {
    QSettings().clear();
    start();
    QVERIFY(!m_machine->placeAddedUnits());
    QSignalSpy changed(m_machine.get(), &Machine::placeAddedUnitsChanged);
    m_machine->setPlaceAddedUnits(true);
    m_machine->setPlaceAddedUnits(true);
    QCOMPARE(changed.size(), 1);
    cleanup();
    // a new session reads it back
    start();
    QVERIFY(m_machine->placeAddedUnits());
}

void TestMachine::tunerMutesEngine() {
    start();
    Machine& m = *m_machine;
    Tuner *t = m.tuner();
    t->setMuteWhileTuning(true);
    m.setEngineState(QStringLiteral("bypassed"));
    QTRY_COMPARE(m.engineState(), QStringLiteral("bypassed"));
    t->setActive(true);
    QTRY_COMPARE(m.engineState(), QStringLiteral("stopped"));
    // the option applies at once while tuning
    t->setMuteWhileTuning(false);
    QTRY_COMPARE(m.engineState(), QStringLiteral("bypassed"));
    t->setMuteWhileTuning(true);
    QTRY_COMPARE(m.engineState(), QStringLiteral("stopped"));
    t->setActive(false);
    QTRY_COMPARE(m.engineState(), QStringLiteral("bypassed"));

    // an engine already stopped stays so
    m.setEngineState(QStringLiteral("stopped"));
    QTRY_COMPARE(m.engineState(), QStringLiteral("stopped"));
    const qsizetype sent = m_engine->received(QStringLiteral("setstate")).size();
    t->setActive(true);
    t->setActive(false);
    QTRY_COMPARE(m_engine->received(QStringLiteral("tuner_used_for_display")).size(), 4);
    QCOMPARE(m_engine->received(QStringLiteral("setstate")).size(), sent);
}

void TestMachine::shutdownRestoresEngine() {
    start();
    Machine& m = *m_machine;
    m.tuner()->setMuteWhileTuning(true);
    m.setEngineState(QStringLiteral("running"));
    QTRY_COMPARE(m.engineState(), QStringLiteral("running"));
    m.tuner()->setActive(true);
    QTRY_COMPARE(m.engineState(), QStringLiteral("stopped"));
    m.midi()->startLearn(QStringLiteral("fumaster.Level"));
    QTRY_VERIFY(m_engine->midiConfigMode());

    m.shutdown();
    // the client is gone before the engine reads it
    m_machine.reset();
    Machine::setInstance(nullptr);
    QTRY_VERIFY(!m_engine->midiConfigMode());
    QTRY_COMPARE(m_engine->received(QStringLiteral("setstate")).last().params, QJsonArray{QStringLiteral("running")});
    QCOMPARE(m_engine->received(QStringLiteral("tuner_used_for_display")).last().params, QJsonArray{0});
}

void TestMachine::switcherDisplay() {
    start();
    Machine& m = *m_machine;
    Tuner *t = m.tuner();
    QVERIFY(!t->switcherActive());
    m.setEngineState(QStringLiteral("running"));
    QTRY_COMPARE(m.engineState(), QStringLiteral("running"));
    QSignalSpy changed(t, &Tuner::switcherChanged);
    t->toggleSwitcher();
    QTRY_COMPARE(t->switcherState(), QStringLiteral("wait_start"));
    QVERIFY(t->switcherActive());
    QCOMPARE(m_engine->received(QStringLiteral("tuner_switcher_toggle")).at(0).params, QJsonArray{0});
    // the current preset until a note selects another one
    QCOMPARE(t->switcherBank(), m.currentBank());
    QCOMPARE(t->switcherPreset(), m.currentPreset());
    QTRY_COMPARE(m.engineState(), QStringLiteral("stopped"));
    m_engine->notifyClients(QStringLiteral("set_display_state"), {QStringLiteral("listening")});
    m_engine->notifyClients(QStringLiteral("display_bank_preset"), {QStringLiteral("Scratchpad"), QStringLiteral("p2")});
    QTRY_COMPARE(t->switcherPreset(), QStringLiteral("p2"));
    QCOMPARE(t->switcherBank(), QStringLiteral("Scratchpad"));
    QCOMPARE(t->switcherState(), QStringLiteral("listening"));
    // cancelled: nothing loaded, engine state back
    t->cancelSwitcher();
    QTRY_VERIFY(!t->switcherActive());
    QTRY_COMPARE(m.engineState(), QStringLiteral("running"));
    QVERIFY(!m.param(QStringLiteral("system.show_tuner"))->value().toBool());

    // started before the connection, e.g. by a MIDI controller
    t->toggleSwitcher();
    QTRY_VERIFY(m_engine->switcherActive());
    m_machine.reset();
    Machine::setInstance(nullptr);
    m_machine = std::make_unique<Machine>(nullptr);
    Machine::setInstance(m_machine.get());
    m_machine->connectTo(QStringLiteral("127.0.0.1"), m_engine->port());
    QTRY_VERIFY(m_machine->ready());
    QTRY_VERIFY(m_machine->tuner()->switcherActive());
    m_machine->tuner()->cancelSwitcher();
    QTRY_VERIFY(!m_machine->tuner()->switcherActive());
}

void TestMachine::switcherTunerRoundTrip_data() {
    QTest::addColumn<bool>("mute");
    QTest::newRow("mute while tuning") << true;
    QTest::newRow("no mute") << false;
}

// a note selects the tuner, the switcher closes it again: the engine ends
// running, as in gx_head
void TestMachine::switcherTunerRoundTrip() {
    QFETCH(bool, mute);
    start();
    Machine& m = *m_machine;
    Tuner *t = m.tuner();
    t->setMuteWhileTuning(mute);
    // what TunerView does
    Parameter *show = m.param(QStringLiteral("system.show_tuner"));
    connect(show, &Parameter::valueChanged, t, [t, show]() { t->setActive(show->value().toBool()); });
    m.setEngineState(QStringLiteral("running"));
    QTRY_COMPARE(m.engineState(), QStringLiteral("running"));

    t->toggleSwitcher();
    QTRY_VERIFY(m_engine->switcherActive());
    m_engine->finishSwitcher(true);
    QTRY_VERIFY(t->active());
    QVERIFY(!t->switcherActive());
    QTRY_COMPARE(m.engineState(), mute ? QStringLiteral("stopped") : QStringLiteral("bypassed"));

    t->toggleSwitcher();
    QTRY_VERIFY(!t->active());
    QCOMPARE(m_engine->received(QStringLiteral("tuner_switcher_toggle")).last().params, QJsonArray{1});
    QTRY_COMPARE(m.engineState(), QStringLiteral("running"));
    QTest::qWait(100);
    QCOMPARE(m.engineState(), QStringLiteral("running"));
}

void TestMachine::switcherKeys() {
    start();
    Machine& m = *m_machine;
    Tuner *t = m.tuner();
    QCOMPARE(Tuner::switcherNoteName(0), QStringLiteral("A4"));
    QCOMPARE(Tuner::switcherNoteName(-24), QStringLiteral("A2"));
    QCOMPARE(Tuner::switcherNoteName(-10), QStringLiteral("B3"));
    QCOMPARE(Tuner::switcherNoteName(7), QStringLiteral("E5"));
    // banks from the last one
    const QVariantList banks = t->switcherBankKeys();
    QCOMPARE(banks.size(), std::min<qsizetype>(14, m.banks().size()));
    QCOMPARE(banks.at(0).toMap().value(QStringLiteral("note")).toString(), QStringLiteral("A2"));
    QCOMPARE(banks.at(0).toMap().value(QStringLiteral("bank")), m.banks().last().toMap().value(QStringLiteral("name")));
    const QVariantList presets = t->switcherPresetKeys(m.currentBank());
    const QStringList names = bankPresets(m.currentBank());
    QCOMPARE(presets.size(), std::min<qsizetype>(18, names.size()));
    QCOMPARE(presets.at(0).toMap().value(QStringLiteral("note")).toString(), QStringLiteral("B3"));
    QCOMPARE(presets.at(0).toMap().value(QStringLiteral("preset")).toString(), names.at(0));
    QVERIFY(t->switcherPresetKeys(QStringLiteral("no such bank")).isEmpty());
}

void TestMachine::filePlacesOldEngine() {
    start(1);
    Machine& m = *m_machine;
    QVERIFY(!m.remoteFiles());
    QTRY_VERIFY(!m.filePlaces().isEmpty());
    QStringList paths;
    for (const QVariant& v : m.filePlaces()) {
        paths.append(v.toMap().value(QStringLiteral("path")).toString());
    }
    QVERIFY(paths.contains(QStringLiteral("/home/guitarist/.config/guitarix/IR")));
    QVERIFY(paths.contains(QStringLiteral("/usr/share/gx_head/sounds")));
    QCOMPARE(m.resolveIrDir(QStringLiteral("%S/amps")), QStringLiteral("/usr/share/gx_head/sounds/amps"));
    // interface 1.1 has no get_file_places nor list_directory
    QVERIFY(m_engine->received(QStringLiteral("get_file_places")).isEmpty());
    QString error;
    m.listDirectory(QStringLiteral("/"), [&error](const QString&, const QVector<DirModel::Entry>&, const QString& e) {
        error = e;
    });
    QVERIFY(!error.isEmpty());
}

void TestMachine::filePlacesRemote() {
    qputenv("GUITARIQX_REMOTE_FILES", "1");
    m_engine = std::make_unique<FakeEngine>(2);
    m_engine->setDirectory(QStringLiteral("/home/guitarist/models"),
                           QJsonArray{QJsonArray{QStringLiteral("clean.nam"), 0, 1234, 1700000000},
                                      QJsonArray{QStringLiteral("old"), 1, 0, 1700000000}});
    QVERIFY(m_engine->listen());
    m_machine = std::make_unique<Machine>(nullptr);
    Machine::setInstance(m_machine.get());
    m_machine->connectTo(QStringLiteral("127.0.0.1"), m_engine->port());
    QTRY_VERIFY(m_machine->ready());
    Machine& m = *m_machine;
    QVERIFY(!m.engineLocal());
    QVERIFY(m.remoteFiles());
    // the engine's home first
    QTRY_COMPARE(m.filePlaces().value(0).toMap().value(QStringLiteral("path")).toString(),
                 QStringLiteral("/home/guitarist"));

    QVector<DirModel::Entry> entries;
    QString path;
    m.listDirectory(QStringLiteral("/home/guitarist/models"),
                    [&](const QString& p, const QVector<DirModel::Entry>& e, const QString&) {
        path = p;
        entries = e;
    });
    QTRY_COMPARE(entries.size(), 2);
    QCOMPARE(path, QStringLiteral("/home/guitarist/models"));
    QCOMPARE(entries.at(0).name, QStringLiteral("clean.nam"));
    QCOMPARE(entries.at(0).size, 1234);
    QVERIFY(entries.at(1).isDir);

    // the file chooser's model uses the engine's listing
    DirModel model;
    QSignalSpy listed(&model, &DirModel::listed);
    model.setPath(QStringLiteral("/home/guitarist/models"));
    QTRY_COMPARE(listed.count(), 1);
    QCOMPARE(model.rowCount(), 2);
    // folders first
    QCOMPARE(model.data(model.index(0), DirModel::NameRole).toString(), QStringLiteral("old"));
    model.setPath(QStringLiteral("/nowhere"));
    QTRY_COMPARE(listed.count(), 2);
    QVERIFY(!model.error().isEmpty());
}

void TestMachine::loadImpulseResponse() {
    start();
    Machine& m = *m_machine;
    QSignalSpy loaded(&m, &Machine::impulseResponseLoaded);
    m.loadImpulseResponse(QStringLiteral("nam"), QStringLiteral("/x.wav"));
    QCOMPARE(loaded.count(), 1);
    QVERIFY(!loaded.at(0).at(2).toString().isEmpty());

    m.loadImpulseResponse(QStringLiteral("jconv_mono"), QStringLiteral("/home/guitarist/IR/hall.wav"));
    QTRY_COMPARE(loaded.count(), 2);
    QCOMPARE(loaded.at(1).at(2).toString(), QString());
    // the signal comes when the request is sent, not when the engine has it
    QTRY_COMPARE(m_engine->value(QStringLiteral("jconv_mono.convolver")).toObject()
                     .value(QStringLiteral("jconv.IRFile")).toString(), QStringLiteral("hall.wav"));
    const QJsonObject settings = m_engine->value(QStringLiteral("jconv_mono.convolver")).toObject();
    QCOMPARE(settings.value(QStringLiteral("jconv.IRDir")).toString(), QStringLiteral("/home/guitarist/IR"));
    QCOMPARE(settings.value(QStringLiteral("jconv.Length")).toInt(), 4);
    // 2 channels, sum of squares 8: normalized gain 1/2
    QCOMPARE(settings.value(QStringLiteral("jconv.Gain")).toDouble(), 0.5);
    // the unit is switched on
    QTRY_COMPARE(m_engine->value(QStringLiteral("jconv_mono.on_off")), QJsonValue(1));
}

void TestMachine::gainLine() {
    using P = ImpulseResponse::Point;
    // no gain line: flat
    QCOMPARE(ImpulseResponse::gainAt({}, 5), 0.0);
    const QVector<P> line{{0, 0}, {3, -20}};
    QCOMPARE(ImpulseResponse::gainAt(line, 0), 0.0);
    QCOMPARE(ImpulseResponse::gainAt(line, 3), -20.0);
    QVERIFY(qFuzzyCompare(ImpulseResponse::gainAt(line, 1), -20.0 / 3));

    // IRWindow::calc_normalized_gain: 4 samples of 1 attenuated by 0,
    // 20/3, 40/3 and 20 dB
    double sum = 0;
    for (double db : {0.0, -20.0 / 3, -40.0 / 3, -20.0}) {
        sum += std::pow(10, db / 10);
    }
    const QVector<float> ones{1, 1, 1, 1};
    QVERIFY(qFuzzyCompare(ImpulseResponse::normalizedGain(ones, 1, 0, 4, line), 1 / std::sqrt(sum)));
    QCOMPARE(ImpulseResponse::normalizedGain(ones, 1, 0, 4, {}), 0.5);
    // only the part used counts
    QCOMPARE(ImpulseResponse::normalizedGain(ones, 1, 3, 1, line), 10.0);
    // stereo: energy per channel
    QCOMPARE(ImpulseResponse::normalizedGain(QVector<float>{1, 1, 1, 1, 1, 1, 1, -1}, 2, 0, 4, {}), 0.5);

    // what the engine accepts: from 0 to the last sample, strictly
    // increasing, gains in range
    const QVector<P> fixed = ImpulseResponse::validLine({{5, -3}, {5, -4}, {2, -1}, {20, -200}}, 10);
    QCOMPARE(fixed, (QVector<P>{{0, -3}, {5, -3}, {9, ImpulseResponse::minGain}}));
    QVERIFY(ImpulseResponse::validLine({{0, 0}}, 1).isEmpty());
    QVERIFY(ImpulseResponse::validLine({}, 100).isEmpty());
}

void TestMachine::impulseResponseEditing() {
    start();
    Machine& m = *m_machine;
    QTRY_VERIFY(m.resolveIrDir(QStringLiteral("%S")) != QLatin1String("%S"));
    QCOMPARE(m.resolveIrDir(QStringLiteral("%S/bands")), QStringLiteral("/usr/share/gx_head/sounds/bands"));
    QCOMPARE(m.resolveIrDir(QStringLiteral("%U")), QStringLiteral("/home/guitarist/.config/guitarix/IR"));

    ImpulseResponse ir;
    QSignalSpy loaded(&ir, &ImpulseResponse::fileLoaded);
    ir.setUnit(QStringLiteral("jconv"));
    QTRY_VERIFY(ir.loaded());
    QCOMPARE(ir.file(), QStringLiteral("/usr/share/gx_head/sounds/bands/Pan de Higo.wav"));
    QCOMPARE(ir.frames(), 4800);
    QCOMPARE(ir.channels(), 1);
    // Length 0: the whole file
    QCOMPARE(ir.offset(), 0);
    QCOMPARE(ir.length(), 4800);
    QVERIFY(ir.gainLine().isEmpty());
    QVERIFY(!ir.modified());

    // the first point brings the end points
    QCOMPARE(ir.addPoint(2400, -20), 1);
    QCOMPARE(ir.gainLine().size(), 3);
    QVERIFY(ir.modified());
    // end points keep their place, the others stay between their
    // neighbours
    QCOMPARE(ir.movePoint(0, 100, -10), 0);
    QCOMPARE(ir.points().first().i, 0);
    QCOMPARE(ir.points().first().g, -10.0);
    ir.movePoint(1, 9000, -500);
    QCOMPARE(ir.points().at(1).i, 4798);
    QCOMPARE(ir.points().at(1).g, ImpulseResponse::minGain);
    QCOMPARE(ir.addPoint(4798, -3), -1);
    ir.removePoint(0);
    ir.removePoint(2);
    QCOMPARE(ir.points().size(), 3);
    ir.setOffset(100);
    ir.setLength(100000);
    QCOMPARE(ir.length(), 4700);
    ir.setLength(1000);
    ir.setDelay(-5);
    QCOMPARE(ir.delay(), 0);
    ir.setDelay(48);
    QVERIFY(!ir.gainCorrection());
    ir.setGainCorrection(true);

    ir.apply();
    QVERIFY(!ir.modified());
    QTRY_COMPARE(m_engine->value(QStringLiteral("jconv.convolver")).toObject()
                     .value(QStringLiteral("jconv.Offset")).toInt(), 100);
    const QJsonObject s = m_engine->value(QStringLiteral("jconv.convolver")).toObject();
    // the folder as the engine gave it
    QCOMPARE(s.value(QStringLiteral("jconv.IRDir")).toString(), QStringLiteral("%S/bands"));
    QCOMPARE(s.value(QStringLiteral("jconv.IRFile")).toString(), QStringLiteral("Pan de Higo.wav"));
    QCOMPARE(s.value(QStringLiteral("jconv.Length")).toInt(), 1000);
    QCOMPARE(s.value(QStringLiteral("jconv.Delay")).toInt(), 48);
    QCOMPARE(s.value(QStringLiteral("jconv.GainCor")), QJsonValue(1));
    QCOMPARE(s.value(QStringLiteral("jconv.gainline")).toArray(),
             (QJsonArray{QJsonArray{0, -10}, QJsonArray{4798, -120}, QJsonArray{4799, 0}}));
    QVERIFY(qFuzzyCompare(s.value(QStringLiteral("jconv.Gain")).toDouble(),
                          ImpulseResponse::normalizedGain(ir.samples(), 1, 100, 1000, ir.points())));

    // reset, and settings changed elsewhere are read again
    ir.resetSettings();
    QCOMPARE(ir.length(), 4800);
    QVERIFY(ir.gainLine().isEmpty());
    ir.reload();
    QCOMPARE(ir.offset(), 100);
    QJsonObject other = s;
    other.insert(QStringLiteral("jconv.Offset"), 7);
    m_engine->notifyClients(QStringLiteral("set"), {QStringLiteral("jconv.convolver"), other});
    QTRY_COMPARE(ir.offset(), 7);
    QCOMPARE(loaded.count(), 1);
}

void TestMachine::sequencer() {
    start();
    Machine& m = *m_machine;
    const auto line = [this](const QString& drum) {
        return m_engine->value(QStringLiteral("seq.sequencer.") + drum).toObject()
            .value(QStringLiteral("seq.seqline")).toArray();
    };
    // a fresh engine has empty lines
    QVERIFY(line(QStringLiteral("kick")).isEmpty());
    QCOMPARE(m.sequencerDrums().size(), 6);
    QCOMPARE(m.sequencerSteps(), 24);

    // the first edit gives every line its steps
    m.toggleSequencerStep(QStringLiteral("kick"), 3);
    QTRY_COMPARE(line(QStringLiteral("kick")).size(), 24);
    QCOMPARE(line(QStringLiteral("kick")).at(3), QJsonValue(1));
    QCOMPARE(line(QStringLiteral("kick")).at(2), QJsonValue(0));
    for (const QString& d : m.sequencerDrums()) {
        QCOMPARE(line(d).size(), 24);
    }
    m.toggleSequencerStep(QStringLiteral("kick"), 3);
    QTRY_COMPARE(line(QStringLiteral("kick")).at(3), QJsonValue(0));
    m.toggleSequencerStep(QStringLiteral("hat"), 23);
    QTRY_COMPARE(line(QStringLiteral("hat")).at(23), QJsonValue(1));
    // out of range or unknown: nothing sent
    m.toggleSequencerStep(QStringLiteral("hat"), 24);
    m.toggleSequencerStep(QStringLiteral("cowbell"), 0);

    // more steps: whole beats, every line follows and keeps its steps
    m.setSequencerSteps(30);
    QTRY_COMPARE(m_engine->value(QStringLiteral("seq.asequences")).toDouble(), 28.0);
    for (const QString& d : m.sequencerDrums()) {
        QTRY_COMPARE(line(d).size(), 28);
    }
    QCOMPARE(line(QStringLiteral("hat")).at(23), QJsonValue(1));
    m.setSequencerSteps(4);
    QTRY_COMPARE(line(QStringLiteral("hat")).size(), 24);
    QCOMPARE(m.sequencerSteps(), 24);
}

void TestMachine::unitPresets() {
    start();
    Machine& m = *m_machine;
    QSignalSpy ready(&m, &Machine::unitPresetsReady);
    const auto names = [&ready]() {
        QStringList l;
        for (const QVariant& v : ready.last().at(1).toList()) {
            const QVariantMap e = v.toMap();
            l.append(QStringLiteral("%1%2%3").arg(e.value(QStringLiteral("factory")).toBool() ? "f:" : "u:",
                                                  e.value(QStringLiteral("name")).toString(),
                                                  e.value(QStringLiteral("active")).toBool() ? "*" : ""));
        }
        return l;
    };
    m.requestUnitPresets(QStringLiteral("graphiceq"));
    QTRY_COMPARE(ready.count(), 1);
    QCOMPARE(ready.last().at(0).toString(), QStringLiteral("graphiceq"));
    QCOMPARE(names(), (QStringList{QStringLiteral("f:drop500"), QStringLiteral("f:flat")}));

    // the values come back as notifications
    Parameter *g1 = m.param(QStringLiteral("graphiceq.g1"));
    m.setUnitPreset(QStringLiteral("graphiceq"), QStringLiteral("drop500"), true);
    QTRY_COMPARE(g1->value().toDouble(), -16.0);
    QCOMPARE(m.param(QStringLiteral("graphiceq.g11"))->value().toDouble(), -23.5);
    m.requestUnitPresets(QStringLiteral("graphiceq"));
    QTRY_COMPARE(ready.count(), 2);
    QCOMPARE(names(), (QStringList{QStringLiteral("f:drop500*"), QStringLiteral("f:flat")}));

    // saving sends the list again, the user's presets first
    g1->setValue(-3.0);
    QCOMPARE(m.saveUnitPreset(QStringLiteral("graphiceq"), QStringLiteral("  my  eq ")), QString());
    QTRY_COMPARE(ready.count(), 3);
    QCOMPARE(names(), (QStringList{QStringLiteral("u:my eq*"), QStringLiteral("f:drop500"), QStringLiteral("f:flat")}));
    QCOMPARE(m_engine->unitPresets(QStringLiteral("graphiceq"), false).at(0).second
                 .value(QStringLiteral("graphiceq.g1")).toDouble(), -3.0);
    QVERIFY(!m.saveUnitPreset(QStringLiteral("graphiceq"), QStringLiteral("  ")).isEmpty());
    m.setUnitPreset(QStringLiteral("graphiceq"), QStringLiteral("flat"), true);
    QTRY_COMPARE(g1->value().toDouble(), 0.0);
    m.setUnitPreset(QStringLiteral("graphiceq"), QStringLiteral("my eq"), false);
    QTRY_COMPARE(g1->value().toDouble(), -3.0);

    QCOMPARE(m.removeUnitPreset(QStringLiteral("graphiceq"), QStringLiteral("my eq")), QString());
    QTRY_COMPARE(ready.count(), 4);
    QCOMPARE(names(), (QStringList{QStringLiteral("f:drop500"), QStringLiteral("f:flat")}));

    // standard values, without the switch of the unit
    QVERIFY(!m.unitHasStdValues(QStringLiteral("graphiceq")));
    m.resetUnit(QStringLiteral("graphiceq"));
    QVERIFY(m.unitHasStdValues(QStringLiteral("graphiceq")));
    QTRY_COMPARE(m_engine->value(QStringLiteral("graphiceq.g1")).toDouble(), g1->value().toDouble());
    QVERIFY(m_engine->received(QStringLiteral("set")).size() > 0);
    for (const FakeEngine::Message& msg : m_engine->received(QStringLiteral("set"))) {
        QVERIFY(msg.params.at(0).toString() != QLatin1String("graphiceq.on_off"));
    }

    // units the engine does not know are not sent
    m_engine->clearReceived();
    m.requestUnitPresets(QStringLiteral("nounit"));
    m.setUnitPreset(QStringLiteral("nounit"), QStringLiteral("x"), false);
    QVERIFY(!m.saveUnitPreset(QStringLiteral("nounit"), QStringLiteral("x")).isEmpty());
    QVERIFY(!m.removeUnitPreset(QStringLiteral("nounit"), QStringLiteral("x")).isEmpty());
    m.stepUnitPreset(QStringLiteral("nounit"), 1);
    m.requestUnitPresets(QStringLiteral("graphiceq"));
    QTRY_COMPARE(ready.count(), 5);
    // only the last request (the sets of the reset may still arrive)
    QStringList sent;
    for (const FakeEngine::Message& msg : m_engine->received()) {
        if (msg.method.startsWith(QLatin1String("plugin_preset_list_"))) {
            sent.append(msg.method + QLatin1Char(' ') + msg.params.at(0).toString());
        }
    }
    QCOMPARE(sent, QStringList{QStringLiteral("plugin_preset_list_load graphiceq")});
}

void TestMachine::sequencerPresetSteps() {
    start();
    Machine& m = *m_machine;
    m_engine->setUnitPresets(QStringLiteral("seq"), false, {
        {QStringLiteral("a"), QJsonObject{{QStringLiteral("seq.bpm"), 61}}},
        {QStringLiteral("b"), QJsonObject{{QStringLiteral("seq.bpm"), 62}}},
        {QStringLiteral("c"), QJsonObject{{QStringLiteral("seq.bpm"), 63}}},
    });
    Parameter *bpm = m.param(QStringLiteral("seq.bpm"));
    // none in use: the first one, then the next ones, wrapping
    m.stepUnitPreset(QStringLiteral("seq"), 1);
    QTRY_COMPARE(bpm->value().toDouble(), 61.0);
    m.stepUnitPreset(QStringLiteral("seq"), -1);
    QTRY_COMPARE(bpm->value().toDouble(), 63.0);
    m.stepUnitPreset(QStringLiteral("seq"), 1);
    QTRY_COMPARE(bpm->value().toDouble(), 61.0);

    // the buttons of the unit (or their MIDI controllers) are set back
    Parameter *next = m.param(QStringLiteral("seq.npreset"));
    next->setValue(1);
    QTRY_COMPARE(bpm->value().toDouble(), 62.0);
    QCOMPARE(next->value().toDouble(), 0.0);
    QTRY_COMPARE(m_engine->value(QStringLiteral("seq.npreset")).toDouble(), 0.0);
    m_engine->notifyClients(QStringLiteral("set"), {QStringLiteral("seq.ppreset"), 1});
    QTRY_COMPARE(bpm->value().toDouble(), 61.0);
    QTRY_COMPARE(m_engine->value(QStringLiteral("seq.ppreset")).toDouble(), 0.0);
}

void TestMachine::loadModel() {
    start();
    Machine& m = *m_machine;
    QSignalSpy loaded(&m, &Machine::modelLoaded);
    m.loadModel(QStringLiteral("nam.loadfile"), QStringLiteral("/home/guitarist/models/crunch.nam"));
    QTRY_COMPARE(loaded.count(), 1);
    QCOMPARE(loaded.at(0).at(2).toString(), QString());
    QCOMPARE(m_engine->value(QStringLiteral("nam.loadpath")).toString(), QStringLiteral("/home/guitarist/models"));
    QCOMPARE(m_engine->value(QStringLiteral("nam.loadfile")).toString(),
             QStringLiteral("/home/guitarist/models/crunch.nam"));
    QTRY_COMPARE(m_engine->value(QStringLiteral("nam.flist")).toDouble(), 2.0);

    // a model the engine does not list
    m.loadModel(QStringLiteral("nam.loadfile"), QStringLiteral("/home/guitarist/models/new.nam"));
    QTRY_COMPARE(loaded.count(), 2);
    QVERIFY(!loaded.at(1).at(2).toString().isEmpty());
    m.loadModel(QStringLiteral("nothing.loadfile"), QStringLiteral("/a.nam"));
    QCOMPARE(loaded.count(), 3);
    QVERIFY(!loaded.at(2).at(2).toString().isEmpty());
}

void TestMachine::ladspaPlugins() {
    start();
    Machine& m = *m_machine;
    PluginManager& pm = *m.pluginManager();
    const QString amp = QStringLiteral("ladspa_1067");
    // the plugin active in the fixture is a rack unit
    QVERIFY(m.hasParam(amp + QStringLiteral(".on_off")));
    m.insertUnit(amp, QString(), false);
    QTRY_VERIFY(m.monoRack().contains(amp));
    const int gainPort = m_engine->ladspaList().at(0).toObject().value(QStringLiteral("ctrl_ports"))
        .toArray().at(0).toObject().value(QStringLiteral("idx")).toInt();
    const QString gain = QStringLiteral("%1.%2").arg(amp).arg(gainPort);
    QVERIFY(m.hasParam(gain));
    m.watchOutput(gain);

    // the engine's answer is repaired (two values as the result)
    QSignalSpy loaded(&pm, &PluginManager::loadedChanged);
    pm.load();
    QVERIFY(pm.busy());
    QTRY_VERIFY(pm.loaded());
    QVERIFY(!pm.busy());
    QCOMPARE(pm.total(), 5);
    QCOMPARE(pm.rowCount(), 5);
    QCOMPARE(pm.activeCount(), 1);
    QCOMPARE(pm.notFound(), QStringList{QStringLiteral("http://example.org/plugins/removed")});
    QVERIFY(!pm.modified());

    // filters
    pm.setView(PluginManager::Lv2Plugins);
    QCOMPARE(pm.rowCount(), 2);
    pm.setChannels(PluginManager::StereoPlugins);
    QCOMPARE(pm.rowCount(), 1);
    QCOMPARE(pm.data(pm.index(0), PluginManager::NameRole).toString(), QStringLiteral("ACE Reverb"));
    pm.setView(PluginManager::AllPlugins);
    pm.setChannels(PluginManager::AnyChannels);
    pm.setFilter(QStringLiteral("amplifier"));
    QCOMPARE(pm.rowCount(), 2);
    pm.setFilter(QString());

    // Aliasing in place of the amplifier, renamed and in another category
    const int alias = 2, ace = 3;
    QCOMPARE(pm.plugin(alias).value(QStringLiteral("name")).toString(), QStringLiteral("Aliasing"));
    pm.setActive(0, false);
    pm.setActive(alias, true);
    pm.setPlugin(alias, {{QStringLiteral("shortname"), QStringLiteral("  Alias ")},
                         {QStringLiteral("category"), QStringLiteral("Distortion")}});
    QVariantMap p = pm.plugin(alias);
    QCOMPARE(p.value(QStringLiteral("shortname")).toString(), QStringLiteral("Alias"));
    QVERIFY(p.value(QStringLiteral("hasSettings")).toBool());
    QVERIFY(pm.modified());
    QCOMPARE(pm.activeCount(), 1);
    // an empty name is the plugin's name
    pm.setPlugin(4, {{QStringLiteral("shortname"), QStringLiteral(" ")}, {QStringLiteral("quirks"), 7}});
    QCOMPARE(pm.plugin(4).value(QStringLiteral("shortname")).toString(), QStringLiteral("GLAME Butterworth Highpass"));
    // the is_lv2 flag (4) is the engine's
    QCOMPARE(pm.plugin(4).value(QStringLiteral("quirks")).toInt(), 3);
    pm.setPlugin(4, {{QStringLiteral("quirks"), pm.plugin(4).value(QStringLiteral("quirksDefault"))}});

    QSignalSpy applied(&pm, &PluginManager::applied);
    m_engine->clearReceived();
    pm.apply();
    QTRY_COMPARE(applied.count(), 1);
    QVERIFY(!pm.modified());
    QVERIFY(!pm.busy());
    QCOMPARE(m_engine->received(QStringLiteral("save_ladspalist")).size(), 1);
    QCOMPARE(m_engine->ladspaList().size(), 5);
    QCOMPARE(m_engine->ladspaList().at(alias).toObject().value(QStringLiteral("active_set")).toInt(), 0);
    QCOMPARE(pm.description(alias).value(QStringLiteral("active_set")).toInt(), 1);
    // the removed unit and its parameters are gone, no longer polled
    QCOMPARE(m_engine->ladspaPlugins().size(), 1);
    const QString aliasId = m_engine->ladspaPlugins().at(0);
    QVERIFY(aliasId.startsWith(QStringLiteral("lv2_")));
    QVERIFY(!m.hasParam(gain));
    // the engine keeps the unit in its rack order, guitariqx hides it
    QVERIFY(m_engine->rack(false).contains(amp));
    QTRY_VERIFY(!m.monoRack().contains(amp));
    QCOMPARE(m.pluginInfo(aliasId).value(QStringLiteral("name")).toString(), QStringLiteral("Aliasing"));
    QCOMPARE(m.pluginInfo(aliasId).value(QStringLiteral("shortname")).toString(), QStringLiteral("Alias"));
    QCOMPARE(m.pluginInfo(aliasId).value(QStringLiteral("category")).toString(), QStringLiteral("Distortion"));
    QVERIFY(m.hasParam(aliasId + QStringLiteral(".on_off")));
    // the layout of a unit no longer known is not asked for
    QVERIFY(m.unitUi(amp).isEmpty());
    QTest::qWait(150);

    // a stereo plugin made mono is unloaded first, then loaded again
    pm.setActive(ace, true);
    pm.apply();
    QTRY_COMPARE(applied.count(), 2);
    QString aceId;
    for (const QString& id : m_engine->ladspaPlugins()) {
        if (id != aliasId) {
            aceId = id;
        }
    }
    QVERIFY(m.pluginInfo(aceId).value(QStringLiteral("stereo")).toBool());
    pm.setPlugin(ace, {{QStringLiteral("stereoToMono"), true}});
    m_engine->clearReceived();
    pm.apply();
    QTRY_COMPARE(applied.count(), 3);
    const auto saves = m_engine->received(QStringLiteral("save_ladspalist"));
    QCOMPARE(saves.size(), 2);
    QCOMPARE(saves.at(0).params.at(0).toArray().at(ace).toObject().value(QStringLiteral("active")).toInt(), 0);
    QCOMPARE(saves.at(1).params.at(0).toArray().at(ace).toObject().value(QStringLiteral("active")).toInt(), 1);
    QCOMPARE(m_engine->received(QStringLiteral("ladspaloader_update_plugins")).size(), 2);
    QVERIFY(!m.pluginInfo(aceId).value(QStringLiteral("stereo")).toBool());

    // factory settings, discarding edits
    pm.resetPlugin(alias);
    p = pm.plugin(alias);
    QCOMPARE(p.value(QStringLiteral("shortname")).toString(), QStringLiteral("Aliasing"));
    QCOMPARE(p.value(QStringLiteral("category")).toString(), p.value(QStringLiteral("deducedCategory")).toString());
    QVERIFY(!p.value(QStringLiteral("hasSettings")).toBool());
    QVERIFY(pm.modified());
    pm.discard();
    QVERIFY(!pm.modified());
    QCOMPARE(pm.plugin(alias).value(QStringLiteral("shortname")).toString(), QStringLiteral("Alias"));

    pm.unload();
    QVERIFY(!pm.loaded());
    QCOMPARE(pm.rowCount(), 0);
}

void TestMachine::pluginsChangedByOtherClient() {
    start();
    Machine& m = *m_machine;
    const QString amp = QStringLiteral("ladspa_1067");
    QVERIFY(m.hasParam(amp + QStringLiteral(".on_off")));
    // the notification another client's change sends: parameters and
    // plugins are loaded again
    m_engine->notifyClients(QStringLiteral("plugins_changed"),
                            {QJsonArray{amp + QStringLiteral(".on_off")}, QJsonArray(), QJsonArray{QJsonArray{0, amp}}});
    QSignalSpy plugins(&m, &Machine::pluginsChanged);
    QTRY_COMPARE(plugins.count(), 1);
    // FakeEngine still has the plugin: the lists come from the engine
    QVERIFY(m.hasParam(amp + QStringLiteral(".on_off")));
    QCOMPARE(m_engine->received(QStringLiteral("parameterlist")).size(), 2);
}

QTEST_GUILESS_MAIN(TestMachine)
#include "tst_machine.moc"
