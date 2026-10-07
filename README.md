# SHOGUN design pack (2026-10-06)

**Working name SHOGUN. A fan instrument in the Jidai Collection. Design pack, voice engine, and a GraphForge pin for the clock switch.**

SHOGUN is a drum computer and two note voices, specified from the first MFB Tanzbär: the 17-page English user manual, the matching German Bedienungsanleitung, and that instrument's MIDI control-change list. The jobs of the voices follow those sources and the design brief. The panel, when it is drawn, is a new face. This tree has the design pack, the voice engine in `engine/`, and the clock-switch pin in `forge/`. Panel art is a later commit.

## Revisions

- 2026-10-06: first pack. Sources, clock bypass, voice equations, sequencer, tests, and the PDF of the same text.
- 2026-10-06: voice engine. Framework-free C++ in `engine/`, INT and EXT trigger paths, and the named tests in `tests/voices.cpp`.
- 2026-10-06: clock switch pinned under `forge/`. A Trig cable cannot force EXT. Printed 48 kHz rows stay named tests.

## What this is

A stand-in design. Every coefficient in [SCHEMATICS.md](SCHEMATICS.md) is marked STAND-IN. Where the English manual, the German manual, and the control-change list disagree, both wordings are quoted and the pick is marked ASSUMED or CHOICE. No service schematic was in the sources. The equations are functional blocks, not a transistor circuit.

The instrument this pack describes:

- 14 drum tracks and 2 note tracks
- An internal sequencer with patterns of 1 to 32 steps
- A clock switch, INT or EXT, that connects or disconnects that sequencer from the voice triggers. The switch chooses. INT ignores Trig jacks. EXT ignores the pattern. A plugged Trig cable does not force EXT.
- Paired outputs plus a main mix

## Files

| File | Role |
| --- | --- |
| README.md | This front page |
| METHODOLOGY.md | Sources, quotations, and the choices |
| SCHEMATICS.md | Block, sample equation, knob range, and a 48 kHz example per voice |
| BUILD_GUIDE.md | Engine layout, PDF build, and the panel rule |
| TESTPLAN.md | Named tests and the numbers they lock |
| REPO_SETUP.md | Tree, git, and the collection link |
| engine/ | Voice engine, clock, pattern, and outs. No JUCE |
| tests/voices.cpp | The named checks, compiled by the Makefile |
| Makefile | Builds `build/shogun_tests` and runs the clock-switch law |
| forge/ | GraphForge pin for the clock switch. Printed sample rows stay in `tests/voices.cpp` |
| docs/SHOGUN_Design_Pack.pdf | The same documents in one PDF |

Rebuild the PDF with:

```
python3 scripts/build_design_pack_pdf.py
```

## The Jidai Collection

BUSHIDO and RONIN share one patch format and one cable feel. JIDAI RACK compiles those two engines. SHOGUN's engine is in this repository. It is not compiled into that rack.

- **BUSHIDO**: a 3 x 12 analog step sequencer with real patch cables. [github.com/martialsystems/bushido](https://github.com/martialsystems/bushido)
- **RONIN**: a semi-modular synthesizer you patch as an effect. [github.com/martialsystems/Ronin](https://github.com/martialsystems/Ronin)
- **JIDAI RACK**: BUSHIDO and RONIN in one rack, in one plugin. [github.com/martialsystems/jidai-collection](https://github.com/martialsystems/jidai-collection)

## Legal

Copyright © 2026 Martial Systems LLC. All rights reserved. See `LICENSE`.

MFB and Tanzbär are names of their owner. Martial Systems is not affiliated with or endorsed by that owner. This pack quotes short phrases for the parameter disputes. It does not reproduce the manuals, and it does not use that maker's logo or dancing-bear mark.
