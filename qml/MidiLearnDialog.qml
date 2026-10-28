import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtHead

// Assign a MIDI controller to a parameter (gx_head: MidiConnect).
// Opened while Gx.midi.learning; move a controller or type its number.
Dialog {
    id: root
    readonly property string paramId: Gx.midi.learnParam
    readonly property Parameter param: paramId && Gx.ready ? Gx.param(paramId) : null
    readonly property bool range: paramId !== "" && Gx.midi.usesRange(paramId)
    readonly property var current: (Gx.midi.revision, paramId ? Gx.midi.assignment(paramId) : ({}))
    readonly property bool assigned: current.ctl !== undefined
    readonly property int control: Gx.midi.learnControl

    title: qsTr("MIDI learn")
    modal: true
    // plain background: Material's elevation layer is costly on small GPUs
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    width: Math.min(460, Overlay.overlay ? Overlay.overlay.width - 24 : 460)

    // set up the fields for the parameter being learned
    function initFields() {
        if (!param)
            return
        if (range) {
            lower.realValue = assigned ? current.lower : param.lower
            upper.realValue = assigned ? current.upper : param.upper
        } else {
            useToggle.checked = assigned ? current.toggle : false
            behaviour.currentIndex = assigned ? current.behaviour : MidiMap.OnOff
        }
    }
    onParamIdChanged: initFields()
    onOpened: initFields()

    // note on events are switches: default to a constant toggle (like gx_head)
    onControlChanged: {
        if (control > 200 && !range) {
            useToggle.checked = true
            behaviour.currentIndex = MidiMap.Constant
        }
    }

    onRejected: Gx.midi.cancelLearn()

    // float entry with the parameter's step
    component RealSpin: SpinBox {
        id: spin
        property real realValue: 0
        readonly property real scale: root.param && root.param.step > 0 && root.param.step < 1
                                      ? Math.round(1 / root.param.step) : 1
        from: root.param ? Math.round(root.param.lower * scale) : 0
        to: root.param ? Math.round(root.param.upper * scale) : 0
        stepSize: root.param && root.param.step > 0 ? Math.max(1, Math.round(root.param.step * scale)) : 1
        editable: true
        value: Math.round(realValue * scale)
        onValueModified: realValue = value / scale
        textFromValue: (v, locale) => (v / spin.scale).toLocaleString(locale, 'f', spin.scale > 1 ? Math.round(Math.log(spin.scale) / Math.LN10) : 0)
        valueFromText: (t, locale) => Math.round(Number.fromLocaleString(locale, t) * spin.scale)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        Label {
            Layout.fillWidth: true
            text: root.param ? root.param.group + ": " + root.param.name : root.paramId
            font.pixelSize: Theme.fontLarge
            font.bold: true
            wrapMode: Text.WordWrap
        }
        Label {
            text: root.paramId
            color: Theme.textDim
            font.pixelSize: Theme.fontSmall
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            radius: Theme.radius
            color: Theme.panelRaised
            border.color: root.control >= 0 ? Theme.accent : Theme.panelBorder
            Label {
                id: ctlLabel
                anchors.centerIn: parent
                text: root.control >= 0 ? Gx.midi.controllerName(root.control)
                                        : qsTr("Move a MIDI controller ...")
                font.pixelSize: root.control >= 0 ? Theme.fontLarge + 4 : Theme.fontNormal
                font.bold: root.control >= 0
                color: root.control >= 0 ? Theme.accent : Theme.textDim
                SequentialAnimation on opacity {
                    running: root.visible && root.control < 0
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.35; duration: 700 }
                    NumberAnimation { to: 1; duration: 700 }
                    onRunningChanged: if (!running) ctlLabel.opacity = 1
                }
            }
        }

        RowLayout {
            Label { text: qsTr("Controller number") }
            Item { Layout.fillWidth: true }
            SpinBox {
                from: -1
                to: 327
                editable: true
                value: root.control
                textFromValue: (v) => v < 0 ? "---" : String(v)
                valueFromText: (t) => t === "---" || t === "" ? -1 : parseInt(t)
                onValueModified: Gx.midi.setLearnControl(value)
            }
        }

        GridLayout {
            visible: root.range
            columns: 2
            Label { text: qsTr("Lower limit") }
            RealSpin { id: lower; Layout.alignment: Qt.AlignRight }
            Label { text: qsTr("Upper limit") }
            RealSpin { id: upper; Layout.alignment: Qt.AlignRight }
        }

        ColumnLayout {
            visible: !root.range
            CheckBox {
                id: useToggle
                text: qsTr("Toggle (for momentary foot switches)")
            }
            ComboBox {
                id: behaviour
                Layout.fillWidth: true
                enabled: useToggle.checked
                // Parameter::toggle_type, texts of gx_main_midi.h
                model: [
                    qsTr("off/on states transition toggle"),
                    qsTr("constant state toggle"),
                    qsTr("toggle state"),
                    qsTr("always toggle"),
                ]
            }
        }

        Label {
            Layout.fillWidth: true
            visible: root.assigned
            text: qsTr("Currently: %1").arg(Gx.midi.controllerName(root.current.ctl))
            color: Theme.textDim
            font.pixelSize: Theme.fontSmall
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Remove")
            visible: root.assigned
            DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
            onClicked: {
                Gx.midi.remove(root.paramId)
                Gx.midi.cancelLearn()
                root.close()
            }
        }
        Button {
            text: qsTr("Cancel")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: qsTr("Connect")
            enabled: root.control >= 0
            highlighted: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            onClicked: {
                Gx.midi.commitLearn(lower.realValue, upper.realValue, useToggle.checked, behaviour.currentIndex)
                root.close()
            }
        }
    }
}
