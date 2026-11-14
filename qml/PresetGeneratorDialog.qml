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

// Describe a sound, a language model chooses the units and settings, which
// are applied to the rack and saved as a new preset of the chosen bank
// (Gx.presetGenerator): the preset loaded before is not changed
Dialog {
    id: root
    readonly property var gen: Gx.presetGenerator
    property string error: ""
    property string presetName: ""
    property string notes: ""
    property var warnings: []
    property bool generated: false
    // the bank the preset is saved in
    property string targetBank: ""
    // bank to select once the engine created it (see onBankCreated)
    property string pendingBank: ""
    readonly property string bankName: bankBox.currentIndex >= 0 && bankBox.currentIndex < bankBox.count
                                       ? bankBox.model[bankBox.currentIndex] : ""

    function generate() {
        error = ""
        generated = false
        warnings = []
        notes = ""
        targetBank = bankName
        const e = gen.generate(description.text, bankName)
        if (e)
            error = e
    }
    // the writable banks, keeping the selection (by default the current
    // bank, if it is writable)
    function fillBanks(select) {
        const banks = Gx.writableBanks()
        const keep = select !== undefined ? select : (bankName !== "" ? bankName : Gx.currentBank)
        bankBox.model = banks
        bankBox.currentIndex = Math.max(0, banks.indexOf(keep))
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
            settings.expanded = true
            open()
        }
    }
    onAboutToShow: fillBanks()
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
    Connections {
        target: Gx
        function onBankCreated(name) {
            if (root.visible)
                root.pendingBank = name
        }
        function onBanksChanged() {
            if (!root.visible)
                return
            root.fillBanks(root.pendingBank !== "" ? root.pendingBank : undefined)
            root.pendingBank = ""
        }
    }

    TextInputDialog { id: newBankDialog }

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
                text: qsTr("Describe the sound: a song, an artist, a style. The rack is replaced by the units the model chooses, saved as a new preset.")
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
                Label { text: qsTr("Bank") }
                ComboBox {
                    id: bankBox
                    Layout.fillWidth: true
                    enabled: !root.gen.busy
                }
                Button {
                    text: qsTr("New ...")
                    enabled: !root.gen.busy
                    onClicked: newBankDialog.ask(qsTr("New bank"), qsTr("Bank name"), "", qsTr("Create"),
                                                 (name) => Gx.newBank(name))
                }
            }
            Label {
                Layout.fillWidth: true
                visible: bankBox.count === 0
                text: qsTr("No writable bank: create one with \"New ...\"")
                color: Theme.textDim
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: root.gen.busy ? qsTr("Cancel") : qsTr("Generate")
                    highlighted: !root.gen.busy
                    enabled: root.gen.busy || (Gx.ready && root.bankName !== "" && description.text.trim() !== "")
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
                text: qsTr("Saved as \"%1\" in \"%2\" and loaded.").arg(root.presetName).arg(root.targetBank)
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

            GeneratorSettings {
                id: settings
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
