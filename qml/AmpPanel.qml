import QtQuick
import QtQuick.Controls
import Guitariqx

// Main amplifier controls ("ampstack" has no UiBuilder layout, gx_head
// builds it from ampbox.glade)
Row {
    id: root
    spacing: Theme.spacing * 2

    function p(id) { return Gx.ready ? Gx.param(id) : null }

    Column {
        spacing: 4
        Text {
            text: qsTr("Tube model")
            color: Theme.textDim
            font.pixelSize: Theme.fontSmall
        }
        ParamSelector { param: root.p("tube.select") }
        Row {
            spacing: Theme.spacing
            ParamSwitch { param: root.p("amp.feed_on_off"); label: qsTr("Feedback") }
            ParamSwitch { param: root.p("amp.bass_boost.on_off"); label: qsTr("Bass boost") }
            ParamSwitch { param: root.p("con.on_off"); label: qsTr("Presence") }
        }
    }

    Knob { param: root.p("amp2.stage1.Pregain"); label: qsTr("Pre gain"); size: "mid" }
    Knob { param: root.p("gxdistortion.drive"); label: qsTr("Drive"); size: "mid" }
    Knob { param: root.p("gxdistortion.wet_dry"); label: qsTr("Distortion"); size: "mid" }
    Knob { param: root.p("amp2.stage2.gain1"); label: qsTr("Gain"); size: "big" }
    Knob { param: root.p("amp.wet_dry"); label: qsTr("Dry/Wet") }
    Knob { param: root.p("bassbooster.Level"); label: qsTr("Bass boost") }
    Knob { param: root.p("con.Level"); label: qsTr("Presence") }
    Knob { param: root.p("amp.out_master"); label: qsTr("Master"); size: "big" }

    Column {
        spacing: 4
        Text {
            text: qsTr("Out")
            color: Theme.textDim
            font.pixelSize: Theme.fontSmall
            anchors.horizontalCenter: parent.horizontalCenter
        }
        Row {
            spacing: 3
            LevelMeter { paramId: "maxlevel.left"; vertical: true; implicitHeight: 90; thickness: 10 }
            LevelMeter { paramId: "maxlevel.right"; vertical: true; implicitHeight: 90; thickness: 10 }
        }
    }
}
