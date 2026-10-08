// Measures each voice's noon peak at g_vel = g_level = 1 (calib = 1) and prints calib = target / peak
// (§9.5, §15.5 step 1). Output is the body of engine/calib_table.inc.
#include <cmath>
#include <cstdio>

#include "shogun.h"

using namespace shogun;

int main(int argc, char**) {
  const bool table = argc > 1;
  double cal[kVoices];
  for (int v = 0; v < kVoices; ++v) {
    Engine e;
    e.setIdeal();
    e.prepare(48000.0, 2);
    e.setCalib(v, 1.0);
    const VoiceParams vp = voiceParams(v);
    e.setParamNow(vp.level, 1.0 / std::sqrt(1.4125375446227544));
    static float vals[kPorts];
    static bool con[kPorts];
    for (int i = 0; i < kPorts; ++i) {
      vals[i] = 0.0f;
      con[i] = false;
    }
    const int op = isDrum(v) ? drumPort(v, DJ_OUT) : synthPort(v - LEAD, SJ_OUT);
    con[op] = true;
    if (v >= LEAD) e.noteOn(v, v == LEAD ? 60.0 : 36.0, 5.0);
    else e.trigger(v, 5.0);
    double peak = 0.0;
    for (int n = 0; n < 96000; ++n) {
      if (v >= LEAD && n == 24000) e.noteOff(v);
      for (int i = 0; i < kPorts; ++i)
        if (kPortTable[i].dir == PortDir::In) vals[i] = 0.0f;
      e.processSample(vals, con);
      peak = std::fmax(peak, std::fabs(static_cast<double>(vals[op]) / 5.0));
    }
    cal[v] = kTargetPeak[v] / peak;
    if (!table) std::printf("%-5s peak %.6f target %.2f calib %.6f\n", kVoiceNames[v], peak, kTargetPeak[v], cal[v]);
  }
  if (table) {
    for (int v = 0; v < kVoices; ++v) std::printf("%.6f%s", cal[v], v + 1 < kVoices ? ", " : "\n");
  }
  return 0;
}
