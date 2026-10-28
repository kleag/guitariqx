# qt_head TODO

Roughly in order of importance for live use. See README.md for what works.

## Next

- [ ] Test on a Raspberry Pi (Bookworm, Qt 6.4, 800x480 touch screen,
      `QT_QPA_PLATFORM=eglfs`), check CPU use of the UI while playing
- [ ] Test the tuner with a real instrument (only tested with simulated
      frequencies: `--debug-state tuner:HZ`) and tune the needle smoothing
- [ ] Test MIDI learn with real hardware (tested by setting the controller
      number from a second connection, which is what the engine does when a
      controller moves)
- [x] Preset editing: save, save as, rename, delete, reorder, copy presets;
      new, rename, delete, reorder banks
- [ ] Preset editing: drag and drop reordering, import/export of bank files
      (bank_insert_content, bank_get_contents), flags of banks with an old
      file version (convert_preset)
- [x] File chooser (KDE style single click) for NAM and RTNeural models,
      convolver impulse responses and looper imports
- [ ] Check that a model chosen in qt_head is heard (the engine receives
      the same loadpath/flist sequence as from gx_head, the audio was not
      tested)
- [x] Remote file choosing when the engine runs on another computer
      (list_directory and get_file_places RPC, interface 1.2)
- [ ] Propose the engine side (jsonrpc.cpp, interface 1.2) upstream
- [ ] Loading an IR fetches all its samples (read_audio) to compute the
      normalized gain: 0.12 s for greathall.wav (112561 frames, stereo), more
      for long reverbs, and other messages wait meanwhile. Better: an engine
      call that loads an IR with normalized gain
- [ ] The neural units list at most 127 models of a folder and rescan it
      only when the folder changes: a model beyond that, or added since,
      is refused with "the engine does not list ..."
- [ ] IR editing of the convolvers (offset, length, delay, gain line, like
      gx_head's IR window); qt_head loads the whole file with normalized gain

## Rack and plugin panels

- [ ] Glade layouts (`UI_FORM_GLADE`): translate the gxw widget subset to
      QML so that the 114 units with a Glade file look like in gx_head
- [ ] Special widgets: looper (dubber) transport, drum sequencer, oscilloscope,
      jconv gain line, IR wave view
- [ ] Tall stacks of meters (e.g. Multi Band Compressor "Analyze")
- [ ] Drag and drop reordering of rack units (now: up/down buttons)
- [ ] Unit presets (plugin_preset_list_*)
- [ ] LV2/LADSPA plugin management (load_ladspalist, ladspaloader_update_plugins)

## Tuner

- [ ] Tuner switcher (MIDI controlled tuner and preset display,
      tuner_switcher_* RPC, "display" and "tuner" listen tokens)
- [ ] Switch the engine's tuner off when the UI quits while the tuner is shown
      (the server does not do it when a connection closes)
- [ ] Strobe display option

## MIDI

- [ ] MIDI feedback option (get/set_midi_feedback)
- [ ] Show incoming controller values (midi_value_changed notification)
- [ ] Rename controllers (MidiStandardControllers::replace has no RPC yet)

## Engine side (needs changes in gx_head/engine)

- `get_last_midi_control_value` returns invalid JSON (an object without a
  key, jsonrpc.cpp); qt_head does not call it
- `get_updates`, `midi_modifyCurrent` and `midi_deleteParameter` crash the
  engine for unknown parameter ids (`ParamMap::operator[]` does not check);
  qt_head only sends known ids
- The server's JSON parser has no `true`/`false` tokens
- `tools/check_rpc` is still Python 2 and does not run
- The bank and preset procedures (erase_preset, reorder_preset, pf_append,
  rename_preset, ...) crash the engine for unknown bank names
  (`banks.get_file()` is not checked); `rename_preset` accepts an existing
  name, and a later `reorder_preset` then aborts the engine. qt_head checks
  names against a fresh bank list and serializes edits
- The RTNeural stack layout names its file parameter `rtneuralloadfile`
  instead of `rtneural.loadfile` (qt_head fixes the id, gx_head shows no
  file button in the stack layout)
- Preset and bank edits are not broadcast (only `bank_insert_new` sends
  `presetlist_changed`), nor are the changes of the current preset they
  cause: other connected UIs do not see them until they reload

## Packaging

- [ ] Debian packaging (separate binary package, depends on the engine)
- [ ] Icon and translations (qsTr is used everywhere, no .ts files yet)
- [ ] Remember window size, last host/port and "mute while tuning" (QSettings)
