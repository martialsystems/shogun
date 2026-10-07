# SHOGUN bugs and open questions (2026-10-07)

Found while putting the engine from `feat/engine-voices` behind the web panel. Checked three ways: reading `engine/` against SCHEMATICS.md, probe runs of the native build, and a sample by sample diff against a second engine written separately from SCHEMATICS.md. Every voice matched that second build to 1e-9, except the conga partial (E3) and BD1 Dist, where the second build had made a different call on S1.

## Engine bugs fixed on this branch

Each has a regression test in `tests/voices.cpp` (`testClockFixes`, `testVoiceFixes`).

| | Bug | What you heard | Fix |
| --- | --- | --- | --- |
| E1 | A step was queued after the sample that holds its boundary. `fireDue()` only fired `when == sampleIndex_`, so the event was never due and stayed live. After 512 stranded events the queue was full and nothing could be scheduled. | Only tempos with a whole-sample period worked (120, 125, 150 BPM). At 130 BPM 5 of 64 steps played. At 133 or 97 BPM, 1 of 64. | The clock keeps a fractional `nextStep_` and queues each step one sample ahead, on the sample that holds the boundary. `fireDue()` fires `when <= sampleIndex_`, so nothing can be stranded. |
| E2 | The step boundary was `(counter + 1) * period` against the absolute sample index, so a new period moved every boundary at once. | 120 to 180 BPM after 80 steps: 40 steps in 40 samples, every trigger at once. 180 to 60 BPM: 20 seconds of silence. | A tempo change keeps the step in progress at the same fraction. |
| E3 | The conga partial was added after the envelope. | After one conga hit the tom rang at 0.35 forever. | The partial rides the body envelope. |
| E4 | Nothing released a pattern note when its track was muted or the switch went to EXT. | The held lead or bass note droned until the next note. | A muted note track rests. INT to EXT releases lead and bass. Stop does too. |
| E5 | The same note on the next step was always a tie. | A bass line of repeated C notes played as one long note. | `NoteStep::tie`. Without it a repeated note plays again. `testClockNotesAndEdges` now marks its tied step. |

Added for the page, not bugs: `setRunning()` and `restart()` (the engine had no stop), and `setExternalClock()` with `clockPulse()` for CLK IN.

## Engine notes, not changed

- N1, plugin transport. The plugin's pattern runs whenever the host calls `processBlock`, playing or not, and is not locked to the host's bar position. `setRunning()` now exists if the plugin should follow the transport.
- N2, voices never end. A drum voice that has fired keeps computing forever. That is about 18% of one core in the browser once all 16 have sounded. `VoiceState::n` is an `int`, so a voice left without a retrigger for about 12.4 hours (2^31 samples) overflows. The envelope then reads `exp` of a large positive number, which is a full scale blast. Suggest ending a drum voice once its envelope is under 1e-6.
- N3, fixed 48 kHz. `kFs` is a constant. The plugin resamples to the host rate by linear interpolation, which dulls and aliases the top octave at 44.1 kHz. The web page asks the browser for 48 kHz and resamples the same way only if refused.
- N4, no headroom. With every level at 1 the main mix peaks near 2 at Master 1. The plugin defaults Master to 0.45. The page does too.
- N5, default knobs. Most `Knobs` fields default to 0, so a fresh engine plays a short, low, dull kit. The page starts from the SCHEMATICS.md example values.

## Design pack questions (SCHEMATICS.md), not changed

- S1, BD1 Dist. `tanh(drive * pre) / tanh(drive)` with `drive = 1 + 8u` is not `y = pre` at Dist 0, but the printed rows need `y = pre`. The engine bypasses at CC 0 only, so CC 0 to CC 1 jumps from no shaping to about 24% more level at half scale. `drive = 9u` with the bypass at 0 would be continuous.
- S2, shuffle. "Even steps only (0-based step index odd)" does not say track step or global counter. The engine uses the track step. With an odd track length the delayed steps stop alternating with the beat.
- S3, snare bend. The step bend list includes SD, but the SD equations do not place it. The engine adds it to the Pitch envelope depth, so at Pitch 0 the bend is gone in about 10 ms.
- S4, rests. "A silent step forces the voice to 0 for the samples of that step" and "a rest after a note starts the release" disagree. The engine releases.
- S5, lead and bass in EXT. Not specified. The engine plays the live note on a rising gate and releases on the falling edge.
- S6, holds. BD2 and the toms at Decay 127 hold forever. Only the next hit ends them. There is no gate length or choke.
- S7, cymbal noise. The block diagram says "noise, fixed mix", the equation says `0.15 * u(tune) * x`. The noise is off at Tune 0 and rises with Tune.
- S8, centre pan. `gL = gR = 0.5` puts maracas, lead and bass 6 dB under a hard panned voice at the same level.
- S9, clock jacks. CLK IN, RST IN and RUN IN are not in the pack. On the web page a CLK IN pulse is one step, RST IN goes back to step 1, and RUN IN starts or stops.
- S10, not built yet. Per-step sound overrides, the play-mode global shuffle hold (the engine has `setGlobalShuffle`), and pattern sets.
