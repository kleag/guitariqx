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

// Last value (0..127) received from a MIDI controller, empty until the
// controller moves. The engine sends the values of the controllers that
// moved (midi_value_changed); for a "constant state" toggle it is the
// state of the switch.
Rectangle {
    id: root
    property int ctl: -1
    property int value: ctl >= 0 ? Gx.midi.controllerValue(ctl) : -1

    implicitHeight: 4
    radius: height / 2
    color: Theme.knobTrack

    onCtlChanged: value = ctl >= 0 ? Gx.midi.controllerValue(ctl) : -1

    Connections {
        target: Gx.midi
        function onControllerValueChanged(ctl, value) {
            if (ctl === root.ctl)
                root.value = value
        }
    }

    Rectangle {
        visible: root.value >= 0
        width: parent.width * Math.max(0, root.value) / 127
        height: parent.height
        radius: parent.radius
        color: Theme.accent
    }
}
