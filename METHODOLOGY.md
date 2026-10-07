# Methodology (2026-10-06)

## Status

This is a stand-in design for a fan instrument. The voices are analog blocks: oscillators, envelopes, noise, and a clip. Hats and cymbals are square or metallic stacks plus noise. No sample is in the voice model. No coefficient is a measured part value. Each one is marked STAND-IN in [SCHEMATICS.md](SCHEMATICS.md).

No service schematic was found in public, and none is invented here. A block diagram is the farthest the circuit claim goes.

## Sources

Read on 2026-10-06.

1. German primary. *MFB Tanzbär Bedienungsanleitung*, 17 PDF pages, two printed pages on most sheets, printed page numbers through 31. PDF metadata: CreationDate D:20131024121806+02'00, Creator Adobe InDesign CS5 (7.0), Producer Adobe PDF Library 9.9. Text extracted from the file published at `https://media.djmania.net/manuales/pdf/Manual_MFB_Tanzbar.pdf`. The MIDI appendix is printed page 31 of this file. That appendix is the control-change list used below.
2. English primary. *MFB Tanzbär User Manual*, the 17-page English edition with the same contents list (overview through MIDI implementation). Printed pages quoted below were read from `https://www.manualshelf.com/manual/mfb/tanzb-ar/user-manual-english/page-3.html` and the neighbouring pages of that edition (pages 4, 6, 8, 9, 11). A separate English PDF file was not downloaded. The contents list and the instrument chapter match the German edition's structure.
3. Published control-change republication. `https://midi.guide/d/mfb/tanzbar-1/`, last update 12 September 2024, 64 parameters. This is the list identified by BD1 Attack on CC 2, Decay on 64, Pitch on 65, Tune on 3, Noise on 4, Filter on 5, Dist on 6, and Trigger on 66. Where it disagrees with the 2013 appendix, the appendix wins and the row is marked.

A secondary web conversion at `https://manuals.plus/mfb/mfb-tanzbar-analog-drum-machine-manual` was read only for the Data-versus-Sound conflict named in the brief. It is not a primary.

Tanzbär 2, Tanzmaus, and Tanzbär Lite were not used.

## Marks

- STAND-IN: a number chosen so the equation is definite. It is not a capture.
- CHOICE: the sources, or the sources plus the brief, support this reading, and the pack uses it.
- ASSUMED: the sources disagree, or they are silent, and a single reading is still required.

## Parameter names

Both primary instrument chapters agree on bass drum 1.

English, printed pages 14 to 15: Attack is the level of the attack transients. Decay is volume decay time. Pitch is time and modulation intensity of the pitch envelope. Tune is pitch. Noise is noise level. Filter is the sound of the noise signal. Data is distortion level (printed "Distorion"). Sound selects 1 of 16 different attack transients.

German, printed pages 14 to 15: Data "regelt die Intensität des Verzerrers". Sound: "16 verschiedene Attack-Transienten stehen zur Auswahl". Filter "beeinflusst den Klang des Rauschanteils".

The 2013 appendix names the same jobs as BD1_DIST on CC 6 and BD1_TRIGGER on CC 66. It has no row named Sound.

The secondary conversion says BD1 Data is "Sound of noise signal". That sentence swaps Data with the job both primaries give to Filter.

CHOICE: Sound, which the appendix calls BD1_TRIGGER, selects the transient, index 0 to 15. Data, BD1_DIST, is the clip. Filter stays on the noise.

## Quoted disputes

### Individual outputs, overview against the jack list

English overview, printed page 4: "Individual outs (in pairs except claps)."

German overview, printed page 4: "Einzelausgänge (bis auf Bassdrum und Clap paarweise ausgeführt)."

Both rear-panel lists then say: BD left is bass drum 1 and BD right is bass drum 2. SD/RS is snare and rim. HH/CY left is open and closed hat, right is cymbal. The clap out is stereo, with the attack transients spread in the field. TO/CO spreads the three toms or congas. CB/CL left is clave, right is cowbell.

The two overview sentences disagree with each other. The jack lists agree with each other and with the brief.

CHOICE: follow the jack list. Maracas, the lead voice, and the bass voice are not on that list. They go to the main mix only.

### Cable in an individual out

English printed page 7: "If you plug in a cable on an instrument out, the sound is erased from the main out."

German printed page 7: "Wird ein Kabel in einen Einzelausgang gesteckt, werden die betreffenden Sounds im Hauptausgang gelöscht."

The two primaries agree: the hardware removes those sounds from the main output.

CHOICE for SHOGUN: an individual out is a tap. The voice stays in the main mix. The hardware sentence is recorded so the difference is explicit. [TESTPLAN.md](TESTPLAN.md) locks it as `testIndividualOutStaysInMix`.

### Seventeen names, fourteen tracks

Both overviews say 17 drum instruments and 14 tracks that trigger drum instruments. The instrument chapter lists 14 drum names: BD1, BD2, SD, RS, CY, OH, HH, CL, CP, LTC, MTC, HTC, CB, MA. The two note tracks are extra.

ASSUMED: each tom track has a tom mode and a conga mode, and the overview counts those as six names on three tracks. 11 other drums plus 6 is 17 names on 14 tracks. The manuals do not write that sum.

### Tom and conga buttons

English, printed pages 15 to 16, gives LTC, MTC, and HTC the same buttons: step button 12 toggles tom and conga, step button 13 enables noise.

German, printed pages 15 to 16: LTC uses the lit LEDs 12 and 13, MTC uses 13 and 14, HTC uses 14 and 15.

The appendix gives each voice its own noise switch and its own tom/conga switch, plus one shared TOM_NOISE on CC 84.

ASSUMED: the voice model uses the per-voice switches and the shared level. The button numbers are a panel procedure, and the two editions do not agree, so they are not part of the equation.

### Track length

German overview: "Länge der Spur (1 bis 32 Steps)."

German record chapter, printed page 23: each track may have an individual length "zwischen einem und 16 Steps".

English overview: step number 1 to 32, with four scale settings.

CHOICE: pattern length and per-track length are integers from 1 to 32, which is the overview and the brief. Steps 1 to 16 are the A half. Steps 17 to 32 are the B half. The "1 to 16" sentence matches the sixteen step buttons on the A half.

### Scale and length

Both manuals tie scale to a default pattern length: 32nd notes give 32 steps, 16th-note triplets give 24, 16ths give 16, 8th-note triplets give 12. Measure can then set another count. The bar still takes the same time, so a 32nd scale runs the sequencer twice as fast as a 16th scale.

The brief asks for scale as step length, and for patterns of 1 to 32 steps as their own setting.

CHOICE: in SHOGUN, scale sets only how many steps fit in a quarter note. Pattern length is a separate integer from 1 to 32. The hardware coupling is not copied. This split is ASSUMED relative to a machine whose scale button also picked the length.

### Cowbell resolution

Both instrument chapters: Data offers 16 tunings, and Sound is the decay time.

Appendix: CB_Tune on CC 85, value 0 to 127, and CB_Decay on CC 86, value 0 to 127.

ASSUMED: Tune and Decay are continuous 0 to 127, under the appendix names. A 16-step control is a quantizer of Tune, step i mapping to CC round(i × 127 / 15). The brief's two detuned squares are the oscillator. The manuals do not name the waveform.

### Clap type and count

Both instrument chapters: Decay is the tail, Filter is colour, Attack is the level of the attack transients, Data is the number of those transients, Sound is 16 different transients.

The record chapter's hidden-parameter paragraph names BD1, the toms and congas, and the cowbell. It does not name the clap. English printed page 20 and German printed page 20 both still say flam cannot be programmed on Clap, CV1, or CV2/3.

Appendix: CP_DECAY 75, CP_FILTER 18, CP_ATTACK 76, CP_TRIGGER 77. No row is named count, and no row is named Sound.

ASSUMED: CP_TRIGGER is the type, on the same pattern as BD1_TRIGGER. The count is Data, an integer from 1 to 8, and it has no published CC. The record paragraph's shorter instrument list is treated as incomplete next to the instrument chapter.

### Set select and one track-delay label

2013 appendix: Set Select, controller 0, value 0 to 2.

midi.guide: Set Select has no controller number, and the range column says 0 to 127.

2013 appendix: Track Delay HH, controller 97.

midi.guide labels controller 97 "Track Track HH". The number matches.

CHOICE: controller 0 with values 0, 1, and 2, and the name Track Delay HH for 97.

## Clock bypass

INT and EXT are SHOGUN. They are not a switch in the Tanzbär manuals. Those manuals have MIDI clock, an analog sync jack that can be an input or an output, and a manual-trigger mode. None of those is this switch.

The switch is the only control that selects the master.

- INT: the internal sequencer triggers the voices. Its tempo is the host tempo while the host is playing, otherwise its own tempo. A cable in a Trig jack does not fire the voice.
- EXT: the internal sequencer does not trigger the voices. Step on and off, flam, and accent from the pattern are ignored for triggering. Knob values still apply. Each voice fires from its own Trig jack. Accent may arrive as velocity on that jack.
- EXT does not stop the clock. The step counter and the clock display keep advancing. Switching back to INT connects the triggers to the step the clock has reached.
- A cable in a Trig jack does not force EXT.

ASSUMED: "resumes" means the counter never froze. The alternative, a frozen counter that continues from the step where EXT was engaged, would stop the clock the brief says keeps running.

## Voices the manuals do name

The instrument chapter gives the jobs below. Ranges and waveforms that the chapter does not give are in [SCHEMATICS.md](SCHEMATICS.md), marked there.

- BD1: attack level, decay, pitch envelope time and depth on one knob, tune, noise level, noise filter, distortion, and 1 of 16 attack transients.
- BD2: decay up to a steady tone, tune, and tone as the attack level. The chapter does not give BD2 a noise, a filter, a distortion, or 16 transient types. It is a second kick.
- SD: tune shared by two tones, detune of tone 2, snappy noise level, noise decay, blend of the two tones, decay of both tones, and pitch-envelope intensity.
- RS: pitch only. The brief asks for a short pitched click.
- CY: decay, blend of both signals, and pitch or colour.
- OH and HH: separate decays, and one pitch or colour shared by both. The appendix exposes that colour as HH_TUNE on CC 73. There is no OH tune row.
- CL: tune and decay.
- CP: tail decay, filter, attack level, a count of attack transients, and 16 transient types. The clap out spreads those transients. The brief adds the spacing, about 10 to 12 ms, which the manuals do not print.
- LTC, MTC, HTC: tune, decay up to a steady tone, noise on or off, tom or conga, and one noise level for all three. Bend is available. The tom out spreads the three.
- CB: tune and decay in the appendix. The brief specifies two detuned squares.
- MA: decay only. The brief specifies filtered noise.
- Lead and bass: the overview says one parameter each. Bass Data is "Filter cutoff or CV 3 value" in the English chapter. Setup plays a steady 440 Hz and tunes both voices with Data. The waveform is not named. ASSUMED waveform: a band-limited saw for each. ASSUMED lead parameter: a one-pole tone, because the chapter names one parameter and does not name the lead knob.

Closed hat kills open hat. The pages read do not state a choke time. The brief requires the choke, so the pack defines it: the closed-hat trigger clears the open-hat envelope on that sample.

## Sequencer the manuals do name

- Patterns live in 3 sets of 3 banks of 16, which is 144. That memory map is the hardware's. SHOGUN stores patterns without a 144-slot cap in this pack. The set-select controller is still defined.
- A and B. A pattern longer than 16 steps has a B half, and the machine walks it. A/B can also alternate. Copy of A onto B exists on the hardware. Chain is runtime only: the English overview calls it a fill that is not storable, and the German overview says the chain is "nicht speicherbar".
- Per track: length, shuffle, shift, mute. Shuffle in record mode is per track, 16 intensities. Shuffle in play mode is global and does not replace the stored per-track values when it is released. Shift is controllers 89 to 104, described as a small delay of the whole track. No millisecond range is printed.
- Per step, drums: on or off, one of three accent levels (off or soft, green or medium, red or loud), flam on 16 patterns, bend where the instrument has it, and an extra sound value where the instrument chapter has one. Bend instruments are BD1, BD2, SD, LTC, MTC, and HTC. Flam is unavailable on clap and on both note tracks.
- Per step, note tracks: on or off, pitch over three octaves, accent on the bass track, and a second value on the bass track that is CV3 and the internal bass cutoff. A/B enters a silent step. Select ties steps into a longer note.
- Level pots and the master volume are not stored in the pattern.
- Manual record mode shares one knob set across steps. Accents and flams can still differ per step. Step and jam modes can store different knob values per step.

## Tempo

English play chapter: the tempo knob covers approximately 60 to 180 BPM, and there is no tempo readout.

CHOICE: SHOGUN's internal tempo uses those endpoints as exact bounds, default 120. The clock display is part of SHOGUN. The hardware has no readout.

## Gate level

A Trig jack is a rising edge through 1.0 V on a gate that rests at 0 V and is high at 5 V. That 1 V threshold is the collection's existing gate rule between RONIN and BUSHIDO. Velocity, when the sender has one, is an integer 0 to 127 riding with the edge. A plain gate with no velocity uses 127.

ASSUMED: the plain-gate default of 127. The Tanzbär manuals say drum notes carry velocity. They do not define a jack that has no velocity byte.
