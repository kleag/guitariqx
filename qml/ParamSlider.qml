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

// Horizontal slider for a float parameter; vertical for eq bands
Item {
    id: root
    property Parameter param: null
    property string label: param ? param.name : ""
    property bool vertical: false
    property bool showValue: true

    implicitWidth: vertical ? Math.max(Theme.touch, labelText.implicitWidth) : 180
    implicitHeight: vertical ? 150 : Math.max(Theme.touch, slider.implicitHeight) + (label ? labelText.height : 0)
    opacity: param ? 1 : 0.4

    Text {
        id: labelText
        visible: root.label !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.showValue && root.param ? root.label + "  " + root.param.displayValue : root.label
        color: Theme.rackLabel
        font.pixelSize: Theme.fontSmall
    }

    Slider {
        id: slider
        anchors.top: labelText.visible ? labelText.bottom : parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        orientation: root.vertical ? Qt.Vertical : Qt.Horizontal
        from: 0
        to: 1
        value: root.param ? root.param.normalized : 0
        onMoved: if (root.param) root.param.normalized = value
    }

    MidiLearnArea { paramId: root.param ? root.param.id : "" }
}
