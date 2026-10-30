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
src/rpcclient.*       JSON-RPC over TCP: newline framed messages, request ids,
                      notifications, reconnect
src/parameter.*       client mirror of a gx_engine::Parameter (from "parameterlist")
src/machine.*         engine proxy exposed to QML as the singleton "Gx"
                      (counterpart of GxMachineRemote in gx_head/gui/machine.cpp);
                      split by topic: presets.cpp (bank and preset editing),
                      bankfiles.cpp (.gx import/export), files.cpp (file places,
                      IR and model loading), sequencer.cpp (drum sequencer lines),
                      unitpresets.cpp (presets of one rack unit)
src/uitree.*          rack unit layouts: converts the UiBuilder call list
                      returned by plugin_load_ui into a tree for QML
src/glade.cpp         the same for the Glade layouts of the units
src/tuner.*           tuner display: note, cents and target string from the
                      engine's frequency (Gx.tuner)
src/midimap.*         MIDI controller assignments and MIDI learn (Gx.midi)
src/dirmodel.*        folder listing of the file chooser, local or by the engine
src/skin.*            gx_head skins: style sheet values and images (Gx.skin)
src/pluginmanager.*   LADSPA/LV2 plugin list and settings (Gx.pluginManager)
src/oscilloscope.*    oscilloscope frames (Gx.oscilloscope)
src/impulseresponse.* convolver settings edited by the IR editor
src/waveview.*        drawing of the oscilloscope and of impulse responses
src/systemkeyboard.*  shows the desktop on-screen keyboard over D-Bus (squeekboard)
skins/                the built-in Guitarix skin and the libgxw images
qml/                  views and controls
tests/                unit tests and the fake engine
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
- The tuner switcher (preset by note) runs in the engine: a controller on
  `ui.live_play_switcher` or `tuner_switcher_toggle [tuner shown]` starts
  it; the client shows `set_display_state`, `display_bank_preset` and
  `show_tuner` ("display" and "tuner" listen tokens). The note is
  `12 log2(f/440)` whatever the reference pitch: A2..A#3 select banks from
  the last one, B3..E5 presets 1..18.
- `midi_modifyCurrent` only works in MIDI config mode, which must be left
  again with `midi_set_config_mode`; `midi_changed` carries the whole map.
- `midi_value_changed [ctl, value]` is sent every 60 ms for the controllers
  that moved (to "midi" listeners); there is no RPC to ask for the current
  values (gx_head's `request_midi_value_update` has no server side).
- `get_midi_feedback` (like `get_last_midi_control_value`) answers with an
  object without key, `"result": {0}`, which is not JSON; `RpcClient` turns
  such a result into an array. The option is not broadcast.
- Some parameters have objects as values (parameterlist tags `JConv`,
  `Seq`, `Osc`). `get_updates` only handles float parameters (also the
  float-backed switches such as the looper's record and play, and
  non-output floats such as `seq.pos`) and `Osc`; it asserts on any other.
- `oscilloscope.info` (`Osc`) is
  `[load, frames, is_rt, jack period, size, [samples]]`, the buffer holding
  `get/set_oscilloscope_mul_buffer` (1 to 6) jack periods.
- `<unit>.convolver` (`JConv`) is `{jconv.IRFile, jconv.IRDir, jconv.Gain,
  jconv.GainCor, jconv.Offset, jconv.Length, jconv.Delay, jconv.gainline}`.
  `IRDir` can start with `%S` (factory IRs) or `%U` (the user's), the first
  two folders of `load_impresp_dirs`; it is sent back as received.
  `Length` 0 means the whole file. The gain line is `[[sample, dB], ...]`,
  linear in dB between the points; it must be empty or go from sample 0 to
  the last one with strictly increasing samples, else the convolver reads
  outside of it. `jconv.Gain` is the normalized gain
  (`IRWindow::calc_normalized_gain`), used when `GainCor` is 1.
- `read_audio` of a file the engine cannot read answers `"result": }`.
- `seq.sequencer.<drum>` (`Seq`) is `{"seq.seqline": [0|1, ...]}`. The
  sequencer plays as many steps as its shortest line has and breaks on an
  empty line, which a fresh engine has until a line is edited: guitariqx
  gives every line `seq.asequences` steps.
- Unit presets: `plugin_preset_list_load [unit]` answers
  `[[name, is_set], ...]`, the user's (and LV2) presets, an entry with an
  empty name, then the factory presets; `is_set` is 1 when the unit's
  values are those of the preset. `plugin_preset_list_set [unit, factory,
  name]` sends a `set` notification for each parameter it changes, to its
  sender too, then an empty `set`; `plugin_preset_list_save` and
  `plugin_preset_list_remove [unit, name]` notify nothing. The engine
  leaves `seq.npreset`/`seq.ppreset` to the GUI, which loads the next or
  previous user preset of the sequencer and sets them back to 0.

The engine trusts its clients: unknown parameter ids (`get_updates`,
`midi_modifyCurrent`, `midi_deleteParameter`), unknown plugin ids
(`plugin_load_ui`, `plugin_preset_list_*`) and unknown bank names (the bank and preset procedures)
crash or abort it. guitariqx only sends what the engine listed, and the
tests check it (see below).

The headless engine (`guitarix -N`) never sets its locale, so loading a
preset whose name is not plain ASCII throws in the middle of `setpreset`
(a log line streams the `Glib::ustring` name): the sound stays faded out, no
`preset_changed` is sent and the engine never reads the requesting
connection again, though it keeps it open. The GTK engine only has the
problem when started with `LANG=C`. guitariqx follows each `setpreset` with
a `get`; without an answer within 8 s it connects again, tells the user to
select another preset (an ASCII one brings the sound back) and does not send
non-ASCII preset names to that engine any more. The old connection must stay
open and be read: the engine never notices that a connection it no longer
reads is closed, retries its writes to it forever, and that starves the idle
handler sending all notifications (`preset_changed`, the `get_updates`
values, ...). It happens anyway once guitariqx quits, so the engine has to
be restarted.

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
| `tst_skin` | gx_head skins: style sheet values and images |
| `tst_systemkeyboard` | the on-screen keyboard: keyboard detection, when it is shown and hidden |
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
| `switcher[:STATE]` | preset by note, in the switcher state STATE (`listening` by default, `wait_start`, `wait_stop`) |
| `learnmode` | MIDI learn mode |
| `learn:PARAM` | the MIDI learn dialog of a parameter |
| `midimap` | the MIDI controllers dialog |
| `saveas` | the Save as dialog |
| `files:UNIT` | the file chooser of a rack unit (which must be in the rack) |
| `bankload`, `banksave:BANK` | the bank file choosers |
| `skins` | the skin dialog |
| `unit:UNIT` | adds UNIT at the top of its rack (if missing) and switches it on |
| `iredit:UNIT` | the IR editor of a convolver (`jconv`, `jconv_mono`) |
| `sequencer` | the drum sequencer (with `unit:seq`) |
| `unitpresets:UNIT` | the presets of a rack unit (which must be in the rack) |
| `rackdrag:UNIT` | a rack unit dragged to the top of its rack (which must be in the rack) |
| `rackplace:UNIT` | a rack unit being placed by hand, as if just added (which must be in the rack) |
| `plugins[:NAME]` | the LADSPA/LV2 plugins dialog, or the settings of the plugin NAME |

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

It records the layouts and parameters of the units in the rack and of a
few more (`EXTRA_UNITS`: convolvers, neural units, looper, drum sequencer,
oscilloscope).

`tests/fixtures/ladspalist.json` is five plugins (LADSPA and LV2, mono and
stereo) cut from a real `load_ladspalist` answer, whose result is
`[not found, plugins]`; the first one is active. FakeEngine loads and
unloads them as `ladspaloader_update_plugins` does.

### Screenshots

The screenshots of this documentation are taken by `tst_ui`:

```sh
docs/update-screenshots.sh build
```

## Continuous integration

`ci/check.sh` installs the build dependencies (`--deps`), builds, runs the
tests and makes the Debian package (`--package`). The GitHub workflow
`.github/workflows/ci.yml` runs it on each push and pull request in Debian
bookworm (Qt 6.4), trixie (Qt 6.8) and Ubuntu 26.04 containers, on x86_64 and arm64
runners, then installs each package in a clean container and starts
guitariqx. The same runs locally in a container:

```sh
podman run --rm -v "$PWD:/src:Z" -w /src debian:bookworm ci/check.sh --deps --package
```

## Releasing

Versions are managed with [bumpver](https://github.com/mbarkhau/bumpver),
run by [uv](https://docs.astral.sh/uv/) without installing anything:
`bumpver.toml` lists the files holding the version (`CMakeLists.txt`, the
package names in `README.md` and `docs/install.md`).

1. Update the status in `README.md` and `docs/features.md`, commit, on an
   up to date `main` with a clean working tree.
2. Bump the version; bumpver rewrites the files, commits
   ("Release 0.2.0") and makes the annotated tag `0.2.0`:

    ```sh
    uvx bumpver update --minor --dry   # show the changes only
    uvx bumpver update --minor         # or --patch, --major
    ```

3. Push the commit and the tag:

    ```sh
    git push origin main 0.2.0
    ```

`.github/workflows/release.yml` checks that the tag matches the CMake
version, builds and tests the packages like the CI, and publishes a GitHub
release with the Debian packages, a source archive and `SHA256SUMS`;
the release notes list the changes since the previous tag. A tag with a
suffix, such as `0.2.0-rc1` made by hand on the commit of version 0.2.0,
makes a pre-release. Tags starting with `v` (`v0.2.0`) work too.

After a release (not a pre-release), `release.yml` runs the documentation
workflow, which publishes the packages of the latest release as the apt
repository `https://kleag.github.io/guitariqx/apt/` with the site
(`ci/apt-repo.sh`, reprepro). Each package goes to the suite of its
`1~<codename>` release. The repository is signed with the key of the
guitarix-kleag repository, kept in the `APT_SIGNING_KEY` secret.

## Documentation

This site is made with [MkDocs](https://www.mkdocs.org/) and the Material
theme from `docs/`, and published on GitHub Pages by
`.github/workflows/docs.yml` when `docs/` changes on `main`. To preview it:

```sh
python3 -m venv /tmp/mkdocs && /tmp/mkdocs/bin/pip install -r docs/requirements.txt
/tmp/mkdocs/bin/mkdocs serve
```
