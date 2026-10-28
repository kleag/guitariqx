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

#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

/*
 * Client side mirror of a gx_engine::Parameter, built from one entry of
 * the "parameterlist" RPC result.
 *
 * value is a double for float parameters, an int for int and enum
 * parameters (enums hold the index, not the value id string), a bool for
 * switches and a string for string parameters. Setting value from QML
 * sends the change to the engine; changes reported by the engine update
 * it without sending anything back.
 */
class Parameter : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Parameters are created by the Machine")
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString group READ group CONSTANT)
    Q_PROPERTY(Kind kind READ kind CONSTANT)
    Q_PROPERTY(double lower READ lower CONSTANT)
    Q_PROPERTY(double upper READ upper CONSTANT)
    Q_PROPERTY(double step READ step CONSTANT)
    Q_PROPERTY(bool logDisplay READ logDisplay CONSTANT)
    Q_PROPERTY(bool isOutput READ isOutput CONSTANT)
    Q_PROPERTY(QStringList valueLabels READ valueLabels CONSTANT)
    Q_PROPERTY(QVariant value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double normalized READ normalized WRITE setNormalized NOTIFY valueChanged)
    Q_PROPERTY(QString displayValue READ displayValue NOTIFY valueChanged)
public:
    enum Kind { Float, Int, Bool, Enum, String, Other };
    Q_ENUM(Kind)

    // tag is the type name preceding the entry in the parameterlist array
    static Parameter *fromJson(const QString& tag, const QJsonObject& entry, QObject *parent);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QString group() const { return m_group; }
    Kind kind() const { return m_kind; }
    double lower() const { return m_lower; }
    double upper() const { return m_upper; }
    double step() const { return m_step; }
    bool logDisplay() const { return m_log; }
    bool isOutput() const { return m_output; }
    bool isMaxlevel() const { return m_maxlevel; }
    bool isControllable() const { return m_controllable; }
    QStringList valueLabels() const { return m_valueLabels; }

    QVariant value() const { return m_value; }
    void setValue(const QVariant& v);
    double normalized() const;
    void setNormalized(double n);
    QString displayValue() const;

    Q_INVOKABLE void reset();

    // value as reported by the engine (a "set" notification or "get")
    void updateFromEngine(const QJsonValue& v);
    // value encoded for the "set" RPC
    QJsonValue toJson() const;

signals:
    void valueChanged();
    // emitted on user changes only, the Machine sends them to the engine
    void changeRequested(Parameter *p);

private:
    explicit Parameter(QObject *parent) : QObject(parent) {}
    QVariant convert(const QJsonValue& v) const;
    QVariant clamp(const QVariant& v) const;

    QString m_id;
    QString m_name;
    QString m_group;
    Kind m_kind = Other;
    double m_lower = 0;
    double m_upper = 1;
    double m_step = 0;
    bool m_log = false;
    bool m_output = false;
    bool m_maxlevel = false;
    bool m_controllable = true;
    QStringList m_valueIds;
    QStringList m_valueLabels;
    QVariant m_value;
    QVariant m_std;
};
