import QtQuick
import Guitariqx

// Polls an engine parameter (Gx.watchOutput) while active: outputs the
// engine changes without notifying, like the looper position
QtObject {
    id: root
    property string paramId: ""
    property bool active: true
    readonly property string wanted: active && Gx.ready ? paramId : ""
    property string watched: ""

    function sync() {
        if (wanted === watched)
            return
        if (watched !== "")
            Gx.unwatchOutput(watched)
        watched = wanted
        if (watched !== "")
            Gx.watchOutput(watched)
    }
    onWantedChanged: sync()
    Component.onCompleted: sync()
    Component.onDestruction: if (watched !== "") Gx.unwatchOutput(watched)
}
