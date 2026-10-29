import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Guitariqx

// Full window display of the engine's tuner switcher (gx_head's Liveplay
// preset mode): the output is muted and the note played selects a bank or
// a preset, loaded when the note stops. Shown while the engine reports the
// switcher active, whoever started it (menu, MIDI controller assigned to
// ui.live_play_switcher, another UI).
Popup {
    id: root
    readonly property Tuner tuner: Gx.tuner
    readonly property string phase: tuner.switcherState
    // bank whose preset notes are listed: the one selected, else the current one
    readonly property string legendBank: (Gx.banks, Gx.bank(tuner.switcherBank).presets !== undefined
                                          ? tuner.switcherBank : Gx.currentBank)

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 800
    height: parent ? parent.height : 480
    modal: true
    closePolicy: Popup.NoAutoClose
    padding: Theme.spacing * 2

    background: Rectangle { color: Theme.background }

    // the engine ends it: by a note, by the MIDI controller or by Cancel
    Connections {
        target: root.tuner
        function onSwitcherChanged() {
            if (root.tuner.switcherActive)
                root.open()
            else
                root.close()
        }
    }
    Component.onCompleted: if (tuner.switcherActive) open()

    component Key: Row {
        property string note
        property string name
        property bool current: false
        spacing: 6
        Text {
            width: 34
            text: parent.note
            color: Theme.accent
            font.pixelSize: Theme.fontSmall
            font.bold: true
        }
        Text {
            width: 150
            text: parent.name
            color: parent.current ? Theme.text : Theme.textDim
            font.pixelSize: Theme.fontSmall
            font.bold: parent.current
            elide: Text.ElideRight
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Preset by note")
                font.pixelSize: Theme.fontLarge
                font.bold: true
                color: Theme.textDim
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                color: root.phase === "wait_stop" ? Theme.accent : Theme.text
                text: root.phase === "wait_start" ? qsTr("Mute the strings ...")
                    : root.phase === "wait_stop" ? qsTr("Stop the note to load")
                    : qsTr("Play a note")
            }
            Button {
                text: qsTr("Cancel")
                onClicked: root.tuner.cancelSwitcher()
                MidiLearnArea { paramId: "ui.live_play_switcher" }
            }
        }

        // selection
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            opacity: root.phase === "wait_start" ? 0.4 : 1
            Item { Layout.fillHeight: true }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: root.tuner.switcherBank
                visible: text !== ""
                color: Theme.textDim
                font.pixelSize: Math.min(36, root.height / 12)
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: root.tuner.switcherPreset || "--"
                color: root.phase === "wait_stop" ? Theme.accent : Theme.text
                font.pixelSize: Math.min(64, root.height / 7)
                font.bold: true
                elide: Text.ElideRight
            }
            Item { Layout.fillHeight: true }
        }

        // which note selects what (TunerSwitcher::on_note_timeout)
        Flickable {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(legend.implicitHeight, root.height * 0.4)
            contentHeight: legend.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: legend
                width: parent.width
                spacing: 4
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacing
                    Repeater {
                        model: (Gx.banks, root.tuner.switcherBankKeys())
                        Key {
                            required property var modelData
                            note: modelData.note
                            name: modelData.bank
                            current: modelData.bank === root.legendBank
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Theme.panelBorder
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.spacing
                    Repeater {
                        model: (Gx.banks, root.tuner.switcherPresetKeys(root.legendBank))
                        Key {
                            required property var modelData
                            note: modelData.note
                            name: modelData.preset
                            current: root.legendBank === Gx.currentBank && modelData.preset === Gx.currentPreset
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSmall
                    text: qsTr("G2: bypass, G#2: mute, below G2 or F5 to A#6: tuner")
                }
            }
        }
    }
}
