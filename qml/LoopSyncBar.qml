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
import Guitariqx

// Under the looper's panel: keeps its tapes in time with the drum
// sequencer (Gx.loopSync)
Flow {
    id: root
    property real maxWidth: 600
    width: maxWidth
    spacing: Theme.spacing

    Switch {
        id: toggle
        text: qsTr("In time with the drums")
        checked: Gx.loopSync.enabled
        onToggled: Gx.loopSync.enabled = checked
        // the label in the skin's color
        contentItem: Text {
            leftPadding: toggle.indicator.width + toggle.spacing
            verticalAlignment: Text.AlignVCenter
            text: toggle.text
            color: Theme.rackLabel
            font.pixelSize: Theme.fontNormal
        }
    }
    Text {
        width: Math.min(implicitWidth, root.maxWidth)
        height: Math.max(toggle.height, implicitHeight)
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        color: Theme.rackLabel
        font.pixelSize: Theme.fontSmall
        visible: Gx.loopSync.enabled
        text: !Gx.loopSync.drumsRunning ? qsTr("Add the drum sequencer to the rack and switch it on")
            : Gx.loopSync.status !== "" ? Gx.loopSync.status
            : qsTr("Loops are cut to whole beats")
    }
}
