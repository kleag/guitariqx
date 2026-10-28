import QtQuick
import QtHead

// Rotary control for a float or int parameter.
// Drag vertically (hold Shift for fine steps), use the mouse wheel;
// double click resets to the default value.
Item {
    id: root
    property Parameter param: null
    property string label: param ? param.name : ""
    property string size: "small"
    readonly property real diameter: Theme.knobSize(size)
    readonly property real value: param ? param.normalized : 0
    property bool active: drag.pressed

    implicitWidth: Math.max(diameter + 8, labelText.implicitWidth)
    implicitHeight: labelText.height + diameter + valueText.height + 4
    opacity: param ? 1 : 0.4

    Text {
        id: labelText
        anchors.horizontalCenter: parent.horizontalCenter
        height: text === "" ? 0 : implicitHeight
        text: root.label
        color: Theme.textDim
        font.pixelSize: Theme.fontSmall
        elide: Text.ElideRight
        width: Math.min(implicitWidth, root.diameter + 24)
        horizontalAlignment: Text.AlignHCenter
    }

    Item {
        id: dial
        width: root.diameter
        height: root.diameter
        anchors.top: labelText.bottom
        anchors.topMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter

        // value arc, 270 degrees from 7:30 to 4:30
        Canvas {
            id: arc
            anchors.fill: parent
            property real v: root.value
            onVChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const c = width / 2
                const r = c - 3
                const a0 = Math.PI * 0.75
                const a1 = a0 + Math.PI * 1.5
                ctx.lineWidth = 4
                ctx.lineCap = "round"
                ctx.strokeStyle = Theme.knobTrack
                ctx.beginPath()
                ctx.arc(c, c, r, a0, a1)
                ctx.stroke()
                ctx.strokeStyle = Theme.accent
                ctx.beginPath()
                ctx.arc(c, c, r, a0, a0 + Math.PI * 1.5 * v)
                ctx.stroke()
            }
        }

        Rectangle {
            id: face
            anchors.centerIn: parent
            width: parent.width - 14
            height: width
            radius: width / 2
            color: Theme.knobFace
            border.color: root.active ? Theme.accent : "#55555e"
            border.width: 1
            rotation: -135 + 270 * root.value

            Rectangle {
                width: 3
                height: face.height * 0.32
                radius: 1.5
                color: Theme.text
                anchors.horizontalCenter: parent.horizontalCenter
                y: 4
            }
        }
    }

    Text {
        id: valueText
        anchors.top: dial.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.param ? root.param.displayValue : ""
        color: root.active ? Theme.accent : Theme.text
        font.pixelSize: Theme.fontSmall
    }

    MouseArea {
        id: drag
        anchors.fill: dial
        anchors.margins: -6
        preventStealing: true
        property real startY: 0
        property real startValue: 0
        onPressed: (mouse) => {
            startY = mouse.y
            startValue = root.value
        }
        onPositionChanged: (mouse) => {
            if (!root.param)
                return
            const range = (mouse.modifiers & Qt.ShiftModifier) ? 1000 : 200
            root.param.normalized = startValue + (startY - mouse.y) / range
        }
        onDoubleClicked: if (root.param) root.param.reset()
        onWheel: (wheel) => {
            if (!root.param)
                return
            const steps = wheel.angleDelta.y / 120
            const p = root.param
            const inc = (p.step > 0 && p.upper > p.lower) ? p.step / (p.upper - p.lower) : 0.01
            p.normalized = p.normalized + steps * Math.max(inc, 0.005)
        }
    }

    MidiLearnArea { paramId: root.param ? root.param.id : "" }
}
