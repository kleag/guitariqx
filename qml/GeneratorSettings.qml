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

// The language model settings of Gx.presetGenerator (shared by the preset
// and drum pattern generators), folded by default
ColumnLayout {
    id: root
    readonly property var gen: Gx.presetGenerator
    property alias expanded: settingsButton.checked
    spacing: Theme.spacing

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
