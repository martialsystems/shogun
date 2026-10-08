#pragma once
// Engine-level test helpers (TESTPLAN.md style).
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "shogun.h"
#include "testutil.h"

namespace rig {
using namespace shogun;

inline std::unique_ptr<Engine> make(double fs = 48000.0, int os = 2) {
  auto e = std::make_unique<Engine>();
  if (!tu::same(fs, 48000.0) || os != 2) e->prepare(fs, os);
  e->setIdeal();
  return e;
}
inline void cc(Engine& e, int p, int v) { e.setParamNow(p, v / 127.0); }
inline void run(Engine& e, long n) {
  for (long i = 0; i < n; ++i) e.processSample();
}
// The old BD1 example knobs (TESTPLAN): Attack 64, Decay 80, Pitch 40, Tune 50, Noise 0, Filter 64, Dist 0.
inline void bd1Example(Engine& e) {
  cc(e, P_BD1_ATTACK, 64);
  cc(e, P_BD1_DECAY, 80);
  cc(e, P_BD1_PITCH, 40);
  cc(e, P_BD1_TUNE, 50);
  cc(e, P_BD1_NOISE, 0);
  cc(e, P_BD1_FILTER, 64);
  cc(e, P_BD1_DRIVE, 0);
  e.setParamNow(P_BD1_SOUND, stepU(0, 16));
}
// Plays only `v` through the main with the other voices' levels at 0 (they are silent anyway when untriggered).
inline double decayTauMs(double u) { return 8.0 * std::exp(4.5 * u); }

// End sample of a −90 dB RC decay (§3.7): sub-sample attack k_A (0 = instant) + decay k_D, in base samples.
inline long endSample(double tau, double fs, int M, double tauA = 0.0, long openSub = 0) {
  const double fsE = fs * M;
  long kA = 0;
  if (tauA > 0.0) kA = static_cast<long>(std::ceil(std::log(6.0) * tauA * fsE));
  const long kD = static_cast<long>(std::ceil(std::log(1.0 / dsp::kQuiet) * tau * fsE));
  return static_cast<long>(std::ceil(static_cast<double>(openSub + kA + kD) / M));
}
// Runs until the voice ends; returns the end sample: the first base sample n (0 = trigger sample) whose voice output
// is exactly 0 (the sample after the last rendered one, where the envelope is under −90 dB).
inline long runToEnd(Engine& e, int v, long limit) {
  for (long n = 0; n < limit; ++n) {
    e.processSample();
    if (!e.voiceActive(v)) return n + 1;
  }
  return -1;
}
inline int outPort(int v) { return isDrum(v) ? drumPort(v, DJ_OUT) : synthPort(v - LEAD, SJ_OUT); }

// A graph-style harness: values/connected arrays the caller fills (inputs) and reads (outputs).
struct Graph {
  float vals[kPorts] = {};
  bool con[kPorts] = {};
  void clearInputs() {
    for (int i = 0; i < kPorts; ++i)
      if (kPortTable[i].dir == PortDir::In) vals[i] = con[i] ? vals[i] : kPortTable[i].rest;
  }
  void step(Engine& e) { e.processSample(vals, con); }
};

}  // namespace rig
