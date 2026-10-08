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
// the plugin and the web page then load bit-identical values. Neither reading nor writing (writePatchJson) needs
// libc: the wasm page saves full documents too. patchToJson() is the native std::string form.

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
  // Load report: saved cables that could not be placed (an end on a removed port, kRemovedPorts, or an unknown id).
  // droppedRemoved counts those on a removed port; droppedIds keeps the first kDropIds cables as "from -> to".
  static constexpr int kDropIds = 4;
  int droppedCables = 0, droppedRemoved = 0;
  char droppedIds[kDropIds][136] = {};
  Patch() { clear(); }
  void clear() {  // INIT: noon defaults, no rows, no cables, CV AMT 1, empty '001 INIT' pattern
    std::strcpy(name, "001 INIT");
    for (int i = 0; i < kParamCount; ++i) u[i] = static_cast<double>(kParams[i].def);
    for (auto& r : rows) r = mod::Row();
    nCables = 0;
    droppedCables = droppedRemoved = 0;
    for (auto& d : droppedIds) d[0] = 0;
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
// skipped; a cable with an end that does not resolve (a removed port such as CLOCK:FILL IN or MOD:LANE A, or an unknown
// id) is dropped and counted in out.droppedCables / droppedRemoved / droppedIds. Jack ids resolve like the plugin's state load (engine/ports.h resolvePort). False on a syntax error.
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
        if (nf < 1 || nt < 1) {  // dropped and reported, never an error (old FILL IN / LANE A cables)
          if (out.droppedCables < Patch::kDropIds) {
            char* d = out.droppedIds[out.droppedCables];
            int n = 0;
            const char* parts[3] = {a, " -> ", b};
            for (const char* q : parts)
              for (; *q && n < 135; ++q) d[n++] = *q;
            d[n] = 0;
          }
          ++out.droppedCables;
          if (isRemovedPort(a) || isRemovedPort(b)) ++out.droppedRemoved;
          return;
        }
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

namespace patchjson {
// Number text without libc (the wasm build writes documents too). fmtG() is correctly rounded %.*g (ties to even);
// shortestNum()/shortestFloat() are the text putNum()/putFloat() pick with snprintf and strtod: the shortest %.*g that
// reads back to the same double, or through double to the same float. Exact arithmetic on the binary value (a small
// bignum). tests/factory.cpp checks fmtG against an embedded correctly-rounded table (not host snprintf: Apple's gdtoa
// can leave trailing zeros, e.g. 305 at p=2 → "3.0e+02").
namespace exact {
struct Big {
  std::uint32_t w[100];
  int n = 1;
};
inline void mulSmall(Big& b, std::uint32_t m) {
  std::uint64_t c = 0;
  for (int i = 0; i < b.n; ++i) {
    const std::uint64_t t = static_cast<std::uint64_t>(b.w[i]) * m + c;
    b.w[i] = static_cast<std::uint32_t>(t);
    c = t >> 32;
  }
  if (c) b.w[b.n++] = static_cast<std::uint32_t>(c);
}
inline std::uint32_t divSmall(Big& b, std::uint32_t d) {
  std::uint64_t r = 0;
  for (int i = b.n - 1; i >= 0; --i) {
    const std::uint64_t t = (r << 32) | b.w[i];
    b.w[i] = static_cast<std::uint32_t>(t / d);
    r = t % d;
  }
  while (b.n > 1 && b.w[b.n - 1] == 0) --b.n;
  return static_cast<std::uint32_t>(r);
}
// A positive decimal: digits d[0..len) (d[0] != '0', no trailing zeros), value 0.d1d2... shifted so d[0] sits at 10^x.
struct Dec {
  char d[800];
  int len = 0;
  int x = 0;
};
// The exact decimal value of a * 2^e (a > 0): every dyadic number has a finite expansion.
inline void dyadic(std::uint64_t a, int e, Dec& out) {
  Big b;
  b.w[0] = static_cast<std::uint32_t>(a);
  b.w[1] = static_cast<std::uint32_t>(a >> 32);
  b.n = b.w[1] ? 2 : 1;
  int scale = 0;
  if (e >= 0) {
    for (int k = e; k > 0; k -= 31) mulSmall(b, 1u << (k < 31 ? k : 31));
  } else {
    for (int k = -e; k > 0; k -= 13) {
      std::uint32_t m = 1;
      for (int j = 0; j < (k < 13 ? k : 13); ++j) m *= 5;
      mulSmall(b, m);
    }
    scale = e;  // a * 5^-e * 10^e
  }
  char rev[820];
  int n = 0;
  while (b.n > 1 || b.w[0] != 0) {
    std::uint32_t r = divSmall(b, 1000000000u);
    const bool last = b.n == 1 && b.w[0] == 0;
    for (int j = 0; j < 9 && (!last || r); ++j) {
      rev[n++] = static_cast<char>('0' + r % 10);
      r /= 10;
    }
  }
  int lo = 0;
  while (lo < n && rev[lo] == '0') ++lo;  // trailing zeros of the number
  out.len = 0;
  for (int i = n - 1; i >= lo; --i) out.d[out.len++] = rev[i];
  out.x = n - 1 + scale;
}
// -1, 0, 1: a < b, a == b, a > b (positive decimals)
inline int cmp(const Dec& a, const Dec& b) {
  if (a.x != b.x) return a.x < b.x ? -1 : 1;
  const int n = a.len > b.len ? a.len : b.len;
  for (int i = 0; i < n; ++i) {
    const char ca = i < a.len ? a.d[i] : '0', cb = i < b.len ? b.d[i] : '0';
    if (ca != cb) return ca < cb ? -1 : 1;
  }
  return 0;
}
// v rounded to p significant digits (ties to even, as printf does on the exact value)
inline void roundTo(const Dec& v, int p, Dec& out) {
  out.x = v.x;
  out.len = p;
  for (int i = 0; i < p; ++i) out.d[i] = i < v.len ? v.d[i] : '0';
  if (v.len > p) {
    const char c = v.d[p];
    const bool up = c > '5' || (c == '5' && (v.len > p + 1 || ((out.d[p - 1] - '0') & 1)));
    if (up) {
      int i = p - 1;
      while (i >= 0 && out.d[i] == '9') out.d[i--] = '0';
      if (i >= 0) {
        ++out.d[i];
      } else {
        out.d[0] = '1';
        for (int j = 1; j < p; ++j) out.d[j] = '0';
        ++out.x;
      }
    }
  }
  while (out.len > 1 && out.d[out.len - 1] == '0') --out.len;
}
// %.*g text of a rounded decimal r (at most p digits) with sign
inline int gText(char* o, bool neg, const Dec& r, int p) {
  int n = 0;
  if (neg) o[n++] = '-';
  if (r.x < -4 || r.x >= p) {
    o[n++] = r.d[0];
    if (r.len > 1) {
      o[n++] = '.';
      for (int i = 1; i < r.len; ++i) o[n++] = r.d[i];
    }
    o[n++] = 'e';
    o[n++] = r.x < 0 ? '-' : '+';
    int e = r.x < 0 ? -r.x : r.x;
    char t[8];
    int k = 0;
    do {
      t[k++] = static_cast<char>('0' + e % 10);
      e /= 10;
    } while (e);
    if (k < 2) t[k++] = '0';
    while (k) o[n++] = t[--k];
  } else if (r.x >= 0) {
    for (int i = 0; i <= r.x; ++i) o[n++] = i < r.len ? r.d[i] : '0';
    if (r.len > r.x + 1) {
      o[n++] = '.';
      for (int i = r.x + 1; i < r.len; ++i) o[n++] = r.d[i];
    }
  } else {
    o[n++] = '0';
    o[n++] = '.';
    for (int i = 0; i < -r.x - 1; ++i) o[n++] = '0';
    for (int i = 0; i < r.len; ++i) o[n++] = r.d[i];
  }
  o[n] = 0;
  return n;
}
inline std::uint64_t bitsOf(double v) {
  std::uint64_t b;
  std::memcpy(&b, &v, sizeof b);
  return b;
}
inline std::uint32_t bitsOf(float v) {
  std::uint32_t b;
  std::memcpy(&b, &v, sizeof b);
  return b;
}
// The interval of reals that round to the binary value m * 2^q (mantissa bits mb, smallest exponent field 1):
// (lo, hi), both ends included when m is even.
inline void bounds(std::uint64_t m, int q, bool lowGapHalf, Dec& lo, Dec& hi) {
  if (lowGapHalf) dyadic(4 * m - 1, q - 2, lo);
  else dyadic(2 * m - 1, q - 1, lo);
  dyadic(2 * m + 1, q - 1, hi);
}
}  // namespace exact

// snprintf(o, ..., "%.*g", p, v) for finite v (o: 40 bytes)
inline int fmtG(char* o, double v, int p) {
  if (p < 1) p = 1;
  const std::uint64_t b = exact::bitsOf(v);
  const bool neg = (b >> 63) != 0;
  const int ef = static_cast<int>((b >> 52) & 0x7ff);
  const std::uint64_t f = b & ((std::uint64_t{1} << 52) - 1);
  if (ef == 0 && f == 0) {
    int n = 0;
    if (neg) o[n++] = '-';
    o[n++] = '0';
    o[n] = 0;
    return n;
  }
  exact::Dec d, r;
  exact::dyadic(ef ? f | (std::uint64_t{1} << 52) : f, ef ? ef - 1075 : -1074, d);
  exact::roundTo(d, p, r);
  return exact::gText(o, neg, r, p);
}
// the text putNum() writes: shortest %.*g that strtod reads back to v
inline void shortestNum(char* o, double v) {
  const std::uint64_t b = exact::bitsOf(v);
  const int ef = static_cast<int>((b >> 52) & 0x7ff);
  const std::uint64_t f = b & ((std::uint64_t{1} << 52) - 1);
  if (ef == 0 && f == 0) {
    fmtG(o, v, 1);
    return;
  }
  const std::uint64_t m = ef ? f | (std::uint64_t{1} << 52) : f;
  const int q = ef ? ef - 1075 : -1074;
  exact::Dec d, lo, hi, r;
  exact::dyadic(m, q, d);
  exact::bounds(m, q, f == 0 && ef > 1, lo, hi);
  const bool even = (m & 1) == 0;
  for (int p = 1; p <= 17; ++p) {
    exact::roundTo(d, p, r);
    const int a = exact::cmp(r, lo), c = exact::cmp(r, hi);
    if (p == 17 || ((a > 0 || (a == 0 && even)) && (c < 0 || (c == 0 && even)))) {
      exact::gText(o, (b >> 63) != 0, r, p);
      return;
    }
  }
}
// the text putFloat() writes: shortest %.*g that strtod reads back to a double that rounds to v
inline void shortestFloat(char* o, float v) {
  const std::uint32_t b = exact::bitsOf(v);
  const int ef = static_cast<int>((b >> 23) & 0xff);
  const std::uint32_t f = b & ((1u << 23) - 1);
  if (ef == 0 && f == 0) {
    fmtG(o, static_cast<double>(v), 1);
    return;
  }
  const std::uint64_t m = ef ? f | (1u << 23) : f;
  const int q = ef ? ef - 150 : -149;
  // The float's rounding interval ends B are doubles; a decimal reads to B itself within half a double ulp of B, and
  // B then rounds to the even float of the two.
  struct End {
    exact::Dec below, above;  // the decimals that read to exactly B: [below, above]
  };
  auto endOf = [](std::uint64_t bm, int bq, End& e) {
    while (bm < (std::uint64_t{1} << 52)) {
      bm <<= 1;
      --bq;
    }
    exact::bounds(bm, bq, bm == (std::uint64_t{1} << 52), e.below, e.above);
  };
  static End lo, hi;  // static: large for the wasm stack
  if (f == 0 && ef > 1) endOf(4 * m - 1, q - 2, lo);
  else endOf(2 * m - 1, q - 1, lo);
  endOf(2 * m + 1, q - 1, hi);
  exact::Dec d, r;
  exact::dyadic(m, q, d);
  const bool even = (m & 1) == 0;
  for (int p = 1; p <= 9; ++p) {
    exact::roundTo(d, p, r);
    // strictly between the two ends: reads to a double inside the interval; on an end: the tie goes to the even float
    const bool inside = exact::cmp(r, lo.above) > 0 && exact::cmp(r, hi.below) < 0;
    const bool onEnd = (exact::cmp(r, lo.below) >= 0 && exact::cmp(r, lo.above) <= 0) ||
                       (exact::cmp(r, hi.below) >= 0 && exact::cmp(r, hi.above) <= 0);
    if (p == 9 || inside || (onEnd && even)) {
      exact::gText(o, (b >> 31) != 0, r, p);
      return;
    }
  }
}
inline void intText(char* o, long v) {
  char t[24];
  int k = 0;
  unsigned long u = v < 0 ? 0ul - static_cast<unsigned long>(v) : static_cast<unsigned long>(v);
  do {
    t[k++] = static_cast<char>('0' + u % 10);
    u /= 10;
  } while (u);
  int n = 0;
  if (v < 0) o[n++] = '-';
  while (k) o[n++] = t[--k];
  o[n] = 0;
}

// The writer, on any sink with put(const char*), put(char) and num(double) / flt(float) (the number text).
template <class Out>
void putStr(Out& s, const char* a, const char* b = nullptr, const char* c = nullptr) {
  s.put('"');
  const char* const parts[3] = {a, b, c};
  for (const char* t : parts)
    for (; t && *t; ++t) {
      if (*t == '"' || *t == '\\') s.put('\\');
      s.put(*t);
    }
  s.put('"');
}
template <class Out>
void putInt(Out& s, long v) {
  char b[24];
  intText(b, v);
  s.put(b);
}
// A sink into a caller's buffer (no libc): text up to cap - 1 bytes, then ok = false.
struct BufOut {
  char* p;
  unsigned long cap;
  unsigned long n = 0;
  bool ok = true;
  BufOut(char* buf, unsigned long size) : p(buf), cap(size) {
    if (cap) p[0] = 0;
  }
  void put(char c) {
    if (n + 1 >= cap) {
      ok = false;
      return;
    }
    p[n++] = c;
    p[n] = 0;
  }
  void put(const char* t) {
    while (*t) put(*t++);
  }
  void num(double v) {
    char b[40];
    shortestNum(b, v);
    put(b);
  }
  void flt(float v) {
    char b[40];
    shortestFloat(b, v);
    put(b);
  }
};
#ifndef SHOGUN_NO_FORMAT
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
// The native sink: std::string, numbers by libc.
struct StringOut {
  std::string s;
  void put(char c) { s += c; }
  void put(const char* t) { s += t; }
  void num(double v) { putNum(s, v); }
  void flt(float v) { putFloat(s, v); }
};
#endif
}  // namespace patchjson

// The patch as JSON. full = every parameter (a saved state); otherwise only those that differ from INIT and only
// non-default step fields (the factory bank style). Deterministic: the same patch gives the same bytes, on either
// sink (libc numbers natively, the exact formatter in the wasm build).
template <class Out>
void writePatchJson(Out& s, const Patch& pt, bool full) {
  using patchjson::putInt;
  using patchjson::putStr;
  s.put("{\"format\": \"shogun-patch\", \"version\": 2, \"name\": ");
  putStr(s, pt.name);
  s.put(",\n \"params\": {");
  bool first = true;
  for (int i = 0; i < kParamCount; ++i) {
    if (!full && dsp::exactEq(static_cast<float>(pt.u[i]), kParams[i].def)) continue;
    s.put(first ? "\n  " : ",\n  ");
    first = false;
    putStr(s, kParams[i].id);
    s.put(": ");
    s.flt(static_cast<float>(pt.u[i]));
  }
  s.put("},\n \"mod\": [");
  first = true;
  for (const mod::Row& r : pt.rows) {
    if (r.src == mod::SRC_NONE || r.dst < 0) continue;
    s.put(first ? "\n  {\"src\": " : ",\n  {\"src\": ");
    first = false;
    if (mod::perVoiceSource(r.src) && r.srcVoice >= 0) putStr(s, kStateSrcNames[r.src], "@", kVoiceNames[r.srcVoice]);
    else putStr(s, kStateSrcNames[r.src]);
    s.put(", \"dst\": ");
    putStr(s, kParams[r.dst].id);
    s.put(", \"depth\": ");
    s.num(r.depth);
    s.put(", \"via\": ");
    if (r.via == mod::SRC_NONE) s.put("null");
    else if (mod::perVoiceSource(r.via) && r.viaVoice >= 0) putStr(s, kStateSrcNames[r.via], "@", kVoiceNames[r.viaVoice]);
    else putStr(s, kStateSrcNames[r.via]);
    s.put(", \"curve\": ");
    putStr(s, kStateCurveNames[r.curve & 3]);
    s.put(r.on ? ", \"on\": true}" : ", \"on\": false}");
  }
  s.put("],\n \"cables\": [");
  for (int i = 0; i < pt.nCables; ++i) {
    s.put(i ? ", [" : "[");
    putStr(s, "SHOGUN/", kPortTable[pt.cableFrom[i]].id);
    s.put(", ");
    putStr(s, "SHOGUN/", kPortTable[pt.cableTo[i]].id);
    s.put("]");
  }
  s.put("],\n \"cvAmt\": {");
  first = true;
  for (int i = 0; i < kPorts; ++i)
    if (kPortTable[i].dir == PortDir::In && !dsp::exactEq(pt.cvAmt[i], 1.0)) {
      s.put(first ? "" : ", ");
      first = false;
      putStr(s, kPortTable[i].id);
      s.put(": ");
      s.num(pt.cvAmt[i]);
    }
  s.put("},\n \"inLaw\": {");
  first = true;
  for (int i = 0; i < kPorts; ++i)
    if (pt.inLaw[i] != 0) {
      s.put(first ? "" : ", ");
      first = false;
      putStr(s, kPortTable[i].id);
      s.put(": ");
      putInt(s, pt.inLaw[i]);
    }
  s.put("},\n \"seq\": {\"pattern\": ");
  putStr(s, pt.pattern.name);
  s.put(", \"seed\": ");
  putInt(s, static_cast<long>(pt.pattern.seed));
  s.put(", \"tracks\": [");
  const Step def;
  const Track defT;
  for (int t = 0; t < 16; ++t) {
    const Track& tr = pt.pattern.tracks[t];
    s.put(t ? ",\n  {\"id\": " : "\n  {\"id\": ");
    putStr(s, kVoiceNames[t]);
    s.put(", \"len\": ");
    putInt(s, tr.len);
    if (full || tr.scale >= 0) {
      s.put(", \"scale\": ");
      if (tr.scale < 0) s.put("null");
      else putStr(s, kStateScaleNames[tr.scale & 3]);
    }
    if (full || tr.swing >= 0.0) {
      s.put(", \"swing\": ");
      if (tr.swing < 0.0) s.put("null");
      else s.num(tr.swing);
    }
    if (full || !dsp::exactEq(tr.shift, defT.shift)) {
      s.put(", \"shift\": ");
      s.num(tr.shift);
    }
    s.put(", \"steps\": [");
    bool firstStep = true;
    for (int i = 0; i < kMaxSteps; ++i) {
      const Step& st = tr.steps[i];
      if (!st.on && st.nLocks == 0) continue;
      s.put(firstStep ? "\n   {\"i\": " : ",\n   {\"i\": ");
      firstStep = false;
      putInt(s, i);
      if (full || !st.on) s.put(st.on ? ", \"on\": true" : ", \"on\": false");
      if (full || st.acc != def.acc) s.put(", \"acc\": "), putInt(s, st.acc);
      if (full || !dsp::exactEq(st.prob, def.prob)) s.put(", \"prob\": "), s.flt(st.prob);
      if (full || !dsp::exactEq(st.micro, def.micro)) s.put(", \"micro\": "), s.flt(st.micro);
      if (full || st.flam != def.flam) s.put(", \"flam\": "), putInt(s, st.flam);
      if (full || st.ratchet != def.ratchet) s.put(", \"ratchet\": "), putInt(s, st.ratchet);
      if (full || !dsp::exactEq(st.bend, def.bend)) s.put(", \"bend\": "), s.flt(st.bend);
      if (!isDrum(t)) {
        if (full || st.note != def.note) s.put(", \"note\": "), putInt(s, st.note);
        if (full || st.tie) s.put(st.tie ? ", \"tie\": true" : ", \"tie\": false");
      }
      if (full || st.nLocks) {
        s.put(", \"locks\": {");
        for (int k = 0; k < st.nLocks; ++k) {
          s.put(k ? ", " : "");
          putStr(s, kParams[st.locks[k].param].id);
          s.put(": ");
          s.flt(st.locks[k].u);
        }
        s.put("}");
      }
      s.put("}");
    }
    s.put("]}");
  }
  s.put("]}}\n");
}

#ifndef SHOGUN_NO_FORMAT
inline std::string patchToJson(const Patch& pt, bool full) {
  patchjson::StringOut o;
  writePatchJson(o, pt, full);
  return o.s;
}
#endif

}  // namespace shogun
