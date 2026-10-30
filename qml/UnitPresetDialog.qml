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
import QtQuick.Layouts
import Guitariqx

// Presets of one rack unit (gx_head's PluginPresetPopup): the unit's
// standard values, the user's presets (which can be removed) and the
// factory presets, the one in use checked. Tapping one loads it.
Dialog {
    id: root
    property string unitId: ""
    // {name, factory, active} as sent by Gx.unitPresetsReady
    property var presets: []
    property bool loading: false
    property bool standard: false
    readonly property var userPresets: presets.filter(p => !p.factory)

    function show() {
        presets = []
        loading = true
        standard = Gx.unitHasStdValues(unitId)
        Gx.requestUnitPresets(unitId)
        open()
    }
    function load(entry) {
        Gx.setUnitPreset(unitId, entry.name, entry.factory)
        close()
    }
    function save(name) {
        const n = name.trim().replace(/\s+/g, " ")
        if (userPresets.some(p => p.name === n)) {
            confirmDialog.ask(qsTr("Replace preset"), qsTr("Replace the preset \"%1\"?").arg(n), qsTr("Replace"),
                              () => Gx.saveUnitPreset(root.unitId, n))
            return ""
        }
        return Gx.saveUnitPreset(unitId, n)
    }

    Connections {
        target: Gx
        function onUnitPresetsReady(unit, list) {
            if (unit !== root.unitId)
                return
            root.presets = list
            root.loading = false
            root.standard = Gx.unitHasStdValues(root.unitId)
        }
    }

    title: qsTr("%1 presets").arg(Gx.pluginInfo(unitId).name || unitId)
    modal: true
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }
    anchors.centerIn: Overlay.overlay
    width: Math.min(420, Overlay.overlay ? Overlay.overlay.width - 32 : 420)
    height: Math.min(560, Overlay.overlay ? Overlay.overlay.height - 32 : 560)

    TextInputDialog { id: textDialog }
    ConfirmDialog { id: confirmDialog }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        model: root.presets
        header: ItemDelegate {
            width: ListView.view.width
            text: (root.standard ? "✓  " : "     ") + qsTr("Default settings")
            onClicked: {
                Gx.resetUnit(root.unitId)
                root.close()
            }
        }
        delegate: Column {
            id: entry
            required property var modelData
            required property int index
            width: ListView.view.width
            Text {
                visible: entry.index === 0 || root.presets[entry.index - 1].factory !== entry.modelData.factory
                text: entry.modelData.factory ? qsTr("Factory presets") : qsTr("Your presets")
                color: Theme.accent
                font.bold: true
                font.pixelSize: Theme.fontNormal
                topPadding: 8
                bottomPadding: 2
            }
            RowLayout {
                width: parent.width
                spacing: 0
                ItemDelegate {
                    Layout.fillWidth: true
                    text: (entry.modelData.active ? "✓  " : "     ") + entry.modelData.name
                    onClicked: root.load(entry.modelData)
                }
                ToolButton {
                    visible: !entry.modelData.factory
                    text: "✕"
                    onClicked: confirmDialog.ask(qsTr("Remove preset"),
                                                 qsTr("Remove the preset \"%1\"?").arg(entry.modelData.name),
                                                 qsTr("Remove"),
                                                 () => Gx.removeUnitPreset(root.unitId, entry.modelData.name))
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Remove")
                }
            }
        }
        ScrollBar.vertical: ScrollBar {}

        BusyIndicator {
            anchors.centerIn: parent
            running: root.loading
            visible: running
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Save…")
            flat: true
            onClicked: textDialog.ask(qsTr("Save unit preset"), qsTr("Preset name"), "", qsTr("Save"),
                                      (name) => root.save(name))
        }
        Button {
            text: qsTr("Close")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
