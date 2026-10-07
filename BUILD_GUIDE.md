# Build guide (2026-10-06)

This commit builds the voice engine and a PDF. It does not build a plugin.

## This tree

The voice engine is `engine/`. Panel art is unpainted. JIDAI RACK is not changed by this repository. The equations in [SCHEMATICS.md](SCHEMATICS.md) are the contract. `make test` builds `build/shogun_tests` and runs the checks in [TESTPLAN.md](TESTPLAN.md).

Rebuild the pack PDF from the markdown:

```
python3 scripts/build_design_pack_pdf.py
```

The script reads README.md, METHODOLOGY.md, SCHEMATICS.md, BUILD_GUIDE.md, TESTPLAN.md, and REPO_SETUP.md, in that order, and writes `docs/SHOGUN_Design_Pack.pdf`. It needs reportlab. It does not fetch audio code.

## Engine

| Piece | Job |
| --- | --- |
| Clock | INT and EXT, tempo, scale, the step counter that keeps running in EXT |
| Pattern | 14 drum tracks, 2 note tracks, lengths, shuffle, shift, mute, per-step flags |
| Voices | One block per voice in SCHEMATICS.md, cleared to 0 until triggered |
| Outs | The pair map, the main sum, master after the sum |
| Tests | The names in TESTPLAN.md, against the printed 48 kHz rows |

Host tempo wins while the host is playing. Otherwise the internal tempo, 60 to 180 BPM, default 120, is the clock. The display shows BPM and the step. EXT leaves both of those moving.

Trig jacks are rising edges through 1.0 V. In INT they are not read. In EXT they are the only triggers. Velocity on the edge is the accent in EXT. A gate with no velocity byte uses 127.

Live knobs apply in both modes. Pattern step on, flam, accent, and step bend apply only in INT.

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
