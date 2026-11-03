# DR. SEGG-UENCER – note per lo sviluppo

- JUCE 8 MIDI sequencer plugin (VST3/AU/AAX, optional VST2). Built as an instrument with MIDI output and silent stereo audio: Ableton Live refuses plugins flagged as MIDI effects. Sibling of SONIC GRAIN, same look and toolchain.
- `Source/SeggEngine.h` and `SeggCore.h` hold no JUCE and must stay real-time safe: no allocation, no locks. Time is in beats, so output does not depend on buffer size (tested).
- Build + tests: `cmake -B build -DCMAKE_BUILD_TYPE=Release -DDRSEGG_BUILD_TESTS=ON -DDRSEGG_COPY_AFTER_BUILD=OFF && cmake --build build -j2`, then run `build/Tests/DrSeggUencerTests_artefacts/Release/DrSeggUencerTests`. `--snap file.png track page preset` renders the editor; look at it after any UI change.
- JUCE: `-DJUCE_DIR=<path>` for a local copy; otherwise FetchContent downloads tag 8.0.10.
- Headless Linux without the X dev packages: export `CXXFLAGS="-DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XCURSOR=0 -DJUCE_USE_XRENDER=0 -DJUCE_ALSA=0 -DJUCE_JACK=0"` before configuring.
- Never change these, or hosts lose saved sessions: plugin code `Dseg`, manufacturer code `Gsof`, parameter IDs (`kParamIds`, the `g*` ids), or the order of any choice list (`kRates`, `kScales`, `kTypeNames`, ...). Append only. New track parameters go at the end of enum `P`.
- On/off parameters are 2-item choices (`bparam` in `Params.h`), so pluginval strictness 8 passes the state-restore test.
- Step and pad data are not host parameters: they live in `Engine::patterns` as atomics and are saved as child nodes of the state. Port assignments are saved with the session but not in presets, because they belong to the machine.
- Ableton Live merges a plugin's MIDI output onto one channel, and AU has no MIDI out there at all. That is why `DirectMidi.h` exists: separate destinations in Live must go through direct or virtual ports.
- `Source/MidiMap.h` holds the two MIDI map tables. The input table is saved with the session (controllers, like ports, belong to the machine); the output table is saved in presets too. Parameters are never set on the audio thread: input values go through a FIFO to `pumpMidiMap()` on the message thread. The Random signal has its own generator, so mapping a track never changes its notes.
- Windows has no virtual MIDI ports (`createNewDevice` does not exist there): users need loopMIDI, whose port shows up as an ordinary device.
- Release: update `CHANGELOG.md` and `project(... VERSION x.y.z)` in `CMakeLists.txt`, then push tag `vx.y.z`. CI builds and publishes the Release.
- The user is Italian: user-facing docs (README, CHANGELOG, scripts) are in Italian. The interface is in English, like SONIC GRAIN.
