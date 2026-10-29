import QtQuick
import Guitariqx

// On/off switch for a bool parameter, drawn as a pedal style LED button,
// or with the switch icons of a gx_head skin (switchType: their base name)
Item {
    id: root
    property Parameter param: null
    property string label: param && param.name !== "on/off" ? param.name : ""
    property string switchType: ""
    readonly property bool on: param ? !!param.value : false
    readonly property url icon: Theme.skinImage((switchType || "switch") + (on ? "_on" : "_off"))
    readonly property bool skinned: icon.toString() !== ""

    implicitWidth: Math.max(Theme.touch, labelText.implicitWidth)
    implicitHeight: (label ? labelText.height + 2 : 0) + Theme.touch
    opacity: param ? 1 : 0.4

    Text {
        id: labelText
        visible: root.label !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        color: Theme.rackLabel
        font.pixelSize: Theme.fontSmall
    }

    Rectangle {
        id: button
        width: Theme.touch
        height: Theme.touch
        radius: Theme.radius
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        color: root.skinned ? (tap.pressed ? "#60000000" : "#30000000")
                            : tap.pressed ? Theme.panelBorder : Theme.panelRaised
        border.color: root.skinned ? "transparent" : Theme.panelBorder

        // the icons are 7 to 37 pixels: scaled to about 24
        Image {
            visible: root.skinned
            anchors.centerIn: parent
            source: root.icon
            readonly property real scale_: Math.min(28 / Math.max(implicitWidth, 1), 24 / Math.max(implicitHeight, 1))
            width: implicitWidth * scale_
            height: implicitHeight * scale_
            smooth: true
            mipmap: true
        }

        Rectangle {
            visible: !root.skinned
            anchors.centerIn: parent
            width: 14
            height: 14
            radius: 7
            color: root.on ? Theme.ledOn : Theme.ledOff
            border.color: "#00000080"
        }

        MouseArea {
            id: tap
            anchors.fill: parent
            onClicked: if (root.param) root.param.value = !root.on
        }
    }

    MidiLearnArea { paramId: root.param ? root.param.id : "" }
}
