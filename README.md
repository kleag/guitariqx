# qt_head

A Qt Quick/QML user interface for guitarix, alongside the GTK `gx_head` one.

qt_head is a pure client: it talks to a guitarix engine over the JSON-RPC
protocol (`src/gx_head/engine/jsonrpc.cpp`) that `guitarix -G` already uses.
It does not link against any guitarix library and is built with CMake,
independently of the waf build. It only needs the free (LGPL) Qt 6 modules
Core, Gui, Network, Qml, Quick and QuickControls2.

## Build

Requires Qt >= 6.4 and CMake >= 3.21.

Debian / Ubuntu / Raspberry Pi OS (Bookworm or newer):

```sh
sudo apt install cmake g++ qt6-base-dev qt6-declarative-dev \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
    qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtqml-workerscript
```

```sh
cd trunk/src/qt_head
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build      # optional, installs /usr/local/bin/qt_head
```

## Run

Start the engine headless with a JSON-RPC port, then the UI:

```sh
guitarix -N -p 7000 &
qt_head -p 7000                  # or: qt_head -H raspberrypi.local -p 7000
```

The engine and the UI can run on different machines, e.g. the engine on a
Raspberry Pi attached to the audio interface and the UI on a laptop or
tablet. Options:

| option | |
|---|---|
| `-H`, `--host HOST` | engine host (default `localhost`) |
| `-p`, `--port PORT` | engine port (default `7000`) |
| `-f`, `--fullscreen` | full screen, for touch screens |
| `--size WxH` | initial window size |
| `--dump` | print connection, preset and rack changes on the terminal |
| `--screenshot FILE` | save a screenshot after `--screenshot-delay` ms and quit (testing) |
| `--debug-state LIST` | testing without instrument or MIDI device: `tuner:HZ`, `learnmode`, `learn:PARAM`, `midimap`, `saveas`, `files:UNIT` |

The connection indicator at the right of the top bar opens the connect
dialog. qt_head reconnects automatically when the engine restarts.

### Files on a remote engine

Models and impulse responses are files of the computer the engine runs
on. qt_head reads them directly when it runs on that computer too; for a
remote engine it needs an engine with JSON-RPC interface 1.2, which adds
the `list_directory` and `get_file_places` methods (older engines can be
used for everything else). `QT_HEAD_REMOTE_FILES=1` makes qt_head use the
engine's listing even when the engine is local, for testing.

The JSON-RPC port has no authentication: whoever can reach it can control
the engine, list directories and read audio files of its computer. Do not
make it reachable from untrusted networks.

### Raspberry Pi

qt_head runs under the desktop, or without one directly on the display:

```sh
QT_QPA_PLATFORM=eglfs qt_head -f -p 7000
```

Below 1100 px width (e.g. the official 7" 800x480 touch screen) the racks are
shown as tabs and the preset list as a drawer. All interactive items are at
least 40 px for touch use.

To test without a sound card, run the engine against a dummy JACK server
(`jackd -ddummy`) or PipeWire's JACK (`pw-jack guitarix -N -p 7000`).
`QT_QPA_PLATFORM=offscreen` runs the UI without a display.

## Design

```
src/rpcclient.*   JSON-RPC over TCP: newline framed messages, request ids,
                  notifications, reconnect
src/parameter.*   client mirror of a gx_engine::Parameter (from "parameterlist")
src/machine.*     engine proxy exposed to QML as the singleton "Gx"
                  (counterpart of GxMachineRemote in gx_head/gui/machine.cpp)
src/uitree.*      rack unit layouts: converts the UiBuilder call list
                  returned by plugin_load_ui into a tree for QML
src/tuner.*       tuner display: note, cents and target string from the
                  engine's frequency (Gx.tuner)
src/midimap.*     MIDI controller assignments and MIDI learn (Gx.midi)
qml/              views and controls
```

Notes on the protocol, as used by qt_head:

- The server does not echo a `set` back to the connection it came from, so
  the client updates its own model when it sends a change.
- One `set` notification can carry several id/value pairs; `get_updates`
  (used to poll meters every 60 ms) answers with such a notification.
- Enum values are indexes in `set` notifications but value ids in `get`
  results.
- The server's JSON parser has no `true`/`false`: booleans are sent as 0/1.
- Meters flagged `[log]` report linear amplitudes; their range is the dB range
  to display.
- The tuner reports the frequency in `tuner_changed` notifications once
  `tuner_used_for_display` is on; note names, cents and strings are computed
  by the client from the reference pitch and temperament parameters.
- `midi_modifyCurrent` only works in MIDI config mode, which must be left
  again with `midi_set_config_mode`; `midi_changed` carries the whole map.

## Status

Tested with Qt 6.10 on x86_64 against guitarix 0.47.0 (also built with
policy QTP0004 set to OLD, which is how Qt 6.4 behaves); not yet tested
on a Raspberry Pi.

Working:

- connection, engine state (on / bypass / mute), DSP load, output meters
- preset banks and presets, previous/next preset
- preset editing in the preset list: Save, Save as (to any writable bank,
  with an explicit replace), and per preset (⋮) save here, rename, copy to
  bank, move up/down, delete; per bank new, rename, move up/down, delete.
  Factory banks are read only, their presets can be copied.
  Bank files: "Load bank file" (⋮ of the preset list) imports a `.gx`
  file as a new bank, "Save to file" (⋮ of a bank) writes a bank to a
  `.gx` file; these files are on the computer qt_head runs on, also when
  the engine runs elsewhere
- files: NAM and RTNeural models, convolver impulse responses and looper
  imports are chosen with a file chooser in KDE's single click style
  (a click on a folder opens it, a click on a file loads it) with places
  (home, model and IR folders, USB drives, recent folders) and a clickable
  path; the arrows next to the file step through its folder. Files are those
  of the engine's computer: when it runs elsewhere, the engine lists them
- mono and stereo racks: on/off, add, remove, reorder, collapse
- rack unit panels with gx_head's arrangement: the Glade layout of the
  unit when it has one (`plugin_load_ui` with `UI_FORM_GLADE`, translated
  by src/glade.cpp: boxes, grids, frames, notebooks, labels and the gxw
  knobs, sliders, switches, selectors, meters and displays, file buttons
  from the signal handlers), else its stack layout (`UI_FORM_STACK`), else
  a generic panel built from the unit's parameters.
  `QT_HEAD_FORM_STACK=1` uses the stack layouts, like
  `GUITARIX_FORM_STACK` for gx_head
- main amplifier panel
- tuner: full window, note, deviation needle, frequency, string tunings of
  gx_head, temperaments, reference pitch, mute while tuning. It follows the
  engine parameter `system.show_tuner`, so a MIDI foot switch assigned to
  the Tuner button opens and closes it
- MIDI learn: the MIDI button switches learn mode on, then a tap on any
  control (knobs, sliders, switches, selectors, unit power, preset
  previous/next, bypass, mute, tuner) opens the learn dialog; right click
  opens it at any time. Move a controller or type its number, set the range
  or toggle behaviour, connect or remove. The MIDI controllers dialog (menu)
  lists all assignments and sets the MIDI channel

Not yet:

- gx_head's skins and images (the Glade layouts keep the arrangement, the
  widgets are qt_head's), the oscilloscope, drum sequencer and IR wave
  view, LV2/LADSPA plugin management

The full list is in [TODO.md](TODO.md).
