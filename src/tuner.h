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

#include <QJsonArray>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class Machine;
class RpcClient;

/*
 * Tuner display state.
 *
 * The engine only reports the estimated frequency (and the note relative
 * to a fixed 440 Hz A4); like Gxw::RackTuner in gx_head, the note,
 * deviation and target string are computed here from the reference pitch
 * (ui.tuner_reference_pitch), the temperament (racktuner.temperament) and
 * the tuning (racktuner.tuning) parameters.
 *
 * It also shows the engine's tuner switcher (gx_head's Liveplay preset
 * mode): started by tuner_switcher_toggle or a MIDI controller assigned to
 * ui.live_play_switcher, the engine mutes the output and selects a bank
 * or preset from the note played (TunerSwitcher in tunerswitcher.cpp).
 * The engine reports its state (set_display_state), the selection
 * (display_bank_preset) and whether the tuner is wanted at the end
 * (show_tuner).
 */
class Tuner : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use Gx.tuner")
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(double frequency READ frequency NOTIFY pitchChanged)
    Q_PROPERTY(bool hasPitch READ hasPitch NOTIFY pitchChanged)
    Q_PROPERTY(QString noteName READ noteName NOTIFY pitchChanged)
    Q_PROPERTY(int octave READ octave NOTIFY pitchChanged)
    // deviation from the target note in cents (-50..50 for 12-ET)
    Q_PROPERTY(double cents READ cents NOTIFY pitchChanged)
    // half the distance between two notes of the temperament, in cents
    Q_PROPERTY(double range READ range NOTIFY pitchChanged)
    Q_PROPERTY(QStringList strings READ strings NOTIFY stringsChanged)
    Q_PROPERTY(int stringIndex READ stringIndex NOTIFY pitchChanged)
    // stop the engine while the tuner is active and restore its state
    // afterwards (UI option, kept in QSettings)
    Q_PROPERTY(bool muteWhileTuning READ muteWhileTuning WRITE setMuteWhileTuning NOTIFY muteWhileTuningChanged)
    // normal_mode, wait_start (until no note is heard), listening, wait_stop
    // (a selection is shown, loaded when the note stops)
    Q_PROPERTY(QString switcherState READ switcherState NOTIFY switcherChanged)
    Q_PROPERTY(bool switcherActive READ switcherActive NOTIFY switcherChanged)
    // selection shown by the engine; the preset may be a number or an
    // action (MUTE, BYPASS, TUNER ON...) with an empty bank
    Q_PROPERTY(QString switcherBank READ switcherBank NOTIFY switcherChanged)
    Q_PROPERTY(QString switcherPreset READ switcherPreset NOTIFY switcherChanged)
public:
    Tuner(Machine& machine, RpcClient& rpc, QObject *parent = nullptr);

    bool active() const { return m_active; }
    void setActive(bool on);
    double frequency() const { return m_freq; }
    bool hasPitch() const { return m_hasPitch; }
    QString noteName() const { return m_noteName; }
    int octave() const { return m_octave; }
    double cents() const { return m_cents; }
    double range() const { return m_range; }
    QStringList strings() const { return m_strings; }
    int stringIndex() const { return m_stringIndex; }
    bool muteWhileTuning() const { return m_muteWhileTuning; }
    void setMuteWhileTuning(bool on);
    QString switcherState() const { return m_switcherState; }
    bool switcherActive() const { return m_switcherState != QLatin1String("normal_mode"); }
    QString switcherBank() const { return m_switcherBank; }
    QString switcherPreset() const { return m_switcherPreset; }

    // starts the switcher, or stops it (without loading), or closes the
    // tuner if it is shown: what a MIDI controller assigned to
    // ui.live_play_switcher does
    Q_INVOKABLE void toggleSwitcher();
    Q_INVOKABLE void cancelSwitcher();
    // notes selecting a bank: [{note: "A2", bank: name}, ...]
    Q_INVOKABLE QVariantList switcherBankKeys() const;
    // notes selecting a preset of a bank: [{note: "B3", preset: name}, ...]
    Q_INVOKABLE QVariantList switcherPresetKeys(const QString& bank) const;
    // note name of a switcher note (semitones from A4)
    Q_INVOKABLE static QString switcherNoteName(int note);

    // called by the Machine
    void engineReady();
    void engineLost();
    void tunerChanged(const QJsonArray& params);
    // set_display_state, display_bank_preset and show_tuner notifications
    void switcherNotification(const QString& method, const QJsonArray& params);

    // for tests: compute the display for a frequency
    void setFrequency(double freq);

    static QString midiNoteName(int note, bool flat = false);

signals:
    void activeChanged();
    void pitchChanged();
    void stringsChanged();
    void muteWhileTuningChanged();
    void switcherChanged();

private:
    void connectParameters();
    void mute();
    void unmute();
    void update();
    void updateStrings();
    double reference() const;
    int temperament() const;
    int tuning() const;

    Machine& m_machine;
    RpcClient& m_rpc;
    bool m_active = false;
    double m_freq = 0;
    bool m_hasPitch = false;
    QString m_noteName;
    int m_octave = 0;
    double m_cents = 0;
    double m_range = 50;
    QStringList m_strings;
    QList<int> m_stringNotes;
    bool m_flat = false;
    int m_stringIndex = -1;
    bool m_muteWhileTuning = true;
    // engine state to restore, empty when not muted by the tuner
    QString m_savedState;
    QString m_switcherState = QStringLiteral("normal_mode");
    QString m_switcherBank;
    QString m_switcherPreset;
};
