import QtQuick
import QtQuick.Controls
import QtHead

// Mono or stereo effect rack
Item {
    id: root
    property bool stereo: false
    readonly property var units: stereo ? Gx.stereoRack : Gx.monoRack

    Item {
        id: title
        width: parent.width
        height: Theme.touch + 8

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            text: root.stereo ? qsTr("Stereo rack") : qsTr("Mono rack")
            color: Theme.textDim
            font.pixelSize: Theme.fontNormal
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.5
        }

        Button {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("+ Add effect")
            enabled: Gx.ready
            onClicked: addDialog.open()
        }
    }

    ListView {
        id: list
        anchors.top: title.bottom
        anchors.bottom: parent.bottom
        width: parent.width
        clip: true
        spacing: 6
        model: root.units
        boundsBehavior: Flickable.StopAtBounds
        // keep units alive while scrolling, they are cheap but layouts
        // are fetched from the engine
        cacheBuffer: 2000
        ScrollBar.vertical: ScrollBar {}

        delegate: RackUnit {
            required property string modelData
            required property int index
            width: ListView.view.width - 10
            unitId: modelData
            stereo: root.stereo
            first: index === 0
            last: index === root.units.length - 1
        }

        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: Gx.ready ? qsTr("No effects in this rack") : ""
            color: Theme.textDim
        }
    }

    AddUnitDialog {
        id: addDialog
        stereo: root.stereo
        exclude: root.units
    }
}
