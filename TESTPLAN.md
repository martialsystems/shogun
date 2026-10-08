# Test plan (spec v2.2, 2026-10-08)

The contract for the v2.2 engine (`engine/`), the plugin shell (`plugin/`) and the web build (`web/`). Each test below is a §15.4 test of `SHOGUN_Redesign.md` v2.2 (or a test this pass added), compiled into `build/shogun_tests` from `tests/blocks.cpp`, `tests/voices.cpp`, `tests/engine.cpp`, `tests/mod.cpp`. Every number is the line the test prints at 48 kHz, double precision; the tolerance column is what the named check uses. `make test` runs them (plus the forge law test), `make asan` runs them under ASan/UBSan.

Conventions: u ∈ [0, 1] for every parameter (SECTION:LABEL ids); 1 V/oct with 0 V = C3 = note 48; accent volts 2.353/3.706/5.0 V; voice end at −90 dB (kQuiet 3.162e-5); OS 2× unless a row says otherwise (latency 23 base samples).

Result of the last run: **all 309 named checks passed** (`make test`, `make asan`; g++ 14.2 and clang++ 19.1 print identical output).

§15.4 tests not in this suite: `testRackLatencyComp` (belongs to jidai-rack, which this pass does not touch) and the v2.1 INIT-kit test (superseded by v2.2 §14.1: INIT ships an empty pattern; `testInitKitAndEmptyPattern` checks that instead).

## DSP blocks (tests/blocks.cpp)

### testDecayIsRateIndependent

§3.1 host-rate coefficients: the −60 dB time of one decay setting is the same at 44.1/48/96 kHz (closed form 940.802 ms).  Tolerance: tol 0.1 ms.

```
-60 dB at 44100 Hz 940.816 ms (closed form 940.802)
-60 dB at 48000 Hz 940.812 ms (closed form 940.802)
-60 dB at 96000 Hz 940.802 ms (closed form 940.802)
```

### testVoiceEndsAtMinus90

§3.6 a voice ends at −90 dB (kQuiet 3.162e-5): end sample = ceil(ln6·τa·fsE) + ceil(ln(1/kQuiet)·τ·fsE), /M ceil. BD1 example 67,738, Decay 64 38,426.  Tolerance: exact sample.

```
BD1 example end 67738, Decay 64 end 38426 (threshold 3.162e-05)
BD1 example ends at n = 67738 (spec 67,738); Decay 64 at 38426 (spec 38,426)
```

### testSvfMinus3dB

§4.1 TPT SVF LP is −3.0103 dB at fc at every host rate.  Tolerance: 1e-3 dB.

```
LP at fc 44100 Hz: -3.0103 dB
LP at fc 48000 Hz: -3.0103 dB
LP at fc 96000 Hz: -3.0103 dB
```

### testResonatorTau

§4.2 resonator decay τ equals the target at every rate, also with the BD1 pitch sweep (quadrature damping correction).  Tolerance: 1e-3 ms.

```
tau at 44100 Hz 136.1950 ms (target 136.195)
tau at 48000 Hz 136.1950 ms (target 136.195)
tau at 96000 Hz 136.1950 ms (target 136.195)
tau with the BD1 pitch sweep 136.1958 ms
```

### testResonatorPeak

§4.2 resonator impulse peak stays bounded across 35 Hz to 3 kHz.  Tolerance: printed.

```
peak at 35 Hz 0.8700
peak at 140 Hz 0.9650
peak at 1000 Hz 0.9908
peak at 3000 Hz 0.9603
```

### testRetriggerNoClick

§4.3 retrigger without a phase reset: max sample step 0.01005 (old reset 1.0); BD1 engine retrigger 0.0032 vs free-running 0.0013.  Tolerance: limit 0.02 + free.

```
max sample step with a retrigger 0.01005 (old phase reset 1.0)
BD1 retrigger max step 0.0032 (free-running 0.0013), limit 0.02 + free
```

### testLadderResponse

§4.4 ladder gain at fc for k 0/2/3.5 (exact −12.041/−6.021/+6.021 dB); at the verify FFT bin 1000.49 Hz −12.050/−6.038/+5.953 dB (spec −12.05/−6.04/5.95).  Tolerance: 0.01 dB.

```
k 0.0 gain at fc -12.041 dB (exact -12.041), at the verify bin 1000.49 Hz -12.050 dB (spec -12.05)
k 2.0 gain at fc -6.021 dB (exact -6.021), at the verify bin 1000.49 Hz -6.038 dB (spec -6.04)
k 3.5 gain at fc 6.021 dB (exact 6.021), at the verify bin 1000.49 Hz 5.953 dB (spec 5.95)
```

### testLadderSelfOsc

§4.4 no self-oscillation at k 3.8, self-oscillation at k 4.2; Newton ≤ 3 iterations.  Tolerance: printed.

```
k 3.8 rms of the last 0.5 s 0, peak 0.0012
k 4.2 rms of the last 0.5 s 0.0759, peak 0.1074
Newton iterations max 3 mean 2.318, hard-drive peak 0.7465
```

### testOtaSvfBounded

§4.5 OTA SVF at R 0 is bounded (impulse peak 0.1042 = verify).  Tolerance: 1e-4.

```
R 0 impulse peak 0.1042 (verify 0.1042); R 0 hard input peak 6.4877
```

### testHalfbandSpec

§3.4 decimators: stage 1 93 taps ≥ 110 dB stopband, stage 2 25 taps ≥ 120 dB. Stage 1 is the shared exact halfband `jidai::dsp::Halfband93` (jidai-common, polyphase, every even-offset tap 0 except the centre, as §3.4 asks). Stage 2 is SHOGUN's own 25-tap equiripple, which is not exact halfband; the shared header has no 4× stage.  Tolerance: spec mins; 46 zero taps; 23 base samples per direction.

```
stage 1 (shared exact halfband, 93 taps, 46 even-offset zeros) stopband 111.64 dB ripple 4.53e-05 dB; stage 2 (25 taps) stopband 123.11 dB
```

### testDecimatorDelay

§3.4 the decimator chain is a pure delay of 23 (2×) / 26 (4×) base samples.  Tolerance: 1e-6.

```
2x output = input delayed 23 base samples, max error 1.22e-07
4x output = input delayed 26 base samples, max error 5.19e-07
```

### testBlepAlias

§3.5 PolyBLEP saw alias at 2× (−60.1 dB at 1318.5 Hz).  Tolerance: spec limits.

```
BLEP saw 1318.51 Hz at 2x, worst alias -60.1 dB
BLEP saw 3500.00 Hz at 2x, worst alias -52.3 dB
```

### testTanhAdaaAlias

§3.5 ADAA tanh alias −77.8 dB (verify −77.8).  Tolerance: 0.1 dB.

```
4987 Hz, drive 4, 2x ADAA worst alias -77.8 dB (verify -77.8)
```

### testOuDriftStd

§5 OU drift: ρ 0.999556, std 3.990 cents over 20,000 s (target 4).  Tolerance: 0.1 cent.

```
rho 0.999556, std over 20,000 s 3.990 cents
```

### testSmoothing

§3.2 5 ms one-pole smoothing coefficient at 48 kHz.  Tolerance: 1e-6.

```
5 ms coefficient at 48 kHz 0.995842
```

### testLevelLaw

§9.1 gLevel = 1.4125·u².  Tolerance: 1e-4.

```
u 0.25 -> 0.0883
u 0.50 -> 0.3531
u 0.75 -> 0.7946
u 1.00 -> 1.4125
```

### testCentrePan

§9.1 equal-power centre pan 0.707107.  Tolerance: 1e-6.

```
centre 0.707107 / 0.707107
```

### testGateHysteresis

JCS R3 Schmitt gate 1.0/0.5 V (the shared `jidai::jcs::Schmitt`, float volts); plain 0/5 V gates give one edge each.  Tolerance: exact.

```
0.0 V -> edge 0 high 0
0.9 V -> edge 0 high 0
1.1 V -> edge 1 high 1
0.7 V -> edge 0 high 1
0.4 V -> edge 0 high 0
1.1 V -> edge 1 high 1
0/5 V logic gate, 4 gates -> 4 edges
```

### testWaveBypass

§4.6 WAVE 0 with trims/SYM/SHAPE 0 is a true bypass (bit-identical).  Tolerance: exact.

```
WAVE 0, trims/SYM/SHAPE 0: bypassed 1, output bit-identical 1, level gain untouched 1.0
```

### testWaveMigration

§4.6 v2.1 → v2.2 WAVE migration (macro + trim 2), max error 0.  Tolerance: exact.

```
max error v 0..127 0; v32 m 0.126 trim2 -0.000; v90 m 0.354 trim2 -0.209; v127 m 0.500 trim2 -0.500
```

### testWaveStageLaw

§4.6 stage law: s(0)=0; SYM +1 triangle and saw harmonic levels.  Tolerance: 0.1 dB.

```
max |s(0)| 0; SYM +1 triangle: H1 -240.0 H3 -240.0 H4 -14.0 dB re 2F; saw: H2 -14.0 H3 -21.3 dB re 1F
```

### testWaveAliasStatic

§4.6 2× ADAA alias levels (verify −60.0/−117.8/−121.9 dB).  Tolerance: 0.1 dB.

```
398 Hz, WAVE 1.0, 2x ADAA alias -60.0 dB (verify -60.0)
220 Hz, WAVE 1.0, 2x ADAA alias -117.8 dB (verify -117.8)
1031 Hz, WAVE 0.5, 2x ADAA alias -121.9 dB (verify -121.9)
```

### testWaveAudioRateVc

§4.6 audio-rate VC alias (verify −58.4/−115.3/−50.8 dB) and unsmoothed VC sidebands (+1.0 dB).  Tolerance: 0.1 dB.

```
body 257.8 Hz, VC 1353.5 Hz, VC -> AMT depth 0.5: 2x ADAA alias -58.4 dB (verify -58.4)
body 257.8 Hz, VC 1353.5 Hz, VC -> SYM depth 0.5: 2x ADAA alias -115.3 dB (verify -115.3)
body 257.8 Hz, VC 1353.5 Hz, VC -> AMT + SYM depth 0.5: 2x ADAA alias -50.8 dB (verify -50.8)
VC sideband energy re harmonics 1.0 dB (unsmoothed reference +1.0, a 5 ms smoother gives -35.8)
```

### testWavePlanBlock

jidai-common 1.1.1 per-block stage skipping (§4.6, `TripleShaper::planBlock`). With WAVE 0.25 (stage 1 live, stages 2 and 3 at a = b = 0), a steady block makes stages 2 and 3 wires. A moving block (ramp or modulation) skips nothing. A live VC with depth on stage 3 keeps it; the same depth with VC not live does not. A moving block keeps the ADAA half-sample delay on its (0, 0) stages instead of switching to the wire.  Tolerance: exact.

```
wires (stage 1 2 3) steady 011, moving 000; VC depth on 3: live 010, not live 011; moving block vs wire max diff 1.464e-01 (ADAA half-sample delay kept)
```

In the engine the block is one base sample (SHOGUN reads its parameters once per base sample). `WaveSlot::control` calls it steady when the 13 stage controls (WAVE, WAVE 1 to 3, SYM 1 to 3, VC→AMT 1 to 3, VC→SYM 1 to 3) equal the previous base sample's and none is ramping (smoother) or modulated (matrix row, CV jack); VC is live when the voice's VC LEVEL is above 0.

### testWaveLevelComp

§4.6 LEVEL COMP keeps 200 random settings within −1.86..+0.20 dB (off: −13.9..+11.6).  Tolerance: ±2 dB.

```
200 random settings, output re input RMS: comp OFF -13.9..11.6 dB, comp ON -1.86..0.20 dB
```

### testWavePreVca

§4.6 PRE-VCA folder input follows the envelope (max deviation 0.0358 FS, verify 0.036).  Tolerance: 0.002.

```
PRE-VCA folder-input peak vs envelope, max deviation 0.0358 FS (verify 0.036); at 250 ms 0.128
```

### testShapeMorph

§4.6 SHAPE morph alias (−62.2/−109.1 dB = verify) and 0.000 % quadrature ripple.  Tolerance: 0.1 dB.

```
398 Hz at 2x: SHAPE 1.0 (saw) alias -62.2 dB (verify -62.2), SHAPE 0.5 (tri) -109.1 dB (verify -109.1)
quadrature amplitude ripple at 35 Hz (2x of 48k): 0.000 %
quadrature amplitude ripple at 70 Hz (2x of 48k): 0.000 %
quadrature amplitude ripple at 400 Hz (2x of 48k): 0.000 %
quadrature amplitude ripple at 1000 Hz (2x of 48k): 0.000 %
```

## Voices (tests/voices.cpp)

### testKickBendDecays

§6.1 BD1 pitch sweep decays toward f_tune; bend −12 halves f(0). y(48)/y(480) locked at first build.  Tolerance: 1e-5.

```
f(0) 83.814360 Hz, f(2400) 72.957296 Hz, f_tune 60.408707 Hz
y(48) 0.143497, y(480) -0.154601
bend -12 f(0) 41.907180 Hz
```

### testVoiceEndsWhenQuiet

§3.6 every drum ends and the main output is exactly 0 afterwards; BD2 Decay 127 holds (env 0.70).  Tolerance: exact.

```
BD1 example end 67738 (limit 3.162e-5), main exactly 0 after: 1, retrigger y(48) 0.143497 vs 0.143497
BD1 with Noise 127 ends at 67738
BD1 ends at n = 37751, then 0: 1
BD2 ends at n = 37768, then 0: 1
SD  ends at n = 37768, then 0: 1
RS  ends at n = 5974, then 0: 1
CP  ends at n = 39880, then 0: 1
CL  ends at n = 37751, then 0: 1
MA  ends at n = 37768, then 0: 1
CB  ends at n = 34470, then 0: 1
CH  ends at n = 37768, then 0: 1
OH  ends at n = 37768, then 0: 1
CY  ends at n = 37768, then 0: 1
LTC ends at n = 37751, then 0: 1
MTC ends at n = 37751, then 0: 1
HTC ends at n = 37751, then 0: 1
BD2 Decay 127 active after 10 s: 1, env 0.700000
```

### testDecayNoonIsShort

§3.6/§6.0 decay τ 8 / 75.902 / 720.137 ms at u 0/0.5/1; per-voice end samples (instant env 38,426, 0.2 ms VCA attack 38,443, CP one burst 38,971).  Tolerance: exact sample.

```
decay_tau 8.000 / 75.902 / 720.137 ms at u = 0 / 0.5 / 1
Decay 64 decay-only end 38426 (spec 38,426); VCA voices (0.2 ms attack) 38443
BD2 ends at 38443 (want 38443)
CY  ends at 38443 (want 38443)
OH  ends at 38443 (want 38443)
CH  ends at 38443 (want 38443)
CL  ends at 38426 (want 38426)
MTC ends at 38426 (want 38426)
MA  ends at 38443 (want 38443)
SD  ends at 38443 (want 38443)
CP (one burst) tail opens at 528, ends at 38971 (want 38971)
CB at n = 38426: 0
```

### testBd2CanHold

§6.2 BD2 Decay 127 holds at 0.70, 126 does not; click transient nonzero.  Tolerance: 2e-4.

```
env(96000) Decay 127 0.702023, Decay 0 0.000000, Decay 126 0.056308
Tune 60 Tone 100 click at n = 10: 0.131217 (nonzero)
```

### testBd2FullHolds

§6.2 BD2 full decay holds 0.70 for 10 s; next hit restarts at 1.0.  Tolerance: 1e-6.

```
env at 10 s 0.700000 (S 0.70), next hit peak 1.000000
```

### testTomFullEnds

§6.7 tom full decay ring ends at 215,434; a second hit restarts the ring.  Tolerance: exact.

```
ring(192000) 0.550005 active 1, ring(194400) 0.202335, ends at 215434 (want 215434)
second hit at 2 s restarts the ring envelope: 0.999973
```

### testSnareBendAtPitchZero

§6.3 SD tone 1 with bend at PITCH 0 and PITCH 127.  Tolerance: 1e-5.

```
f1 466.028122 Hz at n = 0, 300.694079 at 3,840; bend 0 233.014061; Pitch 127 at 6,240 404.878529
```

### testHatChoke

§6.6 CH chokes OH with a 1.5 ms fade (e^-1 at +72 samples), OH ends +745.  Tolerance: 1e-3.

```
OH env 0.986351 before choke, 0.972746 / 0.590000 / 0.357853 at +0 / +36 / +72 samples, OH ends +745
CH own tau 0.033008 s (not cleared by its own trigger)
```

### testClapBurstCount

§6.5 clap burst starts 0/528/1056/1584; burst ratio e^{528/144} exact.  Tolerance: 1e-9.

```
count 4 starts: 0 528 1056 1584; count 1 starts: 1
burst1/burst0 at n = 1,589: 39.121283998 (e^{528/144} = 39.121283998)
```

### testCpNoAlias

§6.5 burst centre below 0.45·fs at 44.1 kHz.  Tolerance: limit.

```
burst centre at s = 15: 9244.2 Hz; 0.45·fs at 44.1 k = 19845.0 Hz
```

### testNoonIsADrumMachine

§6.0 noon kit: BD1 f(100 ms) 89.931 Hz (spec says 70 ±3 % but its own PITCH/τ pins give 89.93: deviation), SD noise share 0.279 ≥ 0.25, tone peak 228.5 Hz (measured 100 to 400 ms: deviation).  Tolerance: printed.

```
BD1 noon f(100 ms) 89.931 Hz, -60 dB time 524.3 ms, SOUND 0 WAVE 0.00 DRIVE 0.00
SD noon noise share (20-200 ms) 0.279, tone peak (100-400 ms) 228.5 Hz
```

### testDistBypassAndDrive

§6.1 BD1 DRIVE 0 is a bypass, drive raises level (values locked at first build).  Tolerance: 1e-5.

```
n = 48: Dist 0 0.143497, Dist 1 0.143073, Dist 64 0.170431
```

### testCenterPanIsNotHalf

§9.1 centre pan 0.707107 per side; CP Attack 0 tail only.  Tolerance: 1e-6.

```
MA centre: main L/voice 0.707107, R/voice 0.707107
CP Attack 0: silent before 528 1, L = R after 1
```

### testIndividualOutStaysInMix

§9.2 patching OUT does not remove the voice from the mix; master −6 dB halves main only.  Tolerance: 1e-6.

```
n = 48: OUT 0.712506 V, main L 0.100764 (patched), main L 0.100764 (unpatched), master -6 dB main 0.050382 OUT 0.712506 V
```

### testRetNormal

§9.2 RET normal: unpatched RET = OUT; RET patched to 0 V silences the voice in the mix.  Tolerance: exact.

```
RET unpatched main peak 0.124851; RET patched 0 V main peak 0, OUT peak 0.8828 V
```

### testVelNormal

§4.8 VEL normal = accent volts 2.353/3.706/5.0 → g_vel 0.55/0.78/1.00.  Tolerance: 1e-4.

```
accent 1 -> VEL normal 2.353 V, g_vel 0.5500
accent 2 -> VEL normal 3.706 V, g_vel 0.7800
accent 3 -> VEL normal 5.000 V, g_vel 1.0000
```

## Engine, clock, ports (tests/engine.cpp)

### testExtBypassIgnoresPattern

§12.2 forge pin: EXT ignores the pattern, TRIG plays (g_vel 0.819291, y(10) = ref × g); back in INT the pattern plays.  Tolerance: 1e-6/1e-9.

```
BD1 silent over step 0 in EXT: 1; display 1 -> 2; TRIG vel 100: g_vel 0.819291, y(10) 0.031264 = 0.038160 x 0.819291
after INT: 2 pattern hits (flam pairs) in the next bar, first at +12988; TRIG edge ignored
```

### testIntIgnoresTrigJacks

§12.2 forge pin: INT ignores TRIG jacks.  Tolerance: 1e-9.

```
TRIG edge in step 1 (INT) silent: 1; step 0 y(10) 0.038160 = ref 0.038160 x 1.000000
```

### testRestIsSilent

§10.2 rest steps output 0 (BD1/LEAD/BASS).  Tolerance: exact.

```
BD1 rest step outputs 0: 1
LEAD rest step outputs 0: 1
BASS rest step outputs 0: 1
```

### testSoloMutesOtherVoices

§9.1 SOLO isolates a voice without moving the counter; MUTE wins.  Tolerance: exact.

```
soloed SD main vs SD alone max diff 0; counter same 1; solo off main vs no-solo diff 0
soloed + muted SD direct trigger main peak 0
```

### testShuffleSurvivesOddLength

§10.2 swing on odd-length tracks (hits 0 8000 12000 20000 24000 32000).  Tolerance: exact.

```
delay 2000 samples; hits: 0 8000 12000 20000 24000 32000
```

### testInitKitAndEmptyPattern

§14.1 INIT kit + empty 16-step pattern "001 INIT" (replaces the v2.1 INIT-kit test). Test beat peak −16.05 dBFS.  Tolerance: 1e-4.

```
pattern '001 INIT', every track 16 long: 1, empty: 1, params at INIT: 1, one bar silent: 1
test beat main peak 0.157557 (-16.05 dBFS), OH on step 3 env max 1.000
```

### testControlOutsZeroLatency

§3.4/JCS: ENV and gate outputs are at n with no OS latency; audio OUT lags 23.  Tolerance: exact.

```
step at n = 0: 2x BD1:ENV 3.8989 V, MOD:LD GATE 5.0 V at n = 0; BD1:OUT lag 23 samples
```

### testOsLatency

§3.4 latency 0/23/26; every audio path (main, OUT, AUX) lags exactly that.  Tolerance: ±1 (ADAA half sample).

```
latencySamples 1x/2x/4x = 0 / 23 / 26
2x lags main 23, BD1:OUT 23, AUX 1 23; voice + bus + master drive main 22
4x lags main 26, BD1:OUT 26, AUX 1 26; voice + bus + master drive main 25
```

### testPitchVoct

§7.1 1 V/oct, 0 V = C3 (note 48): NOTE OUT and NOTE IN exact, V/OCT +1 V = 12 st.  Tolerance: 1e-6.

```
NOTE OUT note 36 = -1.0000 V, flag 0
NOTE OUT note 48 = +0.0000 V, flag 0
NOTE OUT note 60 = +1.0000 V, flag 0
NOTE OUT note 72 = +2.0000 V, flag 0
NOTE OUT note 24 = -2.0000 V, flag 0
NOTE OUT note 108 = +5.0000 V, flag 0
NOTE IN -1 V -> note 36.0000, pitch error 0.00000 cents
NOTE IN +0 V -> note 48.0000, pitch error 0.00000 cents
V/OCT +1 V adds 12.000000 semitones
NOTE IN +1 V -> note 60.0000, pitch error 0.00000 cents
NOTE IN +2 V -> note 72.0000, pitch error 0.00000 cents
```

### testPitchRail

§7.1 NOTE OUT rail ±5 V with over-range flag.  Tolerance: exact.

```
note 109 (OCT +1 on 97) +5.0000 V flag 1; note -13 -5.0000 V flag 1; max |NOTE OUT| over -40..150 5.0000
```

### testTuningIsNotTheVoltLaw

§7.1 TUNE/A4 move the sound, not NOTE OUT (+1.0000 V, 278.4426 Hz).  Tolerance: 1e-4.

```
TUNE +100 c, A4 442: NOTE OUT +1.0000 V, audible f 278.4426 Hz (law 278.4426, untuned 261.6256)
```

### testLin55Migration

§12.3 BASS:HZ/V → BASS:NOTE with V' = log2 V − 1.25 (2.0 V → 110.000000 Hz); TOM:PITCH fans out to 3 toms at CV AMT 1/12.  Tolerance: 1e-6.

```
BASS:HZ/V -> BASS:NOTE, law 1
2.0 V plays 110.000000 Hz (0.00000 cents from 110)
```

### testJackIdsAndTypes

§13.1/13.2: 153 ports, ids exact, unique, types/roles, plainVoltGates.  Tolerance: exact.

```
153 ports, ids exactly as 13.2: 1, unique 1, type/role errors 0, plainVoltGates 1
```

### testJackIdsJcsShared

JCS R6/R14 through the shared jidai-common headers. Every one of the 153 port ids and every alias-table id is parsed whole by `jidai::jcs::parseJackId` in three forms: bare, `SHOGUN/` and `SHOGUN#1/`. Labels keep '/', spaces and digits: LEAD:V/OCT, LEAD:HZ/V OUT, MOD:LD GATE, MOD:LFO 1. `resolvePort` maps each form (and `SHOGUN#12/`) to the same port. Another device's prefix is refused. The three v2.0/2.1 names with no SECTION: (MIX L, MIX R, LFO OUT) are not R6 ids; they are matched whole against the alias table. All 454 parameter ids are R6 SECTION:LABEL ids. Role colours come from the shared table.  Tolerance: exact.

```
ports 153/153 round trip whole in 3 forms (labels with '/' 2, with spaces 22, ids with digits 24)
alias-table ids 25, R6 ids round trip 22, legacy non-R6 names matched whole 3: [MIX L] [MIX R] [LFO OUT]; foreign prefix refused 1; params 454/454 valid R6 ids; PITCH colour #6590f3
shared AliasTable 6 renames (BASS:HZ/V -> BASS:NOTE, Lin55ToVoct(2.0 V) = -0.25 V); shim: fan-out 2 (shared add of a 2nd target refused 1), legacy names 3 (shared add refused 1)
```

Since jidai-common 1.1.1 the one-to-one renames (LEAD/BASS:HZ/V → :NOTE with `AliasLaw::Lin55ToVoct`, LEAD/BASS:HZ/V OUT → :NOTE OUT, SD:SNAPPY → SD:TONE, LFO:OUT → MOD:LFO 1) are rows of the shared `jidai::jcs::AliasTable`, resolved with its `resolve()`; the engine applies the returned `AliasConversion`. The test also shows why the rest stays in a SHOGUN shim: the shared table refuses a second target for one old id (HAT:DECAY, TOM:PITCH fan out) and refuses ids with no SECTION: (MIX L, MIX R, LFO OUT).

Before this change, no bare id was mis-split. SECTION and LABEL never contain ':', so a first-':' split was right, and the engine matched ids whole. What failed was the prefixed forms:
- The plugin state loader stripped only `SHOGUN/`, so every `SHOGUN#N/` id failed: 153 ports plus 11 alias names.
- The web `sg_port_find` matched only bare ids, so all 153 failed in both `SHOGUN/` and `SHOGUN#N/` forms.

Now every external id goes through `resolvePort` / `portFromId` (engine/ports.h, which calls `parseJackId`): the plugin state loader (cables, CV AMT, inLaw), editor jack binds, wasm `sg_port_find`, and the web compat patch loader. The engine also builds in the freestanding wasm target, with small `<string>`, `<string_view>` and `<optional>` shims in web/wasm/include. The editor's mod-destination menu splits parameter ids with `parseJackId`.

### testHostLock

§10.1 host lock: ppq 7.25 → step 29; a transport jump re-syncs at block sample 0.  Tolerance: exact.

```
ppq 7.25 at 1/16 -> step 29; before jump step 29; after jump to 0 synced at block sample 0, BD1 hit yes
```

### testAccOutFollowsPattern

§13.2 ACC OUT = step accent volts while the step is on, in INT and EXT (EXT voice silent). Added this pass.  Tolerance: 1e-6.

```
INT: ACC OUT step 0 5.000 V, step 4 2.353 V, step 8 (off) 0.000 V; BD2 played 1
EXT: ACC OUT step 0 5.000 V, step 4 2.353 V, step 8 (off) 0.000 V; BD2 played 0
```

## Modulation (tests/mod.cpp)

### testModSumLaw

§8.4 u_eff = clamp(u_base + Σd·C(s)·via + AMT·V/5); FREE rate law; engine matrix path matches.  Tolerance: 1e-4.

```
law: 0.5 + 0.18 = 0.68 (tau 170.62 ms); + 2.5 V -> 1.00; 0.2 - 0.3 x VEL 0.5 = 0.05; SOUND 0.40 + 0.10 -> 8
FREE rate 0.0100 / 0.6325 / 40.0000 Hz at u = 0 / 0.5 / 1
engine: DECAY u_eff 0.6800 (tau 170.62 ms), +2.5 V -> 1.0000; TUNE via VEL 0.5 -> 0.0500; SOUND index 8
```

### testOwnVoiceRetrigIndependent

§8.3 OWN VOICE LFO instances restart per voice independently.  Tolerance: 1e-6.

```
n = 12,000: BD1 0.000000 SD 0.000000; n = 18,000: BD1 -1.000000 SD 1.000000; BD1 retrig: BD1 0.000000, SD 1.000000 (unchanged path 1.000000)
```

### testLfoHostPhase

§8.3 SYNC FREE-RUN re-anchored φ = frac(ppq/D): 4.24e-13 cycles over 600 s (verify 6e-13).  Tolerance: 1e-9.

```
worst re-anchored phase error over 600 s: 4.24e-13 cycles (limit 1e-9)
```

### testLfoDeclick

§8.2 declick τ = max(0.25 ms, SLEW·T/4): max step 0.1460, HF −35.77 dB (verify 0.146/−35.8).  Tolerance: limits.

```
40 Hz square, PolyBLEP + 0.25 ms declick: max step 0.1460 (limit 0.15), energy above 2 kHz -35.77 dB (limit -35)
```

### testShSlew

§8.2 S&H SLEW 1 at 1/4 120 BPM: 63 % at 125.021 ms (τ 125 ms).  Tolerance: 0.1 ms.

```
SLEW 1 at 1/4, 120 BPM: tau 125 ms, 63 % at 125.021 ms
```

## Plugin shell (ShogunProbe, `make probe-linux`)

The probe builds the real `ShogunAudioProcessor`/editor (JUCE 8.0.4) and checks the shell laws of §15.0 step 4. Last run, all PASS:

```
PASS params  count 454 (table 454), float 0..1 SECTION:LABEL ids, mismatches 0
PASS latency  0/23/26 samples
PASS host rate  engine fs 44100.0 Hz at host 44100
PASS INIT silent  peak 0.000000, pattern '001 INIT'
PASS MIDI 36 -> BD1  peak 0.149777
PASS INT test beat  peak 0.116818 = -18.65 dBFS
PASS EXT ignores pattern  peak 0.000000
PASS host lock  ppq 7.25 -> global step 29 (bar-of-16 display 14), step-29 hit peak 0.116818
PASS aux 1/2 bus  channels 4, aux peak 0.149767, main peak 0.000000
PASS state round trip  XML <SHOGUN version=2> JSON 11564 chars; params/pattern/lock/mod/cable restored
PASS alias load  cables 4 (1 + 3 toms) = 4, BASS:NOTE law 1, MTC:PITCH CV AMT 0.083333
PASS old HZ/V plays  BASS:HZ/V cable at 2.0 V -> 110.000000 Hz (0.00000 cents from 110), law 1
PASS editor  1200x672, bay jacks 153/153, ops 2622
PASS tab renders  8 PNGs in /workspace/shogun/build/plugin-linux/tabs
ShogunProbe: all checks passed
```

Tab renders: `build/plugin-linux/tabs/tab_<i>_<name>.png` (1200 × 672) next to the spec mockups; mean absolute pixel difference per tab (0 to 255): MAIN 4.7, VOICE 5.8, GRID 12.4, MOD 7.5, ROUTE 8.6, FX/MIX 6.0, SEQ/MIDI 5.0, GLOBAL 4.3. The differences are the mockups' illustrative data (fake pattern, matrix rows, cables, meter levels), which the plugin replaces with live state (INIT: empty), plus font rasterisation.

## Web build (`make web`)

`build/shogun.wasm` is the same engine (freestanding, `-DSHOGUN_NO_FORMAT`), run at the AudioContext rate. `web/test_wasm.mjs` runs `web/parity_scenario.txt` through wasm and the native build of the same facade (`build/web_parity`) and compares every sample, then checks the bay and LFO laws and the jack-id forms. After the switch to the shared exact-halfband stage 1, the largest wasm/native difference went from 9.68e-10 to 3.74e-9, and with jidai-common 1.1.1 (exact C3 in the synth pitch) to 3.77e-9 (float32 output, same code on both sides; the wasm build imports JS Math):

```
132000 samples, peak 0.310, largest wasm/native difference 3.77e-9 at 92575
wasm matches the native engine
ok   CLK OUT into BD1 Trig, INT: silent
ok   CLK OUT into BD1 Trig, EXT: fires
ok   ACC OUT into BD1 Trig, EXT, no loud step: silent
ok   ACC OUT into BD1 Trig, EXT, one loud step: fires
ok   ACC OUT into BD1 Trig, INT, one loud step: silent
ok   LFO 1/16 at 120 BPM is 8 Hz (period 6000.00 samples)
ok   LFO sine at amount 1 spans 0 to 5 V around 2.5 V (0.0002 .. 4.9998)
ok   LFO starts at phase 0 on transport start (2.5000 V)
ok   LFO amount 0 is 0 V
ok   LFO sample and hold moves only in the 5 ms after each cycle start (0 stray changes)
ok   44.1 kHz: rate 44100, latency 23, params 454, ports 153, INIT peak 0
ok   44.1 kHz: BD1 hit peak 0.1498
ok   jack ids: 153/153 resolve in bare, SHOGUN/ and SHOGUN#1/ forms (e.g. LEAD:V/OCT 115); legacy names ok; foreign prefix refused true
ok   LFO OUT into BD1 PITCH changes the kick
```

## jidai-common (vendored shared headers)

`third_party/jidai-common` is jidai-common **1.1.1**, jidai-collection `redesign/jidai` at **24ee621**, copied verbatim with `git archive` (see `VENDORED.md`; the previous copy was 1.1.0 at 9d6e382).
SHOGUN uses these parts of it:

- `jcs::Schmitt` for every trigger, clock, reset, run and gate input;
- `jcs::pitch` `kC3Hz` (exact 130.8127826502993 Hz; the synth voice pitch), `note`, `voltsForNote`, `clampPitch`, `lin55ToVoct` (through the alias law);
- `jcs::Role`, `roleInfo` and `roleArgb`;
- `jcs::parseJackId`, `isValidLocalId` and `AliasTable` with `AliasLaw` / `AliasConversion`;
- `dsp::TripleShaper` (with `planBlock`), `ShaperControls`, `ShaperStage`, `macroAmounts`, `LevelComp` and `DcBlocker`;
- `dsp::Halfband93`, `Downsampler2x` and `Upsampler2x` (decimator and RET upsampler stage 1).

**Removed local copies:** `engine/jidai_local.h` and `engine/jidai/dsp/TripleShaper.h` (at 9d6e382); with 1.1.1 also the per-stage wire workaround in `engine/wave_shaper.h`, the local lin55 formula in `engine/shogun.cpp` and the local alias table in `engine/ports.h`.

**Wrapper:** `engine/wave_shaper.h`, class `shogun::WaveShaper` with `shogun::TripleShaperParams`: one shared `TripleShaper`, planned once per block by `setParams(p, steady, vcLive)`, plus SHAPE morph, PRE-VCA and the v2.1 migration.

**Workarounds removed with 1.1.1** (upstream now covers them):

1. **Wire rule.** Was: three shared `TripleShaper` instances (one live stage each) plus an ADAA-at-zero replacement, to keep the spec's whole-render rule against the per-sample skip. Now: the shared `planBlock(ctl, steady, vcLive)` once per block (testWavePlanBlock).
2. **lin55.** Was: a local `log2(max(V, 1e-3)) − 1.25` because the shared function was 1.9e-7 V low. Now: the shared `lin55ToVoct` (exactly `log2 V − 1.25`) through `AliasLaw::Lin55ToVoct`. 2.0 V still plays 110.000000 Hz (0.00000 cents). Remaining difference from the spec's wording: for V ≤ 0 the shared law gives −5 V (the rail) and for 0 < V < 1e-3 it goes below log2(1e-3) − 1.25 = −11.2 V, where spec §12.3 floors V at 1e-3. Both are below the lowest playable note, so no test number moves.
3. **Alias table.** Was: SHOGUN's own table with fan-out and law codes. Now: the shared `AliasTable` with input laws for every one-to-one rename.

**Still on SHOGUN's side, and why:**

1. **Alias shim** (`engine/ports.h`, `kPortFanOuts`, `kPortLegacyNames`). The shared `AliasTable` maps one old id to one canonical id; `add()` refuses a second target for the same old id. HAT:DECAY → CH + OH:DECAY and TOM:PITCH → LTC/MTC/HTC:PITCH are one-to-many, and TOM:PITCH also sets each new cable's CV AMT to 1/12 (a cable amount the user can edit, not a hidden volts conversion). MIX L, MIX R and LFO OUT have no SECTION:, so `isValidLocalId()` is false and `add()` refuses them. testJackIdsJcsShared shows both refusals.
2. **LEVEL COMP detector floor.** The shared `LevelComp` floors both detector powers at 1e-30 (now documented upstream). Adopted as is; see the bit-identity numbers below.
3. **Detector input type.** `jcs::Schmitt` takes float volts; SHOGUN casts its double jack volts. Only matters within 6e-8 V of the 1.0 V and 0.5 V thresholds.
4. **Halfband stage 2.** The shared header has no 4× stage, so stage 2 (25 taps) stays SHOGUN's. Latency 0/23/26.
5. **Umbrella header.** The engine includes single headers; the freestanding wasm build has small `<string>`, `<string_view>`, `<optional>` and `<vector>` shims for `JackId.h` (load time only). The plugin and tests include `jidai/CableStandard.h`.

**Bit-identity of the wave tests.** A scratch dump program renders every sample the eight wave tests compute.
- 1.1.1 vs the 9d6e382 build (54ff487): all eight are byte-identical, with g++ and with clang++. The per-block plan gives the same wire decisions as the old static rule for these renders, and the shared AdaaStage computes the same ADAA values on (0, 0) samples.
- 9d6e382 vs the local shaper (dce82f8): identical except testWaveLevelComp with LEVEL COMP ON (max abs difference 9.86e-7, relative 5.4e-7, from the 1e-30 floor; under 1.5e-7 after 10 ms, 0 after 150 ms). Every printed wave number is unchanged.

**Exact C3.** The synth voice pitch is now `kC3Hz · (A4/440) · 2^((note − 48)/12 + cents/1200 + V/OCT)` instead of `A4 · 2^((note − 69)/12 + …)`. The two are equal in exact arithmetic; doubles differ in the last bits. No printed test number changed. The tests' own reference pitches stay A4-based (440 · 2^((n − 69)/12)), an independent check of the C3 constant. No test pinned 130.8128.

**Strict warnings on the vendored headers in SHOGUN's build.** The engine, tests and web facade compiled with `-Wall -Wextra -Wconversion -Wfloat-equal -Wdouble-promotion` (no -Werror): 0 warnings from `third_party/jidai-common` with g++ and with clang++. SHOGUN's own code is not held to these flags (g++: 75 unique sites, mostly -Wfloat-equal; clang++: 129 unique sites, mostly -Wsign-conversion in params.h). The upstream `HeaderHygiene.cpp` builds and passes with the full strict set (-Wpedantic -Wshadow -Wconversion -Wno-sign-conversion -Wfloat-equal -Wdouble-promotion -Werror) under both compilers.

**Argument-evaluation-order audit** (jidai-common test bug fixed upstream in 7eaa5ec). No SHOGUN site needs rewriting: the hits are const getters, ternary branches, short-circuit chains or nested calls, which are sequenced. The g++ and clang++ suites print identical output.

**jidai-common's own tests at 24ee621**, with the upstream CMake flags plus -O2, g++ and clang++ (identical output):
- `CommonTests.cpp`: 189 checks, 0 failed (`-std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion -Werror`).
- `DspTests.cpp`: 38 checks, 0 failed, at -O2 and -O0 -g (`-std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow -Werror`). 2× ADAA alias −121.8 dB; skip per block: worst sample-to-sample step 0.00398 (per-sample skip 0.00585, sine slope 0.00393).
- `HeaderHygiene.cpp`: passes (strict set above).

## Not run on this box

- `forge/tests/test_switch_law.py` (`make law`) needs graphforge (`GRAPHFORGE_SRC`), which is not installed here; the same switch law is checked natively by `testExtBypassIgnoresPattern` and `testIntIgnoresTrigJacks`.
- The web page itself (`web/shogun.html`) was rebuilt but not opened in a browser; its panel still uses the old page API through the compat layer.
