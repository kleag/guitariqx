pragma Singleton
import QtQuick
import Guitariqx

// Colors and sizes shared by all guitariqx views
QtObject {
    readonly property color background: "#17171a"
    readonly property color panel: "#232328"
    readonly property color panelRaised: "#2d2d33"
    readonly property color panelBorder: "#3a3a42"
    readonly property color text: "#e8e6e3"
    readonly property color textDim: "#9a978f"
    readonly property color accent: "#e8a33d"
    readonly property color accentDim: "#7a5a2a"
    readonly property color ledOn: "#ff5a36"
    readonly property color ledOff: "#4a2a24"
    readonly property color meterLow: "#4caf50"
    readonly property color meterMid: "#e8c33d"
    readonly property color meterHigh: "#e5483d"
    readonly property color knobFace: "#3b3b42"
    readonly property color knobTrack: "#16161a"

    // gx_head skin (Gx.skin): images and colors of the rack units
    readonly property bool skinned: Gx.skin.active
    // gx_head's images are drawn at this scale, which gives the knob sizes
    // of the flat look
    readonly property real skinScale: 0.8
    // URL of a libgxw icon of the skin, "" without; reading Gx.skin.name
    // makes bindings follow skin changes
    function skinImage(icon) {
        return Gx.skin.name !== "" ? Gx.skin.image(icon) : ""
    }
    function skinColor(key, fallback) {
        const c = skinned ? Gx.skin.style[key] : undefined
        return c !== undefined ? c : fallback
    }
    // texts drawn on a rack unit
    readonly property color rackLabel: skinColor("label", textDim)
    readonly property color rackTitle: skinColor("title", text)
    readonly property color rackFrameLabel: skinColor("label", accent)
    readonly property color rackValue: skinColor("value", text)
    readonly property color rackValueBackground: skinColor("valueBackground", "transparent")

    // touch friendly minimum size of interactive items
    readonly property int touch: 40
    readonly property int spacing: 8
    readonly property int radius: 6
    readonly property int fontSmall: 11
    readonly property int fontNormal: 13
    readonly property int fontLarge: 17

    function knobSize(size) {
        return size === "big" ? 88 : size === "mid" ? 68 : 54
    }
}
