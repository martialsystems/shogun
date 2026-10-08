// SHOGUN engine named tests: sequencer and clock switch, mixer rules, latency, pitch law, jacks (TESTPLAN.md, §15.4).
#include <cstring>
#include <jidai/CableStandard.h>
#include <string>
#include <vector>

#include "rig.h"

using namespace shogun;
using namespace rig;
using tu::atLeast;
using tu::atMost;
using tu::near;
using tu::truth;

namespace {

constexpr double kStep = 6000.0;  // 1/16 at 120 BPM, 48 kHz

void sharedPattern(Engine& e) {  // TESTPLAN shared pattern: BD1 only, length 4, step 0 on
  bd1Example(e);
  e.setParamNow(P_CLOCK_TEMPO, 0.5);
  e.setParamNow(P_CLOCK_SCALE, stepU(1, 4));
  Track& t = e.pattern().tracks[BD1];
  t.len = 4;
  t.steps[0].on = true;
  t.steps[0].acc = 3;
}
double bd1Ref10() {  // BD1 example y(10) at g_vel 1
  auto r = make();
  bd1Example(*r);
  r->trigger(BD1);
  run(*r, 11);
  return r->voiceOut(BD1);
}
// Hit detector: BD1's pitch envelope jumps back up at a trigger.
struct HitLog {
  double prevP = 0.0;
  std::vector<long> hits;
  void step(Engine& e, long n) {
    const double p = e.bd1().p;
    if (p > prevP + 1e-12) hits.push_back(n);
    prevP = p;
  }
};

void testExtBypassIgnoresPattern() {
  const char* T = "testExtBypassIgnoresPattern";
  auto e = make();
  sharedPattern(*e);
  e->pattern().tracks[BD1].steps[0].flam = 1;
  e->setParamNow(P_CLOCK_MODE, stepU(1, 2));  // EXT
  Graph g;
  const int tp = drumPort(BD1, DJ_TRIG), vp = drumPort(BD1, DJ_VEL);
  g.con[tp] = g.con[vp] = true;
  g.vals[vp] = static_cast<float>(5.0 * 100.0 / 127.0);
  e->setRunning(true);
  const int disp0 = (g.step(*e), e->displayStep());
  bool silent = !e->voiceActive(BD1);
  for (int n = 1; n < 6000; ++n) {
    g.step(*e);
    silent = silent && !e->voiceActive(BD1);
  }
  g.step(*e);  // n = 6000: the next step boundary
  const int disp1 = e->displayStep();
  g.vals[tp] = 5.0f;  // TRIG rising edge at n = 6001
  double y10 = 0.0;
  for (int n = 0; n <= 10; ++n) {
    g.step(*e);
    if (n == 0) g.vals[tp] = 5.0f;
    y10 = e->voiceOut(BD1);
  }
  g.vals[tp] = 0.0f;
  const double ref = bd1Ref10();
  std::printf("%s: BD1 silent over step 0 in EXT: %d; display %d -> %d; TRIG vel 100: g_vel %.6f, y(10) %.6f = %.6f x %.6f\n",
              T, silent ? 1 : 0, disp0, disp1, e->voiceGain(BD1), y10, ref, y10 / ref);
  truth(T, "EXT ignores the pattern", silent);
  truth(T, "counter advances", disp1 != disp0);
  near(T, "g_vel(100)", e->voiceGain(BD1), 0.819291, 1e-6);
  near(T, "y(10) = ref x g_vel", y10, ref * (0.15 + 0.85 * 100.0 / 127.0), 1e-9);
  // No flam hits from the pattern step: exactly one hit in this step.
  HitLog h;
  h.prevP = e->bd1().p;
  for (long n = 0; n < 5000; ++n) {
    g.step(*e);
    h.step(*e, n);
  }
  truth(T, "no flam hits in EXT", h.hits.empty());
  // Switch to INT without resetting the counter: the next on-step (track step 0) fires; TRIG no longer does.
  e->setParamNow(P_CLOCK_MODE, stepU(0, 2));
  HitLog h2;
  h2.prevP = e->bd1().p;
  long n0 = -1;
  for (long n = 0; n < 24000; ++n) {
    if (n == 3000) g.vals[tp] = 5.0f;
    if (n == 3100) g.vals[tp] = 0.0f;
    g.step(*e);
    h2.step(*e, n);
    if (n0 < 0 && !h2.hits.empty()) n0 = n;
  }
  std::printf("%s: after INT: %zu pattern hits (flam pairs) in the next bar, first at +%ld; TRIG edge ignored\n", T,
              h2.hits.size(), n0);
  truth(T, "INT plays the pattern", !h2.hits.empty());
  bool trigFired = false;
  for (long x : h2.hits) trigFired = trigFired || (x >= 3000 && x < 3200);
  truth(T, "INT ignores TRIG", !trigFired);
}

void testIntIgnoresTrigJacks() {
  const char* T = "testIntIgnoresTrigJacks";
  auto e = make();
  sharedPattern(*e);
  e->pattern().tracks[BD1].steps[0].on = false;
  Graph g;
  const int tp = drumPort(BD1, DJ_TRIG);
  g.con[tp] = true;
  e->setRunning(true);
  bool silent = true;
  for (long n = 0; n < 12000; ++n) {
    g.vals[tp] = (n >= 9000 && n < 9100) ? 5.0f : 0.0f;
    g.step(*e);
    silent = silent && !e->voiceActive(BD1) && e->voiceOut(BD1) == 0.0;
  }
  auto f = make();
  sharedPattern(*f);
  Graph g2;
  g2.con[tp] = true;  // cable plugged, no edge
  f->setRunning(true);
  for (int n = 0; n <= 10; ++n) g2.step(*f);
  const double ref = bd1Ref10();
  std::printf("%s: TRIG edge in step 1 (INT) silent: %d; step 0 y(10) %.6f = ref %.6f x %.6f\n", T, silent ? 1 : 0,
              f->voiceOut(BD1), ref, f->voiceOut(BD1) / ref);
  truth(T, "INT ignores TRIG", silent);
  near(T, "step 0 fires at accent 3 (g 1)", f->voiceOut(BD1), ref, 1e-12);
}

void testRestIsSilent() {
  const char* T = "testRestIsSilent";
  for (int v : {BD1, LEAD, BASS}) {
    auto e = make();
    e->setParamNow(P_CLOCK_TEMPO, 0.5);
    Track& t = e->pattern().tracks[v];
    t.steps[0].on = false;  // drum off step / note rest
    t.steps[1].on = true;
    e->setRunning(true);
    bool z = true;
    for (int n = 0; n < 6000; ++n) {
      e->processSample();
      z = z && e->mainL() == 0.0 && e->mainR() == 0.0 && e->voiceOut(v) == 0.0;
    }
    std::printf("%s: %s rest step outputs 0: %d\n", T, kVoiceNames[v], z ? 1 : 0);
    truth(T, kVoiceNames[v], z);
  }
}

void testSoloMutesOtherVoices() {
  const char* T = "testSoloMutesOtherVoices";
  auto setup = [](Engine& e, bool bd, bool solo) {
    bd1Example(e);
    e.pattern().tracks[BD1].steps[0].on = bd;
    e.pattern().tracks[SD].steps[0].on = true;
    e.setParamNow(P_SD_SOLO, solo ? 1.0 : 0.0);
    e.setRunning(true);
  };
  auto a = make(), b = make(), c = make();
  setup(*a, true, true);   // BD1 + SD, SD soloed
  setup(*b, false, false); // SD alone
  setup(*c, true, false);  // BD1 + SD, no solo
  double maxDiff = 0.0;
  for (int n = 0; n < 200 + 23; ++n) {
    a->processSample();
    b->processSample();
    c->processSample();
    maxDiff = std::fmax(maxDiff, std::fabs(a->mainL() - b->mainL()) + std::fabs(a->mainR() - b->mainR()));
  }
  const bool counterSame = a->counter() == b->counter();
  a->setParamNow(P_SD_SOLO, 0.0);
  for (int n = 0; n < 200; ++n) {
    a->processSample();
    c->processSample();
  }
  const double after = std::fabs(a->mainL() - c->mainL());
  std::printf("%s: soloed SD main vs SD alone max diff %.3g; counter same %d; solo off main vs no-solo diff %.3g\n", T,
              maxDiff, counterSame ? 1 : 0, after);
  near(T, "main is the snare alone", maxDiff, 0.0, 1e-12);
  truth(T, "counter same", counterSame);
  near(T, "solo off matches", after, 0.0, 1e-12);
  auto m = make();
  m->setParamNow(P_SD_SOLO, 1.0);
  m->setParamNow(P_SD_MUTE, 1.0);
  m->trigger(SD);
  double pk = 0.0;
  for (int n = 0; n < 4800; ++n) {
    m->processSample();
    pk = std::fmax(pk, std::fabs(m->mainL()) + std::fabs(m->mainR()));
  }
  std::printf("%s: soloed + muted SD direct trigger main peak %.3g\n", T, pk);
  near(T, "muted solo is silent", pk, 0.0, 0.0);
}

void testShuffleSurvivesOddLength() {
  const char* T = "testShuffleSurvivesOddLength";
  auto e = make();
  bd1Example(*e);
  e->setParamNow(P_CLOCK_SWING, 2.0 / 3.0);  // sw = 0.5 + 0.25u = 66.7 % (old shuffle 15)
  Track& t = e->pattern().tracks[BD1];
  t.len = 3;
  for (int i = 0; i < 3; ++i) t.steps[i].on = true;
  e->setRunning(true);
  HitLog h;
  for (long n = 0; n < 33000; ++n) {
    e->processSample();
    h.step(*e, n);
  }
  std::printf("%s: delay %.0f samples; hits:", T, (2.0 * (0.5 + 0.25 * 2.0 / 3.0) - 1.0) * kStep);
  for (long x : h.hits) std::printf(" %ld", x);
  std::printf("\n");
  const long want[6] = {0, 8000, 12000, 20000, 24000, 32000};
  truth(T, "six hits", h.hits.size() >= 6);
  for (int i = 0; i < 6 && i < static_cast<int>(h.hits.size()); ++i)
    near(T, "hit sample", static_cast<double>(h.hits[i]), static_cast<double>(want[i]), 0.0);
}

void testInitKitAndEmptyPattern() {
  const char* T = "testInitKitAndEmptyPattern";
  auto e = std::make_unique<Engine>();  // fresh instance, no reset()
  const Pattern& p = e->pattern();
  bool empty = true, len16 = true;
  for (const Track& t : p.tracks) {
    len16 = len16 && t.len == 16;
    for (const Step& s : t.steps) empty = empty && !s.on;
  }
  bool defaults = true;
  for (int i = 0; i < kParamCount; ++i) defaults = defaults && e->param(i) == kParams[i].def;
  e->setRunning(true);
  bool silent = true;
  for (int n = 0; n < 96000; ++n) {
    e->processSample();
    silent = silent && e->mainL() == 0.0 && e->mainR() == 0.0;
  }
  std::printf("%s: pattern '%s', every track 16 long: %d, empty: %d, params at INIT: %d, one bar silent: %d\n", T,
              p.name, len16 ? 1 : 0, empty ? 1 : 0, defaults ? 1 : 0, silent ? 1 : 0);
  truth(T, "name 001 INIT", std::strcmp(p.name, "001 INIT") == 0);
  truth(T, "lengths 16", len16);
  truth(T, "no step on", empty);
  truth(T, "INIT params", defaults);
  truth(T, "one bar of INIT is silence", silent);
  // A test beat (not factory content) on the INIT kit.
  auto b = std::make_unique<Engine>();
  Pattern& q = b->pattern();
  for (int s = 0; s < 16; ++s) {
    if (s == 0 || s == 8) q.tracks[BD1].steps[s].on = true;
    if (s == 4 || s == 12) q.tracks[SD].steps[s].on = q.tracks[CP].steps[s].on = true;
    if (s % 4 == 2) q.tracks[OH].steps[s].on = true;
    else q.tracks[CH].steps[s].on = true;
  }
  b->setRunning(true);
  double pk = 0.0, ohMax = 0.0;
  for (int n = 0; n < 96000; ++n) {
    b->processSample();
    pk = std::fmax(pk, std::fmax(std::fabs(b->mainL()), std::fabs(b->mainR())));
    if (n >= 12000 && n < 18000) ohMax = std::fmax(ohMax, b->oh().env());
  }
  std::printf("%s: test beat main peak %.6f (%.2f dBFS), OH on step 3 env max %.3f\n", T, pk, 20.0 * std::log10(pk), ohMax);
  atMost(T, "main peak under 1", pk, 1.0);
  atLeast(T, "open hat rings", ohMax, 0.5);
}

// Lag (base samples) that best aligns x with the reference r (cross-correlation peak, lags 0..60).
int bestLag(const std::vector<double>& r, const std::vector<double>& x) {
  int best = 0;
  double bv = -1e300;
  for (int L = 0; L <= 60; ++L) {
    double s = 0.0;
    for (size_t n = 0; n + L < x.size(); ++n) s += r[n] * x[n + L];
    if (s > bv) {
      bv = s;
      best = L;
    }
  }
  return best;
}

void testControlOutsZeroLatency() {
  const char* T = "testControlOutsZeroLatency";
  auto render = [](int os, std::vector<double>& out, std::vector<double>& env, std::vector<double>& gate) {
    auto e = make(48000.0, os);
    e->pattern().tracks[BD1].steps[0].on = true;
    e->pattern().tracks[LEAD].steps[0].on = true;
    Graph g;
    g.con[drumPort(BD1, DJ_OUT)] = true;
    e->setRunning(true);
    for (int n = 0; n < 3000; ++n) {
      g.step(*e);
      out.push_back(g.vals[drumPort(BD1, DJ_OUT)]);
      env.push_back(g.vals[drumPort(BD1, DJ_ENV)]);
      gate.push_back(g.vals[PORT_LD_GATE]);
    }
  };
  std::vector<double> o1, e1, g1, o2, e2, g2;
  render(1, o1, e1, g1);
  render(2, o2, e2, g2);
  const int lag = bestLag(o1, o2);
  std::printf("%s: step at n = 0: 2x BD1:ENV %.4f V, MOD:LD GATE %.1f V at n = 0; BD1:OUT lag %d samples\n", T, e2[0],
              g2[0], lag);
  atLeast(T, "ENV at n", e2[0], 1.0);
  near(T, "LD GATE at n", g2[0], 5.0, 0.0);
  near(T, "ENV same at 1x and 2x", e2[0], e1[0], 1e-6);
  near(T, "OUT onset n + 23", lag, 23, 0);
}

void testOsLatency() {
  const char* T = "testOsLatency";
  int lat[3];
  const int os[3] = {1, 2, 4};
  for (int i = 0; i < 3; ++i) lat[i] = make(48000.0, os[i])->latencySamples();
  std::printf("%s: latencySamples 1x/2x/4x = %d / %d / %d\n", T, lat[0], lat[1], lat[2]);
  truth(T, "0/23/26", lat[0] == 0 && lat[1] == 23 && lat[2] == 26);
  auto render = [](int osf, bool drives, std::vector<double>& mainL, std::vector<double>& out, std::vector<double>& aux) {
    auto e = make(48000.0, osf);
    e->pattern().tracks[BD1].steps[0].on = true;
    e->pattern().tracks[BD2].steps[0].on = true;  // deterministic voices only (noise draws differ per rate)
    e->setParamNow(P_BD2_OUTPUT, stepU(5, 14));  // BD2 → AUX 1/2
    if (drives) {
      e->setParamNow(P_BD1_DRIVE, 0.5);
      e->setParamNow(P_BD1_OUTPUT, stepU(1, 14));  // BD1 → BUS A
      e->setParamNow(P_BUS_A_DRIVE, 0.5);
      e->setParamNow(P_MASTER_DRIVE, 0.5);
    }
    Graph g;
    g.con[drumPort(BD1, DJ_OUT)] = true;
    e->setRunning(true);
    for (int n = 0; n < 4000; ++n) {
      g.step(*e);
      mainL.push_back(e->mainL());
      out.push_back(g.vals[drumPort(BD1, DJ_OUT)]);
      aux.push_back(e->aux()[0]);
    }
  };
  for (int osf : {2, 4}) {
    std::vector<double> m1, o1, a1, m2, o2, a2, m3, o3, a3, m4, o4, a4;
    render(1, false, m1, o1, a1);
    render(osf, false, m2, o2, a2);
    render(1, true, m3, o3, a3);
    render(osf, true, m4, o4, a4);
    const int lm = bestLag(m1, m2), lo = bestLag(o1, o2), la = bestLag(a1, a2), ld = bestLag(m3, m4);
    std::printf("%s: %dx lags main %d, BD1:OUT %d, AUX 1 %d; voice + bus + master drive main %d\n", T, osf, lm, lo, la, ld);
    const int want = osf == 2 ? 23 : 26;
    truth(T, "all outputs aligned at the latency", lm == want && lo == want && la == want);
    // ADAA stages each add half a sample of their own rate, so the 1× reference sits ~0.75 sample later than the
    // oversampled render: the measured lag may read one under. Stacked decimators would read 46+.
    near(T, "no stacking with drives", ld, want, 1);
  }
}

double synthNoteOut(Engine& e, int v) {
  Graph g;
  g.step(e);
  return g.vals[synthPort(v - LEAD, SJ_NOTE_OUT)];
}

void testPitchVoct() {
  const char* T = "testPitchVoct";
  const int notes[6] = {36, 48, 60, 72, 24, 108};
  const double want[6] = {-1.0, 0.0, 1.0, 2.0, -2.0, 5.0};
  for (int i = 0; i < 6; ++i) {
    auto e = make();
    e->noteOn(LEAD, notes[i]);
    const double v = synthNoteOut(*e, LEAD);
    std::printf("%s: NOTE OUT note %d = %+.4f V, flag %d\n", T, notes[i], v, e->overRange(LEAD) ? 1 : 0);
    near(T, "NOTE OUT", v, want[i], 1e-4);
    truth(T, "flag clear", !e->overRange(LEAD));
  }
  // NOTE IN (EXT trigger mode so GATE plays): −1/0/+1/+2 V play 36/48/60/72 at A4 440, TUNE 0, OCT 0.
  for (double volts : {-1.0, 0.0, 1.0, 2.0}) {
    auto e = make();
    e->setParamNow(P_CLOCK_MODE, stepU(1, 2));
    Graph g;
    const int np = synthPort(0, SJ_NOTE), gp = synthPort(0, SJ_GATE);
    g.con[np] = g.con[gp] = true;
    g.vals[np] = static_cast<float>(volts);
    g.vals[gp] = 5.0f;
    g.step(*e);
    g.step(*e);
    const SynthVoice& s = e->synth(LEAD);
    const double note = 48.0 + 12.0 * volts;
    const double cents = 1200.0 * std::log2(s.f / (440.0 * std::exp2((note - 69.0) / 12.0)));
    std::printf("%s: NOTE IN %+.0f V -> note %.4f, pitch error %.5f cents\n", T, volts, s.noteTarget, cents);
    near(T, "NOTE IN note", s.noteTarget, note, 1e-9);
    near(T, "NOTE IN pitch", cents, 0.0, 0.01);
    if (volts == 0.0) {
      const double f0 = s.f;
      g.con[synthPort(0, SJ_VOCT)] = true;
      g.vals[synthPort(0, SJ_VOCT)] = 1.0f;
      g.step(*e);
      const double st = 12.0 * std::log2(e->synth(LEAD).f / f0);
      std::printf("%s: V/OCT +1 V adds %.6f semitones\n", T, st);
      near(T, "V/OCT +1 V = 12 st", st * 100.0, 1200.0, 0.01);
    }
  }
}

void testPitchRail() {
  const char* T = "testPitchRail";
  auto e = make();
  e->setParamNow(P_LEAD_OCT, stepU(2, 3));  // OCT +1
  e->noteOn(LEAD, 97);
  const double v1 = synthNoteOut(*e, LEAD);
  const bool f1 = e->overRange(LEAD);
  auto e2 = make();
  e2->noteOn(LEAD, -13);
  const double v2 = synthNoteOut(*e2, LEAD);
  const bool f2 = e2->overRange(LEAD);
  double maxAbs = 0.0;
  for (int n = -40; n <= 150; ++n) {
    auto s = make();
    s->noteOn(BASS, n);
    maxAbs = std::fmax(maxAbs, std::fabs(synthNoteOut(*s, BASS)));
  }
  std::printf("%s: note 109 (OCT +1 on 97) %+.4f V flag %d; note -13 %+.4f V flag %d; max |NOTE OUT| over -40..150 %.4f\n",
              T, v1, f1 ? 1 : 0, v2, f2 ? 1 : 0, maxAbs);
  near(T, "109 -> +5", v1, 5.0, 0.0);
  truth(T, "109 flag", f1);
  near(T, "-13 -> -5", v2, -5.0, 0.0);
  truth(T, "-13 flag", f2);
  atMost(T, "never beyond the rail", maxAbs, 5.0);
}

void testTuningIsNotTheVoltLaw() {
  const char* T = "testTuningIsNotTheVoltLaw";
  auto e = make();
  e->setParamNow(P_LEAD_TUNE, 1.0);                     // +100 cents
  e->setParamNow(P_GLOBAL_A4, (442.0 - 415.0) / 51.0);  // A4 = 442 Hz
  e->noteOn(LEAD, 60);
  const double v = synthNoteOut(*e, LEAD);
  const double f = e->synth(LEAD).f;
  const double fWant = 442.0 * std::exp2((60.0 - 69.0) / 12.0 + 100.0 / 1200.0);
  std::printf("%s: TUNE +100 c, A4 442: NOTE OUT %+.4f V, audible f %.4f Hz (law %.4f, untuned %.4f)\n", T, v, f, fWant,
              440.0 * std::exp2(-9.0 / 12.0));
  near(T, "NOTE OUT stays +1 V", v, 1.0, 0.0);
  near(T, "audible pitch moves", f, fWant, 1e-6);
}

void testLin55Migration() {
  const char* T = "testLin55Migration";
  int ports[3];
  int law = -1;
  const int n = resolvePort("BASS:HZ/V", ports, &law);
  std::printf("%s: BASS:HZ/V -> %s, law %d\n", T, n == 1 ? kPortTable[ports[0]].id : "?", law);
  truth(T, "loads as BASS:NOTE lin55", n == 1 && std::strcmp(kPortTable[ports[0]].id, "BASS:NOTE") == 0 && law == 1);
  auto e = make();
  e->setParamNow(P_CLOCK_MODE, stepU(1, 2));
  e->setInputLaw(ports[0], law);
  Graph g;
  const int gp = synthPort(1, SJ_GATE);
  g.con[ports[0]] = g.con[gp] = true;
  g.vals[ports[0]] = 2.0f;
  g.vals[gp] = 5.0f;
  g.step(*e);
  g.step(*e);
  const double f = e->synth(BASS).f;
  std::printf("%s: 2.0 V plays %.6f Hz (%.5f cents from 110)\n", T, f, 1200.0 * std::log2(f / 110.0));
  near(T, "110 Hz", 1200.0 * std::log2(f / 110.0), 0.0, 0.01);
  int tp[3];
  int law2 = -1;
  const int n2 = resolvePort("TOM:PITCH", tp, &law2);
  truth(T, "TOM:PITCH fans out to 3 toms with AMT 1/12 law", n2 == 3 && law2 == 2);
}

// JCS R6 / R14 through the shared jidai-common header. Every SHOGUN port id and every alias-table id goes through
// jidai::jcs::parseJackId whole, in the bare, first-instance (SHOGUN/) and global (SHOGUN#1/) forms. Labels keep
// '/', spaces and digits. resolvePort maps each form to the same port. R14 role colours are the shared table's.
void testJackIdsJcsShared() {
  const char* T = "testJackIdsJcsShared";
  using jidai::jcs::JackForm;
  using jidai::jcs::parseJackId;
  // One id through all three forms: parsed whole (section + label give the id back) and resolved to `want`.
  auto roundTrip = [](const std::string& id, int want) {
    const auto b = parseJackId(id);
    const auto f = parseJackId("SHOGUN/" + id);
    const auto g = parseJackId("SHOGUN#1/" + id);
    bool ok = b && b->form == JackForm::Bare && b->local() == id && b->text() == id;
    ok = ok && f && f->form == JackForm::FirstInstance && f->prefix == "SHOGUN" && f->local() == id &&
         f->text() == "SHOGUN/" + id;
    ok = ok && g && g->form == JackForm::Global && g->prefix == "SHOGUN" && g->number == 1 && g->local() == id &&
         g->global() == "SHOGUN#1/" + id;
    if (want >= 0) {
      for (const std::string& t : {id, "SHOGUN/" + id, "SHOGUN#1/" + id, "SHOGUN#12/" + id}) {
        int out[3];
        ok = ok && resolvePort(t.c_str(), out, nullptr) >= 1 && out[0] == want;
      }
    }
    return ok;
  };
  int portsOk = 0, slash = 0, spaced = 0, digit = 0;
  std::string failed;
  for (int i = 0; i < kPorts; ++i) {
    const std::string id = kPortTable[i].id;
    const std::string lab = id.substr(id.find(':') + 1);
    slash += lab.find('/') != std::string::npos ? 1 : 0;
    spaced += lab.find(' ') != std::string::npos ? 1 : 0;
    digit += id.find_first_of("0123456789") != std::string::npos ? 1 : 0;
    if (roundTrip(id, i)) ++portsOk;
    else failed += " [" + id + "]";
  }
  int aliasIds = 0, aliasOk = 0, legacyOk = 0;
  std::string legacy;
  // Every alias id: the shared-table renames, the fan-out shim and the legacy-name shim.
  struct AliasRow {
    const char* from;
    const char* to[3];
    int law;
  };
  std::vector<AliasRow> rows;
  for (const PortRename& r : kPortRenames)
    rows.push_back({r.from, {r.to, nullptr, nullptr}, r.law == jidai::jcs::AliasLaw::Lin55ToVoct ? 1 : 0});
  for (const PortFanOut& f : kPortFanOuts) rows.push_back({f.from, {f.to[0], f.to[1], f.to[2]}, f.law});
  for (const PortLegacyName& l : kPortLegacyNames) rows.push_back({l.from, {l.to, nullptr, nullptr}, 0});
  for (const AliasRow& a : rows) {
    int out[3], law = -1;
    const int want = resolvePort(a.from, out, &law) > 0 ? out[0] : -2;
    ++aliasIds;
    if (parseJackId(a.from)) {
      aliasOk += (roundTrip(a.from, want) && law == a.law) ? 1 : 0;
    } else {
      legacy += " [" + std::string(a.from) + "]";  // v2.0/2.1 names with no SECTION: (not R6 ids): matched whole
      legacyOk += (want >= 0 && law == a.law) ? 1 : 0;
    }
    for (int k = 0; k < 3 && a.to[k]; ++k) {
      ++aliasIds;
      aliasOk += roundTrip(a.to[k], findPort(a.to[k])) ? 1 : 0;
    }
  }
  // The shared AliasTable holds every one-to-one rename with its law, and refuses exactly what the shim keeps.
  const jidai::jcs::AliasTable& tab = portAliasTable();
  const auto hz = tab.resolve("BASS:HZ/V");
  const bool sharedOk = tab.size() == sizeof kPortRenames / sizeof kPortRenames[0] && hz.aliased &&
                        hz.id == "BASS:NOTE" && hz.conversion.law == jidai::jcs::AliasLaw::Lin55ToVoct &&
                        hz.conversion.convert(2.0) == -0.25 && tab.resolve("SD:SNAPPY").id == "SD:TONE";
  jidai::jcs::AliasTable probe;
  const bool fanRefused = probe.add("HAT:DECAY", "CH:DECAY") && !probe.add("HAT:DECAY", "OH:DECAY");
  const bool legacyRefused = !probe.add("MIX L", "MIX:L") && !probe.add("LFO OUT", "MOD:LFO 1");
  std::printf("%s: shared AliasTable %zu renames (BASS:HZ/V -> %s, Lin55ToVoct(2.0 V) = %.17g V); shim: fan-out %zu "
              "(shared add of a 2nd target refused %d), legacy names %zu (shared add refused %d)\n",
              T, tab.size(), hz.id.c_str(), hz.conversion.convert(2.0), sizeof kPortFanOuts / sizeof kPortFanOuts[0],
              fanRefused ? 1 : 0, sizeof kPortLegacyNames / sizeof kPortLegacyNames[0], legacyRefused ? 1 : 0);
  truth(T, "one-to-one renames live in the shared AliasTable with their laws", sharedOk);
  truth(T, "shim only for what the shared table refuses", fanRefused && legacyRefused);
  int foreign[3];
  const bool refused = resolvePort("RONIN#1/BD1:TRIG", foreign, nullptr) == 0;
  int paramsOk = 0;
  for (int k = 0; k < kParamCount; ++k) {
    const auto pj = parseJackId(kParams[k].id);
    paramsOk += (pj && pj->local() == kParams[k].id) ? 1 : 0;
  }
  const std::uint32_t vel = jidai::jcs::roleInfo(kPortTable[drumPort(0, DJ_PITCH)].role).rgb;
  std::printf("%s: ports %d/%d round trip whole in 3 forms (labels with '/' %d, with spaces %d, ids with digits %d)%s\n", T,
              portsOk, kPorts, slash, spaced, digit, failed.empty() ? "" : (" FAILED:" + failed).c_str());
  std::printf("%s: alias-table ids %d, R6 ids round trip %d, legacy non-R6 names matched whole %d:%s; foreign prefix "
              "refused %d; params %d/%d valid R6 ids; PITCH colour #%06x\n",
              T, aliasIds, aliasOk, legacyOk, legacy.c_str(), refused ? 1 : 0, paramsOk, kParamCount,
              static_cast<unsigned>(vel));
  truth(T, "153 port ids round trip whole", portsOk == kPorts);
  truth(T, "alias R6 ids round trip whole", aliasOk + legacyOk == aliasIds);
  truth(T, "legacy names are exactly MIX L, MIX R, LFO OUT", legacy == " [MIX L] [MIX R] [LFO OUT]" && legacyOk == 3);
  truth(T, "foreign prefix refused", refused);
  truth(T, "param ids are R6 SECTION:LABEL", paramsOk == kParamCount);
  truth(T, "V/OCT role colour #6590f3", vel == 0x6590f3u);
}

void testJackIdsAndTypes() {
  const char* T = "testJackIdsAndTypes";
  std::vector<std::string> ids;
  const char* drums[14] = {"BD1", "BD2", "SD", "RS", "CP", "CL", "MA", "CB", "CH", "OH", "CY", "LTC", "MTC", "HTC"};
  for (const char* v : drums)
    for (const char* j : {"TRIG", "VEL", "PITCH", "DECAY", "TONE", "RET", "OUT", "ENV"}) ids.push_back(std::string(v) + ":" + j);
  for (const char* v : {"LEAD", "BASS"})
    for (const char* j : {"GATE", "VEL", "NOTE", "V/OCT", "CUTOFF", "RET", "OUT", "NOTE OUT"})
      ids.push_back(std::string(v) + ":" + j);
  for (const char* g : {"MOD:LD GATE", "MOD:BS GATE", "CLOCK:CLK IN", "CLOCK:RST IN", "CLOCK:RUN IN", "CLOCK:FILL IN",
                        "CLOCK:CLK OUT", "CLOCK:RST OUT", "CLOCK:RUN OUT", "CLOCK:ACC OUT", "MOD:LFO 1", "MOD:LFO 2",
                        "MOD:LFO 3", "MOD:LFO 4", "MOD:RND", "MOD:LANE A", "MIX:L", "MIX:R", "BD1:WAVE", "BD2:WAVE",
                        "BD1:FOLD VC", "BD2:FOLD VC", "LTC:FOLD VC", "MTC:FOLD VC", "HTC:FOLD VC"})
    ids.push_back(g);
  bool same = static_cast<int>(ids.size()) == kPorts;
  int typeErr = 0;
  for (int i = 0; i < kPorts && same; ++i) {
    const PortDesc& d = kPortTable[i];
    same = same && ids[i] == d.id;
    const std::string id = d.id;
    const auto pj = jidai::jcs::parseJackId(id);  // shared R6 split
    const std::string lab = pj ? pj->label : id;
    PortType want;
    if (d.dir == PortDir::In) want = lab == "RET" ? PortType::Audio : PortType::CV;
    else if (lab == "OUT" || id.rfind("MIX:", 0) == 0) want = PortType::Audio;
    else if (lab == "CLK OUT" || lab == "RST OUT" || lab == "RUN OUT" || lab == "LD GATE" || lab == "BS GATE") want = PortType::Gate;
    else want = PortType::CV;
    Role role = Role::CV;
    if (lab == "PITCH" || lab == "NOTE" || lab == "V/OCT" || lab == "NOTE OUT") role = Role::VOct;
    else if (lab == "RET" || lab == "OUT" || id.rfind("MIX:", 0) == 0) role = Role::Audio;
    else if (lab == "TRIG" || lab == "GATE" || lab.find("CLK") == 0 || lab.find("RST") == 0 || lab.find("RUN") == 0 ||
             lab == "FILL IN" || lab == "LD GATE" || lab == "BS GATE")
      role = Role::GateClk;
    if (d.type != want || d.role != role) {
      ++typeErr;
      std::printf("%s: type/role mismatch at %s\n", T, d.id);
    }
  }
  bool unique = true;
  for (int i = 0; i < kPorts; ++i)
    for (int j = i + 1; j < kPorts; ++j) unique = unique && std::strcmp(kPortTable[i].id, kPortTable[j].id) != 0;
  std::printf("%s: %d ports, ids exactly as 13.2: %d, unique %d, type/role errors %d, plainVoltGates %d\n", T, kPorts,
              same ? 1 : 0, unique ? 1 : 0, typeErr, kPlainVoltGates ? 1 : 0);
  truth(T, "153 ports", kPorts == 153);
  truth(T, "ids", same);
  truth(T, "unique", unique);
  truth(T, "types and roles", typeErr == 0);
  truth(T, "plainVoltGates", kPlainVoltGates);
}

void testHostLock() {
  const char* T = "testHostLock";
  auto e = make();
  e->setParamNow(P_CLOCK_SOURCE, stepU(SRC_HOST, 3));
  e->pattern().tracks[BD1].steps[0].on = true;
  HostTransport h;
  h.valid = h.playing = true;
  h.bpm = 120.0;
  h.ppq = 7.25;
  e->setHostTransport(h);
  e->processSample();
  const long s1 = e->globalStep();
  for (int n = 1; n < 512; ++n) e->processSample();
  for (int b = 1; b < 4; ++b) {
    h.ppq = 7.25 + b * 512.0 * 120.0 / 60.0 / 48000.0;
    e->setHostTransport(h);
    for (int n = 0; n < 512; ++n) e->processSample();
  }
  const long before = e->globalStep();
  h.ppq = 0.0;  // loop jump back to bar start
  e->setHostTransport(h);
  HitLog hl;
  hl.prevP = e->bd1().p;
  long synced = -1;
  for (int n = 0; n < 512; ++n) {
    e->processSample();
    hl.step(*e, n);
    if (synced < 0 && e->globalStep() == 0) synced = n;
  }
  std::printf("%s: ppq 7.25 at 1/16 -> step %ld; before jump step %ld; after jump to 0 synced at block sample %ld, BD1 hit %s\n",
              T, s1, before, synced, hl.hits.empty() ? "no" : "yes");
  near(T, "step 29", static_cast<double>(s1), 29.0, 0.0);
  truth(T, "re-sync within one block", synced >= 0);
  truth(T, "step 0 fires after the jump", !hl.hits.empty() && hl.hits[0] == 0);
}

}  // namespace

// ACC OUT (§13.2: "the step's accent volts while the step is on") follows the pattern in INT and in EXT; in EXT the
// voice itself stays silent (forge pin). Steps: 0 acc 3, 4 acc 1, step 8 off.
void testAccOutFollowsPattern() {
  const char* T = "testAccOutFollowsPattern";
  for (int ext = 0; ext < 2; ++ext) {
    auto e = make();
    e->setParamNow(P_CLOCK_SOURCE, stepU(SRC_INT, 3));
    e->setParamNow(P_CLOCK_MODE, stepU(ext, 2));
    e->pattern().tracks[BD2].steps[0].on = true;
    e->pattern().tracks[BD2].steps[0].acc = 3;
    e->pattern().tracks[BD2].steps[4].on = true;
    e->pattern().tracks[BD2].steps[4].acc = 1;
    Graph g;
    e->setRunning(true);
    const long P = static_cast<long>(std::lround(e->periodSamples()));
    double a0 = 0, a4 = 0, a8 = 0;
    bool voice = false;
    for (long n = 0; n < 9 * P; ++n) {
      g.step(*e);
      const double v = g.vals[PORT_ACC_OUT];
      if (n == P / 2) a0 = v;
      if (n == 4 * P + P / 2) a4 = v;
      if (n == 8 * P + P / 2) a8 = v;
      voice = voice || e->voiceActive(BD2);
    }
    std::printf("%s: %s: ACC OUT step 0 %.3f V, step 4 %.3f V, step 8 (off) %.3f V; BD2 played %d\n", T, ext ? "EXT" : "INT",
                a0, a4, a8, voice ? 1 : 0);
    near(T, ext ? "EXT acc 3 = 5.0 V" : "INT acc 3 = 5.0 V", a0, 5.0, 1e-6);
    near(T, ext ? "EXT acc 1 = 2.353 V" : "INT acc 1 = 2.353 V", a4, kAccentVolts[0], 1e-6);
    near(T, ext ? "EXT off step = 0 V" : "INT off step = 0 V", a8, 0.0, 1e-12);
    truth(T, ext ? "EXT: the voice stays silent" : "INT: the voice plays", ext ? !voice : voice);
  }
}

void runEngineTests() {
  testExtBypassIgnoresPattern();
  testIntIgnoresTrigJacks();
  testRestIsSilent();
  testSoloMutesOtherVoices();
  testShuffleSurvivesOddLength();
  testInitKitAndEmptyPattern();
  testControlOutsZeroLatency();
  testOsLatency();
  testPitchVoct();
  testPitchRail();
  testTuningIsNotTheVoltLaw();
  testLin55Migration();
  testJackIdsAndTypes();
  testJackIdsJcsShared();
  testHostLock();
  testAccOutFollowsPattern();
}
