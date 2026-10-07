# Test plan (2026-10-06)

These tests are the contract for the engine in `engine/`. `tests/voices.cpp` compiles them into `build/shogun_tests`. Each one is specified against [SCHEMATICS.md](SCHEMATICS.md) at 48,000 Hz, double precision, absolute tolerance 1e-5 on the printed decimals unless a row says otherwise.

The clock switch is also pinned in `forge/graphs/switch_law.json`. A Trig cable cannot force EXT. INT ignores Trig jacks. EXT ignores the pattern. The numbers in this file stay the named checks. A later fit may change a printed row when SCHEMATICS.md and `tests/voices.cpp` change together. `make test` runs those checks and `forge/tests/test_switch_law.py`.

A cleared voice is one that has not been triggered. Its output block is 0.

Shared pattern for the bypass tests: 16th scale, 120 BPM, so a step is 6,000 samples. Pattern length 4. Track length 4. One drum voice, BD1, with the BD1 example knobs (Attack 64, Decay 80, Pitch 40, Tune 50, Noise 0, Filter 64, Dist 0, Trigger 0). Step 0 is on. Steps 1, 2, and 3 are off. No flam. Accent index 2. No bend.

## testKickBendDecays

Render BD1 from the example knobs, bend_st 0, from a trigger at n = 0.

The frequency at n = 0 is 83.814360 Hz. The frequency at n = 2,400 is 72.957296 Hz. f_tune is 60.408707 Hz. The later frequency is closer to f_tune than the first. y at n = 48 is 0.832547. y at n = 480 is -0.907532.

Fail if the later frequency is farther from f_tune than the frequency at the trigger, or if either printed y misses by more than 1e-5.

## testBd1SoundChangesAttack

Same knobs, Attack raised to 127. Render two triggers.

Trigger CC 0: at n = 10, y = 0.306787, transient frequency 160 Hz.

Trigger CC 64: at n = 10, y = 0.810530, transient frequency 1,765.184603 Hz.

The two y values differ. The body tune is the same in both renders, so the difference is the attack transient.

## testBd2CanHold

BD2 envelope, no dependence on phase.

Decay CC 127 at n = 96,000 is 0.702021, and the sustain it is approaching is 0.700000. The absolute difference from 0.70 is under 0.01.

Decay CC 0 at n = 96,000 is 0 within 1e-5.

Decay CC 126 at n = 96,000 is 0.292599. That value is not a hold. The test fails if CC 126 is treated as a hold, or if CC 127 has fallen below 0.5.

Tune 60 and Tone 100, transient only, at n = 10: scaled transient 0.129116. This row keeps the tone path from being a silent stub.

## testSnareTwoTones

SD example knobs, Snappy 0 so the noise term is 0. Tune 70, D-Tune 90, Tone 64, Tone Decay 60, Pitch 30, SN Decay 50.

f1 is 233.014061 Hz. f2 is 282.574688 Hz. At n = 20, t1 is 0.671594, t2 is 0.778808, and y is 0.725623.

A second render with Tone CC 0 equals t1 at n = 20. A third with Tone CC 127 equals t2 at n = 20. Both partials are present at Tone 64, and the blend knob selects between them.

## testHatChoke

HH Tune 60, OH Decay 100, HH Decay 40. Open hat triggers at sample 0. Envelope at sample 200, before any closed trigger, is 0.995412.

A closed-hat trigger at sample 200 sets the open envelope to 0 on that sample. Samples 200 through 500 of the open hat are 0. A later open trigger may sound again. The closed hat's own tau is 0.020598 s and is not cleared by its own trigger.

## testClapBurstCount

Data CC 48, so count is 4. Trigger CC 0, so the type index is 0 and f_tr is 700 Hz. Gap is 528 samples.

Four bursts start at samples 0, 528, 1,056, and 1,584. The gap in time is 11 ms, which is inside 10 to 12 ms. The unscaled transient at 5 samples after each start is 0.427195.

A count of 1 produces one start and no second peak 528 samples later. The voice is not a single noise hit when count is 4: four starts exist, and they are not on the same sample.

Flam is not applied. The manuals exclude clap from flam, and this test does not put a flam on it.

## testExtBypassIgnoresPattern

Mode EXT. The shared pattern has step 0 on. No Trig edge in the first step.

The BD1 output over the first 6,000 samples is 0. The clock counter still advances: after 6,000 samples the display step has moved.

Then a Trig rising edge, velocity 100, at the start of the next block, with the pattern still showing an off step or an on step. The voice fires. g_vel(100) is 0.819291. y at 10 samples after that edge equals the BD1 example y at n = 10, which is 0.208884, multiplied by 0.819291.

The pattern accent is index 2, whose g_accent is 1. The EXT render uses 0.819291, not 1. A flam on the pattern step does not add the extra hits at 180 samples.

Switch to INT on a later step without resetting the counter. The next on-step of the pattern fires, and the Trig jack no longer fires.

## testIntIgnoresTrigJacks

Mode INT. Same pattern. A Trig rising edge is placed in the middle of step 1, which is off. The voice stays 0 through that step.

Step 0, with no dependence on the Trig jack, fires. With accent index 2, y at 10 samples after the step boundary is 0.208884 times 1.000000.

Pulling the Trig cable, or leaving it plugged, does not change INT. The switch does.

## testRestIsSilent

From cleared voices:

- A drum step that is off, one full step at the clock above, outputs 0.
- A lead note-track rest, note slot empty, outputs 0 for that step.
- A bass note-track rest outputs 0 for that step.

A previous hit is not part of this test. The rest does not have to choke a voice that is already ringing. Hats have their own choke test.

## testIndividualOutStaysInMix

One BD1 hit, level 1, master 1, accent gain 1, the example hit. The BD pair's left channel at n = 48 is 0.832547. The main left channel at n = 48 is the same 0.832547. The pair is treated as patched.

Repeat with the pair unpatched. Main left at n = 48 is still 0.832547.

Master 0.5 scales the main to 0.416273 and leaves the pair at 0.832547.

Maracas does not appear on any pair. A maracas trigger with the first noise draw appears on the main only.

## Also locked by those tests

- EXT does not freeze the counter: `testExtBypassIgnoresPattern` reads the display after one silent step.
- A Trig cable does not select EXT: `testIntIgnoresTrigJacks` keeps the cable connected.
- Clap spacing is 11 ms: `testClapBurstCount`.
- BD2 hold is CC 127 only: `testBd2CanHold`.
