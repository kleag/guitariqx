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

// Choose the look of the rack units: guitariqx's flat look or a gx_head
// skin. A tap applies the skin at once.
Dialog {
    id: root
    title: qsTr("Skin")
    modal: true
    // plain background: Material's elevation layer is costly on small GPUs
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.Close

    // --debug-state skins
    readonly property bool debugOpen: {
        const args = Qt.application.arguments
        const i = args.indexOf("--debug-state")
        return i >= 0 && i + 1 < args.length && args[i + 1].split(",").indexOf("skins") >= 0
    }
    Component.onCompleted: if (debugOpen) open()
    onOpened: list.positionViewAtIndex(Math.max(0, list.model.indexOf(Gx.skin.name)), ListView.Center)

    ListView {
        id: list
        implicitWidth: 280
        implicitHeight: Math.min(contentHeight, (root.parent ? root.parent.height : 480) * 0.6)
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        // "" is the flat look
        model: [""].concat(Gx.skin.names)

        delegate: ItemDelegate {
            required property string modelData
            width: ListView.view.width
            height: Math.max(Theme.touch + 8, implicitHeight)
            text: modelData === "" ? qsTr("guitariqx (flat)") : modelData.replace(/_/g, " ")
            highlighted: Gx.skin.name === modelData
            onClicked: Gx.skin.name = modelData
        }
    }
}
