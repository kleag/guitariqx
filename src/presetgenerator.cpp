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

#include "presetgenerator.h"

#include "machine.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <QDebug>
#include <cmath>
#if QT_CONFIG(ssl)
#include <QSslSocket>
#endif

namespace {

// Pollinations' keyless endpoint (gen.pollinations.ai needs a key): one
// answer every 45 s per address, so the whole catalog goes in one request
const char *kDefaultEndpoint = "https://text.pollinations.ai/openai";
const char *kDefaultModel = "openai";
const char *kEndpointKey = "generator/endpoint";
const char *kModelKey = "generator/model";
const char *kApiKeyKey = "generator/apiKey";
const int kTimeout = 180000;

// units that need a file of the user (neural models, impulse responses),
// record, show or play something rather than shape the sound
const QSet<QString>& excludedUnits() {
    static const QSet<QString> s{
        QStringLiteral("jconv"),   QStringLiteral("jconv_mono"), QStringLiteral("dubber"),
        QStringLiteral("dubber_st"), QStringLiteral("seq"),      QStringLiteral("metro"),
        QStringLiteral("recorder"), QStringLiteral("st_recorder"), QStringLiteral("oscilloscope"),
        QStringLiteral("vu"),      QStringLiteral("vu_st"),      QStringLiteral("dout"),
        QStringLiteral("noiseshut"),
    };
    return s;
}

// the amplifier's controls (AmpPanel.qml): its parameters do not share
// the unit's prefix (ampstack)
const char *kAmpParameters[] = {
    "tube.select",         "amp2.stage1.Pregain", "amp2.stage2.gain1", "gxdistortion.drive",
    "gxdistortion.wet_dry", "amp.wet_dry",        "amp.bass_boost.on_off", "bassbooster.Level",
    "con.on_off",          "con.Level",           "amp.feed_on_off",   "amp.out_master",
};

const char *kInstructions = R"(You design guitar sounds for the guitarix amp simulator. Recreate the guitar sound the user describes (a song, an artist, a style) as a guitarix preset.

The signal goes through the mono rack: the units before the amplifier ("pre"), the amplifier (always present), the units after it ("post"), then the stereo rack ("stereo"). Choose the units and set their parameters. Use only the unit ids and parameter names listed below, mono units in "pre" or "post", stereo units in "stereo". Keep the chain realistic: usually 2 to 6 units, drive and fuzz pedals before the amplifier, cabinet and tone units after it, modulation, delay and reverb at the end. Parameters you leave out keep their default value.

Answer with one JSON object only, no other text:
{"name": "short preset name in plain ASCII", "amp": {"parameter": value}, "pre": [{"unit": "id", "params": {"parameter": value}}], "post": [...], "stereo": [...], "notes": "one or two sentences explaining the choices"}

Parameters are written name=range(default): numbers take a value in the range, 0|1 are switches, ["a", "b"] take one of the quoted labels.
)";

QString number(double v) {
    return QString::number(v, 'g', 6);
}

QString describe(const Parameter *p, const QString& name) {
    switch (p->kind()) {
    case Parameter::Bool:
        return QStringLiteral("%1=0|1(%2)").arg(name, p->stdValue().toBool() ? QStringLiteral("1") : QStringLiteral("0"));
    case Parameter::Enum: {
        const QStringList labels = p->valueLabels();
        const int std = p->stdValue().toInt() - int(p->lower());
        const QJsonDocument doc(QJsonArray::fromStringList(labels));
        return QStringLiteral("%1=%2(%3)").arg(name, QString::fromUtf8(doc.toJson(QJsonDocument::Compact)),
                                               std >= 0 && std < labels.size() ? labels.at(std) : QString());
    }
    default:
        return QStringLiteral("%1=%2..%3(%4)").arg(name, number(p->lower()), number(p->upper()),
                                                   number(p->stdValue().toDouble()));
    }
}

// a value of the model for parameter p, as Parameter::setValue takes it
bool convert(const Parameter *p, const QJsonValue& v, QVariant *out) {
    switch (p->kind()) {
    case Parameter::Bool:
        if (v.isBool()) {
            *out = v.toBool();
            return true;
        }
        if (v.isDouble()) {
            *out = v.toDouble() != 0;
            return true;
        }
        if (v.isString()) {
            const QString s = v.toString().trimmed().toLower();
            for (const char *on : {"1", "on", "true", "yes"}) {
                if (s == QLatin1String(on)) {
                    *out = true;
                    return true;
                }
            }
            for (const char *off : {"0", "off", "false", "no"}) {
                if (s == QLatin1String(off)) {
                    *out = false;
                    return true;
                }
            }
        }
        return false;
    case Parameter::Enum: {
        int idx = -1;
        if (v.isString()) {
            idx = p->enumIndex(v.toString());
            if (idx < 0) {
                bool ok = false;
                const int n = v.toString().toInt(&ok);
                if (ok) {
                    idx = n;
                }
            }
        } else if (v.isDouble()) {
            // a position in the list of labels
            idx = int(std::lround(v.toDouble()));
        }
        if (idx < 0 || idx >= p->valueLabels().size()) {
            return false;
        }
        *out = int(p->lower()) + idx;
        return true;
    }
    case Parameter::Float:
    case Parameter::Int: {
        double d = 0;
        bool ok = true;
        if (v.isDouble()) {
            d = v.toDouble();
        } else if (v.isBool()) {
            d = v.toBool() ? 1 : 0;
        } else if (v.isString()) {
            d = v.toString().trimmed().toDouble(&ok);
        } else {
            ok = false;
        }
        if (!ok || !std::isfinite(d)) {
            return false;
        }
        *out = d;
        return true;
    }
    default:
        return false;
    }
}

} // anonymous namespace

PresetGenerator::PresetGenerator(Machine& machine, QObject *parent) : QObject(parent), m_machine(machine) {
    QSettings s;
    m_endpoint = s.value(QLatin1String(kEndpointKey), QString::fromLatin1(kDefaultEndpoint)).toString();
    m_model = s.value(QLatin1String(kModelKey), QString::fromLatin1(kDefaultModel)).toString();
    m_apiKey = s.value(QLatin1String(kApiKeyKey)).toString();
}

QString PresetGenerator::defaultEndpoint() {
    return QString::fromLatin1(kDefaultEndpoint);
}

// The engine stops on presets whose name is not ASCII (see the known
// problems in docs/features.md): accents are dropped, dashes and quotes
// replaced, other characters removed
QString PresetGenerator::asciiName(const QString& name) {
    QString out;
    for (const QChar c : name.normalized(QString::NormalizationForm_KD)) {
        const char16_t u = c.unicode();
        if (u >= 0x20 && u < 0x7f) {
            out += c;
        } else if ((u >= 0x2010 && u <= 0x2015) || u == 0x2212) {
            out += QLatin1Char('-');
        } else if (u == 0x2018 || u == 0x2019) {
            out += QLatin1Char('\'');
        } else if (u == 0x201c || u == 0x201d) {
            out += QLatin1Char('"');
        } else if (c.isSpace()) {
            out += QLatin1Char(' ');
        }
    }
    return out.simplified().left(60).trimmed();
}

bool PresetGenerator::usingDefault() const {
    return m_endpoint == defaultEndpoint();
}

void PresetGenerator::setEndpoint(const QString& url) {
    const QString u = url.trimmed();
    if (u == m_endpoint) {
        return;
    }
    m_endpoint = u;
    save();
}

void PresetGenerator::setModel(const QString& model) {
    const QString m = model.trimmed();
    if (m == m_model) {
        return;
    }
    m_model = m;
    save();
}

void PresetGenerator::setApiKey(const QString& key) {
    const QString k = key.trimmed();
    if (k == m_apiKey) {
        return;
    }
    m_apiKey = k;
    save();
}

void PresetGenerator::resetSettings() {
    m_endpoint = defaultEndpoint();
    m_model = QString::fromLatin1(kDefaultModel);
    m_apiKey.clear();
    save();
}

void PresetGenerator::save() {
    QSettings s;
    s.setValue(QLatin1String(kEndpointKey), m_endpoint);
    s.setValue(QLatin1String(kModelKey), m_model);
    if (m_apiKey.isEmpty()) {
        s.remove(QLatin1String(kApiKeyKey));
    } else {
        s.setValue(QLatin1String(kApiKeyKey), m_apiKey);
    }
    emit settingsChanged();
}

// the preset parameters of a unit, without file names (String) and the
// "show details" switch (s_h), and without those of units whose id starts
// with this one (amp.tonestack in amp)
QList<Parameter*> PresetGenerator::unitParameters(const QString& unit, const QSet<QString>& unitIds) const {
    QList<Parameter*> l;
    const QString prefix = unit + QLatin1Char('.');
    for (Parameter *p : m_machine.unitPresetParameters(unit)) {
        const QString key = p->id().mid(prefix.size());
        if (p->kind() == Parameter::String || key == QLatin1String("s_h")) {
            continue;
        }
        bool nested = false;
        const QStringList parts = key.split(QLatin1Char('.'));
        for (int i = 1; i < parts.size() && !nested; ++i) {
            nested = unitIds.contains(prefix + parts.mid(0, i).join(QLatin1Char('.')));
        }
        if (!nested) {
            l.append(p);
        }
    }
    return l;
}

QList<PresetGenerator::Unit> PresetGenerator::catalog() const {
    QList<Unit> l;
    QSet<QString> ids;
    const QVariantList plugins = m_machine.plugins();
    for (const QVariant& v : plugins) {
        ids.insert(v.toMap().value(QStringLiteral("id")).toString());
    }
    for (const QVariant& v : plugins) {
        const QVariantMap info = v.toMap();
        Unit u;
        u.id = info.value(QStringLiteral("id")).toString();
        if (excludedUnits().contains(u.id) || info.value(QStringLiteral("category")).toString() == QLatin1String("Neural")) {
            continue;
        }
        u.stereo = info.value(QStringLiteral("stereo")).toBool();
        for (Parameter *p : unitParameters(u.id, ids)) {
            u.keys.append(p->id().mid(u.id.size() + 1));
        }
        l.append(u);
    }
    return l;
}

QStringList PresetGenerator::ampParameters() const {
    QStringList l;
    for (const char *id : kAmpParameters) {
        const QString s = QString::fromLatin1(id);
        Parameter *p = m_machine.param(s);
        if (p && p->kind() != Parameter::String && p->kind() != Parameter::Other) {
            l.append(s);
        }
    }
    return l;
}

QString PresetGenerator::systemPrompt() const {
    QString s = QString::fromUtf8(kInstructions);
    QStringList amp;
    for (const QString& id : ampParameters()) {
        amp.append(describe(m_machine.param(id), id));
    }
    s += QStringLiteral("\nAmplifier parameters: ") + amp.join(QLatin1Char(' ')) + QLatin1Char('\n');
    s += QStringLiteral("\nUnits (id | name | category | rack | parameters):\n");
    for (const Unit& u : catalog()) {
        const QVariantMap info = m_machine.pluginInfo(u.id);
        QString name = info.value(QStringLiteral("name")).toString();
        const QString desc = info.value(QStringLiteral("description")).toString().simplified();
        if (!desc.isEmpty() && desc != name) {
            name += QStringLiteral(" (%1)").arg(desc.left(80));
        }
        QStringList params;
        for (const QString& key : u.keys) {
            params.append(describe(m_machine.param(u.id + QLatin1Char('.') + key), key));
        }
        s += QStringLiteral("%1 | %2 | %3 | %4 | %5\n")
                 .arg(u.id, name, info.value(QStringLiteral("category")).toString(),
                      u.stereo ? QStringLiteral("stereo") : QStringLiteral("mono"), params.join(QLatin1Char(' ')));
    }
    return s;
}

QJsonObject PresetGenerator::extractJson(const QString& text) {
    QString t = text;
    // reasoning of some models (DeepSeek, Qwen) comes before the answer
    static const QRegularExpression think(QStringLiteral("<think>.*</think>"),
                                          QRegularExpression::DotMatchesEverythingOption);
    t.remove(think);
    const int start = t.indexOf(QLatin1Char('{'));
    const int end = t.lastIndexOf(QLatin1Char('}'));
    if (start < 0 || end <= start) {
        return QJsonObject();
    }
    return QJsonDocument::fromJson(t.mid(start, end - start + 1).toUtf8()).object();
}

QString PresetGenerator::generate(const QString& description, const QString& bank) {
    const QString d = description.simplified();
    if (d.isEmpty()) {
        return tr("Describe the sound first");
    }
    if (busy()) {
        return tr("A preset is already being generated");
    }
    if (!m_machine.ready()) {
        return tr("Not connected to the engine");
    }
    if (!m_machine.bankMutable(bank)) {
        return m_machine.bank(bank).isEmpty() ? tr("Choose the bank of the new preset")
                                              : tr("Bank \"%1\" is read only").arg(bank);
    }
    const QString error = send(Request::Preset, systemPrompt(), d);
    if (error.isEmpty()) {
        m_bank = bank;
    }
    return error;
}

QString PresetGenerator::send(Request kind, const QString& system, const QString& user) {
    const QUrl url(m_endpoint);
    if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))) {
        return tr("The address of the model is not a http or https URL");
    }
    if (m_model.isEmpty()) {
        return tr("No model is set");
    }
#if QT_CONFIG(ssl)
    const bool ssl = QSslSocket::supportsSsl();
#else
    const bool ssl = false;
#endif
    if (url.scheme() == QLatin1String("https") && !ssl) {
        return tr("HTTPS is not available: install OpenSSL (libssl3)");
    }

    QJsonObject body{
        {QStringLiteral("model"), m_model},
        {QStringLiteral("messages"),
         QJsonArray{
             QJsonObject{{QStringLiteral("role"), QStringLiteral("system")}, {QStringLiteral("content"), system}},
             QJsonObject{{QStringLiteral("role"), QStringLiteral("user")}, {QStringLiteral("content"), user}},
         }},
    };
    // the default model reasons and runs out of its output limit otherwise;
    // other APIs may refuse this parameter (OpenAI for models that do not
    // reason), so it is only sent there
    if (usingDefault()) {
        body.insert(QStringLiteral("reasoning_effort"), QStringLiteral("low"));
        // identical requests are answered from a cache: a new seed gives a
        // new answer when the same sound is asked again
        body.insert(QStringLiteral("seed"), int(QRandomGenerator::global()->bounded(1000000)));
    }

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    const QString version = QCoreApplication::applicationVersion();
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  version.isEmpty() ? QStringLiteral("guitariqx") : QStringLiteral("guitariqx/") + version);
    if (!m_apiKey.isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
    }
    req.setTransferTimeout(kTimeout);
    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_reply = reply;
    m_request = kind;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { onReply(reply); });
    emit busyChanged();

    // Add trace output for debugging
    qDebug() << "Preset Generator: Sending prompt to LLM:";
    qDebug().noquote() << system;
    qDebug() << "User prompt:";
    qDebug().noquote() << user;
    return QString();
}

void PresetGenerator::fail(const QString& error, const QString& notes, const QStringList& warnings) {
    if (m_request == Request::Drums) {
        emit drumsFinished(false, QString(), notes, warnings, error);
    } else {
        emit finished(false, QString(), notes, warnings, error);
    }
}

void PresetGenerator::cancel() {
    if (QNetworkReply *r = m_reply) {
        m_reply = nullptr;
        r->abort();
        r->deleteLater();
        emit busyChanged();
    }
}

void PresetGenerator::onReply(QNetworkReply *reply) {
    reply->deleteLater();
    if (reply != m_reply) {
        // cancelled
        return;
    }
    m_reply = nullptr;
    emit busyChanged();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray data = reply->readAll();
    const QJsonObject answer = QJsonDocument::fromJson(data).object();
    if (status == 0 || (status >= 300 && reply->error() != QNetworkReply::NoError) || status >= 400) {
        QString msg;
        const QJsonValue err = answer.value(QStringLiteral("error"));
        if (err.isObject()) {
            msg = err.toObject().value(QStringLiteral("message")).toString();
        } else if (err.isString()) {
            msg = err.toString();
        }
        if (msg.isEmpty() && status >= 400) {
            msg = QString::fromUtf8(data).simplified().left(200);
        }
        if (msg.isEmpty()) {
            msg = reply->errorString();
        }
        QString text = status ? tr("The model answered with error %1: %2").arg(status).arg(msg)
                               : tr("Cannot reach the model at %1: %2").arg(QUrl(m_endpoint).host(), msg);
        if (status == 401 || status == 403) {
            text += QLatin1Char(' ') + tr("Check the API key in the settings.");
        }
        fail(text);
        return;
    }

    // Add trace output for debugging
    qDebug() << "Preset Generator: Received response from LLM:";
    qDebug() << "Full response data:";
    qDebug().noquote() << QString::fromUtf8(QJsonDocument::fromJson(data).toJson(QJsonDocument::Indented));

    const QJsonValue content = answer.value(QStringLiteral("choices")).toArray().at(0).toObject()
                                   .value(QStringLiteral("message")).toObject().value(QStringLiteral("content"));
    QString text;
    if (content.isArray()) {
        for (const QJsonValue& part : content.toArray()) {
            text += part.toObject().value(QStringLiteral("text")).toString();
        }
    } else {
        text = content.toString();
    }
    const QJsonObject result = extractJson(text);
    if (result.isEmpty()) {
        fail(text.trimmed().isEmpty() ? tr("The model gave an empty answer")
                                      : (m_request == Request::Drums ? tr("The model's answer holds no drum pattern: %1")
                                                                     : tr("The model's answer holds no preset: %1"))
                                            .arg(text.simplified().left(200)));
        return;
    }
    if (!m_machine.ready()) {
        fail(tr("Not connected to the engine"));
        return;
    }
    if (m_request == Request::Drums) {
        finishDrums(result);
    } else {
        finishPreset(result);
    }
}

void PresetGenerator::finishPreset(const QJsonObject& preset) {
    QString error;
    const QStringList warnings = apply(preset, &error);
    const QString notes = preset.value(QStringLiteral("notes")).toString().trimmed();
    if (!error.isEmpty()) {
        emit finished(false, QString(), notes, warnings, error);
        return;
    }
    m_machine.saveNewPreset(m_bank, asciiName(preset.value(QStringLiteral("name")).toString()),
                            [this, notes, warnings](const QString& name, const QString& error) {
        emit finished(error.isEmpty(), name, notes, warnings,
                      error.isEmpty() ? QString() : tr("The settings are applied but not saved: %1").arg(error));
    });
}

QStringList PresetGenerator::apply(const QJsonObject& preset, QString *error) {
    QStringList warnings;
    const QList<Unit> units = catalog();
    QHash<QString, const Unit*> byId;
    for (const Unit& u : units) {
        byId.insert(u.id, &u);
    }

    // the units of each place, with their parameters
    struct Chosen {
        const Unit *unit;
        QJsonObject params;
    };
    QList<Chosen> chosen[3];
    QSet<QString> seen;
    const char *places[] = {"pre", "post", "stereo"};
    for (int place = 0; place < 3; ++place) {
        for (const QJsonValue& v : preset.value(QLatin1String(places[place])).toArray()) {
            QString id;
            QJsonObject params;
            if (v.isString()) {
                id = v.toString();
            } else {
                const QJsonObject o = v.toObject();
                id = o.value(QStringLiteral("unit")).toString();
                if (id.isEmpty()) {
                    id = o.value(QStringLiteral("id")).toString();
                }
                params = o.value(QStringLiteral("params")).toObject();
            }
            const Unit *u = byId.value(id);
            if (!u) {
                warnings.append(tr("Unknown unit \"%1\" skipped").arg(id));
                continue;
            }
            if (u->stereo != (place == 2)) {
                warnings.append(u->stereo ? tr("Stereo unit \"%1\" skipped in the mono rack").arg(id)
                                          : tr("Mono unit \"%1\" skipped in the stereo rack").arg(id));
                continue;
            }
            if (seen.contains(id)) {
                warnings.append(tr("Unit \"%1\" is listed twice").arg(id));
                continue;
            }
            seen.insert(id);
            chosen[place].append({u, params});
        }
    }
    const QStringList ampIds = ampParameters();
    const QJsonObject amp = preset.value(QStringLiteral("amp")).toObject();
    if (seen.isEmpty() && amp.isEmpty()) {
        if (error) {
            *error = tr("The model's answer holds no unit and no amplifier setting");
        }
        return warnings;
    }

    // Add trace output for debugging
    qDebug() << "Preset Generator: Applying preset from LLM:";
    qDebug() << "Received preset JSON:";
    qDebug().noquote() << QJsonDocument(preset).toJson(QJsonDocument::Indented);

    auto setParam = [&warnings](Parameter *p, const QString& name, const QJsonValue& v) {
        QVariant value;
        if (!convert(p, v, &value)) {
            // the value as JSON text: the array brackets are cut off
            const QByteArray json = QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact);
            warnings.append(tr("Value %1 of \"%2\" skipped").arg(QString::fromUtf8(json.mid(1, json.size() - 2)), name));
            return;
        }
        if ((p->kind() == Parameter::Float || p->kind() == Parameter::Int) && p->upper() > p->lower()
            && (value.toDouble() < p->lower() || value.toDouble() > p->upper())) {
            warnings.append(tr("Value %1 of \"%2\" is out of range, set to the closest").arg(number(value.toDouble()), name));
        }
        p->setValue(value);
    };

    // the rack: the units chosen in order; those the user cannot add or
    // remove stay (the amplifier, ampstack, is not flagged PGN_FIXED_GUI);
    // insert_rack_unit moves the units already there
    QSet<QString> removable;
    for (const QVariant& v : m_machine.plugins()) {
        removable.insert(v.toMap().value(QStringLiteral("id")).toString());
    }
    const QStringList racks[2] = {m_machine.monoRack(), m_machine.stereoRack()};
    for (int stereo = 0; stereo < 2; ++stereo) {
        for (const QString& u : racks[stereo]) {
            if (!seen.contains(u) && removable.contains(u)) {
                // the engine switches it off too
                m_machine.removeUnit(u, stereo);
            }
        }
    }
    const bool hasAmp = racks[0].contains(QStringLiteral("ampstack"));
    for (const Chosen& c : chosen[0]) {
        m_machine.insertUnit(c.unit->id, hasAmp ? QStringLiteral("ampstack") : QString(), false);
    }
    for (const Chosen& c : chosen[1]) {
        m_machine.insertUnit(c.unit->id, QString(), false);
    }
    for (const Chosen& c : chosen[2]) {
        m_machine.insertUnit(c.unit->id, QString(), true);
    }

    for (int place = 0; place < 3; ++place) {
        for (const Chosen& c : chosen[place]) {
            const QString& id = c.unit->id;
            m_machine.resetUnit(id);
            if (Parameter *on = m_machine.param(id + QStringLiteral(".on_off"))) {
                on->setValue(true);
            }
            for (auto it = c.params.begin(); it != c.params.end(); ++it) {
                QString key = it.key();
                // the full id instead of the name
                if (key.startsWith(id + QLatin1Char('.')) && !c.unit->keys.contains(key)) {
                    key = key.mid(id.size() + 1);
                }
                if (!c.unit->keys.contains(key)) {
                    warnings.append(tr("Unknown parameter \"%1\" of \"%2\" skipped").arg(it.key(), id));
                    continue;
                }
                setParam(m_machine.param(id + QLatin1Char('.') + key), id + QLatin1Char('.') + key, it.value());
            }
        }
    }

    for (const QString& id : ampIds) {
        m_machine.param(id)->reset();
    }
    for (auto it = amp.begin(); it != amp.end(); ++it) {
        if (!ampIds.contains(it.key())) {
            warnings.append(tr("Unknown amplifier parameter \"%1\" skipped").arg(it.key()));
            continue;
        }
        setParam(m_machine.param(it.key()), it.key(), it.value());
    }
    return warnings;
}
