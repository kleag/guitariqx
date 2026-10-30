import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Guitariqx

// Pick an effect to add to a rack
Dialog {
    id: root
    property bool stereo: false
    property var exclude: []
    // a unit was added, to be placed by hand (Gx.placeAddedUnits)
    signal placeRequested(string id)

    title: stereo ? qsTr("Add stereo effect") : qsTr("Add mono effect")
    modal: true
    // plain background: Material's elevation layer is costly on small GPUs
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }
    anchors.centerIn: Overlay.overlay
    width: Math.min(480, Overlay.overlay ? Overlay.overlay.width - 32 : 480)
    height: Math.min(640, Overlay.overlay ? Overlay.overlay.height - 32 : 640)
    standardButtons: Dialog.Close

    onOpened: {
        filter.text = ""
        filter.forceActiveFocus()
    }

    readonly property var candidates: {
        const f = filter.text.toLowerCase()
        return Gx.plugins.filter(p => p.stereo === root.stereo
                                 && root.exclude.indexOf(p.id) < 0
                                 && (f === "" || p.name.toLowerCase().indexOf(f) >= 0
                                     || p.category.toLowerCase().indexOf(f) >= 0))
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        TextField {
            id: filter
            Layout.fillWidth: true
            placeholderText: qsTr("Search")
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.candidates
            delegate: Column {
                id: entry
                required property var modelData
                required property int index
                width: ListView.view.width
                Text {
                    visible: entry.index === 0
                             || root.candidates[entry.index - 1].category !== entry.modelData.category
                    text: entry.modelData.category
                    color: Theme.accent
                    font.bold: true
                    font.pixelSize: Theme.fontNormal
                    topPadding: 8
                    bottomPadding: 2
                }
                ItemDelegate {
                    width: parent.width
                    text: entry.modelData.name
                    onClicked: {
                        Gx.addUnit(entry.modelData.id, root.stereo)
                        if (Gx.placeAddedUnits)
                            root.placeRequested(entry.modelData.id)
                        root.close()
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }

        // no options page yet: the option is set where it applies
        Switch {
            Layout.fillWidth: true
            text: qsTr("Then place it by hand")
            checked: Gx.placeAddedUnits
            onToggled: Gx.placeAddedUnits = checked
        }
    }
}
