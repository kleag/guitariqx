# Usage

## Starting

Start the engine without its own interface, with a JSON-RPC port, then the
interface:

```sh
guitarix -N -p 7000 &
guitariqx -p 7000                # or: guitariqx -H raspberrypi.local -p 7000
```

The engine and the interface can run on different computers, e.g. the
engine on a Raspberry Pi attached to the audio interface and guitariqx on a
laptop or tablet.

| option | |
|---|---|
| `-H`, `--host HOST` | engine host (default: the last engine connected to, else `localhost`) |
| `-p`, `--port PORT` | engine port (default: the last one, else `7000`) |
| `-f`, `--fullscreen` | full screen, for touch screens |
| `--size WxH` | initial window size (default: the size, or maximized state, of the last session, else 1280x800) |
| `--dump` | print connection, preset and rack changes on the terminal |
| `--screenshot FILE` | save a screenshot after `--screenshot-delay` ms and quit (testing) |
| `--debug-state LIST` | open UI states for testing without instrument or MIDI device, see [Development](development.md#ui-states-for-testing) |

The connection indicator at the right of the top bar opens the connect
dialog. "Full screen" in the top bar menu (⋮), or F11, switches between
full screen and a window at any time (not under `eglfs` or `linuxfb`, which
are always full screen). guitariqx reconnects by itself when the engine restarts.

guitariqx remembers the last engine, the window size and the tuner's "mute
while tuning" option, with the recent folders of the file chooser, in
`~/.config/guitarix/guitariqx.conf`.

!!! warning "Security"
    The engine's JSON-RPC port has no authentication: whoever can reach it
    can control the engine, list directories and read audio files of its
    computer. Do not make it reachable from untrusted networks.

## Raspberry Pi

guitariqx runs under the desktop, or without one directly on the display:

```sh
QT_QPA_PLATFORM=eglfs guitariqx -f -p 7000
```

Below 1100 pixels width (e.g. the official 7" 800x480 touch screen) the
racks are shown as tabs and the preset list as a drawer, and rack units
too wide for the screen wrap their controls onto several rows. All interactive
items are at least 40 pixels for touch use.

To try it without a sound card, run the engine against a dummy JACK server
(`jackd -ddummy`) or PipeWire's JACK (`pw-jack guitarix -N -p 7000`).

## Presets

The preset list shows the banks and their presets; a tap loads a preset,
the arrows of the top bar step through the bank. **Save** writes the
current settings into the loaded preset, **Save as** into a new or existing
preset of any writable bank.

![Save as](images/saveas.png)

The ⋮ menu of a preset saves into it, renames, copies it to another bank,
moves it or deletes it; the one of a bank creates, renames, moves, deletes
or saves the bank to a file. Factory banks are read only, but their
presets can be copied. "Load bank file" in the ⋮ menu of the list imports a
`.gx` file as a new bank. Bank files are on the computer guitariqx runs on,
also when the engine runs elsewhere.

## Racks

**+ Add effect** adds a unit to the mono or stereo rack. Each unit has its
power switch, buttons to move it up and down, to collapse it to a compact
view and to remove it. Units are shown with gx_head's arrangement: the
Glade layout of the unit when it has one, else its stack layout, else a
generic panel built from the unit's parameters.
`GUITARIQX_FORM_STACK=1` uses the stack layouts, like `GUITARIX_FORM_STACK`
for gx_head.

## Files

Neural amp models (NAM and RTNeural) and impulse responses of the
convolvers are files of the computer the engine runs on. The file chooser
opens a folder or loads a file with a single tap; it lists places (home,
model and IR folders, USB drives, recent folders) and a clickable path; the
arrows next to a file control step through its folder.

![The file chooser of a remote engine](images/files-remote.png)

guitariqx reads the files directly when it runs on the engine's computer.
For a remote engine it needs an engine with JSON-RPC interface 1.2, which
adds the `list_directory` and `get_file_places` methods. With an older
remote engine (such as guitarix 0.47.0) the file chooser cannot list
folders and asks for the path of the file instead; everything else works.

![With an older remote engine: the path is typed](images/files-remote-old-engine.png){ width="400" }

`GUITARIQX_REMOTE_FILES=1` makes guitariqx use the engine's listing even
when the engine is local, for testing.

## Tuner

**Tuner** opens the full window tuner: note, deviation, frequency, the
string tunings of gx_head, temperaments, reference pitch, and mute while
tuning. It follows the engine parameter `system.show_tuner`, so a MIDI foot
switch assigned to the Tuner button opens and closes it. Mute while tuning
stops the engine while the tuner is shown and restores its state when the
tuner closes, or when guitariqx quits with the tuner open.

## Preset by note

gx_head's Liveplay preset mode: **Preset by note ...** in the menu, or a
MIDI foot switch assigned to it (in learn mode, tap the menu entry), mutes
the output and lets the note played on the guitar choose what to load. Mute
the strings first, then play:

| notes | |
|---|---|
| A2 to A#3 | a bank, starting from the last one |
| B3 to E5 | preset 1 to 18 of the bank |
| G2 / G#2 | bypass / mute (or back on) |
| below G2, F5 to A#6 | the tuner |

The selection is loaded when the note stops. The view lists which note
selects which bank and preset. Pressing the foot switch again, or Cancel,
leaves without loading anything; with the tuner shown, the foot switch
closes it. The engine does the selection, so the foot switch works without
any UI too.

![Preset by note](images/switcher.png)

## MIDI learn

The **MIDI** button switches learn mode on: a tap on any control (knobs,
sliders, switches, selectors, unit power, preset previous/next, bypass,
mute, tuner) then opens the learn dialog. A right click opens it at any
time.

![Learn mode](images/learnmode.png)

Move a controller or type its number, set the range (for continuous
controls) or the toggle behaviour (for switches), then connect or remove
the assignment. A bar shows the value the controller sends, to check that
it arrives.

![The learn dialog](images/learn.png)

"MIDI controllers" in the menu lists all assignments, with a bar showing
the last value each controller sent, and sets the MIDI channel. **MIDI
feedback** makes the engine send the values of the assigned parameters to
its MIDI output when a preset loads or a controller changes them, so that
controllers with LEDs or motor faders follow (changes made in the UI are
not sent).

![The MIDI controllers](images/midimap.png)
