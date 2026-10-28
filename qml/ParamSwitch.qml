import QtQuick
import QtHead

// On/off switch for a bool parameter, drawn as a pedal style LED button
Item {
    id: root
    property Parameter param: null
    property string label: param && param.name !== "on/off" ? param.name : ""
    readonly property bool on: param ? !!param.value : false

    implicitWidth: Math.max(Theme.touch, labelText.implicitWidth)
    implicitHeight: (label ? labelText.height + 2 : 0) + Theme.touch
    opacity: param ? 1 : 0.4

    Text {
        id: labelText
        visible: root.label !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        color: Theme.textDim
        font.pixelSize: Theme.fontSmall
    }

    Rectangle {
        id: button
        width: Theme.touch
        height: Theme.touch
        radius: Theme.radius
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        color: tap.pressed ? Theme.panelBorder : Theme.panelRaised
        border.color: Theme.panelBorder

        Rectangle {
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
}
