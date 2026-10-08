#pragma once

// TripleShaper: three wave-shaper stages in series (SHOGUN spec v2.2 §4.6), self-contained, header-only.
//
// Shared-module candidate: this file depends only on <cmath> and lives in namespace jidai::dsp so that it can be
// lifted verbatim to the collection-wide location `jidai/dsp/TripleShaper.h` (ORIGAMI uses the same math). The
// include path inside SHOGUN is already "jidai/dsp/TripleShaper.h", so swapping to the shared copy is an include-path
// change only.
//
// Signal units: normalised, x = v / 5 V (one shaper unit = 5 V Jidai = 2.5 V in the ±2.5 V convention).
//
// Per stage i (a ∈ [0,1], b ∈ [−1,1], K = (4, 2, 2)):
//   g = 1 + K·a,  θ(x) = (π/2)·g·(x + b),  s(x) = sin θ(x) − sin θ(0),  y = (1 − a)·x + a·s(x)
// First-order ADAA per stage in double, both antiderivative terms with the CURRENT sample's (a, b), so audio-rate VC
// stays valid. A stage whose static amount and symmetry are 0 and that has no VC depth is a wire (no ADAA, no delay). WAVE = 0 with every trim, SYM, SHAPE and VC
// depth at 0 is a true bypass: process() returns its input bit for bit and LEVEL COMP is skipped.

#include <cmath>

namespace jidai {
namespace dsp {

struct TripleShaperParams {
  double macro = 0.0;                // front WAVE knob m ∈ [0,1], after the mod/CV sum
  double trim[3] = {0.0, 0.0, 0.0};  // WAVE 1/2/3 per-stage amount trims ∈ [−1,1] (incl. matrix rows)
  double sym[3] = {0.0, 0.0, 0.0};   // SYM 1/2/3 ∈ [−1,1] (incl. matrix rows)
  double vcAmt[3] = {0.0, 0.0, 0.0}; // VC → AMT depth dA_i ∈ [−1,1]
  double vcSym[3] = {0.0, 0.0, 0.0}; // VC → SYM depth dB_i ∈ [−1,1]
  double shape = 0.0;                // SHAPE ∈ [0,1], sine → triangle → saw (applied with shapeMorph before process)
  bool preVca = false;               // ROUTING: false = POST, true = PRE-VCA (caller applies preVcaInput)
  bool levelComp = true;             // LEVEL COMP: ON for new patches, OFF for migrated ones
};

class TripleShaper {
 public:
  static constexpr double kPi = 3.14159265358979323846;
  static constexpr double kK[3] = {4.0, 2.0, 2.0};

  void prepare(double fsEff) {
    fs_ = fsEff;
    aDet_ = 1.0 - std::exp(-1.0 / (0.020 * fs_));  // detectors τ = 20 ms
    aGain_ = 1.0 - std::exp(-1.0 / (0.005 * fs_));  // gain smoother τ = 5 ms
    const double g = std::tan(kPi * 8.0 / fs_);    // 8 Hz DC block on the wet detector path
    dcG_ = g / (1.0 + g);
    reset();
  }
  void reset() {
    for (int i = 0; i < 3; ++i) xp_[i] = 0.0;
    px_ = py_ = 1e-12;
    gs_ = 1.0;
    dcS_ = 0.0;
  }
  void setParams(const TripleShaperParams& p) { p_ = p; }
  const TripleShaperParams& params() const { return p_; }

  bool bypassed() const {
    if (p_.macro > 0.0 || p_.shape > 0.0) return false;
    for (int i = 0; i < 3; ++i) {
      if (p_.trim[i] != 0.0 || p_.sym[i] != 0.0 || p_.vcAmt[i] != 0.0 || p_.vcSym[i] != 0.0) return false;
    }
    return true;
  }

  // One sample at fsEff. x: body (after SHAPE / PRE-VCA), vc: audio-rate VC (unsmoothed, V/5).
  double process(double x, double vc = 0.0) {
    if (bypassed()) {
      for (int i = 0; i < 3; ++i) xp_[i] = x;
      return x;
    }
    const double in = x;
    double y = x;
    for (int i = 0; i < 3; ++i) {
      const double a = stageA(i, vc);
      const double b = stageB(i, vc);
      lastA_[i] = a;
      lastB_[i] = b;
      const double xi = y;
      if (stageIsWire(i)) {
        xp_[i] = xi;  // wire
        continue;
      }
      const double d = xi - xp_[i];
      if (std::fabs(d) < 1e-6) {
        y = stage(0.5 * (xi + xp_[i]), a, b, kK[i]);
      } else {
        y = (stageAntiderivative(xi, a, b, kK[i]) - stageAntiderivative(xp_[i], a, b, kK[i])) / d;
      }
      xp_[i] = xi;
    }
    if (!p_.levelComp) return y;
    // LEVEL COMP: C = sqrt(<x²>/<y²>), clamped to ±12 dB, smoothed 5 ms. The wet detector reads y after an 8 Hz
    // DC block (the asymmetric DC from SYM is removed by the voice's own DC block after the chain).
    const double v = (y - dcS_) * dcG_;
    const double lp = v + dcS_;
    dcS_ = lp + v;
    const double yac = y - lp;
    px_ += (in * in - px_) * aDet_;
    py_ += (yac * yac - py_) * aDet_;
    double g = std::sqrt(px_ / (py_ > 1e-12 ? py_ : 1e-12));
    if (g > 3.9810717055349722) g = 3.9810717055349722;
    if (g < 0.25118864315095801) g = 0.25118864315095801;
    gs_ += (g - gs_) * aGain_;
    return y * gs_;
  }

  // A stage is a wire (skipped, no ADAA delay) when its amount and symmetry are 0 with no VC depth routed to it.
  // Decided from the static settings, not per sample, so an audio-rate VC never toggles the half-sample ADAA delay.
  bool stageIsWire(int i) const {
    return macroStage(i, p_.macro) + p_.trim[i] <= 0.0 && p_.sym[i] == 0.0 && p_.vcAmt[i] == 0.0 && p_.vcSym[i] == 0.0;
  }
  double stageA(int i, double vc = 0.0) const {
    return clamp(macroStage(i, p_.macro) + p_.trim[i] + vc * p_.vcAmt[i], 0.0, 1.0);
  }
  double stageB(int i, double vc = 0.0) const { return clamp(p_.sym[i] + vc * p_.vcSym[i], -1.0, 1.0); }
  double levelGain() const { return gs_; }

  // ---------------------------------------------------------------- pure math
  static double clamp(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
  // Staggered macro: c1 = clamp(2m), c2 = clamp(2m − 0.5), c3 = clamp(2m − 1).
  static double macroStage(int i, double m) { return clamp(2.0 * m - 0.5 * i, 0.0, 1.0); }
  static double stage(double x, double a, double b, double K) {
    const double g = 1.0 + K * a;
    return (1.0 - a) * x + a * (std::sin(0.5 * kPi * g * (x + b)) - std::sin(0.5 * kPi * g * b));
  }
  static double stageAntiderivative(double x, double a, double b, double K) {
    const double g = 1.0 + K * a;
    return (1.0 - a) * x * x * 0.5 +
           a * (-(2.0 / (kPi * g)) * std::cos(0.5 * kPi * g * (x + b)) - x * std::sin(0.5 * kPi * g * b));
  }
  // The static (non-ADAA) chain, for migration checks and reference renders.
  static double chainStatic(double x, const TripleShaperParams& p, double vc = 0.0) {
    for (int i = 0; i < 3; ++i) {
      const double a = clamp(macroStage(i, p.macro) + p.trim[i] + vc * p.vcAmt[i], 0.0, 1.0);
      const double b = clamp(p.sym[i] + vc * p.vcSym[i], -1.0, 1.0);
      if (a == 0.0 && b == 0.0) continue;
      x = stage(x, a, b, kK[i]);
    }
    return x;
  }

  // SHAPE from a resonator's quadrature states (lp, bp, damping R): sine → triangle → saw, phase and amplitude
  // following the body exactly. dt = f/fsEff for the PolyBLEP/PolyBLAMP corrections. SHAPE 0 returns lp exactly.
  // The quadrature term is (bp + R·lp)/sqrt(1 − R²), which removes the damping-induced amplitude ripple.
  static double shapeMorph(double lp, double bp, double R, double shape, double dt) {
    if (shape <= 0.0) return lp;
    const double Rc = R < 0.99 ? R : 0.99;
    const double q = (bp + Rc * lp) / std::sqrt(1.0 - Rc * Rc);
    const double r = std::sqrt(lp * lp + q * q);
    const double phase = std::atan2(lp, q) / (2.0 * kPi);
    return morphAt(lp, r, phase, shape, dt);
  }
  // Same, from an oscillator phase (BD2 uses its VCO phase directly; r = 1, sine = its own sine).
  static double morphAt(double sine, double r, double phase, double shape, double dt) {
    if (shape <= 0.0) return sine;
    const double t = phase - std::floor(phase);
    const double tri = r * triAt(t, dt);
    if (shape <= 0.5) return (1.0 - 2.0 * shape) * sine + 2.0 * shape * tri;
    const double saw = r * sawAt(t, dt);
    return (2.0 - 2.0 * shape) * tri + (2.0 * shape - 1.0) * saw;
  }
  // Quadrature amplitude r = sqrt(lp² + q²) (PRE-VCA normalisation).
  static double quadAmp(double lp, double bp, double R) {
    const double Rc = R < 0.99 ? R : 0.99;
    const double q = (bp + Rc * lp) / std::sqrt(1.0 - Rc * Rc);
    return std::sqrt(lp * lp + q * q);
  }
  // PRE-VCA: x = (body'/max(r, 1e-6))·env.
  static double preVcaInput(double body, double r, double env) { return body / (r > 1e-6 ? r : 1e-6) * env; }

  // Exact migration from the v2.1 / sim single-stage WAVE w = v/127: m = w/2, WAVE 2 = −(w − 0.5) for w > 0.5,
  // SYM 0, SHAPE 0, LEVEL COMP OFF, ROUTING POST (PRE-VCA for BD2).
  static TripleShaperParams migrateV21(double w, bool bd2 = false) {
    TripleShaperParams p;
    p.macro = 0.5 * w;
    p.trim[1] = -macroStage(1, p.macro);
    p.levelComp = false;
    p.preVca = bd2;
    return p;
  }
  // The v2.1 single-stage law: (1 − w)·x + w·sin(π/2·(1 + 4w)·x).
  static double v21Law(double x, double w) { return (1.0 - w) * x + w * std::sin(0.5 * kPi * (1.0 + 4.0 * w) * x); }

 private:
  static double polyBlep(double t, double dt) {
    if (t < dt) {
      t /= dt;
      return t + t - t * t - 1.0;
    }
    if (t > 1.0 - dt) {
      t = (t - 1.0) / dt;
      return t * t + t + t + 1.0;
    }
    return 0.0;
  }
  static double polyBlamp(double t, double dt) {
    if (t < dt) {
      t = t / dt - 1.0;
      return -t * t * t / 3.0;
    }
    if (t > 1.0 - dt) {
      t = (t - 1.0) / dt + 1.0;
      return t * t * t / 3.0;
    }
    return 0.0;
  }
  static double triAt(double t, double dt) {
    double u = t + 0.25;
    u -= std::floor(u);
    double u2 = u + 0.5;
    u2 -= std::floor(u2);
    return (1.0 - 4.0 * std::fabs(u - 0.5)) + 4.0 * dt * (polyBlamp(u, dt) - polyBlamp(u2, dt));
  }
  static double sawAt(double t, double dt) {
    double v = t + 0.5;
    v -= std::floor(v);
    return 2.0 * v - 1.0 - polyBlep(v, dt);
  }

  TripleShaperParams p_{};
  double fs_ = 96000.0;
  double xp_[3] = {0.0, 0.0, 0.0};
  double lastA_[3] = {0.0, 0.0, 0.0};
  double lastB_[3] = {0.0, 0.0, 0.0};
  double aDet_ = 0.0, aGain_ = 0.0, dcG_ = 0.0, dcS_ = 0.0;
  double px_ = 1e-12, py_ = 1e-12, gs_ = 1.0;
};

}  // namespace dsp
}  // namespace jidai
