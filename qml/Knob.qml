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

// Rotary control for a float or int parameter.
// Drag vertically (hold Shift for fine steps), use the mouse wheel;
// double click resets to the default value.
// With a gx_head skin, the knob is the skin's image with the LED ring and
// indicator of libgxw's GxKnob drawn over it.
Item {
    id: root
    property Parameter param: null
    property string label: param ? param.name : ""
    property string size: "small"
    // skin image, default by size
    property string image: ""
    readonly property string knobImage: image !== "" ? image
                                      : size === "big" ? "bigknob" : size === "mid" ? "midknob" : "smallknobr"
    readonly property var skinKnob: Theme.skinned && Gx.skin.style.knobs
                                    ? Gx.skin.style.knobs[knobImage] : undefined
    readonly property bool skinned: skinKnob !== undefined && skinImage.status === Image.Ready
    readonly property real diameter: skinned ? skinImage.implicitWidth * Theme.skinScale : Theme.knobSize(size)
    readonly property real value: param ? param.normalized : 0
    property bool active: drag.pressed

    implicitWidth: Math.max(diameter + 8, labelText.implicitWidth)
    implicitHeight: labelText.height + dial.height + valueText.height + (skinned ? 6 : 4)
    opacity: param ? 1 : 0.4

    Text {
        id: labelText
        anchors.horizontalCenter: parent.horizontalCenter
        height: text === "" ? 0 : implicitHeight
        text: root.label
        color: Theme.rackLabel
        font.pixelSize: Theme.fontSmall
        elide: Text.ElideRight
        width: Math.min(implicitWidth, root.diameter + 24)
        horizontalAlignment: Text.AlignHCenter
    }

    Item {
        id: dial
        width: root.diameter
        height: root.skinned ? skinImage.implicitHeight * Theme.skinScale : root.diameter
        anchors.top: labelText.bottom
        anchors.topMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter

        // value arc, 270 degrees from 7:30 to 4:30
        Canvas {
            id: arc
            visible: !root.skinned
            anchors.fill: parent
            property real v: root.value
            onVChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const c = width / 2
                const r = c - 3
                const a0 = Math.PI * 0.75
                const a1 = a0 + Math.PI * 1.5
                ctx.lineWidth = 4
                ctx.lineCap = "round"
                ctx.strokeStyle = Theme.knobTrack
                ctx.beginPath()
                ctx.arc(c, c, r, a0, a1)
                ctx.stroke()
                ctx.strokeStyle = Theme.accent
                ctx.beginPath()
                ctx.arc(c, c, r, a0, a0 + Math.PI * 1.5 * v)
                ctx.stroke()
            }
        }

        Rectangle {
            id: face
            visible: !root.skinned
            anchors.centerIn: parent
            width: parent.width - 14
            height: width
            radius: width / 2
            color: Theme.knobFace
            border.color: root.active ? Theme.accent : "#55555e"
            border.width: 1
            rotation: -135 + 270 * root.value

            Rectangle {
                width: 3
                height: face.height * 0.32
                radius: 1.5
                color: Theme.text
                anchors.horizontalCenter: parent.horizontalCenter
                y: 4
            }
        }
    }

    Image {
        id: skinImage
        parent: dial
        anchors.fill: parent
        visible: root.skinned
        source: root.skinKnob !== undefined ? Theme.skinImage(root.knobImage) : ""
        smooth: true
        mipmap: true
    }

    // GxKnob: a ring from 40 to 320 degrees clockwise from 6 o'clock, dashed
    // when ring-led-size is set, and an indicator line; sizes in pixels of
    // the image, -1 meaning automatic
    Canvas {
        id: ring
        parent: dial
        anchors.fill: parent
        z: 1
        visible: root.skinned
        property real v: root.value
        property var k: root.skinKnob
        onVChanged: requestPaint()
        onKChanged: requestPaint()
        // not painted while hidden, until the image is loaded
        onVisibleChanged: if (visible) requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            if (!k)
                return
            const s = width / skinImage.implicitWidth
            const minrad = Math.min(skinImage.implicitWidth, skinImage.implicitHeight) / 2
            const cx = (k.xCenter >= 0 ? k.xCenter : skinImage.implicitWidth / 2) * s
            const cy = (k.yCenter >= 0 ? k.yCenter : skinImage.implicitHeight / 2) * s
            const rr = (k.ringRadius >= 0 ? k.ringRadius : minrad - k.ringRadius / 2) * s
            const ir = (k.indicatorRadius >= 0 ? k.indicatorRadius : minrad / 2) * s
            const zero = 40 * Math.PI / 180
            const angle = zero + v * 2 * (Math.PI - zero)
            const quarter = Math.PI / 2
            ctx.lineWidth = Math.max(1, (k.ringWidth || 1) * s)
            if (k.ledSize > 0)
                ctx.setLineDash([k.ledSize * s, (k.ledDistance || 0) * s])
            ctx.strokeStyle = Theme.skinColor("ringOff", Theme.knobTrack)
            ctx.beginPath()
            ctx.arc(cx, cy, rr, quarter + zero, quarter + 320 * Math.PI / 180)
            ctx.stroke()
            if (angle > zero) {
                ctx.strokeStyle = Theme.rackValue
                ctx.beginPath()
                ctx.arc(cx, cy, rr, quarter + zero, quarter + angle)
                ctx.stroke()
            }
            ctx.setLineDash([])
            ctx.lineWidth = Math.max(1, (k.indicatorWidth || 1) * s)
            const len = (k.indicatorLength || 0) * s
            ctx.beginPath()
            ctx.moveTo(cx - Math.sin(angle) * ir, cy + Math.cos(angle) * ir)
            ctx.lineTo(cx - Math.sin(angle) * (ir - len), cy + Math.cos(angle) * (ir - len))
            ctx.stroke()
        }
    }

    // the value under the knob; a gx_head style box with a skin
    Rectangle {
        anchors.fill: valueText
        anchors.leftMargin: -4
        anchors.rightMargin: -4
        visible: root.skinned && valueText.text !== ""
        color: Theme.rackValueBackground
        radius: 3
    }

    Text {
        id: valueText
        anchors.top: dial.bottom
        anchors.topMargin: root.skinned ? 2 : 0
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.param ? root.param.displayValue : ""
        color: root.active ? Theme.accent : root.skinned ? Theme.rackValue : Theme.text
        font.pixelSize: Theme.fontSmall
    }

    MouseArea {
        id: drag
        anchors.fill: dial
        // at least the touch size around the small knobs of skins
        anchors.margins: -Math.max(6, (Theme.touch - dial.width) / 2)
        preventStealing: true
        property real startY: 0
        property real startValue: 0
        onPressed: (mouse) => {
            startY = mouse.y
            startValue = root.value
        }
        onPositionChanged: (mouse) => {
            if (!root.param)
                return
            const range = (mouse.modifiers & Qt.ShiftModifier) ? 1000 : 200
            root.param.normalized = startValue + (startY - mouse.y) / range
        }
        onDoubleClicked: if (root.param) root.param.reset()
        onWheel: (wheel) => {
            if (!root.param)
                return
            const steps = wheel.angleDelta.y / 120
            const p = root.param
            const inc = (p.step > 0 && p.upper > p.lower) ? p.step / (p.upper - p.lower) : 0.01
            p.normalized = p.normalized + steps * Math.max(inc, 0.005)
        }
    }

    MidiLearnArea { paramId: root.param ? root.param.id : "" }
}
