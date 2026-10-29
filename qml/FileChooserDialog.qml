import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Guitariqx

// File chooser in the spirit of KDE's single click mode: one click on a
// folder opens it, one click on a file chooses it. Places on the left,
// clickable path on top. Files are those of the engine's computer, or of
// this one with local. In save mode a click on a file takes its name, and
// the file is written with the Save button.
Dialog {
    id: root
    // file name suffixes of the files offered, e.g. ["nam"]
    property alias suffixes: dir.suffixes
    // remembers recent folders per kind
    property alias kind: dir.kind
    property alias local: dir.local
    property string filterName: ""
    // the file currently in use, highlighted
    property string current: ""
    property bool saveMode: false
    // appended to a file name without suffix in save mode, e.g. "gx"
    property string defaultSuffix: ""
    readonly property string saveName: {
        const n = nameField.text.trim()
        if (n === "" || defaultSuffix === "" || n.indexOf(".") > 0)
            return n
        return n + "." + defaultSuffix
    }
    readonly property bool saveExists: saveMode && saveName !== "" && (dir.count, dir.contains(saveName))
    property bool confirmReplace: false
    signal chosen(string path)

    function choose(title_, startPath, name) {
        title = title_
        dir.showAll = false
        dir.openAt(startPath || (dir.places.length ? dir.places[0].path : "/"))
        nameField.text = name || ""
        pathField.text = startPath ? startPath.replace(/\/*$/, "/") : ""
        confirmReplace = false
        open()
        if (saveMode)
            nameField.forceActiveFocus()
    }

    function select(path, isDir) {
        if (isDir) {
            dir.path = path
            return
        }
        if (saveMode) {
            nameField.text = dir.fileName(path)
            return
        }
        finish(path)
    }

    function save() {
        if (saveName === "")
            return
        // replacing a file needs a second click
        if (saveExists && !confirmReplace) {
            confirmReplace = true
            return
        }
        finish(dir.path === "/" ? "/" + saveName : dir.path + "/" + saveName)
    }

    function finish(path) {
        dir.addRecent(path)
        close()
        chosen(path)
    }

    readonly property bool wide: width >= 640

    modal: true
    padding: 0
    topPadding: 0
    anchors.centerIn: Overlay.overlay
    width: Overlay.overlay ? Math.min(960, Overlay.overlay.width - 16) : 800
    height: Overlay.overlay ? Math.min(680, Overlay.overlay.height - 16) : 480
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }

    DirModel {
        id: dir
    }

    header: ColumnLayout {
        spacing: 0
        Label {
            Layout.fillWidth: true
            Layout.margins: Theme.spacing * 1.5
            Layout.bottomMargin: Theme.spacing / 2
            text: root.title
            font.pixelSize: Theme.fontLarge
            font.bold: true
            elide: Text.ElideRight
        }
        // navigation: up, places (narrow screens), path
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spacing
            Layout.rightMargin: Theme.spacing
            spacing: 2
            ToolButton {
                text: "↑"
                font.pixelSize: Theme.fontLarge
                enabled: dir.canGoUp
                onClicked: dir.cdUp()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Parent folder")
            }
            ComboBox {
                visible: !root.wide
                Layout.preferredWidth: 150
                model: dir.places
                textRole: "name"
                displayText: qsTr("Places")
                onActivated: (i) => dir.path = dir.places[i].path
            }
            // breadcrumb
            ListView {
                id: crumbs
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.touch
                orientation: ListView.Horizontal
                clip: true
                spacing: 0
                boundsBehavior: Flickable.StopAtBounds
                model: dir.pathParts.length + 1
                onCountChanged: positionViewAtEnd()
                Component.onCompleted: positionViewAtEnd()
                delegate: Row {
                    id: crumb
                    required property int index
                    readonly property bool last: index === crumbs.count - 1
                    height: crumbs.height
                    Text {
                        visible: crumb.index > 1
                        anchors.verticalCenter: parent.verticalCenter
                        text: "›"
                        color: Theme.textDim
                        font.pixelSize: Theme.fontLarge
                    }
                    ToolButton {
                        height: parent.height
                        text: crumb.index === 0 ? "/" : dir.pathParts[crumb.index - 1]
                        font.bold: crumb.last
                        font.pixelSize: Theme.fontNormal
                        onClicked: dir.path = dir.partPath(crumb.index)
                    }
                }
            }
            ToolButton {
                text: "↻"
                font.pixelSize: Theme.fontLarge
                onClicked: dir.refresh()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Reload")
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.panelBorder
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // places
        ListView {
            id: places
            visible: root.wide
            Layout.preferredWidth: 210
            Layout.fillHeight: true
            clip: true
            model: dir.places
            delegate: Column {
                id: place
                required property var modelData
                required property int index
                width: ListView.view.width
                Text {
                    visible: place.index === 0 || dir.places[place.index - 1].section !== place.modelData.section
                    text: place.modelData.section
                    color: Theme.textDim
                    font.pixelSize: Theme.fontSmall
                    font.capitalization: Font.AllUppercase
                    leftPadding: Theme.spacing
                    topPadding: Theme.spacing
                    bottomPadding: 2
                }
                ItemDelegate {
                    width: parent.width
                    height: Theme.touch
                    highlighted: dir.path === place.modelData.path
                    onClicked: dir.path = place.modelData.path
                    contentItem: Row {
                        spacing: Theme.spacing
                        FileIcon {
                            folder: true
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: places.width - 60
                            text: place.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.fontNormal
                            elide: Text.ElideRight
                        }
                    }
                    ToolTip.visible: hovered
                    ToolTip.delay: 600
                    ToolTip.text: place.modelData.path
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
        Rectangle {
            visible: root.wide
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Theme.panelBorder
        }

        // entries
        ListView {
            id: files
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: dir
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            focus: true
            Keys.onPressed: (event) => {
                if (event.key === Qt.Key_Backspace) {
                    dir.cdUp()
                    event.accepted = true
                }
            }

            delegate: ItemDelegate {
                id: entry
                required property string name
                required property string path
                required property bool isDir
                required property var size
                required property var modified
                width: ListView.view.width
                height: Theme.touch + 4
                highlighted: path === root.current
                // single click, like KDE
                onClicked: root.select(path, isDir)
                contentItem: RowLayout {
                    spacing: Theme.spacing
                    FileIcon { folder: entry.isDir }
                    Text {
                        Layout.fillWidth: true
                        text: entry.name
                        color: entry.highlighted ? Theme.accent : Theme.text
                        font.pixelSize: Theme.fontNormal
                        elide: Text.ElideMiddle
                    }
                    Text {
                        visible: !entry.isDir
                        text: dir.formatSize(entry.size)
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                    }
                    Text {
                        visible: root.wide
                        Layout.preferredWidth: 130
                        horizontalAlignment: Text.AlignRight
                        text: entry.modified ? Qt.formatDateTime(entry.modified, "yyyy-MM-dd hh:mm") : ""
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.spacing
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                visible: files.count === 0 && !dir.busy
                text: dir.error !== "" ? dir.error : qsTr("No matching files in this folder")
                color: Theme.textDim
            }
            BusyIndicator {
                anchors.centerIn: parent
                running: dir.busy
                visible: running
            }
        }
    }

    footer: ColumnLayout {
    spacing: 0
    // save mode: name of the file to write
    RowLayout {
        visible: root.saveMode
        Layout.fillWidth: true
        Layout.leftMargin: Theme.spacing * 1.5
        Layout.rightMargin: Theme.spacing * 1.5
        Layout.topMargin: Theme.spacing
        spacing: Theme.spacing
        Label { text: qsTr("Name") }
        TextField {
            id: nameField
            Layout.fillWidth: true
            onTextEdited: root.confirmReplace = false
            onAccepted: root.save()
        }
        Button {
            text: root.saveExists ? (root.confirmReplace ? qsTr("Replace") : qsTr("Save ...")) : qsTr("Save")
            highlighted: true
            enabled: root.saveName !== ""
            onClicked: root.save()
        }
    }
    // an engine on another computer that cannot list its folders
    // (interface < 1.2): the path is typed
    RowLayout {
        visible: !root.saveMode && !dir.available
        Layout.fillWidth: true
        Layout.leftMargin: Theme.spacing * 1.5
        Layout.rightMargin: Theme.spacing * 1.5
        Layout.topMargin: Theme.spacing
        spacing: Theme.spacing
        Label { text: qsTr("Path") }
        TextField {
            id: pathField
            Layout.fillWidth: true
            placeholderText: qsTr("File on the engine's computer")
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            onAccepted: if (text.trim() !== "") root.finish(text.trim())
        }
        Button {
            text: qsTr("Open")
            highlighted: true
            enabled: pathField.text.trim() !== ""
            onClicked: root.finish(pathField.text.trim())
        }
    }
    Label {
        visible: root.saveMode && root.saveExists
        Layout.leftMargin: Theme.spacing * 1.5
        text: root.confirmReplace ? qsTr("\"%1\" exists: click Replace to overwrite it").arg(root.saveName)
                                  : qsTr("\"%1\" exists").arg(root.saveName)
        color: root.confirmReplace ? Theme.meterHigh : Theme.textDim
        font.pixelSize: Theme.fontSmall
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacing
        Item { Layout.preferredWidth: Theme.spacing }
        Label {
            visible: root.filterName !== "" && !dir.showAll
            text: root.filterName
            color: Theme.textDim
            font.pixelSize: Theme.fontSmall
        }
        CheckBox {
            text: qsTr("All files")
            checked: dir.showAll
            onToggled: dir.showAll = checked
        }
        CheckBox {
            text: qsTr("Hidden")
            checked: dir.showHidden
            onToggled: dir.showHidden = checked
        }
        Item { Layout.fillWidth: true }
        Button {
            text: qsTr("Cancel")
            flat: true
            onClicked: root.close()
        }
        Item { Layout.preferredWidth: Theme.spacing }
    }
    }
}
