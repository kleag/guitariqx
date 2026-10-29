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

#include "tuner.h"
#include "machine.h"
#include "rpcclient.h"

#include <QSettings>
#include <cmath>

namespace {

const char muteKey[] = "tuner/muteWhileTuning";

// switcher notes, in semitones from A4 (TunerSwitcher::on_note_timeout)
const int bankNoteFirst = -24;    // A2 .. A#3: banks, from the last one
const int bankNoteCount = 14;
const int presetNoteFirst = -10;  // B3 .. E5: presets 1 .. 18
const int presetNoteCount = 18;

// divisions of the octave for the values of racktuner.temperament
const int temperaments[] = {12, 19, 24, 31, 41, 53};

// string notes (MIDI note numbers) for the values of racktuner.tuning,
// value 0 is "(Chromatic)"; same table as MainWindow::set_tuning in
// gx_main_window.cpp
struct TuningTab {
    bool flat;
    int notes[6];
};

const TuningTab tunings[] = {
    {false, {40, 45, 50, 55, 59, 64}}, // Standard
    {true,  {39, 44, 49, 54, 58, 63}}, // Standard/Es
    {false, {40, 47, 52, 56, 59, 64}}, // Open E
    {false, {38, 45, 50, 55, 59, 64}}, // Drop D
    {false, {39, 44, 49, 54, 58, 63}}, // Half Step Down
    {false, {38, 43, 48, 53, 57, 62}}, // Full Step Down
    {false, {37, 42, 47, 52, 56, 61}}, // 1 and 1/2 Steps Down
    {false, {38, 45, 50, 55, 59, 62}}, // Double Drop D
    {false, {36, 43, 48, 53, 57, 62}}, // Drop C
    {false, {37, 44, 49, 54, 58, 63}}, // Drop C#
    {false, {35, 42, 47, 52, 56, 61}}, // Drop B
    {false, {34, 41, 46, 51, 55, 60}}, // Drop A#
    {false, {33, 40, 45, 50, 54, 59}}, // Drop A
    {false, {38, 45, 50, 54, 57, 62}}, // Open D
    {false, {38, 45, 50, 53, 57, 62}}, // Open D Minor
    {false, {38, 43, 50, 55, 59, 62}}, // Open G
    {false, {38, 43, 50, 55, 58, 62}}, // Open G Minor
    {false, {36, 43, 48, 55, 60, 64}}, // Open C
    {false, {37, 42, 59, 52, 56, 61}}, // Open C#
    {false, {36, 43, 48, 55, 60, 63}}, // Open C Minor
    {false, {40, 44, 50, 52, 59, 64}}, // Open E7
    {false, {40, 47, 50, 55, 59, 64}}, // Open E Minor7
    {false, {38, 43, 50, 54, 59, 62}}, // Open G Major7
    {false, {40, 45, 52, 57, 60, 64}}, // Open A Minor
    {false, {40, 45, 52, 55, 60, 64}}, // Open A Minor7
    {false, {40, 45, 49, 52, 57, 64}}, // Open A
    {false, {36, 41, 46, 51, 55, 60}}, // C Tuning
    {false, {37, 42, 47, 52, 56, 61}}, // C# Tuning
    {false, {34, 39, 44, 49, 53, 58}}, // Bb Tuning
    {false, {33, 38, 43, 48, 52, 57}}, // A to A (Baritone)
    {false, {38, 45, 50, 55, 57, 62}}, // Open Dsus2
    {false, {38, 43, 50, 55, 60, 62}}, // Open Gsus2
    {false, {38, 43, 50, 55, 59, 64}}, // G6
    {false, {38, 43, 50, 55, 60, 62}}, // Modal G
    {false, {48, 52, 55, 58, 60, 62}}, // Overtone
    {false, {45, 48, 50, 52, 55, 69}}, // Pentatonic
    {false, {48, 51, 54, 57, 60, 63}}, // Minor Third
    {false, {48, 52, 56, 60, 64, 68}}, // Major Third
    {false, {40, 45, 50, 55, 60, 65}}, // All Fourths
    {false, {36, 42, 48, 54, 60, 66}}, // Augmented Fourths
    {false, {38, 43, 50, 53, 60, 62}}, // Slow Motion
    {false, {36, 43, 50, 55, 59, 60}}, // Admiral
    {false, {36, 41, 48, 55, 58, 65}}, // Buzzard
    {false, {36, 43, 50, 55, 57, 62}}, // Face
    {false, {38, 45, 50, 50, 57, 62}}, // Four and Twenty
    {false, {38, 50, 50, 50, 62, 62}}, // Ostrich
    {false, {36, 43, 50, 51, 62, 63}}, // Capo 200
    {false, {40, 45, 50, 52, 52, 57}}, // Balalaika
    {false, {36, 41, 48, 55, 60, 62}}, // Cittern One
    {false, {36, 43, 48, 55, 60, 67}}, // Cittern Two
    {false, {43, 47, 50, 55, 59, 62}}, // Dobro
    {false, {64, 59, 55, 50, 45, 40}}, // Lefty
    {false, {36, 43, 50, 57, 64, 71}}, // Mandoguitar
    {false, {35, 45, 50, 55, 59, 64}}, // Rusty Cage
    {false, {36, 43, 48, 53, 57, 58}}, // Hardcore
};

const int numTunings = sizeof(tunings) / sizeof(tunings[0]);

} // anonymous namespace

Tuner::Tuner(Machine& machine, RpcClient& rpc, QObject *parent)
    : QObject(parent), m_machine(machine), m_rpc(rpc),
      m_muteWhileTuning(QSettings().value(QLatin1String(muteKey), true).toBool()) {
}

void Tuner::setMuteWhileTuning(bool on) {
    if (on == m_muteWhileTuning) {
        return;
    }
    m_muteWhileTuning = on;
    QSettings().setValue(QLatin1String(muteKey), on);
    if (m_active) {
        on ? mute() : unmute();
    }
    emit muteWhileTuningChanged();
}

void Tuner::mute() {
    const QString state = m_machine.engineState();
    if (m_savedState.isEmpty() && !state.isEmpty() && state != QLatin1String("stopped")) {
        m_savedState = state;
        m_machine.setEngineState(QStringLiteral("stopped"));
    }
}

void Tuner::unmute() {
    if (!m_savedState.isEmpty()) {
        m_machine.setEngineState(m_savedState);
        m_savedState.clear();
    }
}

QString Tuner::midiNoteName(int note, bool flat) {
    static const char *sharps[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *flats[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    int i = ((note % 12) + 12) % 12;
    return QString::fromLatin1(flat ? flats[i] : sharps[i]);
}

void Tuner::setActive(bool on) {
    if (on == m_active) {
        return;
    }
    m_active = on;
    // same call as GxMachineRemote::tuner_used_for_display
    m_rpc.notify(QStringLiteral("tuner_used_for_display"), {on ? 1 : 0});
    if (on) {
        if (m_muteWhileTuning) {
            mute();
        }
    } else {
        unmute();
        m_hasPitch = false;
        emit pitchChanged();
    }
    emit activeChanged();
}

void Tuner::engineReady() {
    connectParameters();
    updateStrings();
    if (m_active) {
        m_rpc.notify(QStringLiteral("tuner_used_for_display"), {1});
    }
    // the switcher may have been started before (e.g. by a MIDI controller)
    m_rpc.call(QStringLiteral("get_tuner_switcher_active"), {}, [this](const QJsonValue& r) {
        if (r.toInt() != 0 && !switcherActive()) {
            m_switcherState = QStringLiteral("listening");
            m_switcherBank = m_machine.currentBank();
            m_switcherPreset = m_machine.currentPreset();
            emit switcherChanged();
        }
    });
}

void Tuner::toggleSwitcher() {
    m_rpc.notify(QStringLiteral("tuner_switcher_toggle"), {m_active ? 1 : 0});
}

void Tuner::cancelSwitcher() {
    m_rpc.notify(QStringLiteral("tuner_switcher_deactivate"));
}

void Tuner::switcherNotification(const QString& method, const QJsonArray& params) {
    if (method == QLatin1String("set_display_state")) {
        const QString state = params.at(0).toString();
        if (state == m_switcherState) {
            return;
        }
        if (!switcherActive()) {
            // the engine only sends a selection once a note is played
            m_switcherBank = m_machine.currentBank();
            m_switcherPreset = m_machine.currentPreset();
        }
        m_switcherState = state;
        emit switcherChanged();
    } else if (method == QLatin1String("display_bank_preset")) {
        m_switcherBank = params.at(0).toString();
        m_switcherPreset = params.at(1).toString();
        emit switcherChanged();
    } else if (method == QLatin1String("show_tuner")) {
        const bool show = params.at(0).toInt() != 0;
        // Closing the tuner this way also ends bypass (TunerSwitcher::toggle),
        // which the engine could not do while the tuner had stopped it.
        if (!show && m_savedState == QLatin1String("bypassed")) {
            m_savedState = QStringLiteral("running");
        }
        Parameter *p = m_machine.param(QStringLiteral("system.show_tuner"));
        if (p) {
            p->setValue(show);
        }
    }
}

QString Tuner::switcherNoteName(int note) {
    const int midi = 69 + note;
    return midiNoteName(midi) + QString::number(midi / 12 - 1);
}

QVariantList Tuner::switcherBankKeys() const {
    QVariantList keys;
    const QVariantList banks = m_machine.banks();
    for (int i = 0; i < bankNoteCount; ++i) {
        const int idx = banks.size() - i - 1;
        if (idx < 0) {
            break;
        }
        keys.append(QVariantMap{
            {QStringLiteral("note"), switcherNoteName(bankNoteFirst + i)},
            {QStringLiteral("bank"), banks.at(idx).toMap().value(QStringLiteral("name"))},
        });
    }
    return keys;
}

QVariantList Tuner::switcherPresetKeys(const QString& bank) const {
    QVariantList keys;
    const QStringList presets = m_machine.bank(bank).value(QStringLiteral("presets")).toStringList();
    for (int i = 0; i < presetNoteCount && i < presets.size(); ++i) {
        keys.append(QVariantMap{
            {QStringLiteral("note"), switcherNoteName(presetNoteFirst + i)},
            {QStringLiteral("preset"), presets.at(i)},
        });
    }
    return keys;
}

void Tuner::engineLost() {
    if (switcherActive()) {
        m_switcherState = QStringLiteral("normal_mode");
        emit switcherChanged();
    }
    if (m_hasPitch) {
        m_hasPitch = false;
        emit pitchChanged();
    }
}

void Tuner::connectParameters() {
    const char *ids[] = {"ui.tuner_reference_pitch", "racktuner.temperament", "racktuner.tuning"};
    for (const char *id : ids) {
        Parameter *p = m_machine.param(QString::fromLatin1(id));
        if (p) {
            // UniqueConnection: engineReady() runs on every reconnect
            connect(p, &Parameter::valueChanged, this, &Tuner::updateStrings, Qt::UniqueConnection);
        }
    }
}

double Tuner::reference() const {
    Parameter *p = m_machine.param(QStringLiteral("ui.tuner_reference_pitch"));
    double r = p ? p->value().toDouble() : 440.0;
    return r > 0 ? r : 440.0;
}

int Tuner::temperament() const {
    Parameter *p = m_machine.param(QStringLiteral("racktuner.temperament"));
    int i = p ? p->value().toInt() : 0;
    return (i >= 0 && i < 6) ? temperaments[i] : 12;
}

int Tuner::tuning() const {
    Parameter *p = m_machine.param(QStringLiteral("racktuner.tuning"));
    int i = p ? p->value().toInt() : 0;
    return (i >= 0 && i <= numTunings) ? i : 0;
}

void Tuner::updateStrings() {
    QStringList names;
    QList<int> notes;
    const int t = tuning();
    if (t > 0) {
        const TuningTab& tab = tunings[t - 1];
        m_flat = tab.flat;
        for (int n : tab.notes) {
            notes.append(n);
            names.append(midiNoteName(n, m_flat));
        }
    } else {
        m_flat = false;
    }
    m_stringNotes = notes;
    if (names != m_strings) {
        m_strings = names;
        emit stringsChanged();
    }
    update();
}

void Tuner::tunerChanged(const QJsonArray& params) {
    setFrequency(params.at(0).toDouble());
}

void Tuner::setFrequency(double freq) {
    m_freq = freq;
    update();
}

void Tuner::update() {
    // the pitch tracker reports 0 (note 1000) when it finds no pitch
    m_hasPitch = m_freq > 0;
    if (m_hasPitch) {
        // semitones relative to the reference A4 (MIDI note 69)
        const double semis = 12.0 * std::log2(m_freq / reference());
        if (!m_stringNotes.isEmpty()) {
            // nearest string of the tuning
            int best = 0;
            for (int i = 1; i < m_stringNotes.size(); ++i) {
                if (std::fabs(semis + 69 - m_stringNotes.at(i)) < std::fabs(semis + 69 - m_stringNotes.at(best))) {
                    best = i;
                }
            }
            const int note = m_stringNotes.at(best);
            m_stringIndex = best;
            m_noteName = midiNoteName(note, m_flat);
            m_octave = note / 12 - 1;
            m_cents = 100.0 * (semis + 69 - note);
            m_range = 50;
        } else {
            // nearest step of the equal temperament
            const int n = temperament();
            const long step = std::lround(semis * n / 12.0);
            const double stepSemis = step * 12.0 / n;
            const int note = 69 + int(std::lround(stepSemis));
            m_stringIndex = -1;
            m_noteName = midiNoteName(note, m_flat);
            m_octave = int(std::floor(note / 12.0)) - 1;
            m_cents = 100.0 * (semis - stepSemis);
            m_range = 50.0 * 12.0 / n;
        }
    }
    emit pitchChanged();
}
