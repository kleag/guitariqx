/*
 * Copyright (C) 2026 Gaël de Chalendar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Guitariqx

// The LADSPA and LV2 plugins of the engine's computer: which ones are rack
// units, under which name and category (gx_head's "LADSPA/LV2 plugins"
// window). Edits are kept here until applied; the engine then loads the
// plugins again.
Dialog {
    id: root
    readonly property PluginManager pm: Gx.pluginManager
    // index of the plugin shown in the details page, -1 for the list
    property int current: -1
    property var info: ({})
    title: pm.modified ? qsTr("LADSPA/LV2 plugins (changed)") : qsTr("LADSPA/LV2 plugins")
    modal: true
    closePolicy: Popup.NoAutoClose
    // plain background: Material's elevation layer is costly on small GPUs
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }
    anchors.centerIn: Overlay.overlay
    width: Math.min(720, Overlay.overlay ? Overlay.overlay.width - 16 : 720)
    height: Overlay.overlay ? Overlay.overlay.height - 16 : 560
    padding: Theme.spacing

    // --debug-state plugins[:NAME]: the list, or the details of a plugin
    readonly property string debugPlugin: {
        const args = Qt.application.arguments
        const i = args.indexOf("--debug-state")
        if (i < 0 || i + 1 >= args.length)
            return ""
        const s = args[i + 1].split(",").find(s => s === "plugins" || s.startsWith("plugins:"))
        return s === undefined ? "" : s === "plugins" ? "*" : s.substring(8)
    }
    readonly property bool debugOpen: debugPlugin !== "" && Gx.ready
    onDebugOpenChanged: if (debugOpen) open()

    function showPlugin(index) {
        current = index
        info = index >= 0 ? pm.plugin(index) : {}
    }
    function requestClose() {
        if (pm.modified)
            confirm.ask(qsTr("Discard changes?"),
                        qsTr("The changes to the plugins have not been applied."),
                        qsTr("Discard"), () => root.close())
        else
            close()
    }

    onOpened: {
        showPlugin(-1)
        pm.load()
    }
    // the list is several MB: it is not kept
    onClosed: pm.unload()

    Connections {
        target: root.pm
        function onPluginChanged(index) {
            if (index === root.current)
                root.info = root.pm.plugin(index)
        }
        function onLoadedChanged() {
            if (!root.pm.loaded || root.debugPlugin === "" || root.debugPlugin === "*")
                return
            for (let i = 0; i < root.pm.total; ++i) {
                if (root.pm.plugin(i).name === root.debugPlugin) {
                    root.showPlugin(i)
                    return
                }
            }
        }
        function onLoadFailed() {
            failed.visible = true
        }
    }

    ConfirmDialog {
        id: confirm
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.current < 0 ? 0 : 1

        // the plugins
        ColumnLayout {
            spacing: Theme.spacing

            Flow {
                Layout.fillWidth: true
                spacing: Theme.spacing
                TextField {
                    width: 200
                    placeholderText: qsTr("Search")
                    text: root.pm.filter
                    onTextEdited: root.pm.filter = text
                    inputMethodHints: Qt.ImhNoPredictiveText
                }
                ComboBox {
                    width: 150
                    model: [qsTr("All"), qsTr("Selected"), qsTr("With settings"), qsTr("LADSPA"), qsTr("LV2")]
                    currentIndex: root.pm.view
                    onActivated: index => root.pm.view = index
                }
                ComboBox {
                    width: 190
                    model: [qsTr("Mono and stereo"), qsTr("Mono"), qsTr("Stereo")]
                    currentIndex: root.pm.channels
                    onActivated: index => root.pm.channels = index
                }
                ToolButton {
                    text: "⋮"
                    font.pixelSize: Theme.fontLarge
                    enabled: root.pm.loaded && !root.pm.busy
                    onClicked: listMenu.open()
                    Menu {
                        id: listMenu
                        y: parent.height
                        MenuItem {
                            text: qsTr("Select the plugins shown")
                            onTriggered: root.pm.setAllActive(true)
                        }
                        MenuItem {
                            text: qsTr("Deselect the plugins shown")
                            onTriggered: root.pm.setAllActive(false)
                        }
                        MenuItem {
                            text: qsTr("Discard changes")
                            enabled: root.pm.modified
                            onTriggered: root.pm.discard()
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                color: Theme.textDim
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                visible: root.pm.loaded
                text: qsTr("%1 shown of %2 plugins, %3 selected as rack units.")
                      .arg(list.count).arg(root.pm.total).arg(root.pm.activeCount)
                      + (root.pm.notFound.length > 0
                         ? " " + qsTr("Not installed any more: %1").arg(root.pm.notFound.join(", ")) : "")
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.pm
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: row
                    required property int index
                    required property var model
                    width: ListView.view.width
                    height: Theme.touch + 12
                    onClicked: root.showPlugin(model.index)
                    contentItem: RowLayout {
                        spacing: Theme.spacing
                        CheckBox {
                            checked: row.model.active
                            enabled: !root.pm.busy
                            onToggled: root.pm.setActive(row.model.index, checked)
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: row.model.name
                                color: row.model.changed ? Theme.accent : Theme.text
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                color: Theme.textDim
                                font.pixelSize: Theme.fontSmall
                                elide: Text.ElideRight
                                text: [row.model.lv2 ? "LV2" : "LADSPA",
                                       row.model.stereo ? qsTr("stereo") : qsTr("mono"),
                                       row.model.category, row.model.maker].filter(s => s !== "").join(" · ")
                            }
                        }
                        Label {
                            visible: row.model.hasSettings
                            text: "⚙"
                            color: Theme.textDim
                        }
                        Label {
                            text: "›"
                            font.pixelSize: Theme.fontLarge
                            color: Theme.textDim
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: root.pm.loaded && list.count === 0
                    text: qsTr("No plugin")
                    color: Theme.textDim
                }
                Label {
                    id: failed
                    anchors.centerIn: parent
                    visible: false
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: qsTr("The engine did not send its plugin list.")
                    color: Theme.textDim
                }
            }
        }

        // one plugin
        ColumnLayout {
            spacing: Theme.spacing

            RowLayout {
                Layout.fillWidth: true
                ToolButton {
                    text: "‹"
                    font.pixelSize: Theme.fontLarge
                    onClicked: root.showPlugin(-1)
                }
                Label {
                    Layout.fillWidth: true
                    text: root.info.name || ""
                    font.pixelSize: Theme.fontLarge
                    elide: Text.ElideRight
                }
            }

            Flickable {
                id: details
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentHeight: grid.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                GridLayout {
                    id: grid
                    width: details.width - 12
                    columns: 2
                    columnSpacing: Theme.spacing * 2
                    rowSpacing: Theme.spacing / 2
                    enabled: !root.pm.busy

                    Label {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        wrapMode: Text.WrapAnywhere
                        color: Theme.textDim
                        font.pixelSize: Theme.fontSmall
                        text: [(root.info.lv2 ? "LV2" : "LADSPA") + ", " + (root.info.stereo ? qsTr("stereo") : qsTr("mono")),
                               root.info.maker || "", root.info.ladspaCategory || "", root.info.source || ""]
                              .filter(s => s !== "").join("\n")
                    }

                    Label { text: qsTr("Rack unit") }
                    Switch {
                        checked: !!root.info.active
                        onToggled: root.pm.setActive(root.current, checked)
                    }

                    Label { text: qsTr("Name") }
                    TextField {
                        Layout.fillWidth: true
                        text: root.info.shortname || ""
                        placeholderText: root.info.name || ""
                        color: text !== root.info.name ? Theme.accent : Theme.text
                        onEditingFinished: if (text !== root.info.shortname) root.pm.setPlugin(root.current, {shortname: text})
                    }

                    Label { text: qsTr("Category") }
                    ComboBox {
                        Layout.fillWidth: true
                        readonly property var values: {
                            const l = root.pm.categories.slice()
                            if (root.info.category && l.indexOf(root.info.category) < 0)
                                l.push(root.info.category)
                            return l
                        }
                        model: values
                        currentIndex: values.indexOf(root.info.category)
                        onActivated: index => root.pm.setPlugin(root.current, {category: values[index]})
                    }

                    Label { text: qsTr("Quirks") }
                    ComboBox {
                        Layout.fillWidth: true
                        readonly property var values: [0, PluginManager.NeedActivate, PluginManager.NoCleanup]
                        model: [qsTr("None"), qsTr("Activate again"), qsTr("No cleanup")]
                        currentIndex: values.indexOf(root.info.quirks)
                        onActivated: index => root.pm.setPlugin(root.current, {quirks: values[index]})
                    }

                    Label { text: qsTr("Master slider") }
                    ComboBox {
                        id: master
                        Layout.fillWidth: true
                        readonly property var ports: root.info.ports || []
                        model: [qsTr("None")].concat(ports.map(p => p.name))
                        currentIndex: root.info.masterIdx < 0 ? 0 : ports.findIndex(p => p.pos === root.info.masterIdx) + 1
                        onActivated: index => {
                            const p = index > 0 ? ports[index - 1] : null
                            root.pm.setPlugin(root.current, p ? {masterIdx: p.pos, masterLabel: p.factoryName}
                                                              : {masterIdx: -1})
                        }
                    }

                    Label {
                        text: qsTr("Master label")
                        visible: root.info.masterIdx >= 0
                    }
                    TextField {
                        Layout.fillWidth: true
                        visible: root.info.masterIdx >= 0
                        text: root.info.masterLabel || ""
                        onEditingFinished: if (text !== root.info.masterLabel) root.pm.setPlugin(root.current, {masterLabel: text})
                    }

                    Label { text: qsTr("Dry/wet control") }
                    Switch {
                        checked: !!root.info.addWetDry
                        onToggled: root.pm.setPlugin(root.current, {addWetDry: checked})
                    }

                    Label {
                        text: qsTr("Stereo to mono")
                        visible: !!root.info.stereo
                    }
                    Switch {
                        visible: !!root.info.stereo
                        checked: !!root.info.stereoToMono
                        onToggled: root.pm.setPlugin(root.current, {stereoToMono: checked})
                    }

                    Button {
                        Layout.columnSpan: 2
                        text: qsTr("Factory settings")
                        enabled: !!root.info.hasSettings || !!root.info.addWetDry
                        onClicked: root.pm.resetPlugin(root.current)
                    }
                }
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: root.pm.busy
        visible: running
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Apply")
            enabled: root.pm.modified && !root.pm.busy
            onClicked: root.pm.apply()
        }
        Button {
            text: qsTr("Close")
            flat: true
            enabled: !root.pm.busy
            onClicked: root.requestClose()
        }
    }
}
