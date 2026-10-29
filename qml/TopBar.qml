import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Guitariqx

// Preset navigation, engine state and output level
Rectangle {
    id: root
    signal presetsRequested()
    signal connectRequested()
    signal midiMapRequested()
    signal fullScreenRequested()
    property bool compact: false
    property bool fullScreen: false
    property bool canToggleFullScreen: true

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
            MidiLearnArea { paramId: "engine.previus_preset" }
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
            MidiLearnArea { paramId: "engine.next_preset" }
        }

        // engine state: stopped is shown as mute
        Row {
            spacing: 2
            Repeater {
                model: [
                    { state: "running", label: qsTr("On"), param: "" },
                    { state: "bypassed", label: qsTr("Bypass"), param: "engine.bypass" },
                    { state: "stopped", label: qsTr("Mute"), param: "engine.mute" },
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
                    MidiLearnArea { paramId: parent.modelData.param }
                }
            }
        }

        // the tuner view follows system.show_tuner (see TunerView.qml)
        Button {
            readonly property Parameter show: Gx.ready ? Gx.param("system.show_tuner") : null
            text: qsTr("Tuner")
            enabled: show !== null
            onClicked: show.value = true
            MidiLearnArea { paramId: "system.show_tuner" }
        }

        Button {
            text: qsTr("MIDI")
            checkable: true
            checked: Gx.midi.learnMode
            enabled: Gx.ready
            Material.background: checked ? Theme.accentDim : Theme.panelRaised
            onToggled: Gx.midi.learnMode = checked
            ToolTip.visible: hovered
            ToolTip.text: checked ? qsTr("MIDI learn: tap a control to assign a controller")
                                  : qsTr("MIDI learn mode")
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

        ToolButton {
            text: "\u22EE"
            font.pixelSize: Theme.fontLarge
            onClicked: menu.open()
            Menu {
                id: menu
                y: parent.height
                MenuItem {
                    text: qsTr("Preset by note ...")
                    enabled: Gx.ready
                    onTriggered: Gx.tuner.toggleSwitcher()
                    MidiLearnArea { paramId: "ui.live_play_switcher" }
                }
                MenuItem {
                    text: qsTr("MIDI controllers ...")
                    enabled: Gx.ready
                    onTriggered: root.midiMapRequested()
                }
                MenuItem {
                    text: qsTr("Connect to engine ...")
                    onTriggered: root.connectRequested()
                }
                MenuItem {
                    text: root.fullScreen ? qsTr("Leave full screen") : qsTr("Full screen")
                    visible: root.canToggleFullScreen
                    height: visible ? implicitHeight : 0
                    onTriggered: root.fullScreenRequested()
                }
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
