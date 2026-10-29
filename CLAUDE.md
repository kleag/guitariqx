# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

guitariqx is a Qt Quick/QML UI for the [guitarix](https://github.com/brummer10/guitarix) engine. It is a pure JSON-RPC client of `guitarix -N -p PORT` (server side: `trunk/src/gx_head/engine/jsonrpc.cpp` in the guitarix sources) and links no guitarix library. It targets desktops and Raspberry Pi touch screens (800x480 and up) and may only use the free (LGPL) Qt 6 modules Core, Gui, Network, Qml, Quick and QuickControls2. README.md holds the user documentation and the protocol notes; TODO.md holds the remaining work and the known engine bugs.

## Build and run

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
guitarix -N -p 7000 &            # engine
build/guitariqx -p 7000          # UI (-H HOST for a remote engine)
```

The minimum is Qt 6.4 (Debian 12 / Raspberry Pi OS Bookworm), so APIs added later must not be used, or must be guarded as in CMakeLists.txt (`qt_policy` is guarded and `RESOURCE_PREFIX` is explicit; `main.cpp` uses `engine.load(qrc:/Guitariqx/qml/Main.qml)` because `loadFromModule` needs 6.5). Every new `.cpp`/`.h` or `.qml` file has to be added to `qt_add_executable` / `qt_add_qml_module` in CMakeLists.txt.

## Testing

There is no unit test suite. Changes are checked by running the UI against a real engine and taking screenshots:

```sh
QT_QPA_PLATFORM=offscreen build/guitariqx -p 7123 --size 800x480 \
    --screenshot /path/shot.png --screenshot-delay 3000 --debug-state files:jconv_mono
```

- `--debug-state` (comma separated: `tuner:HZ`, `learnmode`, `learn:PARAM`, `midimap`, `saveas`, `files:UNIT`, `bankload`, `banksave:BANK`) opens UI states that would otherwise need an instrument, a MIDI device or clicks. `tuner`/`learn*` are handled in `src/main.cpp`, `midimap` in `qml/Main.qml`, the others by the QML component concerned reading `Qt.application.arguments` (`PresetPanel.qml`, `FileButton.qml`); a new debug state follows the same pattern. `files:UNIT` needs that unit in the rack.
- `--dump` prints connection, preset, rack and engine state changes.
- Use an isolated engine rather than the user's one (which often runs on port 7000): set `HOME` and `XDG_CONFIG_HOME` to a scratch dir (create its `.config` first or the engine aborts), then `pw-jack guitarix -N -J -K -M -n <name> -p 7123` (or a dummy JACK server, `jackd -ddummy`).
- The distro guitarix 0.47.0 reports JSON-RPC interface 1.1 and is the "old engine" test case. `GUITARIQX_REMOTE_FILES=1` makes a local engine behave like a remote one for file listing; `GUITARIQX_FORM_STACK=1` uses stack layouts instead of Glade layouts.

## Architecture

**`Machine` (`src/machine.*`) is the center**: the engine proxy, the counterpart of `GxMachineRemote` in gx_head, exposed to QML as the singleton `Gx`. It is created in `main()` and handed to the QML engine via `Machine::create()` (it must not be default constructible). Its implementation is split by topic over several files that all define `Machine::` methods: `machine.cpp` (connection, parameters, rack, notifications, polling), `presets.cpp` (bank/preset editing), `bankfiles.cpp` (.gx import/export), `files.cpp` (file places, directory listing, IR and model loading).

Data flow:
- `RpcClient` (`src/rpcclient.*`): newline-framed JSON-RPC over TCP, request ids with callbacks, notifications, automatic reconnect.
- On connect, `Machine` loads `parameterlist` into `Parameter` objects (`src/parameter.*`, one per engine parameter, looked up by id with `Gx.param(id)`), the plugin list, banks and racks.
- QML writes `Parameter.value`; the parameter emits `changeRequested` and `Machine` sends `set`. The server does not echo a `set` back to its sender, so the local model is updated immediately. Incoming `set` notifications (possibly several id/value pairs) update parameters.
- Output parameters (meters) are polled with `get_updates` every 60 ms only while some QML item has registered them via `watchOutput`/`unwatchOutput`.
- `Tuner` (`Gx.tuner`) and `MidiMap` (`Gx.midi`) are sub-objects of `Machine`: the engine only sends the frequency / whole MIDI map; note names, cents, strings, learn state are computed client side.

**Rack unit panels**: `Machine::unitUi(id)` calls `plugin_load_ui` and converts the answer into a tree of `QVariantMap`s (format documented in `src/uitree.h`): `uitree::fromGlade` (`src/glade.cpp`, translates the GtkBuilder XML and gxw widgets) is preferred, then `uitree::fromStackOps` (`UI_FORM_STACK` UiBuilder call list), then `uitree::generic` (one control per parameter). The result arrives asynchronously (`unitUiReady(id)`). In QML, `RackUnit.qml` renders it with `UiNode.qml`, which recurses through `Loader`s and instantiates `Control.qml`, which dispatches to `Knob`, `ParamSwitch`, `ParamSelector`, `ParamSlider`, `ParamSpin`, `LevelMeter`, `FileButton`...

**Files**: models and IRs are files of the engine's computer. When the engine is local (`Gx.engineLocal`) `DirModel` reads the disk directly; otherwise it uses the engine's `list_directory`/`get_file_places` RPCs (interface 1.2). Bank `.gx` files, in contrast, are always on the UI's computer.

## Constraints and pitfalls

- Stay compatible with engines lacking newer RPCs: test `m_interfaceMinor` (e.g. `remoteFiles()` is `minor >= 2`) and degrade gracefully (the file chooser falls back to typing a path).
- The engine is fragile; the client must protect it (details in TODO.md, "Engine side"):
  - its JSON parser has no `true`/`false`: send booleans as 0/1;
  - unknown parameter ids crash `get_updates`, `midi_modifyCurrent` and `midi_deleteParameter`: only send known ids;
  - unknown bank names crash the bank/preset procedures, and duplicate preset names corrupt state: validate names against a fresh bank list first, and run edits one at a time through `enqueueEdit` (bank/preset state is not broadcast and is fetched again after each edit);
  - enum values are indexes in `set` notifications but value ids in `get` results;
  - `midi_modifyCurrent` only works in MIDI config mode, which must be left again with `midi_set_config_mode`.
- Engine-side changes go upstream as small, separate branches of the guitarix repository, not into this repo.
- Touch first: every interaction must work by tapping, with interactive items at least 40 px; mouse-only gestures (right click, hover, wheel) are extras only. Below 1100 px width racks become tabs and the preset list a drawer.
- Source files carry a GPL-2-or-later header with "Copyright (C) 2026 Gaël de Chalendar".
