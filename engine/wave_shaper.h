#pragma once
// SHOGUN WAVE block (spec v2.2 §4.6) on the shared jidai::dsp::TripleShaper (third_party/jidai-common, see
// VENDORED.md). The stage law, ADAA stages, macro stagger, ShaperControls, LevelComp and DcBlocker are the shared
// ones. Only SHOGUN-specific pieces live here:
//   - TripleShaperParams / setParams(): the voice-facing parameter set (incl. SHAPE, ROUTING, LEVEL COMP switch);
//   - SHAPE morph from resonator quadrature or an oscillator phase (shapeMorph, morphAt, quadAmp);
//   - PRE-VCA input normalisation (preVcaInput);
//   - the exact v2.1 single-stage migration (migrateV21, v21Law).
//
// Two behaviours are kept from the SHOGUN tests / verify_folder.py where the shared header differs (do not patch the
// vendored copy; see TESTPLAN.md "jidai-common"):
//   1. Wire rule. A stage is a wire when its STATIC amount and symmetry are 0 and no VC depth is routed to it
//      (verify_folder.py skips a stage only when its a and b are 0 over the whole render). The shared AdaaStage
//      skips per sample whenever the current (a, b) = (0, 0), so with FOLD VC routed, a stage whose amount the VC
//      clamps to 0 would toggle the half-sample ADAA delay at audio rate. Each stage therefore runs in its own
//      shared TripleShaper (the other two stages are exact wires there), driven by the shared
//      process(x, ShaperControls, vc). On a sample where a non-wire stage hits (0, 0), its output is replaced by
//      the ADAA value from the shared antiderivative ShaperStage::F. The shared stage still records x[n-1]
//      correctly on that sample.
//   2. Bypass. WAVE 0 with every trim, SYM, SHAPE and VC depth at 0 returns the input bit for bit, updates the
//      stage histories, and skips LEVEL COMP (LevelComp::track() is not called: §4.6 "LEVEL COMP skipped").
// LEVEL COMP is the shared LevelComp fed by a shared 8 Hz DcBlocker on the wet path. It has the same detectors as
// before. The detector floor is 1e-30 on both powers; the old local copy floored only the wet power, at 1e-12.

#include <jidai/dsp/TripleShaper.h>

#include <cmath>

namespace shogun {

struct TripleShaperParams {
  double macro = 0.0;                 // front WAVE knob m ∈ [0,1], after the mod/CV sum
  double trim[3] = {0.0, 0.0, 0.0};   // WAVE 1/2/3 per-stage amount trims ∈ [−1,1] (incl. matrix rows)
  double sym[3] = {0.0, 0.0, 0.0};    // SYM 1/2/3 ∈ [−1,1] (incl. matrix rows)
  double vcAmt[3] = {0.0, 0.0, 0.0};  // VC → AMT depth dA_i ∈ [−1,1]
  double vcSym[3] = {0.0, 0.0, 0.0};  // VC → SYM depth dB_i ∈ [−1,1]
  double shape = 0.0;                 // SHAPE ∈ [0,1], sine → triangle → saw (applied with shapeMorph before process)
  bool preVca = false;                // ROUTING: false = POST, true = PRE-VCA (caller applies preVcaInput)
  bool levelComp = true;              // LEVEL COMP: ON for new patches, OFF for migrated ones
};

class WaveShaper {
 public:
  static constexpr double kPi = jidai::dsp::kPi;
  static constexpr const double (&kK)[3] = jidai::dsp::kShaperK;

  void prepare(double fsEff) {
    comp_.prepare(fsEff);
    dc_.prepare(fsEff, 8.0);  // 8 Hz DC block on the wet detector path
    reset();
  }
  void reset() {
    for (int i = 0; i < 3; ++i) {
      st_[i].reset();
      xin_[i] = 0.0;
    }
    comp_.reset();
    dc_.reset();
  }

  void setParams(const TripleShaperParams& p) {
    p_ = p;
    // Full controls (bypass test, documentation) and one isolated control set per stage: stage i alone is live in
    // st_[i]; a_i = clamp01((0 + c_i) + trim_i + vc·dA_i), b_i = clampSym(sym_i + 0 + vc·dB_i), bit-equal to the
    // single-instance sum because the other terms are exact zeros.
    ctl_ = jidai::dsp::ShaperControls{};
    ctl_.macro = p.macro;
    double c[3];
    jidai::dsp::macroAmounts(p.macro, c);
    for (int i = 0; i < 3; ++i) {
      ctl_.trim[i] = p.trim[i];
      ctl_.sym[i] = p.sym[i];
      ctl_.vcToAmt[i] = p.vcAmt[i];
      ctl_.vcToSym[i] = p.vcSym[i];
      jidai::dsp::ShaperControls& s = stageCtl_[i];
      s = jidai::dsp::ShaperControls{};
      s.trim[i] = c[i];
      s.modAmt[i] = p.trim[i];
      s.sym[i] = p.sym[i];
      s.vcToAmt[i] = p.vcAmt[i];
      s.vcToSym[i] = p.vcSym[i];
      wire_[i] = c[i] + p.trim[i] <= 0.0 && p.sym[i] == 0.0 && p.vcAmt[i] == 0.0 && p.vcSym[i] == 0.0;
    }
    bypass_ = !(p.shape > 0.0) && ctl_.isBypass(true);
  }
  const TripleShaperParams& params() const { return p_; }
  const jidai::dsp::ShaperControls& controls() const { return ctl_; }

  bool bypassed() const { return bypass_; }

  // One sample at fsEff. x: body (after SHAPE / PRE-VCA), vc: audio-rate VC (unsmoothed, V/5).
  double process(double x, double vc = 0.0) {
    if (bypass_) {
      for (int i = 0; i < 3; ++i) {
        st_[i].process(x, kWireCtl, 0.0);  // every stage a wire: records x[n-1] = x
        xin_[i] = x;
      }
      return x;
    }
    const double in = x;
    double y = x;
    for (int i = 0; i < 3; ++i) {
      const double xi = y;
      y = st_[i].process(xi, stageCtl_[i], vc);
      if (!wire_[i] && stageA(i, vc) == 0.0 && stageB(i, vc) == 0.0) y = adaaAtZero(xi, xin_[i], kK[i]);
      xin_[i] = xi;
    }
    if (!p_.levelComp) return y;
    // LEVEL COMP: C = sqrt(<x²>/<y²>), clamped to ±12 dB, smoothed 5 ms; the wet detector reads y after an 8 Hz DC
    // block (the asymmetric DC from SYM is removed by the voice's own DC block after the chain).
    const double yac = dc_.process(y);
    return y * comp_.gainFor(in * in, yac * yac);
  }

  bool stageIsWire(int i) const { return wire_[i]; }
  double stageA(int i, double vc = 0.0) const {
    return clamp(macroStage(i, p_.macro) + p_.trim[i] + vc * p_.vcAmt[i], 0.0, 1.0);
  }
  double stageB(int i, double vc = 0.0) const { return clamp(p_.sym[i] + vc * p_.vcSym[i], -1.0, 1.0); }
  double levelGain() const { return comp_.gain(); }

  // ---------------------------------------------------------------- pure math (shared law)
  static double clamp(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }
  static double macroStage(int i, double m) {
    double c[3];
    jidai::dsp::macroAmounts(m, c);
    return c[i];
  }
  static double stage(double x, double a, double b, double K) { return jidai::dsp::ShaperStage::f(x, a, b, K); }
  static double stageAntiderivative(double x, double a, double b, double K) {
    return jidai::dsp::ShaperStage::F(x, a, b, K);
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

  // ---------------------------------------------------------------- SHOGUN-only: SHAPE, PRE-VCA, migration
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
  // A live (non-wire) stage whose (a, b) is (0, 0) on this sample: first-order ADAA of the identity, computed with the
  // shared antiderivative exactly as for any other (a, b).
  static double adaaAtZero(double x, double xp, double K) {
    const double d = x - xp;
    if (std::fabs(d) < 1e-6) return stage(0.5 * (x + xp), 0.0, 0.0, K);
    return (stageAntiderivative(x, 0.0, 0.0, K) - stageAntiderivative(xp, 0.0, 0.0, K)) / d;
  }
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

  static inline const jidai::dsp::ShaperControls kWireCtl{};
  TripleShaperParams p_{};
  jidai::dsp::ShaperControls ctl_{};
  jidai::dsp::ShaperControls stageCtl_[3]{};
  jidai::dsp::TripleShaper st_[3];
  jidai::dsp::LevelComp comp_;
  jidai::dsp::DcBlocker dc_;
  double xin_[3] = {0.0, 0.0, 0.0};
  bool wire_[3] = {true, true, true};
  bool bypass_ = true;
};

}  // namespace shogun
