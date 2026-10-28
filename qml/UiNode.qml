import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtHead

// Renders a layout tree node (see uitree.h) recursively. Child boxes are
// created through a Loader because a component cannot contain itself.
Item {
    id: root
    property var node: ({})
    // available width, used by flow boxes to wrap
    property real maxWidth: 1000
    readonly property string type: node.type || ""
    readonly property var children_: node.children || []

    implicitWidth: content.item ? content.item.implicitWidth : 0
    implicitHeight: content.item ? content.item.implicitHeight : 0

    Component {
        id: childDelegate
        Loader {
            required property var modelData
            // used by grid boxes only
            Layout.row: modelData.row !== undefined ? modelData.row : 0
            Layout.column: modelData.column !== undefined ? modelData.column : 0
            Layout.rowSpan: modelData.rowSpan || 1
            Layout.columnSpan: modelData.columnSpan || 1
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Component.onCompleted: {
                if (modelData.type === "box")
                    setSource("UiNode.qml", { node: modelData, maxWidth: Qt.binding(() => root.maxWidth) })
                else if (modelData.type === "control")
                    setSource("Control.qml", { node: modelData })
                else if (modelData.type === "label")
                    setSource("RackLabel.qml", { text: modelData.text })
                else if (modelData.type === "spacer")
                    setSource("Spacer.qml")
            }
        }
    }

    // GtkGrid of Glade layouts
    Component {
        id: grid
        GridLayout {
            columnSpacing: Theme.spacing
            rowSpacing: 4
            Repeater { model: root.children_; delegate: childDelegate }
        }
    }

    Component {
        id: hbox
        Column {
            spacing: 2
            Text {
                visible: text !== ""
                text: root.node.label || ""
                color: Theme.accent
                font.pixelSize: Theme.fontSmall
                font.bold: true
            }
            Row {
                spacing: Theme.spacing
                Repeater { model: root.children_; delegate: childDelegate }
            }
        }
    }

    Component {
        id: vbox
        Column {
            spacing: 4
            Text {
                visible: text !== ""
                text: root.node.label || ""
                color: Theme.accent
                font.pixelSize: Theme.fontSmall
                font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Repeater { model: root.children_; delegate: childDelegate }
        }
    }

    Component {
        id: flow
        Flow {
            width: Math.min(root.maxWidth, implicitFlowWidth)
            spacing: Theme.spacing
            // Flow has no implicit width, estimate it from the children
            property real implicitFlowWidth: {
                let w = 0
                for (let i = 0; i < children.length; ++i)
                    w += children[i].width + spacing
                return Math.max(w, 1)
            }
            Repeater { model: root.children_; delegate: childDelegate }
        }
    }

    Component {
        id: frame
        Rectangle {
            color: "transparent"
            border.color: Theme.panelBorder
            radius: Theme.radius
            implicitWidth: inner.implicitWidth + 12
            implicitHeight: inner.implicitHeight + 12
            Column {
                id: inner
                x: 6
                y: 6
                spacing: 2
                Text {
                    visible: text !== ""
                    text: root.node.label || ""
                    color: Theme.accent
                    font.pixelSize: Theme.fontSmall
                    font.bold: true
                }
                Row {
                    spacing: Theme.spacing
                    Repeater { model: root.children_; delegate: childDelegate }
                }
            }
        }
    }

    Component {
        id: tabs
        Column {
            spacing: 4
            TabBar {
                id: bar
                Repeater {
                    model: root.children_
                    TabButton {
                        required property var modelData
                        required property int index
                        text: modelData.label || qsTr("Page %1").arg(index + 1)
                        width: implicitWidth
                    }
                }
            }
            StackLayout {
                currentIndex: bar.currentIndex
                implicitWidth: children.length > currentIndex && currentIndex >= 0
                               ? children[currentIndex].implicitWidth : 0
                implicitHeight: children.length > currentIndex && currentIndex >= 0
                                ? children[currentIndex].implicitHeight : 0
                Repeater { model: root.children_; delegate: childDelegate }
            }
        }
    }

    Loader {
        id: content
        sourceComponent: {
            if (root.type !== "box")
                return null
            if (root.node.tabs)
                return tabs
            if (root.node.frame)
                return frame
            if (root.node.orientation === "flow")
                return flow
            if (root.node.orientation === "grid")
                return grid
            return root.node.orientation === "v" ? vbox : hbox
        }
    }
}
