# Features and status

guitariqx is tested with Qt 6.10 on x86_64 against guitarix 0.47.0, and
with Qt 6.4.2 on a Raspberry Pi 5 (Raspberry Pi OS Bookworm, arm64, Wayland
desktop), where the tuner and, partially, MIDI control were also checked.
Its automated tests run with Qt 6.4 and Qt 6.8 on x86_64 and arm64.

## Working

- connection, engine state (on / bypass / mute), DSP load, output meters
- preset banks and presets, previous/next preset
- preset editing in the preset list: Save, Save as (to any writable bank,
  with an explicit replace), and per preset save here, rename, copy to
  bank, move up/down, delete; per bank new, rename, move up/down, delete.
  Factory banks are read only, their presets can be copied
- bank files: import a `.gx` file as a new bank, save a bank to a `.gx` file
- files: NAM and RTNeural models, convolver impulse responses and looper
  imports, chosen with a single click file chooser, on the engine's computer
- mono and stereo racks: on/off, add (at the top, or placed by hand),
  remove, reorder (by drag and drop or up/down buttons), collapse
- LADSPA/LV2 plugins: choose which plugins of the engine's computer are
  rack units, with their name, category, master slider, dry/wet control
  and stereo to mono option
- unit presets: load, save and remove the settings of one rack unit,
  factory presets, standard values; previous/next preset of the drum
  sequencer
- rack unit panels with gx_head's arrangement (Glade layouts translated to
  guitariqx's widgets, else stack layouts, else a generic panel); units
  too wide for their rack wrap their controls onto several rows
- main amplifier panel
- special units: the looper's transport with the play head of each tape,
  the oscilloscope, the impulse response editor of the convolvers (level
  in dB, part used, delay, gain line, gain correction) and the drum
  sequencer
- gx_head's skins for the rack units (menu, Skin): background, rack
  handles and screws, knob images with their LED rings, switch icons
  (also the looper's), colors. The Guitarix skin is built in, the others
  are read from an installed guitarix
- tuner: note, deviation needle, frequency, string tunings of gx_head,
  temperaments, reference pitch, mute while tuning, MIDI controlled
- preset by note (the engine's tuner switcher): the note played selects a
  bank or preset, started from the menu or a MIDI foot switch
- MIDI learn on every control, MIDI controller list with live controller
  values, MIDI channel, MIDI feedback
- the desktop's on-screen keyboard (squeekboard) for text fields on
  computers without a physical keyboard
- reconnection when the engine restarts, or when it stops answering after
  loading a preset

## Not yet

- gx_head's slider and meter images, its two-tone unit backgrounds and its
  wallpapers
- the editor of the LADSPA/LV2 plugins' controls (gx_head's port list)
- online presets (musical-artifacts.com), the conversion of banks with an
  old file version
- an on-screen keyboard under `eglfs`, where there is no desktop
- translations

## Known problems

These come from the engine (guitarix 0.47.0 and older), see
[Development](development.md#the-engines-protocol):

- Loading a preset whose name is not plain ASCII (accents, symbols) leaves
  the headless engine silent and stops it sending updates. guitariqx
  notices it, connects again and asks to select another preset, which
  brings the sound back, and then no longer loads such presets; the engine
  has to be restarted once guitariqx quits. Renaming such presets avoids
  the problem.
- Preset and bank edits are not broadcast: other interfaces connected to
  the same engine only see them after reloading.

The full list of what remains is in
[TODO.md](https://github.com/kleag/guitariqx/blob/main/TODO.md).
