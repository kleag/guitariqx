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

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class Parameter;

/*
 * The impulse response of a convolver unit (jconv, jconv_mono) and its
 * settings, edited like in gx_head's IR window (IRWindow in
 * gx_jconv_settings.cpp): the part of the file used (offset, length), a
 * delay, and a gain line.
 *
 * The settings are the value of the <unit>.convolver parameter
 * (GxJConvSettings::writeJSON), the samples come from read_audio.
 *
 * The gain line is a list of points (sample index in the file, gain in
 * dB) between which the engine interpolates the gain in dB
 * (GxConvolver::compute_interpolation). It is empty (no gain change) or
 * goes from sample 0 to the last sample with strictly increasing
 * indexes: otherwise the engine reads outside of it.
 */
class ImpulseResponse : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // jconv or jconv_mono
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY unitChanged)
    Q_PROPERTY(QString file READ file NOTIFY fileLoaded)
    Q_PROPERTY(bool loaded READ loaded NOTIFY fileLoaded)
    // empty, or why the file could not be read
    Q_PROPERTY(QString error READ error NOTIFY fileLoaded)
    Q_PROPERTY(int frames READ frames NOTIFY fileLoaded)
    Q_PROPERTY(int channels READ channels NOTIFY fileLoaded)
    Q_PROPERTY(int rate READ rate NOTIFY fileLoaded)
    Q_PROPERTY(int offset READ offset WRITE setOffset NOTIFY settingsChanged)
    Q_PROPERTY(int length READ length WRITE setLength NOTIFY settingsChanged)
    Q_PROPERTY(int delay READ delay WRITE setDelay NOTIFY settingsChanged)
    Q_PROPERTY(bool gainCorrection READ gainCorrection WRITE setGainCorrection NOTIFY settingsChanged)
    // [{i, g}, ...]
    Q_PROPERTY(QVariantList gainLine READ gainLine NOTIFY settingsChanged)
    // the settings differ from the engine's
    Q_PROPERTY(bool modified READ modified NOTIFY settingsChanged)
public:
    explicit ImpulseResponse(QObject *parent = nullptr);

    struct Point {
        int i;
        double g;
        bool operator==(const Point& o) const { return i == o.i && g == o.g; }
    };
    // range of the gain line, the one of the wave view
    static constexpr double minGain = -120;
    static constexpr double maxGain = 0;

    // IRWindow::calc_normalized_gain: the gain that gives the used part
    // of the response, gain line applied, an energy of 1 per channel
    static double normalizedGain(const QVector<float>& samples, int channels, int offset, int length,
                                 const QVector<Point>& line);
    // gain in dB at sample i of a gain line
    static double gainAt(const QVector<Point>& line, int i);
    // a gain line the engine accepts for a file of this size
    static QVector<Point> validLine(QVector<Point> line, int frames);

    QString unit() const { return m_unit; }
    void setUnit(const QString& unit);
    QString file() const { return m_file; }
    bool loaded() const { return !m_samples.isEmpty(); }
    QString error() const { return m_error; }
    int frames() const { return m_frames; }
    int channels() const { return m_channels; }
    int rate() const { return m_rate; }
    const QVector<float>& samples() const { return m_samples; }

    int offset() const { return m_offset; }
    void setOffset(int v);
    int length() const { return m_length; }
    void setLength(int v);
    int delay() const { return m_delay; }
    void setDelay(int v);
    bool gainCorrection() const { return m_gainCor; }
    void setGainCorrection(bool on);
    QVariantList gainLine() const;
    const QVector<Point>& points() const { return m_line; }
    bool modified() const { return m_modified; }

    // read the settings of the unit again (and the file when it changed)
    Q_INVOKABLE void reload();
    // add a point to the gain line; returns its index (-1: not possible)
    Q_INVOKABLE int addPoint(int i, double g);
    // move a point: the end points only change their gain; returns the
    // point's index
    Q_INVOKABLE int movePoint(int index, int i, double g);
    Q_INVOKABLE void removePoint(int index);
    Q_INVOKABLE double gainAt(int i) const { return gainAt(m_line, i); }
    // like "reset" in gx_head: the whole file, no delay, no gain line
    Q_INVOKABLE void resetSettings();
    // send the settings to the engine
    Q_INVOKABLE void apply();

    // the value of the convolver parameter for these settings
    QVariantMap settings() const;

signals:
    void unitChanged();
    void fileLoaded();
    void settingsChanged();

private:
    void readSettings();
    void readFile(const QString& file);
    void changed();

    QString m_unit;
    QPointer<Parameter> m_param;
    QString m_file;
    // IRFile and IRDir as the engine sent them (IRDir can start with a
    // symbol: %S factory folder, %U user folder)
    QString m_irFile;
    QString m_irDir;
    QString m_error;
    QVector<float> m_samples;
    int m_frames = 0;
    int m_channels = 0;
    int m_rate = 0;
    int m_offset = 0;
    int m_length = 0;
    int m_delay = 0;
    bool m_gainCor = true;
    QVector<Point> m_line;
    bool m_modified = false;
    int m_request = 0;
};
