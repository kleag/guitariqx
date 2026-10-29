# Development

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
build/guitariqx -p 7000
```

Qt 6.4 is the oldest supported version (Debian 12 and Raspberry Pi OS
Bookworm): newer Qt APIs must be avoided or guarded, the CI builds with
both Qt 6.4 and Qt 6.8.

## Design

```
src/rpcclient.*   JSON-RPC over TCP: newline framed messages, request ids,
                  notifications, reconnect
src/parameter.*   client mirror of a gx_engine::Parameter (from "parameterlist")
src/machine.*     engine proxy exposed to QML as the singleton "Gx"
                  (counterpart of GxMachineRemote in gx_head/gui/machine.cpp);
                  also presets.cpp, bankfiles.cpp and files.cpp
src/uitree.*      rack unit layouts: converts the UiBuilder call list
                  returned by plugin_load_ui into a tree for QML
src/glade.cpp     the same for the Glade layouts of the units
src/tuner.*       tuner display: note, cents and target string from the
                  engine's frequency (Gx.tuner)
src/midimap.*     MIDI controller assignments and MIDI learn (Gx.midi)
src/dirmodel.*    folder listing of the file chooser, local or by the engine
qml/              views and controls
tests/            unit tests and the fake engine
```

## The engine's protocol

guitariqx speaks the JSON-RPC protocol of `trunk/src/gx_head/engine/jsonrpc.cpp`
in the guitarix sources, which `guitarix -G` uses too. Notes:

- The server does not echo a `set` back to the connection it came from, so
  the client updates its own model when it sends a change.
- One `set` notification can carry several id/value pairs; `get_updates`
  (used to poll meters every 60 ms) answers with such a notification.
- Enum values are indexes in `set` notifications but value ids in `get`
  results.
- The server's JSON parser has no `true`/`false`: booleans are sent as 0/1.
- The server sends request ids back as strings.
- Meters flagged `[log]` report linear amplitudes; their range is the dB range
  to display.
- The tuner reports the frequency in `tuner_changed` notifications once
  `tuner_used_for_display` is on; note names, cents and strings are computed
  by the client from the reference pitch and temperament parameters.
- `midi_modifyCurrent` only works in MIDI config mode, which must be left
  again with `midi_set_config_mode`; `midi_changed` carries the whole map.

The engine trusts its clients: unknown parameter ids (`get_updates`,
`midi_modifyCurrent`, `midi_deleteParameter`), unknown plugin ids
(`plugin_load_ui`) and unknown bank names (the bank and preset procedures)
crash or abort it. guitariqx only sends what the engine listed, and the
tests check it (see below).

guitariqx works with engines of JSON-RPC interface 1.1 (guitarix 0.47.0)
and uses the methods of newer ones when the engine reports them
(`getversion`).

## Tests

```sh
ctest --test-dir build --output-on-failure      # all, about 40 s
ctest --test-dir build -R tst_machine           # one test program
build/tests/tst_machine midiLearn               # one test function
build/tests/tst_ui "states:tuner"               # one row of a data driven test
```

The tests use Qt Test. The application's classes are compiled a second
time into a static library, without `main()` and without the QML module,
and run against **FakeEngine** (`tests/fakeengine.*`): a JSON-RPC server
that answers with results recorded from a real engine
(`tests/fixtures/engine.json`) and keeps the state the client changes
(presets, banks, rack, parameter values, MIDI map). It records the requests
that would crash or mislead the real engine (JSON booleans, unknown methods,
parameter ids, plugin ids or bank names, MIDI calls outside of config
mode), and each test checks that there are none.

| test | |
|---|---|
| `tst_parameter` | parameter parsing, ranges, display, value encoding |
| `tst_uitree` | stack and Glade layouts, recorded from the engine |
| `tst_rpcclient` | framing, ids, notifications, errors, reconnection |
| `tst_machine` | `Machine`, `Tuner` and `MidiMap` against FakeEngine |
| `tst_dirmodel` | the file chooser's folder listing |
| `tst_ui` | runs `guitariqx` against FakeEngine in each UI state below, offscreen, and fails on any QML warning |

QML errors only show at run time, and some only with the oldest supported
Qt: `tst_ui` is what catches them, keep its list of UI states up to date
when adding views or dialogs.

### UI states for testing

`--debug-state` opens UI states that would otherwise need an instrument, a
MIDI device or clicks (comma separated):

| state | |
|---|---|
| `tuner:HZ` | the tuner, showing the frequency HZ |
| `learnmode` | MIDI learn mode |
| `learn:PARAM` | the MIDI learn dialog of a parameter |
| `midimap` | the MIDI controllers dialog |
| `saveas` | the Save as dialog |
| `files:UNIT` | the file chooser of a rack unit (which must be in the rack) |
| `bankload`, `banksave:BANK` | the bank file choosers |

### Without guitarix

The fake engine is also a program, to run guitariqx without guitarix:

```sh
build/tests/fake-engine -p 7124 &
build/guitariqx -p 7124
```

`--interface-minor 1` makes it answer like guitarix 0.47.0.

### Fixtures

`tests/fixtures/engine.json` is recorded by `tests/fixtures/capture.py`
from a real engine with a fresh configuration. The script changes the
engine's state, so never run it against an engine in use:

```sh
mkdir -p /tmp/gx/.config
HOME=/tmp/gx XDG_CONFIG_HOME=/tmp/gx/.config pw-jack guitarix -N -J -K -M -n fixtures -p 7123 &
tests/fixtures/capture.py --port 7123 > tests/fixtures/engine.json
```

### Screenshots

The screenshots of this documentation are taken by `tst_ui`:

```sh
docs/update-screenshots.sh build
```

## Continuous integration

`ci/check.sh` installs the build dependencies (`--deps`), builds, runs the
tests and makes the Debian package (`--package`). The GitHub workflow
`.github/workflows/ci.yml` runs it on each push and pull request in Debian
bookworm (Qt 6.4) and trixie (Qt 6.8) containers, on x86_64 and arm64
runners, then installs each package in a clean container and starts
guitariqx. The same runs locally in a container:

```sh
podman run --rm -v "$PWD:/src:Z" -w /src debian:bookworm ci/check.sh --deps --package
```

## Releasing

1. Update the version in `project(guitariqx VERSION x.y.z)` in
   `CMakeLists.txt` and the status in `README.md` and `docs/features.md`.
2. Commit, then tag and push:

    ```sh
    git tag -a v0.2.0 -m "guitariqx 0.2.0"
    git push origin main v0.2.0
    ```

`.github/workflows/release.yml` checks that the tag matches the CMake
version, builds and tests the packages like the CI, and publishes a GitHub
release with the four Debian packages, a source archive and `SHA256SUMS`;
the release notes list the changes since the previous tag. A tag with a
suffix, such as `v0.2.0-rc1`, makes a pre-release.

## Documentation

This site is made with [MkDocs](https://www.mkdocs.org/) and the Material
theme from `docs/`, and published on GitHub Pages by
`.github/workflows/docs.yml` when `docs/` changes on `main`. To preview it:

```sh
python3 -m venv /tmp/mkdocs && /tmp/mkdocs/bin/pip install -r docs/requirements.txt
/tmp/mkdocs/bin/mkdocs serve
```
