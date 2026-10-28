import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtHead

// Preset navigation, engine state and output level
Rectangle {
    id: root
    signal presetsRequested()
    signal connectRequested()
    property bool compact: false

    implicitHeight: Theme.touch + 20
    color: Theme.panel

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacing
        anchors.rightMargin: Theme.spacing
        spacing: Theme.spacing

        ToolButton {
            text: "☰"
            font.pixelSize: Theme.fontLarge
            onClicked: root.presetsRequested()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Presets")
        }

        RoundButton {
            text: "◀"
            enabled: Gx.ready
            onClicked: Gx.stepPreset(-1)
        }

        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 80
            Text {
                width: parent.width
                text: Gx.currentPreset || (Gx.ready ? qsTr("(no preset)") : qsTr("Not connected"))
                color: Theme.text
                font.pixelSize: Theme.fontLarge + 3
                font.bold: true
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: Gx.currentBank
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall
                elide: Text.ElideRight
            }
            TapHandler { onTapped: root.presetsRequested() }
        }

        RoundButton {
            text: "▶"
            enabled: Gx.ready
            onClicked: Gx.stepPreset(1)
        }

        // engine state: stopped is shown as mute
        Row {
            spacing: 2
            Repeater {
                model: [
                    { state: "running", label: qsTr("On") },
                    { state: "bypassed", label: qsTr("Bypass") },
                    { state: "stopped", label: qsTr("Mute") },
                ]
                Button {
                    required property var modelData
                    text: modelData.label
                    checkable: true
                    checked: Gx.engineState === modelData.state
                    enabled: Gx.ready
                    Material.background: checked ? (modelData.state === "running" ? Theme.accentDim : Theme.ledOff)
                                                 : Theme.panelRaised
                    onClicked: Gx.setEngineState(modelData.state)
                }
            }
        }

        Column {
            visible: !root.compact
            spacing: 3
            Layout.preferredWidth: 140
            LevelMeter { width: parent.width; paramId: "maxlevel.left" }
            LevelMeter { width: parent.width; paramId: "maxlevel.right" }
            Text {
                text: qsTr("DSP %1 %").arg(Gx.cpuLoad.toFixed(1))
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall
            }
        }

        // connection indicator
        Rectangle {
            width: 12
            height: 12
            radius: 6
            color: Gx.ready ? Theme.meterLow : Gx.connected ? Theme.meterMid : Theme.meterHigh
            TapHandler { onTapped: root.connectRequested() }
        }
    }
}
