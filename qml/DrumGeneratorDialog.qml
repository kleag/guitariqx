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

// Name a song, a language model writes a drum pattern to play along
// (Gx.presetGenerator.generateDrums): it goes to the drum sequencer of the
// current preset, added to the rack if needed, and is saved as a preset of
// the sequencer. The text starts as the current preset's name.
Dialog {
    id: root
    readonly property var gen: Gx.presetGenerator
    property string error: ""
    property string patternName: ""
    property string notes: ""
    property var warnings: []
    property bool generated: false
    // the preset the pattern went to
    property string targetPreset: ""
    // the text put in when opening, replaced at the next opening if unchanged
    property string prefill: ""

    function generate() {
        error = ""
        generated = false
        warnings = []
        notes = ""
        targetPreset = Gx.currentPreset
        const e = gen.generateDrums(description.text)
        if (e)
            error = e
    }

    title: qsTr("Generate drums")
    modal: true
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }
    // near the top of short screens, above the on-screen keyboard
    parent: Overlay.overlay
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height < 600 ? 8 : (parent.height - height) / 2) : 0
    width: Math.min(560, parent ? parent.width - 24 : 560)
    height: Math.min(implicitHeight, parent ? parent.height - 16 : implicitHeight)

    // --debug-state drums: open, prefilled (only the instance of the main
    // window)
    property bool debugOpen: {
        const args = Qt.application.arguments
        const i = args.indexOf("--debug-state")
        return i >= 0 && i + 1 < args.length && args[i + 1].split(",").indexOf("drums") >= 0
    }
    readonly property bool debugReady: debugOpen && Gx.ready
    onDebugReadyChanged: if (debugReady) open()
    onAboutToShow: {
        if (description.text.trim() === "" || description.text === prefill) {
            prefill = Gx.currentPreset
            description.text = prefill
        }
    }
    onOpened: description.forceActiveFocus()
    onClosed: if (gen.busy) gen.cancel()

    Connections {
        target: root.gen
        function onDrumsFinished(ok, name, notes, warnings, error) {
            root.generated = ok
            root.patternName = name
            root.notes = notes
            root.warnings = warnings
            root.error = error
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        implicitHeight: column.implicitHeight
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: column
            width: flick.width
            spacing: Theme.spacing

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Name the song to play along with (or a style). The model writes its drum part for the drum sequencer of the current preset.")
                color: Theme.textDim
            }
            TextArea {
                id: description
                Layout.fillWidth: true
                Layout.minimumHeight: Theme.touch * 2
                wrapMode: TextEdit.Wrap
                placeholderText: qsTr("e.g. Comfortably Numb by Pink Floyd")
                enabled: !root.gen.busy
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: root.gen.busy ? qsTr("Cancel") : qsTr("Generate")
                    highlighted: !root.gen.busy
                    enabled: root.gen.busy || (Gx.ready && description.text.trim() !== "")
                    onClicked: root.gen.busy ? root.gen.cancel() : root.generate()
                }
                BusyIndicator {
                    running: root.gen.busy
                    visible: running
                    Layout.preferredHeight: Theme.touch
                    Layout.preferredWidth: Theme.touch
                }
                Label {
                    Layout.fillWidth: true
                    visible: root.gen.busy
                    text: qsTr("Asking the model ...")
                    color: Theme.textDim
                }
                Item { Layout.fillWidth: true; visible: !root.gen.busy }
            }

            Label {
                Layout.fillWidth: true
                visible: root.error !== ""
                text: root.error
                color: Theme.meterHigh
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: root.generated
                // the pattern is a part of the preset: saved with it
                text: qsTr("Playing in the drum sequencer, saved as its preset \"%1\".").arg(root.patternName)
                      + " "
                      + (root.targetPreset !== "" && Gx.autoSavePreset && Gx.bankMutable(Gx.currentBank)
                         ? qsTr("Preset \"%1\" is saved with it.").arg(root.targetPreset)
                         : qsTr("Save the preset to keep the drums with it."))
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: root.generated && root.notes !== ""
                text: root.notes
                color: Theme.textDim
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: root.generated && root.warnings.length > 0
                text: qsTr("Fixed: %1").arg(root.warnings.join("; "))
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }

            GeneratorSettings {
                Layout.fillWidth: true
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Close")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
