#pragma once

// Shared STAND-IN helpers from SCHEMATICS.md.
// Sample rate is 48 kHz. Voice samples use the printed scale, about ±1.
// Trig gates are a separate 5 V domain.

#include <cmath>
#include <cstdint>

namespace shogun {

constexpr double kFs = 48000.0;
constexpr double kPi = 3.141592653589793;
constexpr double kNyquist = kFs * 0.5;
constexpr int kVoiceCount = 16;
constexpr int kMaxSteps = 32;
constexpr int kMaxEvents = 512;
constexpr double kTrigThreshold = 1.0;
// A drum voice whose envelopes are all under this ends, and outputs 0 until the next trigger.
constexpr double kQuietEnv = 1e-3;
// The drum decay law: tau = 8 ms * exp(4.5 u). u = 0 is 8 ms, u = 0.5 is 75.9 ms, u = 1 is 720 ms.
constexpr double kDecayBase = 0.008;
constexpr double kDecaySpan = 4.5;
// SD step bend: its envelope time is the Pitch time, but never under 80 ms while a bend is set. STAND-IN.
constexpr double kSdBendFloor = 0.08;
// A tom at Decay 127 rings for 4 s, then releases on a 50 ms time constant and ends. STAND-IN.
constexpr int kTomRingSamples = 4 * 48000;
constexpr double kTomRelease = 0.05;
// Equal-power centre: sqrt(0.5) on each side.
constexpr double kCenterGain = 0.7071067811865476;

inline double u(int cc) {
  if (cc < 0) cc = 0;
  if (cc > 127) cc = 127;
  return static_cast<double>(cc) / 127.0;
}

inline double gAccent(int index) {
  if (index <= 0) return 0.55;
  if (index == 1) return 0.78;
  return 1.0;
}

// velocity < 0 means a gate with no velocity byte: use 127.
inline double gVel(int velocity) {
  if (velocity < 0) velocity = 127;
  if (velocity > 127) velocity = 127;
  return 0.15 + 0.85 * (static_cast<double>(velocity) / 127.0);
}

inline int clampCc(int cc) {
  if (cc < 0) return 0;
  if (cc > 127) return 127;
  return cc;
}

inline int soundIndex(int triggerCc) {
  const int cc = clampCc(triggerCc);
  int s = static_cast<int>(std::floor(static_cast<double>(cc) * 16.0 / 128.0));
  if (s < 0) s = 0;
  if (s > 15) s = 15;
  return s;
}

inline int clapCount(int dataCc) {
  const int cc = clampCc(dataCc);
  int n = 1 + static_cast<int>(std::floor(static_cast<double>(cc) * 8.0 / 128.0));
  if (n < 1) n = 1;
  if (n > 8) n = 8;
  return n;
}

inline double noiseDraw(std::uint32_t& state) {
  state = 1664525u * state + 1013904223u;
  return static_cast<double>(state) / 4294967296.0 * 2.0 - 1.0;
}

inline double onePole(double& state, double x, double fc) {
  const double a = std::exp(-2.0 * kPi * fc / kFs);
  state = (1.0 - a) * x + a * state;
  return state;
}

inline double metalPartial(double freq, int n) {
  const double wn = 2.0 * kPi * static_cast<double>(n) / kFs;
  double s = std::sin(wn * freq);
  double w = 1.0;
  if (3.0 * freq < kNyquist) {
    s += (1.0 / 3.0) * std::sin(wn * 3.0 * freq);
    w = 1.0 + 1.0 / 3.0;
  }
  return s / w;
}

inline double metalStack(const double ratios[4], double f0, int n) {
  double mean = 0.0;
  for (int i = 0; i < 4; ++i) mean += metalPartial(f0 * ratios[i], n);
  return mean / 4.0;
}

inline double squareWave(double freq, int n) {
  const double wn = 2.0 * kPi * static_cast<double>(n) / kFs;
  double acc = 0.0;
  double weight = 0.0;
  for (int k = 1; k <= 15; k += 2) {
    const double kf = static_cast<double>(k) * freq;
    if (kf >= kNyquist) break;
    const double amp = 1.0 / static_cast<double>(k);
    acc += amp * std::sin(wn * kf);
    weight += amp;
  }
  if (weight == 0.0) return 0.0;
  return acc / weight;
}

// phase is radians of the fundamental and already includes n.
inline double sawSample(double phase, double freq) {
  double acc = 0.0;
  double weight = 0.0;
  for (int k = 1; k <= 8; ++k) {
    if (static_cast<double>(k) * freq >= kNyquist) break;
    const double amp = 1.0 / static_cast<double>(k);
    acc += amp * std::sin(static_cast<double>(k) * phase);
    weight += amp;
  }
  if (weight == 0.0) return 0.0;
  return acc / weight;
}

// Shaped body: a sine bent toward square, then soft-clipped. tanh(k sin) / tanh(k), so the peak stays 1 at any k.
// k = 1 is a gentle bend that already has odd harmonics; k = 6 is close to a square. STAND-IN.
constexpr double kWaveMin = 1.0;
constexpr double kWaveSpan = 5.0;
constexpr double kBd2Wave = 2.0;
constexpr double kSdWave = 2.0;
constexpr double kTomWave = 1.5;
inline double shapedSine(double phase, double k) { return std::tanh(k * std::sin(phase)) / std::tanh(k); }

// Slow FM on BD2 and the toms: f = f0 + i * sin(2 pi fm t). STAND-IN depths, in units of f0.
constexpr double kBd2FmIndex = 0.12;
constexpr double kTomFmIndex = 0.04;
constexpr double kTomFmHz = 50.0;

inline double frac(double x) { return x - std::floor(x); }

// PolyBLEP correction for a step at phase 0, t and dt in cycles.
inline double polyBlep(double t, double dt) {
  if (t < dt) {
    const double x = t / dt;
    return x + x - x * x - 1.0;
  }
  if (t > 1.0 - dt) {
    const double x = (t - 1.0) / dt;
    return x * x + x + x + 1.0;
  }
  return 0.0;
}

// Band-limited square, +1 then -1, phase from the sample count so it starts at 0 on the trigger.
inline double blepSquare(double freq, int n) {
  const double dt = freq / kFs;
  const double t = frac(dt * static_cast<double>(n));
  double y = t < 0.5 ? 1.0 : -1.0;
  y += polyBlep(t, dt);
  y -= polyBlep(frac(t + 0.5), dt);
  return y;
}

// Six squares at inharmonic ratios, the metal of the hats and cymbal. STAND-IN ratios.
constexpr double kMetalRatios[6] = {1.0, 1.4471, 1.6170, 1.9265, 2.5028, 2.6637};
inline double metalSquares(double f0, int n) {
  double acc = 0.0;
  for (int i = 0; i < 6; ++i) acc += blepSquare(f0 * kMetalRatios[i], n);
  return acc / 6.0;
}

// RBJ band-pass, 0 dB peak. z holds x1, x2, y1, y2.
inline double bandPass(double z[4], double x, double fc, double q) {
  const double w0 = 2.0 * kPi * fc / kFs;
  const double alpha = std::sin(w0) / (2.0 * q);
  const double a0 = 1.0 + alpha;
  const double y = (alpha * x - alpha * z[1] + 2.0 * std::cos(w0) * z[2] - (1.0 - alpha) * z[3]) / a0;
  z[1] = z[0];
  z[0] = x;
  z[3] = z[2];
  z[2] = y;
  return y;
}

inline double midiHz(int note) {
  return 440.0 * std::pow(2.0, (static_cast<double>(note) - 69.0) / 12.0);
}

inline double decayTau(double u) { return kDecayBase * std::exp(kDecaySpan * u); }

inline double expDecay(int n, double tau) {
  return std::exp(-static_cast<double>(n) / (kFs * tau));
}

inline int flamHits(int index) {
  if (index < 0) index = 0;
  if (index > 15) index = 15;
  return 2 + (index % 4);
}

inline int flamGap(int index) {
  if (index < 0) index = 0;
  if (index > 15) index = 15;
  return 180 * (1 + index / 4);
}

inline bool voiceHasBend(int voice) {
  // BD1, BD2, SD, LTC, MTC, HTC. Enum order is fixed in shogun.h.
  return voice == 0 || voice == 1 || voice == 2 || voice == 9 || voice == 10 || voice == 11;
}

}  // namespace shogun
