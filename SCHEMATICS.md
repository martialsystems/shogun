# Schematics (2026-10-06)

Functional blocks and sample equations. Coefficients are STAND-IN. Sample rate in every numeric example: 48,000 Hz. Arithmetic is IEEE-754 double. A later test may treat the printed decimals as exact within 1e-5.

Shared helpers:

```
u(cc) = cc / 127
fs = 48000
pi = 3.141592653589793
```

`cc` is an integer from 0 to 127. A per-step override, when the record mode stored one, replaces the live knob for that step. Manual record mode does not store per-step knobs.

A rising trigger at sample n0 resets that voice's phase to 0 and restarts its envelopes at n0. Until the first trigger, a cleared voice outputs 0.

A drum voice ends when every envelope it has is under 1e-6. From that sample it outputs 0 until its next trigger. A held voice (BD2 or a tom at Decay 127) does not end, because its envelope stays at the sustain. BD1 with Noise above 0 does not end either, because the BD1 noise below has no envelope.

Rests. A drum step that is off fires nothing, and a drum voice that is still ringing rings out. A note-track rest releases the note that is sounding, with the release in Lead and bass.

## Velocity law

Pattern accent index a is 0, 1, or 2. English printed page 20: LED off is soft, green is medium, red is loud.

```
g_accent(0) = 0.55
g_accent(1) = 0.78
g_accent(2) = 1.00
```

STAND-IN.

EXT velocity v, an integer 0 to 127:

```
g_vel(v) = 0.15 + 0.85 * (v / 127)
```

STAND-IN. v = 0 gives 0.150000. v = 100 gives 0.819291. v = 127 gives 1.000000.

INT multiplies the voice by g_accent and ignores Trig velocity. EXT multiplies by g_vel and ignores the pattern accent. The gain multiplies the voice output after the voice equation.

## Clock

Steps per quarter note, from the four scale settings in both manuals:

| Scale | Steps per quarter | Samples per step at 120 BPM |
| --- | --- | --- |
| 32nd | 8 | 3,000 |
| 16th triplet | 6 | 4,000 |
| 16th | 4 | 6,000 |
| 8th triplet | 3 | 8,000 |

```
period = fs * 60 / (bpm * steps_per_quarter)
```

At 120 BPM and a 16th scale, period is 6,000 samples. Internal bpm is clamped to 60 through 180. Default 120.

The step counter increments on each period boundary in INT and in EXT. The clock display shows `(counter mod pattern_length) + 1`. pattern_length is the bar, an integer from 1 to 32.

Each track stores its own length L, also 1 to 32. On global counter c the track reads step `c mod L`. A shorter track cycles against the bar. A longer track has steps past one bar, which is how a track reaches the B half when the bar is shorter. The track fires when that stored step is on. It does not also have to fall inside the bar length.

Shuffle intensity s is an integer 0 to 15. STAND-IN. The delay goes on odd clock steps, counter c odd, whatever the track's length. A track of odd length still swings on the beat, so its own step 0 is delayed on every other pass:

```
delay_samples = (s / 15) * (period / 3)
```

s = 0 is no delay. s = 8 at period 6,000 delays an odd clock step by 1,066.666667 samples. The fractional sample is kept and the trigger fires on the sample that contains the fractional boundary. Play-mode global shuffle, while held, uses one s for every track and does not write the pattern.

Track shift, controllers 89 to 104, one per track including the two note tracks. STAND-IN:

```
shift_samples = round(u(cc) * 0.030 * fs)
```

Full scale is 1,440 samples, 30 ms. The whole track's triggers move later by that many samples.

## INT and EXT

INT fires a drum voice when its step is on. The flam table and the accent index apply. The Trig jack is not read.

EXT does not fire from the pattern. Flam, step on and off, and pattern accent do nothing. The Trig jack's rising edge fires the voice, and g_vel supplies accent. Knobs, including per-step knob overrides if a later editor still wants them, remain in force. This pack's EXT path uses the live knobs, not the pattern's per-step sound overrides, because the brief says the pattern is ignored for step on and off, flam, and accent, and that knob values still apply. ASSUMED: "knob values" means the live knobs, not values stored on ignored steps.

Lead and bass in EXT follow the live gate on their Trig jacks. A rising edge opens the note at the live pitch with g_vel. The falling edge starts the release. The pattern's notes, rests, and ties do nothing in EXT.

Bend stored on a step is part of the pattern. EXT ignores it. The Pitch knob on BD1, BD2, SD, and the toms still applies, because that knob is a sound control, not the step bend.

## Clock jacks

CLK IN, RST IN, and RUN IN are gate inputs at the same 1 V threshold as the Trig jacks. A rising edge on CLK IN advances the counter one step, and while CLK IN is patched the internal period does not. A rising edge on RST IN sets the counter to 0, so the next step is step 1. A rising edge on RUN IN starts a stopped clock or stops a running one. None of them changes the INT and EXT switch. Only the switch chooses.

## Noise generator

One generator for the whole instrument, so examples reproduce.

```
state = (1664525 * state + 1013904223) mod 2^32
x = state / 2^32 * 2 - 1
```

Initial state is 1. The first draw yields x = -0.527089. Each noise consumer takes the next draw when it needs a sample. The maracas example below is the first draw of a cleared generator.

One-pole lowpass, state y previous 0 at a trigger:

```
a = exp(-2 * pi * fc / fs)
y[n] = (1 - a) * x[n] + a * y[n-1]
```

## Output sum

Instrument level and master are linear gains, not stored in the pattern. Default level 1, default master 1. STAND-IN, they are plain multipliers.

Linear pan, p from -1 to 1:

```
gL = 0.5 * (1 - p)
gR = 0.5 * (1 + p)
```

STAND-IN.

Main-only centre voices (maracas, lead, bass) use equal power instead: 0.707107 on each side, which is sqrt(0.5), so they sit level with a hard-panned voice. The linear law above stays for the toms and the clap.

| Voice | Pair | p |
| --- | --- | --- |
| BD1 | BD L | hard left, written only to L at gain 1 |
| BD2 | BD R | hard right, written only to R at gain 1 |
| SD | SD/RS L | hard left |
| RS | SD/RS R | hard right |
| OH + HH | HH/CY L | summed, hard left |
| CY | HH/CY R | hard right |
| CP bursts | CP stereo | pan of burst i, below |
| CP tail | CP stereo | p = 0 |
| LTC | TO/CO | p = -0.7 |
| MTC | TO/CO | p = 0 |
| HTC | TO/CO | p = 0.7 |
| CL | CB/CL L | hard left |
| CB | CB/CL R | hard right |
| MA, lead, bass | main only | centre, 0.707107 each side |

The voice sample is after the accent or velocity gain and after the instrument level. Hard left adds that sample to the left channel of the pair and of the main, and adds 0 to the right. Hard right is the swap. A panned voice uses gL and gR on both the pair and the main. The master multiplies the main only. Pair jacks are before the master.

CHOICE: the main mix is that sum whether or not a pair jack is patched. Maracas, lead, and bass have no pair.

## Flam table

Flam exists on every drum track except clap. Sixteen patterns, index k from 0 to 15. The manuals name 16 patterns and do not print the timings. STAND-IN offsets, in samples, from the step:

```
hits = 2 + (k mod 4)
gap = 180 * (1 + floor(k / 4))
offsets = 0, gap, 2*gap, ... while the list length is hits
```

Index 0 is samples 0 and 180. Index 15 is 0, 720, 1,440, 2,160, and 2,880. Each offset fires the voice again. A new fire restarts that voice's envelopes. Offsets may cross into the next step.

## BD1

```
trig --> pitch envelope --> sine body ----+
      --> transient[s], 4 ms --------------+--> sum --> tanh clip --> gain
      --> noise --> one-pole lowpass ------+
```

The clip is bypassed at Dist 0.

Decay does not reach a steady tone. The body envelope falls to 0 for every knob value.

```
f_tune = 35 * (140/35) ^ u(tune)          # 35 Hz to 140 Hz
depth  = 18 * u(pitch)                    # semitones, 0 to 18
tau_p  = 0.012 + 0.25 * u(pitch)          # seconds
tau_b  = 0.03 + 1.2 * u(decay)            # seconds, always finite
s      = min(15, floor(trigger_cc * 16 / 128))
f_tr   = 160 * (1.35 ^ s)
tau_tr = 0.004
fc     = 200 * (8000/200) ^ u(filter)     # 200 Hz to 8,000 Hz
drive  = 9 * u(dist)                     # 0 at Dist 0, which is the bypass

p_env[n] = exp(-n / (fs * tau_p))
f[n]     = f_tune * 2 ^ (((depth + bend_st) * p_env[n]) / 12)
body[n]  = sin(phase[n]) * exp(-n / (fs * tau_b))
tr[n]    = sin(2 * pi * f_tr * n / fs) * exp(-n / (fs * tau_tr))
noise    = u(noise_cc) * lowpass(x[n], fc)
pre[n]   = body[n] + u(attack) * tr[n] + noise
y[n]     = pre[n]                                   if dist_cc = 0
y[n]     = tanh(drive * pre[n]) / tanh(drive)       if dist_cc > 0
```

phase advances by `2 * pi * f[n] / fs` after the sample is taken. n is samples since the trigger. bend_st is the step bend in semitones. It decays with p_env, on the same time constant as the Pitch knob. INT step bend is `bend_st = 12 * (2 * u(bend_cc) - 1)`. CC 0 is -12 semitones, CC 127 is +12, and CC 64 is 12 * (128/127 - 1) = 0.094488 semitones. STAND-IN. EXT forces bend_st to 0. The printed BD1 example uses bend_st = 0, noise CC 0, and dist CC 0, so y = pre and the noise term is 0. drive = 9u starts at 0.070866 for CC 1, where tanh(drive * pre) / tanh(drive) is within 0.2% of pre, so CC 0 to CC 1 does not jump.

Example knobs: Attack 64, Decay 80, Pitch 40, Tune 50, Noise 0, Filter 64, Dist 0, Trigger 0.

```
u(attack) = 0.503937
f_tune    = 60.408707 Hz
depth     = 5.669291 semitones
tau_p     = 0.090740 s
tau_b     = 0.785906 s
s         = 0
f_tr      = 160 Hz
```

| n | f Hz | y |
| --- | --- | --- |
| 0 | 83.814360 | 0.000000 |
| 10 | 83.751440 | 0.208884 |
| 48 | 83.514083 | 0.832547 |
| 480 | 80.998689 | -0.907532 |
| 2,400 | 72.957296 | -0.605374 |

The instantaneous frequency at n = 2,400 is closer to f_tune than the frequency at n = 0. That is the pitch envelope decaying.

Same knobs, Dist 64: drive = 4.535433, and at n = 48, pre = 0.832547 gives y = 0.999180.

Same knobs, Attack 127, sample n = 10: Trigger 0 gives s = 0, f_tr = 160, y = 0.306787. Trigger 64 gives s = 8, f_tr = 1,765.184603, y = 0.810530. The attack sample changed because the transient frequency changed.

## BD2

```
trig --> sine body, envelope toward sustain or toward 0 --> out
      --> octave sine, 5 ms, level = tone ---------------->
```

No noise, no filter, no clip, no 16-type transient. The body can hold.

```
f_tune = 45 * (100/45) ^ u(tune)          # 45 Hz to 100 Hz
f_tr   = 2 * f_tune
tau_tr = 0.005

if decay_cc >= 127:
    sustain = 0.70
    tau_b   = 0.40
else:
    sustain = 0.0
    tau_b   = 0.04 + 1.6 * u(decay)

env[n]  = sustain + (1 - sustain) * exp(-n / (fs * tau_b))
body[n] = sin(phase[n]) * env[n]
tr[n]   = sin(2 * pi * f_tr * n / fs) * exp(-n / (fs * tau_tr))
y[n]    = body[n] + u(tone) * tr[n]
```

Pitch bend, when the step has one, uses the BD1 pitch-envelope shape with tau_p = 0.08 s STAND-IN and depth_st = 12 * (2 * u(bend_cc) - 1). The Tune knob is the resting frequency. The example below has no bend.

Hold check, envelope only: Decay 127 at n = 0 is 1.000000, at n = 480 is 0.992593, at n = 96,000 (2 s) is 0.702021, against sustain 0.700000. Decay 0 at n = 96,000 is 0.000000 within 1e-5. Decay 126 at n = 96,000 is 0.292599 and is not a hold. Only CC 127 holds.

Tone example, Tune 60, Tone 100, n = 10, envelopes of the transient only: f_tune = 65.621948 Hz, f_tr = 131.243897 Hz, u(tone) * tr = 0.129116.

## SD

```
trig --> pitch envelope --> sine tone 1 --+
                         --> sine tone 2 --+--> blend --> out
      --> noise envelope ------------------>
```

```
f1        = 120 * (400/120) ^ u(tune)      # 120 Hz to 400 Hz
detune_st = -8 + 16 * u(d_tune)            # -8 to +8 semitones
f2        = f1 * 2 ^ (detune_st / 12)
depth     = 14 * u(pitch)                  # semitones
tau_p     = 0.01 + 0.12 * u(pitch)
tau_tone  = 0.02 + 0.55 * u(tone_decay)
tau_n     = 0.01 + 0.25 * u(sn_decay)

f1[n] = f1 * 2 ^ (depth * p_env[n] / 12)
f2[n] = f2 * 2 ^ (depth * p_env[n] / 12)
t1[n] = sin(phase1) * exp(-n / (fs * tau_tone))
t2[n] = sin(phase2) * exp(-n / (fs * tau_tone))
nz[n] = u(snappy) * x[n] * exp(-n / (fs * tau_n))
y[n]  = (1 - u(tone)) * t1[n] + u(tone) * t2[n] + nz[n]
```

Example, noise left out (Snappy 0): Tune 70, D-Tune 90, Snappy 0, SN Decay 50, Tone 64, Tone Decay 60, Pitch 30.

```
f1        = 233.014061 Hz
f2        = 282.574688 Hz
detune_st = 3.338583 semitones
tau_tone  = 0.279843 s
blend     = 0.503937
```

At n = 20, t1 = 0.671594, t2 = 0.778808, y = 0.725623. Both tones are in the sum. Tone CC 0 would output t1. Tone CC 127 would output t2.

## RS

```
trig --> short sine --> out
```

```
f   = 250 * (2500/250) ^ u(tune)           # 250 Hz to 2,500 Hz
tau = 0.012                                # fixed, no decay knob
y[n] = sin(2 * pi * f * n / fs) * exp(-n / (fs * tau))
```

Tune 40: f = 516.298233 Hz. At n = 8, y = 0.507608.

## CY

```
trig --> metallic stack A --+
      --> metallic stack B --+--> blend --> decay --> out
      --> noise, fixed mix 0.15 -->
```

Two stacks. Each partial is a sine plus its third harmonic when 3f is under the Nyquist rate, which is the first two terms of a square. Ratios are STAND-IN.

```
ratios_a = 1.00, 1.52, 1.87, 2.41
ratios_b = 1.00, 1.34, 1.71, 2.05
f0  = 180 * (900/180) ^ u(tune)            # 180 Hz to 900 Hz
tau = 0.05 + 1.8 * u(decay)
```

For each ratio r, one partial is sin(2 * pi * f0 * r * n / fs). When 3 * f0 * r is under the Nyquist rate, add one third of the sine at 3 * f0 * r. Divide that partial by its weight: 1, or 1 + 1/3 when the third is present. The stack sample is the mean of the four partials. Then:

```
y[n] = exp(-n / (fs * tau)) * (
         (1 - u(tone)) * stack_a[n]
       + u(tone) * stack_b[n]
       + 0.15 * x[n] )
```

The noise mix is fixed at 0.15 for every Tune, STAND-IN. The numeric row below uses the stacks only, noise omitted, so the row does not depend on generator position.

Tune 64, Tone 70, Decay 90: f0 = 405.050673 Hz, tau = 1.325591 s, blend = 0.551181. At n = 30, stack A = 0.187710, stack B = 0.382957, y = 0.295187.

## OH and HH

One colour, two decays, one choke.

```
f_c    = 250 * (4000/250) ^ u(hh_tune)     # 250 Hz to 4,000 Hz, CC 73
tau_oh = 0.04 + 1.1 * u(oh_decay)
tau_hh = 0.008 + 0.04 * u(hh_decay)
```

Each hat is a metallic stack at f_c with ratios 1.00, 1.47, 1.80, 2.33, built like a CY stack, plus noise at mix 0.25 * x[n], times its own exp decay. STAND-IN ratios.

A closed-hat trigger sets the open-hat envelope to 0 on that sample, and the open hat stays at 0 until the next open trigger. It does not shorten the closed hat.

Example colour, HH Tune 60: f_c = 926.436359 Hz. OH Decay 100: tau_oh = 0.906142 s. HH Decay 40: tau_hh = 0.020598 s. An open hat that has run 200 samples has envelope 0.995412. On the closed trigger that envelope is 0.

## CL

```
f   = 400 * (3000/400) ^ u(tune)           # 400 Hz to 3,000 Hz
tau = 0.004 + 0.08 * u(decay)
y[n] = sin(2 * pi * f * n / fs) * exp(-n / (fs * tau))
```

Tune 50, Decay 30: f = 884.244363 Hz, tau = 0.022898 s. At n = 12, y = 0.972835.

## CP

```
trig --> N short sines, 11 ms apart, panned --> stereo
      --> filtered noise tail -----------------> both channels
```

```
s      = min(15, floor(trigger_cc * 16 / 128))     # CC 77, ASSUMED type
count  = 1 + floor(data_cc * 8 / 128)              # 1 to 8, ASSUMED, no CC
gap    = round(0.011 * fs)                         # 528 samples, 11 ms
f_tr   = 700 * (1.28 ^ s)
tau_tr = 0.003
fc     = 400 * (6000/400) ^ u(filter)
tau    = 0.05 + 0.8 * u(decay)
```

Burst i, i from 0 to count-1, starts at sample i * gap. Its own n_b = n - i * gap, and it is 0 when n_b < 0.

```
burst_i[n] = sin(2 * pi * f_tr * n_b / fs) * exp(-n_b / (fs * tau_tr))
```

Pan of burst i, count > 1: `p = -1 + 2 * i / (count - 1)`. One burst uses p = 0. The tail is noise through the one-pole at fc, times exp(-n / (fs * tau)), times 0.5 on each channel. The tail is the manual's reverb tail rendered as a noise decay. It is not a room.

```
yL[n] = u(attack) * sum_i burst_i[n] * gL(p_i) + tail[n] * 0.5
yR[n] = u(attack) * sum_i burst_i[n] * gR(p_i) + tail[n] * 0.5
```

Data CC 48 gives count 4. gap is 528. With Trigger 0, f_tr = 700 Hz. The transient at 5 samples after each burst start, before Attack scaling, is 0.427195. Burst starts are samples 0, 528, 1,056, and 1,584.

## LTC, MTC, HTC

Each voice:

```
trig --> sine body, BD2-style hold --> out
      --> noise, if that voice's switch is on, shared level
```

Tom mode is one sine at f_tune. Conga mode adds a second sine at 2.30 * f_tune, gain 0.35. ASSUMED. The manuals say the switch changes tom and conga and do not describe the waveform.

```
LTC f = 70 * (180/70) ^ u(tune)
MTC f = 100 * (280/100) ^ u(tune)
HTC f = 140 * (400/140) ^ u(tune)
```

Decay matches the BD2 hold law with different constants, so a tom is not a copy of BD2. STAND-IN:

```
if decay_cc >= 127:
    sustain = 0.55
    tau_b   = 0.35
else:
    sustain = 0
    tau_b   = 0.03 + 1.2 * u(decay)
```

Noise, when the voice switch is on (CC at or above 64): `u(tom_noise) * x[n] * exp(-n / (fs * 0.12))`. The 0.12 s noise decay is STAND-IN and shared. tom_noise is CC 84.

LTC Tune 40: f = 94.251191 Hz. Decay 127 at n = 96,000: envelope 0.551484, sustain 0.550000. TOM_NOISE 33: level 0.259843.

Bend uses the BD2 bend paragraph.

## CB

```
trig --> square at f ----+
      --> square at 1.015 f --> average --> decay --> out
```

The two squares are the brief. Detune ratio 1.015 is STAND-IN. A square is the odd harmonics k = 1, 3, 5, ... while k * f is under Nyquist and k <= 15, amplitude 1/k, divided by the sum of those amplitudes.

```
f   = 300 * (1200/300) ^ u(tune)           # 300 Hz to 1,200 Hz
tau = 0.01 + 0.40 * u(decay)
y[n] = 0.5 * (square(f) + square(1.015 * f)) * exp(-n / (fs * tau))
```

Tune 48, Decay 70: f = 506.607352 Hz, second square 514.206462 Hz, tau = 0.230472 s. At n = 15, y = 0.405275.

Sixteen panel steps, if a UI wants them, are Tune CC values round(i * 127 / 15) for i from 0 to 15. The voice reads the continuous CC.

## MA

```
trig --> noise --> one-pole lowpass at 1,500 Hz --> decay --> main
```

No tune knob. Colour is fixed. STAND-IN fc = 1,500 Hz. The filter is the lowpass in the shared section, held at that cutoff. Decay:

```
tau = 0.02 + 0.30 * u(decay)
y[n] = lowpass(x[n]) * exp(-n / (fs * tau))
```

Decay 55: tau = 0.149921 s. First noise draw x = -0.527089. a = exp(-2 * pi * 1500 / fs) = 0.821725. With filter state 0, y[0] = -0.093967 before the envelope, and the envelope at n = 0 is 1. On the main, at the equal-power centre, each side is -0.066445.

## Lead and bass

```
note --> band-limited saw --> one-pole lowpass --> envelope --> main
```

Waveform ASSUMED: saw, harmonics k = 1 to 8, amplitude 1/k, skipped when k * f is at or above Nyquist, then divided by the sum of the weights used.

Pitch is MIDI note 36 to 72, which is the appendix range for CV1 and CV23. Hz = 440 * 2 ^ ((note - 69) / 12). Note 60 is 261.625565 Hz. Note 36 is 65.406391 Hz. Note 72 is 523.251131 Hz.

The panel entry in both manuals is step buttons 1 to 13 for pitch class C through the next C, and buttons 14 to 16 for the octave. ASSUMED map onto notes 36 to 72: octave 0, 1, 2 from buttons 14, 15, 16, pitch class 0 to 12 from buttons 1 to 13, note = 36 + 12 * octave + class, then clamp to 72. A class of 12 is the upper C, so some combinations land on the same note. That collision is accepted.

A rest, the A/B silent step, releases the note that is sounding. It does not cut it to 0. A rest on a voice that is not sounding opens nothing. A tied note keeps the envelope open and does not retrigger.

Envelope: attack is one sample to 1 on a new note, hold while the note is tied, release tau = 0.03 s STAND-IN when the note ends.

Lead tone, ASSUMED, one-pole lowpass:

```
fc = 200 * (8000/200) ^ u(lead_tone)
```

Bass tone is the manual's cutoff, and the same u is the CV3 value, scaled STAND-IN as 0 V to 5 V for a jack: `cv3 = 5 * u(bass_tone)`.

```
fc = 80 * (4000/80) ^ u(bass_tone)
```

Lead example, note 60, before the filter, n = 25 samples after a trigger, harmonics 1 to 8: saw = 0.387776. Lead tone CC 80: fc = 2,042.686148 Hz.

Bass accent uses g_accent on the bass voice in INT. The drums' three-level accent is the same table.

## Step bend and the Pitch knob

BD1's Pitch knob is both time and depth, one u, as the instrument chapter states. SD's pitch control is depth, with the time law above. Step bend is an extra signed depth on BD1, BD2, SD, LTC, MTC, and HTC only. CY, RS, hats, clave, clap, cowbell, and maracas have no step bend. EXT ignores step bend.

## Cleared voice

A voice that has never been triggered in the block under test outputs 0 on every sample. A drum step that is off does not trigger. A note-track rest does not open the gate. Neither event produces a new nonzero sample from a cleared voice.
