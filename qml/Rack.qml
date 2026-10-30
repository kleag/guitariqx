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

    // --debug-state rackdrag:UNIT drags UNIT to the top of its rack
    readonly property string debugDragUnit: {
        const args = Qt.application.arguments
        const i = args.indexOf("--debug-state")
        if (i < 0 || i + 1 >= args.length)
            return ""
        const state = args[i + 1].split(",").find(s => s.startsWith("rackdrag:"))
        return state ? state.slice(9) : ""
    }
    readonly property bool debugDrag: Gx.ready && debugDragUnit !== "" && units.indexOf(debugDragUnit) >= 0
    onDebugDragChanged: if (debugDrag) Qt.callLater(() => {
        dragId = debugDragUnit
        dragY = 0
        updateSlot()
    })

    Item {
        id: title
        width: parent.width
        height: Theme.touch + 8

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            text: root.stereo ? qsTr("Stereo rack") : qsTr("Mono rack")
            color: Theme.textDim
            font.pixelSize: Theme.fontNormal
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.5
        }

        Button {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("+ Add effect")
            enabled: Gx.ready
            onClicked: addDialog.open()
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
            // scrolled out of the cache buffer: the drag is lost
            Component.onDestruction: if (dragging) root.endDrag(false)
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
        running: root.dragId !== "" && list.contentHeight > list.height
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

    // the name of the dragged unit follows the pointer
    Rectangle {
        visible: root.dragId !== ""
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
    }
}
