import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Guitariqx

// Choose the engine to connect to
Dialog {
    id: root
    title: qsTr("Connect to guitarix engine")
    modal: true
    // plain background: Material's elevation layer is costly on small GPUs
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }
    anchors.centerIn: Overlay.overlay
    standardButtons: Dialog.Ok | Dialog.Cancel

    onOpened: {
        host.text = Gx.host || "localhost"
        port.value = Gx.port || 7000
    }
    onAccepted: Gx.connectTo(host.text, port.value)

    GridLayout {
        columns: 2
        columnSpacing: Theme.spacing
        rowSpacing: Theme.spacing

        Label { text: qsTr("Host") }
        TextField { id: host; Layout.preferredWidth: 220 }
        Label { text: qsTr("Port") }
        SpinBox { id: port; from: 1; to: 65535; editable: true }
        Label {
            Layout.columnSpan: 2
            Layout.maximumWidth: 320
            wrapMode: Text.WordWrap
            text: qsTr("Start the engine with: guitarix -N -p %1").arg(port.value)
            color: Theme.textDim
            font.pixelSize: Theme.fontSmall
        }
    }
}
