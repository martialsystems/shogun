# Test plan (spec v2.2, 2026-10-08)

The contract for the v2.2 engine (`engine/`), the plugin shell (`plugin/`) and the web build (`web/`). Each test below is a §15.4 test of `SHOGUN_Redesign.md` v2.2 (or a test this pass added), compiled into `build/shogun_tests` from `tests/blocks.cpp`, `tests/voices.cpp`, `tests/engine.cpp`, `tests/mod.cpp`, `tests/factory.cpp`. Every number is the line the test prints at 48 kHz, double precision; the tolerance column is what the named check uses. `make test` runs them (plus the forge law test), `make asan` runs them under ASan/UBSan.

Conventions: u ∈ [0, 1] for every parameter (SECTION:LABEL ids); 1 V/oct with 0 V = C3 = note 48; accent volts 2.353/3.706/5.0 V; voice end at −90 dB (kQuiet 3.162e-5); OS 2× unless a row says otherwise (latency 23 base samples).

Result of the last run: **all 372 named checks passed** (`make test`, `make asan`; g++ 14.2 and clang++ 19.1 print identical output aside from the long-standing clang++ FMA-fast `testWaveMigration` 2.22e-16 line).

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

§4.6 WAVE 0 with trims/SYM/SHAPE 0 (the default patch) is a true bypass (bit-identical). Any SYM at amount 0 is covered by testWaveAmt0AnySym.  Tolerance: exact.

```
WAVE 0, trims/SYM/SHAPE 0 (default patch): bypassed 1, output bit-identical 1, level gain untouched 1.0
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

§4.6 audio-rate VC alias (verify −58.4/−113.7/−50.8 dB) and unsmoothed VC sidebands (+1.0 dB).  Tolerance: 0.1 dB. VC → SYM moved from −115.3 to −113.7 dB with jidai-common 1.1.2: at WAVE 0.5 stage 3 sits at a = 0, so it is now a wire at any SYM; under 1.1.1 it ran ADAA on the identity, whose half-sample average also took 1.6 dB off the alias. The verify figure is `verify_folder.py` rerun (in scratch) with the 1.1.2 skip rule, a = 0 only; it gives −113.7 dB, the engine's number.

```
body 257.8 Hz, VC 1353.5 Hz, VC -> AMT depth 0.5: 2x ADAA alias -58.4 dB (verify -58.4)
body 257.8 Hz, VC 1353.5 Hz, VC -> SYM depth 0.5: 2x ADAA alias -113.7 dB (verify -113.7)
body 257.8 Hz, VC 1353.5 Hz, VC -> AMT + SYM depth 0.5: 2x ADAA alias -50.8 dB (verify -50.8)
VC sideband energy re harmonics 1.0 dB (unsmoothed reference +1.0, a 5 ms smoother gives -35.8)
```

### testWavePlanBlock

jidai-common 1.1.2 per-block stage skipping (§4.6, `TripleShaper::planBlock`), the "amount 0, any SYM" rule: a stage is a wire for a block when its amount a is 0 for the whole block, whatever its SYM. With WAVE 0.25 (stage 1 live, stages 2 and 3 at a = 0) and SYM 2 0.6 and VC → SYM 3 0.5 set, a steady block makes stages 2 and 3 wires (SYM and VC → SYM never keep a stage). A moving block (ramp or modulation on an amount control) skips nothing. A live VC with AMT depth on stage 3 keeps it; the same depth with VC not live does not. A moving block keeps the ADAA half-sample delay on its a = 0 stages instead of switching to the wire (here with SYM 2 0.25 on the a = 0 stage; the steady twin makes that stage a wire).  Tolerance: exact.

```
SYM 2 0.6, VC>SYM 3 0.5: wires (stage 1 2 3) steady 011, moving 000; VC>AMT on 3: live 010, not live 011; moving block vs wire max diff 3.897e-01 (ADAA half-sample delay kept)
```

In the engine the block is one base sample (SHOGUN reads its parameters once per base sample). `WaveSlot::control` calls it steady when the 7 amount controls (WAVE, WAVE 1 to 3, VC→AMT 1 to 3) equal the previous base sample's and none is ramping (smoother) or modulated (matrix row, CV jack); SYM 1 to 3 and VC→SYM 1 to 3 may move. VC is live when the voice's VC LEVEL is above 0. Before 1.1.2 the plan read all 13 stage controls and a stage needed a = b = 0.

### testWaveAmt0AnySym

§4.6 bypass, "amount 0, any SYM" (jidai-common 1.1.2 `ShaperControls::isBypass` ignores SYM, matrix SYM and VC → SYM). Block level: WAVE 0 with the trims at 0, five SYM 1 to 3 sets and five VC → SYM sets, SYM moving every 64 samples (steady and moving blocks), a live audio-rate VC, LEVEL COMP on and off, a VC → AMT depth with no live VC, at 48, 96 and 192 kHz (1×/2×/4×): every case is bypassed and every sample is bit-identical to the input and to the default bypass; LEVEL COMP is never touched (gain stays exactly 1). A live VC → AMT depth or a WAVE 3 trim of 0.01 is not a bypass. Engine level (tests/voices.cpp): BD1, BD2 and LTC at WAVE 0 with SYM 1 to 3 and VC → SYM 1 to 3 off centre, VC LEVEL 1 and a matrix row LFO 1 → SYM 2 render 9600 samples bit for bit like the default patch at 1×, 2× and 4×.  Tolerance: exact.

```
WAVE 0, trims 0, SYM / VC>SYM / idle VC>AMT set: 60/60 cases bypassed, output bit-identical to input and to default bypass 1, level gain untouched 1; live VC>AMT bypass 0, trim 3 0.01 bypass 0
BD1/BD2/LTC at WAVE 0, SYM 1-3 / VC>SYM 1-3 off centre, live VC, LFO 1 -> SYM 2: 9/9 renders bit-identical to the default patch (1x/2x/4x)
```

Before 1.1.2 a SYM other than 0 at amount 0 ran ADAA on a straight line (a two-sample average: half a sample late, duller treble) and then LEVEL COMP. Now the stages are wires (their histories kept), the 8 Hz detector DC block is not fed and `LevelComp::track()` is not called, because §4.6 skips LEVEL COMP in bypass.

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

### testSynthCvAmtNote

LEAD/BASS CV AMT scales the pitch CV into the voice (NOTE, an old HZ/V cable on NOTE after Lin55ToVoct, and V/OCT): 1.0 = exact 1 V/oct (the default), 0.5 = half tracking around 0 V = C3, 0 = C3, -1 = inverted. V/OCT's own per-jack AMT stays a second factor. Trigger path and held continuous path both scale. 30 named checks.  Tolerance: 1e-9 (pitch 0.01 cents).

```
LEAD AMT +1.0, NOTE +1.0 V -> note 60.0000 (want 60)
LEAD AMT +0.5, NOTE +1.0 V -> note 54.0000 (want 54)
LEAD AMT +0.0, NOTE +1.0 V -> note 48.0000 (want 48)
LEAD AMT -1.0, NOTE +1.0 V -> note 36.0000 (want 36)
held LEAD, AMT 0.5, NOTE 0 -> +2 V: note 60.0000
BASS HZ/V 2.0 V, AMT 1.0 -> note 45.0000; AMT 0.5 -> note 46.5000
LEAD AMT 0.5, V/OCT +1 V adds 6.000000 semitones
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

§12.3 BASS:HZ/V → BASS:NOTE with V' = log2(max(V, 1e-3)) − 1.25 (2.0 V → 110.000000 Hz; 0 V and below read the 1 mV floor, −11.215784 V, since jidai-common 1.1.2); TOM:PITCH fans out to 3 toms at CV AMT 1/12.  Tolerance: 1e-6 (floor 1e-12, exact for V ≤ 1 mV).

```
BASS:HZ/V -> BASS:NOTE, law 1
2.0 V plays 110.000000 Hz (0.00000 cents from 110)
lin55 0 V -> -11.215784 V, -2 V -> -11.215784 V, 1 mV -> -11.215784 V (floor log2(1e-3) - 1.25)
```

### testJackIdsAndTypes

§13.1/13.2: 151 ports, ids exact, unique, types/roles, plainVoltGates. §13.2 listed 153; CLOCK:FILL IN (never read) and MOD:LANE A (a constant 0 V) were removed so the table is exactly the panel bay.  Tolerance: exact.

```
151 ports, ids exactly as 13.2 (less FILL IN / LANE A): 1, unique 1, type/role errors 0, plainVoltGates 1
```

### testBayMatchesPortTable

The declared port table is the panel bay: every port in `kPortTable` has exactly one `jack:` bind among the JACK rows of `plugin/Source/PanelLayout.inc` (compiled into the test with the editor's row shape), and every bay jack names a port in `kPortTable`.  Tolerance: exact.

```
bay 151 jacks, ports 151, missing 0, unknown 0, dups 0
```

### testRemovedPortCablesDropped

Old documents with cables on the removed CLOCK:FILL IN and MOD:LANE A still load. `parsePatch` drops each cable whose end does not resolve and reports it (`Patch::droppedCables`, `droppedRemoved` for removed ports, `droppedIds` with the first four as text); the good cable (MOD:LFO 1 → BD1:PITCH) is kept, `applyPatch` runs and BD1 plays. Checked in the bare, `SHOGUN/` and `SHOGUN#1/` forms; `isRemovedPort` knows both ids in any SHOGUN form and refuses another device's prefix. The plugin reports the same through `droppedCables()` / `loadReport()` (written to the JUCE log; probe check "removed jacks") and the web page through `sg_patch_dropped()` (console warning; `web/test_wasm.mjs` law "old FILL IN / LANE A cables").  Tolerance: exact.

```
3 forms (bare, SHOGUN/, SHOGUN#1/): 3/3 load, drop FILL IN + LANE A, keep LFO 1 -> BD1:PITCH, play
```

### testJackIdsJcsShared

JCS R6/R14 through the shared jidai-common headers. Every one of the 151 port ids and every alias-table id is parsed whole by `jidai::jcs::parseJackId` in three forms: bare, `SHOGUN/` and `SHOGUN#1/`. Labels keep '/', spaces and digits: LEAD:V/OCT, LEAD:HZ/V OUT, MOD:LD GATE, MOD:LFO 1. `resolvePort` maps each form (and `SHOGUN#12/`) to the same port. Another device's prefix is refused. The three v2.0/2.1 names with no SECTION: (MIX L, MIX R, LFO OUT) are not R6 ids; they are matched whole against the alias table. All 454 parameter ids are R6 SECTION:LABEL ids. Role colours come from the shared table.  Tolerance: exact.

```
ports 151/151 round trip whole in 3 forms (labels with '/' 2, with spaces 20, ids with digits 24)
alias-table ids 25, R6 ids round trip 22, legacy non-R6 names matched whole 3: [MIX L] [MIX R] [LFO OUT]; foreign prefix refused 1; params 454/454 valid R6 ids; PITCH colour #6590f3
shared AliasTable 6 renames (BASS:HZ/V -> BASS:NOTE, Lin55ToVoct(2.0 V) = -0.25 V); shim: fan-out 2 (shared add of a 2nd target refused 1), legacy names 3 (shared add refused 1)
```

Since jidai-common 1.1.1 (law floored at 1 mV since 1.1.2) the one-to-one renames (LEAD/BASS:HZ/V → :NOTE with `AliasLaw::Lin55ToVoct`, LEAD/BASS:HZ/V OUT → :NOTE OUT, SD:SNAPPY → SD:TONE, LFO:OUT → MOD:LFO 1) are rows of the shared `jidai::jcs::AliasTable`, resolved with its `resolve()`; the engine applies the returned `AliasConversion`. The test also shows why the rest stays in a SHOGUN shim: the shared table refuses a second target for one old id (HAT:DECAY, TOM:PITCH fan out) and refuses ids with no SECTION: (MIX L, MIX R, LFO OUT).

Before this change, no bare id was mis-split. SECTION and LABEL never contain ':', so a first-':' split was right, and the engine matched ids whole. What failed was the prefixed forms:
- The plugin state loader stripped only `SHOGUN/`, so every `SHOGUN#N/` id failed: the 153 ports of the time plus 11 alias names.
- The web `sg_port_find` matched only bare ids, so all 153 (then) failed in both `SHOGUN/` and `SHOGUN#N/` forms.

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

## Factory bank (tests/factory.cpp)

The bank in `engine/factory.h` (INIT, then 21 kits with their patterns; BUILD_GUIDE.md "Factory bank"). 21 named checks.

### testFactoryPresetsLoad

Every program parses with the shared reader (`engine/patch.h`); 16 to 24 kits after INIT; each pattern is named "NNN" + the program name; the engine holds exactly the loaded patch (`applyPatch` then `capturePatch`: every parameter as a host float, every mod row and step); no content that needs the alias table (no cables, no input laws, CV AMT 1); the Acid kit has at least 8 BASS steps; program 0 is INIT, identical to `Engine::loadInit()`.  Tolerance: exact.

```
22 programs (INIT + 21 kits) load: 529 parameters off INIT, 611 steps (12 ratchets, 4 flams, 35 probability, 22 micro-timed, 72 ties, 3 bends), 18 p-locks, 12 mod rows
```

### testFactoryRoundTrip

The bank text is the canonical writer's output (`patchToJson(parsePatch(text), false)`). Each program is loaded into an engine, saved in full (the plugin's state), reloaded into a new engine and saved again: the two saves are byte-identical and every parameter, mod row, cable, CV AMT, input law and step is equal.  Tolerance: exact.

```
22 programs saved (317396 bytes of state), reloaded and saved again
```

### testPatchWriterNoLibc

The libc-free writer (`writePatchJson` on `patchjson::BufOut`, which the wasm page saves with) against the libc one: `fmtG` gives the bytes of `snprintf("%.*g")` at every precision 1 to 17, `shortestNum` and `shortestFloat` give the text `putNum` and `putFloat` choose with `snprintf` and `strtod` (doubles from random bits, [0, 1), k/127, every power of two from 2^-1074 to 2^1023, subnormals, signed zero; floats from random bits), and every program's document is byte-identical to `patchToJson`, sparse and full.  Tolerance: exact.

```
442510 %.*g texts, 182206 doubles and 182206+ floats, 22 programs x 2 documents
```

### testFactoryRenderLevels

Each kit with its pattern, from a fresh engine at the default master, renders 2 bars at 44.1 and 48 kHz: every peak is above -40 dBFS (not silent) and at or below -6 dBFS.  Tolerance: the limits.

```
21 kits x 44.1/48 kHz, 2 bars from a fresh engine, default master: peaks -10.47 (Jungle Break Roller) to -8.00 dBFS (Metallic Industrial)
```

### testFactoryNames

Names are unique (case-insensitive), fit the pattern name, and carry none of 50 banned brand, gear, model-number, artist and trademarked genre words.  Tolerance: exact.

```
22 names, 50 banned words checked
```

## Plugin shell (ShogunProbe, `make probe-linux`)

The probe builds the real `ShogunAudioProcessor`/editor (JUCE 8.0.4) and checks the shell laws of §15.0 step 4. The three `program` checks are the factory bank through the host program list: the count and names, each kit playing 2 bars between -40 and -6 dBFS at 44.1 and 48 kHz (the same peaks as `testFactoryRenderLevels`), and each loaded program's state reloading to the same bytes. The panel checks press the real keys through the editor's binds (`pressBind` / `clickAt` / `wheelAt`): the KIT and PATTERN arrows step every program with wraparound on all 8 tabs, SRC cycles CLOCK:SOURCE and the engine follows the host tempo only on HOST, **SRC on program load** keeps HOST / INT / EXT across every program, INIT PATCH and A/B (undo of the SRC key and host save/restore still restore it), the MOD matrix past ten rows scrolls (▲ ▼ = a page, wheel = a row) so all 32 slots are reachable, editable (CURVE, ON, DEPTH) and removable with ✕, A/B recalls two full snapshots exactly and copies either onto the other, undo/redo restores kit loads, knob gestures and step edits (a no-op click keeps redo; 64 levels), the MOD ◉ keys arm a source and a knob click adds the row, UI SCALE resizes, the OS keys and offline 4× set their parameters, RE-ROLL UNIT changes the unit serial in the engine and the saved state, and VELOCITY CURVE shapes MIDI velocity. 28 checks, last run all PASS:

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
PASS programs  22 (INIT + 21 factory kits), names 22
PASS programs play  21 kits x 2 rates, 2 bars, peaks -10.47 .. -8.00 dBFS
PASS program state  21/21 programs save and reload to the same state
PASS state round trip  XML <SHOGUN version=2> JSON 11564 chars; params/pattern/lock/mod/cable restored
PASS alias load  cables 4 (1 + 3 toms) = 4, BASS:NOTE law 1, MTC:PITCH CV AMT 0.083333
PASS old HZ/V plays  BASS:HZ/V cable at 2.0 V -> 110.000000 Hz (0.00000 cents from 110), law 1
PASS removed jacks  old FILL IN / LANE A cables dropped 2 (removed 2), kept 1; report: SHOGUN: dropped 2 saved cable(s) (2 on removed jacks CLOCK:FILL IN / MOD:LANE A): SHOGUN/CLOCK:CLK OUT -> SHOGUN#1/CLOCK:FILL IN, SHOGUN/MOD:LANE A -> SHOGUN/BD1:DECAY
PASS editor  1200x672, bay jacks 151/151, ops 2550
PASS tab renders  8 PNGs in build/plugin-linux/tabs
PASS matrix 32 rows  pages 0 10 20 22 12 2 0; 32/32 reached by wheel, 32 edited (CURVE/ON/DEPTH), 32 removed with X; 11 rows scroll to 2
PASS KIT/PATTERN arrows  352 steps over 22 programs on 8 tabs, both arrow pairs wrap INIT <-> Lo-Fi Tape Wobble, ▶▶ = program 2 state
PASS SRC key  SRC INT > SRC EXT > SRC HOST > SRC INT, right-click SRC EXT > SRC INT; engine tempo 120.0 / 120.0 / 100.0 / 120.0 BPM (host 100, knob 120.0)
PASS SRC on program load  HOST kept, INT kept, EXT kept over 132 program loads (22 programs, host change + KIT arrows, INIT PATCH); A/B keeps yes; undo of SRC key restores yes; saved state restores EXT yes
PASS A/B compare  B = copy on first visit, A/B recall exact (BD1:DECAY 0.774 / 0.900, SD step 4), copy A>B and B>A
PASS undo/redo  kit load + knob gesture + step edit undone and redone exactly; no-op click keeps redo; 64 levels after 70 edits
PASS ASSIGN keys  LFO 1 > BD1:DECAY +50 % in slot 1, AT > BD2:TUNE in slot 2, second press cancels
PASS UI scale keys  150% = 1800 px, 75% = 900x504, 100% = 1200
PASS OS keys  badge 2x > 4x, right-click back; offline 4x sets OFFLINE (OS stays 2x)
PASS RE-ROLL UNIT  0x5A31C0DE > 0x74C37F51, engine + saved state follow (SN 0x74C3-7F51)
PASS VELOCITY CURVE  velocity 40 peaks HARD 0.035 < LINEAR 0.063 < SOFT 0.094 <= FIXED 0.150
ShogunProbe: all checks passed
```

**Panel bindings (`make panel-check`, `scripts/check_panel_bindings.py`).** Reads `plugin/Source/PanelLayout.inc` and the editor's bind table: every interactive op (KEY, KNOB, LCD, toggle, jack, grid cell) on every tab must carry a bind, and every bind prefix must be one the editor handles. Runs in `make test`, `make plugin` and `make probe-linux`.

```
check_panel_bindings: 860 interactive ops, 0 dead
```

Tab renders: `build/plugin-linux/tabs/tab_<i>_<name>.png` (1200 × 672) next to the spec mockups; mean absolute pixel difference per tab (0 to 255): MAIN 4.7, VOICE 5.8, GRID 12.4, MOD 7.5, ROUTE 8.6, FX/MIX 6.0, SEQ/MIDI 5.0, GLOBAL 4.3. The differences are the mockups' illustrative data (fake pattern, matrix rows, cables, meter levels), which the plugin replaces with live state (INIT: empty), plus font rasterisation, and the keys this pass removed or rebound (FILL, ROLL, MUTE GRP, SCENE, LEARN, LOCK RND, lane editor, NOISE FLOOR, TRANSPOSE, SAVE AS DEFAULT).

## Web build (`make web`)

`build/shogun.wasm` is the same engine (freestanding, `-DSHOGUN_NO_FORMAT`), run at the AudioContext rate. `web/test_wasm.mjs` runs `web/parity_scenario.txt` through wasm and the native build of the same facade (`build/web_parity`) and compares every sample, then checks the bay and LFO laws, the jack-id forms and the factory bank. The scenario ends by loading every factory program (and one rotated for a chain, and INIT): after each load the wasm state hash (parameters as floats, mod rows, pattern) must equal the native one exactly, which checks the freestanding number reader against `strtod`. After the switch to the shared exact-halfband stage 1, the largest wasm/native difference went from 9.68e-10 to 3.74e-9, and with jidai-common 1.1.1 (exact C3 in the synth pitch) to 3.77e-9 (float32 output, same code on both sides; the wasm build imports JS Math):

```
200608 samples, peak 0.341, largest wasm/native difference 5.00e-10 at 188022; 23/23 loaded-state hashes equal (factory programs)
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
ok   44.1 kHz: rate 44100, latency 23, params 454, ports 151, INIT peak 0
ok   44.1 kHz: BD1 hit peak 0.1498
ok   jack ids: 151/151 resolve in bare, SHOGUN/ and SHOGUN#1/ forms (e.g. LEAD:V/OCT 115); legacy names ok; foreign prefix refused true
ok   LFO OUT into BD1 PITCH changes the kick
ok   factory bank: 22 programs (INIT + 21), unique names 22, 42 renders peak -10.47 .. -8.00 dBFS
ok   documents: 22/22 programs write the native bytes (317396 bytes), 22 reload to the same text and state
ok   old FILL IN / LANE A cables: load 1, dropped 2 (on removed jacks 2), LFO 1 -> BD1:PITCH kept true
```

The page setters now leave a value alone when it equals what the panel reads back (a knob's CC, a level, a track's shuffle and shift, a step's bend, the clock steps), so re-sending a panel keeps the engine's finer value; the scenario's largest difference moved from 3.77e-9 to 5.00e-10 with that (still float32 output of the same code on both sides). The new `documents` law checks the wasm `sg_state_json` against the native writer for every program.

**Page save round trip (`web/test_page.mjs`, headless Chrome).** `web/shogun.html` with a test appended, in a real browser. For every program (INIT and the 21 kits): load it, edit one visible control (a knob, a step on/off, an accent, TEMPO, a track LENGTH, a LEVEL, LFO AMOUNT or LFO DIVISION), SAVE, load the saved pattern again. The saved document must differ from the factory document only in the edited field; the reloaded panel must show the edit; the saved text must be a fixed point of load and save; the engine the page plays from (before and after the reload) must hold the saved document, apart from what the page owns (master, solo, INT/EXT, cables); a second SAVE without edits must give the same text. The LFO knobs must be the kit's LFO 1 and the LFO tab must list all four LFOs.

```
test_page: 22 programs: 22 saves change only the edited field, 22 reload showing the edit, 22 saved texts are fixed points, 22 playing engines hold the saved document, 22 re-saves identical; hidden detail carried: 12 mod rows, 35 steps with probability, 22 micro-timed, 12 ratchets, 15 with p-locks
test_page: the web save round trip keeps every field
```

## jidai-common (vendored shared headers)

`third_party/jidai-common` is jidai-common **1.1.2**, jidai-collection `redesign/jidai` at **8a4b5ae** (8a4b5aecf3d6c2ac8900cc2e88db8feecf8440a8), copied verbatim with `git archive` (see `VENDORED.md`; the previous copies were 1.1.1 at 24ee621 and 1.1.0 at 9d6e382).
SHOGUN uses these parts of it:

- `jcs::Schmitt` for every trigger, clock, reset, run and gate input;
- `jcs::pitch` `kC3Hz` (exact 130.8127826502993 Hz; the synth voice pitch), `note`, `voltsForNote`, `clampPitch`, `lin55ToVoct` (through the alias law);
- `jcs::Role`, `roleInfo` and `roleArgb`;
- `jcs::parseJackId`, `isValidLocalId` and `AliasTable` with `AliasLaw` / `AliasConversion`;
- `dsp::TripleShaper` (with `planBlock`), `ShaperControls` (with `isBypass`), `ShaperStage`, `macroAmounts`, `LevelComp` and `DcBlocker`;
- `dsp::Halfband93`, `Downsampler2x` and `Upsampler2x` (decimator and RET upsampler stage 1).

**Removed local copies:** `engine/jidai_local.h` and `engine/jidai/dsp/TripleShaper.h` (at 9d6e382); with 1.1.1 also the per-stage wire workaround in `engine/wave_shaper.h`, the local lin55 formula in `engine/shogun.cpp` and the local alias table in `engine/ports.h`.

**Wrapper:** `engine/wave_shaper.h`, class `shogun::WaveShaper` with `shogun::TripleShaperParams`: one shared `TripleShaper`, planned once per block by `setParams(p, steady, vcLive)`, plus SHAPE morph, PRE-VCA and the v2.1 migration.

**Workarounds removed with 1.1.1** (upstream now covers them):

1. **Wire rule.** Was: three shared `TripleShaper` instances (one live stage each) plus an ADAA-at-zero replacement, to keep the spec's whole-render rule against the per-sample skip. Now: the shared `planBlock(ctl, steady, vcLive)` once per block (testWavePlanBlock).
2. **lin55.** Was: a local `log2(max(V, 1e-3)) − 1.25` because the shared function was 1.9e-7 V low. Now: the shared `lin55ToVoct` through `AliasLaw::Lin55ToVoct`. 2.0 V still plays 110.000000 Hz (0.00000 cents). With 1.1.2 the shared law is `log2(max(V, 1e-3)) − 1.25`, spec §12.3 word for word: V ≤ 1 mV (0 V and negative volts included) gives −11.215784 V, where 1.1.1 gave −5 V (the rail) for V ≤ 0 and went unbounded below for 0 < V < 1 mV. The 1.1.1 difference listed here is gone (testLin55Migration checks the floor). Both values are below the lowest playable note.
3. **Alias table.** Was: SHOGUN's own table with fan-out and law codes. Now: the shared `AliasTable` with input laws for every one-to-one rename.

**Still on SHOGUN's side, and why:**

1. **Alias shim** (`engine/ports.h`, `kPortFanOuts`, `kPortLegacyNames`). The shared `AliasTable` maps one old id to one canonical id; `add()` refuses a second target for the same old id. HAT:DECAY → CH + OH:DECAY and TOM:PITCH → LTC/MTC/HTC:PITCH are one-to-many, and TOM:PITCH also sets each new cable's CV AMT to 1/12 (a cable amount the user can edit, not a hidden volts conversion). MIX L, MIX R and LFO OUT have no SECTION:, so `isValidLocalId()` is false and `add()` refuses them. testJackIdsJcsShared shows both refusals.
2. **LEVEL COMP detector floor.** The shared `LevelComp` floors both detector powers at 1e-30 (now documented upstream). Adopted as is; see the bit-identity numbers below.
3. **Detector input type.** `jcs::Schmitt` takes float volts; SHOGUN casts its double jack volts. Only matters within 6e-8 V of the 1.0 V and 0.5 V thresholds.
4. **Halfband stage 2.** The shared header has no 4× stage, so stage 2 (25 taps) stays SHOGUN's. Latency 0/23/26.
5. **Umbrella header.** The engine includes single headers; the freestanding wasm build has small `<string>`, `<string_view>`, `<optional>` and `<vector>` shims for `JackId.h` (load time only). The plugin and tests include `jidai/CableStandard.h`.

**jidai-common 1.1.2 (output changes).** (1) A WAVE stage at amount 0 is a wire at any SYM, and WAVE 0 with any SYM is the full bypass: stages, detector DC block and LEVEL COMP skipped (`engine/wave_shaper.h`); `WaveSlot::control` calls a block steady from the amount controls only (`engine/voices/common.h`). (2) HZ/V → NOTE is floored at 1 mV. Tests changed: testWavePlanBlock (to the new law; SYM on a = 0 stages stays a wire), testWaveAudioRateVc (VC → SYM −115.3 → −113.7 dB, stage 3 now a wire), testWaveBypass (label only), testLin55Migration (floor check added), and the new testWaveAmt0AnySym (block and engine). Every other printed number in the suite is unchanged (the full log against 1d7826f differs only in those lines). SHOGUN does not call `LevelComp::track()` in bypass or report `groupDelay()` to the host.

**Golden render with 1.1.2.** The scratch busy-patch render described under "Strict build" below (SHA-256 of the raw doubles, first 16 hex digits), against 1d7826f (1.1.1):

| Build | 1.1.1 (1d7826f) | 1.1.2 |
| --- | --- | --- |
| g++ 14.2 and clang++ 19.1, `-ffp-contract=off` | 4feda137ff4c35a9 | 4feda137ff4c35a9 |
| g++ `-mfma -ffp-contract=fast` | 01f7a8f927921fbf | 01f7a8f927921fbf |
| clang++ `-mfma -ffp-contract=on` | 4fc3e37230bd0a01 | 4fc3e37230bd0a01 |
| clang++ `-mfma -ffp-contract=fast` | 126f554cd6e62129 | 126f554cd6e62129 |

It does not change: that patch has no WAVE stage at amount 0 with a SYM other than 0 (BD1 SYM 2 is on a live stage, the a = 0 stages have SYM 0) and no HZ/V cable, so neither 1.1.2 output change (the WAVE amount-0 wire, the lin55 1 mV floor) reaches it. The same patch with SYM set on amount-0 stages (BD2 SYM 3 0.9, LTC SYM 3 0.2, MTC SYM 1 0.85 and VC → SYM 2 0.1 at WAVE 0, HTC SYM 2 0.7 at WAVE 0) does change, because of the WAVE amount-0 wire: 1.1.1 e995ecda8606aaa7 (g++ and clang++, contraction off; g++ FMA fast c96ac2668021ac76, clang++ FMA fast 28dfe551bf491b2d) becomes 4feda137ff4c35a9 (FMA fast 01f7a8f927921fbf / 126f554cd6e62129), the busy patch's own hashes: under 1.1.2 those SYM settings have no effect at all, as the rule says. The lin55 floor is below every playable note and is checked by testLin55Migration, not the render. The test log is byte-identical across g++ and clang++ with contraction off, g++ FMA fast and clang++ FMA on; clang++ FMA fast differs in one line (testWaveMigration max error 2.22e-16 instead of 0, the same as under 1.1.1), all checks passing in every build (339 at the re-vendor, 342 after the port change; 372 after CV AMT / matrix / SRC; golden hash unchanged at 4feda137ff4c35a9 with contraction off).

**Bit-identity of the wave tests.** A scratch dump program renders every sample the eight wave tests compute.
- 1.1.1 vs the 9d6e382 build (54ff487): all eight are byte-identical, with g++ and with clang++. The per-block plan gives the same wire decisions as the old static rule for these renders, and the shared AdaaStage computes the same ADAA values on (0, 0) samples.
- 9d6e382 vs the local shaper (dce82f8): identical except testWaveLevelComp with LEVEL COMP ON (max abs difference 9.86e-7, relative 5.4e-7, from the 1e-30 floor; under 1.5e-7 after 10 ms, 0 after 150 ms). Every printed wave number is unchanged.

**Exact C3.** The synth voice pitch is now `kC3Hz · (A4/440) · 2^((note − 48)/12 + cents/1200 + V/OCT)` instead of `A4 · 2^((note − 69)/12 + …)`. The two are equal in exact arithmetic; doubles differ in the last bits. No printed test number changed. The tests' own reference pitches stay A4-based (440 · 2^((n − 69)/12)), an independent check of the C3 constant. No test pinned 130.8128.

**Strict build (`make strict`, JIDAI RACK flags).** The engine, the tests, the web facade, `tools/measure_calib.cpp` and every `plugin/Source` file compile with JUCE's recommended warning flags (`juce_recommended_warning_flags`) plus `-Wfloat-equal -Wimplicit-int-float-conversion -Wshadow -Wconversion -Wdouble-promotion -Werror` (clang++ 19.1; g++ 14.2 takes the same set without the clang-only `-Wimplicit-int-float-conversion`). Only JUCE's headers are `-isystem`; `third_party/jidai-common` is checked like SHOGUN's own code and gives 0 warnings. Before the fix: clang++ 234 unique warning sites (71 -Wdouble-promotion, 65 -Wsign-conversion, 60 -Wmissing-prototypes, 21 -Wimplicit-int-float-conversion, 16 -Wfloat-equal, 1 -Wunused-const-variable `kNoRing`), g++ 169 (66 -Wsign-conversion, 53 -Wfloat-equal, 29 -Wdouble-promotion, 21 -Wconversion). After: 0 and 0. Every fix spells out a conversion the compiler already made (`static_cast`), routes an intentional exact compare through `dsp::exactEq` / `tu::same` (`std::equal_to`, same result as `==`), declares the web exports in `web/wasm/shogun_web.h`, or drops the unused `kNoRing`. Bit-identity: a scratch render of a busy patch (every voice, wave shaper with VC, buses, master, swing, ratchets, flams, micro-timing, probability, locks, three mod rows) at 44.1 and 48 kHz and 1×/2×/4× (4.2 s each, 37.1 MB of doubles) has the same SHA-256 before and after, with g++ and with clang++; the full test log and the web parity output are byte-identical.

**Argument-evaluation-order audit** (jidai-common test bug fixed upstream in 7eaa5ec). No SHOGUN site needs rewriting: the hits are const getters, ternary branches, short-circuit chains or nested calls, which are sequenced. The g++ and clang++ suites print identical output.

**jidai-common's own tests at 8a4b5ae**, with the upstream CMake flags plus -O2, g++ and clang++ (identical output):
- `CommonTests.cpp`: 190 checks, 0 failed (`-std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion -Werror`).
- `DspTests.cpp`: 55 checks, 0 failed, at -O2 and -O0 -g (`-std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow -Werror`). 2× ADAA alias −121.8 dB; skip per block: worst sample-to-sample step 0.00398 (per-sample skip 0.00585, sine slope 0.00393); group delay 0.5/1.0/1.5 samples for 1/2/3 running stages (measured 0.5000/1.0000/1.5000).
- `HeaderHygiene.cpp`: passes (strict set above).

## Not run on this box

- `forge/tests/test_switch_law.py` (`make law`) needs graphforge (`GRAPHFORGE_SRC`), which is not installed here; the same switch law is checked natively by `testExtBypassIgnoresPattern` and `testIntIgnoresTrigJacks`.
- The web page runs in headless Chrome for `web/test_page.mjs` only; its audio path (AudioWorklet) is not exercised there.
