import QtQuick
import QtQuick.Controls
import Guitariqx

// A leaf of a rack unit layout (see uitree.h): picks the widget for the
// control type and binds it to the parameter
Item {
    id: root
    property var node: ({})
    readonly property string widget: node.widget || ""
    readonly property Parameter param: Gx.ready && node.id ? Gx.param(node.id) : null
    // noLabel: a Glade layout labels the control with a separate label
    readonly property string label: node.noLabel ? ""
                                  : node.label !== undefined && node.label !== "" ? node.label.trim()
                                  : (param ? param.name : "")

    // outputs shown as values or switches (the looper's buffer time,
    // record and play) change in the engine without notification
    OutputWatch {
        paramId: root.param && root.param.isOutput && root.widget !== "meter" && root.widget !== "oscilloscope"
                 && !root.node.id2 ? root.param.id : ""
        active: root.visible
    }

    // --debug-state iredit:UNIT, sequencer: open the editor of the unit
    function debugState(name) {
        const args = Qt.application.arguments
        const i = args.indexOf("--debug-state")
        return i >= 0 && i + 1 < args.length && args[i + 1].split(",").indexOf(name) >= 0
    }

    implicitWidth: loader.item ? loader.item.implicitWidth : 0
    implicitHeight: loader.item ? loader.item.implicitHeight : 0
    visible: param !== null || widget === "meter"

    Component {
        id: knob
        Knob { param: root.param; label: root.label; size: root.node.size || "small"; image: root.node.knob || "" }
    }
    // create_fload_switch, and the impulse response of convolvers
    Component {
        id: fileButton
        FileButton { paramId: root.node.id || "" }
    }
    Component {
        id: slider
        ParamSlider {
            param: root.param
            label: root.node.size === "eq" ? "" : root.label
            vertical: root.node.size === "eq"
        }
    }
    Component {
        id: toggle
        ParamSwitch {
            param: root.param
            label: root.node.label || ""
            nameLabel: !!root.node.nameLabel
            switchType: root.node.switchType || ""
        }
    }
    Component {
        id: selector
        ParamSelector { param: root.param; label: root.node.label || ""; labelColor: Theme.rackLabel; solid: Theme.skinned }
    }
    Component {
        id: spin
        ParamSpin { param: root.param; label: root.label; labelColor: Theme.rackLabel }
    }
    Component {
        id: meter
        LevelMeter {
            paramId: root.node.id || ""
            vertical: !root.node.horizontal
            implicitHeight: vertical ? 90 : 10
            implicitWidth: vertical ? 10 : 120
            thickness: 10
        }
    }
    // create_simple_c_meter: level meter next to a gain slider
    Component {
        id: cmeter
        Column {
            id: cm
            spacing: 2
            readonly property Parameter gain: Gx.ready ? Gx.param(root.node.id2) : null
            Row {
                spacing: 2
                anchors.horizontalCenter: parent.horizontalCenter
                LevelMeter { paramId: root.node.id || ""; vertical: true; height: 130; thickness: 6 }
                ParamSlider { param: cm.gain; vertical: true; label: ""; implicitHeight: 130 }
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.node.label || ""
                color: Theme.rackLabel
                font.pixelSize: Theme.fontSmall
            }
        }
    }
    // play head of a looper tape
    Component {
        id: loopPosition
        LoopPosition {
            param: root.param
            cut: Gx.ready ? Gx.param(root.node.id2) : null
            clip: Gx.ready ? Gx.param(root.node.id3) : null
        }
    }
    // the button opening the IR editor of a convolver; the dialogs are
    // created when first opened
    Component {
        id: irEdit
        Button {
            id: irButton
            readonly property string unit: (root.node.id || "").replace(/\.convolver$/, "")
            text: root.node.label || qsTr("Setup")
            implicitHeight: Math.max(Theme.touch, implicitContentHeight + topPadding + bottomPadding)
            property var editor: null
            function show() {
                if (!editor)
                    editor = editorComponent.createObject(irButton)
                editor.edit(unit)
            }
            onClicked: show()
            Component {
                id: editorComponent
                IrEditor {}
            }
            readonly property bool debugOpen: Gx.ready && visible && root.debugState("iredit:" + unit)
            onDebugOpenChanged: if (debugOpen) show()
        }
    }
    Component {
        id: sequencer
        Button {
            id: seqButton
            text: root.node.label || qsTr("Setup")
            implicitHeight: Math.max(Theme.touch, implicitContentHeight + topPadding + bottomPadding)
            property var dialog: null
            function show() {
                if (!dialog)
                    dialog = dialogComponent.createObject(seqButton)
                dialog.open()
            }
            onClicked: show()
            Component {
                id: dialogComponent
                SequencerDialog {}
            }
            readonly property bool debugOpen: Gx.ready && visible && root.debugState("sequencer")
            onDebugOpenChanged: if (debugOpen) show()
        }
    }
    Component {
        id: oscilloscope
        ScopeView { param: root.param }
    }
    Component {
        id: display
        Column {
            spacing: 2
            Text {
                text: root.label
                color: Theme.rackLabel
                font.pixelSize: Theme.fontSmall
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: root.param ? root.param.displayValue : ""
                color: Theme.skinned ? Theme.rackValue : Theme.accent
                font.pixelSize: Theme.fontNormal
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }

    Loader {
        id: loader
        sourceComponent: {
            switch (root.widget) {
            case "knob":
            case "wheel":
                return root.param && root.param.kind === Parameter.Enum ? selector
                     : root.param && root.param.kind === Parameter.Bool ? toggle : knob
            case "slider":
                return slider
            case "switch":
                return toggle
            case "selector":
                return selector
            case "spin":
                return spin
            case "meter":
                return root.node.id2 ? cmeter : meter
            case "file":
            case "ir":
                return fileButton
            case "display":
                return root.node.id2 ? loopPosition : display
            case "iredit":
                return irEdit
            case "sequencer":
                return sequencer
            case "oscilloscope":
                return oscilloscope
            }
            return null
        }
    }
}
