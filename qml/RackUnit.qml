import QtQuick
import QtQuick.Controls
import QtHead

// One effect unit in the rack: header with power switch, name and unit
// actions, body with the controls of the unit layout
Rectangle {
    id: root
    property string unitId: ""
    property bool stereo: false
    property bool first: false
    property bool last: false
    property bool expanded: true
    readonly property var info: Gx.pluginInfo(unitId)
    readonly property bool isAmp: unitId === "ampstack"
    readonly property Parameter onOff: Gx.ready ? Gx.param(unitId + ".on_off") : null
    readonly property bool on: onOff ? !!onOff.value : true
    property var layout: ({})

    function fetchLayout() {
        if (!isAmp)
            layout = Gx.unitUi(unitId)
    }

    Component.onCompleted: fetchLayout()
    onUnitIdChanged: fetchLayout()
    Connections {
        target: Gx
        function onUnitUiReady(id) { if (id === root.unitId) root.fetchLayout() }
        function onReadyChanged() { if (Gx.ready) root.fetchLayout() }
    }

    implicitHeight: header.height + (body.visible ? body.height + 12 : 0) + 4
    radius: Theme.radius
    color: Theme.panel
    border.color: on ? Theme.accentDim : Theme.panelBorder

    Item {
        id: header
        x: 4
        width: parent.width - 8
        height: Theme.touch + 4

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spacing

            // power: pedal style LED
            Rectangle {
                width: Theme.touch
                height: Theme.touch
                radius: Theme.radius
                color: power.pressed ? Theme.panelBorder : Theme.panelRaised
                visible: root.onOff !== null && !root.info.fixed
                Rectangle {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    radius: 7
                    color: root.on ? Theme.ledOn : Theme.ledOff
                }
                MouseArea {
                    id: power
                    anchors.fill: parent
                    onClicked: root.onOff.value = !root.on
                }
                MidiLearnArea { paramId: root.unitId + ".on_off" }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.info.name || root.unitId
                color: root.on ? Theme.text : Theme.textDim
                font.pixelSize: Theme.fontLarge
                font.bold: true
            }
        }

        // the compact view is shown in the header while collapsed
        UiNode {
            id: mini
            visible: !root.expanded && !root.isAmp
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            node: root.layout.mini || ({})
            enabled: root.on
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            ToolButton {
                text: "▲"
                enabled: !root.first
                onClicked: Gx.moveUnit(root.unitId, -1, root.stereo)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Move up")
            }
            ToolButton {
                text: "▼"
                enabled: !root.last
                onClicked: Gx.moveUnit(root.unitId, 1, root.stereo)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Move down")
            }
            ToolButton {
                text: root.expanded ? "−" : "+"
                onClicked: root.expanded = !root.expanded
                ToolTip.visible: hovered
                ToolTip.text: root.expanded ? qsTr("Collapse") : qsTr("Expand")
            }
            ToolButton {
                text: "✕"
                visible: !root.info.fixed
                onClicked: Gx.removeUnit(root.unitId, root.stereo)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Remove from rack")
            }
        }
    }

    Flickable {
        id: body
        visible: root.expanded
        anchors.top: header.bottom
        anchors.topMargin: 4
        x: 8
        width: parent.width - 16
        height: content.implicitHeight
        contentWidth: content.implicitWidth
        contentHeight: content.implicitHeight
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        interactive: contentWidth > width
        opacity: root.on ? 1 : 0.55

        Loader {
            id: content
            sourceComponent: root.isAmp ? ampPanel : unitPanel
        }

        Component {
            id: unitPanel
            UiNode {
                node: root.layout.main || ({})
                maxWidth: body.width
            }
        }

        Component {
            id: ampPanel
            AmpPanel {}
        }
    }

    BusyIndicator {
        anchors.centerIn: body
        running: root.expanded && !root.isAmp && !root.layout.main
        visible: running
        width: 32
        height: 32
    }
}
