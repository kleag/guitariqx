import QtQuick
import Guitariqx

// Level meter for an output parameter. The engine is polled for the
// value while the meter exists. Levels are shown on a dB scale.
Item {
    id: root
    property string paramId: ""
    property Parameter param: paramId && Gx.ready ? Gx.param(paramId) : null
    property bool vertical: false
    property real minDb: -60
    property real maxDb: 4
    property real thickness: 8
    property bool watching: false

    readonly property real level: {
        if (!param)
            return 0
        const v = Number(param.value)
        // meters flagged [log] report a linear amplitude; their range is
        // the dB range to display (e.g. -70..+5)
        if (param.logDisplay || param.upper === 1) {
            if (v <= 0)
                return 0
            const db = 20 * Math.log(v) / Math.LN10
            const lo = param.lower < 0 ? param.lower : minDb
            const hi = param.lower < 0 ? param.upper : maxDb
            return Math.max(0, Math.min(1, (db - lo) / (hi - lo)))
        }
        return param.normalized
    }
    // simple peak hold with decay
    property real peak: 0
    onLevelChanged: if (level > peak) peak = level

    implicitWidth: vertical ? thickness : 120
    implicitHeight: vertical ? 120 : thickness

    function updateWatch() {
        const want = !!param
        if (want === watching)
            return
        if (want)
            Gx.watchOutput(paramId)
        else
            Gx.unwatchOutput(paramId)
        watching = want
    }
    onParamChanged: updateWatch()
    Component.onCompleted: updateWatch()
    Component.onDestruction: if (watching) Gx.unwatchOutput(paramId)

    Timer {
        interval: 50
        running: root.peak > 0
        repeat: true
        onTriggered: root.peak = Math.max(root.level, root.peak - 0.01)
    }

    Rectangle {
        anchors.fill: parent
        radius: 2
        color: Theme.knobTrack
    }

    Rectangle {
        radius: 2
        color: root.level > 0.94 ? Theme.meterHigh : root.level > 0.8 ? Theme.meterMid : Theme.meterLow
        x: 0
        y: root.vertical ? parent.height * (1 - root.level) : 0
        width: root.vertical ? parent.width : parent.width * root.level
        height: root.vertical ? parent.height * root.level : parent.height
    }

    Rectangle {
        visible: root.peak > 0.01
        color: Theme.text
        x: root.vertical ? 0 : parent.width * root.peak - 1
        y: root.vertical ? parent.height * (1 - root.peak) : 0
        width: root.vertical ? parent.width : 2
        height: root.vertical ? 2 : parent.height
    }
}
