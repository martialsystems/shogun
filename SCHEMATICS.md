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

Decay law. Every drum Decay knob, and the snare's Tone Decay and SN Decay, sets its time constant the same way. STAND-IN:

```
decay_tau(u) = 0.008 * exp(4.5 * u)        # seconds
```

u = 0 is 8 ms, u = 0.5 is 75.902 ms (CC 64 is 77.259 ms), and u = 1 is 720.137 ms. The clap tail uses it. The clap bursts keep their fixed 3 ms. RS has no Decay knob. BD2 at Decay 127 is the one exception: it holds. A tom at Decay 127 rings for 4 s and then ends.

A drum voice ends when every envelope it has is under 1e-3. From that sample, that sample included, it outputs 0 until its next trigger. Only a held BD2 does not end, because its envelope stays at the sustain.

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

Equal-power pan, p from -1 to 1:

```
gL = cos(pi / 4 * (1 + p))
gR = sin(pi / 4 * (1 + p))
```

STAND-IN. Every centred voice, p = 0, is 0.707107 on each side, which is sqrt(0.5), so it sits level with a hard-panned voice. p = -0.7 is gL = 0.972370, gR = 0.233445.

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
| MA, lead, bass | main only | p = 0 |

The voice sample is after the accent or velocity gain and after the instrument level. Hard left adds that sample to the left channel of the pair and of the main, and adds 0 to the right. Hard right is the swap. A panned voice uses gL and gR on both the pair and the main. The master multiplies the main only. Pair jacks are before the master.

Solo. One voice at a time can be soloed, or none. While a voice is soloed, only that voice reaches its pair and the main. Every other voice still triggers and runs its envelopes, and outputs 0, so when solo is turned off each one is heard where it would have been. A muted track stays silent when it is soloed: mute wins. Solo is not stored in the pattern, and it does not change the INT and EXT switch.

CHOICE: the main mix is that sum whether or not a pair jack is patched. Maracas, lead, and bass have no pair.

## Flam table

Flam exists on every drum track except clap. Sixteen patterns, index k from 0 to 15. The manuals name 16 patterns and do not print the timings. STAND-IN offsets, in samples, from the step:

```
hits = 2 + (k mod 4)
gap = 180 * (1 + floor(k / 4))
offsets = 0, gap, 2*gap, ... while the list length is hits
```

Index 0 is samples 0 and 180. Index 15 is 0, 720, 1,440, 2,160, and 2,880. Each offset fires the voice again. A new fire restarts that voice's envelopes. Offsets may cross into the next step.

## Wave

BD1, BD2, LTC, MTC, and HTC each have a Wave knob. Hats, clap, cymbal, and maracas do not. Wave is the west-coast triple wave folder: six identical cells in series, ported from martialsystems/serge_middle (the cell, its Lambert W solver, and its drive peak table), not a second waveshaper.

Each cell, for v not 0, with VT = 0.02585, Is = 2.52e-9, eta = 1.68, R = 33,000:

```
z     = (Is * R) / (eta * VT) * exp(|v| / (eta * VT))
v_out = sign(v) * (|v| - 2 * eta * VT * W(z))         # v_out(0) = 0
C6(v) = C(C(C(C(C(C(v))))))
```

W is the principal Lambert W, solved by Halley on w + ln(w) - ln(z) = 0 so the exponential is never formed. The residual is under 1e-12 through 6 V and at 48 V.

```
g(cc)    = 0              at CC 0, the bypass
g(cc)    = 0.5 + 3.5 * (cc - 1) / 126    CC 1 to 127, 0.5 to 4. STAND-IN taper
fold(x)  = x                                        if wave_cc = 0
fold(x)  = (4.379272 * P(1) / P(g)) * C6(5 * g * x)  otherwise
```

x is the body oscillator, sin(phase), about +/-1, scaled to +/-5 V times g before the cells. P(g) is the serge_middle peak of |C6(v)| for |v| <= 5 g, linear between its 53 knots, so a full-scale x peaks at 1 after the cells at every g and Wave does not change the peak level. Default Wave is CC 32 (0.25), g = 1.361111.

The fold acts on the oscillator, before the body envelope, so the decay law and the 1e-3 end are untouched. On BD1 the Dist clip comes after Wave. The 4x oversampler, the 10 Hz DC block, and the knob smoother of serge_middle are not in this pass. A folded sine is odd, so it has no DC.

Measured on a held body at velocity 1, the share of spectrum energy above 1.5 times the fundamental is under 0.005 at Wave 0, 0.388 at the default, and 0.033 at CC 127. The folds sit in the bottom 5.2 V of the cell's input (fold roots 0.73, 1.55, 2.43, 3.34, 4.27, 5.22 V). Past that each cell is close to an inverter, so at g = 4 most of the 20 V swing passes through and, after the peak is brought back to 1, the folds are a ripple near each zero crossing. The densest setting is the low end of the knob.

The init kit at the default Wave: one BD1 hit peaks at 0.963 against 0.964 at Wave 0, and its RMS over 0.5 s is 0.106 against 0.155 (3.3 dB lower).

## BD1

```
trig --> pitch and bend envelopes --> sine --> Wave -+
      --> click[s], 1 ms ----------------------------+--> sum --> tanh clip --> gain
      --> noise burst on the body env --> one-pole --+
```

Density pass, STAND-IN coefficients set by ear, no hardware capture. The clip is bypassed at Dist 0.

Decay does not reach a steady tone. The body envelope falls to 0 for every knob value.

```
f_tune = 35 * (140/35) ^ u(tune)          # 35 Hz to 140 Hz
depth  = 18 * u(pitch)                    # semitones, 0 to 18
tau_p  = 0.012 + 0.25 * u(pitch)          # seconds
tau_bend = max(tau_p, 0.08)               # the step bend's own time, 80 ms floor
tau_b  = decay_tau(u(decay))             # 8 ms to 720 ms
s      = min(15, floor(trigger_cc * 16 / 128))
f_tr   = 160 * (1.35 ^ s)
fc     = 200 * (8000/200) ^ u(filter)     # 200 Hz to 8,000 Hz
drive  = 9 * u(dist)                     # 0 at Dist 0, which is the bypass

shape(ph, k) = tanh(k * sin(ph)) / tanh(k)

p_env[n] = exp(-n / (fs * tau_p))
b_env[n] = exp(-n / (fs * tau_bend))      # 0 for every n when bend_st = 0
f[n]     = f_tune * 2 ^ ((depth * p_env[n] + bend_st * b_env[n]) / 12)
body[n]  = fold(sin(phase[n])) * exp(-n / (fs * tau_b))      # Wave, above
click[n] = wave_s(n) * (1 - n / 48) ^ 2   for n < 48, else 0
burst[n] = x[n] * exp(-n / (fs * tau_b)) * exp(-n / (fs * 0.015))
noise    = u(noise_cc) * lowpass(burst[n], fc)
pre[n]   = body[n] + u(attack) * click[n] + noise
y[n]     = pre[n]                                   if dist_cc = 0
y[n]     = tanh(drive * pre[n]) / tanh(drive)       if dist_cc > 0
```

The click is 1 ms, one of 16 shapes. s mod 4 picks the wave and s sets its rate f_tr: 0 is sin(2 pi f_tr n / fs), 1 is tanh(6 sin(2 pi f_tr n / fs)) / tanh(6), 2 is cos(2 pi f_tr n / fs), a hard tick, and 3 is a saw, 2 * frac(f_tr * n / fs) - 1. STAND-IN.

The noise is a short burst under the body envelope, then the one-pole. The burst is 0 wherever the body is, so the voice is silent once the body is under 1e-3. There is no separate noise decay knob.

phase advances by `2 * pi * f[n] / fs` after the sample is taken. n is samples since the trigger. bend_st is the step bend in semitones. Like the snare, it is its own drop to Tune: its time is the Pitch time, but never under 80 ms while a bend is set. INT step bend is `bend_st = 12 * (2 * u(bend_cc) - 1)`. CC 0 is -12 semitones, CC 127 is +12, and CC 64 is 12 * (128/127 - 1) = 0.094488 semitones. STAND-IN. EXT forces bend_st to 0. The printed BD1 example uses bend_st = 0, noise CC 0, dist CC 0, and the default Wave, CC 32, so y = pre. drive = 9u starts at 0.070866 for CC 1, where tanh(drive * pre) / tanh(drive) is within 0.2% of pre, so CC 0 to CC 1 does not jump.

Example knobs: Attack 64, Decay 80, Pitch 40, Tune 50, Noise 0, Filter 64, Dist 0, Trigger 0.

```
u(attack) = 0.503937
f_tune    = 60.408707 Hz
depth     = 5.669291 semitones
tau_p     = 0.090740 s
tau_b     = 0.136195 s
s         = 0
f_tr      = 160 Hz
```

| n | f Hz | y |
| --- | --- | --- |
| 0 | 83.814360 | 0.000000 |
| 10 | 83.751440 | 0.055968 |
| 48 | 83.514083 | 0.040808 |
| 480 | 80.998689 | -0.482608 |
| 2,400 | 72.957296 | 0.044642 |

The instantaneous frequency at n = 2,400 is closer to f_tune than the frequency at n = 0. That is the pitch envelope decaying. The small y values are the fold: at n = 48 sin(phase) is about 0.5, which is 3.4 V into the cells, close to a fold root.

Same knobs, Dist 64: drive = 4.535433, and at n = 48 y = 0.183040.

Same knobs, Attack 127, sample n = 10: Trigger 0 gives s = 0, a sine click at f_tr = 160, y = 0.120608. Trigger 64 gives s = 8, a sine click at f_tr = 1,765.184603, y = 0.453201. The attack sample changed because the click rate changed.

## BD2

```
trig --> sine with slow FM --> Wave, envelope toward sustain or toward 0 --> out
      --> octave sine, 5 ms, level = tone --------------------------------->
```

No noise, no filter, no clip, no 16-type transient. The body can hold.

```
f_tune = 45 * (100/45) ^ u(tune)          # 45 Hz to 100 Hz
f_tr   = 2 * f_tune
tau_tr = 0.005
f_m    = 40 * (200/40) ^ u(tone)          # FM rate, 40 Hz to 200 Hz
i      = 0.12 * f_tune                    # FM depth in Hz, STAND-IN

if decay_cc >= 127:
    sustain = 0.70
    tau_b   = 0.40
else:
    sustain = 0.0
    tau_b   = decay_tau(u(decay))

env[n]  = sustain + (1 - sustain) * exp(-n / (fs * tau_b))
f[n]    = f_tune * 2 ^ (bend_st * exp(-n / (fs * 0.08)) / 12) + i * sin(2 * pi * f_m * n / fs)
body[n] = fold(sin(phase[n])) * env[n]      # BD2 Wave
tr[n]   = sin(2 * pi * f_tr * n / fs) * exp(-n / (fs * tau_tr))
y[n]    = body[n] + u(tone) * tr[n]
```

fold is the Wave folder on BD2's own knob. The slow FM lets it bark: the pitch swings f_tune by 12% at the Tone rate, and it keeps swinging while Decay 127 holds. Tone sets both the FM rate and the octave transient level.

Pitch bend, when the step has one, uses tau_p = 0.08 s STAND-IN and depth_st = 12 * (2 * u(bend_cc) - 1). The Tune knob is the resting frequency. The example below has no bend.

Hold check, envelope only: Decay 127 at n = 0 is 1.000000, at n = 480 is 0.992593, at n = 96,000 (2 s) is 0.702021, against sustain 0.700000. Decay 0 at n = 96,000 is 0.000000 within 1e-5. Decay 126 at n = 96,000 is 0.056280 and is not a hold. Only CC 127 holds.

Tone example, Tune 60, Tone 100, n = 10, envelopes of the transient only: f_tune = 65.621948 Hz, f_tr = 131.243897 Hz, u(tone) * tr = 0.129116. f_m = 141.244575 Hz, and f at n = 10 is 67.077722 Hz.

## SD

```
trig --> pitch envelope --> shaped tone 1 --+
                         --> shaped tone 2 --+--> blend --> out
      --> noise --> 4-pole low-pass, ducked --> noise envelope -->
```

```
f1        = 120 * (400/120) ^ u(tune)      # 120 Hz to 400 Hz
detune_st = -8 + 16 * u(d_tune)            # -8 to +8 semitones
f2        = f1 * 2 ^ (detune_st / 12)
depth     = 14 * u(pitch)                  # semitones
tau_p     = 0.01 + 0.12 * u(pitch)
tau_tone  = decay_tau(u(tone_decay))
tau_n     = decay_tau(u(sn_decay))
tau_bend  = max(tau_p, 0.08)               # the bend's own time, 80 ms floor

p_env[n] = exp(-n / (fs * tau_p))
b_env[n] = exp(-n / (fs * tau_bend))       # 0 for every n when bend_st = 0
f1[n] = f1 * 2 ^ ((depth * p_env[n] + bend_st * b_env[n]) / 12)
f2[n] = f2 * 2 ^ ((depth * p_env[n] + bend_st * b_env[n]) / 12)
fc_n  = 1000 * (12000/1000) ^ u(tone)     # noise low-pass, 1 kHz to 12 kHz
t1[n] = shape(phase1, 2) * exp(-n / (fs * tau_tone))
t2[n] = shape(phase2, 2) * exp(-n / (fs * tau_tone))
lp[n] = ladder(duck[n] * x[n], fc_n)
nz[n] = u(snappy) * lp[n] * exp(-n / (fs * tau_n))
y[n]  = (1 - u(tone)) * t1[n] + u(tone) * t2[n] + nz[n]
```

Example, noise left out (Snappy 0): Tune 70, D-Tune 90, Snappy 0, SN Decay 50, Tone 64, Tone Decay 60, Pitch 30.

```
f1        = 233.014061 Hz
f2        = 282.574688 Hz
detune_st = 3.338583 semitones
tau_tone  = 0.067049 s
blend     = 0.503937
```

shape(ph, k) = tanh(k * sin(ph)) / tanh(k), a sine bent toward square, here at a fixed k = 2, STAND-IN. SD has no Wave knob. The noise goes through a 4-pole low-pass, four one-poles at g = 1 - exp(-2 pi fc_n / fs) with feedback 1.6 from the last pole, output times 2.6 so the pass band is unity. Tone sets its cutoff, so Tone 0 is dark. Resonance can pump: a peak follower on |lp| (instant attack, 10 ms release) ducks the noise input, duck = 1 - 0.25 * min(1, follow). It is a stand-in compressor, not a real one. Snappy is the noise level, on the short decay curve.

At n = 20, t1 = 0.899876, t2 = 0.943686, y = 0.921953. Both tones are in the sum. Tone CC 0 would output t1. Tone CC 127 would output t2.

Step bend on SD is a pitch drop on top of Tune with its own envelope. It is not a deeper Pitch knob, and there is no second oscillator. While bend_st is not 0 its time is the Pitch time, but never under 80 ms, so Pitch 0 with a bend still swoops. bend_st = 0 adds nothing, and the floor does nothing. Tune 70, Pitch 0, bend_st = +12: f1[0] = 466.028122 Hz, and at n = 3,840 (80 ms) f1 = 300.694079 Hz. On the Pitch time alone (10 ms at Pitch 0) it would already be 233.068249 Hz, the same as Tune within 0.06 Hz. Pitch 127 and bend_st = +12: tau_bend is tau_p = 0.13 s, and at n = 6,240 f1 = 404.878529 Hz.

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
trig --> six squares, inharmonic --+--> mix --> band-pass --> decay --> out
      --> noise ------------------+
```

The metal is six band-limited squares (PolyBLEP) at inharmonic ratios, averaged. No sample. STAND-IN ratios, mix, band, and makeup:

```
ratios = 1, 1.4471, 1.6170, 1.9265, 2.5028, 2.6637
f0     = 180 * (900/180) ^ u(tune)          # 180 Hz to 900 Hz
mix    = 0.1 + 0.8 * u(tone)                # noise share, metal against noise
tau    = decay_tau(u(decay))

metal[n] = mean over r of square(f0 * r, n)
y[n]     = 2.0 * bandpass((1 - mix) * metal[n] + mix * x[n], 5,500 Hz, Q 0.8) * exp(-n / (fs * tau))
```

Each square starts at phase 0 on the trigger. The band-pass is the RBJ constant 0 dB peak form. Tune moves the stack, not the noise level. Tone is the mix.

Tune 64: f0 = 405.050673 Hz. Tone 70: mix = 0.540945.

## OH and HH

One colour, two decays, one choke.

```
f0     = 250 * (1000/250) ^ u(hh_tune)     # 250 Hz to 1,000 Hz, CC 73
tau_oh = decay_tau(u(oh_decay))
tau_hh = decay_tau(u(hh_decay))
y[n]   = 2.5 * bandpass(0.65 * metal[n] + 0.35 * x[n], 8,000 Hz, Q 1.0) * exp(-n / (fs * tau))
```

Each hat is the CY six-square metal at f0, mixed with noise at a fixed 0.35 share, through a band-pass at 8 kHz, times its own exp decay. STAND-IN mix, band, and makeup. Tune moves the stack, not the noise level. No hat filter knob.

A closed-hat trigger sets the open-hat envelope to 0 on that sample, and the open hat stays at 0 until the next open trigger. It does not shorten the closed hat.

Example colour, HH Tune 60: f0 = 481.257820 Hz. OH Decay 100: tau_oh = 0.276649 s. HH Decay 40: tau_hh = 0.033008 s. An open hat that has run 200 samples has envelope 0.985052. On the closed trigger that envelope is 0.

## CL

```
f   = 400 * (3000/400) ^ u(tune)           # 400 Hz to 3,000 Hz
tau = decay_tau(u(decay))
y[n] = sin(2 * pi * f * n / fs) * exp(-n / (fs * tau))
```

Tune 50, Decay 30: f = 884.244363 Hz, tau = 0.023160 s. At n = 12, y = 0.972955.

## CP

```
trig --> noise --> high-pass --> N bursts, 3 ms, 10 to 12 ms apart, panned --> stereo
                            --> one-pole low-pass, delayed tail --------------> both channels
```

```
s      = min(15, floor(trigger_cc * 16 / 128))     # CC 77, ASSUMED type
count  = 1 + floor(data_cc * 8 / 128)              # 1 to 8, ASSUMED, no CC
f_hp   = 300 * (1.2 ^ s)                           # 300 Hz to 4,622 Hz
gaps   = 480, 576, 528, 504, 552, 576, 480         # samples, 10 ms to 12 ms
tau_tr = 0.003                                     # fixed, Decay never stretches a burst
fc     = 400 * (6000/400) ^ u(filter)
tau    = decay_tau(u(decay))

hp[n]      = x[n] - lowpass(x[n], f_hp)
burst_i[n] = hp[n] * exp(-n_b / (fs * tau_tr))     # n_b = n - start_i, 0 before the start
tail[n]    = lowpass(hp[n], fc) * exp(-(n - t_0) / (fs * tau)) for n >= t_0, else 0
t_0        = start_(count-1) + 528
```

Burst i starts at the sum of the first i gaps: 0, 480, 1,056, 1,584, 2,088, 2,640, 3,216, 3,696. Each burst is the same high-passed noise under its own fixed 3 ms decay. Pan of burst i, count > 1: `p = -1 + 2 * i / (count - 1)`. One burst uses p = 0. The tail is one filtered delay: the same high-passed noise through the one-pole at fc, opening one gap (11 ms) after the last burst starts and decaying on the drum law, times 0.707107 on each channel. The one-pole runs from the hit, so it is settled when the tail opens. STAND-IN.

```
yL[n] = u(attack) * sum_i burst_i[n] * gL(p_i) + tail[n] * 0.707107
yR[n] = u(attack) * sum_i burst_i[n] * gR(p_i) + tail[n] * 0.707107
```

Data CC 48 gives count 4. At any one sample, burst i over burst i - 1 is exp(gap / 144): the bursts share one noise and one 3 ms curve.

## LTC, MTC, HTC

Each voice:

```
trig --> sine with slow FM --> Wave, ring --> out
      --> noise, if that voice's switch is on, shared level
```

Tom mode is the sine through that tom's own Wave folder, with the BD2 FM term at a quieter depth and a fixed rate: f[n] = f_tune * bend + 0.04 * f_tune * sin(2 * pi * 50 * n / fs). STAND-IN. Conga mode adds a second sine at 2.30 * f_tune, gain 0.35. ASSUMED. The manuals say the switch changes tom and conga and do not describe the waveform.

```
LTC f = 70 * (180/70) ^ u(tune)
MTC f = 100 * (280/100) ^ u(tune)
HTC f = 140 * (400/140) ^ u(tune)
```

Below CC 127 Decay is the shared decay law. At CC 127 a tom rings and then ends. It does not hold, because a tom that never ends is a stuck voice. Only BD2 drones. STAND-IN:

```
if decay_cc >= 127:
    sustain = 0.55
    tau_b   = 0.35
    ring[n] = sustain + (1 - sustain) * exp(-min(n, 192000) / (fs * tau_b))
    env[n]  = ring[n]                                         for n <= 192,000 (4 s)
    env[n]  = ring[192000] * exp(-(n - 192000) / (fs * 0.05))  after that
else:
    env[n]  = exp(-n / (fs * decay_tau(u(decay))))
```

There is no choke jack. The next hit replaces the ring, as for every drum voice.

Noise, when the voice switch is on (CC at or above 64): `u(tom_noise) * x[n] * exp(-n / (fs * 0.12))`. The 0.12 s noise decay is STAND-IN and shared. tom_noise is CC 84.

LTC Tune 40: f_tune = 94.251191 Hz (f[0], before the FM moves). Decay 127 at n = 96,000: envelope 0.551484, sustain 0.550000. At n = 192,000 (4 s) it is 0.550005. 50 ms into the release, at n = 194,400, it is 0.202335. It is under 1e-3 at n = 207,144 (4.32 s), and the tom ends there. TOM_NOISE 33: level 0.259843. On the first noise draw that is -0.136958, and on the TO/CO left channel at p = -0.7 it is -0.133176.

Bend uses the BD2 bend paragraph.

## CB

```
trig --> square at f ----+
      --> square at 1.015 f --> average --> decay --> out
```

The two squares are the brief. Detune ratio 1.015 is STAND-IN. A square is the odd harmonics k = 1, 3, 5, ... while k * f is under Nyquist and k <= 15, amplitude 1/k, divided by the sum of those amplitudes.

```
f   = 300 * (1200/300) ^ u(tune)           # 300 Hz to 1,200 Hz
tau = decay_tau(u(decay))
y[n] = 0.5 * (square(f) + square(1.015 * f)) * exp(-n / (fs * tau))
```

Tune 48, Decay 70: f = 506.607352 Hz, second square 514.206462 Hz, tau = 0.095560 s. At n = 15, y = 0.404500.

Sixteen panel steps, if a UI wants them, are Tune CC values round(i * 127 / 15) for i from 0 to 15. The voice reads the continuous CC.

## MA

```
trig --> noise --> one-pole lowpass at 1,500 Hz --> decay --> main
```

No tune knob. Colour is fixed. STAND-IN fc = 1,500 Hz. The filter is the lowpass in the shared section, held at that cutoff. Decay:

```
tau = decay_tau(u(decay))
y[n] = lowpass(x[n]) * exp(-n / (fs * tau))
```

Decay 55: tau = 0.056163 s. First noise draw x = -0.527089. a = exp(-2 * pi * 1500 / fs) = 0.821725. With filter state 0, y[0] = -0.093967 before the envelope, and the envelope at n = 0 is 1. On the main, at the equal-power centre, each side is -0.066445.

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

BD1's Pitch knob is both time and depth, one u, as the instrument chapter states. SD's pitch control is depth, with the time law above. Step bend is an extra signed depth on BD1, BD2, SD, LTC, MTC, and HTC only. On SD it has its own envelope with the 80 ms floor, written in the SD equation. CY, RS, hats, clave, clap, cowbell, and maracas have no step bend. EXT ignores step bend.

## Init kit (INIT)

STAND-IN. A balance set by ear from the knob ranges above, with no samples. A new engine loads it. reset() still clears to empty knobs, empty steps, level 1, and master 1, which is the state every named test arms from. The plugin writes its own parameters over both on the first block.

Knob CC is round(u * 127).

| Voice | Knobs, u (CC) | Level |
| --- | --- | --- |
| BD1 | Tune 0.22 (28), Pitch 0.55 (70), Decay 0.28 (36), Attack 0.45 (57), Dist 0.15 (19), Noise 0.08 (10), Filter 0.35 (44) | 0.85 |
| BD2 | off | 0 |
| SD | Tune 0.48 (61), Detune 0.12 (15), Pitch 0.35 (44), Tone Decay 0.22 (28), SN Decay 0.30 (38), Snappy 0.62 (79), Tone 0.45 (57) | 0.75 |
| RS | Tune 0.62 (79) | 0.4 |
| CP | Data 48 (4 bursts), Attack 0.7 (89), Decay 0.25 (32), Filter 0.55 (70) | 0.7 |
| HH | Decay 0.08 (10), Tune 0.7 (89) | 0.45 |
| OH | Decay 0.34 (43), Tune shared with HH | 0.4 |
| CY | Decay 0.55 (70) | 0.35 |
| LTC, MTC, HTC | Tune 0.30, 0.42, 0.55 (38, 53, 70), Decay 0.32, 0.28, 0.24 (41, 36, 30) | 0 |
| CL, MA, CB | default | 0 |
| Lead, bass | default, no steps | 1 |

Every knob not listed keeps its Knobs default, including Wave at CC 32 on BD1, BD2, and the toms. Master 0.7, 120 BPM, 16ths, INT.

Pattern "INIT", length 16, every track length 16, no step on. The factory patterns are cleared for now; tests that need steps arm their own.

CHOICE: the closed hat skips the open-hat steps. A closed trigger on the same sample chokes the open hat, so a closed hat on every step would leave the open hat silent.

Not in the engine, so not in the kit: a hat filter knob, a master Drive, and a global Accent amount. The sheet called the BD1 and SD sweep "Bend"; here that is the Pitch knob. Step bend stays off. One bar peaks at 0.943890 on the main.

## Cleared voice

A voice that has never been triggered in the block under test outputs 0 on every sample. A drum step that is off does not trigger. A note-track rest does not open the gate. Neither event produces a new nonzero sample from a cleared voice.
