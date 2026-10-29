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

// The drum sequencer of the seq unit (DrumSequencer in
// gx_internal_plugins.cpp, SEQWindow in gx_sequencer_settings.cpp).
//
// Each drum has a line of steps, the parameter seq.sequencer.<drum>
// ({"seq.seqline": [0 or 1, ...]}). The engine plays as many steps as the
// shortest line has (min_seq_size) and an empty line breaks it: so every
// line always gets seq.asequences steps. A fresh engine has empty lines
// until one is edited.

#include "machine.h"

#include <algorithm>
#include <cmath>

namespace {

const char *const drums[] = {"tom", "tom1", "tom2", "kick", "snare", "hat"};

QVariantList stepsOf(const Parameter *p) {
    return p ? p->value().toMap().value(QStringLiteral("seq.seqline")).toList() : QVariantList();
}

QVariantList resized(const QVariantList& line, int n) {
    QVariantList out;
    for (int i = 0; i < n; ++i) {
        out.append(i < line.size() && line.at(i).toDouble() != 0 ? 1 : 0);
    }
    return out;
}

} // anonymous namespace

QStringList Machine::sequencerDrums() const {
    QStringList l;
    for (const char *d : drums) {
        l.append(QString::fromLatin1(d));
    }
    return l;
}

int Machine::sequencerSteps() const {
    const Parameter *p = m_params.value(QStringLiteral("seq.asequences"));
    return p ? int(std::lround(p->value().toDouble())) : 0;
}

void Machine::fixSequencerLines(int n) {
    for (const char *d : drums) {
        Parameter *p = m_params.value(QStringLiteral("seq.sequencer.") + QLatin1String(d));
        const QVariantList line = stepsOf(p);
        if (p && line.size() != n) {
            p->setValue(QVariantMap{{QStringLiteral("seq.seqline"), resized(line, n)}});
        }
    }
}

void Machine::toggleSequencerStep(const QString& drum, int step) {
    Parameter *p = m_params.value(QStringLiteral("seq.sequencer.") + drum);
    const int n = sequencerSteps();
    if (!p || p->tag() != QLatin1String("Seq") || step < 0 || step >= n) {
        return;
    }
    fixSequencerLines(n);
    QVariantList line = resized(stepsOf(p), n);
    line[step] = line.at(step).toInt() ? 0 : 1;
    p->setValue(QVariantMap{{QStringLiteral("seq.seqline"), line}});
}

void Machine::setSequencerSteps(int n) {
    Parameter *p = m_params.value(QStringLiteral("seq.asequences"));
    if (!p) {
        return;
    }
    // whole beats of 4 steps, as the step of the parameter
    const int lower = int(p->lower());
    const int step = std::max(1, int(p->step()));
    n = std::clamp(lower + (n - lower) / step * step, lower, int(p->upper()));
    p->setValue(n);
    fixSequencerLines(n);
}
