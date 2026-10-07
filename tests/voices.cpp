#include "shogun.h"

#include <cmath>
#include <cstdio>
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
  expect(near(y48, 0.829514), t, "y(48)", y48, 0.829514);
  expect(near(y480, -0.855516), t, "y(480)", y480, -0.855516);

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
  expect(near(y0, 0.306649), t, "trigger 0 y", y0, 0.306649);
  expect(near(tr0, 160.0), t, "trigger 0 f_tr", tr0, 160.0);
  expect(near(y64, 0.810392), t, "trigger 64 y", y64, 0.810392);
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
  expect(near(t1, 0.668428), t, "t1", t1, 0.668428);
  expect(near(t2, 0.775136), t, "t2", t2, 0.775136);
  expect(near(y, 0.722202), t, "blend", y, 0.722202);

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

  const double wantVel = 0.208746 * 0.819291;
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
      expect(near(fa.bdL, 0.208746), t, "next on-step", fa.bdL, 0.208746);
      expect(near(fb.bdL, 0.208746), t, "next on-step pair", fb.bdL, 0.208746);
    }
  }
  expect(near(yA, wantVel), t, "ext velocity y", yA, wantVel);
  expect(near(yB, wantVel), t, "ext velocity y pair", yB, wantVel);
  expect(std::fabs(yA - 0.208746) > 1e-4, t, "not pattern accent", yA, 0.208746);

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
  expect(near(y10, 0.208746), t, "step 0 accent", y10, 0.208746);
  expect(matchPulse, t, "mid-step jack ignored", matchPulse ? 0.0 : 1.0, 0.0);
  expect(matchHeld, t, "plugged cable ignored", matchHeld ? 0.0 : 1.0, 0.0);
  expect(std::fabs(y10 - 0.208746 * 0.819291) > 1e-3, t, "not the jack velocity", y10, 0.208746);
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
  expect(near(pair, 0.829514), t, "patched pair", pair, 0.829514);
  expect(near(main, 0.829514), t, "patched main", main, 0.829514);

  shogun::Engine open;
  arm(open, bd1Example(), silentPattern());
  open.setPairPatched(shogun::Pair::Bd, false);
  open.trigger(shogun::Voice::Bd1);
  for (int n = 0; n <= 48; ++n) {
    open.process(in, f);
    if (n == 48) main = f.mainL;
  }
  expect(near(main, 0.829514), t, "unpatched main", main, 0.829514);

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
  expect(near(halfPair, 0.829514), t, "master leaves pair", halfPair, 0.829514);
  expect(near(halfMain, 0.414757), t, "master scales main", halfMain, 0.414757);

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
  expect(near(e.cyStackA(), 0.187710), t, "cymbal A", e.cyStackA(), 0.187710);
  expect(near(e.cyStackB(), 0.382957), t, "cymbal B", e.cyStackB(), 0.382957);
  const double blend = (1.0 - 70.0 / 127.0) * e.cyStackA() + (70.0 / 127.0) * e.cyStackB();
  const double env = std::exp(-30.0 / (48000.0 * shogun::decayTau(90.0 / 127.0)));
  const double yNoNoise = env * blend;
  expect(near(yNoNoise, 0.294377), t, "cymbal without noise", yNoNoise, 0.294377);
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
  expect(near(onTime, 0.208746), t, "shuffle fire", onTime, 0.208746);
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
  expect(near(heldY, 0.208746), t, "global shuffle override", heldY, 0.208746);
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
  expect(near(at, 0.208746), t, "shift of 1440", at, 0.208746);

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
  expect(near(f.bdL, 0.208746), t, "even step is not shuffled", f.bdL, 0.208746);

  shogun::Pattern cyc = silentPattern();
  cyc.track[static_cast<int>(shogun::Voice::Bd1)].length = 2;
  cyc.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].on = true;
  cyc.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].accent = 2;
  shogun::Engine cycE;
  arm(cycE, bd1Example(), cyc);
  cycE.setMode(shogun::ClockMode::Int);
  for (int i = 0; i <= 12010; ++i) cycE.process(in, f);
  expect(near(f.bdL, 0.208746), t, "track length cycles", f.bdL, 0.208746);

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
  expect(std::fabs(f.bdL - 0.829514) > 1e-4, t, "dist is a clip", f.bdL, 0.829514);

  shogun::Engine edge;
  arm(edge, bd1Example(), silentPattern());
  edge.setMode(shogun::ClockMode::Ext);
  in.volts[static_cast<int>(shogun::Voice::Bd1)] = 1.0;
  in.velocity[static_cast<int>(shogun::Voice::Bd1)] = -1;
  for (int n = 0; n <= 10; ++n) edge.process(in, f);
  expect(near(f.bdL, 0.208746), t, "edge at 1 V uses velocity 127", f.bdL, 0.208746);

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
  tied.track[static_cast<int>(shogun::Voice::Lead)].note[1].tie = true;
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

// Regressions for the bugs in BUGS.md that the engine fixes.
int bd1Restarts(shogun::Engine& e, long samples) {
  shogun::TrigIn in;
  shogun::Frame f;
  int fires = 0;
  double last = e.bd1Hz();
  for (long i = 0; i < samples; ++i) {
    e.process(in, f);
    const double hz = e.bd1Hz();
    if (hz > last + 1e-9) ++fires;
    last = hz;
  }
  return fires;
}

void testClockFixes() {
  const char* t = "testClockFixes";
  shogun::Pattern every = silentPattern();
  every.track[static_cast<int>(shogun::Voice::Bd1)].length = 1;
  every.track[static_cast<int>(shogun::Voice::Bd1)].drum[0].on = true;

  // BUGS.md E1: at a period that is not a whole number of samples every step still fires.
  const double tempos[] = {120.0, 130.0, 133.0, 97.0, 179.3};
  for (double bpm : tempos) {
    shogun::Engine e;
    arm(e, bd1Example(), every);
    e.setTempo(bpm);
    const int fires = bd1Restarts(e, static_cast<long>(e.periodSamples() * 64.0));
    expect(fires == 64, t, "every step fires at a fractional period", fires, 64);
  }

  // The step fires on the sample that holds its boundary: 130 BPM step 1 is at 5538.46.
  shogun::Engine at;
  arm(at, bd1Example(), every);
  at.setTempo(130);
  shogun::TrigIn in;
  shogun::Frame f;
  for (int i = 0; i <= 5538 + 10; ++i) at.process(in, f);
  expect(near(f.bdL, 0.208746), t, "step 1 at floor(5538.46)", f.bdL, 0.208746);

  // E2: a tempo change while running moves the next step by the new period, no burst and no stall.
  shogun::Engine up;
  arm(up, bd1Example(), silentPattern());
  for (int i = 0; i < 480000 + 3000; ++i) up.process(in, f);
  const std::int64_t c0 = up.counter();
  up.setTempo(180);
  long wait = 0;
  while (up.counter() == c0) {
    up.process(in, f);
    ++wait;
  }
  expect(wait == 2000, t, "120 to 180 halfway through a step", static_cast<double>(wait), 2000);
  for (int i = 0; i < 100; ++i) up.process(in, f);
  expect(up.counter() == c0 + 1, t, "no burst after the change", static_cast<double>(up.counter()), static_cast<double>(c0 + 1));
  up.setTempo(60);
  const std::int64_t c1 = up.counter();
  wait = 0;
  while (up.counter() == c1 && wait < 100000) {
    up.process(in, f);
    ++wait;
  }
  expect(wait < 12000, t, "180 to 60 does not stall", static_cast<double>(wait), 11800);

  // Transport: stop holds the count, start plays step 1 at once.
  shogun::Engine tr;
  arm(tr, bd1Example(), every);
  for (int i = 0; i < 7000; ++i) tr.process(in, f);
  tr.setRunning(false);
  expect(bd1Restarts(tr, 30000) == 0, t, "stopped plays nothing", 0, 0);
  expect(tr.counter() == 1, t, "stopped holds the counter", static_cast<double>(tr.counter()), 1);
  tr.setRunning(true);
  for (int i = 0; i <= 10; ++i) tr.process(in, f);
  expect(tr.counter() == 0 && near(f.bdL, 0.208746), t, "start plays step 1 now", f.bdL, 0.208746);

  // External clock: pulses move the counter, the period does not.
  shogun::Engine ext;
  arm(ext, bd1Example(), every);
  ext.setExternalClock(true);
  for (int i = 0; i < 20000; ++i) ext.process(in, f);
  expect(ext.counter() == 0, t, "external clock waits for a pulse", static_cast<double>(ext.counter()), 0);
  ext.clockPulse();
  for (int i = 0; i <= 10; ++i) ext.process(in, f);
  expect(ext.counter() == 1 && near(f.bdL, 0.208746), t, "a pulse is one step", f.bdL, 0.208746);
}

void testVoiceFixes() {
  const char* t = "testVoiceFixes";
  shogun::TrigIn in;
  shogun::Frame f;

  // E3: the conga partial decays with the body.
  shogun::Knobs conga;
  conga.ltcMode = 127;
  conga.ltcDecay = 0;
  shogun::Engine c;
  arm(c, conga, silentPattern());
  c.trigger(shogun::Voice::Ltc);
  double late = 0;
  for (int i = 0; i < 48000; ++i) {
    c.process(in, f);
    if (i > 24000) late = std::fmax(late, std::fabs(f.mainL));
  }
  expect(late < 1e-6, t, "conga is silent half a second later", late, 0);

  // E4: muting a note track, stopping, or switching to EXT releases the held note.
  shogun::Pattern held = silentPattern();
  for (int s = 0; s < 4; ++s) {
    held.track[static_cast<int>(shogun::Voice::Lead)].note[s].note = 60;
    held.track[static_cast<int>(shogun::Voice::Lead)].note[s].tie = s > 0;
  }
  for (int way = 0; way < 3; ++way) {
    shogun::Engine e;
    arm(e, shogun::Knobs{}, held);
    for (int i = 0; i < 3000; ++i) e.process(in, f);
    if (way == 0) {
      shogun::Pattern muted = held;
      muted.track[static_cast<int>(shogun::Voice::Lead)].mute = true;
      e.setPattern(muted);
    } else if (way == 1) {
      e.setRunning(false);
    } else {
      e.setMode(shogun::ClockMode::Ext);
    }
    double peak = 0;
    for (int i = 0; i < 96000; ++i) {
      e.process(in, f);
      if (i > 48000) peak = std::fmax(peak, std::fabs(f.mainL));
    }
    const char* what[] = {"mute releases the held note", "stop releases the held note", "EXT releases the held note"};
    expect(peak < 1e-6, t, what[way], peak, 0);
  }

  // E5: the same note twice without a tie plays twice.
  shogun::Pattern again = silentPattern();
  again.track[static_cast<int>(shogun::Voice::Lead)].note[0].note = 60;
  again.track[static_cast<int>(shogun::Voice::Lead)].note[1].note = 60;
  shogun::Engine r;
  arm(r, shogun::Knobs{}, again);
  for (int i = 0; i <= 6000; ++i) r.process(in, f);
  expect(r.leadSaw() == 0.0, t, "repeated note restarts", r.leadSaw(), 0.0);
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
  expect(near(f.bdL, 0.208746), t, "ended voice retriggers", f.bdL, 0.208746);
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
  expect(near(at48(0), 0.829514), t, "Dist 0 is y = pre", at48(0), 0.829514);
  expect(std::fabs(at48(1) - at48(0)) < 2e-3, t, "Dist 1 is next to the bypass", at48(1), at48(0));
  expect(near(at48(64), 0.999151), t, "Dist 64 printed row", at48(64), 0.999151);
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
  cp.process(in, f);
  // Attack 0 leaves only the tail: the first noise draw through the one-pole at fc 1,565.8 Hz, times 0.707107.
  const double a = std::exp(-2.0 * 3.141592653589793 * (400.0 * std::pow(15.0, 64.0 / 127.0)) / 48000.0);
  const double want = (1.0 - a) * -0.527088949456811 * 0.707107;
  expect(near(f.cpL, want, 1e-6) && near(f.cpR, want, 1e-6), t, "clap tail at 0.707", f.cpL, want);
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
    expect(std::fabs(static_cast<double>(n - want)) <= 1.0, t, c.name, static_cast<double>(n), static_cast<double>(want));
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
  testClockFixes();
  testVoiceFixes();
  testVoiceEndsWhenQuiet();
  testDistBypassAndDrive();
  testCenterPanIsNotHalf();
  testShuffleSurvivesOddLength();
  testDecayNoonIsShort();
  testBd2FullHolds();
  testTomFullEnds();
  testSnareBendAtPitchZero();
  if (gFails != 0) {
    std::printf("%d failed\n", gFails);
    return 1;
  }
  std::printf("all named checks passed\n");
  return 0;
}
