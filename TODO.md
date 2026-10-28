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
- [ ] File choosers for impulse responses (cab, jconv), NAM and RTNeural
      models (`create_fload_switch`, RPC get_file_list, load_impresp_dirs);
      without them the NAM/RTNeural units cannot load a model

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
- The bank and preset procedures (erase_preset, reorder_preset, pf_append,
  rename_preset, ...) crash the engine for unknown bank names
  (`banks.get_file()` is not checked); `rename_preset` accepts an existing
  name, and a later `reorder_preset` then aborts the engine. qt_head checks
  names against a fresh bank list and serializes edits
- Preset and bank edits are not broadcast (only `bank_insert_new` sends
  `presetlist_changed`), nor are the changes of the current preset they
  cause: other connected UIs do not see them until they reload

## Packaging

- [ ] Debian packaging (separate binary package, depends on the engine)
- [ ] Icon and translations (qsTr is used everywhere, no .ts files yet)
- [ ] Remember window size, last host/port and "mute while tuning" (QSettings)
