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
#include <QVector>
#include <QtQml/qqmlregistration.h>

/*
 * Last frame of the oscilloscope unit (Gx.oscilloscope).
 *
 * The engine sends it as the value of the output parameter
 * "oscilloscope.info" (OscilloscopeInfo::writeJSON in
 * gx_internal_ui_plugins.cpp) when get_updates polls it:
 *   [load, frames, is_rt, jack buffer size, size, [samples...]]
 * The buffer holds mulBuffer jack periods (set_oscilloscope_mul_buffer).
 */
class Oscilloscope : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Gx.oscilloscope")
    Q_PROPERTY(int load READ load NOTIFY frameChanged)
    Q_PROPERTY(int frames READ frames NOTIFY frameChanged)
    Q_PROPERTY(bool realtime READ realtime NOTIFY frameChanged)
    Q_PROPERTY(int jackBufferSize READ jackBufferSize NOTIFY frameChanged)
    Q_PROPERTY(int size READ size NOTIFY frameChanged)
    Q_PROPERTY(int mulBuffer READ mulBuffer WRITE setMulBuffer NOTIFY mulBufferChanged)
public:
    explicit Oscilloscope(QObject *parent) : QObject(parent) {}

    static constexpr int maxMulBuffer = 6;

    int load() const { return m_load; }
    int frames() const { return m_frames; }
    bool realtime() const { return m_realtime; }
    int jackBufferSize() const { return m_jackBufferSize; }
    int size() const { return m_samples.size(); }
    const QVector<float>& samples() const { return m_samples; }

    int mulBuffer() const { return m_mulBuffer; }
    // from the UI: sent to the engine
    void setMulBuffer(int n);
    // from the engine
    void updateMulBuffer(int n);

    // a value of oscilloscope.info
    void update(const QJsonArray& info);

signals:
    void frameChanged();
    void mulBufferChanged();
    void mulBufferRequested(int n);

private:
    QVector<float> m_samples;
    int m_load = 0;
    int m_frames = 0;
    bool m_realtime = false;
    int m_jackBufferSize = 0;
    int m_mulBuffer = 1;
};
