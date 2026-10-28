import QtQuick
import QtQuick.Controls
import QtHead

// Preset banks with their presets; tap a preset to load it
Rectangle {
    id: root
    signal presetSelected()
    color: Theme.panel

    // banks the user expanded, by name; the current bank is always open
    property var openBanks: ({})

    function isOpen(name) {
        return name === Gx.currentBank || !!openBanks[name]
    }
    function toggle(name) {
        const o = Object.assign({}, openBanks)
        o[name] = !isOpen(name)
        openBanks = o
    }

    ListView {
        id: banks
        anchors.fill: parent
        anchors.margins: 4
        clip: true
        model: Gx.banks
        spacing: 2
        ScrollBar.vertical: ScrollBar {}

        delegate: Column {
            id: bank
            required property var modelData
            readonly property bool open: root.isOpen(modelData.name)
            width: ListView.view.width

            ItemDelegate {
                width: parent.width
                height: Theme.touch + 4
                contentItem: Row {
                    spacing: Theme.spacing
                    Text {
                        text: bank.open ? "▾" : "▸"
                        color: Theme.textDim
                        font.pixelSize: Theme.fontNormal
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: bank.modelData.name
                        color: bank.modelData.name === Gx.currentBank ? Theme.accent : Theme.text
                        font.pixelSize: Theme.fontNormal
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: bank.modelData.type === "factory" ? qsTr("factory") : ""
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                onClicked: root.toggle(bank.modelData.name)
            }

            Repeater {
                model: bank.open ? bank.modelData.presets : []
                ItemDelegate {
                    required property string modelData
                    readonly property bool current: bank.modelData.name === Gx.currentBank
                                                    && modelData === Gx.currentPreset
                    width: bank.width
                    height: Theme.touch + 4
                    leftPadding: 32
                    highlighted: current
                    text: modelData
                    font.pixelSize: Theme.fontNormal
                    onClicked: {
                        Gx.setPreset(bank.modelData.name, modelData)
                        root.presetSelected()
                    }
                }
            }
        }
    }
}
