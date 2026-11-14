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
    signal generatorRequested()
    property bool compact: false
    property bool fullScreen: false
    property bool canToggleFullScreen: true
    // set by the window when it is taller than wide
    property bool portrait: false

    // portrait or narrow screen: the preset name gets its own, larger row
    // above the buttons (presetRow), so that it can be read standing up
    readonly property bool stacked: portrait || width < 640
    // stacked preset name size: grows with the width (36 px at 720)
    readonly property int nameSize: Math.max(Theme.fontLive + 4, Math.round(width / 20))
    // height of the row of buttons
    readonly property int barHeight: Theme.touchLive + 8

    // the menu, for screenshots and tests
    readonly property bool debugMenu: {
        const args = Qt.application.arguments
        const i = args.indexOf("--debug-state")
        return i >= 0 && i + 1 < args.length && args[i + 1].split(",").indexOf("menu") >= 0
    }
    // opened once connected, when the window is laid out
    readonly property bool debugMenuOpen: debugMenu && Gx.ready
    onDebugMenuOpenChanged: if (debugMenuOpen) menu.open()

    implicitHeight: stacked ? presetRow.implicitHeight + 2 * Theme.spacing + barHeight
                            : barHeight
    color: Theme.panel

    // everything here is used while playing: large enough to be hit at once
    // (Material's minimum width and wide padding are dropped so that the
    // row fits 480 px)
    component LiveButton: Button {
        font.pixelSize: Theme.fontLive
        implicitHeight: Theme.touchLive
        leftPadding: 10
        rightPadding: 10
        implicitWidth: Math.max(Theme.touchLive, implicitContentWidth + leftPadding + rightPadding)
    }
    component GlyphButton: ToolButton {
        font.pixelSize: Theme.fontLiveGlyph
        implicitWidth: Theme.touchLive - 8
        implicitHeight: Theme.touchLive
    }
    component LiveMenuItem: MenuItem {
        font.pixelSize: Theme.fontLive
        implicitHeight: Theme.touchLive
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.barHeight
        anchors.leftMargin: Theme.spacing
        anchors.rightMargin: Theme.spacing
        spacing: root.stacked ? 0 : Theme.spacing / 2

        GlyphButton {
            text: "☰"
            visible: !root.stacked
            onClicked: root.presetsRequested()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Presets")
        }

        GlyphButton {
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

        GlyphButton {
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
                LiveButton {
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
        LiveButton {
            readonly property Parameter show: Gx.ready ? Gx.param("system.show_tuner") : null
            text: qsTr("Tuner")
            enabled: show !== null
            onClicked: show.value = true
            MidiLearnArea { paramId: "system.show_tuner" }
        }

        LiveButton {
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

        GlyphButton {
            text: "\u22EE"
            onClicked: menu.open()
            Menu {
                id: menu
                y: parent.height
                // wide enough for the larger font, within the window
                width: Math.min(340, root.width - 2 * Theme.spacing)
                font.pixelSize: Theme.fontLive
                LiveMenuItem {
                    text: qsTr("Preset by note ...")
                    enabled: Gx.ready
                    onTriggered: Gx.tuner.toggleSwitcher()
                    MidiLearnArea { paramId: "ui.live_play_switcher" }
                }
                LiveMenuItem {
                    text: qsTr("Generate a preset ...")
                    enabled: Gx.ready
                    onTriggered: root.generatorRequested()
                }
                LiveMenuItem {
                    text: qsTr("MIDI controllers ...")
                    enabled: Gx.ready
                    onTriggered: root.midiMapRequested()
                }
                LiveMenuItem {
                    text: qsTr("LADSPA/LV2 plugins ...")
                    enabled: Gx.ready
                    onTriggered: root.pluginsRequested()
                }
                LiveMenuItem {
                    // for a pedalboard that can be unplugged at any time
                    text: qsTr("Restore last preset")
                    checkable: true
                    checked: Gx.restorePreset
                    onToggled: Gx.restorePreset = checked
                }
                LiveMenuItem {
                    text: qsTr("Skin ...")
                    onTriggered: root.skinsRequested()
                }
                LiveMenuItem {
                    text: qsTr("Connect to engine ...")
                    onTriggered: root.connectRequested()
                }
                LiveMenuItem {
                    text: root.fullScreen ? qsTr("Leave full screen") : qsTr("Full screen")
                    visible: root.canToggleFullScreen
                    height: visible ? implicitHeight : 0
                    onTriggered: root.fullScreenRequested()
                }
            }
        }

        // connection indicator, with a touch sized tap area
        Item {
            implicitWidth: 28
            implicitHeight: Theme.touchLive
            Rectangle {
                anchors.centerIn: parent
                width: 16
                height: 16
                radius: 8
                color: Gx.ready ? Theme.meterLow : Gx.connected ? Theme.meterMid : Theme.meterHigh
            }
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

        GlyphButton {
            text: "☰"
            onClicked: root.presetsRequested()
        }

        GlyphButton {
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

        GlyphButton {
            text: "▶"
            enabled: Gx.ready
            onClicked: Gx.stepPreset(1)
            MidiLearnArea { paramId: "engine.next_preset" }
        }
    }
}
