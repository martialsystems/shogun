

# Shogun

**A sixteen-voice analog-style drum machine with a full patch bay, from Martial Systems.**

SHOGUN has fourteen drum voices and two synth voices, a step sequencer for every voice, and a 151-jack patch bay that cables to the other instruments in the Jidai Collection. It runs as a VST3 instrument in your DAW and as a playable web page built from the same engine.

## Download:

- [Mac](https://github.com/martialsystems/shogun/releases/latest/download/SHOGUN-macOS.zip)
- [Windows](https://github.com/martialsystems/shogun/releases/latest/download/SHOGUN-Windows.zip)
  
- [Jidai Collection](https://github.com/martialsystems/jidai-collection)

**Found a bug?** Please [open a GitHub issue](https://github.com/martialsystems/shogun/issues/new/choose) and fill in the bug report form.

**Download the Manual** [Here.](https://github.com/martialsystems/shogun/blob/main/docs/manual/SHOGUN_Manual.pdf)

## Features

### Voices

- **Fourteen drums:** two kicks (BD1, BD2), snare (SD), rim (RS), clap (CP), claves (CL), maracas (MA), cowbell (CB), closed and open hats (CH, OH) with choke, cymbal (CY), and three toms that switch to congas (LTC, MTC, HTC).
- **Two synth voices:** LEAD with a state-variable filter and BASS with a 4-pole ladder filter, both with glide and ties.
- **Triple wave shaper** on the kicks and toms, with SHAPE morph, symmetry, an optional pre-VCA position and level compensation. At 0 it is a true bypass, whatever the symmetry.
- **Drive** with alias-suppressed saturation, analog drift and a per-unit tolerance you can save with the patch.
- **Oversampling** at 1×, 2× or 4× (0, 23 or 26 samples of latency, reported to the host), with a separate setting for offline renders.

### Sequencer

- Per-track length from 1 to 32 steps and per-track scale (1/32, 1/16, 1/8T, 1/8) for polymeter.
- Per-track swing and shift. Per-step accent (3 levels), probability, micro-timing, flam (16 types), ratchets (up to 8), pitch bend, and notes and ties for the synth voices.
- Parameter locks: a patch can lock any parameter to its own value on a step (the factory patterns use them). The GRID tab shows the selected step's locks and clears them.
- Clock from the host, the internal clock or the CLK IN jack, picked with the SRC key in the header. In HOST mode SHOGUN locks to your DAW's song position, including after loops and jumps.
- Copy, paste, clear, rotate, randomise, panic and INIT patch.

### Modulation

- Four LFOs, each either global or running a separate instance per voice (OWN VOICE), with host sync, sample and hold with slew, and declicked shapes.
- A 32-row mod matrix. Sources include the LFOs, each voice's envelope, pitch envelope, velocity, accent and per-hit random, plus note, mod wheel and aftertouch. Any continuous parameter can be a target.

### Mixer

- Equal-power pan, mute and solo for every voice.
- Routing to the main mix, four buses (drive, tilt tone, compressor with sidechain) or eight stereo aux outputs.
- Master section with drive, glue compressor, stereo width with mono bass, a tempo-synced delay send, and a soft clipper with an adjustable ceiling.

### Patch bay

- 151 jacks: trigger, velocity, pitch, decay, tone, return, output and envelope for every drum. Gate, velocity, note, V/oct, cutoff and note out for each synth voice. Clock and transport in and out, accent out, the four LFOs, random and the mix outputs.
- Jacks follow the Jidai Collection standard: 1 V/oct with C3 at 0 V, ±5 V signals and colour-coded cable roles. SHOGUN's clock out can drive BUSHIDO directly, and any SHOGUN output can be cabled into RONIN.
- Patches saved with older jack names still load with every cable in place.

### Panel

- Eight tabs (MAIN, VOICE, GRID, MOD, SEQ/MIDI, FX/MIX, ROUTE, GLOBAL) on a 1200 × 672 panel with rack ears, scalable from 75 % to 200 % for HiDPI screens.
- KIT and PATTERN arrows step through INIT and the factory bank, and the search key opens the program list.
- A/B compare (two full snapshots, either copied onto the other) and 64 levels of undo and redo over knob moves, step edits and kit loads.
- Every key on the panel does something: the build checks that no control is left unbound.

### MIDI

- Drums on notes 36 to 49 (BD1 to HTC). LEAD on channel 1 and BASS on channel 2, with MIDI note 48 at C3.
- Twelve voice controls on fixed MIDI CCs (the map is on the SEQ/MIDI tab), the mod wheel on CC 1 and channel aftertouch as mod sources, and a velocity curve (linear, soft, hard or fixed).

## Install and build

SHOGUN builds from source. You need CMake, a C++17 compiler and JUCE 8.0.4. Put JUCE 8.0.4 next to this repository (`../JUCE`) or set `JUCE_DIR` to your JUCE checkout (`make plugin-linux JUCE_DIR=/path/to/JUCE`; `JUCE_DIR=fetch` downloads it).

| Command | What it does |
| --- | --- |
| `make plugin` | Builds the universal macOS VST3 (Apple silicon and Intel) and signs it for local use |
| `make install-vst` | Builds and links `SHOGUN.vst3` into `~/Library/Audio/Plug-Ins/VST3` |
| `make plugin-linux` | Builds the Linux VST3 |
| `make test` | Runs the engine test suite |
| `make factory` | Rebuilds the factory bank (`engine/factory_bank.inc`) from `scripts/make_factory.py`, trimming each kit to its peak target |
| `make web` | Compiles the engine to WebAssembly (clang with wasm-ld, plus Node) and writes the playable page `web/shogun.html` |

SHOGUN is built for VST3 hosts such as FL Studio. A new instance starts on the INIT kit with an empty pattern. The factory bank follows INIT: 21 kits, each with its own pattern, from house, techno, acid and electro to breakbeat, jungle, half-time and odd meters. Pick one from your host's program list, with the KIT or PATTERN arrows and displays on the panel, or from the web page's pattern list. A pattern saved on the web page keeps its whole patch document, so step probability, micro-timing, ratchets, parameter locks and mod rows the page has no controls for are kept.

## Documentation

- [SHOGUN User Manual](docs/manual/SHOGUN_Manual.md) ([PDF](docs/manual/SHOGUN_Manual.pdf)): the panel, patching, MIDI, factory kits, DAW setup and the JIDAI RACK back panel
- [SCHEMATICS.md](SCHEMATICS.md): the signal flow and equations for every voice
- [TESTPLAN.md](TESTPLAN.md): the named tests and the numbers they check
- [BUILD_GUIDE.md](BUILD_GUIDE.md): engine layout and build notes
- [METHODOLOGY.md](METHODOLOGY.md): the design notes behind the voice choices
- [CHANGELOG.md](CHANGELOG.md): release history
- `docs/SHOGUN_Design_Pack.pdf`: the design documents in one PDF, rebuilt with `python3 scripts/build_design_pack_pdf.py`

## The Jidai Collection

SHOGUN uses the same jack names, voltages and cable colours as the rest of the collection, so a cable from one instrument works the same way on another.

- **BUSHIDO**: a 3 × 12 analog step sequencer with real patch cables. [github.com/martialsystems/bushido](https://github.com/martialsystems/bushido)
- **RONIN**: a semi-modular synthesizer you patch as an effect. [github.com/martialsystems/Ronin](https://github.com/martialsystems/Ronin)
- **ORIGAMI**: a west-coast triple wave folder, as a standalone effect and as a JIDAI RACK device. [github.com/martialsystems/origami](https://github.com/martialsystems/origami)
- **JIDAI RACK**: the collection in one rack, in one plugin. [github.com/martialsystems/jidai-collection](https://github.com/martialsystems/jidai-collection)

## Legal

Copyright © 2026 Martial Systems LLC. All rights reserved. See `LICENSE`.

SHOGUN is an original Martial Systems design inspired by classic analog drum machines. Martial Systems is not affiliated with or endorsed by any drum machine maker.
