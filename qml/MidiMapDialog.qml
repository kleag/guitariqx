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
import QtQuick.Layouts
import Guitariqx

// All MIDI controller assignments (gx_head: MidiControllerTable)
Dialog {
    id: root
    title: qsTr("MIDI controllers")
    modal: true
    // plain background: Material's elevation layer is costly on small GPUs
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, Overlay.overlay ? Overlay.overlay.width - 24 : 560)
    height: Math.min(560, Overlay.overlay ? Overlay.overlay.height - 24 : 560)
    standardButtons: Dialog.Close

    // the option is not broadcast by the engine
    onOpened: Gx.midi.refreshFeedback()

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        // wraps on small screens
        Flow {
            Layout.fillWidth: true
            spacing: Theme.spacing
            Row {
                spacing: Theme.spacing
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("MIDI channel")
                }
                ParamSelector { param: Gx.ready ? Gx.param("system.midi_channel") : null }
            }
            Switch {
                text: qsTr("Learn mode")
                checked: Gx.midi.learnMode
                onToggled: Gx.midi.learnMode = checked
            }
            Switch {
                text: qsTr("MIDI feedback")
                checked: Gx.midi.feedback
                onToggled: Gx.midi.feedback = checked
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Send the values of the assigned parameters to the MIDI output when a preset loads or a controller changes them")
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: Theme.textDim
            font.pixelSize: Theme.fontSmall
            text: qsTr("In learn mode, tap any control (or right click it at any time) to assign a MIDI controller.")
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Gx.midi.assignments
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property var modelData
                width: ListView.view.width
                height: Theme.touch + 12
                onClicked: {
                    root.close()
                    Gx.midi.startLearn(modelData.id)
                }
                contentItem: RowLayout {
                    spacing: Theme.spacing
                    ColumnLayout {
                        Layout.preferredWidth: 150
                        Layout.fillWidth: false
                        spacing: 4
                        Label {
                            Layout.fillWidth: true
                            text: row.modelData.ctlName
                            color: Theme.accent
                            elide: Text.ElideRight
                        }
                        MidiValueBar {
                            Layout.fillWidth: true
                            ctl: row.modelData.ctl
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: row.modelData.group + ": " + row.modelData.name
                        elide: Text.ElideRight
                    }
                    Label {
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                        text: row.modelData.lower !== undefined
                              ? row.modelData.lower + " .. " + row.modelData.upper
                              : (row.modelData.toggle ? qsTr("toggle") : "")
                    }
                    ToolButton {
                        text: "✕"
                        onClicked: Gx.midi.remove(row.modelData.id)
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Remove assignment")
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("No MIDI controllers assigned")
                color: Theme.textDim
            }
        }
    }
}
