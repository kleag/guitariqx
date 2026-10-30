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
import Guitariqx

// On/off switch for a bool parameter, drawn as a pedal style LED button,
// or with the switch icons of a gx_head skin (switchType: their base name)
Item {
    id: root
    property Parameter param: null
    property string label: param && param.name !== "on/off" ? param.name : ""
    property string switchType: ""
    // named after what it does (the transport switches) or the parameter
    // instead of label
    property bool nameLabel: false
    readonly property bool on: param ? !!param.value : false
    readonly property url icon: Theme.skinImage((switchType || "switch") + (on ? "_on" : "_off"))
    readonly property bool skinned: icon.toString() !== ""
    // transport switches of the looper and the recorder: a symbol in the
    // flat look (gx_head draws their icon sets)
    readonly property var transport: ({
        "rbutton": { glyph: "●", name: qsTr("Record"), color: Theme.ledOn },
        "pbutton": { glyph: "▶", name: qsTr("Play"), color: Theme.meterLow },
        "prbutton": { glyph: "◀", name: qsTr("Reverse"), color: Theme.meterLow },
        "button": { glyph: "✕", name: qsTr("Erase"), color: Theme.meterHigh },
        "overdub": { glyph: "+", name: qsTr("Overdub"), color: Theme.accent },
    })
    readonly property var transportInfo: transport[switchType]
    readonly property string shownLabel: !nameLabel ? label
                                       : transportInfo ? transportInfo.name : param ? param.name : ""

    implicitWidth: Math.max(Theme.touch, labelText.implicitWidth)
    implicitHeight: (shownLabel ? labelText.height + 2 : 0) + Theme.touch
    opacity: param ? 1 : 0.4

    Text {
        id: labelText
        visible: root.shownLabel !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.shownLabel
        color: Theme.rackLabel
        font.pixelSize: Theme.fontSmall
    }

    Rectangle {
        id: button
        width: Theme.touch
        height: Theme.touch
        radius: Theme.radius
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        color: root.skinned ? (tap.pressed ? "#60000000" : "#30000000")
                            : tap.pressed ? Theme.panelBorder : Theme.panelRaised
        border.color: root.skinned ? "transparent"
                    : root.on && root.transportInfo ? root.transportInfo.color : Theme.panelBorder

        // the icons are 7 to 37 pixels: scaled to about 24
        Image {
            visible: root.skinned
            anchors.centerIn: parent
            source: root.icon
            readonly property real scale_: Math.min(28 / Math.max(implicitWidth, 1), 24 / Math.max(implicitHeight, 1))
            width: implicitWidth * scale_
            height: implicitHeight * scale_
            smooth: true
            mipmap: true
        }

        Text {
            visible: !root.skinned && root.transportInfo !== undefined
            anchors.centerIn: parent
            text: root.transportInfo ? root.transportInfo.glyph : ""
            color: root.on && root.transportInfo ? root.transportInfo.color : Theme.textDim
            font.pixelSize: Theme.fontLarge
            font.bold: true
        }

        Rectangle {
            visible: !root.skinned && root.transportInfo === undefined
            anchors.centerIn: parent
            width: 14
            height: 14
            radius: 7
            color: root.on ? Theme.ledOn : Theme.ledOff
            border.color: "#00000080"
        }

        MouseArea {
            id: tap
            anchors.fill: parent
            onClicked: if (root.param) root.param.value = !root.on
        }
    }

    MidiLearnArea { paramId: root.param ? root.param.id : "" }
}
