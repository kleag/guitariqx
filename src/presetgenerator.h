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

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class Machine;
class Parameter;
class QNetworkReply;

/*
 * Preset generator (Gx.presetGenerator): asks a language model for the
 * settings that recreate a sound the user describes ("the solo of
 * Comfortably Numb"), applies them to the rack and saves them as a new
 * preset of the bank the user chose, which becomes the current one: the
 * preset loaded before is left as it was saved.
 *
 * The model is reached through the OpenAI compatible chat completions API
 * (OpenAI, OpenRouter, Mistral, Groq, Ollama, Pollinations...). The
 * default is Pollinations' keyless endpoint, free but limited to one
 * answer every 45 s per address; a user's own endpoint, model and API key
 * are kept in QSettings (the key in plain text).
 *
 * Everything the model answers is checked against the catalog it was sent
 * (the units that can be added, their parameters, the amplifier's): the
 * engine crashes on unknown ids, so nothing else reaches it.
 */
class PresetGenerator : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Gx.presetGenerator")
    Q_PROPERTY(QString endpoint READ endpoint WRITE setEndpoint NOTIFY settingsChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY settingsChanged)
    Q_PROPERTY(QString apiKey READ apiKey WRITE setApiKey NOTIFY settingsChanged)
    Q_PROPERTY(bool usingDefault READ usingDefault NOTIFY settingsChanged)
    Q_PROPERTY(QString defaultEndpoint READ defaultEndpoint CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    PresetGenerator(Machine& machine, QObject *parent = nullptr);

    QString endpoint() const { return m_endpoint; }
    void setEndpoint(const QString& url);
    QString model() const { return m_model; }
    void setModel(const QString& model);
    QString apiKey() const { return m_apiKey; }
    void setApiKey(const QString& key);
    bool usingDefault() const;
    static QString defaultEndpoint();
    // back to the free endpoint and model, without API key
    Q_INVOKABLE void resetSettings();

    bool busy() const { return !m_reply.isNull(); }
    // Sends the description to the model; the result, saved as a new
    // preset of bank, comes with finished(). Returns an error text, or an
    // empty string when the request was sent.
    Q_INVOKABLE QString generate(const QString& description, const QString& bank);
    Q_INVOKABLE void cancel();

    // What the model is told (the instructions and the catalog of units
    // and parameters of the connected engine)
    QString systemPrompt() const;
    // Applies an answer of the model to the engine: the rack is changed to
    // hold the chosen units (the others are removed), their parameters
    // and the amplifier's are reset then set. Returns the problems found
    // (unknown units or parameters, values out of place), which are
    // skipped; error is set when nothing could be applied.
    QStringList apply(const QJsonObject& preset, QString *error = nullptr);
    // the JSON object in a model's answer (code fences, reasoning or
    // prose around it are skipped); empty if there is none
    static QJsonObject extractJson(const QString& text);

signals:
    void settingsChanged();
    void busyChanged();
    // ok: the preset was applied and saved; name is the name it was saved
    // under (the model's proposal, made unique in the bank), notes the
    // model's explanation, warnings what was skipped
    void finished(bool ok, const QString& name, const QString& notes, const QStringList& warnings,
                  const QString& error);

private:
    struct Unit {
        QString id;
        bool stereo = false;
        // parameter name in the prompt (id without the unit prefix)
        QStringList keys;
    };
    QList<Unit> catalog() const;
    QList<Parameter*> unitParameters(const QString& unit, const QSet<QString>& unitIds) const;
    QStringList ampParameters() const;
    void onReply(QNetworkReply *reply);
    void save();

    Machine& m_machine;
    QNetworkAccessManager m_nam;
    QPointer<QNetworkReply> m_reply;
    QString m_endpoint;
    QString m_model;
    QString m_apiKey;
    // where the preset is saved
    QString m_bank;
};
