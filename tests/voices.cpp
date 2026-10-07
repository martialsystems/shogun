#include "shogun.h"
#include "wave_folder.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// Named checks from TESTPLAN.md, plus the short-voice rows in SCHEMATICS.md.
// Tolerance is 1e-5 on the printed decimals.

int gFails = 0;

void expect(bool ok, const char* test, const char* msg, double got, double want) {
  if (ok) return;
  std::printf("FAIL %s: %s got %.9f want %.9f\n", test, msg, got, want);
  ++gFails;
}

bool near(double got, double want, double tol = 1e-5) { return std::fabs(got - want) <= tol; }

// BD1 example knobs, printed outputs: y at n = 48 after a hit, and y at n = 10 after an accent-2 step.
constexpr double kBd1At48 = 0.040808;
constexpr double kBd1At10 = 0.055968;

shogun::Knobs bd1Example() {
  shogun::Knobs k;
  k.bd1Attack = 64;
  k.bd1Decay = 80;
  k.bd1Pitch = 40;
  k.bd1Tune = 50;
  k.bd1Noise = 0;
  k.bd1Filter = 64;
  k.bd1Dist = 0;
  k.bd1Trigger = 0;
  return k;
}

shogun::Pattern silentPattern() {
  shogun::Pattern p;
  p.length = 4;
  for (int v = 0; v < shogun::kVoiceCount; ++v) {
    p.track[v].length = 4;
    p.track[v].shuffle = 0;
    p.track[v].shiftCc = 0;
    for (int s = 0; s < shogun::kMaxSteps; ++s) {
      p.track[v].drum[s].on = false;
      p.track[v].drum[s].accent = 2;
      p.track[v].drum[s].flam = false;
      p.track[v].drum[s].flamIndex = 0;
      p.track[v].drum[s].bendCc = -1;
      p.track[v].note[s].note = -1;
      p.track[v].note[s].accent = 2;
    }
  }
  return p;
}

shogun::Pattern bypassPattern() {
  shogun::Pattern p = silentPattern();
  p.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].on = true;
  p.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].accent = 2;
  p.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].flam = false;
  p.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].bendCc = -1;
  return p;
}

void arm(shogun::Engine& e, const shogun::Knobs& k, const shogun::Pattern& p) {
  e.reset();
  e.setTempo(120);
  e.setScaleSteps(4);
  e.setKnobs(k);
  e.setPattern(p);
  e.setMaster(1);
  e.setLevel(shogun::Voice::Bd1, 1);
}

bool audioSilent(const shogun::Frame& f) {
  const double a[] = {f.bdL, f.bdR, f.sdL, f.rsR, f.hhL, f.cyR, f.cpL, f.cpR,
                      f.toL, f.toR, f.clL, f.cbR, f.mainL, f.mainR};
  for (double s : a) {
    if (s != 0.0) return false;
  }
  return true;
}

// 2048-point Hann-windowed DFT magnitudes, bins 0 to 1024. Bin width is 48000 / 2048 = 23.4375 Hz.
std::vector<double> spectrum2048(const std::vector<double>& x) {
  const int N = 2048;
  std::vector<double> mag(N / 2 + 1, 0.0);
  for (int b = 0; b <= N / 2; ++b) {
    double re = 0.0;
    double im = 0.0;
    for (int n = 0; n < N && n < static_cast<int>(x.size()); ++n) {
      const double w = 0.5 - 0.5 * std::cos(2.0 * shogun::kPi * n / (N - 1));
      const double a = 2.0 * shogun::kPi * b * n / N;
      re += w * x[n] * std::cos(a);
      im -= w * x[n] * std::sin(a);
    }
    mag[b] = std::sqrt(re * re + im * im);
  }
  return mag;
}

// Largest magnitude within two bins of frequency hz.
double peakNear(const std::vector<double>& mag, double hz) {
  const int c = static_cast<int>(std::lround(hz / (48000.0 / 2048.0)));
  double m = 0.0;
  for (int b = c - 2; b <= c + 2; ++b) {
    if (b >= 0 && b < static_cast<int>(mag.size())) m = std::fmax(m, mag[b]);
  }
  return m;
}

// Third harmonic over fundamental, from a 2048-point spectrum.
double thirdOverFirst(const std::vector<double>& x, double hz) {
  const std::vector<double> mag = spectrum2048(x);
  return peakNear(mag, 3.0 * hz) / peakNear(mag, hz);
}

void testKickBendDecays() {
  const char* t = "testKickBendDecays";
  shogun::Engine e;
  arm(e, bd1Example(), silentPattern());
  e.trigger(shogun::Voice::Bd1, 1.0, 0.0);
  shogun::TrigIn in;
  shogun::Frame f;
  double f0 = 0;
  double f2400 = 0;
  double ftune = 0;
  double y48 = 0;
  double y480 = 0;
  for (int n = 0; n <= 2400; ++n) {
    e.process(in, f);
    if (n == 0) {
      f0 = e.bd1Hz();
      ftune = e.bd1TuneHz();
    }
    if (n == 48) y48 = f.bdL;
    if (n == 480) y480 = f.bdL;
    if (n == 2400) f2400 = e.bd1Hz();
  }
  expect(near(f0, 83.814360), t, "f(0)", f0, 83.814360);
  expect(near(ftune, 60.408707), t, "f_tune", ftune, 60.408707);
  expect(near(f2400, 72.957296), t, "f(2400)", f2400, 72.957296);
  expect(std::fabs(f2400 - ftune) < std::fabs(f0 - ftune), t, "decay toward tune", f2400, f0);
  expect(near(y48, kBd1At48), t, "y(48)", y48, kBd1At48);
  expect(near(y480, -0.482608), t, "y(480)", y480, -0.482608);

  // Step bend is an extra depth. CC 64 is 0.094488 semitones, not a center detent.
  shogun::Engine bent;
  shogun::Pattern pat = bypassPattern();
  pat.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].bendCc = 64;
  arm(bent, bd1Example(), pat);
  bent.setMode(shogun::ClockMode::Int);
  bent.process(in, f);
  expect(near(bent.bd1Hz(), 84.273057), t, "bend cc 64 at n=0", bent.bd1Hz(), 84.273057);

  shogun::Engine down;
  arm(down, bd1Example(), silentPattern());
  down.trigger(shogun::Voice::Bd1, 1.0, -12.0);
  down.process(in, f);
  expect(near(down.bd1Hz(), 41.907180), t, "bend_st -12 at n=0", down.bd1Hz(), 41.907180);
}

void testBd1SoundChangesAttack() {
  const char* t = "testBd1SoundChangesAttack";
  shogun::Knobs k = bd1Example();
  k.bd1Attack = 127;
  k.bd1Trigger = 0;
  shogun::Engine a;
  arm(a, k, silentPattern());
  a.trigger(shogun::Voice::Bd1);
  shogun::TrigIn in;
  shogun::Frame f;
  double y0 = 0;
  double tr0 = 0;
  double tune0 = 0;
  for (int n = 0; n <= 10; ++n) {
    a.process(in, f);
    if (n == 10) {
      y0 = f.bdL;
      tr0 = a.bd1TransientHz();
      tune0 = a.bd1TuneHz();
    }
  }
  k.bd1Trigger = 64;
  shogun::Engine b;
  arm(b, k, silentPattern());
  b.trigger(shogun::Voice::Bd1);
  double y64 = 0;
  double tr64 = 0;
  double tune64 = 0;
  for (int n = 0; n <= 10; ++n) {
    b.process(in, f);
    if (n == 10) {
      y64 = f.bdL;
      tr64 = b.bd1TransientHz();
      tune64 = b.bd1TuneHz();
    }
  }
  expect(near(y0, 0.120608), t, "trigger 0 y", y0, 0.120608);
  expect(near(tr0, 160.0), t, "trigger 0 f_tr", tr0, 160.0);
  expect(near(y64, 0.453201), t, "trigger 64 y", y64, 0.453201);
  expect(near(tr64, 1765.184603), t, "trigger 64 f_tr", tr64, 1765.184603);
  expect(near(tune0, tune64, 0.0), t, "body tune matches", tune0, tune64);
  expect(std::fabs(y0 - y64) > 0.1, t, "attacks differ", y0, y64);
}

void renderBd2(int decay, int tune, int tone, int nStop, double& env, double& tr) {
  shogun::Knobs k;
  k.bd2Decay = decay;
  k.bd2Tune = tune;
  k.bd2Tone = tone;
  shogun::Engine e;
  arm(e, k, silentPattern());
  e.trigger(shogun::Voice::Bd2);
  shogun::TrigIn in;
  shogun::Frame f;
  env = 0;
  tr = 0;
  for (int n = 0; n <= nStop; ++n) {
    e.process(in, f);
    if (n == nStop) {
      env = e.bd2Env();
      tr = e.bd2ScaledTransient();
    }
  }
}

void testBd2CanHold() {
  const char* t = "testBd2CanHold";
  double env = 0;
  double tr = 0;
  renderBd2(127, 60, 0, 96000, env, tr);
  expect(near(env, 0.702021), t, "decay 127", env, 0.702021);
  expect(std::fabs(env - 0.70) < 0.01, t, "near sustain", env, 0.70);
  expect(env >= 0.5, t, "hold stays up", env, 0.5);

  renderBd2(0, 60, 0, 96000, env, tr);
  expect(near(env, 0.0), t, "decay 0", env, 0.0);

  renderBd2(126, 60, 0, 96000, env, tr);
  expect(near(env, 0.056280), t, "decay 126", env, 0.056280);
  expect(env < 0.5, t, "126 is not a hold", env, 0.5);

  renderBd2(0, 60, 100, 10, env, tr);
  expect(near(tr, 0.129116), t, "scaled transient", tr, 0.129116);
}

void renderSd(int tone, double& f1, double& f2, double& t1, double& t2, double& y) {
  shogun::Knobs k;
  k.sdTune = 70;
  k.sdDTune = 90;
  k.sdSnappy = 0;
  k.sdSnDecay = 50;
  k.sdTone = tone;
  k.sdToneDecay = 60;
  k.sdPitch = 30;
  shogun::Engine e;
  arm(e, k, silentPattern());
  e.trigger(shogun::Voice::Sd);
  shogun::TrigIn in;
  shogun::Frame f;
  for (int n = 0; n <= 20; ++n) e.process(in, f);
  f1 = e.sdF1();
  f2 = e.sdF2();
  t1 = e.sdT1();
  t2 = e.sdT2();
  y = f.sdL;
}

void testSnareTwoTones() {
  const char* t = "testSnareTwoTones";
  double f1, f2, t1, t2, y;
  renderSd(64, f1, f2, t1, t2, y);
  expect(near(f1, 233.014061), t, "f1", f1, 233.014061);
  expect(near(f2, 282.574688), t, "f2", f2, 282.574688);
  expect(near(t1, 0.899876), t, "t1", t1, 0.899876);
  expect(near(t2, 0.943686), t, "t2", t2, 0.943686);
  expect(near(y, 0.921953), t, "blend", y, 0.921953);

  double y0, y127;
  renderSd(0, f1, f2, t1, t2, y0);
  expect(near(y0, t1), t, "tone 0 is t1", y0, t1);
  renderSd(127, f1, f2, t1, t2, y127);
  expect(near(y127, t2), t, "tone 127 is t2", y127, t2);
}

void testHatChoke() {
  const char* t = "testHatChoke";
  shogun::Knobs k;
  k.hhTune = 60;
  k.ohDecay = 100;
  k.hhDecay = 40;
  shogun::TrigIn in;
  shogun::Frame f;

  shogun::Engine openOnly;
  arm(openOnly, k, silentPattern());
  openOnly.trigger(shogun::Voice::Oh);
  double env200 = 0;
  for (int n = 0; n <= 200; ++n) {
    openOnly.process(in, f);
    if (n == 200) env200 = openOnly.ohEnv();
  }
  expect(near(env200, 0.985052), t, "open env at 200", env200, 0.985052);

  shogun::Engine choked;
  arm(choked, k, silentPattern());
  choked.trigger(shogun::Voice::Oh);
  for (int n = 0; n < 200; ++n) choked.process(in, f);
  choked.trigger(shogun::Voice::Hh);
  bool openDead = true;
  double hhAt10 = 0;
  for (int n = 200; n <= 500; ++n) {
    choked.process(in, f);
    if (choked.ohSample() != 0.0 || choked.ohEnv() != 0.0) openDead = false;
    if (n == 210) hhAt10 = choked.hhEnv();
  }
  expect(openDead, t, "open stays 0 after choke", openDead ? 0.0 : 1.0, 0.0);
  expect(near(choked.hhTau(), 0.033008), t, "closed tau", choked.hhTau(), 0.033008);
  expect(hhAt10 > 0.9, t, "closed hat keeps decaying", hhAt10, 0.99);

  choked.trigger(shogun::Voice::Oh);
  choked.process(in, f);
  expect(near(choked.ohEnv(), 1.0), t, "open can retrigger", choked.ohEnv(), 1.0);

  shogun::Engine both;
  arm(both, k, silentPattern());
  both.trigger(shogun::Voice::Oh);
  both.trigger(shogun::Voice::Hh);
  both.process(in, f);
  expect(both.ohEnv() == 0.0, t, "same-sample choke wins", both.ohEnv(), 0.0);
  expect(near(both.hhEnv(), 1.0), t, "closed still starts", both.hhEnv(), 1.0);
}

void testClapBurstCount() {
  const char* t = "testClapBurstCount";
  shogun::Knobs k;
  k.cpData = 48;
  k.cpTrigger = 0;
  k.cpAttack = 127;
  k.cpDecay = 64;
  k.cpFilter = 64;
  // Bursts start 10 ms to 12 ms apart. Each is the same high-passed noise under a fixed 3 ms decay, so at one
  // sample burst i over burst i-1 is exp(gap / 144), whatever Decay is.
  const int starts[4] = {0, 480, 1056, 1584};
  auto run = [&](int decay, double out[4], int& count) {
    shogun::Knobs kk = k;
    kk.cpDecay = decay;
    shogun::Engine e;
    arm(e, kk, silentPattern());
    e.trigger(shogun::Voice::Cp);
    shogun::TrigIn in;
    shogun::Frame f;
    bool early = false;
    for (int n = 0; n <= 1589; ++n) {
      e.process(in, f);
      for (int i = 1; i < 4; ++i) {
        if (n < starts[i] && e.cpBurst(i) != 0.0) early = true;
      }
    }
    expect(!early, t, "no burst before its start", 0, 0);
    for (int i = 0; i < 4; ++i) out[i] = e.cpBurst(i);
    count = e.cpCount();
  };
  double b[4];
  double bLong[4];
  int count = 0;
  int countLong = 0;
  run(64, b, count);
  run(127, bLong, countLong);
  expect(count == 4, t, "count", static_cast<double>(count), 4);
  for (int i = 1; i < 4; ++i) {
    const int gap = starts[i] - starts[i - 1];
    expect(gap >= 480 && gap <= 576, t, "gap 10 ms to 12 ms", gap, 528);
    const double want = std::exp(gap / (48000.0 * 0.003));
    expect(near(b[i] / b[i - 1], want, 1e-9 * want), t, "fixed 3 ms bursts", b[i] / b[i - 1], want);
  }
  for (int i = 0; i < 4; ++i) {
    expect(b[i] == bLong[i], t, "Decay does not stretch the bursts", bLong[i], b[i]);
  }
  shogun::TrigIn in;
  shogun::Frame f;

  shogun::Knobs one = k;
  one.cpData = 0;
  shogun::Engine single;
  arm(single, one, silentPattern());
  single.trigger(shogun::Voice::Cp);
  double oneAt5 = 0;
  double oneLater = 0;
  for (int n = 0; n <= 533; ++n) {
    single.process(in, f);
    if (n == 5) oneAt5 = single.cpBurst(0);
    if (n == 533) oneLater = single.cpBurst(1);
  }
  expect(single.cpCount() == 1, t, "count 1", static_cast<double>(single.cpCount()), 1);
  expect(oneAt5 != 0.0, t, "single burst", oneAt5, 1.0);
  expect(oneLater == 0.0, t, "no second peak", oneLater, 0.0);

  // Flam is not on clap. Index 0 would otherwise retrigger at 180 samples.
  shogun::Pattern flam = silentPattern();
  auto& step = flam.track[static_cast<int>(shogun::Voice::Cp)].drum[0];
  step.on = true;
  step.flam = true;
  step.flamIndex = 0;
  step.accent = 2;
  shogun::Pattern plain = flam;
  plain.track[static_cast<int>(shogun::Voice::Cp)].drum[0].flam = false;
  shogun::Engine withFlam;
  shogun::Engine noFlam;
  arm(withFlam, k, flam);
  arm(noFlam, k, plain);
  withFlam.setMode(shogun::ClockMode::Int);
  noFlam.setMode(shogun::ClockMode::Int);
  double flamB = 0;
  double plainB = 0;
  for (int n = 0; n <= 180; ++n) {
    withFlam.process(in, f);
    noFlam.process(in, f);
    if (n == 180) {
      flamB = withFlam.cpBurst(0);
      plainB = noFlam.cpBurst(0);
    }
  }
  expect(near(flamB, plainB, 1e-12), t, "flam ignored", flamB, plainB);
  expect(std::fabs(flamB) > 1e-3, t, "clap still in its first hit", flamB, plainB);
}

void testExtBypassIgnoresPattern() {
  const char* t = "testExtBypassIgnoresPattern";
  shogun::Engine a;
  shogun::Engine b;
  arm(a, bd1Example(), bypassPattern());
  arm(b, bd1Example(), bypassPattern());
  a.setMode(shogun::ClockMode::Ext);
  b.setMode(shogun::ClockMode::Ext);
  shogun::TrigIn silence;
  shogun::Frame fa;
  shogun::Frame fb;
  bool silent = true;
  for (int i = 0; i < 6000; ++i) {
    a.process(silence, fa);
    b.process(silence, fb);
    if (!audioSilent(fa) || !audioSilent(fb)) silent = false;
  }
  expect(silent, t, "first step silent", silent ? 0.0 : 1.0, 0.0);
  expect(a.counter() == 1, t, "counter advanced", static_cast<double>(a.counter()), 1);
  expect(a.displayStep() == 2, t, "display moved", static_cast<double>(a.displayStep()), 2);
  expect(a.periodSamples() == 6000.0, t, "period", a.periodSamples(), 6000.0);

  const double wantVel = kBd1At10 * 0.819291;
  double yA = 0;
  double yB = 0;
  const std::int64_t counterBeforeSwitch = a.counter();
  for (int i = 6000; i <= 24010; ++i) {
    if (i == 6011) {
      a.setMode(shogun::ClockMode::Int);
      b.setMode(shogun::ClockMode::Int);
      expect(a.counter() == counterBeforeSwitch, t, "switch keeps counter",
             static_cast<double>(a.counter()), static_cast<double>(counterBeforeSwitch));
    }
    shogun::TrigIn inA;
    shogun::TrigIn inB;
    if (i == 6000 || i == 9000) {
      inA.volts[static_cast<int>(shogun::Voice::Bd1)] = 5.0;
      inA.velocity[static_cast<int>(shogun::Voice::Bd1)] = 100;
    }
    if (i == 6000) {
      inB.volts[static_cast<int>(shogun::Voice::Bd1)] = 5.0;
      inB.velocity[static_cast<int>(shogun::Voice::Bd1)] = 100;
    }
    a.process(inA, fa);
    b.process(inB, fb);
    if (i == 6010) {
      yA = fa.bdL;
      yB = fb.bdL;
    }
    if (i >= 6011 && (fa.bdL != fb.bdL || fa.mainL != fb.mainL)) {
      expect(false, t, "jack after INT switch", fa.bdL, fb.bdL);
      break;
    }
    if (i == 24010) {
      expect(near(fa.bdL, kBd1At10), t, "next on-step", fa.bdL, kBd1At10);
      expect(near(fb.bdL, kBd1At10), t, "next on-step pair", fb.bdL, kBd1At10);
    }
  }
  expect(near(yA, wantVel), t, "ext velocity y", yA, wantVel);
  expect(near(yB, wantVel), t, "ext velocity y pair", yB, wantVel);
  expect(std::fabs(yA - kBd1At10) > 1e-4, t, "not pattern accent", yA, kBd1At10);

  shogun::Pattern flamOn = bypassPattern();
  flamOn.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].flam = true;
  flamOn.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].flamIndex = 0;
  shogun::Engine flam;
  shogun::Engine clean;
  arm(flam, bd1Example(), flamOn);
  arm(clean, bd1Example(), bypassPattern());
  flam.setMode(shogun::ClockMode::Ext);
  clean.setMode(shogun::ClockMode::Ext);
  bool flamMatches = true;
  double at180 = 0;
  for (int i = 0; i <= 6180; ++i) {
    shogun::TrigIn in;
    if (i == 6000) {
      in.volts[static_cast<int>(shogun::Voice::Bd1)] = 5.0;
      in.velocity[static_cast<int>(shogun::Voice::Bd1)] = 100;
    }
    flam.process(in, fa);
    clean.process(in, fb);
    if (fa.bdL != fb.bdL) flamMatches = false;
    if (i == 6180) at180 = fa.bdL;
  }
  expect(flamMatches, t, "flam does not add hits", flamMatches ? 0.0 : 1.0, 0.0);
  expect(std::fabs(at180) > 1e-3, t, "n=180 is not a restart", at180, 0.2);

  shogun::Pattern bent = bypassPattern();
  bent.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].bendCc = 0;
  shogun::Engine extBend;
  arm(extBend, bd1Example(), bent);
  extBend.setMode(shogun::ClockMode::Ext);
  for (int i = 0; i <= 6000; ++i) {
    shogun::TrigIn in;
    if (i == 6000) {
      in.volts[static_cast<int>(shogun::Voice::Bd1)] = 5.0;
      in.velocity[static_cast<int>(shogun::Voice::Bd1)] = 127;
    }
    extBend.process(in, fa);
  }
  expect(near(extBend.bd1Hz(), 83.814360), t, "ext ignores step bend", extBend.bd1Hz(), 83.814360);
}

void testIntIgnoresTrigJacks() {
  const char* t = "testIntIgnoresTrigJacks";
  // Step 0 rings into step 1 (body tau is about 0.786 s). An off step does
  // not choke. The jack must add nothing, and step 0 uses the pattern accent.
  shogun::Engine plain;
  shogun::Engine pulsed;
  shogun::Engine held;
  arm(plain, bd1Example(), bypassPattern());
  arm(pulsed, bd1Example(), bypassPattern());
  arm(held, bd1Example(), bypassPattern());
  plain.setMode(shogun::ClockMode::Int);
  pulsed.setMode(shogun::ClockMode::Int);
  held.setMode(shogun::ClockMode::Int);
  shogun::Frame fp, fj, fh;
  double y10 = 0;
  bool matchPulse = true;
  bool matchHeld = true;
  for (int i = 0; i <= 11999; ++i) {
    shogun::TrigIn inP;
    shogun::TrigIn inJ;
    shogun::TrigIn inH;
    inH.volts[static_cast<int>(shogun::Voice::Bd1)] = 5.0;
    inH.velocity[static_cast<int>(shogun::Voice::Bd1)] = 100;
    if (i == 9000) {
      inJ.volts[static_cast<int>(shogun::Voice::Bd1)] = 5.0;
      inJ.velocity[static_cast<int>(shogun::Voice::Bd1)] = 100;
    }
    plain.process(inP, fp);
    pulsed.process(inJ, fj);
    held.process(inH, fh);
    if (fp.bdL != fj.bdL || fp.mainL != fj.mainL) matchPulse = false;
    if (fp.bdL != fh.bdL || fp.mainL != fh.mainL) matchHeld = false;
    if (i == 10) y10 = fp.bdL;
  }
  expect(near(y10, kBd1At10), t, "step 0 accent", y10, kBd1At10);
  expect(matchPulse, t, "mid-step jack ignored", matchPulse ? 0.0 : 1.0, 0.0);
  expect(matchHeld, t, "plugged cable ignored", matchHeld ? 0.0 : 1.0, 0.0);
  expect(std::fabs(y10 - kBd1At10 * 0.819291) > 1e-3, t, "not the jack velocity", y10, kBd1At10);
}

void testRestIsSilent() {
  const char* t = "testRestIsSilent";
  shogun::Engine e;
  arm(e, bd1Example(), silentPattern());
  e.setMode(shogun::ClockMode::Int);
  shogun::TrigIn in;
  shogun::Frame f;
  bool silent = true;
  for (int i = 0; i < 6000; ++i) {
    e.process(in, f);
    if (!audioSilent(f)) silent = false;
  }
  expect(silent, t, "off drums and note rests", silent ? 0.0 : 1.0, 0.0);

  shogun::Knobs k = bd1Example();
  k.leadTone = 80;
  k.bassTone = 64;

  shogun::Pattern leadOn = silentPattern();
  leadOn.track[static_cast<int>(shogun::Voice::Lead)].note[0].note = 60;
  shogun::Engine lead;
  arm(lead, k, leadOn);
  lead.setMode(shogun::ClockMode::Int);
  bool leadSounds = false;
  for (int n = 0; n <= 25; ++n) {
    lead.process(in, f);
    if (n == 25 && std::fabs(f.mainL) > 0.01) leadSounds = true;
  }
  expect(leadSounds, t, "a lead note is not a rest", leadSounds ? 1.0 : 0.0, 1.0);
  expect(near(lead.leadSaw(), 0.387776), t, "lead saw", lead.leadSaw(), 0.387776);

  shogun::Pattern bassOn = silentPattern();
  bassOn.track[static_cast<int>(shogun::Voice::Bass)].note[0].note = 36;
  bassOn.track[static_cast<int>(shogun::Voice::Bass)].note[0].accent = 2;
  shogun::Engine bass;
  arm(bass, k, bassOn);
  bass.setMode(shogun::ClockMode::Int);
  bool bassSounds = false;
  for (int n = 0; n <= 25; ++n) {
    bass.process(in, f);
    if (n == 25 && std::fabs(f.mainL) > 0.01) bassSounds = true;
  }
  expect(bassSounds, t, "a bass note is not a rest", bassSounds ? 1.0 : 0.0, 1.0);
}

void testIndividualOutStaysInMix() {
  const char* t = "testIndividualOutStaysInMix";
  shogun::TrigIn in;
  shogun::Frame f;
  shogun::Engine patched;
  arm(patched, bd1Example(), silentPattern());
  patched.setPairPatched(shogun::Pair::Bd, true);
  patched.trigger(shogun::Voice::Bd1);
  double pair = 0;
  double main = 0;
  for (int n = 0; n <= 48; ++n) {
    patched.process(in, f);
    if (n == 48) {
      pair = f.bdL;
      main = f.mainL;
    }
  }
  expect(patched.pairPatched(shogun::Pair::Bd), t, "flag stored", 1, 1);
  expect(near(pair, kBd1At48), t, "patched pair", pair, kBd1At48);
  expect(near(main, kBd1At48), t, "patched main", main, kBd1At48);

  shogun::Engine open;
  arm(open, bd1Example(), silentPattern());
  open.setPairPatched(shogun::Pair::Bd, false);
  open.trigger(shogun::Voice::Bd1);
  for (int n = 0; n <= 48; ++n) {
    open.process(in, f);
    if (n == 48) main = f.mainL;
  }
  expect(near(main, kBd1At48), t, "unpatched main", main, kBd1At48);

  shogun::Engine half;
  arm(half, bd1Example(), silentPattern());
  half.setMaster(0.5);
  half.trigger(shogun::Voice::Bd1);
  double halfPair = 0;
  double halfMain = 0;
  for (int n = 0; n <= 48; ++n) {
    half.process(in, f);
    if (n == 48) {
      halfPair = f.bdL;
      halfMain = f.mainL;
    }
  }
  expect(near(halfPair, kBd1At48), t, "master leaves pair", halfPair, kBd1At48);
  expect(near(halfMain, 0.5 * kBd1At48), t, "master scales main", halfMain, 0.5 * kBd1At48);

  shogun::Knobs mk;
  mk.maDecay = 55;
  shogun::Engine ma;
  arm(ma, mk, silentPattern());
  ma.trigger(shogun::Voice::Ma);
  ma.process(in, f);
  const double printed = -0.093967;
  expect(near(ma.maSample(), printed), t, "maracas y0", ma.maSample(), printed);
  expect(near(f.mainL, shogun::kCenterGain * printed), t, "maracas main L", f.mainL, shogun::kCenterGain * printed);
  expect(near(f.mainR, shogun::kCenterGain * printed), t, "maracas main R", f.mainR, shogun::kCenterGain * printed);
  expect(f.bdL == 0 && f.bdR == 0 && f.sdL == 0 && f.rsR == 0, t, "not on bd/sd", f.bdL, 0);
  expect(f.hhL == 0 && f.cyR == 0 && f.cpL == 0 && f.cpR == 0, t, "not on hh/cp", f.hhL, 0);
  expect(f.toL == 0 && f.toR == 0 && f.clL == 0 && f.cbR == 0, t, "not on to/cb", f.toL, 0);
}

void testShortVoices() {
  const char* t = "testShortVoices";
  shogun::TrigIn in;
  shogun::Frame f;

  shogun::Knobs rs;
  rs.rsTune = 40;
  shogun::Engine e;
  arm(e, rs, silentPattern());
  e.trigger(shogun::Voice::Rs);
  for (int n = 0; n <= 8; ++n) e.process(in, f);
  expect(near(f.rsR, 0.507608), t, "rim", f.rsR, 0.507608);

  shogun::Knobs cl;
  cl.clTune = 50;
  cl.clDecay = 30;
  arm(e, cl, silentPattern());
  e.trigger(shogun::Voice::Cl);
  for (int n = 0; n <= 12; ++n) e.process(in, f);
  expect(near(f.clL, 0.972955), t, "clave", f.clL, 0.972955);

  shogun::Knobs cb;
  cb.cbTune = 48;
  cb.cbDecay = 70;
  arm(e, cb, silentPattern());
  e.trigger(shogun::Voice::Cb);
  for (int n = 0; n <= 15; ++n) e.process(in, f);
  expect(near(f.cbR, 0.404500), t, "cowbell", f.cbR, 0.404500);

  shogun::Knobs cy;
  cy.cyTune = 64;
  cy.cyTone = 70;
  cy.cyDecay = 90;
  arm(e, cy, silentPattern());
  e.trigger(shogun::Voice::Cy);
  for (int n = 0; n <= 30; ++n) e.process(in, f);
  // The metal is the six-square stack at f0 = 180 * 5^u(Tune); Stack B is the noise draw.
  const double f0 = 180.0 * std::pow(5.0, 64.0 / 127.0);
  expect(near(e.cyStackA(), shogun::metalSquares(f0, 30), 1e-12), t, "cymbal metal", e.cyStackA(), shogun::metalSquares(f0, 30));
  expect(std::fabs(e.cyStackB()) <= 1.0 && e.cyStackB() != 0.0, t, "cymbal noise", e.cyStackB(), 0.5);
  expect(f.cyR != 0.0, t, "cymbal in the bus", f.cyR, 0.5);

  shogun::Knobs tom;
  tom.ltcTune = 40;
  tom.ltcDecay = 127;
  tom.ltcMode = 0;
  tom.ltcNoise = 0;
  arm(e, tom, silentPattern());
  e.trigger(shogun::Voice::Ltc);
  e.process(in, f);
  expect(near(e.ltcHz(), 94.251191), t, "ltc tune", e.ltcHz(), 94.251191);
  for (int n = 1; n <= 96000; ++n) e.process(in, f);
  expect(near(e.ltcEnv(), 0.551484), t, "ltc hold", e.ltcEnv(), 0.551484);

  tom.ltcMode = 64;
  shogun::Engine conga;
  arm(conga, tom, silentPattern());
  conga.trigger(shogun::Voice::Ltc);
  shogun::Engine asTom;
  tom.ltcMode = 0;
  arm(asTom, tom, silentPattern());
  asTom.trigger(shogun::Voice::Ltc);
  bool differ = false;
  for (int n = 0; n <= 10; ++n) {
    conga.process(in, f);
    const double c = f.toL;
    asTom.process(in, f);
    if (std::fabs(c - f.toL) > 1e-6) differ = true;
  }
  expect(differ, t, "conga is not a tom", differ ? 1.0 : 0.0, 1.0);

  shogun::Knobs bassK;
  bassK.bassTone = 127;
  arm(e, bassK, silentPattern());
  e.process(in, f);
  expect(near(f.cv3, 5.0, 1e-12), t, "cv3", f.cv3, 5.0);
}

void testShuffleAndShift() {
  const char* t = "testShuffleAndShift";
  shogun::Pattern shuf = silentPattern();
  auto& bd = shuf.track[static_cast<int>(shogun::Voice::Bd1)];
  bd.drum[1].on = true;
  bd.drum[1].accent = 2;
  bd.shuffle = 8;
  shogun::Engine e;
  arm(e, bd1Example(), shuf);
  e.setMode(shogun::ClockMode::Int);
  shogun::TrigIn in;
  shogun::Frame f;
  double early = 1;
  double onTime = 0;
  for (int i = 0; i <= 7076; ++i) {
    e.process(in, f);
    if (i == 6010) early = f.bdL;
    if (i == 7076) onTime = f.bdL;
  }
  expect(early == 0.0, t, "odd step waits", early, 0.0);
  expect(near(onTime, kBd1At10), t, "shuffle fire", onTime, kBd1At10);
  expect(shuf.track[static_cast<int>(shogun::Voice::Bd1)].shuffle == 8, t, "pattern shuffle unchanged", 8, 8);

  shogun::Engine held;
  arm(held, bd1Example(), shuf);
  held.setMode(shogun::ClockMode::Int);
  held.setGlobalShuffle(0);
  double heldY = 0;
  for (int i = 0; i <= 6010; ++i) {
    held.process(in, f);
    if (i == 6010) heldY = f.bdL;
  }
  expect(near(heldY, kBd1At10), t, "global shuffle override", heldY, kBd1At10);
  expect(shuf.track[static_cast<int>(shogun::Voice::Bd1)].shuffle == 8, t, "override does not write", 8, 8);

  shogun::Pattern shifted = silentPattern();
  shifted.track[static_cast<int>(shogun::Voice::Bd1)].shiftCc = 127;
  shifted.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].on = true;
  shifted.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].accent = 2;
  shogun::Engine sh;
  arm(sh, bd1Example(), shifted);
  sh.setMode(shogun::ClockMode::Int);
  double before = 1;
  double at = 0;
  for (int i = 0; i <= 1450; ++i) {
    sh.process(in, f);
    if (i == 10) before = f.bdL;
    if (i == 1450) at = f.bdL;
  }
  expect(before == 0.0, t, "shift holds the step", before, 0.0);
  expect(near(at, kBd1At10), t, "shift of 1440", at, kBd1At10);

  shogun::Engine host;
  arm(host, bd1Example(), silentPattern());
  host.setHostTempo(60, true);
  expect(host.periodSamples() == 12000.0, t, "host tempo", host.periodSamples(), 12000.0);
  host.setHostTempo(60, false);
  expect(host.periodSamples() == 6000.0, t, "internal tempo returns", host.periodSamples(), 6000.0);
}

void testClockNotesAndEdges() {
  const char* t = "testClockNotesAndEdges";
  shogun::TrigIn in;
  shogun::Frame f;
  shogun::Engine scales;
  arm(scales, bd1Example(), silentPattern());
  scales.setScaleSteps(8);
  expect(scales.periodSamples() == 3000.0, t, "32nd", scales.periodSamples(), 3000.0);
  scales.setScaleSteps(6);
  expect(scales.periodSamples() == 4000.0, t, "16th triplet", scales.periodSamples(), 4000.0);
  scales.setScaleSteps(3);
  expect(scales.periodSamples() == 8000.0, t, "8th triplet", scales.periodSamples(), 8000.0);
  scales.setScaleSteps(5);
  expect(scales.periodSamples() == 8000.0, t, "unknown scale ignored", scales.periodSamples(), 8000.0);

  shogun::Pattern even = silentPattern();
  even.track[static_cast<int>(shogun::Voice::Bd1)].shuffle = 8;
  even.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].on = true;
  even.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].accent = 2;
  shogun::Engine evenE;
  arm(evenE, bd1Example(), even);
  evenE.setMode(shogun::ClockMode::Int);
  for (int i = 0; i <= 10; ++i) evenE.process(in, f);
  expect(near(f.bdL, kBd1At10), t, "even step is not shuffled", f.bdL, kBd1At10);

  shogun::Pattern cyc = silentPattern();
  cyc.track[static_cast<int>(shogun::Voice::Bd1)].length = 2;
  cyc.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].on = true;
  cyc.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].accent = 2;
  shogun::Engine cycE;
  arm(cycE, bd1Example(), cyc);
  cycE.setMode(shogun::ClockMode::Int);
  for (int i = 0; i <= 12010; ++i) cycE.process(in, f);
  expect(near(f.bdL, kBd1At10), t, "track length cycles", f.bdL, kBd1At10);

  shogun::Pattern muted = bypassPattern();
  muted.track[static_cast<int>(shogun::Voice::Bd1)].mute = true;
  shogun::Engine mu;
  arm(mu, bd1Example(), muted);
  mu.setMode(shogun::ClockMode::Int);
  bool muteSilent = true;
  for (int i = 0; i <= 20; ++i) {
    mu.process(in, f);
    if (f.bdL != 0.0) muteSilent = false;
  }
  expect(muteSilent, t, "mute", muteSilent ? 0.0 : 1.0, 0.0);

  shogun::Knobs dist = bd1Example();
  dist.bd1Dist = 64;
  shogun::Engine d;
  arm(d, dist, silentPattern());
  d.trigger(shogun::Voice::Bd1);
  for (int n = 0; n <= 48; ++n) d.process(in, f);
  expect(std::fabs(f.bdL - kBd1At48) > 1e-4, t, "dist is a clip", f.bdL, kBd1At48);

  shogun::Engine edge;
  arm(edge, bd1Example(), silentPattern());
  edge.setMode(shogun::ClockMode::Ext);
  in.volts[static_cast<int>(shogun::Voice::Bd1)] = 1.0;
  in.velocity[static_cast<int>(shogun::Voice::Bd1)] = -1;
  for (int n = 0; n <= 10; ++n) edge.process(in, f);
  expect(near(f.bdL, kBd1At10), t, "edge at 1 V uses velocity 127", f.bdL, kBd1At10);

  shogun::Engine low;
  arm(low, bd1Example(), silentPattern());
  low.setMode(shogun::ClockMode::Ext);
  in = shogun::TrigIn{};
  in.volts[static_cast<int>(shogun::Voice::Bd1)] = 0.9;
  in.velocity[static_cast<int>(shogun::Voice::Bd1)] = 127;
  bool lowSilent = true;
  for (int n = 0; n <= 10; ++n) {
    low.process(in, f);
    if (f.bdL != 0.0) lowSilent = false;
  }
  expect(lowSilent, t, "0.9 V is not an edge", lowSilent ? 0.0 : 1.0, 0.0);

  shogun::Knobs tone;
  tone.leadTone = 80;
  tone.bassTone = 64;
  shogun::Pattern tied = silentPattern();
  tied.track[static_cast<int>(shogun::Voice::Lead)].note[0].note = 60;
  tied.track[static_cast<int>(shogun::Voice::Lead)].note[1].note = 60;
  tied.track[static_cast<int>(shogun::Voice::Lead)].note[0].accent = 0;
  shogun::Pattern changed = tied;
  changed.track[static_cast<int>(shogun::Voice::Lead)].note[1].note = 62;
  shogun::Engine tieE;
  shogun::Engine chgE;
  arm(tieE, tone, tied);
  arm(chgE, tone, changed);
  tieE.setMode(shogun::ClockMode::Int);
  chgE.setMode(shogun::ClockMode::Int);
  double tiedSaw = 0;
  double changedSaw = 1;
  for (int i = 0; i <= 6000; ++i) {
    tieE.process(in, f);
    chgE.process(in, f);
    if (i == 6000) {
      tiedSaw = tieE.leadSaw();
      changedSaw = chgE.leadSaw();
    }
  }
  expect(std::fabs(tiedSaw) > 0.05, t, "tied note keeps phase", tiedSaw, 0.2);
  expect(changedSaw == 0.0, t, "new note restarts", changedSaw, 0.0);

  shogun::Pattern accented = silentPattern();
  accented.track[static_cast<int>(shogun::Voice::Bass)].note[0].note = 48;
  accented.track[static_cast<int>(shogun::Voice::Bass)].note[0].accent = 0;
  shogun::Pattern loud = accented;
  loud.track[static_cast<int>(shogun::Voice::Bass)].note[0].accent = 2;
  shogun::Engine soft;
  shogun::Engine full;
  arm(soft, tone, accented);
  arm(full, tone, loud);
  soft.setMode(shogun::ClockMode::Int);
  full.setMode(shogun::ClockMode::Int);
  double softB = 0;
  double fullB = 0;
  shogun::TrigIn none;
  for (int n = 0; n <= 25; ++n) {
    soft.process(none, f);
    softB = f.mainL;
    full.process(none, f);
    fullB = f.mainL;
  }
  expect(near(softB / fullB, 0.55, 1e-9), t, "bass accent", softB / fullB, 0.55);

  shogun::Pattern leadSoft = silentPattern();
  leadSoft.track[static_cast<int>(shogun::Voice::Lead)].note[0].note = 60;
  leadSoft.track[static_cast<int>(shogun::Voice::Lead)].note[0].accent = 0;
  shogun::Pattern leadLoud = leadSoft;
  leadLoud.track[static_cast<int>(shogun::Voice::Lead)].note[0].accent = 2;
  shogun::Engine ls;
  shogun::Engine ll;
  arm(ls, tone, leadSoft);
  arm(ll, tone, leadLoud);
  ls.setMode(shogun::ClockMode::Int);
  ll.setMode(shogun::ClockMode::Int);
  double softL = 0;
  double fullL = 0;
  double softMain = 0;
  double fullMain = 0;
  for (int n = 0; n <= 25; ++n) {
    ls.process(none, f);
    softL = ls.leadSaw();
    softMain = f.mainL;
    ll.process(none, f);
    fullL = ll.leadSaw();
    fullMain = f.mainL;
  }
  expect(near(softL, fullL, 0.0), t, "lead has no accent", softL, fullL);
  expect(near(softMain, fullMain, 1e-12), t, "lead mix ignores accent", softMain, fullMain);

  shogun::Pattern restAfter = silentPattern();
  restAfter.track[static_cast<int>(shogun::Voice::Lead)].note[0].note = 60;
  shogun::Engine released;
  shogun::Engine heldNote;
  arm(released, tone, restAfter);
  arm(heldNote, tone, tied);
  released.setMode(shogun::ClockMode::Int);
  heldNote.setMode(shogun::ClockMode::Int);
  double rel = 0;
  double held = 0;
  for (int i = 0; i <= 8000; ++i) {
    released.process(none, f);
    rel = f.mainL;
    heldNote.process(none, f);
    held = f.mainL;
  }
  expect(std::fabs(rel) > 1e-4, t, "rest releases instead of choking", rel, held);
  expect(std::fabs(rel) < 0.5 * std::fabs(held), t, "release is quieter", rel, held);

  shogun::Engine extNote;
  shogun::Engine extHeld;
  arm(extNote, tone, silentPattern());
  arm(extHeld, tone, silentPattern());
  extNote.setMode(shogun::ClockMode::Ext);
  extHeld.setMode(shogun::ClockMode::Ext);
  extNote.setLiveNote(shogun::Voice::Lead, 60);
  extHeld.setLiveNote(shogun::Voice::Lead, 60);
  shogun::TrigIn gate;
  gate.volts[static_cast<int>(shogun::Voice::Lead)] = 5.0;
  gate.velocity[static_cast<int>(shogun::Voice::Lead)] = 127;
  double extSaw = 0;
  for (int n = 0; n <= 25; ++n) {
    extNote.process(gate, f);
    extHeld.process(gate, f);
    if (n == 25) extSaw = extNote.leadSaw();
  }
  expect(near(extSaw, 0.387776), t, "ext note edge", extSaw, 0.387776);
  shogun::TrigIn dropped;
  double extRel = 0;
  double extHold = 0;
  for (int n = 0; n < 2000; ++n) {
    extNote.process(dropped, f);
    extRel = f.mainL;
    extHeld.process(gate, f);
    extHold = f.mainL;
  }
  expect(std::fabs(extRel) > 1e-4, t, "ext release is not a choke", extRel, extHold);
  expect(std::fabs(extRel) < 0.5 * std::fabs(extHold), t, "ext gate drop releases", extRel, extHold);

  shogun::Knobs nz;
  nz.ltcNoise = 64;
  nz.tomNoise = 33;
  nz.ltcTune = 40;
  shogun::Engine tomN;
  arm(tomN, nz, silentPattern());
  tomN.trigger(shogun::Voice::Ltc);
  tomN.process(in, f);
  const double x0 = -0.527088949456811;
  const double y = (33.0 / 127.0) * x0;
  // Equal-power pan at p = -0.7: gL = cos(pi / 4 * 0.3) = 0.972370. Printed toL = -0.133176.
  const double gL = 0.972370;
  expect(near(f.toL, y * gL), t, "shared tom noise", f.toL, y * gL);
  expect(near(f.toL, -0.133176), t, "printed tom noise", f.toL, -0.133176);
  expect(f.bdL == 0.0 && f.mainL != 0.0, t, "tom reaches main", f.mainL, y * gL);
}

// Sample index of every BD1 restart, read from the instantaneous frequency jumping back up.
std::vector<long> bd1Starts(shogun::Engine& e, long samples) {
  shogun::TrigIn in;
  shogun::Frame f;
  std::vector<long> at;
  double last = e.bd1Hz();
  for (long i = 0; i < samples; ++i) {
    e.process(in, f);
    if (e.bd1Hz() > last + 1e-9) at.push_back(i);
    last = e.bd1Hz();
  }
  return at;
}

void testVoiceEndsWhenQuiet() {
  const char* t = "testVoiceEndsWhenQuiet";
  shogun::TrigIn in;
  shogun::Frame f;
  // Every drum voice, default knobs and the BD1 example: it sounds, ends, then outputs exactly 0.
  for (int v = 0; v < static_cast<int>(shogun::Voice::Lead); ++v) {
    shogun::Engine e;
    arm(e, v == 0 ? bd1Example() : shogun::Knobs{}, silentPattern());
    e.setLevel(static_cast<shogun::Voice>(v), 1);
    e.trigger(static_cast<shogun::Voice>(v));
    bool sounded = false;
    long endedAt = -1;
    bool silentAfter = true;
    for (long i = 0; i < 12L * 48000; ++i) {
      e.process(in, f);
      if (endedAt < 0) {
        if (f.mainL != 0.0 || f.mainR != 0.0) sounded = true;
        if (!e.voiceActive(static_cast<shogun::Voice>(v))) endedAt = i;
      } else if (i > endedAt && (f.mainL != 0.0 || f.mainR != 0.0)) {
        silentAfter = false;
      }
    }
    expect(sounded, t, "voice sounded", static_cast<double>(v), 1);
    expect(endedAt > 0, t, "voice ended", static_cast<double>(v), 1);
    expect(silentAfter, t, "0 after the end", static_cast<double>(v), 0);
  }
  // BD1 example: body tau 0.136195 s, so the end is where exp(-n / (fs * tau)) crosses 1e-3.
  shogun::Engine bd;
  arm(bd, bd1Example(), silentPattern());
  bd.trigger(shogun::Voice::Bd1);
  long n = 0;
  while (bd.voiceActive(shogun::Voice::Bd1) || n == 0) {
    bd.process(in, f);
    ++n;
  }
  const double want = std::ceil(-std::log(1e-3) * 48000.0 * shogun::decayTau(80.0 / 127.0));
  expect(std::fabs(static_cast<double>(n) - want) <= 1.0, t, "BD1 ends at the 1e-3 crossing", n, want);
  // With Noise up, the noise rides the body envelope: BD1 ends at the same crossing and is 0 after it.
  shogun::Knobs noisy = bd1Example();
  noisy.bd1Noise = 127;
  shogun::Engine bn;
  arm(bn, noisy, silentPattern());
  bn.trigger(shogun::Voice::Bd1);
  long m = 0;
  while (bn.voiceActive(shogun::Voice::Bd1) || m == 0) {
    bn.process(in, f);
    ++m;
  }
  expect(std::fabs(static_cast<double>(m) - want) <= 1.0, t, "noisy BD1 ends with the body", m, want);
  bool noisySilent = true;
  for (int i = 0; i < 48000; ++i) {
    bn.process(in, f);
    if (f.bdL != 0.0) noisySilent = false;
  }
  expect(noisySilent, t, "noisy BD1 is 0 after the body", noisySilent ? 0.0 : 1.0, 0.0);
  // A held BD2 does not end, and an ended voice plays again on the next trigger.
  shogun::Knobs hold;
  hold.bd2Decay = 127;
  shogun::Engine b2;
  arm(b2, hold, silentPattern());
  b2.trigger(shogun::Voice::Bd2);
  for (long i = 0; i < 10L * 48000; ++i) b2.process(in, f);
  expect(b2.voiceActive(shogun::Voice::Bd2), t, "held BD2 keeps sounding", 0, 1);
  bd.trigger(shogun::Voice::Bd1);
  for (int i = 0; i <= 10; ++i) bd.process(in, f);
  expect(near(f.bdL, kBd1At10), t, "ended voice retriggers", f.bdL, kBd1At10);
}

void testDistBypassAndDrive() {
  const char* t = "testDistBypassAndDrive";
  shogun::TrigIn in;
  auto at48 = [&](int dist) {
    shogun::Knobs k = bd1Example();
    k.bd1Dist = dist;
    shogun::Engine e;
    arm(e, k, silentPattern());
    e.trigger(shogun::Voice::Bd1);
    shogun::Frame f;
    for (int n = 0; n <= 48; ++n) e.process(in, f);
    return f.bdL;
  };
  expect(near(at48(0), kBd1At48), t, "Dist 0 is y = pre", at48(0), kBd1At48);
  expect(std::fabs(at48(1) - at48(0)) < 2e-3, t, "Dist 1 is next to the bypass", at48(1), at48(0));
  expect(near(at48(64), 0.183040), t, "Dist 64 printed row", at48(64), 0.183040);
}

void testCenterPanIsNotHalf() {
  const char* t = "testCenterPanIsNotHalf";
  shogun::TrigIn in;
  shogun::Frame f;
  shogun::Knobs k;
  k.maDecay = 55;
  shogun::Engine e;
  arm(e, k, silentPattern());
  e.trigger(shogun::Voice::Ma);
  e.process(in, f);
  const double y = e.maSample();
  expect(near(f.mainL, 0.707107 * y, 1e-6) && near(f.mainR, 0.707107 * y, 1e-6), t, "maracas at 0.707", f.mainL, 0.707107 * y);
  expect(std::fabs(f.mainL - 0.5 * y) > 1e-3, t, "not 0.5", f.mainL, 0.5 * y);
  expect(near(f.mainL, -0.066445), t, "printed maracas main", f.mainL, -0.066445);

  // Every centre is 0.707: the mid tom on its pair and the main, and the clap tail on both channels.
  shogun::Knobs mt;
  mt.mtcTune = 50;
  mt.mtcDecay = 60;
  shogun::Engine m;
  arm(m, mt, silentPattern());
  m.trigger(shogun::Voice::Mtc);
  for (int i = 0; i <= 40; ++i) m.process(in, f);
  expect(f.toL != 0.0 && near(f.toL, f.toR, 1e-12), t, "MTC is centred", f.toL, f.toR);
  expect(near(f.mainL, f.toL, 1e-12), t, "MTC main matches its pair", f.mainL, f.toL);
  shogun::Knobs tail;
  tail.cpAttack = 0;
  tail.cpDecay = 50;
  shogun::Engine cp;
  arm(cp, tail, silentPattern());
  cp.trigger(shogun::Voice::Cp);
  // Attack 0 leaves only the tail. It opens one gap (528 samples) after the last burst and is equal on both sides.
  bool quietBefore = true;
  for (int i = 0; i < 528; ++i) {
    cp.process(in, f);
    if (f.cpL != 0.0 || f.cpR != 0.0) quietBefore = false;
  }
  expect(quietBefore, t, "no tail before its delay", f.cpL, 0.0);
  for (int i = 0; i < 40; ++i) cp.process(in, f);
  expect(f.cpL != 0.0 && near(f.cpL, f.cpR, 1e-12) && near(f.mainL, f.cpL, 1e-12), t, "clap tail centred", f.cpL, f.cpR);
}

// Decay law: tau = 8 ms * exp(4.5 u) on every drum decay, and a voice is 0 once its envelope is under 1e-3.
void testDecayNoonIsShort() {
  const char* t = "testDecayNoonIsShort";
  expect(near(shogun::decayTau(0.0), 0.008), t, "decay 0 is 8 ms", shogun::decayTau(0.0), 0.008);
  expect(near(shogun::decayTau(0.5), 0.075902), t, "noon is 76 ms", shogun::decayTau(0.5), 0.075902);
  expect(near(shogun::decayTau(1.0), 0.720137), t, "full is 720 ms", shogun::decayTau(1.0), 0.720137);
  // Decay 64 on each knob: the voice ends where exp(-n / (fs * tau)) crosses 1e-3, about 0.53 s, not seconds later.
  shogun::TrigIn in;
  shogun::Frame f;
  const double tau64 = shogun::decayTau(64.0 / 127.0);
  const long want = static_cast<long>(std::ceil(-std::log(1e-3) * 48000.0 * tau64));
  struct Case { shogun::Voice v; const char* name; void (*set)(shogun::Knobs&); };
  const Case cases[] = {
      {shogun::Voice::Bd2, "bd2", [](shogun::Knobs& k) { k.bd2Decay = 64; }},
      {shogun::Voice::Cy, "cymbal", [](shogun::Knobs& k) { k.cyDecay = 64; }},
      {shogun::Voice::Oh, "open hat", [](shogun::Knobs& k) { k.ohDecay = 64; }},
      {shogun::Voice::Hh, "closed hat", [](shogun::Knobs& k) { k.hhDecay = 64; }},
      {shogun::Voice::Cl, "claves", [](shogun::Knobs& k) { k.clDecay = 64; }},
      {shogun::Voice::Mtc, "mid tom", [](shogun::Knobs& k) { k.mtcDecay = 64; }},
      {shogun::Voice::Cb, "cowbell", [](shogun::Knobs& k) { k.cbDecay = 64; }},
      {shogun::Voice::Ma, "maracas", [](shogun::Knobs& k) { k.maDecay = 64; }},
      // The snare's tone and noise and the clap tail use the same curve.
      {shogun::Voice::Sd, "snare tone and noise", [](shogun::Knobs& k) { k.sdToneDecay = 64; k.sdSnDecay = 64; k.sdSnappy = 127; }},
      {shogun::Voice::Cp, "clap tail", [](shogun::Knobs& k) { k.cpDecay = 64; k.cpData = 0; }},
  };
  // The clap tail opens 528 samples after its one burst, then runs the same curve.
  const long tailDelay = 528;
  for (const Case& c : cases) {
    shogun::Knobs k;
    c.set(k);
    shogun::Engine e;
    arm(e, k, silentPattern());
    e.trigger(c.v);
    long n = 0;
    for (; n < 10L * 48000; ++n) {
      e.process(in, f);
      if (!e.voiceActive(c.v)) break;
    }
    const long w = want + (c.v == shogun::Voice::Cp ? tailDelay : 0);
    expect(std::fabs(static_cast<double>(n - w)) <= 1.0, t, c.name, static_cast<double>(n), static_cast<double>(w));
  }
  // The sample where the envelope goes under 1e-3 is already 0.
  shogun::Knobs k;
  k.cbDecay = 64;
  shogun::Engine e;
  arm(e, k, silentPattern());
  e.trigger(shogun::Voice::Cb);
  for (long n = 0; n <= want; ++n) e.process(in, f);
  expect(f.cbR == 0.0 && f.mainR == 0.0, t, "0 under 1e-3", f.cbR, 0.0);
}

// BD2 at Decay 127 is the steady tone: it holds until the next hit.
void testBd2FullHolds() {
  const char* t = "testBd2FullHolds";
  shogun::TrigIn in;
  shogun::Frame f;
  shogun::Knobs h;
  h.bd2Decay = 127;
  shogun::Engine b2;
  arm(b2, h, silentPattern());
  b2.trigger(shogun::Voice::Bd2);
  for (int n = 0; n <= 480000; ++n) b2.process(in, f);
  expect(b2.voiceActive(shogun::Voice::Bd2), t, "still sounding at 10 s", 1.0, 1.0);
  expect(near(b2.bd2Env(), 0.7), t, "at the sustain", b2.bd2Env(), 0.7);
  b2.trigger(shogun::Voice::Bd2);
  b2.process(in, f);
  expect(near(b2.bd2Env(), 1.0), t, "the next hit restarts it", b2.bd2Env(), 1.0);
}

// A tom at Decay 127 rings for 4 s, releases on 50 ms and ends. It does not hold.
void testTomFullEnds() {
  const char* t = "testTomFullEnds";
  shogun::TrigIn in;
  shogun::Frame f;
  shogun::Knobs k;
  k.ltcTune = 40;
  k.ltcDecay = 127;
  shogun::Engine e;
  arm(e, k, silentPattern());
  e.trigger(shogun::Voice::Ltc);
  for (int n = 0; n <= 192000; ++n) e.process(in, f);
  expect(near(e.ltcEnv(), 0.550005), t, "rings at 4 s", e.ltcEnv(), 0.550005);
  expect(e.voiceActive(shogun::Voice::Ltc), t, "still sounding at 4 s", 1.0, 1.0);
  for (int n = 192001; n <= 194400; ++n) e.process(in, f);
  expect(near(e.ltcEnv(), 0.202335), t, "50 ms into the release", e.ltcEnv(), 0.202335);
  long endedAt = -1;
  for (int n = 194401; n <= 240000 && endedAt < 0; ++n) {
    e.process(in, f);
    if (!e.voiceActive(shogun::Voice::Ltc)) endedAt = n;
  }
  expect(endedAt == 207144, t, "ends under 1e-3 at 4.32 s", static_cast<double>(endedAt), 207144.0);

  shogun::Engine again;
  arm(again, k, silentPattern());
  again.trigger(shogun::Voice::Ltc);
  for (int n = 0; n <= 96000; ++n) again.process(in, f);
  again.trigger(shogun::Voice::Ltc);
  again.process(in, f);
  expect(near(again.ltcEnv(), 1.0), t, "the next hit replaces the ring", again.ltcEnv(), 1.0);
}

// S3: the SD step bend is a drop to Tune with its own time. Pitch 0 with a bend still swoops: the time has an 80 ms floor.
void testSnareBendAtPitchZero() {
  const char* t = "testSnareBendAtPitchZero";
  shogun::TrigIn in;
  shogun::Frame f;
  shogun::Knobs k;
  k.sdTune = 70;
  k.sdPitch = 0;
  k.sdToneDecay = 127;
  shogun::Engine e;
  arm(e, k, silentPattern());
  e.trigger(shogun::Voice::Sd, 1.0, 12.0);
  e.process(in, f);
  expect(near(e.sdHz(), 466.028122), t, "bend +12 starts an octave up", e.sdHz(), 466.028122);
  for (int n = 1; n <= 3840; ++n) e.process(in, f);
  // n = 3840 is 80 ms: the bend is at 1/e. On the Pitch time (10 ms at Pitch 0) it would be 233.068249.
  expect(near(e.sdHz(), 300.694079), t, "pitch 0 still swoops at 80 ms", e.sdHz(), 300.694079);

  shogun::Engine flat;
  arm(flat, k, silentPattern());
  flat.trigger(shogun::Voice::Sd);
  bool atTune = true;
  for (int n = 0; n <= 4000; ++n) {
    flat.process(in, f);
    if (!near(flat.sdHz(), 233.014061)) atTune = false;
  }
  expect(atTune, t, "bend 0 adds nothing", flat.sdHz(), 233.014061);

  // A Pitch time longer than the floor wins, and the bend adds on top of the Pitch depth.
  k.sdPitch = 127;
  shogun::Engine deep;
  arm(deep, k, silentPattern());
  deep.trigger(shogun::Voice::Sd, 1.0, 12.0);
  for (int n = 0; n <= 6240; ++n) deep.process(in, f);
  expect(near(deep.sdHz(), 404.878529), t, "pitch 127 time wins", deep.sdHz(), 404.878529);
}

// Solo: only the soloed voice reaches its pair and the main. The others still run and come back in place. Mute wins.
void testSoloMutesOtherVoices() {
  const char* t = "testSoloMutesOtherVoices";
  shogun::TrigIn in;
  shogun::Frame f;
  shogun::Frame r;
  shogun::Pattern p = silentPattern();
  p.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].on = true;
  p.track[static_cast<int>(shogun::Voice::Sd)].drum[0].on = true;
  shogun::Knobs k = bd1Example();
  k.sdToneDecay = 127;
  shogun::Engine ref;
  arm(ref, k, p);
  ref.setLevel(shogun::Voice::Sd, 1);
  shogun::Engine e;
  arm(e, k, p);
  e.setLevel(shogun::Voice::Sd, 1);
  e.setSolo(static_cast<int>(shogun::Voice::Sd));
  bool othersOut = true;
  bool soloAsBefore = true;
  for (int n = 0; n < 200; ++n) {
    ref.process(in, r);
    e.process(in, f);
    if (f.bdL != 0.0 || f.mainL != f.sdL || f.mainR != 0.0) othersOut = false;
    if (!near(f.sdL, r.sdL, 1e-12)) soloAsBefore = false;
  }
  expect(othersOut, t, "only the snare reaches the main", f.bdL, 0.0);
  expect(soloAsBefore, t, "the soloed voice is unchanged", f.sdL, r.sdL);
  expect(e.counter() == ref.counter(), t, "the clock runs on", static_cast<double>(e.counter()), static_cast<double>(ref.counter()));
  e.setSolo(-1);
  ref.process(in, r);
  e.process(in, f);
  expect(r.bdL != 0.0 && near(f.bdL, r.bdL, 1e-12), t, "solo off: BD1 is back where it would be", f.bdL, r.bdL);
  expect(near(f.mainL, r.mainL, 1e-12), t, "solo off: the mix returns", f.mainL, r.mainL);

  // Mute wins over solo on the same track.
  shogun::Pattern muted = p;
  muted.track[static_cast<int>(shogun::Voice::Sd)].mute = true;
  shogun::Engine m;
  arm(m, k, muted);
  m.setLevel(shogun::Voice::Sd, 1);
  m.setSolo(static_cast<int>(shogun::Voice::Sd));
  m.trigger(shogun::Voice::Sd);
  bool silent = true;
  for (int n = 0; n < 200; ++n) {
    m.process(in, f);
    if (!audioSilent(f)) silent = false;
  }
  expect(silent, t, "a muted solo is silent", f.mainL, 0.0);
}

void testShuffleSurvivesOddLength() {
  const char* t = "testShuffleSurvivesOddLength";
  // Track length 3, every step on, shuffle 15: the delay is period / 3 = 2,000 samples on odd clock steps.
  shogun::Pattern p = silentPattern();
  auto& bd = p.track[static_cast<int>(shogun::Voice::Bd1)];
  bd.length = 3;
  bd.shuffle = 15;
  for (int s = 0; s < 3; ++s) bd.drum[s].on = true;
  shogun::Engine e;
  arm(e, bd1Example(), p);
  const std::vector<long> at = bd1Starts(e, 6 * 6000);
  const long want[] = {0, 8000, 12000, 20000, 24000, 32000};
  expect(at.size() == 6, t, "six steps", static_cast<double>(at.size()), 6);
  for (size_t i = 0; i < at.size() && i < 6; ++i) {
    expect(at[i] == want[i], t, "odd clock step swings", static_cast<double>(at[i]), static_cast<double>(want[i]));
  }
}

void testInitKitIs909Steps() {
  const char* t = "testInitKitIs909Steps";
  shogun::Engine e;  // fresh engine, no reset()
  const shogun::Pattern& p = e.pattern();
  expect(std::strcmp(p.name, "909") == 0, t, "the init pattern is named 909", 0, 0);
  expect(p.length == 16, t, "one bar of 16 steps", p.length, 16);
  // Kick 1 and 9, snare and clap 5 and 13, open hat 3 7 11 15, closed hat on the other steps (it would choke the open hat).
  bool map = true;
  for (int v = 0; v < shogun::kVoiceCount; ++v) {
    const shogun::Track& tr = p.track[v];
    if (tr.length != 16) map = false;
    for (int s = 0; s < shogun::kMaxSteps; ++s) {
      bool want = false;
      if (s < 16) {
        switch (static_cast<shogun::Voice>(v)) {
          case shogun::Voice::Bd1: want = s % 8 == 0; break;
          case shogun::Voice::Sd:
          case shogun::Voice::Cp: want = s % 8 == 4; break;
          case shogun::Voice::Oh: want = s % 4 == 2; break;
          case shogun::Voice::Hh: want = s % 4 != 2; break;
          default: break;
        }
      }
      if (tr.drum[s].on != want) {
        map = false;
        std::printf("  voice %d step %d: on=%d want %d\n", v, s + 1, tr.drum[s].on ? 1 : 0, want ? 1 : 0);
      }
    }
  }
  expect(map, t, "step map matches the 909 sheet", 0, 0);
  const double bd1Decay = e.knobs().bd1Decay / 127.0;
  expect(bd1Decay < 0.35, t, "BD1 Decay under 0.35", bd1Decay, 0.35);
  expect(e.level(shogun::Voice::Bd2) == 0.0 && e.level(shogun::Voice::Ltc) == 0.0, t, "BD2 and toms at level 0",
         e.level(shogun::Voice::Bd2), 0.0);
  expect(near(e.master(), 0.7), t, "master 0.7", e.master(), 0.7);

  // One bar at 120 BPM, INT: the open hats ring (no choke), and the mix stays under full scale.
  shogun::TrigIn in;
  shogun::Frame f;
  double peak = 0;
  double ohPeak = 0;
  for (int n = 0; n < 16 * 6000; ++n) {
    e.process(in, f);
    peak = std::fmax(peak, std::fmax(std::fabs(f.mainL), std::fabs(f.mainR)));
    if (n >= 2 * 6000 && n < 3 * 6000) ohPeak = std::fmax(ohPeak, e.ohEnv());
  }
  std::printf("%s: one bar peak %.6f\n", t, peak);
  expect(ohPeak > 0.5, t, "the open hat on step 3 is not choked", ohPeak, 1.0);
  expect(peak > 0.1 && peak < 1.0, t, "one bar plays under full scale", peak, 0.5);
}

void testBd1WaveAddsHarmonics() {
  const char* t = "testBd1WaveAddsHarmonics";
  // Tune 127 is 140 Hz. Pitch 0 so the body holds its pitch, Attack and Noise 0 so only the body speaks.
  const double hz = 140.0;
  auto hit = [&](int wave) {
    shogun::Knobs k;
    k.bd1Tune = 127;
    k.bd1Pitch = 0;
    k.bd1Decay = 127;
    k.bd1Attack = 0;
    k.bd1Noise = 0;
    k.bd1Wave = wave;
    shogun::Engine e;
    arm(e, k, silentPattern());
    e.trigger(shogun::Voice::Bd1, 1.0);
    shogun::TrigIn in;
    shogun::Frame f;
    std::vector<double> x;
    for (int n = 0; n < 2048; ++n) {
      e.process(in, f);
      x.push_back(f.bdL);
    }
    return thirdOverFirst(x, hz);
  };
  std::vector<double> sine;
  for (int n = 0; n < 2048; ++n) {
    sine.push_back(std::sin(2.0 * shogun::kPi * hz * n / 48000.0) * std::exp(-n / (48000.0 * shogun::decayTau(1.0))));
  }
  const double pure = thirdOverFirst(sine, hz);
  const double bypass = hit(0);
  const double dflt = hit(shogun::Knobs{}.bd1Wave);
  std::printf("%s: 3rd / 1st sine %.6f, Wave 0 %.6f, default Wave %.6f\n", t, pure, bypass, dflt);
  expect(pure < 0.01, t, "a sine-only body has no third", pure, 0.0);
  expect(bypass < 0.01, t, "Wave 0 is the sine", bypass, 0.0);
  expect(dflt > 0.03, t, "velocity 1 hit at the default Wave has harmonics above the fundamental", dflt, 0.03);
}

void testBd2FmMovesSpectrum() {
  const char* t = "testBd2FmMovesSpectrum";
  // BD2 Tune 127 is 100 Hz. Tone sets the FM rate, 40 Hz at 0 and 200 Hz at 127.
  auto hit = [](int tone, int tune) {
    shogun::Knobs k;
    k.bd2Tune = tune;
    k.bd2Decay = 127;
    k.bd2Tone = tone;
    shogun::Engine e;
    arm(e, k, silentPattern());
    e.setLevel(shogun::Voice::Bd2, 1);
    e.trigger(shogun::Voice::Bd2, 1.0);
    shogun::TrigIn in;
    shogun::Frame f;
    std::vector<double> x;
    for (int n = 0; n < 2048 + 4800; ++n) {
      e.process(in, f);
      // Skip the first 100 ms so the transient is gone.
      if (n >= 4800) x.push_back(f.bdR);
    }
    return spectrum2048(x);
  };
  const std::vector<double> slow = hit(0, 127);
  const std::vector<double> fast = hit(127, 127);
  double moved = 0.0;
  double total = 0.0;
  for (size_t b = 0; b < slow.size(); ++b) {
    moved += std::fabs(fast[b] - slow[b]);
    total += slow[b];
  }
  const double rel = moved / total;
  std::printf("%s: spectrum moved %.6f of its sum\n", t, rel);
  expect(rel > 0.2, t, "Tone moves the FM spectrum", rel, 0.2);
  // Over one second the pitch swings f0 +/- 12 percent at the FM rate, never settling.
  shogun::Knobs k;
  k.bd2Tune = 127;
  k.bd2Decay = 127;
  k.bd2Tone = 0;
  shogun::Engine e;
  arm(e, k, silentPattern());
  e.trigger(shogun::Voice::Bd2, 1.0);
  shogun::TrigIn in;
  shogun::Frame f;
  double lo = 1e9;
  double hi = 0.0;
  for (int n = 0; n < 48000; ++n) {
    e.process(in, f);
    lo = std::fmin(lo, e.bd2Hz());
    hi = std::fmax(hi, e.bd2Hz());
  }
  expect(near(hi, 112.0, 0.01) && near(lo, 88.0, 0.01), t, "FM swings 100 Hz by 12 Hz", hi, 112.0);
}

void testSnareFilterDarkensNoise() {
  const char* t = "testSnareFilterDarkensNoise";
  // Brightness: RMS of the first difference over RMS of the signal. Lower is darker.
  auto bright = [](int tone) {
    shogun::Knobs k;
    k.sdSnappy = 127;
    k.sdSnDecay = 127;
    k.sdTone = tone;
    shogun::Engine e;
    arm(e, k, silentPattern());
    e.trigger(shogun::Voice::Sd, 1.0);
    shogun::TrigIn in;
    shogun::Frame f;
    double prev = 0.0;
    double d2 = 0.0;
    double s2 = 0.0;
    for (int n = 0; n < 2048; ++n) {
      e.process(in, f);
      const double x = e.sdNoise();
      d2 += (x - prev) * (x - prev);
      s2 += x * x;
      prev = x;
    }
    return std::sqrt(d2 / s2);
  };
  const double dark = bright(0);
  const double open = bright(127);
  std::printf("%s: brightness Tone 0 %.6f, Tone 127 %.6f\n", t, dark, open);
  expect(dark < 0.5 * open, t, "Tone 0 darkens the snare noise", dark, open);
}

void testHatIsInharmonic() {
  const char* t = "testHatIsInharmonic";
  // The metal: share of spectrum energy within two bins of a multiple of the lowest square.
  // One square would put nearly all of it on its harmonics.
  shogun::Knobs k;
  k.hhTune = 0;  // 250 Hz
  k.hhDecay = 127;
  shogun::Engine e;
  arm(e, k, silentPattern());
  e.trigger(shogun::Voice::Hh, 1.0);
  shogun::TrigIn in;
  shogun::Frame f;
  std::vector<double> metal;
  std::vector<double> one;
  for (int n = 0; n < 2048; ++n) {
    e.process(in, f);
    metal.push_back(e.hhStack());
    one.push_back(shogun::blepSquare(250.0, n));
  }
  auto harmonicShare = [](const std::vector<double>& x) {
    const std::vector<double> mag = spectrum2048(x);
    const double bin = 48000.0 / 2048.0;
    double on = 0.0;
    double all = 0.0;
    for (size_t b = 1; b < mag.size(); ++b) {
      const double e2 = mag[b] * mag[b];
      all += e2;
      const double h = b * bin / 250.0;
      if (std::fabs(h - std::round(h)) * 250.0 <= 2.0 * bin) on += e2;
    }
    return on / all;
  };
  const double square = harmonicShare(one);
  const double stack = harmonicShare(metal);
  std::printf("%s: harmonic share, one square %.6f, six-square stack %.6f\n", t, square, stack);
  expect(square > 0.95, t, "one square is harmonic", square, 1.0);
  expect(stack < 0.6, t, "the stack is inharmonic", stack, 0.6);
}

// Share of 2048-point spectrum energy above 1.5 times the fundamental.
double overtoneShare(const std::vector<double>& x, double hz) {
  const std::vector<double> mag = spectrum2048(x);
  const double bin = 48000.0 / 2048.0;
  double hi = 0.0;
  double all = 0.0;
  for (size_t b = 1; b < mag.size(); ++b) {
    const double e2 = mag[b] * mag[b];
    all += e2;
    if (b * bin > 1.5 * hz) hi += e2;
  }
  return hi / all;
}

// One hit at velocity 1 on a body voice, 2,048 samples of its own output. Pitch and bend held still.
std::vector<double> bodyHit(shogun::Voice v, int wave) {
  shogun::Knobs k;
  k.bd1Tune = 127;
  k.bd1Pitch = 0;
  k.bd1Decay = 127;
  k.bd1Attack = 0;
  k.bd1Wave = wave;
  k.bd2Tune = 127;
  k.bd2Decay = 127;
  k.bd2Tone = 0;
  k.bd2Wave = wave;
  k.ltcTune = 127;
  k.ltcDecay = 126;
  k.ltcWave = wave;
  k.mtcTune = 127;
  k.mtcDecay = 126;
  k.mtcWave = wave;
  k.htcTune = 127;
  k.htcDecay = 126;
  k.htcWave = wave;
  shogun::Engine e;
  arm(e, k, silentPattern());
  for (int i = 0; i < shogun::kVoiceCount; ++i) e.setLevel(static_cast<shogun::Voice>(i), 1);
  e.trigger(v, 1.0);
  shogun::TrigIn in;
  shogun::Frame f;
  std::vector<double> x;
  for (int n = 0; n < 2048; ++n) {
    e.process(in, f);
    x.push_back(f.mainL + f.mainR);
  }
  return x;
}

struct BodyVoice {
  shogun::Voice v;
  const char* name;
  double hz;
};
const BodyVoice kBodyVoices[] = {
    {shogun::Voice::Bd1, "BD1", 140.0},
    {shogun::Voice::Bd2, "BD2", 100.0},
    {shogun::Voice::Ltc, "LTC", 180.0},
    {shogun::Voice::Mtc, "MTC", 280.0},
    {shogun::Voice::Htc, "HTC", 400.0},
};

void testWaveZeroIsBypass() {
  const char* t = "testWaveZeroIsBypass";
  // The folder at Wave 0 returns its input bit for bit.
  bool same = true;
  for (int i = -1000; i <= 1000; ++i) {
    const double x = i / 1000.0;
    if (shogun::wave::fold(x, 0) != x) same = false;
  }
  expect(same, t, "fold(x, 0) = x", 0, 0);
  // BD1 at Wave 0, Pitch 0, Attack 0, Noise 0, Dist 0 is the sine body under its envelope.
  shogun::Knobs k;
  k.bd1Tune = 127;
  k.bd1Pitch = 0;
  k.bd1Decay = 80;
  k.bd1Attack = 0;
  k.bd1Wave = 0;
  shogun::Engine e;
  arm(e, k, silentPattern());
  e.trigger(shogun::Voice::Bd1, 1.0);
  shogun::TrigIn in;
  shogun::Frame f;
  double phase = 0.0;
  double worst = 0.0;
  for (int n = 0; n < 4800; ++n) {
    e.process(in, f);
    const double want = std::sin(phase) * std::exp(-n / (48000.0 * shogun::decayTau(80.0 / 127.0)));
    worst = std::fmax(worst, std::fabs(f.bdL - want));
    phase += 2.0 * shogun::kPi * 140.0 / 48000.0;
  }
  expect(worst < 1e-9, t, "BD1 Wave 0 body is the sine", worst, 0.0);
  // Every body voice at Wave 0 has no overtones to speak of.
  for (const BodyVoice& b : kBodyVoices) {
    const double share = overtoneShare(bodyHit(b.v, 0), b.hz);
    expect(share < 0.01, t, b.name, share, 0.0);
  }
}

void testWaveAddsHarmonics() {
  const char* t = "testWaveAddsHarmonics";
  const int dflt = shogun::Knobs{}.bd1Wave;
  expect(dflt == 32, t, "default Wave is 0.25 (CC 32)", dflt, 32);
  expect(near(shogun::wave::driveOf(1), 0.5, 1e-12) && near(shogun::wave::driveOf(127), 4.0, 1e-12), t,
         "g is 0.5 at CC 1 and 4 at CC 127", shogun::wave::driveOf(127), 4.0);
  for (const BodyVoice& b : kBodyVoices) {
    const double off = overtoneShare(bodyHit(b.v, 0), b.hz);
    const double on = overtoneShare(bodyHit(b.v, dflt), b.hz);
    const double full = overtoneShare(bodyHit(b.v, 127), b.hz);
    std::printf("%s: %s overtone share Wave 0 %.6f, default %.6f, full %.6f\n", t, b.name, off, on, full);
    expect(on > 0.05 && on > 10.0 * off, t, b.name, on, 0.05);
    expect(full > 2.0 * off, t, b.name, full, off);
  }
  // A full-scale input peaks at 1 after the cells at any drive, so the default does not jump the level.
  const double gs[] = {0.5, 1.0, 1.2, 2.0, 3.3, 4.0, shogun::wave::driveOf(dflt)};
  for (double g : gs) {
    double peak = 0.0;
    for (int i = 0; i <= 20000; ++i) {
      const double a = -1.0 + 2.0 * i / 20000.0;
      peak = std::fmax(peak, std::fabs(shogun::wave::outputScale(g) * shogun::wave::stack(5.0 * g * a)));
    }
    expect(std::fabs(peak - 1.0) <= 1e-3, t, "full-scale peak is 1", peak, 1.0);
  }
}

// The ported cell against serge_middle GOLDEN.md.
void testWaveCellMatchesSergeMiddle() {
  const char* t = "testWaveCellMatchesSergeMiddle";
  const double vin[] = {-6.0, -1.0, -0.5, 0.0, 0.5, 1.0, 6.0};
  const double vout[] = {-0.574899, 0.160778, -0.136312, 0.0, 0.136312, -0.160778, 0.574899};
  for (int i = 0; i < 7; ++i) {
    const double got = shogun::wave::stack(vin[i]);
    expect(near(got, vout[i], 1e-6), t, "curve at g = 1", got, vout[i]);
  }
  const double peakY = shogun::wave::stack(shogun::wave::kFullScalePeakVin);
  expect(std::fabs(shogun::wave::kOutputGain * std::fabs(peakY) - 1.0) < 1e-12, t, "OUTPUT_GAIN * |y(4.707287 V)| = 1",
         shogun::wave::kOutputGain * std::fabs(peakY), 1.0);
  expect(shogun::wave::drivePeak(2.0) == 4.104493885791202, t, "P(2) is the stored knot", shogun::wave::drivePeak(2.0),
         4.104493885791202);
  // The W residual stays under 1e-12 through 6 V and at 48 V.
  double worst = 0.0;
  const double logK = std::log((shogun::wave::kIs * shogun::wave::kR) / shogun::wave::kEtaVT);
  for (int i = 1; i <= 601; ++i) {
    const double v = i <= 600 ? i * 0.01 : 48.0;
    const double w = shogun::wave::lambertW0KExp(v);
    const double logZ = v / shogun::wave::kEtaVT + logK;
    worst = std::fmax(worst, std::fabs(w + std::log(w) - logZ) / std::fmax(1.0, std::fabs(logZ)));
  }
  expect(worst < 1e-12, t, "Lambert W residual", worst, 0.0);
}

void testHatHasNoWave() {
  const char* t = "testHatHasNoWave";
  // Every Wave knob at 0 against every Wave knob at 127: hats, cymbal, clap, and maracas do not move.
  const shogun::Voice none[] = {shogun::Voice::Hh, shogun::Voice::Oh, shogun::Voice::Cy, shogun::Voice::Cp,
                                shogun::Voice::Ma};
  for (shogun::Voice v : none) {
    shogun::Knobs a;
    a.bd1Wave = a.bd2Wave = a.ltcWave = a.mtcWave = a.htcWave = 0;
    a.hhDecay = a.ohDecay = a.cyDecay = a.cpDecay = a.maDecay = 64;
    shogun::Knobs b = a;
    b.bd1Wave = b.bd2Wave = b.ltcWave = b.mtcWave = b.htcWave = 127;
    shogun::Engine ea;
    shogun::Engine eb;
    arm(ea, a, silentPattern());
    arm(eb, b, silentPattern());
    ea.trigger(v, 1.0);
    eb.trigger(v, 1.0);
    shogun::TrigIn in;
    shogun::Frame fa;
    shogun::Frame fb;
    bool same = true;
    bool sounded = false;
    for (int n = 0; n < 9600; ++n) {
      ea.process(in, fa);
      eb.process(in, fb);
      if (fa.mainL != fb.mainL || fa.mainR != fb.mainR) same = false;
      if (fa.mainL != 0.0 || fa.mainR != 0.0) sounded = true;
    }
    expect(same && sounded, t, "Wave does not reach this voice", same ? 1 : 0, 1);
  }
}

int main() {
  testKickBendDecays();
  testBd1SoundChangesAttack();
  testBd2CanHold();
  testSnareTwoTones();
  testHatChoke();
  testClapBurstCount();
  testExtBypassIgnoresPattern();
  testIntIgnoresTrigJacks();
  testRestIsSilent();
  testIndividualOutStaysInMix();
  testShortVoices();
  testShuffleAndShift();
  testClockNotesAndEdges();
  testVoiceEndsWhenQuiet();
  testDistBypassAndDrive();
  testCenterPanIsNotHalf();
  testShuffleSurvivesOddLength();
  testDecayNoonIsShort();
  testBd2FullHolds();
  testTomFullEnds();
  testSnareBendAtPitchZero();
  testSoloMutesOtherVoices();
  testInitKitIs909Steps();
  testBd1WaveAddsHarmonics();
  testBd2FmMovesSpectrum();
  testSnareFilterDarkensNoise();
  testHatIsInharmonic();
  testWaveZeroIsBypass();
  testWaveAddsHarmonics();
  testWaveCellMatchesSergeMiddle();
  testHatHasNoWave();
  if (gFails != 0) {
    std::printf("%d failed\n", gFails);
    return 1;
  }
  std::printf("all named checks passed\n");
  return 0;
}
