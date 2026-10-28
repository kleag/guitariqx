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

    // called by the Machine
    void engineReady();
    void engineLost();
    void tunerChanged(const QJsonArray& params);

    // for tests: compute the display for a frequency
    void setFrequency(double freq);

    static QString midiNoteName(int note, bool flat = false);

signals:
    void activeChanged();
    void pitchChanged();
    void stringsChanged();

private:
    void connectParameters();
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
};
