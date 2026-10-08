# SHOGUN

**A sixteen-voice analog-style drum machine with a full patch bay, from Martial Systems.**

SHOGUN has fourteen drum voices and two synth voices, a step sequencer for every voice, and a 153-jack patch bay that cables to the other instruments in the Jidai Collection. It runs as a VST3 instrument in your DAW and as a playable web page built from the same engine.

## Features

### Voices

- **Fourteen drums:** two kicks (BD1, BD2), snare (SD), rim (RS), clap (CP), claves (CL), maracas (MA), cowbell (CB), closed and open hats (CH, OH) with choke, cymbal (CY), and three toms that switch to congas (LTC, MTC, HTC).
- **Two synth voices:** LEAD with a state-variable filter and BASS with a 4-pole ladder filter, both with glide and ties.
- **Triple wave shaper** on the kicks and toms, with SHAPE morph, symmetry, an optional pre-VCA position and level compensation. At 0 it is a true bypass.
- **Drive** with alias-suppressed saturation, analog drift and a per-unit tolerance you can save with the patch.
- **Oversampling** at 1×, 2× or 4× (0, 23 or 26 samples of latency, reported to the host), with a separate setting for offline renders.

### Sequencer

- Per-track length from 1 to 32 steps and per-track scale (1/32, 1/16, 1/8T, 1/8) for polymeter.
- Per-track swing and shift. Per-step accent (3 levels), probability, micro-timing, flam (16 types), ratchets (up to 8), pitch bend, and notes and ties for the synth voices.
- Parameter locks: hold a step and turn any knob to store a value for that step.
- Clock from the host, the internal clock or the CLK IN jack. In HOST mode SHOGUN locks to your DAW's song position, including after loops and jumps.
- Fill, copy, paste, clear, rotate, randomise, panic and INIT patch.

### Modulation

- Four LFOs, each either global or running a separate instance per voice (OWN VOICE), with host sync, sample and hold with slew, and declicked shapes.
- A 32-row mod matrix. Sources include the LFOs, each voice's envelope, pitch envelope, velocity, accent and per-hit random, plus note, mod wheel and aftertouch. Any continuous parameter can be a target.

### Mixer

- Equal-power pan, mute and solo for every voice.
- Routing to the main mix, four buses (drive, tilt tone, compressor with sidechain) or eight stereo aux outputs.
- Master section with drive, glue compressor, stereo width with mono bass, a tempo-synced delay send, and a soft clipper with an adjustable ceiling.

### Patch bay

- 153 jacks: trigger, velocity, pitch, decay, tone, return, output and envelope for every drum. Gate, velocity, note, V/oct, cutoff and note out for each synth voice. Clock and transport in and out, accent out, the four LFOs, random and the mix outputs.
- Jacks follow the Jidai Collection standard: 1 V/oct with C3 at 0 V, ±5 V signals and colour-coded cable roles. SHOGUN's clock out can drive BUSHIDO directly, and any SHOGUN output can be cabled into RONIN.
- Patches saved with older jack names still load with every cable in place.

### Panel

- Eight tabs (MAIN, VOICE, GRID, MOD, SEQ/MIDI, FX/MIX, ROUTE, GLOBAL) on a 1200 × 672 panel with rack ears, scalable for HiDPI screens.

### MIDI

- Drums on notes 36 to 49 (BD1 to HTC). LEAD on channel 1 and BASS on channel 2, with MIDI note 48 at C3.
- MIDI CC for the voice controls, the mod wheel on CC 1, and channel aftertouch.

## Install and build

SHOGUN builds from source. You need CMake, a C++17 compiler and JUCE 8.0.4. Set `JUCE_SRC` to your JUCE checkout if it isn't in the default place.

| Command | What it does |
| --- | --- |
| `make plugin` | Builds the universal macOS VST3 (Apple silicon and Intel) and signs it for local use |
| `make install-vst` | Builds and links `SHOGUN.vst3` into `~/Library/Audio/Plug-Ins/VST3` |
| `make plugin-linux` | Builds the Linux VST3 |
| `make test` | Runs the engine test suite |
| `make web` | Compiles the engine to WebAssembly (clang with wasm-ld, plus Node) and writes the playable page `web/shogun.html` |

SHOGUN is built for VST3 hosts such as FL Studio. A new instance starts on the INIT kit with an empty pattern. Factory kits and patterns are coming in a later release.

## Documentation

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
- **JIDAI RACK**: the collection in one rack, in one plugin. [github.com/martialsystems/jidai-collection](https://github.com/martialsystems/jidai-collection)

## Legal

Copyright © 2026 Martial Systems LLC. All rights reserved. See `LICENSE`.

SHOGUN is an original Martial Systems design inspired by classic analog drum machines. Martial Systems is not affiliated with or endorsed by any drum machine maker.
