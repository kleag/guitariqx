# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

guitariqx is a Qt Quick/QML UI for the [guitarix](https://github.com/brummer10/guitarix) engine. It is a pure JSON-RPC client of `guitarix -N -p PORT` (server side: `trunk/src/gx_head/engine/jsonrpc.cpp` in the guitarix sources) and links no guitarix library. It targets desktops and Raspberry Pi touch screens (800x480 and up) and may only use the free (LGPL) Qt 6 modules Core, Gui, Network, Qml, Quick and QuickControls2. The user and developer documentation is in `docs/` (published at https://kleag.github.io/guitariqx/, protocol notes in `docs/development.md`); TODO.md holds the remaining work and the known engine bugs.

## Build and run

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
guitarix -N -p 7000 &            # engine
build/guitariqx -p 7000          # UI (-H HOST for a remote engine)
```

The minimum is Qt 6.4 (Debian 12 / Raspberry Pi OS Bookworm), so APIs added later must not be used, or must be guarded as in CMakeLists.txt (`qt_policy` is guarded and `RESOURCE_PREFIX` is explicit; `main.cpp` uses `engine.load(qrc:/Guitariqx/qml/Main.qml)` because `loadFromModule` needs 6.5). Every new `.cpp`/`.h` or `.qml` file has to be added to `qt_add_executable` / `qt_add_qml_module` in CMakeLists.txt.

## Testing

```sh
ctest --test-dir build --output-on-failure   # all (~40 s); BUILD_TESTING=ON by default
ctest --test-dir build -R tst_machine        # one test program
build/tests/tst_machine midiLearn            # one test function
build/tests/tst_ui "states:tuner"            # one data row
```

Qt Test programs in `tests/`. `GUITARIQX_SOURCES` (CMakeLists.txt, everything but `main.cpp`) is compiled a second time into `guitariqx_testlib` without the QML module, so a new source file goes in that list. Most tests run against `FakeEngine` (`tests/fakeengine.*`), an in-process JSON-RPC server answering from `tests/fixtures/engine.json` (recorded from a real engine by `tests/fixtures/capture.py`) and keeping the state the client changes. It records in `violations()` every request that would crash the real engine (JSON booleans, unknown methods/parameter ids/plugin ids/bank names, MIDI calls outside config mode); tests check it stays empty, so new engine calls need a handler in `FakeEngine::dispatch()`.

`tst_ui` runs the real `guitariqx` binary offscreen against FakeEngine for each `--debug-state` UI state and fails on any QML warning on stderr: QML errors only show at run time, and some only on Qt 6.4. Add a row when adding a view or dialog.

- `--debug-state` (comma separated: `tuner:HZ`, `switcher[:STATE]`, `learnmode`, `learn:PARAM`, `midimap`, `saveas`, `files:UNIT`, `bankload`, `banksave:BANK`, `skins`, `unit:UNIT`, `iredit:UNIT`, `sequencer`, `unitpresets:UNIT`) opens UI states that would otherwise need an instrument, a MIDI device or clicks. `tuner`/`switcher`/`learn*`/`unit` are handled in `src/main.cpp`, `midimap` in `qml/Main.qml`, the others by the QML component concerned reading `Qt.application.arguments` (`PresetPanel.qml`, `FileButton.qml`, `SkinDialog.qml`, `Control.qml`, `RackUnit.qml`); a new debug state follows the same pattern. `files:UNIT`, `iredit:UNIT` and `unitpresets:UNIT` need that unit in the rack; `unit:UNIT` adds a unit the recorded rack lacks (`unit:seq,sequencer`).
- `build/tests/fake-engine -p 7124` runs guitariqx without guitarix (`--interface-minor 1` mimics guitarix 0.47.0).
- To check against a real engine, use an isolated one rather than the user's (which often runs on port 7000): set `HOME` and `XDG_CONFIG_HOME` to a scratch dir (create its `.config` first or the engine aborts), then `pw-jack guitarix -N -J -K -M -n <name> -p 7123`. The real engine expects compact JSON and returns ids as strings.
- The distro guitarix 0.47.0 reports JSON-RPC interface 1.1 and is the "old engine" case. `GUITARIQX_REMOTE_FILES=1` makes a local engine behave like a remote one for file listing; `GUITARIQX_FORM_STACK=1` uses stack layouts instead of Glade layouts.
- `docs/update-screenshots.sh build` refreshes the documentation screenshots from `tst_ui`.

## CI, packaging, releases, docs

- `ci/check.sh [--deps] [--no-tests] [--package]` is what CI runs (build, ctest, `cpack -G DEB` into `dist/`). Check Qt 6.4 locally with `podman run --rm -v "$PWD:/src:Z" -w /src debian:bookworm ci/check.sh --deps`.
- `.github/workflows/ci.yml`: Debian bookworm (Qt 6.4), trixie (Qt 6.8) and Ubuntu 26.04 resolute containers on amd64 and arm64 runners, then an install test of each `.deb` in a clean container. `release.yml` runs on version tags (`0.2.0` or `v0.2.0`, must match `project(VERSION)`; made by `uvx bumpver update --patch|--minor|--major`, configured in `bumpver.toml`, which also lists the files holding the version) and publishes the debs, a source archive and SHA256SUMS. `docs.yml` publishes `docs/` (MkDocs Material, `mkdocs build --strict`) to GitHub Pages, with the apt repository `apt/` built by `ci/apt-repo.sh` from the latest release's debs (signed with the `APT_SIGNING_KEY` secret, the key of the guitarix-kleag repository); `release.yml` runs it on `main` after each release.
- Debian package settings are in `cmake/Packaging.cmake`; its `Depends` lists the QML modules by hand (dpkg-shlibdeps cannot see QML imports), so a new `import` in `qml/` needs its `qml6-module-*` package there and in `ci/check.sh`.
- User documentation lives in `docs/` (README.md keeps a summary); keep the options table, debug states and features list in sync.

## Architecture

**`Machine` (`src/machine.*`) is the center**: the engine proxy, the counterpart of `GxMachineRemote` in gx_head, exposed to QML as the singleton `Gx`. It is created in `main()` and handed to the QML engine via `Machine::create()` (it must not be default constructible). Its implementation is split by topic over several files that all define `Machine::` methods: `machine.cpp` (connection, parameters, rack, notifications, polling), `presets.cpp` (bank/preset editing), `bankfiles.cpp` (.gx import/export), `files.cpp` (file places, directory listing, IR and model loading), `sequencer.cpp` (drum sequencer lines), `unitpresets.cpp` (presets of one rack unit, `plugin_preset_list_*`).

Data flow:
- `RpcClient` (`src/rpcclient.*`): newline-framed JSON-RPC over TCP, request ids with callbacks, notifications, automatic reconnect.
- On connect, `Machine` loads `parameterlist` into `Parameter` objects (`src/parameter.*`, one per engine parameter, looked up by id with `Gx.param(id)`), the plugin list, banks and racks.
- QML writes `Parameter.value`; the parameter emits `changeRequested` and `Machine` sends `set`. The server does not echo a `set` back to its sender, so the local model is updated immediately. Incoming `set` notifications (possibly several id/value pairs) update parameters.
- Output parameters (meters) are polled with `get_updates` every 60 ms only while some QML item has registered them via `watchOutput`/`unwatchOutput`.
- `Tuner` (`Gx.tuner`) and `MidiMap` (`Gx.midi`) are sub-objects of `Machine`: the engine only sends the frequency / whole MIDI map; note names, cents, strings, learn state are computed client side.
- Special parameters (parameterlist tags `JConv`, `Seq`, `Osc`) have JSON objects or arrays as values. `Parameter::isPollable()` (tag `Float`/`FloatEnum`/`Osc`) is what `get_updates` accepts; the `oscilloscope.info` frames go to `Gx.oscilloscope` (`src/oscilloscope.*`), not the parameter. `ImpulseResponse` (`src/impulseresponse.*`, QML creatable) edits a convolver's settings and keeps the gain line in the shape the engine needs; `WaveView` (`src/waveview.*`, a `QQuickPaintedItem`) draws the oscilloscope and the IR. The special widgets come from Glade signal handlers in `glade.cpp` (`uitree::addSpecialWidgets()` for stack layouts).
- `Skin` (`Gx.skin`, `src/skin.*`) reads gx_head skins: the built-in Guitarix skin and libgxw images (`skins/`, a Qt resource) and the installed ones (`gx_head/skins` in the XDG data dirs). It never applies the GTK CSS, only picks values from it (colors, knob rings). QML asks for images through `Theme.skinImage()` and colors through `Theme.rack*`/`Theme.skinColor()`; controls that also appear outside rack units (`ParamSelector`, `ParamSpin`) take their label color as a property instead.

**Rack unit panels**: `Machine::unitUi(id)` calls `plugin_load_ui` and converts the answer into a tree of `QVariantMap`s (format documented in `src/uitree.h`): `uitree::fromGlade` (`src/glade.cpp`, translates the GtkBuilder XML and gxw widgets) is preferred, then `uitree::fromStackOps` (`UI_FORM_STACK` UiBuilder call list), then `uitree::generic` (one control per parameter). The result arrives asynchronously (`unitUiReady(id)`). In QML, `RackUnit.qml` renders it with `UiNode.qml`, which recurses through `Loader`s and instantiates `Control.qml`, which dispatches to `Knob`, `ParamSwitch`, `ParamSelector`, `ParamSlider`, `ParamSpin`, `LevelMeter`, `FileButton`...

**Files**: models and IRs are files of the engine's computer. When the engine is local (`Gx.engineLocal`) `DirModel` reads the disk directly; otherwise it uses the engine's `list_directory`/`get_file_places` RPCs (interface 1.2). Bank `.gx` files, in contrast, are always on the UI's computer.

## Constraints and pitfalls

- Stay compatible with engines lacking newer RPCs: test `m_interfaceMinor` (e.g. `remoteFiles()` is `minor >= 2`) and degrade gracefully (the file chooser falls back to typing a path).
- The engine is fragile; the client must protect it (details in TODO.md, "Engine side"):
  - its JSON parser has no `true`/`false`: send booleans as 0/1;
  - unknown parameter ids crash `get_updates`, `midi_modifyCurrent` and `midi_deleteParameter`, an unknown plugin id aborts `plugin_load_ui` and crashes the `plugin_preset_list_*` procedures: only send known ids;
  - unknown bank names crash the bank/preset procedures, and duplicate preset names corrupt state: validate names against a fresh bank list first, and run edits one at a time through `enqueueEdit` (bank/preset state is not broadcast and is fetched again after each edit);
  - enum values are indexes in `set` notifications but value ids in `get` results;
  - `midi_modifyCurrent` only works in MIDI config mode, which must be left again with `midi_set_config_mode`.
- Engine-side changes go upstream as small, separate branches of the guitarix repository, not into this repo.
- Touch first: every interaction must work by tapping, with interactive items at least 40 px; mouse-only gestures (right click, hover, wheel) are extras only. Below 1100 px width racks become tabs and the preset list a drawer.
- Source files carry a GPL-2-or-later header with "Copyright (C) 2026 Gaël de Chalendar".
