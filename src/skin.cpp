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

#include "skin.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

namespace {

const QString kSettingsKey = QStringLiteral("ui/skin");
const QString kDefaultSkin = QStringLiteral("Guitarix");

QString cssName(const QString& skin) {
    return QStringLiteral("gx_head_%1.css").arg(skin);
}

QString readFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(f.readAll());
}

} // namespace

namespace skincss {

Rules parse(const QString& css, const std::function<QString(const QString&)>& load) {
    Rules rules;
    QString text = css;
    text.remove(QRegularExpression(QStringLiteral("/\\*.*?\\*/"),
                                   QRegularExpression::DotMatchesEverythingOption));
    // imports first: the rules of the importing sheet override them
    static const QRegularExpression importRe(
        QStringLiteral("@import\\s+url\\(\\s*[\"']?([^\"')]+)[\"']?\\s*\\)\\s*;?"));
    auto it = importRe.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        if (load) {
            const Rules imported = parse(load(m.captured(1)), load);
            for (auto r = imported.cbegin(); r != imported.cend(); ++r) {
                QHash<QString, QString>& decls = rules[r.key()];
                for (auto d = r.value().cbegin(); d != r.value().cend(); ++d) {
                    decls.insert(d.key(), d.value());
                }
            }
        }
    }
    text.remove(importRe);

    static const QRegularExpression ruleRe(QStringLiteral("([^{}]+)\\{([^{}]*)\\}"));
    it = ruleRe.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QStringList selectors = m.captured(1).split(QLatin1Char(','));
        const QStringList decls = m.captured(2).split(QLatin1Char(';'));
        for (const QString& sel : selectors) {
            const QString key = sel.simplified();
            if (key.isEmpty() || key.startsWith(QLatin1Char('@'))) {
                continue;
            }
            QHash<QString, QString>& target = rules[key];
            for (const QString& decl : decls) {
                const int colon = decl.indexOf(QLatin1Char(':'));
                if (colon > 0) {
                    target.insert(decl.left(colon).trimmed(), decl.mid(colon + 1).trimmed());
                }
            }
        }
    }
    return rules;
}

QColor color(const QString& value) {
    const QString v = value.trimmed();
    static const QRegularExpression rgbRe(
        QStringLiteral("^rgba?\\(\\s*([\\d.]+)\\s*,\\s*([\\d.]+)\\s*,\\s*([\\d.]+)\\s*(?:,\\s*([\\d.]+)\\s*)?\\)$"));
    const QRegularExpressionMatch m = rgbRe.match(v);
    if (m.hasMatch()) {
        QColor c(qRound(m.captured(1).toDouble()), qRound(m.captured(2).toDouble()),
                 qRound(m.captured(3).toDouble()));
        if (!m.captured(4).isEmpty()) {
            c.setAlphaF(m.captured(4).toDouble());
        }
        return c;
    }
    return QColor(v);
}

} // namespace skincss

Skin::Skin(const QStringList& searchPaths, const QString& builtin, QObject* parent)
    : QObject(parent)
    , m_dirs(searchPaths)
    , m_builtin(builtin) {
    if (m_dirs.isEmpty()) {
        m_dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation,
                                           QStringLiteral("gx_head/skins"),
                                           QStandardPaths::LocateDirectory);
    }
    for (const QString& dir : std::as_const(m_dirs)) {
        const QStringList files = QDir(dir).entryList({QStringLiteral("gx_head_*.css")}, QDir::Files);
        for (const QString& f : files) {
            const QString n = f.mid(8, f.size() - 12);
            if (!m_names.contains(n)) {
                m_names.append(n);
            }
        }
    }
    if (!m_names.contains(kDefaultSkin) && QFile::exists(m_builtin + QLatin1Char('/') + cssName(kDefaultSkin))) {
        m_names.append(kDefaultSkin);
    }
    m_names.sort(Qt::CaseInsensitive);
    select(QSettings().value(kSettingsKey).toString());
}

void Skin::setName(const QString& name) {
    select(name);
    QSettings().setValue(kSettingsKey, m_name);
}

void Skin::select(const QString& name) {
    const QString n = m_names.contains(name) ? name : QString();
    if (n == m_name) {
        return;
    }
    m_name = n;
    load();
    emit changed();
}

QString Skin::cssFile(const QString& file) const {
    for (const QString& dir : m_dirs) {
        const QString path = dir + QLatin1Char('/') + file;
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    const QString path = m_builtin + QLatin1Char('/') + file;
    return QFile::exists(path) ? path : QString();
}

QString Skin::loadCss(const QString& file) const {
    const QString path = cssFile(file);
    return path.isEmpty() ? QString() : readFile(path);
}

void Skin::load() {
    m_style.clear();
    m_images.clear();
    m_skinDir.clear();
    if (m_name.isEmpty()) {
        return;
    }
    for (const QString& dir : std::as_const(m_dirs)) {
        if (QFileInfo(dir + QLatin1Char('/') + m_name).isDir()) {
            m_skinDir = dir + QLatin1Char('/') + m_name;
            break;
        }
    }
    const skincss::Rules rules = skincss::parse(loadCss(cssName(m_name)),
                                                [this](const QString& f) { return loadCss(f); });
    auto value = [&rules](const char* selector, const char* prop) {
        return rules.value(QLatin1String(selector)).value(QLatin1String(prop));
    };
    auto putColor = [this](const char* key, const QString& v) {
        const QColor c = skincss::color(v);
        if (c.isValid()) {
            m_style.insert(QLatin1String(key), c);
        }
    };
    putColor("value", value("gx-regler", "color"));
    putColor("valueBackground", value("gx-regler", "background-color"));
    putColor("ringOff", value("gx-regler:disabled", "color"));
    putColor("label", value("#rack_label", "color"));
    putColor("labelInverse", value("#rack_label_inverse", "color"));
    putColor("title", value("#effect_title", "color"));

    // knob rings (GxKnob style properties, in pixels of the knob image)
    static const struct { const char* key; const char* selector; } knobs[] = {
        {"smallknob", "gx-small-knob"},
        {"smallknobr", "gx-small-knob-r"},
        {"midknob", "gx-mid-knob"},
        {"bigknob", "gx-big-knob"},
    };
    static const struct { const char* key; const char* prop; } props[] = {
        {"xCenter", "-GxKnob-x-center"},
        {"yCenter", "-GxKnob-y-center"},
        {"ringRadius", "-GxKnob-ring-radius"},
        {"ringWidth", "-GxKnob-ring-width"},
        {"ledSize", "-GxKnob-ring-led-size"},
        {"ledDistance", "-GxKnob-ring-led-distance"},
        {"indicatorRadius", "-GxKnob-indicator-radius"},
        {"indicatorWidth", "-GxKnob-indicator-width"},
        {"indicatorLength", "-GxKnob-indicator-length"},
    };
    QVariantMap knobStyles;
    for (const auto& k : knobs) {
        // the shared rule (gx-regler, gx-knob, ...) first, then the knob's own
        QHash<QString, QString> decls = rules.value(QStringLiteral("gx-knob"));
        const QHash<QString, QString> own = rules.value(QLatin1String(k.selector));
        for (auto d = own.cbegin(); d != own.cend(); ++d) {
            decls.insert(d.key(), d.value());
        }
        QVariantMap m;
        for (const auto& p : props) {
            bool ok = false;
            // GTK reads these integer style properties truncated
            const int v = static_cast<int>(decls.value(QLatin1String(p.prop)).toDouble(&ok));
            if (ok) {
                m.insert(QLatin1String(p.key), v);
            }
        }
        if (m.contains(QStringLiteral("ringRadius"))) {
            knobStyles.insert(QLatin1String(k.key), m);
        }
    }
    m_style.insert(QStringLiteral("knobs"), knobStyles);
}

QUrl Skin::image(const QString& icon) const {
    if (m_name.isEmpty() || icon.isEmpty()) {
        return {};
    }
    const auto cached = m_images.constFind(icon);
    if (cached != m_images.constEnd()) {
        return cached.value();
    }
    QUrl url;
    const QString file = icon + QStringLiteral(".png");
    if (!m_skinDir.isEmpty() && QFileInfo::exists(m_skinDir + QLatin1Char('/') + file)) {
        url = QUrl::fromLocalFile(m_skinDir + QLatin1Char('/') + file);
    } else {
        const QString path = m_builtin + QStringLiteral("/gxw/") + file;
        if (QFile::exists(path)) {
            url = path.startsWith(QLatin1Char(':')) ? QUrl(QStringLiteral("qrc") + path)
                                                    : QUrl::fromLocalFile(path);
        }
    }
    m_images.insert(icon, url);
    return url;
}
