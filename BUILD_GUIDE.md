# Build guide (2026-10-06)

This tree builds the voice engine, the design-pack PDF, and a placeholder VST3. `make test` stays on the engine and the clock-switch law. It does not compile JUCE.

## This tree

The voice engine is `engine/`. Panel art is unpainted. The editor in `plugin/` is a placeholder plate for listening in a host. JIDAI RACK is not changed by this repository. The equations in [SCHEMATICS.md](SCHEMATICS.md) are the voice contract. `make test` builds `build/shogun_tests`, runs the checks in [TESTPLAN.md](TESTPLAN.md), and runs the clock-switch law. That law test imports GraphForge from `~/graphforge/src`.

`make strict` compiles the engine, the tests, the web facade and the plugin sources with the warning flags JIDAI RACK uses (JUCE's recommended set plus `-Wfloat-equal -Wimplicit-int-float-conversion -Wshadow -Wconversion -Wdouble-promotion -Werror`) under clang++ and g++. It needs the JUCE checkout at `JUCE_LINUX` (default `/workspace/JUCE`) for the plugin headers.

## Factory bank

Program 1 is INIT (the INIT kit and the empty pattern "001 INIT"). Programs 2 to 22 are the factory kits, each with its pattern "NNN name". The bank is data in one place:

| File | Job |
| --- | --- |
| `engine/factory.h` | `shogun::factory::kBank`, `kCount` (21), `kPrograms` (22), `programName(i)`, `programJson(i)`, `loadProgram(i, Patch&)` |
| `engine/factory_bank.inc` | the 21 documents, generated; do not edit by hand |
| `engine/patch.h` | the saved-state document: `Patch`, `parsePatch`, `applyPatch`, `capturePatch`, `writePatchJson` (any sink, no libc; the wasm page saves with it) and `patchToJson` (native `std::string`) |
| `scripts/make_factory.py` | the kits and patterns as code; `make factory` runs it |
| `tools/factory_fmt.cpp` | writes the canonical text and measures the peaks (native renders at 44.1 and 48 kHz) |

Each document is what the plugin saves inside `<SHOGUN version=2>`: sparse `params` by `SECTION:LABEL` id (missing ids keep INIT), `mod` rows, `cables` (none in the bank), `cvAmt`, `inLaw` and `seq`. The bank text equals `patchToJson(parsePatch(text), false)`, so it is canonical and nothing in it goes through the alias table. `make factory` trims every kit until its 8-bar peak is -8 dBFS at the default master (within 0.35 dB), then writes the canonical text.

The engine loads a program with `factory::loadProgram` and `applyPatch`. The plugin lists the programs through `getNumPrograms`, `getProgramName` and `setCurrentProgram` (INIT, then the document through the same reader as a saved state; the parameters land at once, without the 5 ms glide), and the KIT and PATTERN displays open the same list. The web build exports `sg_factory_count`, `sg_factory_name`, `sg_factory_load` and `sg_factory_kit`; the page reads the bank back into its pattern and kit lists at load and plays a factory pattern from the document itself.

## Embedding in JIDAI RACK

To run SHOGUN as a rack device, compile one engine translation unit:

- `engine/shogun.cpp` (everything else in `engine/` and `engine/voices/` is headers and `.inc` tables);
- include directories `engine/` and `third_party/jidai-common/include` (the vendored shared headers, header-only);
- C++17, no other defines (`SHOGUN_NO_FORMAT` is only for the freestanding wasm build).

Entry points: `shogun::Engine` (`prepare(fs, os)`, `loadInit()`, `setParam`/`setParamNow`, `setHostTransport`, `setRunning`, `trigger`/`noteOn`/`noteOff`, `processSample(values, connected)` with the `kPorts` jack buffer, `mainL`/`mainR`/`aux`, `latencySamples`), the parameter table in `engine/params.h` (`kParams`, `findParam`) and the jack table in `engine/ports.h` (`kPortTable`, `findPort`, `resolvePort`). The factory bank and the state reader are header-only: include `engine/factory.h` (it includes `engine/patch.h` and `engine/factory_bank.inc`) in the device source; there is no extra file to compile. Saved state is `patchToJson` / `parsePatch`.

The panel is data: `plugin/layout/export_layout.py` records the spec mockups and binds every control, and writes `plugin/Source/PanelLayout.inc`. `scripts/check_panel_bindings.py` (`make panel-check`, run by `make test`, `make plugin` and `make probe-linux`) fails when an interactive op on any tab has no bind, or a bind kind the editor does not handle. Regenerate the layout after changing the binders: `python3 plugin/layout/export_layout.py`.

If the rack reuses the JUCE processor and panel instead of its own wrapper, add `plugin/Source/PluginProcessor.cpp` and `plugin/Source/PluginEditor.cpp` (with `plugin/Source/` on the include path for `PluginProcessor.h`, `PluginEditor.h` and `PanelLayout.inc`). `plugin/Source/Probe.cpp` is the test console only. `make strict` checks all of these with the rack's flags.

Rebuild the pack PDF from the markdown:

```
python3 scripts/build_design_pack_pdf.py
```

The script reads README.md, METHODOLOGY.md, SCHEMATICS.md, BUILD_GUIDE.md, TESTPLAN.md, and REPO_SETUP.md, in that order, and writes `docs/SHOGUN_Design_Pack.pdf`. It needs reportlab. It does not fetch audio code.

## Engine

| Piece | Job |
| --- | --- |
| Clock | INT and EXT, tempo, scale, the step counter that keeps running in EXT |
| Clock law | `forge/graphs/switch_law.json`. The switch chooses. Printed sample rows stay in the voice tests |
| Pattern | 14 drum tracks, 2 note tracks, lengths, shuffle, shift, mute, per-step flags |
| Voices | One block per voice in SCHEMATICS.md, cleared to 0 until triggered |
| Outs | The pair map, the main sum, master after the sum |
| Tests | The names in TESTPLAN.md, against the printed 48 kHz rows |

Host tempo wins while the host is playing. Otherwise the internal tempo, 60 to 180 BPM, default 120, is the clock. The display shows BPM and the step. EXT leaves both of those moving.

Trig jacks are rising edges through 1.0 V. In INT they are not read. In EXT they are the only triggers. Velocity on the edge is the accent in EXT. A gate with no velocity byte uses 127.

Live knobs apply in both modes. Pattern step on, flam, accent, and step bend apply only in INT.

## Placeholder plate (2026-10-06)

`make plugin` configures `plugin/` against the JUCE 8.0.4 tree already on this machine, then builds a universal (arm64 and x86_64) VST3 and a console probe. The deployment target is 11.0. Standalone is not a target on this machine: Command Line Tools do not include ibtool.

```
make plugin
make install-vst
```

`make install-vst` symlinks `~/Library/Audio/Plug-Ins/VST3/SHOGUN.vst3` to the Release artefact. FL Studio 2024 needs a plugin rescan after that link exists. The binary is an instrument, MIDI input is on, and the editor has a fixed 1200 by 672 logical size with a user scale.

The plate is flat greys and stock sliders. Each voice is one row: name, TRIG, level, and that voice's knobs. The name selects which voice the 16 step buttons edit. INT is the default and plays a beat: BD1 and bass on steps 1 and 9, snare on steps 5 and 13, hats on the eighths. EXT stays silent until TRIG or a MIDI note. MIDI notes 36 to 51 play BD1 through bass, in voice order. The clock parameter is the only writer of INT and EXT. TRIG and MIDI call the engine trigger path and leave the switch where it is.

The wrapper resamples the engine's 48 kHz stream to the host rate. Trig jacks stay unwired, so a note cannot act as a cable that forces EXT. Saved state is the parameter tree plus 16 step masks.

`make plugin` runs `ShogunProbe`, then ad-hoc signs the bundle so the module info stays inside the signature. The probe checks INT sound, EXT silence, MIDI and TRIG while EXT stays EXT, a bass note and its release, a 44.1 kHz host rate, a quiet tail after the steps are cleared, a restored step mask and clock, and that the plate paints. It writes `/tmp/shogun-plate.png` when given that path.

## Panel

The jobs may match the instrument chapter: the same knob names, the same 14 drums, the two note tracks, the INT/EXT switch, and a Trig jack on each voice. The face must not be a trace of the hardware panel. No maker's logo or mascot from the source instrument. Draw a new plate. Level and master stay off the pattern, as in both manuals.

The INT/EXT switch is a SHOGUN control. Putting it on the plate does not make it a copy of a control on the source instrument, because that machine does not have this switch.

## Outputs

Pairs, post level and post accent, before master:

- BD: left BD1, right BD2
- SD/RS: left snare, right rim
- HH/CY: left open plus closed, right cymbal
- CP: stereo clap
- TO/CO: LTC, MTC, and HTC at pans -0.7, 0, and 0.7
- CB/CL: left clave, right cowbell

Main is the sum of those contributions, plus maracas, lead, and bass at center pan, then the master. A patched pair does not remove its voice from the main.

## Limits

The voice is the equation set in SCHEMATICS.md. A STAND-IN number stays marked until a measurement replaces it. A sampled kit is outside this design. A transistor netlist is outside this design: the manuals did not provide one. JIDAI RACK does not compile this repository, and this guide does not add it.
