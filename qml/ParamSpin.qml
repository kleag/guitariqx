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

import QtQuick
import QtQuick.Controls
import Guitariqx

// Numeric entry for a float or int parameter
Item {
    id: root
    property Parameter param: null
    property string label: ""
    property color labelColor: Theme.textDim
    // SpinBox works on integers: scale floats by the step precision
    readonly property real scale: param && param.kind === Parameter.Float
                                  ? (param.step > 0 && param.step < 1 ? Math.round(1 / param.step) : 100) : 1

    implicitWidth: Math.max(spin.implicitWidth, labelText.implicitWidth)
    implicitHeight: (label ? labelText.height : 0) + spin.implicitHeight

    Text {
        id: labelText
        visible: root.label !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        color: root.labelColor
        font.pixelSize: Theme.fontSmall
    }

    SpinBox {
        id: spin
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        editable: true
        font.pixelSize: Theme.fontNormal
        from: root.param ? Math.round(root.param.lower * root.scale) : 0
        to: root.param ? Math.round(root.param.upper * root.scale) : 0
        stepSize: root.param && root.param.step > 0 ? Math.max(1, Math.round(root.param.step * root.scale)) : 1
        value: root.param ? Math.round(root.param.value * root.scale) : 0
        textFromValue: (v, locale) => root.param ? root.param.displayValue : ""
        valueFromText: (text, locale) => Math.round(Number.fromLocaleString(locale, text) * root.scale)
        onValueModified: if (root.param) root.param.value = value / root.scale
    }

    MidiLearnArea { paramId: root.param ? root.param.id : "" }
}
