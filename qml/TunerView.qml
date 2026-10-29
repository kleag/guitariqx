import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Guitariqx

// Full window tuner. It is shown while the engine parameter
// system.show_tuner is on, so a MIDI foot switch assigned to it (or the
// Tuner button) opens and closes it, also from other connected UIs.
Popup {
    id: root
    readonly property Parameter showParam: Gx.ready ? Gx.param("system.show_tuner") : null
    readonly property bool wanted: showParam ? !!showParam.value : false
    // mute the output while tuning and restore the engine state afterwards
    property bool muteWhileTuning: true
    property string savedState: ""
    readonly property Tuner tuner: Gx.tuner
    readonly property real inTune: 3     // cents
    readonly property bool tuned: tuner.hasPitch && Math.abs(tuner.cents) < inTune

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 800
    height: parent ? parent.height : 480
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: Theme.spacing * 2

    background: Rectangle { color: Theme.background }

    onWantedChanged: wanted ? open() : close()
    onOpened: {
        Gx.tuner.active = true
        if (muteWhileTuning && Gx.engineState !== "stopped") {
            savedState = Gx.engineState
            Gx.setEngineState("stopped")
        }
    }
    onClosed: {
        Gx.tuner.active = false
        if (savedState !== "") {
            Gx.setEngineState(savedState)
            savedState = ""
        }
        if (showParam && showParam.value)
            showParam.value = false
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Tuner")
                font.pixelSize: Theme.fontLarge
                font.bold: true
                color: Theme.textDim
            }
            Item { Layout.fillWidth: true }
            Switch {
                text: qsTr("Mute while tuning")
                checked: root.muteWhileTuning
                onToggled: {
                    root.muteWhileTuning = checked
                    if (checked && Gx.engineState !== "stopped") {
                        root.savedState = Gx.engineState
                        Gx.setEngineState("stopped")
                    } else if (!checked && root.savedState !== "") {
                        Gx.setEngineState(root.savedState)
                        root.savedState = ""
                    }
                }
            }
            Button {
                text: qsTr("Close")
                onClicked: root.close()
                MidiLearnArea { paramId: "system.show_tuner" }
            }
        }

        // note name
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 120

            Row {
                anchors.centerIn: parent
                spacing: 4
                opacity: root.tuner.hasPitch ? 1 : 0.25
                Text {
                    id: noteText
                    text: root.tuner.noteName || "-"
                    color: root.tuned ? Theme.meterLow : Theme.text
                    font.pixelSize: Math.min(root.height * 0.28, 180)
                    font.bold: true
                }
                Text {
                    anchors.baseline: noteText.baseline
                    text: root.tuner.noteName ? root.tuner.octave : ""
                    color: Theme.textDim
                    font.pixelSize: noteText.font.pixelSize * 0.35
                }
            }
        }

        // deviation scale
        Item {
            id: dial
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            readonly property real range: root.tuner.range
            readonly property real pos: root.tuner.hasPitch
                                        ? Math.max(-1, Math.min(1, root.tuner.cents / range)) : 0

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 6
                radius: 3
                color: Theme.panelBorder
            }
            // in tune zone
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.max(8, parent.width * root.inTune / dial.range)
                height: 22
                radius: 4
                color: root.tuned ? Theme.meterLow : "#2a4a2c"
            }
            Repeater {
                model: 11
                Rectangle {
                    required property int index
                    x: dial.width * index / 10 - 1
                    anchors.verticalCenter: parent.verticalCenter
                    width: 2
                    height: index === 5 ? 40 : index % 5 === 0 ? 28 : 16
                    color: Theme.textDim
                }
            }
            Rectangle {
                id: needle
                visible: root.tuner.hasPitch
                width: 6
                height: 60
                radius: 3
                anchors.verticalCenter: parent.verticalCenter
                x: (dial.width - width) * (dial.pos + 1) / 2
                color: root.tuned ? Theme.meterLow : Math.abs(root.tuner.cents) < 15 ? Theme.meterMid : Theme.meterHigh
                Behavior on x { NumberAnimation { duration: 90 } }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "♭"
                color: Theme.textDim
                font.pixelSize: Theme.fontLarge
            }
            Item { Layout.fillWidth: true }
            Text {
                text: root.tuner.hasPitch
                      ? qsTr("%1 Hz   %2%3 cents").arg(root.tuner.frequency.toFixed(1))
                            .arg(root.tuner.cents >= 0 ? "+" : "").arg(root.tuner.cents.toFixed(1))
                      : qsTr("Play a string")
                color: Theme.textDim
                font.pixelSize: Theme.fontNormal
            }
            Item { Layout.fillWidth: true }
            Text {
                text: "♯"
                color: Theme.textDim
                font.pixelSize: Theme.fontLarge
            }
        }

        // strings of the selected tuning
        Row {
            Layout.alignment: Qt.AlignHCenter
            visible: root.tuner.strings.length > 0
            spacing: Theme.spacing
            Repeater {
                model: root.tuner.strings
                Rectangle {
                    required property string modelData
                    required property int index
                    readonly property bool current: root.tuner.hasPitch && root.tuner.stringIndex === index
                    width: 52
                    height: Theme.touch
                    radius: Theme.radius
                    color: current ? (root.tuned ? Theme.meterLow : Theme.accentDim) : Theme.panelRaised
                    Text {
                        anchors.centerIn: parent
                        text: parent.modelData
                        color: Theme.text
                        font.pixelSize: Theme.fontLarge
                        font.bold: parent.current
                    }
                }
            }
        }

        Flow {
            Layout.fillWidth: true
            spacing: Theme.spacing * 2
            ParamSelector {
                label: qsTr("Tuning")
                param: Gx.ready ? Gx.param("racktuner.tuning") : null
            }
            ParamSelector {
                label: qsTr("Temperament")
                param: Gx.ready ? Gx.param("racktuner.temperament") : null
            }
            ParamSpin {
                label: qsTr("Reference A4 (Hz)")
                param: Gx.ready ? Gx.param("ui.tuner_reference_pitch") : null
            }
        }
    }
}
