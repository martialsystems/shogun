// SHOGUN factory bank named tests (TESTPLAN.md "Factory bank"): every preset loads, save/reload is identical, each
// kit with its pattern renders 2 bars between -40 and -6 dBFS at 44.1 and 48 kHz, names are unique and clean.
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "factory.h"
#include "fmtg_ref.inc"
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
      if (db < lo) {
        lo = db;
        loName = factory::programName(pr);
      }
      if (db > hi) {
        hi = db;
        hiName = factory::programName(pr);
      }
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


// The libc-free writer (the wasm page saves documents with it) against a correctly-rounded %.*g table
// (not host snprintf: Apple's gdtoa leaves trailing zeros on some small integers, e.g. 305 at p=2 → "3.0e+02"
// instead of "3e+02"), the shortest round-trip text for doubles and floats, and the same document for every
// program, sparse and full.
static void testPatchWriterNoLibc() {
  const char* T = "testPatchWriterNoLibc";
  long refBad = 0, refChecks = 0;
  char b[64];
  for (int i = 0; i < kFmtGRefN; ++i) {
    double v;
    std::memcpy(&v, &kFmtGRef[i].bits, sizeof v);
    patchjson::fmtG(b, v, kFmtGRef[i].p);
    ++refChecks;
    if (std::strcmp(b, kFmtGRef[i].text) != 0) {
      ++refBad;
      if (refBad <= 8)
        std::printf("%s: fmtG ref mismatch bits=%016llx p=%d want '%s' got '%s'\n", T,
                    static_cast<unsigned long long>(kFmtGRef[i].bits), kFmtGRef[i].p, kFmtGRef[i].text, b);
    }
  }
  // Exact Apple gdtoa regression: 305 at precision 2 must be "3e+02", not "3.0e+02".
  {
    constexpr std::uint64_t k305 = 0x4073100000000000ull;
    double v305;
    std::memcpy(&v305, &k305, sizeof v305);
    patchjson::fmtG(b, v305, 2);
    truth(T, "305 at p=2 is 3e+02 (not Apple trailing-zero 3.0e+02)", std::strcmp(b, "3e+02") == 0);
  }
  std::uint64_t x = 0x9e3779b97f4a7c15ull;
  auto rnd = [&x]() {
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
  };
  std::vector<double> ds = {0.0, -0.0, 1.0, 0.5, 0.1, 1.0 / 3.0, 5e-324, 2.2250738585072014e-308, 1.7976931348623157e308,
                            9007199254740993.0, 123456789012345678.0, 1e-5, 1e-4, 99999.5, 100000.0, 0.00012345};
  for (int k = 0; k <= 127; ++k) ds.push_back(k / 127.0);
  for (int k = -1074; k <= 1023; ++k) ds.push_back(std::ldexp(1.0, k));
  for (int i = 0; i < 60000; ++i) {
    std::uint64_t bb = rnd();
    double v;
    std::memcpy(&v, &bb, sizeof v);
    if (std::isfinite(v)) ds.push_back(v);
    ds.push_back(static_cast<double>(rnd() >> 11) / 9007199254740992.0);  // [0, 1)
    ds.push_back((static_cast<double>(rnd() % 2000001) - 1000000.0) / 1000.0);
  }
  long gChecks = 0, gLibcDiff = 0, nBad = 0, fBad = 0;
  char a[64];
  for (std::size_t i = 0; i < ds.size(); ++i) {
    const double v = ds[i];
    if (i % 7 == 0)
      for (int p = 1; p <= 17; ++p) {
        std::snprintf(a, sizeof a, "%.*g", p, v);
        patchjson::fmtG(b, v, p);
        ++gChecks;
        if (std::strcmp(a, b) != 0) {
          ++gLibcDiff;
          if (gLibcDiff <= 8) {
            std::uint64_t bits;
            std::memcpy(&bits, &v, sizeof bits);
            std::printf("%s: host snprintf differs bits=%016llx p=%d libc='%s' ours='%s'\n", T,
                        static_cast<unsigned long long>(bits), p, a, b);
          }
        }
      }
    std::string ref;
    patchjson::putNum(ref, v);
    patchjson::shortestNum(b, v);
    nBad += ref != b;
    const float f = static_cast<float>(v);
    if (std::isfinite(f)) {
      std::string rf;
      patchjson::putFloat(rf, f);
      patchjson::shortestFloat(b, f);
      fBad += rf != b;
    }
  }
  for (int i = 0; i < 100000; ++i) {  // every float exponent, random mantissas
    const std::uint32_t bits = static_cast<std::uint32_t>(rnd());
    float f;
    std::memcpy(&f, &bits, sizeof f);
    if (!std::isfinite(f)) continue;
    std::string rf;
    patchjson::putFloat(rf, f);
    patchjson::shortestFloat(b, f);
    fBad += rf != b;
  }
  bool docs = true;
  static char buf[1 << 20];
  for (int pr = 0; pr < factory::kPrograms; ++pr) {
    Patch p;
    factory::loadProgram(pr, p);
    Engine e;
    applyPatch(p, e);
    Patch s;
    capturePatch(e, s);
    for (bool full : {false, true}) {
      patchjson::BufOut o(buf, sizeof buf);
      writePatchJson(o, full ? s : p, full);
      docs = docs && o.ok && patchToJson(full ? s : p, full) == buf;
    }
  }
  std::printf("%s: %ld ref texts, %ld %%.*g vs host snprintf (%ld host diffs), %zu doubles and %zu+ floats, %d programs x 2 "
              "documents\n",
              T, refChecks, gChecks, gLibcDiff, ds.size(), ds.size(), factory::kPrograms);
  truth(T, "%.*g bytes = correctly rounded table", refBad == 0);
  truth(T, "shortest double text = putNum", nBad == 0);
  truth(T, "shortest float text = putFloat", fBad == 0);
  truth(T, "documents = patchToJson (sparse and full)", docs);
}


// Exact-silence sleep (shogun.h): an engine that sleeps renders the same bits as one that never does, through stops,
// long silences, routing/bus/glue/delay/width changes while asleep, wakes by hit, note, RET and OUT jacks, at 1/2/4x.
static void testSleepBitExact() {
  const char* T = "testSleepBitExact";
  long samples = 0, diffs = 0, slept = 0;
  auto pid = [](const char* n) { return findParam(n); };
  for (int os : {1, 2, 4})
    for (int prog : {0, 3, 17}) {
      Engine a, b;
      for (Engine* e : {&a, &b}) {
        e->prepare(44100.0, os);
        Patch pt;
        factory::loadProgram(prog, pt);
        applyPatch(pt, *e);
        e->setParamNow(P_CLOCK_SOURCE, stepU(1, 3));
      }
      b.setSleepEnabled(false);
      auto both = [&](auto f) { f(a); f(b); };
      auto run = [&](long n) {
        for (long i = 0; i < n; ++i) {
          a.processSample();
          b.processSample();
          ++samples;
          std::uint64_t x, y;
          double la = a.mainL(), lb = b.mainL(), ra = a.mainR(), rb = b.mainR();
          std::memcpy(&x, &la, 8);
          std::memcpy(&y, &lb, 8);
          bool same = x == y;
          std::memcpy(&x, &ra, 8);
          std::memcpy(&y, &rb, 8);
          same = same && x == y;
          for (int k = 0; k < 16; ++k) {
            double ua = a.aux()[k], ub = b.aux()[k];
            std::memcpy(&x, &ua, 8);
            std::memcpy(&y, &ub, 8);
            same = same && x == y;
          }
          diffs += !same;
        }
      };
      const long s = 44100;
      both([](Engine& e) { e.setRunning(true); });
      run(2 * s);
      both([](Engine& e) { e.setRunning(false); });
      run(12 * s);
      both([&](Engine& e) { e.setParam(pid("BD1:OUTPUT"), stepU(6, 14)); e.setParam(pid("SD:OUTPUT"), stepU(2, 14)); });
      run(2 * s);
      both([&](Engine& e) { e.setParam(pid("BUS B:COMP"), stepU(1, 2)); e.setParam(pid("MASTER:GLUE"), 0.5);
                            e.setParam(pid("MASTER:WIDTH"), 0.8); e.setParam(pid("MASTER:DRIVE"), 0.4);
                            e.setParam(pid("MASTER:CLIP"), stepU(1, 2)); });
      run(2 * s);
      both([](Engine& e) { e.trigger(BD1); e.trigger(SD); });
      run(8 * s);
      both([&](Engine& e) { e.setParam(pid("CH:SEND"), 0.6); e.trigger(CH); });
      run(10 * s);
      both([](Engine& e) { e.noteOn(LEAD, 62); });
      run(s / 10);
      both([](Engine& e) { e.noteOff(LEAD); });
      run(6 * s);
      both([](Engine& e) { e.setExternalInput(drumPort(CP, DJ_OUT), 0.0f, true); });
      run(2 * s);
      both([](Engine& e) { e.trigger(CP); });
      run(4 * s);
      both([](Engine& e) { e.setExternalInput(drumPort(RS, DJ_RET), 1.5f, true); });
      run(s);
      both([](Engine& e) { e.setExternalInput(drumPort(RS, DJ_RET), 0.0f, false); });
      run(6 * s);
      both([](Engine& e) { e.setRunning(true); });
      run(2 * s);
      both([](Engine& e) { e.setRunning(false); });
      run(6 * s);
      slept += a.sleptSamples();
      if (b.sleptSamples() != 0) ++diffs;
    }
  std::printf("%s: %ld samples, %ld slept (%.0f %%), %ld differing\n", T, samples, slept,
              100.0 * static_cast<double>(slept) / static_cast<double>(samples), diffs);
  truth(T, "sleeping engine is bit-identical to a non-sleeping one", diffs == 0);
  truth(T, "the engine sleeps in silence", slept > samples / 4);
}

// BD1 DRIVE gliding to 0 (and tiny drive with the TONE jack moving) stays bounded: the tanh ADAA difference quotient
// used to cancel to noise for drive in (0, 1e-2) and peak at +191 dBFS.
static void testBd1DriveToZeroBounded() {
  const char* T = "testBd1DriveToZeroBounded";
  double voicePk = 0.0, mainPk = 0.0;
  bool finite = true;
  for (double from : {1.0, 0.5, 0.2, 0.05, 0.01}) {
    Engine e;
    e.prepare(48000.0, 2);
    e.loadInit();
    e.setParamNow(P_BD1_DRIVE, from);
    for (int h = 0; h < 6; ++h) {
      if (h == 1) e.setParam(P_BD1_DRIVE, 0.0);
      if (h == 3) e.setParam(P_BD1_DRIVE, from);
      if (h == 4) e.setParam(P_BD1_DRIVE, 0.0005);
      e.trigger(BD1, 5.0, 0.0, 3);
      for (long i = 0; i < 24000; ++i) {
        if (h >= 4) e.setExternalInput(drumPort(BD1, DJ_TONE), static_cast<float>(5.0 * std::sin(static_cast<double>(i) * 0.001)), true);
        e.processSample();
        const double v = std::fabs(e.voiceOut(BD1)), m = std::fmax(std::fabs(e.mainL()), std::fabs(e.mainR()));
        finite = finite && std::isfinite(v) && std::isfinite(m);
        voicePk = std::fmax(voicePk, v);
        mainPk = std::fmax(mainPk, m);
      }
    }
  }
  std::printf("%s: BD1 voice peak %.2f dBFS, main peak %.2f dBFS over drive glides to 0 and tiny drive + TONE jack\n", T,
              20.0 * std::log10(voicePk), 20.0 * std::log10(mainPk));
  truth(T, "finite", finite);
  atMost(T, "BD1 voice peak (linear) at or below 4 (+12 dBFS)", voicePk, 4.0);
  atMost(T, "main peak (linear) at or below 2 (+6 dBFS)", mainPk, 2.0);
}

void runFactoryTests() {
  testFactoryPresetsLoad();
  testFactoryRoundTrip();
  testPatchWriterNoLibc();
  testFactoryRenderLevels();
  testFactoryNames();
  testSleepBitExact();
  testBd1DriveToZeroBounded();
}
