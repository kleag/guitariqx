/*
 * Copyright (C) 2026 Guitarix developers
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

#include "parameter.h"

#include <QJsonArray>
#include <algorithm>
#include <cmath>

namespace {

// gx_engine::Parameter::ctrl_type
enum { ctl_none, ctl_continuous, ctl_switch, ctl_enum };
// gx_engine::Parameter::display_flags
enum { dtp_log = 1 };

} // anonymous namespace

/*
 * Entries look like (see Parameter::serializeJSON in gx_paramtable.cpp):
 *   "Float", {"Parameter": {...}, "lower": .., "upper": .., "step": .., "value": .., "std_value": ..}
 *   "Enum", {"IntParameter": {"Parameter": {...}, "lower": .., ...}, "value_names": [id, label, ...]}
 *   "FloatEnum", {"FloatParameter": {...}, "value_names": [...]}
 */
Parameter *Parameter::fromJson(const QString& tag, const QJsonObject& entry, QObject *parent) {
    QJsonObject body = entry;
    QJsonArray valueNames;
    if (body.contains(QStringLiteral("value_names"))) {
        valueNames = body.value(QStringLiteral("value_names")).toArray();
        if (body.contains(QStringLiteral("IntParameter"))) {
            body = body.value(QStringLiteral("IntParameter")).toObject();
        } else if (body.contains(QStringLiteral("FloatParameter"))) {
            body = body.value(QStringLiteral("FloatParameter")).toObject();
        }
    }
    const QJsonObject base = body.value(QStringLiteral("Parameter")).toObject();
    if (base.isEmpty()) {
        return nullptr;
    }
    Parameter *p = new Parameter(parent);
    p->m_id = base.value(QStringLiteral("id")).toString();
    p->m_name = base.value(QStringLiteral("name")).toString();
    p->m_group = base.value(QStringLiteral("group")).toString();
    p->m_log = base.value(QStringLiteral("d_flags")).toInt() & dtp_log;
    p->m_output = base.value(QStringLiteral("output")).toInt();
    p->m_maxlevel = base.value(QStringLiteral("maxlevel")).toInt();
    p->m_controllable = !base.value(QStringLiteral("non_controllable")).toInt();
    const int ctype = base.value(QStringLiteral("c_type")).toInt();

    if (tag == QLatin1String("Float")) {
        p->m_kind = Float;
    } else if (tag == QLatin1String("Int")) {
        p->m_kind = Int;
    } else if (tag == QLatin1String("Bool")) {
        p->m_kind = Bool;
    } else if (tag == QLatin1String("Enum") || tag == QLatin1String("FloatEnum")) {
        p->m_kind = Enum;
    } else if (tag == QLatin1String("String")) {
        p->m_kind = String;
    } else {
        p->m_kind = Other;
    }
    if (p->m_kind == Float && ctype == ctl_switch) {
        // a few float parameters act as switches
        p->m_kind = Bool;
    }

    p->m_lower = body.value(QStringLiteral("lower")).toDouble(0);
    p->m_upper = body.value(QStringLiteral("upper")).toDouble(p->m_kind == Bool ? 1 : 0);
    p->m_step = body.value(QStringLiteral("step")).toDouble(p->m_kind == Float ? 0 : 1);
    for (int i = 0; i + 1 < valueNames.size(); i += 2) {
        p->m_valueIds.append(valueNames.at(i).toString());
        p->m_valueLabels.append(valueNames.at(i + 1).toString());
    }
    if (p->m_kind == Enum && !p->m_valueIds.isEmpty() && p->m_upper < p->m_lower + 1) {
        p->m_upper = p->m_lower + p->m_valueIds.size() - 1;
    }
    p->m_value = p->convert(body.value(QStringLiteral("value")));
    p->m_std = p->convert(body.value(QStringLiteral("std_value")));
    return p;
}

QVariant Parameter::convert(const QJsonValue& v) const {
    switch (m_kind) {
    case Float:
        return v.toDouble();
    case Int:
        return v.toInt();
    case Bool:
        return v.toDouble() != 0 || v.toBool();
    case Enum:
        if (v.isString()) {
            // "get" returns the value id instead of the index
            int idx = m_valueIds.indexOf(v.toString());
            return idx < 0 ? int(m_lower) : int(m_lower) + idx;
        }
        return int(std::lround(v.toDouble()));
    case String:
        return v.toString();
    case Other:
        break;
    }
    return v.toVariant();
}

QVariant Parameter::clamp(const QVariant& v) const {
    switch (m_kind) {
    case Float: {
        double d = v.toDouble();
        if (m_upper > m_lower) {
            d = std::max(m_lower, std::min(m_upper, d));
        }
        return d;
    }
    case Int:
    case Enum: {
        int i = int(std::lround(v.toDouble()));
        if (m_upper > m_lower) {
            i = std::max(int(m_lower), std::min(int(m_upper), i));
        }
        return i;
    }
    case Bool:
        return v.toBool();
    case String:
        return v.toString();
    case Other:
        break;
    }
    return v;
}

void Parameter::setValue(const QVariant& v) {
    QVariant nv = clamp(v);
    if (nv == m_value) {
        return;
    }
    m_value = nv;
    emit valueChanged();
    emit changeRequested(this);
}

void Parameter::updateFromEngine(const QJsonValue& v) {
    QVariant nv = convert(v);
    if (nv == m_value) {
        return;
    }
    m_value = nv;
    emit valueChanged();
}

void Parameter::reset() {
    setValue(m_std);
}

QJsonValue Parameter::toJson() const {
    switch (m_kind) {
    case Float:
        return m_value.toDouble();
    case Int:
    case Enum:
        return m_value.toInt();
    case Bool:
        return m_value.toBool() ? 1 : 0;
    case String:
        return m_value.toString();
    case Other:
        break;
    }
    return QJsonValue::fromVariant(m_value);
}

double Parameter::normalized() const {
    if (m_upper <= m_lower) {
        return 0;
    }
    double v = m_value.toDouble();
    if (m_log && m_lower > 0) {
        return std::log(v / m_lower) / std::log(m_upper / m_lower);
    }
    return (v - m_lower) / (m_upper - m_lower);
}

void Parameter::setNormalized(double n) {
    n = std::max(0.0, std::min(1.0, n));
    double v;
    if (m_log && m_lower > 0) {
        v = m_lower * std::pow(m_upper / m_lower, n);
    } else {
        v = m_lower + n * (m_upper - m_lower);
    }
    if (m_step > 0 && m_kind == Float) {
        v = m_lower + std::round((v - m_lower) / m_step) * m_step;
    }
    setValue(v);
}

QString Parameter::displayValue() const {
    switch (m_kind) {
    case Float: {
        double v = m_value.toDouble();
        int prec = 0;
        if (m_step > 0 && m_step < 1) {
            prec = std::min(3, int(std::ceil(-std::log10(m_step) - 1e-9)));
        } else if (m_step <= 0) {
            prec = 2;
        }
        return QString::number(v, 'f', prec);
    }
    case Int:
        return QString::number(m_value.toInt());
    case Enum: {
        int idx = m_value.toInt() - int(m_lower);
        if (idx >= 0 && idx < m_valueLabels.size()) {
            return m_valueLabels.at(idx);
        }
        return QString::number(m_value.toInt());
    }
    case Bool:
        return m_value.toBool() ? tr("on") : tr("off");
    case String:
        return m_value.toString();
    case Other:
        break;
    }
    return QString();
}
