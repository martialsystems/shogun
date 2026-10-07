# Test plan (2026-10-06)

These tests are the contract for the engine in `engine/`. `tests/voices.cpp` compiles them into `build/shogun_tests`. Each one is specified against [SCHEMATICS.md](SCHEMATICS.md) at 48,000 Hz, double precision, absolute tolerance 1e-5 on the printed decimals unless a row says otherwise.

The clock switch is also pinned in `forge/graphs/switch_law.json`. A Trig cable cannot force EXT. INT ignores Trig jacks. EXT ignores the pattern. The numbers in this file stay the named checks. A later fit may change a printed row when SCHEMATICS.md and `tests/voices.cpp` change together. `make test` runs those checks and `forge/tests/test_switch_law.py`.

A cleared voice is one that has not been triggered. Its output block is 0.

Shared pattern for the bypass tests: 16th scale, 120 BPM, so a step is 6,000 samples. Pattern length 4. Track length 4. One drum voice, BD1, with the BD1 example knobs (Attack 64, Decay 80, Pitch 40, Tune 50, Noise 0, Filter 64, Dist 0, Trigger 0). Step 0 is on. Steps 1, 2, and 3 are off. No flam. Accent index 2. No bend.

## testKickBendDecays

Render BD1 from the example knobs, bend_st 0, from a trigger at n = 0.

The frequency at n = 0 is 83.814360 Hz. The frequency at n = 2,400 is 72.957296 Hz. f_tune is 60.408707 Hz. The later frequency is closer to f_tune than the first. y at n = 48 is 0.829514. y at n = 480 is -0.855516.

Fail if the later frequency is farther from f_tune than the frequency at the trigger, or if either printed y misses by more than 1e-5.

## testBd1SoundChangesAttack

Same knobs, Attack raised to 127. Render two triggers.

Trigger CC 0: at n = 10, y = 0.306649, transient frequency 160 Hz.

Trigger CC 64: at n = 10, y = 0.810392, transient frequency 1,765.184603 Hz.

The two y values differ. The body tune is the same in both renders, so the difference is the attack transient.

## testBd2CanHold

BD2 envelope, no dependence on phase.

Decay CC 127 at n = 96,000 is 0.702021, and the sustain it is approaching is 0.700000. The absolute difference from 0.70 is under 0.01.

Decay CC 0 at n = 96,000 is 0 within 1e-5.

Decay CC 126 at n = 96,000 is 0.056280. That value is not a hold. The test fails if CC 126 is treated as a hold, or if CC 127 has fallen below 0.5.

Tune 60 and Tone 100, transient only, at n = 10: scaled transient 0.129116. This row keeps the tone path from being a silent stub.

## testSnareTwoTones

SD example knobs, Snappy 0 so the noise term is 0. Tune 70, D-Tune 90, Tone 64, Tone Decay 60, Pitch 30, SN Decay 50.

f1 is 233.014061 Hz. f2 is 282.574688 Hz. At n = 20, t1 is 0.668428, t2 is 0.775136, and y is 0.722202.

A second render with Tone CC 0 equals t1 at n = 20. A third with Tone CC 127 equals t2 at n = 20. Both partials are present at Tone 64, and the blend knob selects between them.

## testHatChoke

HH Tune 60, OH Decay 100, HH Decay 40. Open hat triggers at sample 0. Envelope at sample 200, before any closed trigger, is 0.985052.

A closed-hat trigger at sample 200 sets the open envelope to 0 on that sample. Samples 200 through 500 of the open hat are 0. A later open trigger may sound again. The closed hat's own tau is 0.033008 s and is not cleared by its own trigger.

## testClapBurstCount

Data CC 48, so count is 4. Trigger CC 0, so the type index is 0 and f_tr is 700 Hz. Gap is 528 samples.

Four bursts start at samples 0, 528, 1,056, and 1,584. The gap in time is 11 ms, which is inside 10 to 12 ms. The unscaled transient at 5 samples after each start is 0.427195.

A count of 1 produces one start and no second peak 528 samples later. The voice is not a single noise hit when count is 4: four starts exist, and they are not on the same sample.

Flam is not applied. The manuals exclude clap from flam, and this test does not put a flam on it.

## testExtBypassIgnoresPattern

Mode EXT. The shared pattern has step 0 on. No Trig edge in the first step.

The BD1 output over the first 6,000 samples is 0. The clock counter still advances: after 6,000 samples the display step has moved.

Then a Trig rising edge, velocity 100, at the start of the next block, with the pattern still showing an off step or an on step. The voice fires. g_vel(100) is 0.819291. y at 10 samples after that edge equals the BD1 example y at n = 10, which is 0.208746, multiplied by 0.819291.

The pattern accent is index 2, whose g_accent is 1. The EXT render uses 0.819291, not 1. A flam on the pattern step does not add the extra hits at 180 samples.

Switch to INT on a later step without resetting the counter. The next on-step of the pattern fires, and the Trig jack no longer fires.

## testIntIgnoresTrigJacks

Mode INT. Same pattern. A Trig rising edge is placed in the middle of step 1, which is off. The voice stays 0 through that step.

Step 0, with no dependence on the Trig jack, fires. With accent index 2, y at 10 samples after the step boundary is 0.208746 times 1.000000.

Pulling the Trig cable, or leaving it plugged, does not change INT. The switch does.

## testRestIsSilent

From cleared voices:

- A drum step that is off, one full step at the clock above, outputs 0.
- A lead note-track rest, note slot empty, outputs 0 for that step.
- A bass note-track rest outputs 0 for that step.

A previous hit is not part of this test. The rest does not have to choke a voice that is already ringing. Hats have their own choke test.

## testIndividualOutStaysInMix

One BD1 hit, level 1, master 1, accent gain 1, the example hit. The BD pair's left channel at n = 48 is 0.829514. The main left channel at n = 48 is the same 0.829514. The pair is treated as patched.

Repeat with the pair unpatched. Main left at n = 48 is still 0.829514.

Master 0.5 scales the main to 0.414757 and leaves the pair at 0.829514.

Maracas does not appear on any pair. A maracas trigger with the first noise draw appears on the main only. Each side of the main is -0.066445, the printed -0.093967 at the equal-power centre.

## testVoiceEndsWhenQuiet

Trigger each drum voice once, BD1 with the example knobs and the others with default knobs. Each one sounds, then ends, then outputs exactly 0 on both main channels until a new trigger. The BD1 example ends where its body envelope crosses 1e-3: ceil(-ln(1e-3) * 48,000 * 0.136195) samples, within one sample. With Noise 127 it ends at the same sample and stays 0, because the noise rides the body envelope. BD2 at Decay 127 is still sounding after 10 s. A BD1 that has ended plays the example y(10) = 0.208746 on its next trigger.

## testDecayNoonIsShort

decay_tau is 8 ms at u = 0, 75.902 ms at u = 0.5, and 720.137 ms at u = 1. With Decay 64 on BD2, CY, OH, HH, CL, MTC, CB, and MA, on the snare's Tone Decay and SN Decay together (Snappy 127), and on the clap tail (one burst), each voice ends at ceil(-ln(1e-3) * 48,000 * 0.077259) = 25,617 samples, within one sample. The cowbell outputs exactly 0 on that sample.

## testBd2FullHolds

BD2 at Decay 127 is still sounding after 10 s, at the sustain 0.7. The next hit starts it again at 1.

## testTomFullEnds

LTC Tune 40, Decay 127. At 4 s (n = 192,000) it is 0.550005 and still sounding. At n = 194,400 it is 0.202335. It ends at n = 207,144. A second hit at 2 s starts the envelope again at 1.

## testSnareBendAtPitchZero

SD Tune 70, Pitch 0, Tone Decay 127, bend +12. f1 is 466.028122 Hz at n = 0 and 300.694079 Hz at n = 3,840. With bend 0, f1 stays 233.014061 Hz. With Pitch 127, at n = 6,240, f1 is 404.878529 Hz.

## testSoloMutesOtherVoices

BD1 (example knobs) and SD on step 0, SD soloed. For 200 samples BD1 and its pair are 0, the main is the snare alone, and the snare matches an engine without solo. The counter matches too. With solo off, the next sample of BD1 and of the main matches the engine without solo. A soloed SD on a muted track is 0 on every output, even from a direct trigger.

## testDistBypassAndDrive

BD1 example knobs, n = 48. Dist 0 is y = pre = 0.829514. Dist 1 is within 2e-3 of Dist 0. Dist 64 is 0.999151.

## testCenterPanIsNotHalf

A maracas trigger puts 0.707107 times the maracas sample on each side of the main, not 0.5. At n = 0 that is -0.066445. The mid tom is equal on both TO/CO channels and on the main. With Attack 0 the clap is its tail alone, 0.707107 times the filtered first noise draw on each channel.

## testShuffleSurvivesOddLength

BD1 track of length 3, every step on, shuffle 15, 120 BPM 16ths. The delay is period / 3 = 2,000 samples. The first six hits are at samples 0, 8,000, 12,000, 20,000, 24,000, and 32,000: every odd clock step is late, including track step 0 on the second pass.

## Also locked by those tests

- EXT does not freeze the counter: `testExtBypassIgnoresPattern` reads the display after one silent step.
- A Trig cable does not select EXT: `testIntIgnoresTrigJacks` keeps the cable connected.
- Clap spacing is 11 ms: `testClapBurstCount`.
- BD2 hold is CC 127 only: `testBd2CanHold`.
- Only BD2 holds; a tom at CC 127 ends: `testBd2FullHolds`, `testTomFullEnds`.
