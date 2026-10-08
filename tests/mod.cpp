// SHOGUN modulation named tests (§8, §15.4): sum/clamp law, OWN VOICE retrigger (locked), LFO host phase,
// LFO declick, S&H slew.
#include <random>

#include "rig.h"

using namespace shogun;
using namespace shogun::mod;
using namespace rig;
using tu::atLeast;
using tu::atMost;
using tu::near;
using tu::truth;

namespace {

void testModSumLaw() {
  const char* T = "testModSumLaw";
  const double ex1 = effectiveU(0.5, 0.18 * curve(LIN, 1.0), 1.0, 0.0);
  const double tau1 = decayTauMs(ex1);
  const double ex2 = effectiveU(0.5, 0.18, 1.0, 2.5);
  const double ex3 = effectiveU(0.2, -0.3 * curve(LIN, 1.0) * 0.5, 1.0, 0.0);
  const int ex4 = stepIndex(effectiveU(0.40, 0.10, 1.0, 0.0), 16);
  std::printf("%s: law: 0.5 + 0.18 = %.2f (tau %.2f ms); + 2.5 V -> %.2f; 0.2 - 0.3 x VEL 0.5 = %.2f; SOUND 0.40 + 0.10 -> %d\n",
              T, ex1, tau1, ex2, ex3, ex4);
  near(T, "0.68", ex1, 0.68, 1e-12);
  near(T, "tau 170.62 ms", tau1, 170.62, 0.005);
  near(T, "clamp 1.0", ex2, 1.0, 0.0);
  near(T, "0.05", ex3, 0.05, 1e-12);
  truth(T, "SOUND index 8", ex4 == 8);
  // Curves.
  near(T, "EXP", curve(EXP, -0.5), -0.25, 1e-12);
  near(T, "LOG", curve(LOG, 0.25), 0.5, 1e-12);
  near(T, "S-CRV", curve(SCRV, 1.0), 1.0, 1e-12);
  // FREE rates.
  Lfo l;
  l.prepare(48000.0, 0);
  double fr[3];
  for (int i = 0; i < 3; ++i) {
    l.rateU = 0.5 * i;
    fr[i] = l.freqHz(120.0);
  }
  std::printf("%s: FREE rate %.4f / %.4f / %.4f Hz at u = 0 / 0.5 / 1\n", T, fr[0], fr[1], fr[2]);
  near(T, "0.01 Hz", fr[0], 0.01, 1e-9);
  near(T, "0.6325 Hz", fr[1], 0.6325, 1e-4);
  near(T, "40 Hz", fr[2], 40.0, 1e-9);
  // The same examples through the engine matrix (MOD W = 1 as a constant source, BD1 at 48 k, ideal).
  auto e = make();
  e->setParamNow(P_BD1_DECAY, 0.5);
  e->setParamNow(P_BD1_TUNE, 0.2);
  e->setParamNow(P_BD1_SOUND, 0.40);
  ModSystem& m = e->modulation();
  Row r;
  r.src = SRC_MODW;
  r.dst = P_BD1_DECAY;
  r.depth = 0.18;
  m.addRow(r);
  r.dst = P_BD1_TUNE;
  r.depth = -0.3;
  r.via = SRC_VEL;
  m.addRow(r);
  r.dst = P_BD1_SOUND;
  r.depth = 0.10;
  r.via = SRC_NONE;
  m.addRow(r);
  e->setModWheel(1.0);
  run(*e, 40);
  const double uDecay = e->effective(P_BD1_DECAY);
  e->trigger(BD1, 2.5);  // VEL source (g_vel − 0.15)/0.85 = V/5 = 0.5
  run(*e, 40);
  const double uTune = e->effective(P_BD1_TUNE);
  const int sound = e->bd1().sound;
  const double tauB = e->bd1().tauB * 1000.0;
  Graph g;
  g.con[drumPort(BD1, DJ_DECAY)] = true;
  g.vals[drumPort(BD1, DJ_DECAY)] = 2.5f;
  for (int n = 0; n < 40; ++n) g.step(*e);
  const double uClamp = e->effective(P_BD1_DECAY);
  std::printf("%s: engine: DECAY u_eff %.4f (tau %.2f ms), +2.5 V -> %.4f; TUNE via VEL 0.5 -> %.4f; SOUND index %d\n", T,
              uDecay, tauB, uClamp, uTune, sound);
  near(T, "engine 0.68", uDecay, 0.68, 1e-9);
  near(T, "engine tau", tauB, 170.62, 0.005);
  near(T, "engine clamp", uClamp, 1.0, 0.0);
  near(T, "engine 0.05", uTune, 0.05, 1e-9);
  truth(T, "engine SOUND index 8 at the trigger", sound == 8);
}

// LOCKED (§15.4): LFO 1 SIN, SYNC 1/4, 120 BPM, OWN VOICE → BD1 DECAY and SD TUNE, fs 48 k.
void testOwnVoiceRetrigIndependent() {
  const char* T = "testOwnVoiceRetrigIndependent";
  auto e = make();
  e->setParamNow(P_CLOCK_TEMPO, 0.5);
  e->setParamNow(P_LFO_1_SHAPE, stepU(L_SIN, 6));
  e->setParamNow(P_LFO_1_SYNC, 1.0);
  e->setParamNow(P_LFO_1_DIV, stepU(15, kLfoDivCount));
  e->setParamNow(P_LFO_1_MODE, stepU(M_RETRIG, 3));
  e->setParamNow(P_LFO_1_RETRIG_BY, stepU(kRetrigOwn, 3 + kVoices));
  ModSystem& m = e->modulation();
  Row r;
  r.src = SRC_LFO1;
  r.dst = P_BD1_DECAY;
  r.depth = 0.2;
  m.addRow(r);
  r.dst = P_SD_TUNE;
  m.addRow(r);
  const Lfo& l = m.lfo[0];
  double bd12k = 9, sd12k = 9, bd18k = 9, sd18k = 9, bdAfter = 9, sdAfter = 9;
  e->trigger(BD1);
  for (long n = 0; n <= 18000; ++n) {
    if (n == 12000) e->trigger(SD);
    if (n == 18000) {
      bd18k = l.rawValue(BD1);  // values at the end of sample 17,999 → phase of n = 18,000 before its tick
    }
    e->processSample();
    if (n == 12000) {
      bd12k = l.rawValue(BD1);
      sd12k = l.rawValue(SD);
    }
    if (n == 18000) {
      bd18k = l.rawValue(BD1);
      sd18k = l.rawValue(SD);
    }
  }
  e->trigger(BD1);  // third trig at n = 18,000 (the next processed sample is the trigger sample)
  e->processSample();
  bdAfter = l.rawValue(BD1);
  sdAfter = l.rawValue(SD);
  const double sdWant = std::sin(2.0 * kPi * 6001.0 / 24000.0);
  std::printf("%s: n = 12,000: BD1 %.6f SD %.6f; n = 18,000: BD1 %.6f SD %.6f; BD1 retrig: BD1 %.6f, SD %.6f (unchanged path %.6f)\n",
              T, bd12k, sd12k, bd18k, sd18k, bdAfter, sdAfter, sdWant);
  near(T, "BD1 at 12,000", bd12k, 0.0, 1e-6);
  near(T, "SD at 12,000", sd12k, 0.0, 1e-6);
  near(T, "BD1 at 18,000", bd18k, -1.0, 1e-6);
  near(T, "SD at 18,000", sd18k, 1.0, 1e-6);
  near(T, "BD1 reset", bdAfter, 0.0, 1e-6);
  near(T, "SD unchanged", sdAfter, sdWant, 1e-6);
}

void testLfoHostPhase() {
  const char* T = "testLfoHostPhase";
  // As verify_lfo.py: 123 BPM, 1/4, random host blocks 32..2047 over 600 s; the engine's per-sample ppq is the block
  // ppq plus the in-block offset, and SYNC FREE-RUN re-anchors φ = frac(ppq / D) every sample.
  Lfo l;
  l.prepare(48000.0, 0);
  l.sync = true;
  l.mode = M_FREE_RUN;
  l.div = 15;  // 1/4
  l.shape = L_SIN;
  const double bpm = 123.0, fs = 48000.0;
  std::mt19937 rng(3);
  long long n = 0;
  double worst = 0.0;
  while (n < static_cast<long long>(fs * 600.0)) {
    const int B = 32 + static_cast<int>(rng() % 2016);
    const double blockPpq = static_cast<double>(n) / fs * bpm / 60.0;
    for (int k = 0; k < B; ++k) {
      const double ppq = blockPpq + k * bpm / (60.0 * fs);
      l.tick(bpm, ppq, true);
      const long double exact = static_cast<long double>(n + k) / static_cast<long double>(fs) * static_cast<long double>(bpm) / 60.0L;
      const double ex = static_cast<double>(exact - std::floor(exact));
      double err = l.global.ph - ex;
      err -= std::round(err);
      worst = std::fmax(worst, std::fabs(err));
    }
    n += B;
  }
  std::printf("%s: worst re-anchored phase error over 600 s: %.3g cycles (limit 1e-9)\n", T, worst);
  atMost(T, "phase error", worst, 1e-9);
}

void testLfoDeclick() {
  const char* T = "testLfoDeclick";
  Lfo l;
  l.prepare(48000.0, 0);
  l.shape = L_SQR;
  l.rateU = 1.0;  // 40 Hz FREE
  l.phase = 0.123;
  l.slew = 0.0;   // declick floor 0.25 ms
  l.pw = 0.5;
  const int N = 9600;
  std::vector<double> y(N);
  double maxStep = 0.0;
  for (int n = 0; n < N; ++n) {
    l.tick(120.0, 0.0, false);
    y[static_cast<size_t>(n)] = l.value(-1);
    if (n > 0) maxStep = std::fmax(maxStep, std::fabs(y[static_cast<size_t>(n)] - y[static_cast<size_t>(n - 1)]));
  }
  const auto w = tu::hann(N);
  std::vector<std::complex<double>> a(16384);
  for (int i = 0; i < N; ++i) a[static_cast<size_t>(i)] = y[static_cast<size_t>(i)] * w[static_cast<size_t>(i)];
  // Energy above 2 kHz over total, on the exact-length DFT as verify (direct, N not a power of two).
  double hi = 0.0, tot = 0.0;
  for (int k = 0; k <= N / 2; ++k) {
    std::complex<double> s(0.0, 0.0);
    const double ang = -2.0 * kPi * k / N;
    for (int i = 0; i < N; ++i) s += y[static_cast<size_t>(i)] * w[static_cast<size_t>(i)] * std::polar(1.0, ang * i);
    const double p = std::norm(s);
    tot += p;
    if (k * 48000.0 / N > 2000.0) hi += p;
  }
  const double hfDb = 10.0 * std::log10(hi / tot);
  std::printf("%s: 40 Hz square, PolyBLEP + 0.25 ms declick: max step %.4f (limit 0.15), energy above 2 kHz %.2f dB (limit -35)\n",
              T, maxStep, hfDb);
  atMost(T, "max step", maxStep, 0.15);
  atMost(T, "HF energy", hfDb, -35.0);
}

void testShSlew() {
  const char* T = "testShSlew";
  Lfo l;
  l.prepare(48000.0, 0);
  l.shape = L_SH;
  l.sync = true;
  l.div = 15;  // 1/4 at 120 BPM: T = 0.5 s
  l.slew = 1.0;
  l.global.hold = 1.0;
  l.global.y = 0.0;
  long n63 = -1;
  for (long n = 0; n < 20000 && n63 < 0; ++n) {
    l.tick(120.0, 0.0, false);
    if (l.global.y >= 1.0 - std::exp(-1.0)) n63 = n + 1;
  }
  const double ms = 1000.0 * static_cast<double>(n63) / 48000.0;
  std::printf("%s: SLEW 1 at 1/4, 120 BPM: tau 125 ms, 63 %% at %.3f ms\n", T, ms);
  near(T, "63% at 125.0 ms", ms, 125.0, 0.1);
}

}  // namespace

void runModTests() {
  testModSumLaw();
  testOwnVoiceRetrigIndependent();
  testLfoHostPhase();
  testLfoDeclick();
  testShSlew();
}
