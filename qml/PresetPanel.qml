import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtHead

// Preset banks with their presets; tap a preset to load it. The toolbar
// saves the current settings; the ⋮ buttons of banks and presets edit them.
Rectangle {
    id: root
    signal presetSelected()
    color: Theme.panel

    // banks the user expanded, by name; the current bank is always open
    property var openBanks: ({})
    readonly property bool canSave: (Gx.banks, Gx.ready && Gx.currentPreset !== "" && Gx.bankMutable(Gx.currentBank))

    function isOpen(name) {
        return name === Gx.currentBank || !!openBanks[name]
    }
    function toggle(name) {
        const o = Object.assign({}, openBanks)
        o[name] = !isOpen(name)
        openBanks = o
    }
    function report(error) {
        if (error)
            errorText.show(error, true)
    }

    TextInputDialog { id: textDialog }
    ConfirmDialog { id: confirmDialog }
    SavePresetDialog { id: saveDialog }

    // bank files are read and written on this computer: the contents are
    // sent to / received from the engine
    FileChooserDialog {
        id: loadBankDialog
        local: true
        kind: "bank"
        suffixes: ["gx"]
        filterName: qsTr("Bank files (*.gx)")
        onChosen: (path) => root.report(Gx.importBank(path))
    }
    FileChooserDialog {
        id: saveBankDialog
        property string bank: ""
        local: true
        kind: "bank"
        suffixes: ["gx"]
        filterName: qsTr("Bank files (*.gx)")
        saveMode: true
        defaultSuffix: "gx"
        onChosen: (path) => root.report(Gx.exportBank(bank, path))
    }
    function saveBank(name) {
        saveBankDialog.bank = name
        saveBankDialog.choose(Gx.engineLocal ? qsTr("Save bank \"%1\"").arg(name)
                                             : qsTr("Save bank \"%1\" on this computer").arg(name),
                              "", Gx.bankFileName(name))
    }
    Connections {
        target: Gx
        function onBankImported(file, name, error) {
            if (error) {
                errorText.show(error, true)
            } else {
                errorText.show(qsTr("Bank \"%1\" loaded").arg(name), false)
                const o = Object.assign({}, root.openBanks)
                o[name] = true
                root.openBanks = o
            }
        }
        function onBankExported(name, file, error) {
            errorText.show(error ? error : qsTr("Bank \"%1\" saved to %2").arg(name).arg(file), !!error)
        }
    }

    // --debug-state bankload, banksave:BANK
    readonly property string debugArgs: Qt.application.arguments.join(" ")
    readonly property bool debugBankFiles: Gx.ready && Gx.banks.length > 0 && visible
                                           && debugArgs.indexOf("bank") >= 0
    onDebugBankFilesChanged: {
        if (!debugBankFiles)
            return
        const m = debugArgs.match(/banksave:([^, ]+)/)
        if (m)
            saveBank(m[1])
        else if (debugArgs.indexOf("bankload") >= 0)
            loadBankDialog.choose(qsTr("Load a bank file"), "/usr/share/gx_head/factorysettings")
    }

    // --debug-state saveas
    readonly property bool debugSaveAs: Gx.ready && Gx.banks.length > 0 && visible
                                        && Qt.application.arguments.join(" ").indexOf("saveas") >= 0
    onDebugSaveAsChanged: if (debugSaveAs) saveDialog.saveAs(Gx.currentBank, Gx.currentPreset)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            Button {
                Layout.fillWidth: true
                text: qsTr("Save")
                enabled: root.canSave
                onClicked: root.report(Gx.saveCurrent())
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Save the current settings to \"%1\"").arg(Gx.currentPreset)
            }
            Button {
                Layout.fillWidth: true
                text: qsTr("Save as ...")
                enabled: Gx.ready
                onClicked: saveDialog.saveAs(Gx.bankMutable(Gx.currentBank) ? Gx.currentBank : "",
                                             Gx.currentPreset)
            }
            ToolButton {
                text: "⋮"
                font.pixelSize: Theme.fontLarge
                enabled: Gx.ready
                onClicked: banksMenu.open()
                Menu {
                    id: banksMenu
                    MenuItem {
                        text: qsTr("New bank ...")
                        onTriggered: textDialog.ask(qsTr("New bank"), qsTr("Bank name"), "", qsTr("Create"),
                                                    (name) => Gx.newBank(name))
                    }
                    MenuItem {
                        text: qsTr("Load bank file ...")
                        onTriggered: loadBankDialog.choose(
                            Gx.engineLocal ? qsTr("Load a bank file") : qsTr("Load a bank file from this computer"), "")
                    }
                }
            }
        }

        Label {
            id: errorText
            Layout.fillWidth: true
            visible: false
            color: Theme.meterHigh
            wrapMode: Text.WordWrap
            font.pixelSize: Theme.fontSmall
            function show(t, isError) {
                text = t
                color = isError ? Theme.meterHigh : Theme.text
                visible = true
                hideTimer.restart()
            }
            Timer {
                id: hideTimer
                interval: 5000
                onTriggered: errorText.visible = false
            }
        }

        ListView {
            id: banks
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Gx.banks
            spacing: 2
            ScrollBar.vertical: ScrollBar {}

            delegate: Column {
                id: bank
                required property var modelData
                required property int index
                readonly property string name: modelData.name
                readonly property bool open: root.isOpen(name)
                readonly property bool writable: !!modelData.mutable
                readonly property bool userBank: modelData.type === "file"
                width: ListView.view.width

                ItemDelegate {
                    width: parent.width
                    height: Theme.touch + 4
                    onClicked: root.toggle(bank.name)
                    contentItem: RowLayout {
                        spacing: Theme.spacing
                        Text {
                            text: bank.open ? "▾" : "▸"
                            color: Theme.textDim
                            font.pixelSize: Theme.fontNormal
                        }
                        Text {
                            Layout.fillWidth: true
                            text: bank.name
                            color: bank.name === Gx.currentBank ? Theme.accent : Theme.text
                            font.pixelSize: Theme.fontNormal
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: bank.modelData.type === "factory" ? qsTr("factory")
                                : !bank.writable ? qsTr("read only") : ""
                            color: Theme.textDim
                            font.pixelSize: Theme.fontSmall
                        }
                        ToolButton {
                            text: "⋮"
                            onClicked: bankMenu.open()
                            Menu {
                                id: bankMenu
                                MenuItem {
                                    text: qsTr("Save to file ...")
                                    onTriggered: root.saveBank(bank.name)
                                }
                                MenuItem {
                                    text: qsTr("Rename ...")
                                    enabled: bank.writable
                                    onTriggered: textDialog.ask(qsTr("Rename bank"), qsTr("New name"), bank.name,
                                                                qsTr("Rename"),
                                                                (n) => Gx.renameBank(bank.name, n))
                                }
                                MenuItem {
                                    text: qsTr("Move up")
                                    enabled: bank.index > 0
                                    onTriggered: root.report(Gx.moveBank(bank.name, -1))
                                }
                                MenuItem {
                                    text: qsTr("Move down")
                                    enabled: bank.index < Gx.banks.length - 1
                                    onTriggered: root.report(Gx.moveBank(bank.name, 1))
                                }
                                MenuItem {
                                    text: qsTr("Delete ...")
                                    enabled: bank.userBank
                                    onTriggered: confirmDialog.ask(
                                        qsTr("Delete bank"),
                                        qsTr("Delete the bank \"%1\" and its %2 presets?")
                                            .arg(bank.name).arg(bank.modelData.presets.length),
                                        qsTr("Delete"), () => root.report(Gx.removeBank(bank.name)))
                                }
                            }
                        }
                    }
                }

                Repeater {
                    model: bank.open ? bank.modelData.presets : []
                    ItemDelegate {
                        id: preset
                        required property string modelData
                        required property int index
                        readonly property bool current: bank.name === Gx.currentBank
                                                        && modelData === Gx.currentPreset
                        width: bank.width
                        height: Theme.touch + 4
                        leftPadding: 32
                        highlighted: current
                        onClicked: {
                            Gx.setPreset(bank.name, modelData)
                            root.presetSelected()
                        }
                        contentItem: RowLayout {
                            Text {
                                Layout.fillWidth: true
                                text: preset.modelData
                                color: Theme.text
                                font.pixelSize: Theme.fontNormal
                                elide: Text.ElideRight
                            }
                            ToolButton {
                                text: "⋮"
                                onClicked: presetMenu.open()
                                Menu {
                                    id: presetMenu
                                    MenuItem {
                                        text: qsTr("Save current settings here")
                                        enabled: bank.writable
                                        onTriggered: confirmDialog.ask(
                                            qsTr("Replace preset"),
                                            qsTr("Replace \"%1\" with the current settings?").arg(preset.modelData),
                                            qsTr("Replace"),
                                            () => root.report(Gx.savePreset(bank.name, preset.modelData)))
                                    }
                                    MenuItem {
                                        text: qsTr("Rename ...")
                                        enabled: bank.writable
                                        onTriggered: textDialog.ask(qsTr("Rename preset"), qsTr("New name"),
                                                                    preset.modelData, qsTr("Rename"),
                                                                    (n) => Gx.renamePreset(bank.name, preset.modelData, n))
                                    }
                                    MenuItem {
                                        text: qsTr("Copy to bank ...")
                                        onTriggered: saveDialog.copy(bank.name, preset.modelData)
                                    }
                                    MenuItem {
                                        text: qsTr("Move up")
                                        enabled: bank.writable && preset.index > 0
                                        onTriggered: root.report(Gx.movePreset(bank.name, preset.modelData, -1))
                                    }
                                    MenuItem {
                                        text: qsTr("Move down")
                                        enabled: bank.writable && preset.index < bank.modelData.presets.length - 1
                                        onTriggered: root.report(Gx.movePreset(bank.name, preset.modelData, 1))
                                    }
                                    MenuItem {
                                        text: qsTr("Delete ...")
                                        enabled: bank.writable
                                        onTriggered: confirmDialog.ask(
                                            qsTr("Delete preset"),
                                            qsTr("Delete the preset \"%1\" from \"%2\"?").arg(preset.modelData).arg(bank.name),
                                            qsTr("Delete"),
                                            () => root.report(Gx.erasePreset(bank.name, preset.modelData)))
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
