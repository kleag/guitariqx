# guitariqx TODO

Roughly in order of importance for live use. See README.md for what works.

## Next

- [ ] Test on a Raspberry Pi (Bookworm, Qt 6.4, 800x480 touch screen,
      `QT_QPA_PLATFORM=eglfs`), check CPU use of the UI while playing
- [ ] Test the tuner with a real instrument (only tested with simulated
      frequencies: `--debug-state tuner:HZ`) and tune the needle smoothing
- [ ] Test preset by note with a guitar and a foot switch (only simulated:
      `--debug-state switcher`, FakeEngine; the note mapping comes from
      tunerswitcher.cpp)
- [ ] Test MIDI learn with real hardware (tested by setting the controller
      number from a second connection, which is what the engine does when a
      controller moves)
- [x] Preset editing: save, save as, rename, delete, reorder, copy presets;
      new, rename, delete, reorder banks
- [x] Bank files: load (.gx import) and save (export) on the UI's computer
- [ ] Preset editing: flags of banks with an old file version
      (convert_preset), online presets (musical-artifacts.com) like gx_head
- [x] File chooser (KDE style single click) for NAM and RTNeural models,
      convolver impulse responses and looper imports
- [ ] Check that a model chosen in guitariqx is heard (the engine receives
      the same loadpath/flist sequence as from gx_head, the audio was not
      tested)
- [x] Remote file choosing when the engine runs on another computer
      (list_directory and get_file_places RPC, interface 1.2)
- [x] Propose the engine side (jsonrpc.cpp, interface 1.2) upstream:
      brummer10/guitarix#295, waiting for review
- [ ] Loading an IR fetches all its samples (read_audio) to compute the
      normalized gain: 0.12 s for greathall.wav (112561 frames, stereo), more
      for long reverbs, and other messages wait meanwhile. Better: an engine
      call that loads an IR with normalized gain
- [ ] The neural units list at most 127 models of a folder and rescan it
      only when the folder changes: a model beyond that, or added since,
      is refused with "the engine does not list ..."
- [x] IR editing of the convolvers (offset, length, delay, gain line, gain
      correction, like gx_head's IR window)
- [ ] IR editor: zoom into the start of long responses, show the channels
      separately (gx_head's left/right/sum)

## Rack and plugin panels

- [x] Glade layouts (`UI_FORM_GLADE`) of the 114 units with a Glade file
- [x] Wide units (Graphic EQ, Zita Rev1 in the stereo rack, the amp) wrap
      their rows instead of scrolling sideways on small screens
- [ ] Glade layouts: grid boxes do not wrap and still scroll sideways when
      too wide; packing expand/fill, alignment and margins are ignored
- [x] gx_head skins (menu, Skin): unit background with rack handles and
      screws, knob images and LED rings, switch icons (looper too), colors;
      Guitarix built in, the others from an installed guitarix
- [ ] More of gx_head's look: two-tone unit backgrounds (GxPaintBox
      alternate-box needs labels placed as in gx_head to stay readable),
      slider and meter images, wallpapers (amp, tuner), skinned top bar and
      preset list
- [x] Special widgets: looper (dubber) transport, drum sequencer, oscilloscope,
      jconv gain line, IR wave view
- [ ] Looper layout: the unit's Gain/Mix knobs end up low and the unit is
      wider than a rack column at 1280 px (the label_ref labels of the Glade
      grid are dropped, so the rows next to the notebook spanning them get
      its height)
- [x] Drum sequencer: next/previous unit preset (seq.npreset/ppreset)
- [ ] Tall stacks of meters (e.g. Multi Band Compressor "Analyze")
- [x] Drag and drop reordering of rack units (and up/down buttons)
- [x] Unit presets (plugin_preset_list_*)
- [ ] Unit presets of the looper: gx_head sets dubber.filename and
      dubber.savefile before saving so that the loop files follow the
      preset (and removes them with it)
- [ ] seq.npreset/ppreset changed by another client are followed (checked
      against guitarix); not checked with a MIDI controller
- [ ] Drum sequencer presets by MIDI: gx_head's "connect midi..." registers
      a seq.<preset> switch per preset (insert_param)
- [ ] Unit presets of the amplifier (ampstack): its parameters are amp.*,
      not ampstack.*, so the client side reset does not apply; no ≡ button
- [ ] LV2/LADSPA plugin management (load_ladspalist, ladspaloader_update_plugins)

## Tuner

- [x] Tuner switcher (preset by note: tuner_switcher_* RPC, "display" and
      "tuner" listen tokens)
- [x] Switch the engine's tuner off when the UI quits while the tuner is shown
      (the server does not do it when a connection closes); quitting also
      unmutes the engine and leaves MIDI config mode
- [ ] Strobe display option

## MIDI

- [x] MIDI feedback option (get/set_midi_feedback)
- [x] Show incoming controller values (midi_value_changed notification)
- [ ] Rename controllers (MidiStandardControllers::replace has no RPC yet)

## Engine side (needs changes in gx_head/engine)

- `get_last_midi_control_value` and `get_midi_feedback` return invalid JSON
  (an object without a key, `{0}`, jsonrpc.cpp); guitariqx does not call
  the first and repairs the answer of the second (RpcClient). Fixed by
  brummer10/guitarix#294 (open)
- `request_midi_value_update` (sent by GxMachineRemote) has no server side:
  current controller values are only known once a controller moves
- `get_updates`, `midi_modifyCurrent` and `midi_deleteParameter` crash the
  engine for unknown parameter ids (`ParamMap::operator[]` does not check);
  guitariqx only sends known ids
- `plugin_load_ui` aborts the engine for an unknown plugin id ("lookup
  plugin *** id not found"), and the plugin_preset_list_* procedures
  dereference find_plugin() without a check; guitariqx only sends listed
  units
- The server's JSON parser has no `true`/`false` tokens
- `tools/check_rpc` is still Python 2 and does not run
- The bank and preset procedures (erase_preset, reorder_preset, pf_append,
  rename_preset, ...) crash the engine for unknown bank names
  (`banks.get_file()` is not checked); `rename_preset` accepts an existing
  name, and a later `reorder_preset` then aborts the engine. guitariqx checks
  names against a fresh bank list and serializes edits
- The RTNeural stack layout names its file parameter `rtneuralloadfile`
  instead of `rtneural.loadfile` (guitariqx fixes the id, gx_head shows no
  file button in the stack layout)
- `read_audio` of a file it cannot read answers `"result": }` (invalid
  JSON); guitariqx only asks for resolved paths
- The drum sequencer's lines (`seq.sequencer.*`) are empty in a fresh
  engine, and setting an empty line makes its length -1 (min_seq_size);
  guitariqx always sends all lines with `seq.asequences` steps
- A convolver gain line that does not start at sample 0 or is not strictly
  increasing makes the convolver read outside of it (GxConvolver)
- Preset and bank edits are not broadcast (only `bank_insert_new` sends
  `presetlist_changed`), nor are the changes of the current preset they
  cause: other connected UIs do not see them until they reload

## Packaging

- [ ] Debian packaging (separate binary package, depends on the engine)
- [ ] Icon and translations (qsTr is used everywhere, no .ts files yet)
- [x] Remember window size, last host/port and "mute while tuning" (QSettings)
