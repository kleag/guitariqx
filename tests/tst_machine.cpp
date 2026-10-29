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
#include "machine.h"
#include "uitree.h"

#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
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
    void tunerMutesEngine();
    void shutdownRestoresEngine();
    void switcherDisplay();
    void switcherTunerRoundTrip_data();
    void switcherTunerRoundTrip();
    void switcherKeys();

    void filePlacesOldEngine();
    void filePlacesRemote();
    void loadImpulseResponse();
    void loadModel();

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

    m.insertUnit(QStringLiteral("chorus"), QString(), true);
    QTRY_COMPARE(m.stereoRack().last(), QStringLiteral("chorus"));
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
    // not outputs, or unknown: never polled (get_updates would crash the engine)
    m.watchOutput(QStringLiteral("fumaster.Level"));
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

QTEST_GUILESS_MAIN(TestMachine)
#include "tst_machine.moc"
