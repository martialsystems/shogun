// Block tests (spec §15.4, step 1): each reproduces a number from /workspace/shogun-redesign/verify/.
#include "dsp.h"
#include "wave_shaper.h"
#include "testutil.h"

#include <jidai/dsp/Halfband.h>
#include <jidai/jcs/Detect.h>
#include <jidai/jcs/Pitch.h>
#include "data/levelcomp_cases.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace shogun::dsp;
// The wave tests are unchanged from the local-shaper build: TripleShaper here is the SHOGUN wrapper on the shared
// jidai::dsp::TripleShaper (engine/wave_shaper.h).
using TripleShaper = shogun::WaveShaper;
using shogun::TripleShaperParams;

namespace {

const double kRates[3] = {44100.0, 48000.0, 96000.0};

void testDecayIsRateIndependent() {
  const char* t = "testDecayIsRateIndependent";
  const double tau = 0.136195;
  for (double fs : kRates) {
    RcEnv e;
    e.setDecay(tau, fs);
    e.trigger();
    int n = 0;
    while (e.tick() >= 1e-3) ++n;
    const double ms = 1000.0 * n / fs;
    std::printf("%s: -60 dB at %.0f Hz %.3f ms (closed form 940.802)\n", t, fs, ms);
    tu::near(t, "-60 dB time ms", ms, 940.802, 0.05);
  }
}

int endSample(double tau, double fs) {
  RcEnv e;
  e.setDecay(tau, fs);
  e.trigger();
  int n = 0;
  while (true) {
    e.tick();
    ++n;
    if (e.quiet()) return n;
  }
}

void testVoiceEndsAtMinus90Envelope() {
  const char* t = "testVoiceEndsAtMinus90 (envelope)";
  const int bd1 = endSample(0.136195, 48000.0);
  const int d64 = endSample(decayTau(64.0 / 127.0), 48000.0);
  std::printf("%s: BD1 example end %d, Decay 64 end %d (threshold %.4g)\n", t, bd1, d64, kQuiet);
  tu::near(t, "BD1 example end", bd1, 67738, 1);
  tu::near(t, "Decay 64 end", d64, 38426, 1);
}

void testSvfMinus3dB() {
  const char* t = "testSvfMinus3dB";
  for (double fs : kRates) {
    TptSvf f;
    f.set(1000.0, 1.0 / std::sqrt(2.0), fs);
    std::vector<double> y(static_cast<size_t>(fs));
    for (size_t i = 0; i < y.size(); ++i) {
      f.tick(std::sin(2.0 * kPi * 1000.0 * static_cast<double>(i) / fs));
      y[i] = f.lp;
    }
    const double g = tu::db(tu::sineAmp(y, 1000.0, fs, y.size() / 2));
    std::printf("%s: LP at fc %.0f Hz: %.4f dB\n", t, fs, g);
    tu::near(t, "LP at fc dB", g, -3.0103, 0.001);
  }
}

// Envelope fit: slope of ln r over [t0, t1], r = quadrature amplitude.
double fitTauMs(Resonator& r, double fs, double (*fOf)(int), double tau) {
  const int n = static_cast<int>(fs);
  double sx = 0, sy = 0, sxx = 0, sxy = 0;
  int m = 0;
  for (int i = 0; i < n; ++i) {
    r.set(fOf(i), tau, fs);
    if (i == 0) r.kick(1.0);
    r.tick();
    const double tt = i / fs;
    if (tt > 0.05 && tt < 0.6) {
      const double amp = TripleShaper::quadAmp(r.lp(), r.bp(), r.svf.R);
      const double ly = std::log(amp);
      sx += tt;
      sy += ly;
      sxx += tt * tt;
      sxy += tt * ly;
      ++m;
    }
  }
  const double slope = (m * sxy - sx * sy) / (m * sxx - sx * sx);
  return -1000.0 / slope;
}
double f60(int) { return 60.0; }
double fSweep(int i) { return 60.408707 * std::pow(2.0, 5.669291 * std::exp(-i / (48000.0 * 0.090740)) / 12.0); }

void testResonatorTau() {
  const char* t = "testResonatorTau";
  for (double fs : kRates) {
    Resonator r;
    const double ms = fitTauMs(r, fs, f60, 0.136195);
    std::printf("%s: tau at %.0f Hz %.4f ms (target 136.195)\n", t, fs, ms);
    tu::near(t, "tau ms", ms, 136.195, 136.195e-4);
  }
  Resonator r;
  const double ms = fitTauMs(r, 48000.0, fSweep, 0.136195);
  std::printf("%s: tau with the BD1 pitch sweep %.4f ms\n", t, ms);
  tu::near(t, "tau with sweep ms", ms, 136.195, 136.195 * 5e-4);
}

void testResonatorPeak() {
  const char* t = "testResonatorPeak";
  const double fr[4] = {35.0, 140.0, 1000.0, 3000.0};
  const double want[4] = {0.87, 0.965, 0.991, 0.960};
  for (int k = 0; k < 4; ++k) {
    Resonator r;
    r.set(fr[k], 0.05, 48000.0);
    r.kick(1.0);
    double pk = 0.0;
    for (int i = 0; i < 4800; ++i) pk = std::fmax(pk, std::fabs(r.tick()));
    std::printf("%s: peak at %.0f Hz %.4f\n", t, fr[k], pk);
    tu::near(t, "peak", pk, want[k], 0.01);
  }
}

void testRetriggerNoClick() {
  const char* t = "testRetriggerNoClick";
  Resonator r;
  r.set(60.0, 0.3, 48000.0);
  double prev = 0.0, worst = 0.0;
  for (int i = 0; i < 9600; ++i) {
    if (i == 0 || i == 3000) r.kick(1.0);
    const double y = r.tick();
    worst = std::fmax(worst, std::fabs(y - prev));
    prev = y;
  }
  std::printf("%s: max sample step with a retrigger %.5f (old phase reset 1.0)\n", t, worst);
  tu::atMost(t, "max step", worst, 0.02);
}

double ladderGainDb(double f, double k) {
  ZdfLadder l;
  l.set(1000.0, 48000.0);
  const double fs = 48000.0;
  std::vector<double> y(48000);
  for (size_t i = 0; i < y.size(); ++i) y[i] = l.tick(1e-5 * std::sin(2.0 * kPi * f * static_cast<double>(i) / fs), k);
  return tu::db(tu::sineAmp(y, f, fs, 24000) / 1e-5);
}

void testLadderResponse() {
  const char* t = "testLadderResponse";
  const double ks[3] = {0.0, 2.0, 3.5};
  const double spec[3] = {-12.05, -6.04, 5.95};  // verify_dsp.py: analog prototype at the FFT bin nearest 1 kHz
  for (int i = 0; i < 3; ++i) {
    const double exact = 20.0 * std::log10(1.0 / std::fabs(ks[i] - 4.0));  // H(fc) = 1/(k − 4)
    const double atFc = ladderGainDb(1000.0, ks[i]);
    const double atBin = ladderGainDb(683.0 * 48000.0 / 32768.0, ks[i]);
    std::printf("%s: k %.1f gain at fc %.3f dB (exact %.3f), at the verify bin 1000.49 Hz %.3f dB (spec %.2f)\n", t,
                ks[i], atFc, exact, atBin, spec[i]);
    tu::near(t, "gain at fc vs 1/(k-4)", atFc, exact, 0.05);
    tu::near(t, "gain at the verify bin vs spec", atBin, spec[i], 0.05);
  }
}

void testLadderSelfOsc() {
  const char* t = "testLadderSelfOsc";
  const double fs = 48000.0;
  for (double k : {3.8, 4.2}) {
    ZdfLadder l;
    l.set(500.0, fs);
    std::vector<double> y(2 * 48000);
    for (size_t i = 0; i < y.size(); ++i) y[i] = l.tick(i == 0 ? 0.1 : 0.0, k);
    const double r = tu::rms(y, 72000);
    const double pk = tu::peakAbs(y);
    std::printf("%s: k %.1f rms of the last 0.5 s %.3g, peak %.4f\n", t, k, r, pk);
    if (k < 4.0) tu::atMost(t, "k 3.8 rms after 2 s", r, 1e-9);
    else tu::atMost(t, "k 4.2 peak", pk, 0.2);
  }
  ZdfLadder l;
  l.set(2000.0, fs);
  int worst = 0;
  double sum = 0.0, pk = 0.0;
  for (int i = 0; i < 48000; ++i) {
    const double x = 3.0 * (std::sin(2.0 * kPi * 55.0 * i / fs) >= 0 ? 1.0 : -1.0);
    pk = std::fmax(pk, std::fabs(l.tick(x, 4.0)));
    worst = l.lastIters > worst ? l.lastIters : worst;
    sum += l.lastIters;
  }
  std::printf("%s: Newton iterations max %d mean %.3f, hard-drive peak %.4f\n", t, worst, sum / 48000.0, pk);
  tu::atMost(t, "Newton iterations", worst, 4);
  bool finite = true;
  for (double fc : {20.0, 18000.0, 23000.0}) {
    ZdfLadder m;
    m.set(fc, fs);
    for (int i = 0; i < 4800; ++i) finite = finite && std::isfinite(m.tick(3.0 * (i % 872 < 436 ? 1.0 : -1.0), 4.0));
  }
  tu::truth(t, "finite at fc 20 / 18k / 23k", finite);
}

void testOtaSvfBounded() {
  const char* t = "testOtaSvfBounded";
  OtaSvf f;
  f.svf.set(800.0, 0.0, 48000.0);
  double pk = 0.0;
  for (int i = 0; i < 96000; ++i) pk = std::fmax(pk, std::fabs(f.tick(i == 0 ? 1.0 : 0.0)));
  OtaSvf h;
  h.svf.set(3000.0, 0.0, 48000.0);
  double pkh = 0.0;
  bool finite = true;
  for (int i = 0; i < 48000; ++i) {
    const double y = h.tick(5.0 * (std::sin(2.0 * kPi * 110.0 * i / 48000.0) >= 0 ? 1.0 : -1.0));
    finite = finite && std::isfinite(y);
    pkh = std::fmax(pkh, std::fabs(y));
  }
  std::printf("%s: R 0 impulse peak %.4f (verify 0.1042); R 0 hard input peak %.4f\n", t, pk, pkh);
  tu::truth(t, "finite", finite);
  tu::atMost(t, "hard input peak", pkh, 10.0);
  tu::near(t, "impulse peak", pk, 0.1042, 0.001);
}

void stopRipple(const double* h, int n, double fp, double fsb, double& stop, double& ripple) {
  double mx = 0.0, pmax = 0.0, pmin = 1e9;
  const int G = 1 << 16;
  for (int i = 0; i <= G; ++i) {
    const double f = 0.5 * i / G;
    std::complex<double> H(0.0, 0.0);
    for (int k = 0; k < n; ++k) H += h[k] * std::polar(1.0, -2.0 * kPi * f * k);
    const double m = std::abs(H);
    if (f >= fsb) mx = std::fmax(mx, m);
    if (f <= fp) {
      pmax = std::fmax(pmax, m);
      pmin = std::fmin(pmin, m);
    }
  }
  stop = -tu::db(mx);
  ripple = tu::db(pmax) - tu::db(pmin);
}

void testHalfbandSpec() {
  const char* t = "testHalfbandSpec";
  // Stage 1 is the shared exact halfband (jidai/dsp/Halfband.h): h[46] = 0.5, h[46 ± (2j+1)] = odd[j], else 0.
  double h1[jidai::dsp::Halfband93::kTaps] = {};
  h1[46] = 0.5;
  for (int j = 0; j < jidai::dsp::Halfband93::kSide; ++j) h1[46 - (2 * j + 1)] = h1[46 + (2 * j + 1)] = jidai::dsp::Halfband93::odd()[j];
  int zeros = 0;
  for (int k = 0; k < jidai::dsp::Halfband93::kTaps; ++k) zeros += (k != 46 && (k - 46) % 2 == 0 && tu::same(h1[k], 0.0)) ? 1 : 0;
  double s1, r1, s2, r2;
  stopRipple(h1, jidai::dsp::Halfband93::kTaps, 0.2125, 0.2875, s1, r1);
  stopRipple(shogun::hb::kStage2, shogun::hb::kStage2Taps, 0.10625, 0.39375, s2, r2);
  std::printf("%s: stage 1 (shared exact halfband, 93 taps, %d even-offset zeros) stopband %.2f dB ripple %.2e dB; stage 2 (25 taps) stopband %.2f dB\n",
              t, zeros, s1, r1, s2);
  tu::near(t, "stage 1 exact halfband zeros", zeros, 46, 0);
  tu::near(t, "stage 1 latency per direction", jidai::dsp::Halfband93::kLatencyPerDirection, 23, 0);
  tu::atLeast(t, "stage 1 stopband", s1, 111.5);
  tu::atMost(t, "stage 1 ripple", r1, 1e-4);
  tu::atLeast(t, "stage 2 stopband", s2, 123.0);
  tu::near(t, "latency 1x/2x/4x", osLatency(1) + 100 * osLatency(2) + 10000 * osLatency(4), 0 + 2300 + 260000, 0);
}

void testDecimatorDelay() {
  const char* t = "testDecimatorDelay";
  for (int M : {2, 4}) {
    Decimator d;
    d.prepare(M);
    // A slow sine rendered at M·fs must come out as the same sine at fs, delayed by exactly osLatency(M).
    const double fs = 48000.0, f = 500.0;
    std::vector<double> out;
    for (int n = 0; n < 4800; ++n) {
      for (int s = 0; s < M; ++s) {
        double y;
        if (d.push(std::sin(2.0 * kPi * f * (n + double(s) / M) / fs), y)) out.push_back(y);
      }
    }
    double err = 0.0;
    const int L = osLatency(M);
    for (int n = 1000; n < 4800; ++n) err = std::fmax(err, std::fabs(out[static_cast<size_t>(n)] - std::sin(2.0 * kPi * f * (n - L) / fs)));
    std::printf("%s: %dx output = input delayed %d base samples, max error %.2e\n", t, M, L, err);
    tu::atMost(t, "aligned error", err, 1e-4);
  }
}

void testBlepAlias() {
  const char* t = "testBlepAlias";
  const double f0 = 1318.51, fs = 48000.0;
  const int n = 1 << 15;
  for (double f : {f0, 3500.0}) {
    PolyBlepOsc o;
    o.setFreq(f, 2.0 * fs);
    Decimator d;
    d.prepare(2);
    std::vector<double> y;
    while (static_cast<int>(y.size()) < n + 200) {
      double v;
      if (d.push(o.saw(), v)) y.push_back(v);
      o.advance();
    }
    y.erase(y.begin(), y.begin() + 200);
    const double a = tu::aliasWorstDb(y, f, fs);
    std::printf("%s: BLEP saw %.2f Hz at 2x, worst alias %.1f dB\n", t, f, a);
    if (tu::same(f, f0)) tu::atMost(t, "worst alias (verify config 1318.51 Hz)", a, -60.0);
  }
}

void testTanhAdaaAlias() {
  const char* t = "testTanhAdaaAlias";
  const double f1 = 4987.0, fs = 48000.0;
  const int n = 1 << 15;
  TanhAdaa s;
  Decimator d;
  d.prepare(2);
  std::vector<double> y;
  for (int i = 0; static_cast<int>(y.size()) < n + 200; ++i) {
    double v;
    // tanh(4·sin)/tanh(4): the same spectrum shape as verify_dsp.py's tanh(4·sin).
    if (d.push(s.tick(std::sin(2.0 * kPi * f1 * i / (2.0 * fs)), 4.0), v)) y.push_back(v);
  }
  y.erase(y.begin(), y.begin() + 200);
  const double a = tu::aliasWorstDb(y, f1, fs);
  std::printf("%s: 4987 Hz, drive 4, 2x ADAA worst alias %.1f dB (verify -77.8)\n", t, a);
  tu::atMost(t, "worst alias", a, -77.0);
}

void testOuDriftStd() {
  const char* t = "testOuDriftStd";
  OuDrift d;
  d.prepare(48000.0);
  d.setSigma(4.0);
  const long n = static_cast<long>(1500.0 * 20000.0);
  double x = 0.0, s = 0.0, ss = 0.0;
  for (long i = 0; i < n; ++i) {
    x = d.rho * x + d.sigma * std::sqrt(1.0 - d.rho * d.rho) * d.rng.noise();
    s += x;
    ss += x * x;
  }
  const double sd = std::sqrt(ss / n - (s / n) * (s / n));
  std::printf("%s: rho %.6f, std over 20,000 s %.3f cents\n", t, d.rho, sd);
  tu::near(t, "rho", d.rho, 0.999556, 1e-6);
  tu::near(t, "std cents", sd, 4.0, 0.05);
}

void testSmoothing() {
  const char* t = "testSmoothing";
  Smoother s;
  s.prepare(48000.0);
  std::printf("%s: 5 ms coefficient at 48 kHz %.6f\n", t, s.a);
  tu::near(t, "coefficient", s.a, 0.995842, 1e-6);
}

void testLevelLawAndCentrePan() {
  const char* t = "testLevelLaw";
  const double u[4] = {0.25, 0.5, 0.75, 1.0};
  const double want[4] = {0.0883, 0.353, 0.795, 1.4125};
  for (int i = 0; i < 4; ++i) {
    std::printf("%s: u %.2f -> %.4f\n", t, u[i], gLevel(u[i]));
    tu::near(t, "g_level", gLevel(u[i]), want[i], 0.0005);
  }
  std::printf("testCentrePan: centre %.6f / %.6f\n", panL(0.0), panR(0.0));
  tu::near("testCentrePan", "L", panL(0.0), 0.707107, 1e-6);
  tu::near("testCentrePan", "R", panR(0.0), 0.707107, 1e-6);
}

void testGateHysteresis() {
  const char* t = "testGateHysteresis";
  jidai::jcs::Schmitt d;  // the shared JCS R3 detector (float volts)
  const double seq[6] = {0.0, 0.9, 1.1, 0.7, 0.4, 1.1};
  const bool edge[6] = {false, false, true, false, false, true};
  const bool high[6] = {false, false, true, true, false, true};
  for (int i = 0; i < 6; ++i) {
    const bool e = d.rising(static_cast<float>(seq[i]));
    std::printf("%s: %.1f V -> edge %d high %d\n", t, seq[i], e ? 1 : 0, d.high ? 1 : 0);
    tu::truth(t, "edge", e == edge[i]);
    tu::truth(t, "state", d.high == high[i]);
  }
  jidai::jcs::Schmitt g;
  int edges = 0;
  for (int i = 0; i < 4800; ++i) edges += g.rising((i % 1200) < 600 ? 5.0f : 0.0f) ? 1 : 0;
  std::printf("%s: 0/5 V logic gate, 4 gates -> %d edges\n", t, edges);
  tu::near(t, "edges", edges, 4, 0);
}

// ------------------------------------------------------------ triple wave shaper (§4.6)
std::vector<double> renderShaper(TripleShaper& s, int bins, double amp, int M, int vcBins = 0, bool selfVc = false) {
  const int N = 32768;
  const int L = N * M;
  std::vector<double> y(static_cast<size_t>(2 * L));
  for (int n = 0; n < 2 * L; ++n) {
    const double x = amp * std::sin(2.0 * kPi * bins * n / L);
    double vc = 0.0;
    if (vcBins > 0) vc = std::sin(2.0 * kPi * vcBins * n / L);
    if (selfVc) vc = x / amp;
    y[static_cast<size_t>(n)] = s.process(x, vc);
  }
  return std::vector<double>(y.begin() + L, y.end());
}

void testWaveBypass() {
  const char* t = "testWaveBypass";
  TripleShaper s;
  s.prepare(96000.0);
  TripleShaperParams p;
  p.levelComp = true;
  s.setParams(p);
  bool same = true;
  for (int n = 0; n < 9600; ++n) {
    const double x = 0.6 * std::sin(0.01 * n) * std::exp(-n / 3000.0);
    same = same && tu::same(s.process(x), x);
  }
  std::printf("%s: WAVE 0, trims/SYM/SHAPE 0 (default patch): bypassed %d, output bit-identical %d, level gain untouched %.1f\n", t,
              s.bypassed() ? 1 : 0, same ? 1 : 0, s.levelGain());
  tu::truth(t, "bypassed", s.bypassed());
  tu::truth(t, "bit-identical", same);
  tu::truth(t, "level comp skipped", tu::same(s.levelGain(), 1.0));
  tu::truth(t, "SHAPE 0 is the body", tu::same(TripleShaper::shapeMorph(0.123456789, -0.3, 0.01, 0.0, 0.01), 0.123456789));
}

void testWaveMigration() {
  const char* t = "testWaveMigration";
  double worst = 0.0;
  for (int v = 0; v <= 127; ++v) {
    const double w = v / 127.0;
    const TripleShaperParams p = TripleShaper::migrateV21(w);
    for (int i = 0; i <= 4000; ++i) {
      const double x = -1.2 + 2.4 * i / 4000.0;
      worst = std::fmax(worst, std::fabs(TripleShaper::chainStatic(x, p) - TripleShaper::v21Law(x, w)));
    }
  }
  const auto p32 = TripleShaper::migrateV21(32.0 / 127.0), p90 = TripleShaper::migrateV21(90.0 / 127.0),
             p127 = TripleShaper::migrateV21(1.0);
  std::printf("%s: max error v 0..127 %.3g; v32 m %.3f trim2 %.3f; v90 m %.3f trim2 %.3f; v127 m %.3f trim2 %.3f\n", t,
              worst, p32.macro, p32.trim[1], p90.macro, p90.trim[1], p127.macro, p127.trim[1]);
  tu::atMost(t, "max error", worst, 1e-6);
  tu::truth(t, "LEVEL COMP off", !p90.levelComp);
  tu::near(t, "v90 trim2", p90.trim[1], -0.2087, 1e-4);
}

double harmDb(const std::vector<double>& y, int b0, int h, int ref) {
  std::vector<double> z(y);
  double mean = 0.0;
  for (double v : z) mean += v;
  mean /= static_cast<double>(z.size());
  for (double& v : z) v -= mean;
  auto Y = tu::rfftMag(z);
  return tu::db(Y[static_cast<size_t>(h * b0)] / Y[static_cast<size_t>(ref * b0)] + 1e-12);
}

void testWaveStageLaw() {
  const char* t = "testWaveStageLaw";
  double worst = 0.0;
  for (int i = 0; i <= 10; ++i) {
    for (int j = 0; j <= 20; ++j) {
      for (int k = 0; k < 3; ++k) worst = std::fmax(worst, std::fabs(TripleShaper::stage(0.0, i / 10.0, -1.0 + j / 10.0, TripleShaper::kK[k])));
    }
  }
  const int N = 32768, B0 = 704;
  std::vector<double> tri(N), saw(N);
  for (int n = 0; n < N; ++n) {
    const double ph = 2.0 * kPi * B0 * n / N;
    tri[static_cast<size_t>(n)] = TripleShaper::stage((2.0 / kPi) * std::asin(std::sin(ph)), 1.0, 1.0, 0.0);
    double s = std::fmod(ph / kPi + 1.0, 2.0);
    saw[static_cast<size_t>(n)] = TripleShaper::stage(s - 1.0, 1.0, 1.0, 0.0);
  }
  const double t1 = harmDb(tri, B0, 1, 2), t3 = harmDb(tri, B0, 3, 2), t4 = harmDb(tri, B0, 4, 2);
  const double s2 = harmDb(saw, B0, 2, 1), s3 = harmDb(saw, B0, 3, 1);
  std::printf("%s: max |s(0)| %.2g; SYM +1 triangle: H1 %.1f H3 %.1f H4 %.1f dB re 2F; saw: H2 %.1f H3 %.1f dB re 1F\n", t,
              worst, t1, t3, t4, s2, s3);
  tu::atMost(t, "s(0)", worst, 1e-15);
  tu::atMost(t, "triangle odd H1", t1, -120.0);
  tu::atMost(t, "triangle odd H3", t3, -120.0);
  tu::near(t, "triangle H4", t4, -14.0, 0.1);
  tu::near(t, "saw H2", s2, -14.0, 0.1);
}

void testWaveAliasStatic() {
  const char* t = "testWaveAliasStatic";
  struct Row {
    int bins;
    double amp, m, limit, verify;
    const char* name;
  } rows[3] = {{272, 0.8, 1.0, -59.0, -60.0, "398 Hz, WAVE 1.0"},
               {150, 0.8, 1.0, -116.8, -117.8, "220 Hz, WAVE 1.0"},
               {704, 1.0, 0.5, -120.9, -121.9, "1031 Hz, WAVE 0.5"}};
  for (const auto& r : rows) {
    TripleShaper s;
    s.prepare(96000.0);
    TripleShaperParams p;
    p.macro = r.m;
    p.levelComp = false;
    s.setParams(p);
    const auto y = renderShaper(s, r.bins, r.amp, 2);
    const double a = tu::aliasLinesDb(y, 2, 32768, 48000.0, r.bins);
    std::printf("%s: %s, 2x ADAA alias %.1f dB (verify %.1f)\n", t, r.name, a, r.verify);
    tu::atMost(t, r.name, a, r.limit);
  }
}

// jidai-common 1.1.2 per-block plan (§4.6): a stage is a wire for a block when its amount is 0 for the whole block,
// at any SYM: the amount controls are steady, a is exactly 0 and no live VC has AMT depth on it. SYM and VC > SYM
// never keep a stage running. A moving block or a live VC > AMT keeps the stage's ADAA.
void testWavePlanBlock() {
  const char* t = "testWavePlanBlock";
  TripleShaper s;
  s.prepare(96000.0);
  TripleShaperParams p;
  p.macro = 0.25;  // c = (0.5, 0, 0): stage 1 live, stages 2 and 3 at a = 0
  p.sym[1] = 0.6;  // SYM on the a = 0 stages: still wires (1.1.2)
  p.vcSym[2] = 0.5;
  auto wires = [&s] { return (s.stageIsWire(0) ? 4 : 0) + (s.stageIsWire(1) ? 2 : 0) + (s.stageIsWire(2) ? 1 : 0); };
  s.setParams(p, true, true);
  const int steady = wires();
  s.setParams(p, false, true);
  const int moving = wires();
  p.vcAmt[2] = 0.5;
  s.setParams(p, true, true);
  const int vcLive = wires();
  s.setParams(p, true, false);
  const int vcOff = wires();
  // A moving block whose stage 2 sits at a = 0 (SYM 0.25) runs ADAA on it: one sample of the identity's ADAA is the
  // mean of x[n] and x[n-1] (half-sample delay), never a mid-stream switch to the wire. The steady block wires it.
  TripleShaperParams q;
  q.levelComp = false;
  q.macro = 0.25;   // stage 1 live
  q.sym[1] = 0.25;  // stage 2 at a = 0 with SYM: a wire only when steady
  TripleShaper w, m;
  w.prepare(96000.0);
  m.prepare(96000.0);
  w.setParams(q, true, false);
  m.setParams(q, false, false);
  const bool wWire = w.stageIsWire(1) && w.stageIsWire(2) && !w.stageIsWire(0);
  double maxWire = 0.0;
  for (int n = 0; n < 64; ++n) {
    const double x = 0.5 * std::sin(0.3 * n);
    const double yw = w.process(x), ym = m.process(x);
    maxWire = std::max(maxWire, std::fabs(yw - ym));
  }
  std::printf("%s: SYM 2 0.6, VC>SYM 3 0.5: wires (stage 1 2 3) steady %d%d%d, moving %d%d%d; VC>AMT on 3: live %d%d%d, "
              "not live %d%d%d; moving block vs wire max diff %.3e (ADAA half-sample delay kept)\n",
              t, steady >> 2 & 1, steady >> 1 & 1, steady & 1, moving >> 2 & 1, moving >> 1 & 1, moving & 1,
              vcLive >> 2 & 1, vcLive >> 1 & 1, vcLive & 1, vcOff >> 2 & 1, vcOff >> 1 & 1, vcOff & 1, maxWire);
  tu::truth(t, "steady block: a = 0 stages are wires at any SYM / VC>SYM", steady == 3);
  tu::truth(t, "moving block: no stage skipped", moving == 0);
  tu::truth(t, "live VC with AMT depth keeps the stage", vcLive == 2 && vcOff == 3);
  tu::truth(t, "steady SYM-only stage is a wire", wWire);
  tu::truth(t, "moving block keeps ADAA on a = 0 stages", maxWire > 1e-3);
}

// Amount 0, any SYM (jidai-common 1.1.2): WAVE 0 with the trims and live VC > AMT at 0 is a full bypass whatever SYM 1-3
// and VC > SYM 1-3 are, steady or not, LEVEL COMP on or off, at 1x/2x/4x: bit-identical to the input and to the
// default bypass, LEVEL COMP never touched. A VC > AMT depth with no live VC is still a bypass.
void testWaveAmt0AnySym() {
  const char* t = "testWaveAmt0AnySym";
  const double syms[5][3] = {{1.0, -1.0, 0.5}, {-0.37, 0.0, 0.81}, {0.0, 0.0, 1e-9}, {-1.0, -1.0, -1.0}, {0.25, 0.6, -0.9}};
  int cases = 0, bypassAll = 0;
  bool same = true, gainOne = true;
  for (double fs : {48000.0, 96000.0, 192000.0}) {
    for (int k = 0; k < 5; ++k) {
      for (int mode = 0; mode < 4; ++mode) {
        TripleShaper ref, s;
        ref.prepare(fs);
        s.prepare(fs);
        TripleShaperParams p;  // default: WAVE 0, every trim / SYM / depth 0
        p.levelComp = (mode & 1) == 0;
        ref.setParams(p);
        for (int i = 0; i < 3; ++i) {
          p.sym[i] = syms[k][i];
          p.vcSym[i] = syms[(k + 2) % 5][i];
        }
        if (mode >= 2) p.vcAmt[1] = 0.7;  // AMT depth with no live VC
        const bool vcLive = mode < 2;
        bool b = true;
        for (int n = 0; n < 4800; ++n) {
          // SYM moves every 64 samples (a non-steady block for SYM only); VC is a live audio-rate signal.
          if (n % 64 == 0) {
            for (int i = 0; i < 3; ++i) p.sym[i] = -p.sym[i] * 0.97;
            s.setParams(p, (n / 64) % 2 == 0, vcLive);
            b = b && s.bypassed();
          }
          const double x = 0.8 * std::sin(0.013 * n * 48000.0 / fs) * std::exp(-n / 2000.0);
          const double vc = std::sin(0.21 * n);
          const double y = s.process(x, vc), yr = ref.process(x, vc);
          same = same && tu::same(y, x) && tu::same(y, yr);
        }
        gainOne = gainOne && tu::same(s.levelGain(), 1.0);
        bypassAll += b ? 1 : 0;
        ++cases;
      }
    }
  }
  // The edge: a live VC with AMT depth, or any trim, is not a bypass.
  TripleShaper e;
  e.prepare(96000.0);
  TripleShaperParams p;
  p.sym[0] = 0.5;
  p.vcAmt[0] = 0.1;
  e.setParams(p, true, true);
  const bool vcAmtLive = !e.bypassed();
  p.vcAmt[0] = 0.0;
  p.trim[2] = 0.01;
  e.setParams(p, true, true);
  const bool trimLive = !e.bypassed();
  std::printf("%s: WAVE 0, trims 0, SYM / VC>SYM / idle VC>AMT set: %d/%d cases bypassed, output bit-identical to input "
              "and to default bypass %d, level gain untouched %d; live VC>AMT bypass %d, trim 3 0.01 bypass %d\n",
              t, bypassAll, cases, same ? 1 : 0, gainOne ? 1 : 0, vcAmtLive ? 0 : 1, trimLive ? 0 : 1);
  tu::truth(t, "every case bypassed", bypassAll == cases && cases == 60);
  tu::truth(t, "bit-identical to bypass", same);
  tu::truth(t, "level comp skipped", gainOne);
  tu::truth(t, "live VC > AMT is not a bypass", vcAmtLive);
  tu::truth(t, "a trim is not a bypass", trimLive);
}

void testWaveAudioRateVc() {
  const char* t = "testWaveAudioRateVc";
  const int q = 44, F1 = 4 * q, F2 = 21 * q;
  // VC -> SYM at WAVE 0.5: stage 3 sits at a = 0, so under jidai-common 1.1.2 it is a wire at any SYM. Under 1.1.1 it
  // ran ADAA on the identity, whose half-sample average took the alias to -115.3 dB; verify_folder.py with the 1.1.2
  // skip rule (a = 0 only) gives -113.7 dB, the figure here.
  struct Row {
    double dA, dB, limit, verify;
    const char* name;
  } rows[3] = {{0.5, 0.0, -57.4, -58.4, "VC -> AMT"}, {0.0, 0.5, -112.7, -113.7, "VC -> SYM"},
               {0.5, 0.5, -49.8, -50.8, "VC -> AMT + SYM"}};
  for (const auto& r : rows) {
    TripleShaper s;
    s.prepare(96000.0);
    TripleShaperParams p;
    p.macro = 0.5;
    p.levelComp = false;
    for (int i = 0; i < 3; ++i) {
      p.vcAmt[i] = r.dA;
      p.vcSym[i] = r.dB;
    }
    s.setParams(p);
    const auto y = renderShaper(s, F1, 0.8, 2, F2);
    const double a = tu::aliasLinesDb(y, 2, 32768, 48000.0, q);
    std::printf("%s: body 257.8 Hz, VC 1353.5 Hz, %s depth 0.5: 2x ADAA alias %.1f dB (verify %.1f)\n", t, r.name, a,
                r.verify);
    tu::atMost(t, r.name, a, r.limit);
  }
  // No smoother on the VC path: sideband energy re harmonics equals the unsmoothed reference (+1.0 dB in verify).
  TripleShaper s;
  s.prepare(96000.0);
  TripleShaperParams p;
  p.macro = 0.5;
  p.levelComp = false;
  for (int i = 0; i < 3; ++i) p.vcAmt[i] = 0.5;
  s.setParams(p);
  const auto y = renderShaper(s, F1, 0.8, 2, F2);
  auto Y = tu::rfftMag(y);
  double sb = 0.0, h = 0.0;
  for (int k = 1; k <= 32768 / 2; ++k) {
    const double pw = Y[static_cast<size_t>(k)] * Y[static_cast<size_t>(k)];
    if (k % q == 0 && k % F1 != 0) sb += pw;
    if (k % F1 == 0) h += pw;
  }
  const double sideDb = 10.0 * std::log10(sb / h);
  std::printf("%s: VC sideband energy re harmonics %.1f dB (unsmoothed reference +1.0, a 5 ms smoother gives -35.8)\n",
              t, sideDb);
  tu::near(t, "sidebands vs unsmoothed reference", sideDb, 1.0, 0.5);
}

void testWaveLevelComp() {
  const char* t = "testWaveLevelComp";
  const double fs = 96000.0;
  const int n = static_cast<int>(0.25 * fs), k0 = static_cast<int>(0.15 * fs);
  double lo = 1e9, hi = -1e9, loOff = 1e9, hiOff = -1e9;
  for (int trial = 0; trial < 200; ++trial) {
    const double* c = kLevelCompCases[trial];  // verify_folder.py's exact draws
    TripleShaperParams p;
    for (int i = 0; i < 3; ++i) {
      p.trim[i] = c[i];
      p.sym[i] = c[3 + i];
    }
    const double amp = c[6], f = c[7];
    for (int comp = 0; comp < 2; ++comp) {
      TripleShaper s;
      s.prepare(fs);
      p.levelComp = comp == 1;
      s.setParams(p);
      TptOnePole dc;
      dc.set(8.0, fs);
      double sx = 0.0, sy = 0.0;
      for (int i = 0; i < n; ++i) {
        const double x = amp * std::sin(2.0 * kPi * f * i / fs);
        const double y = dc.hp(s.process(x));
        if (i >= k0) {
          sx += x * x;
          sy += y * y;
        }
      }
      const double d = 10.0 * std::log10(sy / sx);
      if (comp) {
        lo = std::fmin(lo, d);
        hi = std::fmax(hi, d);
      } else {
        loOff = std::fmin(loOff, d);
        hiOff = std::fmax(hiOff, d);
      }
    }
  }
  std::printf("%s: 200 random settings, output re input RMS: comp OFF %.1f..%.1f dB, comp ON %.2f..%.2f dB\n", t, loOff,
              hiOff, lo, hi);
  tu::atLeast(t, "comp ON min", lo, -2.0);
  tu::atMost(t, "comp ON max", hi, 0.2);
}

void testWavePreVca() {
  const char* t = "testWavePreVca";
  const double fs = 96000.0, f = 110.0;
  const int T = static_cast<int>(0.6 * fs);
  Resonator r;
  r.set(f, 0.25, fs);
  r.kick(1.0);
  std::vector<double> xpre(T), env(T), post(T), lp(T);
  for (int n = 0; n < T; ++n) {
    r.tick();
    const double e = std::fmin(1.0, n / (0.002 * fs)) * (n < 0.15 * fs ? 1.0 : std::exp(-(n - 0.15 * fs) / (0.05 * fs)));
    env[static_cast<size_t>(n)] = e;
    lp[static_cast<size_t>(n)] = r.lp();
    xpre[static_cast<size_t>(n)] = 0.9 * TripleShaper::preVcaInput(r.lp(), TripleShaper::quadAmp(r.lp(), r.bp(), r.svf.R), e);
  }
  const int win = static_cast<int>(fs / f) + 1;
  double dev = 0.0, pre250 = 0.0;
  for (int i = 0; i + win < T; i += win) {
    double pk = 0.0;
    for (int j = i; j < i + win; ++j) pk = std::fmax(pk, std::fabs(xpre[static_cast<size_t>(j)]));
    dev = std::fmax(dev, std::fabs(pk - 0.9 * env[static_cast<size_t>(i + win / 2)]));
    if (i / win == static_cast<int>(0.25 * fs) / win) pre250 = pk;
  }
  std::printf("%s: PRE-VCA folder-input peak vs envelope, max deviation %.4f FS (verify 0.036); at 250 ms %.3f\n", t, dev,
              pre250);
  tu::atMost(t, "peak tracks envelope", dev, 0.04);
  // POST: the shaper input is the body itself (unchanged from v2.1).
  tu::truth(t, "POST input is the body", tu::same(TripleShaper::shapeMorph(lp[1000], 0.2, 0.01, 0.0, 0.001), lp[1000]));
}

void testShapeMorph() {
  const char* t = "testShapeMorph";
  // 398 Hz saw (SHAPE 1) at 2x, ideal decimation, BLEP on.
  const int N = 32768, M = 2, bins = 272;
  const double dt = static_cast<double>(bins) / (N * M);
  std::vector<double> y(2 * N * M);
  double ph = 0.25;
  for (size_t n = 0; n < y.size(); ++n) {
    y[n] = TripleShaper::morphAt(std::sin(2.0 * kPi * ph), 1.0, ph, 1.0, dt);
    ph += dt;
  }
  const double sawA = tu::aliasLinesDb(std::vector<double>(y.begin() + N * M, y.end()), M, N, 48000.0, bins);
  for (size_t n = 0; n < y.size(); ++n) y[n] = TripleShaper::morphAt(std::sin(2.0 * kPi * (0.25 + static_cast<double>(n) * dt)), 1.0, 0.25 + static_cast<double>(n) * dt, 0.5, dt);
  const double triA = tu::aliasLinesDb(std::vector<double>(y.begin() + N * M, y.end()), M, N, 48000.0, bins);
  std::printf("%s: 398 Hz at 2x: SHAPE 1.0 (saw) alias %.1f dB (verify -62.2), SHAPE 0.5 (tri) %.1f dB (verify -109.1)\n",
              t, sawA, triA);
  tu::atMost(t, "saw alias", sawA, -62.0);
  double worst = 0.0;
  for (double f : {35.0, 70.0, 400.0, 1000.0}) {
    const double fs = 96000.0, tau = 0.3;
    Resonator r;
    r.set(f, tau, fs);
    r.kick(1.0);
    double kmin = 1e9, kmax = 0.0, ksum = 0.0;
    int cnt = 0;
    for (int n = 0; n < static_cast<int>(0.2 * fs); ++n) {
      r.tick();
      if (n < static_cast<int>(0.01 * fs)) continue;
      const double k = TripleShaper::quadAmp(r.lp(), r.bp(), r.svf.R) / std::exp(-n / (tau * fs));
      kmin = std::fmin(kmin, k);
      kmax = std::fmax(kmax, k);
      ksum += k;
      ++cnt;
    }
    const double rip = 100.0 * (kmax - kmin) / (ksum / cnt);
    worst = std::fmax(worst, rip);
    std::printf("%s: quadrature amplitude ripple at %.0f Hz (2x of 48k): %.3f %%\n", t, f, rip);
  }
  tu::atMost(t, "quadrature ripple 35..1000 Hz", worst, 1.0);
}

}  // namespace

void runBlockTests() {
  testDecayIsRateIndependent();
  testVoiceEndsAtMinus90Envelope();
  testSvfMinus3dB();
  testResonatorTau();
  testResonatorPeak();
  testRetriggerNoClick();
  testLadderResponse();
  testLadderSelfOsc();
  testOtaSvfBounded();
  testHalfbandSpec();
  testDecimatorDelay();
  testBlepAlias();
  testTanhAdaaAlias();
  testOuDriftStd();
  testSmoothing();
  testLevelLawAndCentrePan();
  testGateHysteresis();
  testWaveBypass();
  testWaveMigration();
  testWaveStageLaw();
  testWaveAliasStatic();
  testWaveAudioRateVc();
  testWavePlanBlock();
  testWaveAmt0AnySym();
  testWaveLevelComp();
  testWavePreVca();
  testShapeMorph();
}
