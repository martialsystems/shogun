# Build guide (2026-10-06)

This tree builds the voice engine, the design-pack PDF, and a placeholder VST3. `make test` stays on the engine and the clock-switch law. It does not compile JUCE.

## This tree

The voice engine is `engine/`. Panel art is unpainted. The editor in `plugin/` is a placeholder plate for listening in a host. JIDAI RACK is not changed by this repository. The equations in [SCHEMATICS.md](SCHEMATICS.md) are the voice contract. `make test` builds `build/shogun_tests`, runs the checks in [TESTPLAN.md](TESTPLAN.md), and runs the clock-switch law. That law test imports GraphForge from `~/graphforge/src`.

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

`make install-vst` symlinks `~/Library/Audio/Plug-Ins/VST3/SHOGUN.vst3` to the Release artefact. FL Studio 2024 needs a plugin rescan after that link exists. The binary is an instrument, MIDI input is on, and the editor is fixed at 980 by 640.

The plate is flat greys and stock sliders. Each voice is one row: name, TRIG, level, and that voice's knobs. The name selects which voice the 16 step buttons edit. INT is the default and plays a beat: BD1 and bass on steps 1 and 9, snare on steps 5 and 13, hats on the eighths. EXT stays silent until TRIG or a MIDI note. MIDI notes 36 to 51 play BD1 through bass, in voice order. The clock parameter is the only writer of INT and EXT. TRIG and MIDI call the engine trigger path and leave the switch where it is.

The wrapper resamples the engine's 48 kHz stream to the host rate. Trig jacks stay unwired, so a note cannot act as a cable that forces EXT. Saved state is the parameter tree plus 16 step masks.

`make plugin` runs `ShogunProbe`, then ad-hoc signs the bundle so the module info stays inside the signature. The probe checks INT sound, EXT silence, MIDI and TRIG while EXT stays EXT, a bass note and its release, a 44.1 kHz host rate, a quiet tail after the steps are cleared, a restored step mask and clock, and that the plate paints. It writes `/tmp/shogun-plate.png` when given that path.

## Panel

The jobs may match the instrument chapter: the same knob names, the same 14 drums, the two note tracks, the INT/EXT switch, and a Trig jack on each voice. The face must not be a trace of the hardware panel. No MFB logo. No dancing-bear mark. Draw a new plate. Level and master stay off the pattern, as in both manuals.

The INT/EXT switch is a SHOGUN control. Putting it on the plate does not make it a copy of a Tanzbär control, because that machine does not have this switch.

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
