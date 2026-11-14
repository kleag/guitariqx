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

// Drum patterns of the PresetGenerator (see presetgenerator.h): a language
// model writes the steps of the drum sequencer (seq, DrumSequencer in
// gx_internal_plugins.cpp) to accompany a song.
//
// The sequencer plays a loop of seq.asequences steps (24 to 240, by 4);
// seq.tact is the number of steps per beat (its labels "1/4" to "4/4" are
// not time signatures): a step lasts 60 / (bpm * tact) s. The toms are one
// voice tuned to 90 (tom), 128 (tom1) or 150 Hz (tom2), the highest wins
// when several are set on a step. The lines are saved in the presets and
// in the unit presets of seq (SeqParameter is a preset parameter), so a
// pattern needs nothing more than its parameters set.

#include "presetgenerator.h"

#include "machine.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace {

const char *kDrumInstructions = R"(You program the drum machine of the guitarix amp simulator to accompany a guitarist playing the song the user names (or a style). Write the drum part of the song as a short loop, true to its groove, tempo and feel.

The drum machine has six voices: "kick", "snare", "hat" (closed hi-hat), "tom" (low tom), "tom1" (mid tom), "tom2" (high tom); only one tom sounds on a step. It plays a grid of steps:
- "subdivision" is the number of steps per beat: 1 = quarter notes, 2 = eighth notes, 3 = eighth-note triplets (shuffle, swing, 12/8 and 6/8 feels), 4 = sixteenth notes.
- The loop length in steps must be a multiple of 4 between 24 and 240. Write whole bars: 2 bars of 4/4 in sixteenths = 32 steps, 4 bars of 4/4 in eighths = 32, 2 bars of 4/4 in triplets = 24, 2 bars of 3/4 in sixteenths = 24, 4 bars of 4/4 in sixteenths = 64.
- "bpm" is the tempo in beats per minute, 24 to 360.
- Write each voice as a string of one character per step, "x" for a hit and "." for a rest, all voices the same length; "|" between bars is allowed and ignored. Leave out the voices that do not play.
- "levels" (optional) are voice volumes in dB, -60 to 40, default -20.
- "humanize" (optional) 1 varies the timing slightly.

Answer with one JSON object only, no other text:
{"name": "short pattern name in plain ASCII", "bpm": 120, "subdivision": 4, "lines": {"kick": "x.......x.x.....|x.......x.......", "snare": "....x.......x...|....x.......x..x", "hat": "x.x.x.x.x.x.x.x.|x.x.x.x.x.x.x.x."}, "levels": {"kick": -18}, "humanize": 0, "notes": "one or two sentences: the song's tempo and groove"}
)";

const char *const kDrums[] = {"tom", "tom1", "tom2", "kick", "snare", "hat"};

// the gain parameter of each voice
QString levelId(const QString& drum) {
    if (drum == QLatin1String("tom")) {
        return QStringLiteral("seq.tom.dsp.Gain");
    }
    if (drum == QLatin1String("tom1")) {
        return QStringLiteral("seq.tom.dsp.Gain1");
    }
    if (drum == QLatin1String("tom2")) {
        return QStringLiteral("seq.tom.dsp.Gain2");
    }
    if (drum == QLatin1String("hat")) {
        return QStringLiteral("seq.hat_closed.dsp.Gain");
    }
    return QStringLiteral("seq.%1.dsp.Gain").arg(drum);
}

// the names a model may use for a voice
QString drumName(const QString& key) {
    const QString k = key.trimmed().toLower().remove(QLatin1Char(' ')).remove(QLatin1Char('_')).remove(QLatin1Char('-'));
    for (const char *d : kDrums) {
        if (k == QLatin1String(d)) {
            return k;
        }
    }
    if (k == QLatin1String("bass") || k == QLatin1String("bassdrum") || k == QLatin1String("bd")) {
        return QStringLiteral("kick");
    }
    if (k == QLatin1String("sd") || k == QLatin1String("snaredrum")) {
        return QStringLiteral("snare");
    }
    if (k == QLatin1String("hihat") || k == QLatin1String("hh") || k == QLatin1String("closedhat")
        || k == QLatin1String("closedhihat") || k == QLatin1String("hats")) {
        return QStringLiteral("hat");
    }
    if (k == QLatin1String("lowtom") || k == QLatin1String("floortom") || k == QLatin1String("tom0")) {
        return QStringLiteral("tom");
    }
    if (k == QLatin1String("midtom")) {
        return QStringLiteral("tom1");
    }
    if (k == QLatin1String("hightom") || k == QLatin1String("hitom")) {
        return QStringLiteral("tom2");
    }
    return QString();
}

// the steps of a line written as text ("x..x|x...", bars split at "|")
// or as an array ([1, 0, true...], a single bar); ok is false when
// something could not be read
QList<QList<int>> parseLine(const QJsonValue& v, bool *ok) {
    QList<QList<int>> bars{{}};
    *ok = true;
    if (v.isArray()) {
        for (const QJsonValue& s : v.toArray()) {
            if (s.isBool()) {
                bars.last().append(s.toBool() ? 1 : 0);
            } else if (s.isDouble()) {
                bars.last().append(s.toDouble() != 0 ? 1 : 0);
            } else if (s.isString()) {
                const QString t = s.toString().trimmed().toLower();
                bars.last().append(t == QLatin1String("x") || t == QLatin1String("1") || t == QLatin1String("o") ? 1 : 0);
            } else {
                bars.last().append(0);
                *ok = false;
            }
        }
        return bars;
    }
    if (!v.isString()) {
        *ok = false;
        return {};
    }
    for (const QChar c : v.toString()) {
        switch (c.toLower().unicode()) {
        case 'x':
        case 'o':
        case '1':
        case '*':
        case '#':
            bars.last().append(1);
            break;
        case '.':
        case '-':
        case '0':
        case '_':
            bars.last().append(0);
            break;
        case '|':
            if (!bars.last().isEmpty()) {
                bars.append(QList<int>());
            }
            break;
        case ' ':
        case '\t':
            break;
        default:
            // an accent, a ghost note...
            bars.last().append(c.isLetter() ? 1 : 0);
            *ok = false;
        }
    }
    if (bars.last().isEmpty()) {
        bars.removeLast();
    }
    return bars;
}

// the most frequent of lengths, the longest of the most frequent
int mostCommon(const QList<int>& lengths) {
    QMap<int, int> count;
    for (int n : lengths) {
        ++count[n];
    }
    int best = 0, times = 0;
    for (auto it = count.cbegin(); it != count.cend(); ++it) {
        if (it.value() >= times) {
            best = it.key();
            times = it.value();
        }
    }
    return best;
}

// l made n steps long: cut or completed with rests
void fit(QList<int>& l, int n) {
    while (l.size() > n) {
        l.removeLast();
    }
    while (l.size() < n) {
        l.append(0);
    }
}

QString number(double v) {
    return QString::number(v, 'g', 6);
}

} // anonymous namespace

QString PresetGenerator::drumPrompt() const {
    return QString::fromUtf8(kDrumInstructions);
}

QString PresetGenerator::generateDrums(const QString& description) {
    const QString d = description.simplified();
    if (d.isEmpty()) {
        return tr("Name the song first");
    }
    if (busy()) {
        return tr("The model is already busy");
    }
    if (!m_machine.ready()) {
        return tr("Not connected to the engine");
    }
    if (m_machine.pluginInfo(QStringLiteral("seq")).isEmpty() || !m_machine.hasParam(QStringLiteral("seq.asequences"))) {
        return tr("The engine has no drum sequencer");
    }
    return send(Request::Drums, drumPrompt(), d);
}

QStringList PresetGenerator::applyDrums(const QJsonObject& pattern, QString *error) {
    QStringList warnings;
    auto fail = [&](const QString& e) {
        if (error) {
            *error = e;
        }
        return warnings;
    };
    Parameter *steps = m_machine.param(QStringLiteral("seq.asequences"));
    if (!steps) {
        return fail(tr("The engine has no drum sequencer"));
    }

    // the voices, as bars
    QHash<QString, QList<QList<int>>> given;
    QJsonObject source = pattern.value(QStringLiteral("lines")).toObject();
    const bool inLines = !source.isEmpty();
    if (!inLines) {
        // the voices at the top level
        source = pattern;
    }
    for (auto it = source.begin(); it != source.end(); ++it) {
        const QString drum = drumName(it.key());
        if (drum.isEmpty()) {
            if (inLines) {
                warnings.append(tr("Unknown drum \"%1\" skipped").arg(it.key()));
            }
            continue;
        }
        bool ok = true;
        const QList<QList<int>> bars = parseLine(it.value(), &ok);
        if (!ok) {
            warnings.append(tr("Unexpected steps of \"%1\" read as rests or hits").arg(it.key()));
        }
        if (given.contains(drum)) {
            warnings.append(tr("Drum \"%1\" is listed twice").arg(drum));
            continue;
        }
        given.insert(drum, bars);
    }

    // a miscounted bar must not shift the next ones: the bars of lines
    // written in bars get the usual bar length
    QList<int> barLengths;
    for (const QList<QList<int>>& bars : std::as_const(given)) {
        if (bars.size() > 1) {
            for (const QList<int>& bar : bars) {
                barLengths.append(bar.size());
            }
        }
    }
    const int barLength = mostCommon(barLengths);
    QHash<QString, QList<int>> lines;
    QList<int> lengths;
    for (auto it = given.cbegin(); it != given.cend(); ++it) {
        QList<int> l;
        bool fixed = false;
        for (QList<int> bar : it.value()) {
            if (it.value().size() > 1 && bar.size() != barLength) {
                fit(bar, barLength);
                fixed = true;
            }
            l += bar;
        }
        if (fixed) {
            warnings.append(tr("Bars of \"%1\" made %2 steps long").arg(it.key()).arg(barLength));
        }
        if (!l.isEmpty()) {
            lines.insert(it.key(), l);
            lengths.append(l.size());
        }
    }
    if (std::none_of(lines.cbegin(), lines.cend(), [](const QList<int>& l) { return l.contains(1); })) {
        return fail(tr("The model's answer holds no drum steps"));
    }

    // the length of most lines, made a multiple of the sequencer's step;
    // shorter figures (a bar of hi-hat) that divide it are repeated, the
    // other lines cut or completed with rests
    const int lower = int(steps->lower());
    const int upper = int(steps->upper());
    const int quantum = std::max(1, int(steps->step()));
    const int common = mostCommon(lengths);
    const int length = std::max(quantum, int(std::lround(double(common) / quantum)) * quantum);
    for (auto it = lines.begin(); it != lines.end(); ++it) {
        QList<int>& l = it.value();
        if (l.size() < common && common % l.size() == 0) {
            const QList<int> figure = l;
            while (l.size() < common) {
                l += figure;
            }
        } else if (l.size() != common) {
            warnings.append(tr("Line \"%1\" has %2 steps instead of %3, cut or completed with rests")
                                .arg(it.key()).arg(l.size()).arg(common));
        }
        fit(l, length);
    }
    if (length != common) {
        warnings.append(tr("The pattern of %1 steps is made %2 steps long").arg(common).arg(length));
    }
    // repeated up to the sequencer's minimum
    int n = length;
    int repeat = 1;
    while (n < lower) {
        n += length;
        ++repeat;
    }
    if (n > upper) {
        n = lower + (upper - lower) / quantum * quantum;
        warnings.append(tr("The pattern of %1 steps is cut to %2").arg(length).arg(n));
    }

    // tempo and subdivision
    Parameter *tact = m_machine.param(QStringLiteral("seq.tact"));
    const QJsonValue sub = pattern.contains(QStringLiteral("subdivision")) ? pattern.value(QStringLiteral("subdivision"))
                                                                           : pattern.value(QStringLiteral("steps_per_beat"));
    if (tact && !sub.isUndefined() && !sub.isNull()) {
        int t = 0;
        if (sub.isDouble()) {
            t = int(std::lround(sub.toDouble()));
        } else if (sub.isString()) {
            // "4", "4/4" (the labels of seq.tact)
            static const QRegularExpression lead(QStringLiteral("^\\s*(\\d+)"));
            const QRegularExpressionMatch m = lead.match(sub.toString());
            t = m.hasMatch() ? m.captured(1).toInt() : 0;
        }
        if (t >= int(tact->lower()) && t <= int(tact->upper())) {
            tact->setValue(t);
        } else {
            const QByteArray json = QJsonDocument(QJsonArray{sub}).toJson(QJsonDocument::Compact);
            warnings.append(tr("Subdivision %1 skipped").arg(QString::fromUtf8(json.mid(1, json.size() - 2))));
        }
    }
    if (Parameter *bpm = m_machine.param(QStringLiteral("seq.bpm"))) {
        const QJsonValue v = pattern.value(QStringLiteral("bpm"));
        const double b = v.isString() ? v.toString().toDouble() : v.toDouble(std::nan(""));
        if (std::isfinite(b) && b > 0) {
            const double c = std::clamp(std::round(b), bpm->lower(), bpm->upper());
            if (c != std::round(b)) {
                warnings.append(tr("Tempo %1 is out of range, set to %2").arg(number(b), number(c)));
            }
            bpm->setValue(c);
        } else if (!v.isUndefined()) {
            warnings.append(tr("Tempo skipped"));
        }
    }
    if (Parameter *hum = m_machine.param(QStringLiteral("seq.hum"))) {
        const QJsonValue v = pattern.value(QStringLiteral("humanize"));
        if (v.isBool() || v.isDouble()) {
            hum->setValue(v.isBool() ? v.toBool() : v.toDouble() != 0);
        }
    }
    const QJsonObject levels = pattern.value(QStringLiteral("levels")).toObject();
    for (auto it = levels.begin(); it != levels.end(); ++it) {
        const QString drum = drumName(it.key());
        Parameter *p = drum.isEmpty() ? nullptr : m_machine.param(levelId(drum));
        const double v = it.value().isString() ? it.value().toString().toDouble() : it.value().toDouble(std::nan(""));
        if (!p || !std::isfinite(v)) {
            warnings.append(tr("Level of \"%1\" skipped").arg(it.key()));
            continue;
        }
        p->setValue(std::clamp(v, p->lower(), p->upper()));
    }

    // the unit, then its lines, all n steps long (an empty or short line
    // shortens the loop: min_seq_size)
    const bool inRack = m_machine.monoRack().contains(QStringLiteral("seq"));
    if (!inRack) {
        m_machine.insertUnit(QStringLiteral("seq"), QString(), false);
    }
    if (Parameter *on = m_machine.param(QStringLiteral("seq.on_off"))) {
        on->setValue(true);
    }
    steps->setValue(n);
    for (const char *d : kDrums) {
        const QString drum = QString::fromLatin1(d);
        Parameter *p = m_machine.param(QStringLiteral("seq.sequencer.") + drum);
        if (!p) {
            continue;
        }
        const QList<int> l = lines.value(drum);
        QVariantList line;
        for (int i = 0; i < n; ++i) {
            line.append(l.isEmpty() ? 0 : l.at(i % l.size()));
        }
        p->setValue(QVariantMap{{QStringLiteral("seq.seqline"), line}});
    }
    if (repeat > 1) {
        warnings.append(tr("The pattern of %1 steps is played %2 times in a loop of %3").arg(length).arg(repeat).arg(n));
    }
    return warnings;
}

void PresetGenerator::finishDrums(const QJsonObject& pattern) {
    QString error;
    const QStringList warnings = applyDrums(pattern, &error);
    const QString notes = pattern.value(QStringLiteral("notes")).toString().trimmed();
    if (!error.isEmpty()) {
        emit drumsFinished(false, QString(), notes, warnings, error);
        return;
    }
    QString base = asciiName(pattern.value(QStringLiteral("name")).toString());
    if (base.isEmpty()) {
        base = tr("Drums");
    }
    // plugin_preset_list_save overwrites a preset of the same name
    m_machine.loadUnitPresets(QStringLiteral("seq"), [this, base, notes, warnings](const QVariantList& presets) {
        QSet<QString> names;
        for (const QVariant& v : presets) {
            const QVariantMap m = v.toMap();
            if (!m.value(QStringLiteral("factory")).toBool()) {
                names.insert(m.value(QStringLiteral("name")).toString());
            }
        }
        QString name = base;
        for (int i = 2; names.contains(name); ++i) {
            name = QStringLiteral("%1 %2").arg(base).arg(i);
        }
        const QString error = m_machine.saveUnitPreset(QStringLiteral("seq"), name);
        emit drumsFinished(error.isEmpty(), error.isEmpty() ? name : QString(), notes, warnings,
                           error.isEmpty() ? QString() : tr("The pattern is applied but not saved: %1").arg(error));
    });
}
