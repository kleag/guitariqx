import QtQuick
import QtQuick.Controls
import Guitariqx

// Combo box for an enum parameter
Item {
    id: root
    property Parameter param: null
    property string label: ""
    property color labelColor: Theme.textDim
    // dark box behind the combo box, for skin backgrounds
    property bool solid: false

    implicitWidth: Math.max(combo.implicitWidth, labelText.implicitWidth)
    implicitHeight: (label ? labelText.height : 0) + combo.implicitHeight

    Text {
        id: labelText
        visible: root.label !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        color: root.labelColor
        font.pixelSize: Theme.fontSmall
    }

    Rectangle {
        visible: root.solid
        anchors.fill: combo
        anchors.topMargin: combo.topInset
        anchors.bottomMargin: combo.bottomInset
        anchors.leftMargin: combo.leftInset
        anchors.rightMargin: combo.rightInset
        radius: 4
        color: "#e0141414"
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

    MidiLearnArea { paramId: root.param ? root.param.id : "" }
}
