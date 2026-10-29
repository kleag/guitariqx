import QtQuick
import QtQuick.Controls
import Guitariqx

// Horizontal slider for a float parameter; vertical for eq bands
Item {
    id: root
    property Parameter param: null
    property string label: param ? param.name : ""
    property bool vertical: false
    property bool showValue: true

    implicitWidth: vertical ? Math.max(Theme.touch, labelText.implicitWidth) : 180
    implicitHeight: vertical ? 150 : Math.max(Theme.touch, slider.implicitHeight) + (label ? labelText.height : 0)
    opacity: param ? 1 : 0.4

    Text {
        id: labelText
        visible: root.label !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.showValue && root.param ? root.label + "  " + root.param.displayValue : root.label
        color: Theme.textDim
        font.pixelSize: Theme.fontSmall
    }

    Slider {
        id: slider
        anchors.top: labelText.visible ? labelText.bottom : parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        orientation: root.vertical ? Qt.Vertical : Qt.Horizontal
        from: 0
        to: 1
        value: root.param ? root.param.normalized : 0
        onMoved: if (root.param) root.param.normalized = value
    }

    MidiLearnArea { paramId: root.param ? root.param.id : "" }
}
