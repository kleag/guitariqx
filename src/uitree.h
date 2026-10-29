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
 *             "meter"|"display"|"wheel"|"file"|"ir"|"iredit"|"sequencer"|"oscilloscope",
 *             size: "small"|"mid"|"big", id, label, ...}
 *            knob: "smallknob" for the small knobs drawn with that skin image
 *            (GxSmallKnob), switchType: the icon set of a switch (base_name)
 *            display: id2 and id3 are the cut and clip parameters of a
 *            looper tape position (create_p_display)
 *            iredit: button opening the IR editor of a convolver (id
 *            <unit>.convolver), sequencer: the drum sequencer (id: one of
 *            its seq.sequencer.* lines), oscilloscope: id oscilloscope.info
 *   spacer:  {type: "spacer"}
 *   label:   {type: "label", text}
 *
 * The children of a box with orientation "grid" have row, column, rowSpan
 * and columnSpan, and halign or valign ("start", "end") when they are not
 * centered in their cell.
 *
 * A unit layout is {mini: <box>, main: <box>}: the mini box is the
 * compact view of a collapsed rack unit.
 */
namespace uitree {

// Build from the result of plugin_load_ui(id, UI_FORM_STACK), which is a
// flat list of UiBuilder calls (see UiBuilderVirt in jsonrpc.cpp).
QVariantMap fromStackOps(const QJsonArray& ops);

// Build from a Glade (GtkBuilder) description, the result of
// plugin_load_ui(id, UI_FORM_GLADE): the objects "rackbox" and "minibox"
// with GTK containers and gxw widgets (see glade.cpp). Returns an empty
// map when there is no rackbox.
QVariantMap fromGlade(const QString& xml);

// Fallback when the engine has no usable stack description: one control
// per parameter of the unit.
QVariantMap generic(const QList<Parameter*>& params);

// true when the layout contains at least one control
bool hasControls(const QVariantMap& node);

// replace the parameter id of every control of a unit layout by fix(id)
void fixIds(QVariantMap& ui, const std::function<QString(const QString&)>& fix);

// add a control at the top of the main view of a unit layout
void prependControl(QVariantMap& ui, const QString& widget, const QString& id);

// true when the layout has a control of this widget type
bool hasWidget(const QVariantMap& node, const QString& widget);

// Add the special widgets of a unit that its layout lacks (the stack
// layouts have none): IR editor of the convolvers, drum sequencer,
// oscilloscope. hasParam tells the parameters the engine has.
void addSpecialWidgets(QVariantMap& ui, const QString& unit, const std::function<bool(const QString&)>& hasParam);

} // namespace uitree
