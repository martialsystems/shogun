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

## Fixed on feat/engine-voices (2e49469)

N2 (voices never end), S1 (BD1 Dist), S2 (shuffle), S4 (rests), S5 (EXT lead and bass), S7 (cymbal noise), S8 (centre pan) and S9 (clock jacks) are settled in the engine and SCHEMATICS.md.

Then 64aeeb4: BD1 noise rides the body envelope, so BD1 ends under 1e-6 at any Noise setting (there is no separate noise decay). Every panned voice uses the equal-power law, 0.707 each side at centre, including MTC and the clap tail. 

Then f525218: one decay law for every drum, tau = 8 ms * exp(4.5 u) (8 ms, about 76 ms at noon, 720 ms at full), the snare's tone and noise and the clap tail included, and a voice outputs 0 under 1e-3. That settles S3 and S6 below.

## Engine notes, not changed

- N1, plugin transport. The plugin's pattern runs whenever the host calls `processBlock`, playing or not, and is not locked to the host's bar position. `setRunning()` now exists if the plugin should follow the transport.
- N3, fixed 48 kHz. `kFs` is a constant. The plugin resamples to the host rate by linear interpolation, which dulls and aliases the top octave at 44.1 kHz. The web page asks the browser for 48 kHz and resamples the same way only if refused.
- N4, no headroom. With every level at 1 the main mix peaks near 2 at Master 1. The plugin defaults Master to 0.45. The page now loads each factory pattern with its own kit and measured levels (web/page/kits.js, web/make_levels.mjs) and defaults Master to 0.65, where every factory pattern peaks under 1 on either side.
- N5, default knobs. Most `Knobs` fields default to 0, so a fresh engine plays a short, low, dull kit. The page loads a kit with every pattern instead.

## Design pack questions settled in f525218

- S3, snare bend. The bend is a drop on top of Tune with its own envelope, never under 80 ms while a bend is set, so Pitch 0 still swoops. Bend 0 adds nothing. It is in the SD equation.
- S6, holds. BD2 at Decay 127 holds until the next hit. A tom at Decay 127 rings for 4 s, releases and ends. No choke jack.

## Design pack questions (SCHEMATICS.md), still open

- S10, not built yet. Per-step sound overrides, the play-mode global shuffle hold (the engine has `setGlobalShuffle`), and pattern sets.
