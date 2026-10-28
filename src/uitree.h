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

#include <QJsonArray>
#include <QList>
#include <QVariantMap>
#include <functional>

class Parameter;

/*
 * Rack unit layouts for QML.
 *
 * A layout is a tree of QVariantMaps:
 *   box:     {type: "box", orientation: "h"|"v"|"flow", label, frame, tabs, children: [...]}
 *   control: {type: "control", widget: "knob"|"slider"|"switch"|"selector"|"spin"|
 *             "meter"|"display"|"wheel", size: "small"|"mid"|"big", id, label, ...}
 *   spacer:  {type: "spacer"}
 *
 * A unit layout is {mini: <box>, main: <box>}: the mini box is the
 * compact view of a collapsed rack unit.
 */
namespace uitree {

// Build from the result of plugin_load_ui(id, UI_FORM_STACK), which is a
// flat list of UiBuilder calls (see UiBuilderVirt in jsonrpc.cpp).
QVariantMap fromStackOps(const QJsonArray& ops);

// Fallback when the engine has no usable stack description: one control
// per parameter of the unit.
QVariantMap generic(const QList<Parameter*>& params);

// true when the layout contains at least one control
bool hasControls(const QVariantMap& node);

// replace the parameter id of every control of a unit layout by fix(id)
void fixIds(QVariantMap& ui, const std::function<QString(const QString&)>& fix);

// add a control at the top of the main view of a unit layout
void prependControl(QVariantMap& ui, const QString& widget, const QString& id);

} // namespace uitree
