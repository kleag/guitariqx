import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtHead

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

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        RowLayout {
            Label { text: qsTr("MIDI channel") }
            ParamSelector { param: Gx.ready ? Gx.param("system.midi_channel") : null }
            Item { Layout.fillWidth: true }
            Switch {
                text: qsTr("Learn mode")
                checked: Gx.midi.learnMode
                onToggled: Gx.midi.learnMode = checked
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
                    Label {
                        Layout.preferredWidth: 150
                        text: row.modelData.ctlName
                        color: Theme.accent
                        elide: Text.ElideRight
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
