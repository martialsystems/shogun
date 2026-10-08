# SHOGUN design pack (2026-10-06)

**Working name SHOGUN. A fan instrument in the Jidai Collection. Design pack, voice engine, a GraphForge pin for the clock switch, and a placeholder plate.**

SHOGUN is a drum computer and two note voices, an original Martial Systems design inspired by classic analog drum machines. The jobs of the voices follow the design pack (its sources are cited in `METHODOLOGY.md`) and the design brief. The panel, when it is drawn, is a new face. This tree has the design pack, the voice engine in `engine/`, the clock-switch pin in `forge/`, and a placeholder VST3 under `plugin/`. Panel art is a later commit. The editor drawn now is a flat plate so the instrument can be loaded in FL Studio.

## Revisions

- 2026-10-06: first pack. Sources, clock bypass, voice equations, sequencer, tests, and the PDF of the same text.
- 2026-10-06: voice engine. Framework-free C++ in `engine/`, INT and EXT trigger paths, and the named tests in `tests/voices.cpp`.
- 2026-10-06: clock switch pinned under `forge/`. A Trig cable cannot force EXT. Printed 48 kHz rows stay named tests.
- 2026-10-06: placeholder VST3. Flat plate, knobs, levels, a 16-step grid, and TRIG buttons. `make plugin` builds a universal instrument. Panel art stays later.
- 2026-10-07: engine fixes and the web panel. Clock, conga, held-note and tie fixes listed in [BUGS.md](BUGS.md), with tests. `make web` compiles the engine to WebAssembly, checks it against the native build, and writes `web/shogun.html`.
- 2026-10-07: quiet drum voices end at 1e-6. BD1 Dist 0 is a bypass and drive is 9u above it. Main-only centre voices are equal power. Shuffle follows the clock step. Cymbal noise is a fixed mix. The pack states the rest rules, EXT lead and bass, and the CLK IN, RST IN, and RUN IN jacks.
- 2026-10-07: BD1 noise rides the body envelope, so a noisy kick ends with its body. The pan law is equal power: every centre, toms and clap tail included, is 0.707107 on each side.
- 2026-10-07: one decay law for every drum, tau = 8 ms * exp(4.5 u), and a voice is 0 under 1e-3. BD2 at Decay 127 holds; a tom at Decay 127 rings 4 s and ends. The snare bend is its own drop to Tune with an 80 ms floor.
- 2026-10-07: track solo. The soloed voice alone reaches its pair and the main; the others keep running at 0. Mute wins.
- 2026-10-07: a new engine loads the init kit and pattern. STAND-IN knob values set by ear. reset() still clears.
- 2026-10-07: factory patterns and kits cleared for now. The engine, plugin and web page start on INIT, an empty bar, with the INIT kit. New factory content will be written later.

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
| plugin/ | Placeholder VST3. JUCE wraps the engine. The plate is stock controls |
| Makefile | `make test` builds the engine checks and runs the clock-switch law. `make plugin` builds the VST3 |
| forge/ | GraphForge pin for the clock switch. Printed sample rows stay in `tests/voices.cpp` |
| docs/SHOGUN_Design_Pack.pdf | The same documents in one PDF |
| BUGS.md | Engine bugs fixed, engine notes, and open questions on the pack |
| web/ | The playable panel. `web/wasm/` wraps the engine for the browser, `web/page/` is the panel source, `web/shogun.html` is the built page |

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

SHOGUN is an original Martial Systems design inspired by classic analog drum machines. Martial Systems is not affiliated with or endorsed by any drum machine maker. The design pack quotes short phrases from published manuals for the parameter disputes and cites them in `METHODOLOGY.md`. It does not reproduce the manuals, and it does not use any maker's logo or marks.
