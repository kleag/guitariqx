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

#include "oscilloscope.h"

#include <algorithm>

void Oscilloscope::setMulBuffer(int n) {
    n = std::clamp(n, 1, maxMulBuffer);
    if (n == m_mulBuffer) {
        return;
    }
    m_mulBuffer = n;
    emit mulBufferChanged();
    emit mulBufferRequested(n);
}

void Oscilloscope::updateMulBuffer(int n) {
    n = std::clamp(n, 1, maxMulBuffer);
    if (n != m_mulBuffer) {
        m_mulBuffer = n;
        emit mulBufferChanged();
    }
}

void Oscilloscope::update(const QJsonArray& info) {
    if (info.size() < 6) {
        return;
    }
    m_load = info.at(0).toInt();
    m_frames = info.at(1).toInt();
    m_realtime = info.at(2).toInt() != 0;
    m_jackBufferSize = info.at(3).toInt();
    const QJsonArray s = info.at(5).toArray();
    m_samples.resize(s.size());
    for (int i = 0; i < s.size(); ++i) {
        m_samples[i] = float(s.at(i).toDouble());
    }
    emit frameChanged();
}
