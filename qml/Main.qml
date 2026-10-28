import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtHead

ApplicationWindow {
    id: window
    property bool startFullScreen: false
    // --debug-state midimap
    property bool openMidiMap: false
    // side by side racks and a docked preset list need room; small
    // touch screens (800x480) get tabs and a drawer
    readonly property bool wide: width >= 1100

    visible: true
    visibility: startFullScreen ? Window.FullScreen : Window.Windowed
    title: Gx.currentPreset ? qsTr("Guitarix - %1").arg(Gx.currentPreset) : qsTr("Guitarix")
    color: Theme.background

    Material.theme: Material.Dark
    Material.accent: Theme.accent
    Material.background: Theme.panel

    header: TopBar {
        compact: !window.wide
        onPresetsRequested: {
            if (window.wide)
                presetDock.visible = !presetDock.visible
            else
                presetDrawer.open()
        }
        onConnectRequested: connectDialog.open()
        onMidiMapRequested: midiMapDialog.open()
    }

    footer: Rectangle {
        implicitHeight: statusText.implicitHeight + 8
        color: Theme.panel
        Text {
            id: statusText
            anchors.verticalCenter: parent.verticalCenter
            x: Theme.spacing
            width: parent.width - 2 * Theme.spacing
            text: Gx.lastMessage && Gx.ready ? Gx.lastMessage : Gx.status
            color: Theme.textDim
            font.pixelSize: Theme.fontSmall
            elide: Text.ElideMiddle
        }
    }

    Drawer {
        id: presetDrawer
        width: Math.min(360, window.width * 0.8)
        height: window.height
        edge: Qt.LeftEdge
        PresetPanel {
            anchors.fill: parent
            onPresetSelected: presetDrawer.close()
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        PresetPanel {
            id: presetDock
            visible: window.wide
            Layout.preferredWidth: 280
            Layout.fillHeight: true
        }

        // wide: both racks side by side
        RowLayout {
            visible: window.wide
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.spacing
            spacing: Theme.spacing
            Rack {
                stereo: false
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 3
            }
            Rack {
                stereo: true
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 2
            }
        }

        // narrow: one rack at a time
        ColumnLayout {
            visible: !window.wide
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            TabBar {
                id: rackTabs
                Layout.fillWidth: true
                TabButton { text: qsTr("Mono") }
                TabButton { text: qsTr("Stereo") }
            }
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: Theme.spacing / 2
                currentIndex: rackTabs.currentIndex
                Rack { stereo: false }
                Rack { stereo: true }
            }
        }
    }

    ConnectDialog {
        id: connectDialog
    }

    TunerView {}

    MidiMapDialog {
        id: midiMapDialog
        readonly property bool debugOpen: window.openMidiMap && Gx.ready
        onDebugOpenChanged: if (debugOpen) open()
    }

    MidiLearnDialog {
        id: midiLearnDialog
    }

    // the learn dialog is open while the engine is in MIDI config mode
    Connections {
        target: Gx.midi
        function onLearnChanged() {
            if (Gx.midi.learning && !midiLearnDialog.visible)
                midiLearnDialog.open()
            else if (!Gx.midi.learning && midiLearnDialog.visible)
                midiLearnDialog.close()
        }
    }

    // the engine was not found: offer to change host/port
    Timer {
        interval: 6000
        running: !Gx.connected && !connectDialog.visible
        onTriggered: connectDialog.open()
    }
}
