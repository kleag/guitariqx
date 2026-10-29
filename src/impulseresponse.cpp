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

#include "impulseresponse.h"
#include "machine.h"
#include "parameter.h"

#include <QJsonArray>
#include <algorithm>
#include <cmath>

ImpulseResponse::ImpulseResponse(QObject *parent) : QObject(parent) {
    // the IR folders behind %S and %U come with the file places
    if (Machine *m = Machine::instance()) {
        connect(m, &Machine::filePlacesChanged, this, [this]() {
            if (m_param && !m_modified && m_irDir.startsWith(QLatin1Char('%'))) {
                reload();
            }
        });
    }
}

double ImpulseResponse::gainAt(const QVector<Point>& line, int i) {
    if (line.isEmpty()) {
        return 0;
    }
    if (i <= line.first().i) {
        return line.first().g;
    }
    for (int n = 1; n < line.size(); ++n) {
        const Point& b = line.at(n);
        if (i <= b.i) {
            const Point& a = line.at(n - 1);
            return a.g + (b.g - a.g) * double(i - a.i) / double(b.i - a.i);
        }
    }
    return line.last().g;
}

double ImpulseResponse::normalizedGain(const QVector<float>& samples, int channels, int offset, int length,
                                       const QVector<Point>& line) {
    if (channels <= 0) {
        return 1;
    }
    const int frames = samples.size() / channels;
    double sum = 0;
    for (int i = std::max(0, offset); i < std::min(frames, offset + length); ++i) {
        const double g = line.isEmpty() ? 1 : std::pow(10, gainAt(line, i) / 20);
        for (int c = 0; c < channels; ++c) {
            const double v = samples.at(i * channels + c) * g;
            sum += v * v;
        }
    }
    const double gain = std::sqrt(sum / channels);
    return gain != 0 ? 1 / gain : 1;
}

QVector<ImpulseResponse::Point> ImpulseResponse::validLine(QVector<Point> line, int frames) {
    if (line.isEmpty() || frames < 2) {
        return {};
    }
    const int last = frames - 1;
    QVector<Point> out;
    for (Point p : line) {
        p.i = std::clamp(p.i, 0, last);
        p.g = std::clamp(p.g, minGain, maxGain);
        if (!out.isEmpty() && p.i <= out.last().i) {
            continue;
        }
        out.append(p);
    }
    if (out.first().i != 0) {
        out.prepend({0, out.first().g});
    }
    if (out.last().i != last) {
        out.append({last, out.last().g});
    }
    return out;
}

void ImpulseResponse::setUnit(const QString& unit) {
    if (unit == m_unit) {
        return;
    }
    m_unit = unit;
    if (m_param) {
        disconnect(m_param, nullptr, this, nullptr);
    }
    Machine *m = Machine::instance();
    m_param = m ? m->param(unit + QStringLiteral(".convolver")) : nullptr;
    if (m_param) {
        // presets, the file button and other clients change the settings
        connect(m_param, &Parameter::valueChanged, this, [this]() {
            if (!m_modified) {
                reload();
            }
        });
    }
    emit unitChanged();
    reload();
}

void ImpulseResponse::reload() {
    m_modified = false;
    if (!m_param) {
        m_error = tr("No convolver in unit %1").arg(m_unit);
        emit fileLoaded();
        return;
    }
    const QVariantMap v = m_param->value().toMap();
    m_irFile = v.value(QStringLiteral("jconv.IRFile")).toString();
    m_irDir = v.value(QStringLiteral("jconv.IRDir")).toString();
    QString file;
    if (!m_irFile.isEmpty()) {
        file = Machine::instance()->resolveIrDir(m_irDir) + QLatin1Char('/') + m_irFile;
    }
    if (file.startsWith(QLatin1Char('%'))) {
        // the folders of %S and %U are not known yet: read again with the
        // file places (read_audio answers invalid JSON for a missing file)
        m_file = file;
        m_samples.clear();
        m_error = tr("Waiting for the IR folders of the engine");
        emit fileLoaded();
        return;
    }
    if (file != m_file || m_samples.isEmpty()) {
        readFile(file);
    } else {
        readSettings();
    }
}

void ImpulseResponse::readFile(const QString& file) {
    m_file = file;
    m_samples.clear();
    m_frames = m_channels = m_rate = 0;
    m_error.clear();
    const int request = ++m_request;
    if (file.isEmpty()) {
        m_error = tr("No impulse response file");
        readSettings();
        emit fileLoaded();
        return;
    }
    QPointer<ImpulseResponse> self(this);
    Machine::instance()->readAudio(file, [self, request, file](const QJsonArray& a) {
        if (!self || request != self->m_request) {
            return;
        }
        // [size, channels, type, form, rate, [samples...]]
        const int frames = a.at(0).toInt();
        const int channels = a.at(1).toInt();
        const QJsonArray s = a.at(5).toArray();
        if (a.size() < 6 || channels <= 0 || frames <= 0 || s.size() < frames * channels) {
            self->m_error = tr("Cannot read the audio file %1").arg(file);
        } else {
            self->m_frames = frames;
            self->m_channels = channels;
            self->m_rate = a.at(4).toInt();
            self->m_samples.resize(frames * channels);
            for (int i = 0; i < frames * channels; ++i) {
                self->m_samples[i] = float(s.at(i).toDouble());
            }
        }
        self->readSettings();
        emit self->fileLoaded();
    });
}

// like IRWindow::load_state / load_data
void ImpulseResponse::readSettings() {
    const QVariantMap v = m_param ? m_param->value().toMap() : QVariantMap();
    const int last = std::max(0, m_frames - 1);
    m_offset = std::clamp(v.value(QStringLiteral("jconv.Offset")).toInt(), 0, last);
    m_length = v.value(QStringLiteral("jconv.Length")).toInt();
    if (m_length <= 0 || m_offset + m_length > m_frames) {
        m_length = m_frames - m_offset;
    }
    m_delay = std::max(0, v.value(QStringLiteral("jconv.Delay")).toInt());
    m_gainCor = v.value(QStringLiteral("jconv.GainCor"), 1).toDouble() != 0;
    QVector<Point> line;
    for (const QVariant& p : v.value(QStringLiteral("jconv.gainline")).toList()) {
        const QVariantList l = p.toList();
        if (l.size() >= 2) {
            line.append({int(std::lround(l.at(0).toDouble())), l.at(1).toDouble()});
        }
    }
    m_line = validLine(line, m_frames);
    m_modified = false;
    emit settingsChanged();
}

void ImpulseResponse::changed() {
    m_modified = true;
    emit settingsChanged();
}

void ImpulseResponse::setOffset(int v) {
    v = std::clamp(v, 0, std::max(0, m_frames - 1));
    if (v == m_offset) {
        return;
    }
    // the end stays in place
    const int end = m_offset + m_length;
    m_offset = v;
    m_length = std::max(1, end - v);
    if (m_offset + m_length > m_frames) {
        m_length = m_frames - m_offset;
    }
    changed();
}

void ImpulseResponse::setLength(int v) {
    v = std::clamp(v, 1, std::max(1, m_frames - m_offset));
    if (v == m_length) {
        return;
    }
    m_length = v;
    changed();
}

void ImpulseResponse::setDelay(int v) {
    // gx_head's delay spin goes up to 10 s
    v = std::clamp(v, 0, std::max(m_rate, 48000) * 10);
    if (v == m_delay) {
        return;
    }
    m_delay = v;
    changed();
}

void ImpulseResponse::setGainCorrection(bool on) {
    if (on == m_gainCor) {
        return;
    }
    m_gainCor = on;
    changed();
}

QVariantList ImpulseResponse::gainLine() const {
    QVariantList l;
    for (const Point& p : m_line) {
        l.append(QVariantMap{{QStringLiteral("i"), p.i}, {QStringLiteral("g"), p.g}});
    }
    return l;
}

int ImpulseResponse::addPoint(int i, double g) {
    if (m_frames < 3) {
        return -1;
    }
    if (m_line.isEmpty()) {
        m_line = {{0, 0}, {m_frames - 1, 0}};
    }
    i = std::clamp(i, 1, m_frames - 2);
    g = std::clamp(g, minGain, maxGain);
    int n = 1;
    while (n < m_line.size() && m_line.at(n).i < i) {
        ++n;
    }
    if (m_line.at(n).i == i || m_line.at(n - 1).i == i) {
        return -1;
    }
    m_line.insert(n, {i, g});
    changed();
    return n;
}

int ImpulseResponse::movePoint(int index, int i, double g) {
    if (index < 0 || index >= m_line.size()) {
        return -1;
    }
    Point& p = m_line[index];
    if (index > 0 && index < m_line.size() - 1) {
        // between its neighbours
        p.i = std::clamp(i, m_line.at(index - 1).i + 1, m_line.at(index + 1).i - 1);
    }
    p.g = std::clamp(g, minGain, maxGain);
    changed();
    return index;
}

void ImpulseResponse::removePoint(int index) {
    // the end points stay
    if (index <= 0 || index >= m_line.size() - 1) {
        return;
    }
    m_line.remove(index);
    changed();
}

void ImpulseResponse::resetSettings() {
    m_offset = 0;
    m_length = m_frames;
    m_delay = 0;
    m_gainCor = true;
    m_line.clear();
    changed();
}

QVariantMap ImpulseResponse::settings() const {
    QVariantList line;
    for (const Point& p : m_line) {
        line.append(QVariant(QVariantList{p.i, p.g}));
    }
    return QVariantMap{
        {QStringLiteral("jconv.IRFile"), m_irFile},
        {QStringLiteral("jconv.IRDir"), m_irDir},
        {QStringLiteral("jconv.Gain"), normalizedGain(m_samples, m_channels, m_offset, m_length, m_line)},
        {QStringLiteral("jconv.GainCor"), m_gainCor ? 1 : 0},
        {QStringLiteral("jconv.Offset"), m_offset},
        {QStringLiteral("jconv.Length"), m_length},
        {QStringLiteral("jconv.Delay"), m_delay},
        {QStringLiteral("jconv.gainline"), line},
    };
}

void ImpulseResponse::apply() {
    if (!m_param || !loaded()) {
        return;
    }
    // the parameter's valueChanged must not read the settings back
    m_modified = true;
    m_param->setValue(settings());
    m_modified = false;
    emit settingsChanged();
}
