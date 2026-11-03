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

// Describe a sound, a language model chooses the units and settings and
// they are applied to the rack (Gx.presetGenerator); "Save as" keeps them
Dialog {
    id: root
    readonly property var gen: Gx.presetGenerator
    property string error: ""
    property string presetName: ""
    property string notes: ""
    property var warnings: []
    property bool generated: false

    function generate() {
        error = ""
        generated = false
        warnings = []
        notes = ""
        const e = gen.generate(description.text)
        if (e)
            error = e
    }

    title: qsTr("Generate a preset")
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

    // --debug-state generator: open, with the model settings shown
    readonly property bool debugOpen: {
        const args = Qt.application.arguments
        const i = args.indexOf("--debug-state")
        return i >= 0 && i + 1 < args.length && args[i + 1].split(",").indexOf("generator") >= 0
    }
    Component.onCompleted: {
        if (debugOpen) {
            settingsButton.checked = true
            open()
        }
    }
    onOpened: description.forceActiveFocus()
    onClosed: gen.cancel()

    Connections {
        target: root.gen
        function onFinished(ok, name, notes, warnings, error) {
            root.generated = ok
            root.presetName = name
            root.notes = notes
            root.warnings = warnings
            root.error = error
        }
    }

    SavePresetDialog { id: saveDialog }

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
                text: qsTr("Describe the sound: a song, an artist, a style. The rack is replaced by the units the model chooses.")
                color: Theme.textDim
            }
            TextArea {
                id: description
                Layout.fillWidth: true
                Layout.minimumHeight: Theme.touch * 2
                wrapMode: TextEdit.Wrap
                placeholderText: qsTr("e.g. the second solo of Comfortably Numb by Pink Floyd")
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
                Button {
                    text: qsTr("Save as ...")
                    visible: root.generated
                    onClicked: saveDialog.saveAs(Gx.currentBank, root.presetName)
                }
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
                text: root.presetName !== "" ? qsTr("\"%1\" is applied: try it, then save it.").arg(root.presetName)
                                             : qsTr("The preset is applied: try it, then save it.")
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
                text: qsTr("Skipped: %1").arg(root.warnings.join("; "))
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }

            Button {
                id: settingsButton
                checkable: true
                flat: true
                text: (checked ? "▾ " : "▸ ") + qsTr("Model settings")
            }
            GridLayout {
                Layout.fillWidth: true
                visible: settingsButton.checked
                columns: 2
                columnSpacing: Theme.spacing
                rowSpacing: Theme.spacing

                Label { text: qsTr("Address") }
                TextField {
                    Layout.fillWidth: true
                    text: root.gen.endpoint
                    inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                    onEditingFinished: root.gen.endpoint = text
                }
                Label { text: qsTr("Model") }
                TextField {
                    Layout.fillWidth: true
                    text: root.gen.model
                    inputMethodHints: Qt.ImhNoAutoUppercase
                    onEditingFinished: root.gen.model = text
                }
                Label { text: qsTr("API key") }
                TextField {
                    Layout.fillWidth: true
                    text: root.gen.apiKey
                    echoMode: TextInput.PasswordEchoOnEdit
                    placeholderText: qsTr("none")
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                    onEditingFinished: root.gen.apiKey = text
                }
                Label {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSmall
                    text: (root.gen.usingDefault
                           ? qsTr("The default is a free model of Pollinations (one answer every 45 s). Any OpenAI compatible chat completions address works: OpenAI, OpenRouter, Mistral, Groq, a local Ollama (http://localhost:11434/v1/chat/completions)...")
                           : "")
                          + " " + qsTr("The description is sent to %1. The API key is stored unencrypted in the settings.")
                                  .arg(root.gen.endpoint.replace(/^https?:\/\//, "").split("/")[0])
                }
                Button {
                    Layout.columnSpan: 2
                    text: qsTr("Use the free model")
                    onClicked: root.gen.resetSettings()
                }
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
