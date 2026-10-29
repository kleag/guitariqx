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

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class Machine;
class RpcClient;

/*
 * MIDI controller assignments and MIDI learn, the guitariqx counterpart of
 * gx_main_midi::MidiConnect and MidiControllerTable in gx_head.
 *
 * Learning follows the gx_head protocol: the engine is put in config mode
 * (midi_set_config_mode), it then records the last controller that moved
 * (polled with midi_get_config_mode, or set by hand with
 * midi_set_current_control), midi_modifyCurrent assigns it to the
 * parameter and config mode is switched off again.
 */
class MidiMap : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use Gx.midi")
    // incremented on every change of the assignments, for QML bindings
    Q_PROPERTY(int revision READ revision NOTIFY assignmentsChanged)
    Q_PROPERTY(QVariantList assignments READ assignments NOTIFY assignmentsChanged)
    // UI state: tapping a control opens the learn dialog
    Q_PROPERTY(bool learnMode READ learnMode WRITE setLearnMode NOTIFY learnModeChanged)
    Q_PROPERTY(bool learning READ learning NOTIFY learnChanged)
    Q_PROPERTY(QString learnParam READ learnParam NOTIFY learnChanged)
    Q_PROPERTY(int learnControl READ learnControl NOTIFY learnChanged)
public:
    // toggle behaviours of switch parameters (Parameter::toggle_type)
    enum ToggleType { OnOff = 0, Constant = 1, Toggle = 2, Commander = 3 };
    Q_ENUM(ToggleType)

    MidiMap(Machine& machine, RpcClient& rpc, QObject *parent = nullptr);

    int revision() const { return m_revision; }
    QVariantList assignments() const { return m_assignments; }
    bool learnMode() const { return m_learnMode; }
    void setLearnMode(bool on);
    bool learning() const { return m_learning; }
    QString learnParam() const { return m_learnParam; }
    int learnControl() const { return m_learnControl; }

    // controller number assigned to a parameter, -1 if none
    Q_INVOKABLE int controllerFor(const QString& id) const;
    // {ctl, lower, upper} or {ctl, toggle, behaviour}; empty if none
    Q_INVOKABLE QVariantMap assignment(const QString& id) const;
    Q_INVOKABLE static QString controllerName(int ctl);
    // parameters that can be assigned (continuous, enum and switch)
    Q_INVOKABLE bool isLearnable(const QString& id) const;
    // continuous and enum parameters take a range, switches a toggle mode
    Q_INVOKABLE bool usesRange(const QString& id) const;

    Q_INVOKABLE void startLearn(const QString& id);
    Q_INVOKABLE void setLearnControl(int ctl);
    Q_INVOKABLE void commitLearn(double lower, double upper, bool toggle, int behaviour);
    Q_INVOKABLE void cancelLearn();
    Q_INVOKABLE void remove(const QString& id);

    // called by the Machine
    void engineReady();
    // leaves MIDI config mode if learning (quitting)
    void stopLearn();
    void engineLost();
    void midiChanged(const QJsonArray& params);

signals:
    void assignmentsChanged();
    void learnModeChanged();
    void learnChanged();

private:
    void load(const QJsonArray& map);
    void poll();

    Machine& m_machine;
    RpcClient& m_rpc;
    QHash<QString, QVariantMap> m_byParam;
    QVariantList m_assignments;
    int m_revision = 0;
    bool m_learnMode = false;
    bool m_learning = false;
    QString m_learnParam;
    int m_learnControl = -1;
    QTimer m_pollTimer;
};
