import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtHead

// Asks for a name. The callback returns an error text (the dialog stays
// open and shows it) or an empty string.
Dialog {
    id: root
    property string label: ""
    property string acceptText: qsTr("OK")
    property var callback: null
    property string error: ""

    function ask(title_, label_, text_, acceptText_, callback_) {
        title = title_
        label = label_
        field.text = text_
        acceptText = acceptText_
        callback = callback_
        error = ""
        open()
    }

    function submit() {
        const e = callback ? callback(field.text) : ""
        if (e) {
            error = e
            return
        }
        close()
    }

    modal: true
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }
    anchors.centerIn: Overlay.overlay
    width: Math.min(420, Overlay.overlay ? Overlay.overlay.width - 24 : 420)
    onOpened: {
        field.forceActiveFocus()
        field.selectAll()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing
        Label {
            visible: text !== ""
            text: root.label
            color: Theme.textDim
        }
        TextField {
            id: field
            Layout.fillWidth: true
            onAccepted: root.submit()
            onTextEdited: root.error = ""
        }
        Label {
            Layout.fillWidth: true
            visible: root.error !== ""
            text: root.error
            color: Theme.meterHigh
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
            text: root.acceptText
            highlighted: true
            enabled: field.text.trim() !== ""
            // not AcceptRole: the dialog must stay open on errors
            onClicked: root.submit()
        }
    }
}
