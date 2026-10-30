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
import Guitariqx

// Play head of a looper tape (GxPortDisplay with portdisplay:clip in
// gx_head): the position in the tape (0 to 1000) and the part played,
// between the cut at the start and the clip at the end (in percent)
Item {
    id: root
    property Parameter param: null
    property Parameter cut: null
    property Parameter clip: null
    readonly property real position: param ? Math.max(0, Math.min(1, param.value / Math.max(1, param.upper))) : 0
    readonly property real low: cut ? Math.max(0, Math.min(1, cut.value / 100)) : 0
    readonly property real high: clip ? Math.max(0, Math.min(1, clip.value / 100)) : 1

    implicitWidth: 180
    implicitHeight: 16

    OutputWatch {
        paramId: root.param ? root.param.id : ""
        active: root.visible
    }

    Rectangle {
        id: track
        anchors.fill: parent
        radius: 3
        color: Theme.knobTrack
        border.color: Theme.panelBorder
        clip: true

        // played part of the tape
        Rectangle {
            x: track.width * root.low
            width: Math.max(0, track.width * (root.high - root.low))
            height: parent.height
            color: Theme.panelRaised
        }
        Rectangle {
            x: track.width * root.low
            width: Math.max(0, track.width * (Math.min(root.position, root.high) - root.low))
            height: parent.height
            color: Theme.accentDim
        }
        Rectangle {
            x: Math.min(track.width - width, track.width * root.position)
            width: 2
            height: parent.height
            color: Theme.accent
        }
    }
}
