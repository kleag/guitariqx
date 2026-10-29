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

#include <QColor>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <functional>

/*
 * gx_head skins: a GTK style sheet gx_head_NAME.css and a directory NAME
 * of images overriding the icons of libgxw, in gx_head/skins of the XDG
 * data directories. The "Guitarix" skin and the libgxw icons are built in
 * (skins/ in the sources).
 *
 * guitariqx does not apply the style sheets, it reads a few values from
 * them (colors, knob rings, see Skin::style()).
 */
namespace skincss {

// selector -> property -> value. Comma separated selectors are split, later
// declarations win. @import url("FILE") is replaced by load(FILE).
using Rules = QHash<QString, QHash<QString, QString>>;
Rules parse(const QString& css, const std::function<QString(const QString&)>& load = {});

// #rgb, #rrggbb, rgb(), rgba() and color names; invalid otherwise
QColor color(const QString& value);

} // namespace skincss

class Skin : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use Gx.skin")
    // installed skins and the built-in one, sorted
    Q_PROPERTY(QStringList names READ names CONSTANT)
    // "" for guitariqx's own flat look
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    // colors and knob rings of the current skin, see style()
    Q_PROPERTY(QVariantMap style READ style NOTIFY changed)

public:
    // searchPaths: directories holding gx_head_*.css; empty for the XDG
    // data directories. builtin: the directory of the built-in skin.
    explicit Skin(const QStringList& searchPaths = {},
                  const QString& builtin = QStringLiteral(":/Guitariqx/skins"),
                  QObject* parent = nullptr);

    QStringList names() const { return m_names; }
    QString name() const { return m_name; }
    // unknown names select the flat look; the choice is saved in the settings
    void setName(const QString& name);
    // select without saving (--skin on the command line)
    void select(const QString& name);
    bool active() const { return !m_name.isEmpty(); }
    QVariantMap style() const { return m_style; }

    // URL of an icon of libgxw (e.g. "smallknobr", "rbutton_on") for the
    // current skin, empty when there is none
    Q_INVOKABLE QUrl image(const QString& icon) const;

signals:
    void changed();

private:
    QString cssFile(const QString& name) const;
    QString loadCss(const QString& name) const;
    void load();

    QStringList m_dirs;
    QString m_builtin;
    QStringList m_names;
    QString m_name;
    QString m_skinDir;
    QVariantMap m_style;
    mutable QHash<QString, QUrl> m_images;
};
