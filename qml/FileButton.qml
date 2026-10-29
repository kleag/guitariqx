import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Guitariqx

// Shows the file a unit uses and chooses another one: a model of the
// neural amp units, a looper track, or the impulse response of a
// convolver (paramId ending in ".convolver"). The arrows step through the
// other files of the same folder.
Item {
    id: root
    property string paramId: ""
    readonly property Parameter param: Gx.ready && paramId ? Gx.param(paramId) : null
    readonly property bool convolver: paramId.endsWith(".convolver")
    readonly property string unit: paramId.substring(0, paramId.indexOf("."))
    readonly property string kind: convolver ? "ir"
                                 : paramId.indexOf("rtneural") >= 0 ? "rtneural"
                                 : paramId.indexOf("nam.") >= 0 ? "nam"
                                 : paramId.indexOf("dubber") >= 0 ? "loop" : "file"
    readonly property var kinds: ({
        "ir": { suffixes: ["wav", "aif", "aiff", "caf", "flac", "ogg"], title: qsTr("Choose an impulse response"),
                filter: qsTr("Audio files") },
        "rtneural": { suffixes: ["json", "aidax"], title: qsTr("Choose an RTNeural model"),
                      filter: qsTr("RTNeural models (*.json, *.aidax)") },
        "nam": { suffixes: ["nam"], title: qsTr("Choose a NAM model"), filter: qsTr("NAM models (*.nam)") },
        "loop": { suffixes: ["wav"], title: qsTr("Choose a loop to import"), filter: qsTr("WAV files") },
        "file": { suffixes: [], title: qsTr("Choose a file"), filter: "" },
    })
    // full path of the current file
    readonly property string file: {
        if (!param)
            return ""
        if (convolver) {
            const v = param.value || {}
            const f = v["jconv.IRFile"] || ""
            const d = (Gx.filePlaces,Gx.resolveIrDir(v["jconv.IRDir"] || ""))
            return f === "" ? "" : (d === "" ? f : d + "/" + f)
        }
        return String(param.value || "")
    }
    readonly property bool absolute: file.startsWith("/")
    readonly property string folder: absolute ? siblings.dirName(file) : ""
    property string error: ""

    function load(path) {
        error = ""
        if (convolver)
            Gx.loadImpulseResponse(unit, path)
        else if (kind === "nam" || kind === "rtneural")
            Gx.loadModel(paramId, path)
        else
            param.value = path
    }

    // the file before or after the current one in its folder
    function step(delta) {
        if (siblings.rowCount() === 0)
            return
        let names = []
        for (let i = 0; i < siblings.rowCount(); ++i) {
            const idx = siblings.index(i, 0)
            if (!siblings.data(idx, DirModel.IsDirRole))
                names.push(siblings.data(idx, DirModel.PathRole))
        }
        if (names.length === 0)
            return
        let i = names.indexOf(file)
        i = i < 0 ? 0 : (i + delta + names.length) % names.length
        load(names[i])
    }

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight + (error ? errorText.implicitHeight : 0)
    visible: param !== null

    Connections {
        target: Gx
        function onImpulseResponseLoaded(unit, file, error) {
            if (unit === root.unit)
                root.error = error
        }
        function onModelLoaded(fileParam, file, error) {
            if (fileParam === root.paramId)
                root.error = error
        }
    }

    // files of the current folder, for the arrows
    DirModel {
        id: siblings
        suffixes: root.kinds[root.kind].suffixes
        Component.onCompleted: if (root.folder) path = root.folder
    }
    onFolderChanged: if (folder) siblings.path = folder

    RowLayout {
        id: row
        spacing: 2
        ToolButton {
            text: "◀"
            visible: root.absolute && root.kind !== "loop"
            onClicked: root.step(-1)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Previous file in the folder")
        }
        Button {
            Layout.maximumWidth: 320
            onClicked: chooser.choose(root.kinds[root.kind].title,
                                      root.folder || (Gx.param(root.unit + ".loadpath") ? Gx.param(root.unit + ".loadpath").value : ""))
            contentItem: Row {
                spacing: Theme.spacing
                FileIcon {
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, 260)
                    text: root.file ? siblings.fileName(root.file).replace(/\.[^.]*$/, "")
                                    : root.kind === "loop" ? qsTr("Import ...") : qsTr("Choose a file ...")
                    color: root.file ? Theme.text : Theme.textDim
                    font.pixelSize: Theme.fontNormal
                    elide: Text.ElideMiddle
                }
            }
            ToolTip.visible: hovered && root.file !== ""
            ToolTip.text: root.file
        }
        ToolButton {
            text: "▶"
            visible: root.absolute && root.kind !== "loop"
            onClicked: root.step(1)
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Next file in the folder")
        }
    }

    Text {
        id: errorText
        anchors.top: row.bottom
        visible: root.error !== ""
        text: root.error
        color: Theme.meterHigh
        font.pixelSize: Theme.fontSmall
    }

    // --debug-state files:UNIT
    readonly property bool debugOpen: Gx.ready && root.visible
                                      && Qt.application.arguments.join(" ").indexOf("files:" + root.unit) >= 0
    onDebugOpenChanged: if (debugOpen) chooser.choose(kinds[kind].title, folder || "/usr/share/gx_head/sounds")

    FileChooserDialog {
        id: chooser
        kind: root.kind
        suffixes: root.kinds[root.kind].suffixes
        filterName: root.kinds[root.kind].filter
        current: root.file
        onChosen: (path) => root.load(path)
    }
}
