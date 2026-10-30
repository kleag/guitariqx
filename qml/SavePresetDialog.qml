import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Guitariqx

// "Save as" (current settings) and "Copy to bank" (an existing preset):
// choose a writable bank, or create one, and a name
Dialog {
    id: root
    // empty: save the current settings, else copy this preset
    property string fromBank: ""
    property string fromPreset: ""
    readonly property bool copying: fromPreset !== ""
    property string error: ""
    // bank to select once the engine created it (see onBankCreated)
    property string pendingBank: ""
    readonly property string bankName: bankBox.currentIndex >= 0 ? bankBox.model[bankBox.currentIndex] : ""
    readonly property bool exists: (Gx.banks, bankName !== "" && Gx.presetExists(bankName, nameField.text.trim()))

    function saveAs(defaultBank, defaultName) {
        fromBank = ""
        fromPreset = ""
        show(defaultBank, defaultName)
    }
    function copy(bank, preset) {
        fromBank = bank
        fromPreset = preset
        show("", preset)
    }
    function show(defaultBank, defaultName) {
        const banks = Gx.writableBanks()
        bankBox.model = banks
        bankBox.currentIndex = Math.max(0, banks.indexOf(defaultBank))
        nameField.text = defaultName
        replace.checked = false
        error = ""
        open()
    }
    function submit() {
        const name = nameField.text.trim()
        let e
        if (copying) {
            e = Gx.copyPreset(fromBank, fromPreset, bankName, name)
        } else {
            if (exists && !replace.checked) {
                error = qsTr("A preset with this name exists: check \"Replace\" to overwrite it")
                return
            }
            e = Gx.savePreset(bankName, name)
        }
        if (e) {
            error = e
            return
        }
        close()
    }

    title: copying ? qsTr("Copy \"%1\" to bank").arg(fromPreset) : qsTr("Save preset as")
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
    width: Math.min(460, Overlay.overlay ? Overlay.overlay.width - 24 : 460)
    onOpened: {
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    Connections {
        target: Gx
        function onBankCreated(name) {
            if (!root.visible)
                return
            root.pendingBank = name
        }
        function onBanksChanged() {
            if (!root.visible || root.pendingBank === "")
                return
            const banks = Gx.writableBanks()
            bankBox.model = banks
            bankBox.currentIndex = Math.max(0, banks.indexOf(root.pendingBank))
            root.pendingBank = ""
        }
    }

    TextInputDialog {
        id: newBankDialog
    }

    GridLayout {
        anchors.fill: parent
        columns: 2
        columnSpacing: Theme.spacing
        rowSpacing: Theme.spacing

        Label { text: qsTr("Bank") }
        RowLayout {
            Layout.fillWidth: true
            ComboBox {
                id: bankBox
                Layout.fillWidth: true
                onActivated: root.error = ""
            }
            Button {
                text: qsTr("New ...")
                onClicked: newBankDialog.ask(qsTr("New bank"), qsTr("Bank name"), "", qsTr("Create"),
                                             (name) => Gx.newBank(name))
            }
        }

        Label { text: qsTr("Name") }
        TextField {
            id: nameField
            Layout.fillWidth: true
            onAccepted: root.submit()
            onTextEdited: root.error = ""
        }

        CheckBox {
            id: replace
            Layout.columnSpan: 2
            visible: root.exists && !root.copying
            text: qsTr("Replace the existing preset")
        }

        Label {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            visible: root.error !== ""
            text: root.error
            color: Theme.meterHigh
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.columnSpan: 2
            Layout.fillWidth: true
            visible: bankBox.count === 0
            text: qsTr("No writable bank: create one with \"New ...\"")
            color: Theme.textDim
            wrapMode: Text.WordWrap
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: root.copying ? qsTr("Copy") : qsTr("Save")
            highlighted: true
            enabled: root.bankName !== "" && nameField.text.trim() !== ""
            onClicked: root.submit()
        }
    }
}
