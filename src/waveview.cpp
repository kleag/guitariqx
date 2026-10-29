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

#include "waveview.h"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

WaveView::WaveView(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
    connect(this, &WaveView::styleChanged, this, [this]() { update(); });
    connect(this, &QQuickItem::widthChanged, this, [this]() { m_envelopeValid = false; });
}

void WaveView::connectSource(QObject *o) {
    if (!o) {
        return;
    }
    if (auto *osc = qobject_cast<Oscilloscope*>(o)) {
        connect(osc, &Oscilloscope::frameChanged, this, [this]() { update(); });
    } else if (auto *ir = qobject_cast<ImpulseResponse*>(o)) {
        connect(ir, &ImpulseResponse::fileLoaded, this, [this]() {
            m_envelopeValid = false;
            update();
        });
        connect(ir, &ImpulseResponse::settingsChanged, this, [this]() { update(); });
    }
}

void WaveView::setOscilloscope(Oscilloscope *o) {
    if (o == m_osc) {
        return;
    }
    if (m_osc) {
        disconnect(m_osc, nullptr, this, nullptr);
    }
    m_osc = o;
    connectSource(o);
    emit sourceChanged();
    update();
}

void WaveView::setImpulse(ImpulseResponse *ir) {
    if (ir == m_ir) {
        return;
    }
    if (m_ir) {
        disconnect(m_ir, nullptr, this, nullptr);
    }
    m_ir = ir;
    m_envelopeValid = false;
    connectSource(ir);
    emit sourceChanged();
    update();
}

void WaveView::setScale(double s) {
    s = std::clamp(s, 0.1, 100.0);
    if (s != m_scale) {
        m_scale = s;
        emit styleChanged();
    }
}

void WaveView::setSelectedPoint(int i) {
    if (i != m_selected) {
        m_selected = i;
        emit styleChanged();
    }
}

double WaveView::xForSample(double i) const {
    const int frames = m_ir ? m_ir->frames() : 0;
    return frames > 1 ? i * (width() - 1) / (frames - 1) : 0;
}

int WaveView::sampleAt(double x) const {
    const int frames = m_ir ? m_ir->frames() : 0;
    if (frames < 2 || width() <= 1) {
        return 0;
    }
    return std::clamp(int(std::lround(x * (frames - 1) / (width() - 1))), 0, frames - 1);
}

double WaveView::yForGain(double g) const {
    return (ImpulseResponse::maxGain - g) * (height() - 1) / (ImpulseResponse::maxGain - ImpulseResponse::minGain);
}

double WaveView::gainAt(double y) const {
    const double g = ImpulseResponse::maxGain
        - y * (ImpulseResponse::maxGain - ImpulseResponse::minGain) / std::max(1.0, height() - 1);
    return std::clamp(g, ImpulseResponse::minGain, ImpulseResponse::maxGain);
}

void WaveView::paint(QPainter *p) {
    p->fillRect(QRectF(0, 0, width(), height()), m_background);
    if (m_ir) {
        paintImpulse(p);
    } else {
        paintScope(p);
    }
}

void WaveView::paintScope(QPainter *p) {
    const double w = width();
    const double h = height();
    p->setPen(QPen(m_grid, 1));
    for (int i = 1; i < 8; ++i) {
        const double x = std::round(w * i / 8) + 0.5;
        p->drawLine(QPointF(x, 0), QPointF(x, h));
    }
    for (int i = 1; i < 4; ++i) {
        const double y = std::round(h * i / 4) + 0.5;
        p->drawLine(QPointF(0, y), QPointF(w, y));
    }
    const QVector<float> empty;
    const QVector<float>& s = m_osc ? m_osc->samples() : empty;
    p->setPen(QPen(m_wave, 1.5));
    if (s.size() < 2) {
        p->drawLine(QPointF(0, h / 2), QPointF(w, h / 2));
        return;
    }
    // at most two points per pixel
    const int n = s.size();
    const int step = std::max(1, int(n / std::max(1.0, 2 * w)));
    QPolygonF line;
    line.reserve(n / step + 1);
    for (int i = 0; i < n; i += step) {
        const double v = std::clamp(double(s.at(i)) * m_scale, -1.0, 1.0);
        line.append(QPointF(i * w / (n - 1), (1 - v) * h / 2));
    }
    p->drawPolyline(line);
}

void WaveView::paintImpulse(QPainter *p) {
    const double w = width();
    const double h = height();
    const int columns = std::max(1, int(w));
    // dB grid, every 20 dB
    p->setPen(QPen(m_grid, 1));
    for (double g = ImpulseResponse::maxGain - 20; g > ImpulseResponse::minGain; g -= 20) {
        const double y = std::round(yForGain(g)) + 0.5;
        p->drawLine(QPointF(0, y), QPointF(w, y));
    }
    if (!m_ir->loaded()) {
        return;
    }
    const int frames = m_ir->frames();
    const int channels = m_ir->channels();
    if (!m_envelopeValid || m_envelope.size() != columns) {
        const QVector<float>& s = m_ir->samples();
        m_envelope.fill(float(ImpulseResponse::minGain), columns);
        for (int c = 0; c < columns; ++c) {
            const int from = int(std::floor(double(c) * frames / columns));
            const int to = std::min(frames, std::max(from + 1, int(std::floor(double(c + 1) * frames / columns))));
            float peak = 0;
            for (int i = from; i < to; ++i) {
                for (int ch = 0; ch < channels; ++ch) {
                    peak = std::max(peak, std::abs(s.at(i * channels + ch)));
                }
            }
            if (peak > 0) {
                m_envelope[c] = float(std::max(ImpulseResponse::minGain, 20 * std::log10(double(peak))));
            }
        }
        m_envelopeValid = true;
    }
    QPainterPath area;
    area.moveTo(0, h);
    for (int c = 0; c < columns; ++c) {
        area.lineTo(c + 0.5, yForGain(m_envelope.at(c)));
    }
    area.lineTo(w, h);
    area.closeSubpath();
    QColor fill = m_wave;
    fill.setAlpha(0x60);
    p->fillPath(area, fill);
    p->setPen(QPen(m_wave, 1));
    p->drawPath(area);

    // parts of the file left out
    const double x0 = xForSample(m_ir->offset());
    const double x1 = xForSample(m_ir->offset() + m_ir->length() - 1);
    p->fillRect(QRectF(0, 0, x0, h), m_shade);
    p->fillRect(QRectF(x1, 0, w - x1, h), m_shade);

    // gain line: flat at 0 dB when empty
    const QVector<ImpulseResponse::Point>& line = m_ir->points();
    p->setPen(QPen(m_gainLine, 2));
    if (line.isEmpty()) {
        p->drawLine(QPointF(0, yForGain(0) + 1), QPointF(w, yForGain(0) + 1));
        return;
    }
    QPolygonF poly;
    for (const ImpulseResponse::Point& pt : line) {
        poly.append(QPointF(xForSample(pt.i), yForGain(pt.g)));
    }
    p->drawPolyline(poly);
    p->setPen(Qt::NoPen);
    for (int i = 0; i < poly.size(); ++i) {
        const bool sel = i == m_selected;
        p->setBrush(sel ? QColor(Qt::white) : m_gainLine);
        const double r = sel ? 7 : 5;
        p->drawEllipse(poly.at(i), r, r);
    }
}
