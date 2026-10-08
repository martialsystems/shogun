#pragma once
// SHOGUN patch state (§14): the JSON document inside `<SHOGUN version=2>` plugin states, also the format of the
// factory bank (engine/factory_bank.inc). Framework-free so the engine, the plugin and the wasm page share one reader.
//
//   {"format": "shogun-patch", "version": 2, "name": "...",
//    "params": {"SECTION:LABEL": u, ...},            // sparse is fine: missing ids keep their INIT value
//    "mod": [{"src": "LFO 1", "dst": "LEAD:CUTOFF", "depth": 0.3, "via": null, "curve": "LIN", "on": true}, ...],
//    "cables": [["SHOGUN/MOD:LFO 1", "SHOGUN/BD1:PITCH"], ...], "cvAmt": {"PORT": amt}, "inLaw": {"PORT": law},
//    "seq": {"pattern": "002 ...", "seed": n, "tracks": [{"id": "BD1", "len": 16, "scale": null, "swing": null,
//            "shift": 0, "steps": [{"i": 0, "on": true, "acc": 3, "prob": 1, "micro": 0, "flam": 0, "ratchet": 1,
//            "bend": 0, "note": 48, "tie": false, "locks": {"BD1:DECAY": 0.8}}, ...]}, ...]}}
//
// Step fields left out take the defaults above (on true, acc 2, prob 1, micro 0, flam 0, ratchet 1, bend 0, note 48,
// tie false). Parameters are host floats in the plugin, so applyPatch() rounds each u through float: the engine,
// the plugin and the web page then load bit-identical values. Reading needs no libc (the wasm build); writing
// (patchToJson) is native only.

#include <cstdint>
#include <cstring>
#ifndef SHOGUN_NO_FORMAT
#include <cstdio>
#include <cstdlib>
#include <string>
#endif

#include "shogun.h"

namespace shogun {

// Mod source and curve names of the saved state (the plugin's ROUTE tab uses the same strings).
inline const char* const kStateSrcNames[mod::kSourceCount] = {"",    "LFO 1", "LFO 2",   "LFO 3", "LFO 4",
                                                              "ENV", "PENV",  "VEL",     "ACC",   "RND HIT",
                                                              "NOTE", "RND",  "MOD W",   "AT"};
inline const char* const kStateCurveNames[mod::kCurveCount] = {"LIN", "EXP", "LOG", "S"};
inline const char* const kStateScaleNames[4] = {"1/32", "1/16", "1/8T", "1/8"};

struct Patch {
  char name[32] = "001 INIT";
  double u[kParamCount] = {};
  mod::Row rows[mod::kRows];
  int nCables = 0;
  int cableFrom[Engine::kMaxCables] = {}, cableTo[Engine::kMaxCables] = {};
  double cvAmt[kPorts] = {};
  std::uint8_t inLaw[kPorts] = {};
  Pattern pattern;
  Patch() { clear(); }
  void clear() {  // INIT: noon defaults, no rows, no cables, CV AMT 1, empty '001 INIT' pattern
    std::strcpy(name, "001 INIT");
    for (int i = 0; i < kParamCount; ++i) u[i] = static_cast<double>(kParams[i].def);
    for (auto& r : rows) r = mod::Row();
    nCables = 0;
    for (int i = 0; i < kPorts; ++i) {
      cvAmt[i] = 1.0;
      inLaw[i] = 0;
    }
    pattern = Pattern();
  }
};

namespace patchjson {

// A pull reader over a NUL-terminated JSON text. Any syntax error sets ok = false and stops.
struct Reader {
  const char* p;
  bool ok = true;
  explicit Reader(const char* text) : p(text) {}
  void ws() {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
  }
  bool peek(char c) {
    ws();
    return *p == c;
  }
  bool eat(char c) {
    ws();
    if (*p != c) return false;
    ++p;
    return true;
  }
  bool expect(char c) {
    if (!eat(c)) ok = false;
    return ok;
  }
  bool word(const char* w) {
    ws();
    const std::size_t n = std::strlen(w);
    if (std::strncmp(p, w, n) != 0) return false;
    p += n;
    return true;
  }
  // A string into out (n bytes, truncated). Escapes: \" \\ \/ \b \f \n \r \t; \uXXXX keeps ASCII only.
  bool str(char* out, int n) {
    if (!expect('"')) return false;
    int k = 0;
    while (*p && *p != '"') {
      char c = *p++;
      if (c == '\\') {
        const char e = *p++;
        switch (e) {
          case 'b': c = '\b'; break;
          case 'f': c = '\f'; break;
          case 'n': c = '\n'; break;
          case 'r': c = '\r'; break;
          case 't': c = '\t'; break;
          case 'u': {
            int v = 0;
            for (int i = 0; i < 4; ++i) {
              const char h = *p;
              if (!h) break;
              ++p;
              v = v * 16 + (h >= '0' && h <= '9' ? h - '0' : (h | 0x20) - 'a' + 10);
            }
            c = v < 128 ? static_cast<char>(v) : '?';
            break;
          }
          case 0: ok = false; return false;
          default: c = e; break;  // \" \\ \/
        }
      }
      if (k < n - 1) out[k++] = c;
    }
    if (n > 0) out[k] = 0;
    return expect('"');
  }
  bool num(double& v) {
    ws();
#ifndef SHOGUN_NO_FORMAT
    char* end = nullptr;
    v = std::strtod(p, &end);  // correctly rounded
    if (end == p) return ok = false;
    p = end;
    return true;
#else
    // Freestanding (wasm): exact for up to 15 significant digits and |exponent| <= 22 (every factory value),
    // where mantissa / 10^k is one correctly rounded IEEE operation, the same double strtod gives.
    bool neg = false;
    if (*p == '-') {
      neg = true;
      ++p;
    }
    std::uint64_t m = 0;
    int digits = 0, e10 = 0;
    if (!(*p >= '0' && *p <= '9')) return ok = false;
    for (; *p >= '0' && *p <= '9'; ++p)
      if (digits < 19) {
        m = m * 10u + static_cast<std::uint64_t>(*p - '0');
        if (m) ++digits;
      } else {
        ++e10;
      }
    if (*p == '.') {
      ++p;
      for (; *p >= '0' && *p <= '9'; ++p)
        if (digits < 19) {
          m = m * 10u + static_cast<std::uint64_t>(*p - '0');
          if (m) ++digits;
          --e10;
        }
    }
    if (*p == 'e' || *p == 'E') {
      ++p;
      bool en = false;
      if (*p == '+' || *p == '-') en = (*p++ == '-');
      int x = 0;
      for (; *p >= '0' && *p <= '9'; ++p) x = x * 10 + (*p - '0');
      e10 += en ? -x : x;
    }
    double d = static_cast<double>(m);
    double s = 1.0;
    int a = e10 < 0 ? -e10 : e10;
    for (double b = 10.0; a; a >>= 1, b *= b)
      if (a & 1) s *= b;
    d = e10 < 0 ? d / s : d * s;
    v = neg ? -d : d;
    return true;
#endif
  }
  bool boolean(bool& b) {
    if (word("true")) {
      b = true;
      return true;
    }
    if (word("false")) {
      b = false;
      return true;
    }
    return ok = false;
  }
  bool null() { return word("null"); }
  void skip() {  // any value
    ws();
    if (*p == '"') {
      char tmp[2];
      str(tmp, 2);
    } else if (*p == '{') {
      object([this](const char*) { skip(); });
    } else if (*p == '[') {
      array([this]() { skip(); });
    } else if (!(word("true") || word("false") || word("null"))) {
      double d;
      num(d);
    }
  }
  // {"key": value, ...}: f(key) must read the value.
  template <class F>
  bool object(F f) {
    if (!expect('{')) return false;
    if (eat('}')) return true;
    do {
      char key[64];
      if (!str(key, sizeof key) || !expect(':')) return false;
      f(static_cast<const char*>(key));
      if (!ok) return false;
    } while (eat(','));
    return expect('}');
  }
  template <class F>
  bool array(F f) {
    if (!expect('[')) return false;
    if (eat(']')) return true;
    do {
      f();
      if (!ok) return false;
    } while (eat(','));
    return expect(']');
  }
  double number() {
    double d = 0.0;
    num(d);
    return d;
  }
  int integer() { return static_cast<int>(number()); }
};

inline bool is(const char* a, const char* b) { return std::strcmp(a, b) == 0; }
inline int voiceFromName(const char* s) {
  for (int v = 0; v < kVoices; ++v)
    if (is(s, kVoiceNames[v])) return v;
  return -1;
}
inline void srcFromName(const char* s, int& src, int& voice) {
  src = mod::SRC_NONE;
  voice = -1;
  char name[32];
  int k = 0;
  while (s[k] && s[k] != '@' && k < 31) {
    name[k] = s[k];
    ++k;
  }
  name[k] = 0;
  for (int i = 1; i < mod::kSourceCount; ++i)
    if (is(name, kStateSrcNames[i])) src = i;
  if (s[k] == '@') voice = voiceFromName(s + k + 1);
}
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline void readStep(Reader& r, Track& tr) {
  Step st;
  st.on = true;
  int idx = -1;
  r.object([&](const char* k) {
    if (is(k, "i")) idx = r.integer();
    else if (is(k, "on")) r.boolean(st.on);
    else if (is(k, "acc")) st.acc = static_cast<std::uint8_t>(clampi(r.integer(), 1, 3));
    else if (is(k, "prob")) st.prob = static_cast<float>(r.number());
    else if (is(k, "micro")) st.micro = static_cast<float>(r.number());
    else if (is(k, "flam")) st.flam = static_cast<std::uint8_t>(clampi(r.integer(), 0, 16));
    else if (is(k, "ratchet")) st.ratchet = static_cast<std::uint8_t>(clampi(r.integer(), 1, 8));
    else if (is(k, "bend")) st.bend = static_cast<float>(r.number());
    else if (is(k, "note")) st.note = static_cast<std::int8_t>(clampi(r.integer(), -128, 127));
    else if (is(k, "tie")) r.boolean(st.tie);
    else if (is(k, "locks"))
      r.object([&](const char* id) {
        const int p = findParam(id);
        const float u = static_cast<float>(r.number());
        if (p >= 0) st.setLock(p, u);
      });
    else r.skip();
  });
  if (idx >= 0 && idx < kMaxSteps) tr.steps[idx] = st;
}

inline void readTrack(Reader& r, Pattern& pat) {
  Track tr;
  int t = -1;
  r.object([&](const char* k) {
    if (is(k, "id")) {
      char id[16];
      r.str(id, sizeof id);
      t = voiceFromName(id);
    } else if (is(k, "len")) {
      tr.len = clampi(r.integer(), 1, kMaxSteps);
    } else if (is(k, "scale")) {
      tr.scale = -1;
      if (!r.null()) {
        char s[8];
        r.str(s, sizeof s);
        for (int i = 0; i < 4; ++i)
          if (is(s, kStateScaleNames[i])) tr.scale = i;
      }
    } else if (is(k, "swing")) {
      tr.swing = r.null() ? -1.0 : r.number();
    } else if (is(k, "shift")) {
      tr.shift = r.number();
    } else if (is(k, "steps")) {
      r.array([&]() { readStep(r, tr); });
    } else {
      r.skip();
    }
  });
  if (t >= 0) pat.tracks[t] = tr;
}

inline void readMod(Reader& r, Patch& out, int& nRows) {
  mod::Row row;
  r.object([&](const char* k) {
    char s[48];
    if (is(k, "src")) {
      r.str(s, sizeof s);
      srcFromName(s, row.src, row.srcVoice);
    } else if (is(k, "dst")) {
      r.str(s, sizeof s);
      row.dst = findParam(s);
    } else if (is(k, "depth")) {
      row.depth = r.number();
    } else if (is(k, "via")) {
      if (!r.null()) {
        r.str(s, sizeof s);
        srcFromName(s, row.via, row.viaVoice);
      }
    } else if (is(k, "curve")) {
      r.str(s, sizeof s);
      for (int i = 0; i < mod::kCurveCount; ++i)
        if (is(s, kStateCurveNames[i])) row.curve = i;
    } else if (is(k, "on")) {
      r.boolean(row.on);
    } else {
      r.skip();
    }
  });
  if (row.src != mod::SRC_NONE && row.dst >= 0 && nRows < mod::kRows) out.rows[nRows++] = row;
}

}  // namespace patchjson

// Parses a patch document into out, starting from INIT (so a sparse document is complete). Unknown keys and ids are
// skipped. Jack ids resolve like the plugin's state load (engine/ports.h resolvePort). False on a syntax error.
inline bool parsePatch(const char* json, Patch& out) {
  using namespace patchjson;
  out.clear();
  Reader r(json);
  int nRows = 0;
  r.object([&](const char* k) {
    if (is(k, "name")) {
      r.str(out.name, sizeof out.name);
    } else if (is(k, "params")) {
      r.object([&](const char* id) {
        const int p = findParam(id);
        const double u = r.number();
        if (p >= 0) out.u[p] = u;
      });
    } else if (is(k, "mod")) {
      r.array([&]() { readMod(r, out, nRows); });
    } else if (is(k, "cables")) {
      r.array([&]() {
        char a[64] = {}, b[64] = {};
        int i = 0;
        r.array([&]() {
          if (i == 0) r.str(a, sizeof a);
          else if (i == 1) r.str(b, sizeof b);
          else r.skip();
          ++i;
        });
        int from[3], to[3], lf = 0, lt = 0;
        const int nf = resolvePort(a, from, &lf), nt = resolvePort(b, to, &lt);
        if (nf < 1) return;
        for (int j = 0; j < nt && out.nCables < Engine::kMaxCables; ++j) {
          if (to[j] < 0) continue;
          out.cableFrom[out.nCables] = from[0];
          out.cableTo[out.nCables] = to[j];
          ++out.nCables;
          if (lt == 1) out.inLaw[to[j]] = 1;
          if (lt == 2) out.cvAmt[to[j]] = 1.0 / 12.0;
        }
      });
    } else if (is(k, "cvAmt")) {
      r.object([&](const char* id) {
        const double a = r.number();
        int ports[3], law = 0;
        const int n = resolvePort(id, ports, &law);
        for (int j = 0; j < n; ++j)
          if (ports[j] >= 0) out.cvAmt[ports[j]] = a * (law == 2 ? 1.0 / 12.0 : 1.0);
      });
    } else if (is(k, "inLaw")) {
      r.object([&](const char* id) {
        const int law = r.integer();
        const int p = portFromId(id);
        if (p >= 0) out.inLaw[p] = static_cast<std::uint8_t>(law);
      });
    } else if (is(k, "seq")) {
      r.object([&](const char* sk) {
        if (is(sk, "pattern")) r.str(out.pattern.name, sizeof out.pattern.name);
        else if (is(sk, "seed")) out.pattern.seed = static_cast<std::uint32_t>(r.number());
        else if (is(sk, "tracks")) r.array([&]() { readTrack(r, out.pattern); });
        else r.skip();
      });
    } else {
      r.skip();
    }
  });
  return r.ok;
}

// Parameters compare as the host floats they are.
inline bool sameParams(const Patch& a, const Patch& b) {
  for (int i = 0; i < kParamCount; ++i)
    if (!dsp::exactEq(static_cast<float>(a.u[i]), static_cast<float>(b.u[i]))) return false;
  return true;
}

// Loads a patch into the engine: INIT first, then every parameter (through float, as the plugin's host parameters
// hold it), mod rows, cables, CV AMT, input laws and the pattern.
inline void applyPatch(const Patch& pt, Engine& e) {
  e.loadInit();
  for (int i = 0; i < kParamCount; ++i) e.setParamNow(i, static_cast<double>(static_cast<float>(pt.u[i])));
  for (int i = 0; i < mod::kRows; ++i) e.modulation().rows[i] = pt.rows[i];
  e.clearCables();
  for (int i = 0; i < pt.nCables; ++i) e.addCable(pt.cableFrom[i], pt.cableTo[i]);
  for (int i = 0; i < kPorts; ++i) {
    e.setCvAmt(i, pt.cvAmt[i]);
    e.setInputLaw(i, pt.inLaw[i]);
  }
  e.setPattern(pt.pattern);
}

// The engine's current state as a patch (the inverse of applyPatch for parameters, rows, cables, CV AMT, laws, pattern).
inline void capturePatch(const Engine& e, Patch& pt) {
  pt.clear();
  std::strcpy(pt.name, e.pattern().name);
  for (int i = 0; i < kParamCount; ++i) pt.u[i] = e.param(i);
  int n = 0;
  for (int i = 0; i < mod::kRows; ++i) {
    const mod::Row& r = e.modulation().rows[i];
    if (r.src != mod::SRC_NONE && r.dst >= 0) pt.rows[n++] = r;
  }
  pt.nCables = e.cableCount();
  for (int i = 0; i < pt.nCables; ++i) e.cable(i, pt.cableFrom[i], pt.cableTo[i]);
  for (int i = 0; i < kPorts; ++i) {
    pt.cvAmt[i] = e.cvAmt(i);
    pt.inLaw[i] = static_cast<std::uint8_t>(e.inputLaw(i));
  }
  pt.pattern = e.pattern();
}

#ifndef SHOGUN_NO_FORMAT
namespace patchjson {
inline void put(std::string& s, const char* t) { s += t; }
inline void putStr(std::string& s, const char* t) {
  s += '"';
  for (; *t; ++t) {
    if (*t == '"' || *t == '\\') s += '\\';
    s += *t;
  }
  s += '"';
}
inline void putNum(std::string& s, double v) {  // shortest text that reads back to the same double
  char b[40];
  for (int prec = 1; prec <= 17; ++prec) {
    std::snprintf(b, sizeof b, "%.*g", prec, v);
    if (dsp::exactEq(std::strtod(b, nullptr), v)) break;
  }
  s += b;
}
inline void putFloat(std::string& s, float v) {  // shortest text that reads back (through float) to the same float
  char b[40];
  for (int prec = 1; prec <= 9; ++prec) {
    std::snprintf(b, sizeof b, "%.*g", prec, static_cast<double>(v));
    if (dsp::exactEq(static_cast<float>(std::strtod(b, nullptr)), v)) break;
  }
  s += b;
}
inline void putInt(std::string& s, long v) {
  char b[24];
  std::snprintf(b, sizeof b, "%ld", v);
  s += b;
}
}  // namespace patchjson

// The patch as JSON. full = every parameter (a saved state); otherwise only those that differ from INIT and only
// non-default step fields (the factory bank style). Deterministic: the same patch gives the same bytes.
inline std::string patchToJson(const Patch& pt, bool full) {
  using namespace patchjson;
  std::string s;
  put(s, "{\"format\": \"shogun-patch\", \"version\": 2, \"name\": ");
  putStr(s, pt.name);
  put(s, ",\n \"params\": {");
  bool first = true;
  for (int i = 0; i < kParamCount; ++i) {
    if (!full && dsp::exactEq(static_cast<float>(pt.u[i]), kParams[i].def)) continue;
    put(s, first ? "\n  " : ",\n  ");
    first = false;
    putStr(s, kParams[i].id);
    put(s, ": ");
    putFloat(s, static_cast<float>(pt.u[i]));
  }
  put(s, "},\n \"mod\": [");
  first = true;
  for (const mod::Row& r : pt.rows) {
    if (r.src == mod::SRC_NONE || r.dst < 0) continue;
    put(s, first ? "\n  {\"src\": " : ",\n  {\"src\": ");
    first = false;
    std::string src = kStateSrcNames[r.src];
    if (mod::perVoiceSource(r.src) && r.srcVoice >= 0) src = src + "@" + kVoiceNames[r.srcVoice];
    putStr(s, src.c_str());
    put(s, ", \"dst\": ");
    putStr(s, kParams[r.dst].id);
    put(s, ", \"depth\": ");
    putNum(s, r.depth);
    put(s, ", \"via\": ");
    if (r.via == mod::SRC_NONE) {
      put(s, "null");
    } else {
      std::string via = kStateSrcNames[r.via];
      if (mod::perVoiceSource(r.via) && r.viaVoice >= 0) via = via + "@" + kVoiceNames[r.viaVoice];
      putStr(s, via.c_str());
    }
    put(s, ", \"curve\": ");
    putStr(s, kStateCurveNames[r.curve & 3]);
    put(s, r.on ? ", \"on\": true}" : ", \"on\": false}");
  }
  put(s, "],\n \"cables\": [");
  for (int i = 0; i < pt.nCables; ++i) {
    put(s, i ? ", [" : "[");
    putStr(s, (std::string("SHOGUN/") + kPortTable[pt.cableFrom[i]].id).c_str());
    put(s, ", ");
    putStr(s, (std::string("SHOGUN/") + kPortTable[pt.cableTo[i]].id).c_str());
    put(s, "]");
  }
  put(s, "],\n \"cvAmt\": {");
  first = true;
  for (int i = 0; i < kPorts; ++i)
    if (kPortTable[i].dir == PortDir::In && !dsp::exactEq(pt.cvAmt[i], 1.0)) {
      put(s, first ? "" : ", ");
      first = false;
      putStr(s, kPortTable[i].id);
      put(s, ": ");
      putNum(s, pt.cvAmt[i]);
    }
  put(s, "},\n \"inLaw\": {");
  first = true;
  for (int i = 0; i < kPorts; ++i)
    if (pt.inLaw[i] != 0) {
      put(s, first ? "" : ", ");
      first = false;
      putStr(s, kPortTable[i].id);
      put(s, ": ");
      putInt(s, pt.inLaw[i]);
    }
  put(s, "},\n \"seq\": {\"pattern\": ");
  putStr(s, pt.pattern.name);
  put(s, ", \"seed\": ");
  putInt(s, static_cast<long>(pt.pattern.seed));
  put(s, ", \"tracks\": [");
  const Step def;
  const Track defT;
  for (int t = 0; t < 16; ++t) {
    const Track& tr = pt.pattern.tracks[t];
    put(s, t ? ",\n  {\"id\": " : "\n  {\"id\": ");
    putStr(s, kVoiceNames[t]);
    put(s, ", \"len\": ");
    putInt(s, tr.len);
    if (full || tr.scale >= 0) {
      put(s, ", \"scale\": ");
      if (tr.scale < 0) put(s, "null");
      else putStr(s, kStateScaleNames[tr.scale & 3]);
    }
    if (full || tr.swing >= 0.0) {
      put(s, ", \"swing\": ");
      if (tr.swing < 0.0) put(s, "null");
      else putNum(s, tr.swing);
    }
    if (full || !dsp::exactEq(tr.shift, defT.shift)) {
      put(s, ", \"shift\": ");
      putNum(s, tr.shift);
    }
    put(s, ", \"steps\": [");
    bool firstStep = true;
    for (int i = 0; i < kMaxSteps; ++i) {
      const Step& st = tr.steps[i];
      if (!st.on && st.nLocks == 0) continue;
      put(s, firstStep ? "\n   {\"i\": " : ",\n   {\"i\": ");
      firstStep = false;
      putInt(s, i);
      if (full || !st.on) put(s, st.on ? ", \"on\": true" : ", \"on\": false");
      if (full || st.acc != def.acc) put(s, ", \"acc\": "), putInt(s, st.acc);
      if (full || !dsp::exactEq(st.prob, def.prob)) put(s, ", \"prob\": "), putFloat(s, st.prob);
      if (full || !dsp::exactEq(st.micro, def.micro)) put(s, ", \"micro\": "), putFloat(s, st.micro);
      if (full || st.flam != def.flam) put(s, ", \"flam\": "), putInt(s, st.flam);
      if (full || st.ratchet != def.ratchet) put(s, ", \"ratchet\": "), putInt(s, st.ratchet);
      if (full || !dsp::exactEq(st.bend, def.bend)) put(s, ", \"bend\": "), putFloat(s, st.bend);
      if (!isDrum(t)) {
        if (full || st.note != def.note) put(s, ", \"note\": "), putInt(s, st.note);
        if (full || st.tie) put(s, st.tie ? ", \"tie\": true" : ", \"tie\": false");
      }
      if (full || st.nLocks) {
        put(s, ", \"locks\": {");
        for (int k = 0; k < st.nLocks; ++k) {
          put(s, k ? ", " : "");
          putStr(s, kParams[st.locks[k].param].id);
          put(s, ": ");
          putFloat(s, st.locks[k].u);
        }
        put(s, "}");
      }
      put(s, "}");
    }
    put(s, "]}");
  }
  put(s, "]}}\n");
  return s;
}
#endif

}  // namespace shogun
