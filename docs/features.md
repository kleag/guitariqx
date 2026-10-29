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
- mono and stereo racks: on/off, add, remove, reorder, collapse
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

## Not yet

- gx_head's slider and meter images, its two-tone unit backgrounds and its
  wallpapers, LV2/LADSPA plugin management

The full list of what remains is in
[TODO.md](https://github.com/kleag/guitariqx/blob/main/TODO.md).
