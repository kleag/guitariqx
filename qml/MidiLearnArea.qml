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

// Overlay that makes a control MIDI learnable: in MIDI learn mode a tap
// opens the learn dialog for the parameter, otherwise a right click does.
// The assigned controller is shown while learn mode is on.
Item {
    id: root
    property string paramId: ""
    readonly property bool learnable: Gx.ready && paramId !== "" && Gx.midi.isLearnable(paramId)
    readonly property int controller: (Gx.midi.revision, learnable ? Gx.midi.controllerFor(paramId) : -1)
    readonly property bool showing: Gx.midi.learnMode && learnable

    anchors.fill: parent
    z: 100

    Rectangle {
        visible: root.showing
        anchors.fill: parent
        anchors.margins: -3
        radius: Theme.radius
        color: root.controller >= 0 ? "#30e8a33d" : "#18e8a33d"
        border.color: Theme.accent
        border.width: Gx.midi.learnParam === root.paramId && Gx.midi.learning ? 3 : 1
    }

    Rectangle {
        visible: root.showing && root.controller >= 0
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: -6
        height: 16
        width: Math.max(16, badge.implicitWidth + 8)
        radius: 8
        color: Theme.accent
        Text {
            id: badge
            anchors.centerIn: parent
            text: root.controller
            color: Theme.background
            font.pixelSize: 10
            font.bold: true
        }
    }

    MouseArea {
        anchors.fill: parent
        anchors.margins: -3
        enabled: root.learnable
        // right clicks only outside learn mode: left clicks reach the control
        acceptedButtons: Gx.midi.learnMode ? (Qt.LeftButton | Qt.RightButton) : Qt.RightButton
        cursorShape: Gx.midi.learnMode ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: Gx.midi.startLearn(root.paramId)
        // MouseArea takes wheel events whatever acceptedButtons is
        onWheel: (wheel) => { wheel.accepted = false }
    }
}
