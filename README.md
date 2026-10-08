# SHOGUN design pack (2026-10-06)

**Working name SHOGUN. A fan instrument in the Jidai Collection. Design pack, voice engine, a GraphForge pin for the clock switch, and a placeholder plate.**

SHOGUN is a drum computer and two note voices, an original Martial Systems design inspired by classic analog drum machines. The jobs of the voices follow the design pack (its sources are cited in `METHODOLOGY.md`) and the design brief. The panel, when it is drawn, is a new face. This tree has the design pack, the voice engine in `engine/`, the clock-switch pin in `forge/`, and a placeholder VST3 under `plugin/`. Panel art is a later commit. The editor drawn now is a flat plate so the instrument can be loaded in a DAW.

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
- 2026-10-07: density pass. Shaped bodies (tanh(k sin)) on BD1, BD2, SD, and the toms, BD2 and tom slow FM, a 1 ms BD1 click, SD noise through a ducked 4-pole, six-square metal stacks with a band-pass on the hats and cymbal, and clap bursts fixed at 3 ms with a delayed filtered tail. STAND-IN values set by ear.
- 2026-10-07: Wave on BD1, BD2, and the three toms: a six-cell wave folder on the body oscillator, CC 0 bypass, default CC 32, g 0.5 to 4. Dist stays after Wave. Hats, clap, cymbal, and maracas have no Wave.
- 2026-10-08: **redesign v2.2, first pass** (branch `redesign/shogun`, spec `SHOGUN_Redesign.md` v2.2). The engine is rebuilt per §15.0: host-rate DSP blocks with one oversampling domain (1×/2×/4×, latency 0/23/26), the triple wave shaper (the shared `jidai::dsp::TripleShaper` behind `engine/wave_shaper.h`), new voices, the mod matrix and 4 LFOs, 454 float parameters with `SECTION:LABEL` ids, and the 153-jack bay (§13.2). The plugin runs the engine at the host rate (resampler removed), locks to the host transport, and draws the 8-tab 1200 × 672 panel with rack ears from the spec mockups. The web build compiles the same engine at the AudioContext rate. Numbers in [TESTPLAN.md](TESTPLAN.md). Not yet: room reverb, LANE A/B editor, factory content.
- 2026-10-08: the shared Jidai headers are vendored under `third_party/jidai-common/` (jidai-collection `redesign/jidai` at 9d6e382; see `VENDORED.md`). The JCS detector, pitch and roles, the TripleShaper stages, LevelComp, DcBlocker and the 93-tap halfband come from there. SHAPE, PRE-VCA and the v2.1 migration stay SHOGUN-only in `engine/wave_shaper.h`.

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
| third_party/jidai-common/ | Vendored shared Jidai headers (JCS v1.1, `jidai::dsp`). Do not edit; see `VENDORED.md` |
| tests/ | The named checks (`blocks.cpp`, `voices.cpp`, `engine.cpp`, `mod.cpp`), compiled by the Makefile |
| plugin/ | The VST3 (JUCE 8.0.4). `plugin/layout/` exports the spec mockups to the editor's op table (`plugin/Source/PanelLayout.inc`); `ShogunProbe` checks the shell and renders the 8 tabs |
| Makefile | `make test` / `make asan` run the engine checks (and the clock-switch law). `make plugin` builds the macOS VST3, `make probe-linux` the Linux VST3 + probe against `/workspace/JUCE`, `make web` the wasm page |
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
