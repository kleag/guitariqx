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

#include "loopsync.h"

#include "machine.h"
#include "parameter.h"
#include "rpcclient.h"

#include <QJsonObject>
#include <QSettings>
#include <QTimer>

#include <cmath>

namespace {

const char *enabledKey = "looper/syncToDrums";

// the time a poll answer, or a notification, comes after the change
const double observedDelay = 30;
// polls sent before a change of rec here may still report the old value
const double staleWindow = 400;
// a deferred stop comes this long after the last beat, so that the
// recording is not shorter than the loop (the change is applied at the
// next buffer, up to a buffer late)
const double stopMargin = 60;
// time left to send the clip of the first pass before the tape reaches it
const double sendTime = 250;

} // anonymous namespace

QString LoopSync::Tape::id(const char *name) const {
    return QStringLiteral("%1.%2%3").arg(unit, QLatin1String(name)).arg(n);
}

LoopSync::LoopSync(Machine& machine, RpcClient& rpc, QObject *parent)
    : QObject(parent),
      m_machine(machine),
      m_rpc(rpc),
      m_enabled(QSettings().value(QLatin1String(enabledKey), false).toBool()) {
    m_clock.start();
    for (const char *unit : {"dubber", "dubber_st"}) {
        for (int n = 1; n <= 4; ++n) {
            Tape *t = new Tape;
            t->unit = QLatin1String(unit);
            t->n = n;
            t->stopTimer = new QTimer(this);
            t->stopTimer->setSingleShot(true);
            t->stopTimer->setTimerType(Qt::PreciseTimer);
            connect(t->stopTimer, &QTimer::timeout, this, [this, t]() {
                if (t->state == Tape::StopPending) {
                    t->state = Tape::Recording;
                    stopRecording(t);
                }
            });
            m_tapes.append(t);
        }
    }
    connect(&m_machine, &Machine::readyChanged, this, &LoopSync::bind);
    connect(&m_machine, &Machine::pluginsChanged, this, &LoopSync::bind);
    connect(&m_machine, &Machine::rackChanged, this, &LoopSync::updateWatch);
}

LoopSync::~LoopSync() {
    qDeleteAll(m_tapes);
}

void LoopSync::setEnabled(bool on) {
    if (on == m_enabled) {
        return;
    }
    m_enabled = on;
    QSettings().setValue(QLatin1String(enabledKey), on);
    if (!on) {
        for (Tape *t : std::as_const(m_tapes)) {
            if (t->state == Tape::StopPending) {
                t->state = Tape::Recording;
                stopRecording(t);
            }
            t->state = Tape::Idle;
            ++t->generation;
        }
        setStatus(QString());
    }
    emit enabledChanged();
    updateWatch();
}

bool LoopSync::drumsRunning() const {
    return m_seqOn && m_seqOn->value().toBool() && m_machine.monoRack().contains(QStringLiteral("seq"));
}

void LoopSync::setStatus(const QString& s) {
    if (s != m_status) {
        m_status = s;
        emit statusChanged();
    }
}

// the parameters are (re)loaded: Parameter objects may be new ones
void LoopSync::bind() {
    if (m_seqOn) {
        disconnect(m_seqOn, nullptr, this, nullptr);
    }
    m_seqOn = m_machine.ready() ? m_machine.param(QStringLiteral("seq.on_off")) : nullptr;
    if (m_seqOn) {
        connect(m_seqOn, &Parameter::valueChanged, this, &LoopSync::updateWatch);
    }
    for (Tape *t : std::as_const(m_tapes)) {
        if (t->rec) {
            disconnect(t->rec, nullptr, this, nullptr);
        }
        t->stopTimer->stop();
        t->state = Tape::Idle;
        ++t->generation;
        t->rec = m_machine.ready() && hasTapeParams(t) ? m_machine.param(t->id("rec")) : nullptr;
        if (t->rec) {
            // after interceptChange() for changes made here
            connect(t->rec, &Parameter::valueChanged, this, [this, t]() { onRecChanged(t); }, Qt::QueuedConnection);
        }
    }
    if (!m_machine.ready()) {
        // the machine keeps its watch counts over reconnections
        return;
    }
    updateWatch();
}

bool LoopSync::hasTapeParams(const Tape *t) const {
    for (const char *name : {"rec", "play", "clip", "clips", "bar", "playh", "speed"}) {
        if (!m_machine.hasParam(t->id(name))) {
            return false;
        }
    }
    return m_machine.hasParam(QStringLiteral("seq.bpm")) && m_machine.hasParam(QStringLiteral("seq.tact"));
}

double LoopSync::paramValue(const QString& id) const {
    Parameter *p = m_machine.param(id);
    return p ? p->value().toDouble() : 0;
}

// rec is polled while the drums run, so that a recording started or
// stopped elsewhere (a MIDI controller, gx_head) is seen
void LoopSync::updateWatch() {
    const bool running = drumsRunning();
    if (running != m_drumsRunning) {
        m_drumsRunning = running;
        emit drumsRunningChanged();
    }
    QSet<QString> wanted;
    if (m_enabled && running && m_machine.ready()) {
        for (const Tape *t : std::as_const(m_tapes)) {
            const bool stereo = t->unit == QLatin1String("dubber_st");
            if (t->rec && (stereo ? m_machine.stereoRack() : m_machine.monoRack()).contains(t->unit)) {
                wanted.insert(t->rec->id());
            }
        }
    }
    for (const QString& id : m_watched - wanted) {
        m_machine.unwatchOutput(id);
    }
    for (const QString& id : wanted - m_watched) {
        m_machine.watchOutput(id);
    }
    m_watched = wanted;
}

LoopSync::Tape *LoopSync::tapeOf(Parameter *p) {
    for (Tape *t : std::as_const(m_tapes)) {
        if (t->rec == p) {
            return t;
        }
    }
    return nullptr;
}

bool LoopSync::interceptChange(Parameter *p) {
    Tape *t = tapeOf(p);
    if (!t || !m_enabled || !drumsRunning()) {
        return false;
    }
    if (paramValue(t->id("speed")) != 0) {
        setStatus(tr("Tape %1 plays at another speed: not kept in time").arg(t->n));
        t->state = Tape::Idle;
        return false;
    }
    if (p->value().toBool()) {
        startRecording(t, true);
        return true;
    }
    if (t->state == Tape::StopPending) {
        // still recording until the end of the beat
        p->updateFromEngine(1);
        return true;
    }
    if (t->state == Tape::Recording && m_rate > 0) {
        const int beat = beatLength(paramValue(QStringLiteral("seq.bpm")), int(paramValue(QStringLiteral("seq.tact"))), m_rate);
        if (beat > 0) {
            const double beatMs = 1000.0 * beat / m_rate;
            const double elapsed = now() - t->started;
            const int beats = std::max(1, int(std::lround(elapsed / beatMs)));
            const double delay = t->started + beats * beatMs + stopMargin - now();
            if (delay > 5) {
                t->state = Tape::StopPending;
                p->updateFromEngine(1);
                t->stopTimer->start(int(std::ceil(delay)));
                setStatus(tr("Tape %1: stopping after beat %2").arg(t->n).arg(beats));
                return true;
            }
        }
    }
    stopRecording(t);
    return true;
}

void LoopSync::onRecChanged(Tape *t) {
    if (!t->rec || !m_enabled || !drumsRunning() || now() - t->sent < staleWindow) {
        return;
    }
    const bool on = t->rec->value().toBool();
    if (on && (t->state == Tape::Idle || t->state == Tape::Fitting)) {
        if (paramValue(t->id("speed")) == 0) {
            startRecording(t, false);
        }
    } else if (!on && (t->state == Tape::Recording || t->state == Tape::StopPending)) {
        // stopped elsewhere, or by the engine (tape full)
        t->stopTimer->stop();
        onStopped(t, now() - observedDelay);
    }
}

void LoopSync::startRecording(Tape *t, bool here) {
    t->stopTimer->stop();
    const quint64 gen = ++t->generation;
    t->state = Tape::Recording;
    if (here) {
        t->started = now();
        t->sent = t->started;
        // the whole tape, whatever the clip values were (98 then 100 ends
        // at 100 whichever change is too small to be taken); play is on
        // before rec so that it reads behind the recording from its start
        send({t->id("clip"), 98, t->id("clip"), 100, t->id("clips"), 2, t->id("clips"), 0,
              t->id("play"), 1, t->id("rec"), 1});
        t->lockstep = true;
    } else {
        t->started = now() - observedDelay;
        t->lockstep = false;
        get({t->id("play"), t->id("clip"), t->id("clips")}, [this, t, gen](const QJsonObject& o) {
            if (gen != t->generation) {
                return;
            }
            // play on: it went back to 0 with the recording
            t->lockstep = o.value(t->id("play")).toDouble() != 0
                && o.value(t->id("clip")).toDouble() == 100 && o.value(t->id("clips")).toDouble() == 0;
            if (!t->lockstep) {
                send({t->id("clip"), 98, t->id("clip"), 100, t->id("clips"), 2, t->id("clips"), 0,
                      t->id("play"), 1});
            }
        });
    }
    setStatus(tr("Tape %1: recording").arg(t->n));
    // the sample rate, from the time left on the tape (full when the
    // recording started)
    QTimer::singleShot(150, this, [this, t, gen]() {
        if (gen != t->generation) {
            return;
        }
        get({t->id("bar")}, [this, t, gen](const QJsonObject& o) {
            if (gen != t->generation || t->state == Tape::Fitting) {
                return;
            }
            const int rate = sampleRate(o.value(t->id("bar")).toDouble(), (now() - t->started) / 1000);
            if (rate > 0) {
                m_rate = rate;
            }
        });
    });
}

void LoopSync::stopRecording(Tape *t) {
    t->sent = now();
    send({t->id("rec"), 0});
    onStopped(t, t->sent);
}

void LoopSync::onStopped(Tape *t, double when) {
    t->state = Tape::Fitting;
    t->stopped = when;
    const quint64 gen = ++t->generation;
    QTimer::singleShot(150, this, [this, t, gen]() { readRecordedLength(t, gen, -1, 0); });
}

// bar, read until it no longer changes (the engine stops the recording at
// its next buffer)
void LoopSync::readRecordedLength(Tape *t, quint64 gen, double last, int tries) {
    if (gen != t->generation) {
        return;
    }
    get({t->id("bar")}, [this, t, gen, last, tries](const QJsonObject& o) {
        if (gen != t->generation) {
            return;
        }
        const double bar = o.value(t->id("bar")).toDouble();
        if (bar == last || tries >= 8) {
            fit(t, bar);
            return;
        }
        QTimer::singleShot(60, this, [this, t, gen, bar, tries]() { readRecordedLength(t, gen, bar, tries + 1); });
    });
}

void LoopSync::fit(Tape *t, double bar) {
    const quint64 gen = t->generation;
    const double duration = (t->stopped - t->started) / 1000;
    int rate = m_rate;
    if (rate <= 0 || std::abs(double(recordedLength(bar, rate)) / rate - duration) > 1) {
        rate = sampleRate(bar, duration);
    }
    if (rate <= 0) {
        // a tape loaded from a long file is longer than TapeSize
        setStatus(tr("Tape %1: unknown length, not kept in time").arg(t->n));
        t->state = Tape::Idle;
        return;
    }
    m_rate = rate;
    const int recorded = recordedLength(bar, rate);
    const int beat = beatLength(paramValue(QStringLiteral("seq.bpm")), int(paramValue(QStringLiteral("seq.tact"))), rate);
    if (beat <= 0 || recorded <= 0) {
        t->state = Tape::Idle;
        return;
    }
    int beats = int(std::lround(double(recorded) / beat));
    if (qint64(beats) * beat > recorded + 2) {
        --beats;
    }
    if (beats < 1) {
        setStatus(tr("Tape %1: shorter than a beat").arg(t->n));
        t->state = Tape::Idle;
        return;
    }
    const int loop = beats * beat;
    const double finalClip = clipFor(recorded, loop - 2);
    const QString done = beats == 1 ? tr("Tape %1: 1 beat").arg(t->n) : tr("Tape %1: %2 beats").arg(t->n).arg(beats);
    const auto finish = [this, t, gen, done]() {
        if (gen == t->generation) {
            t->state = Tape::Idle;
            setStatus(done);
        }
    };
    // the tape started again at 0 two samples after the recording stopped
    // (R + 2 samples after it started); a first pass to end1 + 1 makes the
    // next start fall at 2 * loop
    const int end1 = 2 * loop - recorded - 4;
    const double played = (now() - t->stopped) * rate / 1000;
    const bool phase = t->lockstep && end1 > 0 && loop >= 0.4 * rate
        && end1 - played > sendTime * rate / 1000;
    if (!phase) {
        applyClip(t, gen, recorded, finalClip, 0, [this, t, gen, finish]() {
            finish();
            if (gen == t->generation && !t->lockstep) {
                setStatus(m_status + QLatin1Char(' ') + tr("(not in phase: start recording here with the drums on)"));
            }
        });
        return;
    }
    applyClip(t, gen, recorded, clipFor(recorded, end1), 0, [this, t, gen, recorded, rate, end1, loop, finalClip, finish]() {
        // a quarter of the loop into the second pass
        const double at = t->stopped + 1000.0 * (end1 + 2) / rate + 250.0 * loop / rate;
        QTimer::singleShot(int(std::max(0.0, at - now())), this, [this, t, gen, recorded, finalClip, finish]() {
            applyClip(t, gen, recorded, finalClip, 0, finish);
        });
    });
}

void LoopSync::applyClip(Tape *t, quint64 gen, int recorded, double target, int tries, std::function<void()> done) {
    if (gen != t->generation) {
        return;
    }
    get({t->id("clip"), t->id("playh")}, [this, t, gen, recorded, target, tries, done](const QJsonObject& o) {
        if (gen != t->generation) {
            return;
        }
        const double current = o.value(t->id("clip")).toDouble(100);
        if (std::abs(target - current) >= 1) {
            send({t->id("clip"), target});
            done();
            return;
        }
        // through a lower value, still above the play position
        const double mid = std::min(current, target) - 1.5;
        const double position = o.value(t->id("playh")).toDouble() / 1000 * recorded;
        if (mid > 0 && clipEnd(recorded, mid) > position + 0.25 * m_rate) {
            send({t->id("clip"), mid, t->id("clip"), target});
            done();
            return;
        }
        if (mid <= 0 || tries >= 40) {
            setStatus(tr("Tape %1: not kept in time").arg(t->n));
            t->state = Tape::Idle;
            return;
        }
        QTimer::singleShot(200, this, [this, t, gen, recorded, target, tries, done]() {
            applyClip(t, gen, recorded, target, tries + 1, done);
        });
    });
}

// one message: the engine applies the changes one after the other, all
// before its next buffer in practice
void LoopSync::send(const QJsonArray& pairs) {
    m_rpc.notify(QStringLiteral("set"), pairs);
    // the last value of each parameter
    for (int i = 0; i + 1 < pairs.size(); i += 2) {
        if (Parameter *p = m_machine.param(pairs.at(i).toString())) {
            p->updateFromEngine(pairs.at(i + 1));
        }
    }
}

void LoopSync::get(const QStringList& ids, std::function<void(const QJsonObject&)> handler) {
    m_rpc.call(QStringLiteral("get"), QJsonArray::fromStringList(ids), [handler](const QJsonValue& r) {
        handler(r.toObject());
    });
}

int LoopSync::sampleRate(double bar, double elapsed) {
    static const int rates[] = {22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000};
    int best = 0;
    double bestDiff = 1;
    for (int r : rates) {
        const double diff = std::abs(double(TapeSize) / r - elapsed - bar);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = r;
        }
    }
    return best;
}

int LoopSync::recordedLength(double bar, int rate) {
    if (rate <= 0) {
        return 0;
    }
    const qint64 r = TapeSize - std::llround(bar * rate);
    // the engine writes 6 significant digits; a recording is a whole number
    // of buffers (whose sizes are multiples of 64 in practice) minus one
    const int digits = bar >= 1 ? int(std::floor(std::log10(bar))) + 1 : 1;
    const double resolution = std::pow(10.0, digits - 6);
    const qint64 tolerance = qint64(std::ceil(0.5 * resolution * rate)) + 1;
    const qint64 snapped = 64 * std::llround((r + 1) / 64.0) - 1;
    if (tolerance < 32 && std::abs(snapped - r) <= tolerance) {
        return int(snapped);
    }
    return int(r);
}

int LoopSync::beatLength(double bpm, int tact, int rate) {
    if (bpm <= 0 || tact <= 0) {
        return 0;
    }
    // DrumSequencer::compute: float parameters, double step length
    const float product = float(bpm) * float(tact);
    const double step = (60.0 / double(product)) * rate;
    return int(step) * tact;
}

int LoopSync::clipEnd(int recorded, double clip) {
    // LiveLooper::compute: float iClip1 = fclip1*0.01; int(iTemp3*iClip1)
    const float fclip = float(clip);
    const float factor = float(fclip * 0.01);
    return int(float(recorded) * factor);
}

double LoopSync::clipFor(int recorded, int end) {
    if (recorded <= 0) {
        return 100;
    }
    double clip = std::min(100.0, (end + 0.5) * 100.0 / recorded);
    const double nudge = 0.1 * 100.0 / recorded;
    for (int i = 0; i < 20; ++i) {
        const int e = clipEnd(recorded, clip);
        if (e == end) {
            break;
        }
        clip += e < end ? nudge : -nudge;
    }
    return std::min(100.0, clip);
}
