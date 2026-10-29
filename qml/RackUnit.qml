import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import Guitariqx

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
    // width of the rack handles of a skin
    readonly property real handle: Theme.skinned ? 22 : 0

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
    radius: Theme.skinned ? 0 : Theme.radius
    color: Theme.skinned ? "black" : Theme.panel
    border.color: Theme.skinned ? "black" : on ? Theme.accentDim : Theme.panelBorder

    // gx_head rack unit (GxPaintBox gx_rack_unit_expose): tiled skin
    // background between two handles, screws in the corners
    Item {
        anchors.fill: parent
        anchors.margins: 1
        visible: Theme.skinned
        clip: true
        Image {
            anchors.fill: parent
            fillMode: Image.Tile
            source: Theme.skinImage("background1")
        }
        Repeater {
            model: ["handle_left", "handle_right"]
            Image {
                required property string modelData
                required property int index
                x: index === 0 ? 0 : parent.width - width
                width: root.handle
                height: parent.height
                fillMode: Image.TileVertically
                source: Theme.skinImage(modelData)
            }
        }
        Repeater {
            model: 4
            Image {
                required property int index
                readonly property bool atRight: index % 2 === 1
                readonly property bool atBottom: index >= 2
                width: 14
                height: 14
                x: atRight ? parent.width - (root.handle + width) / 2 : (root.handle - width) / 2
                y: atBottom ? parent.height - height - 4 : 4
                source: Theme.skinImage("screw")
                smooth: true
                mipmap: true
            }
        }
    }

    Item {
        id: header
        x: 4 + root.handle
        width: parent.width - 8 - 2 * root.handle
        height: Theme.touch + 4

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spacing

            // power: pedal style LED
            Rectangle {
                id: powerButton
                width: Theme.touch
                height: Theme.touch
                radius: Theme.radius
                color: Theme.skinned ? (power.pressed ? "#60000000" : "#30000000")
                                     : power.pressed ? Theme.panelBorder : Theme.panelRaised
                visible: root.onOff !== null && !root.info.fixed
                // the skin's power icon (switch_on/off of libgxw)
                Image {
                    id: powerIcon
                    anchors.centerIn: parent
                    width: 24
                    height: 24
                    source: Theme.skinImage(root.on ? "switch_on" : "switch_off")
                    visible: status === Image.Ready
                    smooth: true
                    mipmap: true
                }
                Rectangle {
                    visible: !powerIcon.visible
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
                // elided in narrow racks rather than hidden under the buttons
                width: Math.min(implicitWidth, header.width - actions.width - Theme.spacing
                                - (powerButton.visible ? powerButton.width + Theme.spacing : 0))
                elide: Text.ElideRight
                text: root.info.name || root.unitId
                color: Theme.skinned ? Theme.rackTitle : root.on ? Theme.text : Theme.textDim
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
            id: actions
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            // five buttons: the touch size rather than Material's 48 px, or
            // the names of narrow units would be elided away
            // Material's dark theme foreground, else the skin's title color
            Material.foreground: Theme.skinned ? Theme.rackTitle : "#ffffff"

            ToolButton {
                implicitWidth: Theme.touch
                id: presetButton
                text: "≡"
                // the amplifier's parameters are not those of its unit id
                visible: !root.isAmp
                property var dialog: null
                function show() {
                    if (!dialog)
                        dialog = presetDialog.createObject(presetButton, { unitId: root.unitId })
                    dialog.show()
                }
                onClicked: show()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Unit presets")
                Component {
                    id: presetDialog
                    UnitPresetDialog {}
                }
                // --debug-state unitpresets:UNIT
                readonly property bool debugOpen: {
                    const args = Qt.application.arguments
                    const i = args.indexOf("--debug-state")
                    return Gx.ready && i >= 0 && i + 1 < args.length
                        && args[i + 1].split(",").indexOf("unitpresets:" + root.unitId) >= 0
                }
                onDebugOpenChanged: if (debugOpen) show()
            }
            ToolButton {
                implicitWidth: Theme.touch
                text: "▲"
                enabled: !root.first
                onClicked: Gx.moveUnit(root.unitId, -1, root.stereo)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Move up")
            }
            ToolButton {
                implicitWidth: Theme.touch
                text: "▼"
                enabled: !root.last
                onClicked: Gx.moveUnit(root.unitId, 1, root.stereo)
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Move down")
            }
            ToolButton {
                implicitWidth: Theme.touch
                text: root.expanded ? "−" : "+"
                onClicked: root.expanded = !root.expanded
                ToolTip.visible: hovered
                ToolTip.text: root.expanded ? qsTr("Collapse") : qsTr("Expand")
            }
            ToolButton {
                implicitWidth: Theme.touch
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
        x: 8 + root.handle
        width: parent.width - 16 - 2 * root.handle
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
            AmpPanel { maxWidth: body.width }
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
