import QtQuick
import QtQuick.Controls
import QtHead

// Combo box for an enum parameter
Item {
    id: root
    property Parameter param: null
    property string label: ""

    implicitWidth: Math.max(combo.implicitWidth, labelText.implicitWidth)
    implicitHeight: (label ? labelText.height : 0) + combo.implicitHeight

    Text {
        id: labelText
        visible: root.label !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        color: Theme.textDim
        font.pixelSize: Theme.fontSmall
    }

    ComboBox {
        id: combo
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        implicitWidth: Math.min(Math.max(120, implicitContentWidth + 48), 240)
        font.pixelSize: Theme.fontNormal
        model: root.param ? root.param.valueLabels : []
        currentIndex: root.param ? root.param.value - root.param.lower : -1
        onActivated: (index) => {
            if (root.param)
                root.param.value = index + root.param.lower
        }
    }
}
