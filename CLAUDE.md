# Pump 96 — project notes

BoomTown Beats Pump 96: JUCE 9.0.4 audio effect (AU + VST3), tempo-synced volume shaper ("sidechain pump" without a
sidechain) with a drawable curve. Part of the BoomTown Beats 96 line (festerlabz-cyber/console96, reverb96, delay96,
kick96, clip96), sharing their look and build setup.
Owner speaks Dutch; write user-facing docs in Dutch, GUI text in English.

## Layout
- `Source/Pump96DSP.h` — whole DSP engine, no JUCE dependency. Port of `prototype/dsp.js`; keep them in sync
  (`tests/dsp_pairs.cpp` output matches the JS engine to float precision, including custom points and free-running).
  Curve points (max 64, x 0..1 per period, ends pinned) → 513-entry LUT; gain = 1 − depth·(1 − curve(phase)),
  one-pole Smooth; Band Full / Lows / Highs via an LR4 split; output, bypass. Phase = host ppq / rate + offset;
  free-runs from the last position when the host is stopped.
- `Source/Parameters.h` — parameter IDs (same keys as the prototype), ranges, formatting, 8 factory presets (a preset may
  carry its own points), points ↔ string for the saved state.
- `Source/PluginProcessor.*` — APVTS, presets as programs, bypass, host ppq/tempo, no latency. The curve lives in the
  processor (SpinLock + version counter; the audio thread copies it with tryLock). Choosing a shape loads its points;
  editing the curve switches Shape to Custom. Points are saved in the state as "x,y;x,y;…".
- `Source/PluginEditor.*`, `UIComponents.h` — GUI at 1160×372, scaled as a whole. Same layout as the prototype:
  Depth/Rate column, `CurveEditor` (drag, click = add, double/right-click = remove, Shift = no snap, playhead),
  Feel / Band / Out groups, bottom bar Shape / host tempo / Presets.
- `.github/workflows/build-macos.yml` — universal AU + VST3, auval, DMG artifact with install script.

## Decisions
- Manufacturer code `Btbt`, plug-in code `Pm96`, bundle id `com.boomtownbeats.pump96`. Never change these.
- Own algorithm and names; do not use other brands' names.
- `g.setOpacity (1.0f)` before `drawImage`: drawing uses the current opacity left by earlier `setColour` calls.

## Testing
- `cmake -S . -B build-linux -G Ninja -DJUCE_DIR=<juce> -DPM96_BUILD_TESTS=ON && cmake --build build-linux --target Pump96RenderTest`
  then `xvfb-run -a build-linux/Pump96RenderTest_artefacts/Release/Pump96RenderTest <outdir>`: per-preset levels with a
  fake host at 160 BPM, beat-grid check, custom points state round trip, GUI PNG at 1× and 2×.
