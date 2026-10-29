#!/usr/bin/env python3
# Copyright (C) 2026 Gaël de Chalendar
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

"""Record the answers of a real guitarix engine for the FakeEngine of the tests.

Run it against an engine with a fresh configuration (factory presets, default
rack), never against one in use:

    mkdir -p /tmp/gx/.config
    HOME=/tmp/gx XDG_CONFIG_HOME=/tmp/gx/.config \\
        pw-jack guitarix -N -J -K -M -n fixtures -p 7123 &
    tests/fixtures/capture.py --port 7123 > tests/fixtures/engine.json

It changes the engine's state first: loads a factory preset, adds a
convolver and a NAM unit to the rack and assigns a MIDI controller.
The parameter list is reduced to the units whose layout is recorded (those of
the rack and a few more) and the parameters that are not those of a unit
(system, ui, the amplifier, ...).
"""

import argparse
import json
import socket
import sys
import time

# units whose plugin_load_ui answers are recorded, besides those of the rack:
# the convolvers and neural units have file controls
EXTRA_UNITS = ["jconv_mono", "jconv", "nam", "rtneural"]
# plugin ids whose parameters the UI uses outside of a rack unit
# (AmpPanel.qml, TopBar.qml)
KEEP_GROUPS = {"amp", "con", "maxlevel"}
PRESET = ["Musiclab", "Distortion - Iron Man"]
MIDI_CONTROL = (7, "amp2.stage1.Pregain")
# the user's folder of the recording engine is replaced by this one
HOME = "/home/guitarist"

UI_FORM_STACK = 0x01
UI_FORM_GLADE = 0x02


class Engine:
    def __init__(self, host, port):
        self.sock = socket.create_connection((host, port))
        self.buf = b""
        self.next_id = 1

    def call(self, method, params=None):
        req = {"jsonrpc": "2.0", "method": method, "params": params or [], "id": self.next_id}
        self.next_id += 1
        self.sock.sendall(json.dumps(req, separators=(",", ":")).encode() + b"\n")
        while True:
            line = self.readline()
            msg = json.loads(line)
            # the engine returns the id as a string
            if str(msg.get("id")) == str(req["id"]):
                if "error" in msg:
                    raise RuntimeError("%s: %s" % (method, msg["error"]))
                return msg["result"]

    def notify(self, method, params=None):
        req = {"jsonrpc": "2.0", "method": method, "params": params or []}
        self.sock.sendall(json.dumps(req, separators=(",", ":")).encode() + b"\n")

    def readline(self):
        while b"\n" not in self.buf:
            data = self.sock.recv(1 << 16)
            if not data:
                raise EOFError("engine closed the connection")
            self.buf += data
        line, self.buf = self.buf.split(b"\n", 1)
        return line


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="localhost")
    ap.add_argument("--port", type=int, default=7123)
    args = ap.parse_args()
    if args.port == 7000:
        sys.exit("refusing port 7000: use an engine started for this script")

    e = Engine(args.host, args.port)
    setup(e)
    out = {}
    out["getversion"] = e.call("getversion")
    out["getstate"] = e.call("getstate")
    out["banks"] = e.call("banks")
    racks = [e.call("get_rack_unit_order", [s]) for s in (0, 1)]
    out["get_rack_unit_order"] = racks
    plugins = e.call("pluginlist")
    out["pluginlist"] = plugins

    # plugin_load_ui aborts the engine for an unknown plugin id
    known = {p["id"] for p in plugins}
    units = []
    for u in racks[0] + racks[1] + EXTRA_UNITS:
        if u in known and u not in units:
            units.append(u)
    out["plugin_load_ui"] = {
        u: {
            "glade": e.call("plugin_load_ui", [u, UI_FORM_GLADE | UI_FORM_STACK]),
            "stack": e.call("plugin_load_ui", [u, UI_FORM_STACK]),
        }
        for u in units
    }

    plist = e.call("parameterlist")
    kept = []
    for i in range(0, len(plist) - 1, 2):
        pid = param_id(plist[i + 1])
        if pid is None:
            continue
        group = pid.split(".", 1)[0]
        if group in units or group in KEEP_GROUPS or group not in known:
            kept += [plist[i], plist[i + 1]]
    out["parameterlist"] = kept
    out["get_midi_controller_map"] = e.call("get_midi_controller_map")
    out["load_impresp_dirs"] = e.call("load_impresp_dirs")
    text = json.dumps(out, indent=1, sort_keys=True)
    user_ir = out["load_impresp_dirs"][0][0]
    suffix = "/.config/guitarix/IR"
    if user_ir.endswith(suffix):
        text = text.replace(user_ir[: -len(suffix)], HOME)
    sys.stdout.write(text + "\n")


def setup(e):
    e.notify("setpreset", PRESET)
    e.call("getstate")  # answers in order: the preset is loaded
    time.sleep(1)
    e.notify("insert_rack_unit", ["jconv_mono", "", 0])
    e.notify("insert_rack_unit", ["nam", "jconv_mono", 0])
    ctl, param = MIDI_CONTROL
    e.notify("midi_set_config_mode", [1, -1])
    e.notify("midi_set_current_control", [ctl])
    e.notify("midi_modifyCurrent", [param, 0, 1, 0, 0])
    e.notify("midi_set_config_mode", [0, -1])
    e.call("getstate")
    time.sleep(0.5)


def param_id(entry):
    body = entry
    if "value_names" in body:
        body = body.get("IntParameter") or body.get("FloatParameter") or {}
    return body.get("Parameter", {}).get("id")


if __name__ == "__main__":
    main()
