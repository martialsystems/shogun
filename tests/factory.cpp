// SHOGUN factory bank named tests (TESTPLAN.md "Factory bank"): every preset loads, save/reload is identical, each
// kit with its pattern renders 2 bars between -40 and -6 dBFS at 44.1 and 48 kHz, names are unique and clean.
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "factory.h"
#include "testutil.h"

using namespace shogun;
using tu::atLeast;
using tu::atMost;
using tu::truth;

namespace {

bool sameStep(const Step& a, const Step& b) {
  if (a.on != b.on || a.acc != b.acc || a.flam != b.flam || a.ratchet != b.ratchet || a.note != b.note ||
      a.tie != b.tie || a.nLocks != b.nLocks || !tu::same(a.prob, b.prob) || !tu::same(a.micro, b.micro) ||
      !tu::same(a.bend, b.bend))
    return false;
  for (int k = 0; k < a.nLocks; ++k)
    if (a.locks[k].param != b.locks[k].param || !tu::same(a.locks[k].u, b.locks[k].u)) return false;
  return true;
}
bool samePattern(const Pattern& a, const Pattern& b) {
  if (std::strcmp(a.name, b.name) != 0 || a.seed != b.seed) return false;
  for (int t = 0; t < 16; ++t) {
    const Track &x = a.tracks[t], &y = b.tracks[t];
    if (x.len != y.len || x.scale != y.scale || !tu::same(x.swing, y.swing) || !tu::same(x.shift, y.shift)) return false;
    for (int s = 0; s < kMaxSteps; ++s)
      if (!sameStep(x.steps[s], y.steps[s])) return false;
  }
  return true;
}
bool sameRows(const mod::Row* a, const mod::Row* b) {
  for (int i = 0; i < mod::kRows; ++i)
    if (a[i].src != b[i].src || a[i].srcVoice != b[i].srcVoice || a[i].dst != b[i].dst ||
        !tu::same(a[i].depth, b[i].depth) || a[i].via != b[i].via || a[i].viaVoice != b[i].viaVoice ||
        a[i].curve != b[i].curve || a[i].on != b[i].on)
      return false;
  return true;
}
// Engine state equal: every parameter, mod row, cable, CV AMT, input law and step.
bool sameEngine(const Engine& a, const Engine& b) {
  for (int i = 0; i < kParamCount; ++i)
    if (!tu::same(a.param(i), b.param(i))) return false;
  if (!sameRows(a.modulation().rows, b.modulation().rows) || a.cableCount() != b.cableCount()) return false;
  for (int i = 0; i < a.cableCount(); ++i) {
    int f1, t1, f2, t2;
    a.cable(i, f1, t1);
    b.cable(i, f2, t2);
    if (f1 != f2 || t1 != t2) return false;
  }
  for (int p = 0; p < kPorts; ++p)
    if (!tu::same(a.cvAmt(p), b.cvAmt(p)) || a.inputLaw(p) != b.inputLaw(p)) return false;
  return samePattern(a.pattern(), b.pattern());
}

double barSamples(const Engine& e, double fs) {
  const double bpm = 40.0 + 160.0 * e.param(P_CLOCK_TEMPO);
  const int bar = 1 + stepIndex(e.param(P_CLOCK_BAR), 32);
  return bar * 60.0 * fs / (bpm * stepsPerQuarter(stepIndex(e.param(P_CLOCK_SCALE), 4)));
}

void testFactoryPresetsLoad() {
  const char* T = "testFactoryPresetsLoad";
  int nParams = 0, nSteps = 0, nLocks = 0, nRows = 0, nFeat[6] = {}, acidBass = 0;
  bool allLoad = true, namesMatch = true, enginesMatch = true, noAlias = true;
  for (int pr = 0; pr < factory::kPrograms; ++pr) {
    Patch p;
    const bool ok = factory::loadProgram(pr, p);
    allLoad = allLoad && ok;
    char want[40];
    std::snprintf(want, sizeof want, "%03d %s", pr + 1, factory::programName(pr));
    namesMatch = namesMatch && std::strcmp(p.name, pr == 0 ? "001 INIT" : want) == 0 &&
                 std::strcmp(p.pattern.name, p.name) == 0;
    // Nothing depends on the alias table: no cables, no migrated input laws, CV AMT at 1.
    noAlias = noAlias && p.nCables == 0;
    for (int i = 0; i < kPorts; ++i) noAlias = noAlias && p.inLaw[i] == 0 && tu::same(p.cvAmt[i], 1.0);
    for (int i = 0; i < kParamCount; ++i) nParams += tu::same(static_cast<float>(p.u[i]), kParams[i].def) ? 0 : 1;
    for (const auto& r : p.rows) nRows += r.src != mod::SRC_NONE ? 1 : 0;
    for (const auto& tr : p.pattern.tracks)
      for (int s = 0; s < tr.len; ++s) {
        const Step& st = tr.steps[s];
        nSteps += st.on ? 1 : 0;
        nLocks += st.nLocks;
        nFeat[0] += st.ratchet > 1 ? 1 : 0;
        nFeat[1] += st.flam ? 1 : 0;
        nFeat[2] += st.prob < 1.0f ? 1 : 0;
        nFeat[3] += !tu::same(st.micro, 0.0f) ? 1 : 0;
        nFeat[4] += st.tie ? 1 : 0;
        nFeat[5] += !tu::same(st.bend, 0.0f) ? 1 : 0;
      }
    if (std::strstr(factory::programName(pr), "Acid") != nullptr)  // the acid kit carries an accented, sliding bass line
      for (int s = 0; s < p.pattern.tracks[BASS].len; ++s)
        acidBass += p.pattern.tracks[BASS].steps[s].on ? 1 : 0;
    Engine e;
    applyPatch(p, e);
    Patch back;
    capturePatch(e, back);
    enginesMatch = enginesMatch && sameParams(p, back) && samePattern(p.pattern, back.pattern) &&
                   sameRows(p.rows, back.rows);
  }
  std::printf("%s: %d programs (INIT + %d kits) load: %d parameters off INIT, %d steps (%d ratchets, %d flams, "
              "%d probability, %d micro-timed, %d ties, %d bends), %d p-locks, %d mod rows\n",
              T, factory::kPrograms, factory::kCount, nParams, nSteps, nFeat[0], nFeat[1], nFeat[2], nFeat[3],
              nFeat[4], nFeat[5], nLocks, nRows);
  truth(T, "every program parses", allLoad);
  truth(T, "16 to 24 factory kits after INIT", factory::kCount >= 16 && factory::kCount <= 24);
  truth(T, "pattern name is NNN + program name", namesMatch);
  truth(T, "engine holds the loaded patch", enginesMatch);
  truth(T, "no alias-table content (cables, input laws, CV AMT)", noAlias);
  atLeast(T, "Acid kit BASS steps", acidBass, 8);
  Patch init;
  factory::loadProgram(0, init);
  Engine fresh, e;
  fresh.loadInit();
  applyPatch(init, e);
  truth(T, "program 0 is INIT", sameEngine(fresh, e) && std::strcmp(factory::programName(0), "INIT") == 0);
}

void testFactoryRoundTrip() {
  const char* T = "testFactoryRoundTrip";
  bool canon = true, stateSame = true, engineSame = true;
  std::size_t bytes = 0;
  for (int pr = 0; pr < factory::kPrograms; ++pr) {
    Patch p;
    factory::loadProgram(pr, p);
    if (pr > 0) canon = canon && patchToJson(p, false) == factory::programJson(pr);  // the bank text is canonical
    // Save (every parameter, the plugin's state) then reload into a new engine, save again.
    Engine a;
    applyPatch(p, a);
    Patch s1;
    capturePatch(a, s1);
    const std::string saved = patchToJson(s1, true);
    Patch r;
    const bool ok = parsePatch(saved.c_str(), r);
    Engine b;
    applyPatch(r, b);
    Patch s2;
    capturePatch(b, s2);
    const std::string again = patchToJson(s2, true);
    stateSame = stateSame && ok && saved == again;
    engineSame = engineSame && sameEngine(a, b);
    bytes += saved.size();
  }
  std::printf("%s: %d programs saved (%zu bytes of state), reloaded and saved again\n", T, factory::kPrograms, bytes);
  truth(T, "bank text = canonical writer output", canon);
  truth(T, "save, reload, save: byte-identical", stateSame);
  truth(T, "every parameter, row and step equal after reload", engineSame);
}

void testFactoryRenderLevels() {
  const char* T = "testFactoryRenderLevels";
  double lo = 1e9, hi = -1e9;
  const char *loName = "", *hiName = "";
  bool loud = true, quiet = true;
  for (int pr = 1; pr < factory::kPrograms; ++pr) {
    Patch p;
    factory::loadProgram(pr, p);
    for (double fs : {44100.0, 48000.0}) {
      Engine e;
      e.prepare(fs, 2);
      applyPatch(p, e);
      e.setRunning(true);
      const long n = static_cast<long>(std::ceil(2.0 * barSamples(e, fs)));
      double pk = 0.0;
      for (long i = 0; i < n; ++i) {
        e.processSample();
        pk = std::fmax(pk, std::fmax(std::fabs(e.mainL()), std::fabs(e.mainR())));
      }
      const double db = pk > 0.0 ? tu::db(pk) : -999.0;
      if (db < lo) lo = db, loName = factory::programName(pr);
      if (db > hi) hi = db, hiName = factory::programName(pr);
      if (!(db > -40.0)) {
        loud = false;
        std::printf("%s: %s at %.0f Hz peaks %.2f dBFS (silent)\n", T, factory::programName(pr), fs, db);
      }
      if (!(db <= -6.0)) {
        quiet = false;
        std::printf("%s: %s at %.0f Hz peaks %.2f dBFS (over -6)\n", T, factory::programName(pr), fs, db);
      }
    }
  }
  std::printf("%s: %d kits x 44.1/48 kHz, 2 bars from a fresh engine, default master: peaks %.2f (%s) to %.2f dBFS "
              "(%s)\n",
              T, factory::kCount, lo, loName, hi, hiName);
  atLeast(T, "quietest kit peak (dBFS) above -40", lo, -40.0 + 1e-9);
  atMost(T, "loudest kit peak (dBFS) at or below -6", hi, -6.0);
  truth(T, "no kit silent", loud);
  truth(T, "no kit over -6 dBFS", quiet);
}

// Brand, gear, model-number and artist words the factory names must not carry (case-insensitive).
const char* const kBanned[] = {"808", "909", "707", "727", "606", "303", "101", "tr-", "tr8", "roland", "linn",
                               "tanzb", "tanzm", "mfb", "korg", "moog", "elektron", "oberheim", "dmx", "simmons",
                               "sequential", "serge", "buchla", "arturia", "behringer", "akai", "mpc", "sp-12",
                               "sp12", "e-mu", "emu ", "yamaha", "boss", "vermona", "jomox", "novation", "nord ",
                               "ableton", "native instr", "maschine", "dfam", "drm", "amen", "funky drummer",
                               "dilla", "aphex", "kraftwerk", "daft", "skrillex", "dubstep"};

void testFactoryNames() {
  const char* T = "testFactoryNames";
  bool unique = true, clean = true, fits = true;
  for (int a = 0; a < factory::kPrograms; ++a) {
    std::string n = factory::programName(a);
    for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    fits = fits && !n.empty() && n.size() <= 27;
    for (const char* w : kBanned)
      if (n.find(w) != std::string::npos) {
        clean = false;
        std::printf("%s: '%s' contains '%s'\n", T, factory::programName(a), w);
      }
    for (int b = a + 1; b < factory::kPrograms; ++b) {
      std::string m = factory::programName(b);
      for (char& c : m) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      unique = unique && n != m;
    }
  }
  std::printf("%s: %d names, %d banned words checked\n", T, factory::kPrograms,
              static_cast<int>(sizeof kBanned / sizeof kBanned[0]));
  truth(T, "names unique", unique);
  truth(T, "no brand, gear, model or artist words", clean);
  truth(T, "names fit the pattern name (<= 27 chars)", fits);
}

}  // namespace

void runFactoryTests() {
  testFactoryPresetsLoad();
  testFactoryRoundTrip();
  testFactoryRenderLevels();
  testFactoryNames();
}
