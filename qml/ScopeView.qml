import QtQuick
import QtQuick.Controls
import Guitariqx

// The oscilloscope unit (GxWaveView with connect_oscilloscope in
// gx_head): the last frames of the signal, polled while the unit is on and
// shown, with the buffer length (in jack periods) and a vertical zoom
Column {
    id: root
    property Parameter param: null
    readonly property Parameter onOff: Gx.ready ? Gx.param("oscilloscope.on_off") : null
    readonly property bool running: onOff ? !!onOff.value : false
    readonly property var scope: Gx.oscilloscope
    spacing: 4

    OutputWatch {
        paramId: root.param ? root.param.id : ""
        active: root.visible && root.running
    }

    WaveView {
        id: view
        width: 360
        height: 140
        oscilloscope: root.scope
        opacity: root.running ? 1 : 0.5
        backgroundColor: Theme.knobTrack
        gridColor: Theme.panelBorder
        waveColor: Theme.meterLow
    }

    Row {
        spacing: Theme.spacing
        anchors.horizontalCenter: parent.horizontalCenter

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Buffer")
            color: Theme.rackLabel
            font.pixelSize: Theme.fontSmall
        }
        RoundButton {
            text: "−"
            enabled: root.scope.mulBuffer > 1
            onClicked: root.scope.mulBuffer = root.scope.mulBuffer - 1
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            width: 56
            horizontalAlignment: Text.AlignHCenter
            // the buffer holds mulBuffer jack periods
            text: root.scope.jackBufferSize > 0 ? root.scope.size + "" : "×" + root.scope.mulBuffer
            color: Theme.skinned ? Theme.rackValue : Theme.text
            font.pixelSize: Theme.fontNormal
        }
        RoundButton {
            text: "+"
            enabled: root.scope.mulBuffer < 6
            onClicked: root.scope.mulBuffer = root.scope.mulBuffer + 1
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            leftPadding: Theme.spacing
            text: qsTr("Zoom")
            color: Theme.rackLabel
            font.pixelSize: Theme.fontSmall
        }
        RoundButton {
            text: "−"
            enabled: view.scale > 1
            onClicked: view.scale = Math.max(1, view.scale / 2)
        }
        Label {
            anchors.verticalCenter: parent.verticalCenter
            width: 40
            horizontalAlignment: Text.AlignHCenter
            text: "×" + view.scale
            color: Theme.skinned ? Theme.rackValue : Theme.text
            font.pixelSize: Theme.fontNormal
        }
        RoundButton {
            text: "+"
            enabled: view.scale < 64
            onClicked: view.scale = view.scale * 2
        }
    }

    // OscilloscopeInfo: jack load and state
    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: root.running && root.scope.jackBufferSize > 0
        text: qsTr("DSP load %1 % · %2 · period %3").arg(root.scope.load)
              .arg(root.scope.realtime ? qsTr("realtime") : qsTr("not realtime"))
              .arg(root.scope.jackBufferSize)
        color: Theme.rackLabel
        font.pixelSize: Theme.fontSmall
    }
}
