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

#include "impulseresponse.h"
#include "oscilloscope.h"

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QVector>
#include <QtQml/qqmlregistration.h>

/*
 * Draws a waveform, the counterpart of GxWaveView (oscilloscope) and
 * GxIREdit (impulse response) of libgxw:
 * - oscilloscope set: the last frame of Gx.oscilloscope, -1 to 1
 * - impulse set: the peak level of the response in dB (minGain to
 *   maxGain of ImpulseResponse), the unused parts shaded, and the gain
 *   line with its points (QML adds the touch handling)
 */
class WaveView : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Oscilloscope *oscilloscope READ oscilloscope WRITE setOscilloscope NOTIFY sourceChanged)
    Q_PROPERTY(ImpulseResponse *impulse READ impulse WRITE setImpulse NOTIFY sourceChanged)
    // vertical zoom of the oscilloscope
    Q_PROPERTY(double scale READ scale WRITE setScale NOTIFY styleChanged)
    // point of the gain line drawn as selected (-1: none)
    Q_PROPERTY(int selectedPoint READ selectedPoint WRITE setSelectedPoint NOTIFY styleChanged)
    Q_PROPERTY(QColor backgroundColor MEMBER m_background NOTIFY styleChanged)
    Q_PROPERTY(QColor waveColor MEMBER m_wave NOTIFY styleChanged)
    Q_PROPERTY(QColor gridColor MEMBER m_grid NOTIFY styleChanged)
    Q_PROPERTY(QColor gainLineColor MEMBER m_gainLine NOTIFY styleChanged)
    Q_PROPERTY(QColor shadeColor MEMBER m_shade NOTIFY styleChanged)
public:
    explicit WaveView(QQuickItem *parent = nullptr);

    Oscilloscope *oscilloscope() const { return m_osc; }
    void setOscilloscope(Oscilloscope *o);
    ImpulseResponse *impulse() const { return m_ir; }
    void setImpulse(ImpulseResponse *ir);
    double scale() const { return m_scale; }
    void setScale(double s);
    int selectedPoint() const { return m_selected; }
    void setSelectedPoint(int i);

    // coordinates of the impulse response view
    Q_INVOKABLE double xForSample(double i) const;
    Q_INVOKABLE int sampleAt(double x) const;
    Q_INVOKABLE double yForGain(double g) const;
    Q_INVOKABLE double gainAt(double y) const;

    void paint(QPainter *p) override;

signals:
    void sourceChanged();
    void styleChanged();

private:
    void paintScope(QPainter *p);
    void paintImpulse(QPainter *p);
    void connectSource(QObject *o);

    QPointer<Oscilloscope> m_osc;
    QPointer<ImpulseResponse> m_ir;
    // peak level in dB of each pixel column, computed once per file and
    // width
    QVector<float> m_envelope;
    bool m_envelopeValid = false;
    double m_scale = 1;
    int m_selected = -1;
    QColor m_background = QColor(0x10, 0x10, 0x10);
    QColor m_wave = QColor(0x50, 0xd0, 0x70);
    QColor m_grid = QColor(0x40, 0x40, 0x40);
    QColor m_gainLine = QColor(0xf0, 0xc0, 0x30);
    QColor m_shade = QColor(0, 0, 0, 0x90);
};
