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
