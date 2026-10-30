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

// Folder or document icon, drawn (no icon theme or emoji font needed)
Item {
    id: root
    property bool folder: false
    property color color: folder ? Theme.accent : Theme.textDim
    implicitWidth: 22
    implicitHeight: 22

    // folder: tab and body
    Rectangle {
        visible: root.folder
        x: 1
        y: 4
        width: 9
        height: 4
        radius: 1.5
        color: root.color
    }
    Rectangle {
        visible: root.folder
        x: 1
        y: 6
        width: 20
        height: 13
        radius: 2
        color: root.color
    }

    // document: page with folded corner
    Rectangle {
        visible: !root.folder
        x: 4
        y: 2
        width: 14
        height: 18
        radius: 2
        color: "transparent"
        border.color: root.color
        border.width: 1.5
        Rectangle {
            x: 3
            y: 7
            width: 8
            height: 1.5
            color: root.color
        }
        Rectangle {
            x: 3
            y: 11
            width: 8
            height: 1.5
            color: root.color
        }
    }
}
