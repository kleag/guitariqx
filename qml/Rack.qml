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

// Mono or stereo effect rack
Item {
    id: root
    property bool stereo: false
    readonly property var units: stereo ? Gx.stereoRack : Gx.monoRack

    // drag and drop of a unit: the slot (0 to units.length) it goes to and
    // the pointer y in the list
    property string dragId: ""
    property int dropSlot: -1
    property real dragY: 0
    readonly property int dragIndex: units.indexOf(dragId)
    // the slots next to the dragged unit leave it in place
    readonly property bool dropMoves: dragIndex >= 0 && dropSlot >= 0
                                      && dropSlot !== dragIndex && dropSlot !== dragIndex + 1
    // placing an added unit by hand (Gx.placeAddedUnits): it is dragged
    // without a pointer down, a press on the rack moves it and the release
    // drops it. pendingPlace waits for the unit to be in the rack.
    property string placeId: ""
    property string pendingPlace: ""
    readonly property bool dragPressed: dragId !== "" && (placeId === "" || placeArea.pressed)

    function startDrag(id, sceneY) {
        dragId = id
        moveDrag(sceneY)
    }
    function moveDrag(sceneY) {
        dragY = Math.max(0, Math.min(list.height, list.mapFromItem(null, 0, sceneY).y))
        updateSlot()
    }
    function endDrag(drop) {
        if (drop && dropMoves)
            Gx.moveUnitTo(dragId, dropSlot, stereo)
        dragId = ""
        dropSlot = -1
        placeId = ""
    }
    function startPlacing(id) {
        dragId = id
        dropSlot = -1
        placeId = id
        Qt.callLater(() => list.positionViewAtIndex(units.indexOf(id), ListView.Contain))
    }
    onUnitsChanged: {
        if (pendingPlace !== "" && units.indexOf(pendingPlace) >= 0) {
            startPlacing(pendingPlace)
            pendingPlace = ""
        } else if (placeId !== "" && units.indexOf(placeId) < 0) {
            // removed meanwhile (preset change)
            endDrag(false)
        }
    }
    function updateSlot() {
        const y = list.mapToItem(list.contentItem, 0, dragY).y
        // units out of the cache buffer have no item but are far away
        for (let i = 0; i < list.count; ++i) {
            const item = list.itemAtIndex(i)
            if (item && y < item.y + item.height / 2) {
                dropSlot = i
                return
            }
        }
        dropSlot = list.count
    }

    // --debug-state rackdrag:UNIT drags UNIT to the top of its rack,
    // rackplace:UNIT starts placing UNIT as if just added
    function debugStateUnit(name) {
        const args = Qt.application.arguments
        const i = args.indexOf("--debug-state")
        if (i < 0 || i + 1 >= args.length)
            return ""
        const state = args[i + 1].split(",").find(s => s.startsWith(name + ":"))
        return state ? state.slice(name.length + 1) : ""
    }
    readonly property string debugDragUnit: debugStateUnit("rackdrag")
    readonly property bool debugDrag: Gx.ready && debugDragUnit !== "" && units.indexOf(debugDragUnit) >= 0
    onDebugDragChanged: if (debugDrag) Qt.callLater(() => {
        dragId = debugDragUnit
        dragY = 0
        updateSlot()
    })
    readonly property string debugPlaceUnit: debugStateUnit("rackplace")
    readonly property bool debugPlace: Gx.ready && debugPlaceUnit !== "" && units.indexOf(debugPlaceUnit) >= 0
    onDebugPlaceChanged: if (debugPlace) Qt.callLater(() => startPlacing(debugPlaceUnit))

    Item {
        id: title
        width: parent.width
        height: Theme.touch + 8

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.right: titleButton.left
            anchors.rightMargin: Theme.spacing
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            text: root.placeId !== ""
                  ? qsTr("Tap where %1 goes").arg(Gx.pluginInfo(root.placeId).name || root.placeId)
                  : root.stereo ? qsTr("Stereo rack") : qsTr("Mono rack")
            color: root.placeId !== "" ? Theme.accent : Theme.textDim
            font.pixelSize: Theme.fontNormal
            font.capitalization: root.placeId !== "" ? Font.MixedCase : Font.AllUppercase
            font.letterSpacing: root.placeId !== "" ? 0 : 1.5
        }

        Button {
            id: titleButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: root.placeId !== "" ? qsTr("Keep here") : qsTr("+ Add effect")
            enabled: Gx.ready
            onClicked: root.placeId !== "" ? root.endDrag(false) : addDialog.open()
        }
    }

    ListView {
        id: list
        anchors.top: title.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        spacing: 6
        model: root.units
        boundsBehavior: Flickable.StopAtBounds
        // no flick while a unit is dragged
        interactive: root.dragId === ""
        // keep units alive while scrolling, they are cheap but layouts
        // are fetched from the engine
        cacheBuffer: 2000
        ScrollBar.vertical: ScrollBar {}

        delegate: RackUnit {
            required property string modelData
            required property int index
            width: ListView.view.width - 10
            unitId: modelData
            stereo: root.stereo
            first: index === 0
            last: index === root.units.length - 1
            dragging: root.dragId === modelData
            onDragStarted: (sceneY) => root.startDrag(modelData, sceneY)
            onDragMoved: (sceneY) => root.moveDrag(sceneY)
            onDragEnded: (drop) => root.endDrag(drop)
            // scrolled out of the cache buffer: the drag is lost (placing
            // does not depend on the unit's item)
            Component.onDestruction: if (dragging && root.placeId === "") root.endDrag(false)
        }

        // where the dragged unit goes
        Rectangle {
            visible: root.dropMoves
            z: 10
            width: list.width - 10
            height: 4
            radius: 2
            color: Theme.accent
            y: {
                if (!root.dropMoves)
                    return 0
                const after = root.dropSlot >= list.count
                const item = list.itemAtIndex(after ? list.count - 1 : root.dropSlot)
                if (!item)
                    return 0
                // on the edge of the first and last units, the space around
                // them is clipped
                if (after)
                    return item.y + item.height - height
                return root.dropSlot === 0 ? item.y : item.y - (list.spacing + height) / 2
            }
        }

        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: Gx.ready ? qsTr("No effects in this rack") : ""
            color: Theme.textDim
        }
    }

    // scroll while the pointer is near the top or the bottom
    Timer {
        interval: 16
        repeat: true
        running: root.dragPressed && list.contentHeight > list.height
        onTriggered: {
            const edge = Theme.touch
            const step = 12
            let y = list.contentY
            if (root.dragY < edge)
                y = Math.max(list.originY, y - step)
            else if (root.dragY > list.height - edge)
                y = Math.min(list.originY + list.contentHeight - list.height, y + step)
            if (y !== list.contentY) {
                list.contentY = y
                root.updateSlot()
            }
        }
    }

    // placing an added unit: a press anywhere on the rack drags it
    MouseArea {
        id: placeArea
        anchors.fill: list
        enabled: root.placeId !== ""
        preventStealing: true
        onPressed: (mouse) => root.moveDrag(mapToItem(null, mouse.x, mouse.y).y)
        onPositionChanged: (mouse) => root.moveDrag(mapToItem(null, mouse.x, mouse.y).y)
        onReleased: root.endDrag(true)
        onCanceled: root.dropSlot = -1
    }

    // the name of the dragged unit follows the pointer
    Rectangle {
        visible: root.dragPressed
        z: 10
        x: (parent.width - width) / 2
        y: list.y + root.dragY - height / 2
        width: Math.min(parent.width - 20, dragLabel.implicitWidth + 2 * Theme.spacing)
        height: Theme.touch
        radius: Theme.radius
        color: Theme.panelRaised
        border.color: Theme.accent
        opacity: 0.9
        Text {
            id: dragLabel
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width - 2 * Theme.spacing)
            elide: Text.ElideRight
            text: root.dragId === "" ? "" : Gx.pluginInfo(root.dragId).name || root.dragId
            color: Theme.text
            font.pixelSize: Theme.fontLarge
            font.bold: true
        }
    }

    AddUnitDialog {
        id: addDialog
        stereo: root.stereo
        exclude: root.units
        onPlaceRequested: (id) => root.pendingPlace = id
    }
}
