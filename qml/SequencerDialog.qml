import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Guitariqx

// The drum sequencer of the seq unit (gx_head's SEQWindow,
// gx_sequencer_settings.cpp): one line of steps per drum, the
// seq.sequencer.<drum> parameters, with tempo, beat and the stepper
// controls. The play head is the polled seq.pos (0 to 2300 over the
// sequence).
Dialog {
    id: root
    title: qsTr("Drum sequencer")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Overlay.overlay ? Math.min(1100, Overlay.overlay.width - 16) : 800
    height: Overlay.overlay ? Math.min(640, Overlay.overlay.height - 16) : 480
    padding: Theme.spacing
    standardButtons: Dialog.Close
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }

    readonly property var drums: [
        { id: "tom", name: qsTr("Tom"), gain: "seq.tom.dsp.Gain" },
        { id: "tom1", name: qsTr("Tom 1"), gain: "seq.tom.dsp.Gain1" },
        { id: "tom2", name: qsTr("Tom 2"), gain: "seq.tom.dsp.Gain2" },
        { id: "kick", name: qsTr("Kick"), gain: "seq.kick.dsp.Gain" },
        { id: "snare", name: qsTr("Snare"), gain: "seq.snare.dsp.Gain" },
        { id: "hat", name: qsTr("Hat"), gain: "seq.hat_closed.dsp.Gain" },
    ]
    function p(id) { return Gx.ready ? Gx.param(id) : null }
    readonly property Parameter stepsParam: p("seq.asequences")
    readonly property Parameter tactParam: p("seq.tact")
    readonly property Parameter posParam: p("seq.pos")
    readonly property Parameter stepParam: p("seq.step")
    readonly property Parameter stepOrigParam: p("seq.step_orig")
    readonly property Parameter onOff: p("seq.on_off")
    readonly property int steps: stepsParam ? Math.round(stepsParam.value) : 24
    // steps per beat
    readonly property int tact: tactParam ? Math.max(1, Math.round(tactParam.value)) : 4
    readonly property bool running: onOff ? !!onOff.value : false
    readonly property int playStep: posParam && steps > 0
                                    ? Math.min(steps - 1, Math.floor(posParam.value / 2300 * steps)) : 0
    readonly property real cell: Theme.touch + 2

    UnitPresetDialog { id: presetDialog; unitId: "seq" }
    OutputWatch { paramId: "seq.pos"; active: root.opened && root.running }
    OutputWatch { paramId: "seq.step"; active: root.opened && root.running }
    OutputWatch { paramId: "seq.step_orig"; active: root.opened && root.running }

    // the steps of a drum (edited by Gx, which keeps all the lines as long
    // as seq.asequences)
    function line(drum) {
        const prm = p("seq.sequencer." + drum)
        const v = prm && prm.value ? prm.value["seq.seqline"] : undefined
        return v ? v : []
    }
    // the stepper buttons of gx_head (SEQWindow::on_set_step ...)
    function setStep(v) {
        if (stepParam)
            stepParam.value = v
    }

    // small screens scroll the whole page, the steps scroll sideways
    Flickable {
        id: page
        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: content.implicitHeight
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {
            policy: page.contentHeight > page.height ? ScrollBar.AlwaysOn : ScrollBar.AsNeeded
        }

        ColumnLayout {
            id: content
            width: page.width
            spacing: Theme.spacing

            // tempo and controls
            Flow {
                Layout.fillWidth: true
                spacing: Theme.spacing

                ParamSwitch { param: root.onOff; label: qsTr("Run") }
                Knob { param: root.p("seq.bpm"); label: qsTr("BPM") }
                Knob { param: root.p("seq.gain"); label: qsTr("Gain") }
                ParamSelector { param: root.tactParam; label: qsTr("Beat"); labelColor: Theme.textDim }
                Column {
                    spacing: 2
                    Text {
                        text: qsTr("Steps")
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Row {
                        RoundButton {
                            text: "−"
                            enabled: root.stepsParam && root.steps > root.stepsParam.lower
                            onClicked: Gx.setSequencerSteps(root.steps - 4)
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36
                            horizontalAlignment: Text.AlignHCenter
                            text: root.steps
                            font.pixelSize: Theme.fontNormal
                        }
                        RoundButton {
                            text: "+"
                            enabled: root.stepsParam && root.steps < root.stepsParam.upper
                            onClicked: Gx.setSequencerSteps(root.steps + 4)
                        }
                    }
                }
                ParamSwitch { param: root.p("seq.hum"); label: qsTr("Humanize") }
                ParamSwitch { param: root.p("seq.follow"); label: qsTr("Follow") }
                ParamSwitch { param: root.p("seq.direct_out"); label: qsTr("Direct out") }
                // the unit presets: the patterns saved by the user
                Column {
                    spacing: 2
                    Text {
                        text: qsTr("Preset")
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Row {
                        RoundButton {
                            text: "◀"
                            onClicked: Gx.stepUnitPreset("seq", -1)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Previous preset")
                        }
                        Button {
                            text: qsTr("Presets…")
                            flat: true
                            onClicked: presetDialog.show()
                        }
                        RoundButton {
                            text: "▶"
                            onClicked: Gx.stepUnitPreset("seq", 1)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Next preset")
                        }
                    }
                }
                Column {
                    spacing: 2
                    Text {
                        text: qsTr("Step %1").arg(root.stepParam ? Math.round(root.stepParam.value) : 0)
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Row {
                        spacing: 2
                        RoundButton {
                            text: "|◀"
                            onClicked: { root.setStep(0); if (root.stepOrigParam) root.stepOrigParam.value = 0; if (root.posParam) root.posParam.value = 0 }
                        }
                        RoundButton {
                            text: "◀"
                            onClicked: root.setStep(Math.max(0, Math.round(root.stepParam.value) - root.tact))
                        }
                        RoundButton {
                            // back on the beat
                            text: "↺"
                            onClicked: root.setStep(Math.round(root.stepOrigParam.value))
                        }
                        RoundButton {
                            text: "▶"
                            onClicked: root.setStep(Math.min(root.steps, Math.round(root.stepParam.value) + root.tact))
                        }
                    }
                }
            }

            // drum levels
            Flow {
                Layout.fillWidth: true
                spacing: Theme.spacing
                Repeater {
                    model: root.drums
                    Knob {
                        required property var modelData
                        param: root.p(modelData.gain)
                        label: modelData.name
                    }
                }
            }

            // the steps; names on the left, steps scroll
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                Column {
                    Layout.alignment: Qt.AlignTop
                    spacing: 2
                    Repeater {
                        model: root.drums
                        Label {
                            required property var modelData
                            height: Theme.touch
                            width: implicitWidth
                            verticalAlignment: Text.AlignVCenter
                            text: modelData.name
                            font.pixelSize: Theme.fontNormal
                        }
                    }
                }

                Flickable {
                    id: flick
                    Layout.fillWidth: true
                    Layout.preferredHeight: grid.height + 14
                    clip: true
                    contentWidth: grid.width
                    contentHeight: grid.height
                    flickableDirection: Flickable.HorizontalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AlwaysOn }

                    // keep the play head in view
                    readonly property real playX: root.playStep * root.cell
                    onPlayXChanged: {
                        const follow = root.p("seq.follow")
                        if (root.running && follow && follow.value && (playX < contentX || playX + root.cell > contentX + width))
                            contentX = Math.max(0, Math.min(contentWidth - width, playX - width / 4))
                    }

                    Column {
                        id: grid
                        spacing: 2
                        Repeater {
                            model: root.drums
                            Row {
                                id: drumRow
                                required property var modelData
                                readonly property var steps_: root.line(modelData.id)
                                spacing: 2
                                Repeater {
                                    model: root.steps
                                    Rectangle {
                                        required property int index
                                        readonly property bool on: !!drumRow.steps_[index]
                                        readonly property bool beat: index % root.tact === 0
                                        width: Theme.touch
                                        height: Theme.touch
                                        radius: 4
                                        color: on ? Theme.accent : beat ? Theme.panelRaised : Theme.knobTrack
                                        border.color: root.running && index === root.playStep ? Theme.text : Theme.panelBorder
                                        border.width: root.running && index === root.playStep ? 2 : 1
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: Gx.toggleSequencerStep(drumRow.modelData.id, parent.index)
                                        }
                                    }
                                }
                            }
                        }
                        // step numbers of the beats
                        Row {
                            spacing: 2
                            Repeater {
                                model: root.steps
                                Text {
                                    required property int index
                                    width: Theme.touch
                                    horizontalAlignment: Text.AlignHCenter
                                    text: index % root.tact === 0 ? (index / root.tact + 1) : ""
                                    color: Theme.textDim
                                    font.pixelSize: Theme.fontSmall
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
