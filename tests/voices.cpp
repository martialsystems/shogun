#include "shogun.h"

#include <cmath>
#include <cstdio>

// Named checks from TESTPLAN.md, plus the short-voice rows in SCHEMATICS.md.
// Tolerance is 1e-5 on the printed decimals.

int gFails = 0;

void expect(bool ok, const char* test, const char* msg, double got, double want) {
  if (ok) return;
  std::printf("FAIL %s: %s got %.9f want %.9f\n", test, msg, got, want);
  ++gFails;
}

bool near(double got, double want, double tol = 1e-5) { return std::fabs(got - want) <= tol; }

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
  expect(near(y48, 0.832547), t, "y(48)", y48, 0.832547);
  expect(near(y480, -0.907532), t, "y(480)", y480, -0.907532);

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
  expect(near(y0, 0.306787), t, "trigger 0 y", y0, 0.306787);
  expect(near(tr0, 160.0), t, "trigger 0 f_tr", tr0, 160.0);
  expect(near(y64, 0.810530), t, "trigger 64 y", y64, 0.810530);
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
  expect(near(env, 0.292599), t, "decay 126", env, 0.292599);
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
  expect(near(t1, 0.671594), t, "t1", t1, 0.671594);
  expect(near(t2, 0.778808), t, "t2", t2, 0.778808);
  expect(near(y, 0.725623), t, "blend", y, 0.725623);

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
  expect(near(env200, 0.995412), t, "open env at 200", env200, 0.995412);

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
  expect(near(choked.hhTau(), 0.020598), t, "closed tau", choked.hhTau(), 0.020598);
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
  shogun::Engine e;
  arm(e, k, silentPattern());
  e.trigger(shogun::Voice::Cp);
  shogun::TrigIn in;
  shogun::Frame f;
  double b0 = 0, b1 = 0, b2 = 0, b3 = 0;
  for (int n = 0; n <= 1589; ++n) {
    e.process(in, f);
    if (n == 5) b0 = e.cpBurst(0);
    if (n == 533) b1 = e.cpBurst(1);
    if (n == 1061) b2 = e.cpBurst(2);
    if (n == 1589) b3 = e.cpBurst(3);
  }
  expect(e.cpCount() == 4, t, "count", static_cast<double>(e.cpCount()), 4);
  expect(near(b0, 0.427195), t, "burst 0", b0, 0.427195);
  expect(near(b1, 0.427195), t, "burst 1", b1, 0.427195);
  expect(near(b2, 0.427195), t, "burst 2", b2, 0.427195);
  expect(near(b3, 0.427195), t, "burst 3", b3, 0.427195);
  const double gapSec = 528.0 / 48000.0;
  expect(gapSec >= 0.010 && gapSec <= 0.012, t, "gap seconds", gapSec, 0.011);

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
  expect(near(oneAt5, 0.427195), t, "single burst", oneAt5, 0.427195);
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

  const double wantVel = 0.208884 * 0.819291;
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
      expect(near(fa.bdL, 0.208884), t, "next on-step", fa.bdL, 0.208884);
      expect(near(fb.bdL, 0.208884), t, "next on-step pair", fb.bdL, 0.208884);
    }
  }
  expect(near(yA, wantVel), t, "ext velocity y", yA, wantVel);
  expect(near(yB, wantVel), t, "ext velocity y pair", yB, wantVel);
  expect(std::fabs(yA - 0.208884) > 1e-4, t, "not pattern accent", yA, 0.208884);

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
  expect(near(y10, 0.208884), t, "step 0 accent", y10, 0.208884);
  expect(matchPulse, t, "mid-step jack ignored", matchPulse ? 0.0 : 1.0, 0.0);
  expect(matchHeld, t, "plugged cable ignored", matchHeld ? 0.0 : 1.0, 0.0);
  expect(std::fabs(y10 - 0.208884 * 0.819291) > 1e-3, t, "not the jack velocity", y10, 0.208884);
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
  expect(near(pair, 0.832547), t, "patched pair", pair, 0.832547);
  expect(near(main, 0.832547), t, "patched main", main, 0.832547);

  shogun::Engine open;
  arm(open, bd1Example(), silentPattern());
  open.setPairPatched(shogun::Pair::Bd, false);
  open.trigger(shogun::Voice::Bd1);
  for (int n = 0; n <= 48; ++n) {
    open.process(in, f);
    if (n == 48) main = f.mainL;
  }
  expect(near(main, 0.832547), t, "unpatched main", main, 0.832547);

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
  expect(near(halfPair, 0.832547), t, "master leaves pair", halfPair, 0.832547);
  expect(near(halfMain, 0.416273), t, "master scales main", halfMain, 0.416273);

  shogun::Knobs mk;
  mk.maDecay = 55;
  shogun::Engine ma;
  arm(ma, mk, silentPattern());
  ma.trigger(shogun::Voice::Ma);
  ma.process(in, f);
  const double printed = -0.093967;
  expect(near(ma.maSample(), printed), t, "maracas y0", ma.maSample(), printed);
  expect(near(f.mainL, 0.5 * printed), t, "maracas main L", f.mainL, 0.5 * printed);
  expect(near(f.mainR, 0.5 * printed), t, "maracas main R", f.mainR, 0.5 * printed);
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
  expect(near(f.clL, 0.972835), t, "clave", f.clL, 0.972835);

  shogun::Knobs cb;
  cb.cbTune = 48;
  cb.cbDecay = 70;
  arm(e, cb, silentPattern());
  e.trigger(shogun::Voice::Cb);
  for (int n = 0; n <= 15; ++n) e.process(in, f);
  expect(near(f.cbR, 0.405275), t, "cowbell", f.cbR, 0.405275);

  shogun::Knobs cy;
  cy.cyTune = 64;
  cy.cyTone = 70;
  cy.cyDecay = 90;
  arm(e, cy, silentPattern());
  e.trigger(shogun::Voice::Cy);
  for (int n = 0; n <= 30; ++n) e.process(in, f);
  expect(near(e.cyStackA(), 0.187710), t, "cymbal A", e.cyStackA(), 0.187710);
  expect(near(e.cyStackB(), 0.382957), t, "cymbal B", e.cyStackB(), 0.382957);
  const double blend = (1.0 - 70.0 / 127.0) * e.cyStackA() + (70.0 / 127.0) * e.cyStackB();
  const double env = std::exp(-30.0 / (48000.0 * (0.05 + 1.8 * (90.0 / 127.0))));
  const double yNoNoise = env * blend;
  expect(near(yNoNoise, 0.295187), t, "cymbal without noise", yNoNoise, 0.295187);
  expect(std::fabs(f.cyR - yNoNoise) > 1e-6, t, "cymbal noise is in the bus", f.cyR, yNoNoise);

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
  expect(near(onTime, 0.208884), t, "shuffle fire", onTime, 0.208884);
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
  expect(near(heldY, 0.208884), t, "global shuffle override", heldY, 0.208884);
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
  expect(near(at, 0.208884), t, "shift of 1440", at, 0.208884);

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
  expect(near(f.bdL, 0.208884), t, "even step is not shuffled", f.bdL, 0.208884);

  shogun::Pattern cyc = silentPattern();
  cyc.track[static_cast<int>(shogun::Voice::Bd1)].length = 2;
  cyc.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].on = true;
  cyc.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].accent = 2;
  shogun::Engine cycE;
  arm(cycE, bd1Example(), cyc);
  cycE.setMode(shogun::ClockMode::Int);
  for (int i = 0; i <= 12010; ++i) cycE.process(in, f);
  expect(near(f.bdL, 0.208884), t, "track length cycles", f.bdL, 0.208884);

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
  expect(std::fabs(f.bdL - 0.832547) > 1e-4, t, "dist is a clip", f.bdL, 0.832547);

  shogun::Engine edge;
  arm(edge, bd1Example(), silentPattern());
  edge.setMode(shogun::ClockMode::Ext);
  in.volts[static_cast<int>(shogun::Voice::Bd1)] = 1.0;
  in.velocity[static_cast<int>(shogun::Voice::Bd1)] = -1;
  for (int n = 0; n <= 10; ++n) edge.process(in, f);
  expect(near(f.bdL, 0.208884), t, "edge at 1 V uses velocity 127", f.bdL, 0.208884);

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
  const double gL = 0.5 * (1.0 - -0.7);
  expect(near(f.toL, y * gL), t, "shared tom noise", f.toL, y * gL);
  expect(f.bdL == 0.0 && f.mainL != 0.0, t, "tom reaches main", f.mainL, y * gL);
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
  if (gFails != 0) {
    std::printf("%d failed\n", gFails);
    return 1;
  }
  std::printf("all named checks passed\n");
  return 0;
}
