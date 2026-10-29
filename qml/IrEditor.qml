import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Guitariqx

// Impulse response of a convolver (gx_head's IR window, IRWindow and
// GxIREdit): the level of the response in dB with the gain line drawn on
// it. The handles above the view set the part of the file used; a tap on
// the gain line adds a point, points are dragged, and the selected one
// can be removed. The changes are sent with Apply.
Dialog {
    id: root
    // jconv or jconv_mono
    property string unit: ""
    property int selected: -1
    readonly property bool wide: width >= 640

    title: qsTr("Impulse response")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Overlay.overlay ? Math.min(1000, Overlay.overlay.width - 16) : 800
    height: Overlay.overlay ? Math.min(640, Overlay.overlay.height - 16) : 480
    padding: Theme.spacing
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }

    function edit(unit_) {
        selected = -1
        // setting another unit reads its settings
        if (unit === unit_)
            ir.reload()
        unit = unit_
        open()
    }

    ImpulseResponse {
        id: ir
        unit: root.unit
        onFileLoaded: root.selected = -1
    }

    function ms(samples) {
        return ir.rate > 0 ? samples * 1000 / ir.rate : 0
    }
    function samples(ms_) {
        return Math.round(ms_ * ir.rate / 1000)
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Reset")
            enabled: ir.loaded
            DialogButtonBox.buttonRole: DialogButtonBox.ResetRole
            onClicked: {
                root.selected = -1
                ir.resetSettings()
            }
        }
        Button {
            text: qsTr("Apply")
            enabled: ir.loaded && ir.modified
            DialogButtonBox.buttonRole: DialogButtonBox.ApplyRole
            onClicked: ir.apply()
        }
        Button {
            text: qsTr("Close")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        Label {
            Layout.fillWidth: true
            elide: Text.ElideMiddle
            text: ir.error !== "" ? ir.error
                : ir.loaded ? qsTr("%1 — %2 ch, %3 Hz, %4 ms").arg(ir.file).arg(ir.channels).arg(ir.rate)
                                  .arg(root.ms(ir.frames).toFixed(0))
                : qsTr("Loading %1…").arg(ir.file)
            color: ir.error !== "" ? Theme.meterHigh : Theme.textDim
            font.pixelSize: Theme.fontSmall
        }

        // start and end of the part used
        Item {
            id: handles
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touch
            enabled: ir.loaded
            visible: ir.loaded

            Repeater {
                model: [
                    { name: qsTr("Start"), sample: ir.offset },
                    { name: qsTr("End"), sample: ir.offset + ir.length - 1 },
                ]
                Item {
                    id: handle
                    required property var modelData
                    required property int index
                    width: Theme.touch
                    height: Theme.touch
                    x: Math.max(0, Math.min(handles.width - width, view.xForSample(modelData.sample) - width / 2))
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: 18
                        height: 26
                        radius: 3
                        color: drag.pressed ? Theme.accent : Theme.accentDim
                        border.color: Theme.accent
                        Text {
                            anchors.centerIn: parent
                            text: handle.index === 0 ? "[" : "]"
                            color: Theme.text
                            font.pixelSize: Theme.fontNormal
                            font.bold: true
                        }
                    }
                    MouseArea {
                        id: drag
                        anchors.fill: parent
                        preventStealing: true
                        onPositionChanged: (mouse) => {
                            const s = view.sampleAt(handle.mapToItem(view, mouse.x, 0).x)
                            if (handle.index === 0)
                                ir.offset = s
                            else
                                ir.length = s - ir.offset + 1
                        }
                    }
                }
            }
        }

        WaveView {
            id: view
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100
            impulse: ir
            selectedPoint: root.selected
            backgroundColor: Theme.knobTrack
            gridColor: Theme.panelBorder
            waveColor: Theme.meterLow
            gainLineColor: Theme.accent

            Text {
                x: 4
                y: 2
                text: "0 dB"
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall
            }
            Text {
                x: 4
                anchors.bottom: parent.bottom
                text: "-120 dB"
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall
            }

            // gain line editing; points and the line are touched within
            // half the touch size
            MouseArea {
                id: edit
                anchors.fill: parent
                enabled: ir.loaded
                preventStealing: true
                readonly property real reach: Theme.touch / 2
                property int dragged: -1

                function nearestPoint(x, y) {
                    const line = ir.gainLine
                    let best = -1
                    let bestD = reach * reach
                    for (let i = 0; i < line.length; ++i) {
                        const dx = view.xForSample(line[i].i) - x
                        const dy = view.yForGain(line[i].g) - y
                        const d = dx * dx + dy * dy
                        if (d <= bestD) {
                            best = i
                            bestD = d
                        }
                    }
                    return best
                }

                onPressed: (mouse) => {
                    let p = nearestPoint(mouse.x, mouse.y)
                    if (p < 0) {
                        const s = view.sampleAt(mouse.x)
                        if (Math.abs(view.yForGain(ir.gainAt(s)) - mouse.y) <= reach)
                            p = ir.addPoint(s, ir.gainAt(s))
                    }
                    root.selected = p
                    dragged = p
                }
                onPositionChanged: (mouse) => {
                    if (dragged >= 0)
                        ir.movePoint(dragged, view.sampleAt(mouse.x), view.gainAt(mouse.y))
                }
                onReleased: dragged = -1
                onCanceled: dragged = -1
                // mouse extra: double click removes a point
                onDoubleClicked: (mouse) => {
                    const p = nearestPoint(mouse.x, mouse.y)
                    if (p > 0) {
                        ir.removePoint(p)
                        root.selected = -1
                    }
                }
            }
        }

        // the settings as numbers, in milliseconds
        GridLayout {
            Layout.fillWidth: true
            columns: root.wide ? 8 : 4
            columnSpacing: Theme.spacing
            rowSpacing: 2
            enabled: ir.loaded

            Label { text: qsTr("Start (ms)"); font.pixelSize: Theme.fontSmall; color: Theme.textDim }
            SpinBox {
                from: 0
                to: Math.max(0, Math.round(root.ms(ir.frames - 1)))
                value: Math.round(root.ms(ir.offset))
                editable: true
                onValueModified: ir.offset = root.samples(value)
            }
            Label { text: qsTr("Length (ms)"); font.pixelSize: Theme.fontSmall; color: Theme.textDim }
            SpinBox {
                from: 1
                to: Math.max(1, Math.round(root.ms(ir.frames)))
                value: Math.round(root.ms(ir.length))
                editable: true
                onValueModified: ir.length = root.samples(value)
            }
            Label { text: qsTr("Delay (ms)"); font.pixelSize: Theme.fontSmall; color: Theme.textDim }
            SpinBox {
                from: 0
                to: 10000
                value: Math.round(root.ms(ir.delay))
                editable: true
                onValueModified: ir.delay = root.samples(value)
            }
            Switch {
                Layout.columnSpan: 1
                text: qsTr("Gain correction")
                checked: ir.gainCorrection
                onToggled: ir.gainCorrection = checked
            }
            Button {
                // the end points of the gain line stay
                text: root.selected >= 0 ? qsTr("Remove point (%1 dB)").arg(ir.gainLine[root.selected]
                                                                           ? ir.gainLine[root.selected].g.toFixed(1) : "")
                                         : qsTr("Remove point")
                enabled: root.selected > 0 && root.selected < ir.gainLine.length - 1
                onClicked: {
                    ir.removePoint(root.selected)
                    root.selected = -1
                }
            }
        }
    }
}
