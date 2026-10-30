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
    signal skinsRequested()
    signal pluginsRequested()
    property bool compact: false
    property bool fullScreen: false
    property bool canToggleFullScreen: true
    // set by the window when it is taller than wide
    property bool portrait: false

    // portrait or narrow screen: the preset name gets its own, larger row
    // above the buttons (presetRow), so that it can be read standing up
    readonly property bool stacked: portrait || width < 640
    // stacked preset name size: grows with the width (55 px at 720)
    readonly property int nameSize: Math.max(2 * Theme.fontLarge, Math.round(width / 13))

    implicitHeight: stacked ? presetRow.implicitHeight + 2 * Theme.spacing + Theme.touch + 20
                            : Theme.touch + 20
    color: Theme.panel

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: Theme.touch + 20
        anchors.leftMargin: Theme.spacing
        anchors.rightMargin: Theme.spacing
        spacing: root.stacked ? 0 : Theme.spacing

        ToolButton {
            text: "☰"
            visible: !root.stacked
            font.pixelSize: Theme.fontLarge
            onClicked: root.presetsRequested()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Presets")
        }

        RoundButton {
            text: "◀"
            visible: !root.stacked
            enabled: Gx.ready
            onClicked: Gx.stepPreset(-1)
            MidiLearnArea { paramId: "engine.previus_preset" }
        }

        Column {
            visible: !root.stacked
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
            visible: !root.stacked
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

        Item {
            visible: root.stacked
            Layout.fillWidth: true
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
                    text: qsTr("LADSPA/LV2 plugins ...")
                    enabled: Gx.ready
                    onTriggered: root.pluginsRequested()
                }
                MenuItem {
                    text: qsTr("Skin ...")
                    onTriggered: root.skinsRequested()
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

    RowLayout {
        id: presetRow
        visible: root.stacked
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.spacing
        spacing: Theme.spacing

        ToolButton {
            text: "☰"
            font.pixelSize: Theme.fontLarge
            onClicked: root.presetsRequested()
        }

        RoundButton {
            text: "◀"
            enabled: Gx.ready
            onClicked: Gx.stepPreset(-1)
            MidiLearnArea { paramId: "engine.previus_preset" }
        }

        Column {
            Layout.fillWidth: true
            // two lines are always reserved so that the racks do not move
            // when the preset changes; long names wrap, then shrink, before
            // being elided
            Text {
                width: parent.width
                height: Math.ceil(2 * root.nameSize * 1.25)
                text: Gx.currentPreset || (Gx.ready ? qsTr("(no preset)") : qsTr("Not connected"))
                color: Theme.text
                font.pixelSize: root.nameSize
                fontSizeMode: Text.Fit
                minimumPixelSize: Theme.fontLarge + 3
                font.bold: true
                wrapMode: Text.Wrap
                maximumLineCount: 2
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: Gx.currentBank
                color: Theme.textDim
                font.pixelSize: Theme.fontNormal
                horizontalAlignment: Text.AlignHCenter
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
    }
}
