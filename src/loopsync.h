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

#include <QElapsedTimer>
#include <QJsonArray>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QtQml/qqmlregistration.h>

#include <functional>

class Machine;
class Parameter;
class QTimer;
class RpcClient;

/*
 * Keeps the looper (dubber, dubber_st) in time with the drum sequencer
 * (seq), Gx.loopSync.
 *
 * Both run on the engine's sample clock, so a loop drifts from the drums
 * only because its length is not a whole number of beats. The engine has
 * no tempo sync, but the length a tape plays is set by its clip parameter
 * (dubber.clipN, a percentage of the recorded length that the engine
 * accepts with any fraction), so once a recording stops the tape is cut to
 * the nearest whole number of beats, to the sample:
 *
 * - the recorded length R comes from dubber.barN, the time left on the
 *   tape: (4194304 - R) / rate seconds; the sample rate, which no RPC
 *   reports, is the standard one matching the full tape time, and R, a
 *   whole number of buffers minus one, is snapped to a multiple of 64
 *   minus one when that is within the precision of the value;
 * - a drum step lasts int(60 * rate / (bpm * tact)) samples
 *   (DrumSequencer::compute), a beat tact steps;
 * - with clips at 0 and speed at 0, a tape plays samples 0 to
 *   int(R * clip / 100) + 1 (LiveLooper::compute), hence the clip.
 *
 * The loop also keeps the drums' phase when it plays from the end of the
 * recording, that is when play was on as the recording started (play
 * then reads behind the recording and starts again at sample 0 two
 * samples after it stops): a first, shorter pass makes the next start
 * fall on a whole number of loop lengths after the start of the
 * recording, then the tape gets its final length. Recording from here
 * switches play on; when a stop is asked a little before the last beat,
 * it is sent just after it so that nothing is cut.
 *
 * FloatParameter::set ignores changes smaller than the step (1 %), so a
 * clip close to the current one goes through a lower value first, above
 * the play position so that the tape does not jump.
 */
class LoopSync : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use Gx.loopSync")
    // the user wants the looper kept in time with the drums (QSettings)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    // the drum sequencer is in the rack and on
    Q_PROPERTY(bool drumsRunning READ drumsRunning NOTIFY drumsRunningChanged)
    // what was done last, for the looper's panel
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    LoopSync(Machine& machine, RpcClient& rpc, QObject *parent = nullptr);
    ~LoopSync() override;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    bool drumsRunning() const;
    QString status() const { return m_status; }
    // Machine::onParameterChangeRequested asks first: true when the change
    // was sent (or deferred) here
    bool interceptChange(Parameter *p);

    // the arithmetic, public for the tests
    static constexpr int TapeSize = 4194304;
    // standard rate whose full tape time, minus the seconds elapsed since
    // the tape was empty, is closest to bar (0 if none is close)
    static int sampleRate(double bar, double elapsed);
    // recorded length from the time left on a tape
    static int recordedLength(double bar, int rate);
    // samples of a drum beat
    static int beatLength(double bpm, int tact, int rate);
    // last sample played with clip (LiveLooper::compute, float arithmetic)
    static int clipEnd(int recorded, double clip);
    // clip making the tape play samples 0..end + 1
    static double clipFor(int recorded, int end);

signals:
    void enabledChanged();
    void drumsRunningChanged();
    void statusChanged();

private:
    struct Tape {
        QString unit;
        int n = 0;
        enum State { Idle, Recording, StopPending, Fitting } state = Idle;
        // play read behind the recording from its start
        bool lockstep = false;
        double started = 0;   // ms of m_clock
        double stopped = 0;
        // last change of rec sent from here: polls answered before it
        // may still report the old value
        double sent = -1e9;
        quint64 generation = 0;
        QTimer *stopTimer = nullptr;
        QPointer<Parameter> rec;
        QString id(const char *name) const;
    };

    double now() const { return m_clock.nsecsElapsed() / 1e6; }
    Tape *tapeOf(Parameter *p);
    void bind();
    void updateWatch();
    void setStatus(const QString& s);
    void onRecChanged(Tape *t);
    void startRecording(Tape *t, bool here);
    void stopRecording(Tape *t);
    void onStopped(Tape *t, double when);
    void readRecordedLength(Tape *t, quint64 gen, double last, int tries);
    void fit(Tape *t, double bar);
    void applyClip(Tape *t, quint64 gen, int recorded, double target, int tries, std::function<void()> done);
    void send(const QJsonArray& pairs);
    void get(const QStringList& ids, std::function<void(const QJsonObject&)> handler);
    bool hasTapeParams(const Tape *t) const;
    double paramValue(const QString& id) const;

    Machine& m_machine;
    RpcClient& m_rpc;
    QElapsedTimer m_clock;
    QVector<Tape*> m_tapes;
    QSet<QString> m_watched;
    QPointer<Parameter> m_seqOn;
    bool m_enabled = false;
    bool m_drumsRunning = false;
    int m_rate = 0;
    QString m_status;
};
