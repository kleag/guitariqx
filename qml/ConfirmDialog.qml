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

// Confirmation for destructive actions
Dialog {
    id: root
    property string text: ""
    property string actionText: qsTr("Delete")
    property var callback: null

    function ask(title_, text_, actionText_, callback_) {
        title = title_
        text = text_
        actionText = actionText_
        callback = callback_
        open()
    }

    modal: true
    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius * 2
        border.color: Theme.panelBorder
    }
    anchors.centerIn: Overlay.overlay
    width: Math.min(420, Overlay.overlay ? Overlay.overlay.width - 24 : 420)

    Label {
        width: parent.width
        text: root.text
        wrapMode: Text.WordWrap
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("Cancel")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: root.actionText
            Material.background: Theme.ledOff
            onClicked: {
                root.close()
                if (root.callback)
                    root.callback()
            }
        }
    }
}
