// C entry points for the SHOGUN web page (spec v2.2 §15.3): the same engine as the plugin, compiled to wasm and run
// at the AudioContext rate (no resampler). Two layers:
//   * the v2 API (sg_param_*, sg_port_*, sg_cable, sg_step, sg_hit, sg_note_*): SECTION:LABEL ids, u ∈ [0, 1];
//   * the old page API (sg_set_knob, sg_set_drum, sg_patch, …), kept so web/page keeps working while its panel is
//     redesigned: old CC values map to u = cc/127, shuffle s to swing 0.5 + s/90, old jacks through the alias table.

#include "shogun.h"

#ifdef __wasm__
#define EXPORT(name) extern "C" __attribute__((export_name(#name)))

// ---------------------------------------------------------------- freestanding runtime (no libc in the wasm build)
extern "C" void* memcpy(void* d, const void* s, unsigned long n) {
  auto* a = static_cast<unsigned char*>(d);
  const auto* b = static_cast<const unsigned char*>(s);
  while (n--) *a++ = *b++;
  return d;
}
extern "C" void* memmove(void* d, const void* s, unsigned long n) {
  auto* a = static_cast<unsigned char*>(d);
  const auto* b = static_cast<const unsigned char*>(s);
  if (a < b) {
    while (n--) *a++ = *b++;
  } else {
    a += n;
    b += n;
    while (n--) *--a = *--b;
  }
  return d;
}
extern "C" void* memset(void* d, int c, unsigned long n) {
  auto* a = static_cast<unsigned char*>(d);
  while (n--) *a++ = static_cast<unsigned char>(c);
  return d;
}
extern "C" int strcmp(const char* a, const char* b) {
  while (*a && *a == *b) {
    ++a;
    ++b;
  }
  return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}
extern "C" const char* strchr(const char* s, int c) {
  for (;; ++s) {
    if (*s == static_cast<char>(c)) return s;
    if (!*s) return nullptr;
  }
}
extern "C" unsigned long strlen(const char* s) {
  unsigned long n = 0;
  while (s[n]) ++n;
  return n;
}
extern "C" char* strncpy(char* d, const char* s, unsigned long n) {
  unsigned long i = 0;
  for (; i < n && s[i]; ++i) d[i] = s[i];
  for (; i < n; ++i) d[i] = 0;
  return d;
}
// snprintf: %s, %d, %c and %.*s (all the engine needs once formatParam is compiled out).
extern "C" int snprintf(char* out, unsigned long n, const char* f, ...) {
  __builtin_va_list ap;
  __builtin_va_start(ap, f);
  unsigned long k = 0;
  auto put = [&](char c) {
    if (k + 1 < n) out[k] = c;
    ++k;
  };
  for (; *f; ++f) {
    if (*f != '%') {
      put(*f);
      continue;
    }
    ++f;
    int prec = -1;
    if (f[0] == '.' && f[1] == '*') {
      prec = __builtin_va_arg(ap, int);
      f += 2;
    }
    if (*f == 's') {
      const char* s = __builtin_va_arg(ap, const char*);
      for (int i = 0; s[i] && (prec < 0 || i < prec); ++i) put(s[i]);
    } else if (*f == 'd') {
      int v = __builtin_va_arg(ap, int);
      char buf[12];
      int m = 0;
      unsigned u = v < 0 ? 0u - static_cast<unsigned>(v) : static_cast<unsigned>(v);
      do {
        buf[m++] = static_cast<char>('0' + u % 10);
        u /= 10;
      } while (u);
      if (v < 0) put('-');
      while (m) put(buf[--m]);
    } else if (*f == 'c') {
      put(static_cast<char>(__builtin_va_arg(ap, int)));
    } else if (*f == '%') {
      put('%');
    }
  }
  if (n) out[k < n ? k : n - 1] = 0;
  __builtin_va_end(ap);
  return static_cast<int>(k);
}

// Heap: wasm memory.grow, with exact-size free lists (prepare() frees and re-allocates the same sizes).
namespace {
struct FreeBlock {
  FreeBlock* next;
  unsigned long size;
};
FreeBlock* gFree = nullptr;
unsigned long gTop = 0, gEnd = 0;
void* alloc(unsigned long n) {
  n = (n + 15) & ~15ul;
  for (FreeBlock** p = &gFree; *p; p = &(*p)->next)
    if ((*p)->size == n) {
      FreeBlock* b = *p;
      *p = b->next;
      return reinterpret_cast<unsigned char*>(b) + 16;
    }
  const unsigned long need = n + 16;
  if (gTop + need > gEnd) {
    const unsigned long pages = (need + 65535) / 65536 + 16;
    const long old = __builtin_wasm_memory_grow(0, pages);
    if (old < 0) __builtin_trap();
    if (gEnd != static_cast<unsigned long>(old) * 65536ul || gTop == 0) gTop = static_cast<unsigned long>(old) * 65536ul;
    gEnd = (static_cast<unsigned long>(old) + pages) * 65536ul;
  }
  auto* h = reinterpret_cast<unsigned long*>(gTop);
  h[0] = n;
  gTop += need;
  return reinterpret_cast<unsigned char*>(h) + 16;
}
void release(void* p) {
  if (!p) return;
  auto* b = reinterpret_cast<FreeBlock*>(static_cast<unsigned char*>(p) - 16);
  b->size = *reinterpret_cast<unsigned long*>(b);
  b->next = gFree;
  gFree = b;
}
}  // namespace
void* operator new(unsigned long n) { return alloc(n); }
void* operator new[](unsigned long n) { return alloc(n); }
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, unsigned long) noexcept { release(p); }
void operator delete[](void* p, unsigned long) noexcept { release(p); }
extern "C" void __cxa_pure_virtual() { __builtin_trap(); }
extern "C" int __cxa_atexit(void (*)(void*), void*, void*) { return 0; }
void* __dso_handle = nullptr;
#else
// Native build, for tests/web_parity.cpp.
#define EXPORT(name) extern "C"
#endif

using namespace shogun;

namespace {

constexpr int kBlock = 1024;
Engine* gE = nullptr;
Pattern gPattern;
float gL[kBlock];
float gR[kBlock];
char gText[128];  // string exchange with the page (ids in and out)
// Before the first processed sample, parameter changes apply at once (like the plugin's prepareToPlay); afterwards they
// take the engine's 5 ms smoothing.
bool gFresh = true;
void applyU(int p, double u) {
  u = u < 0 ? 0 : (u > 1 ? 1 : u);
  if (gFresh) gE->setParamNow(p, u);
  else gE->setParam(p, u);
}

// Old page voice order (sim) → v2 voice ids.
constexpr int kOldVoice[16] = {BD1, BD2, SD, RS, CY, OH, CH, CL, CP, LTC, MTC, HTC, CB, MA, LEAD, BASS};
int voiceOf(int old) { return old >= 0 && old < 16 ? kOldVoice[old] : -1; }

void setU(const char* id, double u) {
  const int p = findParam(id);
  if (p >= 0) applyU(p, u);
}
void setVoiceU(int v, const char* label, double u) {
  char buf[48];
  std::snprintf(buf, sizeof buf, "%s:%s", kVoiceNames[v], label);
  setU(buf, u);
}

// Old knob names (CC 0..127) → v2 parameter ids.
struct OldKnob {
  const char* name;
  const char* id;
};
const OldKnob kKnobs[] = {
    {"bd1Attack", "BD1:ATTACK"}, {"bd1Decay", "BD1:DECAY"}, {"bd1Pitch", "BD1:PITCH"}, {"bd1Tune", "BD1:TUNE"},
    {"bd1Noise", "BD1:NOISE"}, {"bd1Filter", "BD1:FILTER"}, {"bd1Dist", "BD1:DRIVE"}, {"bd1Trigger", "BD1:SOUND"},
    {"bd2Decay", "BD2:DECAY"}, {"bd2Tune", "BD2:TUNE"}, {"bd2Tone", "BD2:TONE"}, {"sdTune", "SD:TUNE"},
    {"sdDTune", "SD:DETUNE"}, {"sdSnappy", "SD:SNAPPY"}, {"sdSnDecay", "SD:SN.DEC"}, {"sdTone", "SD:TONE"},
    {"sdToneDecay", "SD:T.DECAY"}, {"sdPitch", "SD:PITCH"}, {"rsTune", "RS:TUNE"}, {"cyDecay", "CY:DECAY"},
    {"cyTone", "CY:TONE"}, {"cyTune", "CY:TUNE"}, {"ohDecay", "OH:DECAY"}, {"hhTune", "CH:TUNE"},
    {"hhDecay", "CH:DECAY"}, {"clTune", "CL:TUNE"}, {"clDecay", "CL:DECAY"}, {"cpDecay", "CP:DECAY"},
    {"cpFilter", "CP:FILTER"}, {"cpAttack", "CP:ATTACK"}, {"cpTrigger", "CP:COUNT"}, {"cpData", "CP:SOUND"},
    {"htcTune", "HTC:TUNE"}, {"htcDecay", "HTC:DECAY"}, {"htcNoise", "HTC:NZ"}, {"htcMode", "HTC:CONGA"},
    {"mtcTune", "MTC:TUNE"}, {"mtcDecay", "MTC:DECAY"}, {"mtcNoise", "MTC:NZ"}, {"mtcMode", "MTC:CONGA"},
    {"ltcTune", "LTC:TUNE"}, {"ltcDecay", "LTC:DECAY"}, {"ltcNoise", "LTC:NZ"}, {"ltcMode", "LTC:CONGA"},
    {"tomNoise", "TOM:NOISE"}, {"cbTune", "CB:TUNE"}, {"cbDecay", "CB:DECAY"}, {"maDecay", "MA:DECAY"},
    {"leadTone", "LEAD:CUTOFF"}, {"bassTone", "BASS:CUTOFF"}, {"bd1Wave", "BD1:WAVE"}, {"bd2Wave", "BD2:WAVE"},
    {"ltcWave", "LTC:WAVE"}, {"mtcWave", "MTC:WAVE"}, {"htcWave", "HTC:WAVE"}};
constexpr int kKnobCount = static_cast<int>(sizeof kKnobs / sizeof kKnobs[0]);

// Old page jacks: 0..15 voice TRIG (old order), 16 RST IN, 17 RUN IN, 18 CLK IN, 19 BD1 PITCH, 20 BD2 PITCH,
// 21 SD PITCH, 22 TOM PITCH, 23 HAT DECAY, 24 SD SNAPPY. Sources: 1 CLK OUT, 2 ACC OUT, 4 LFO OUT (= MOD:LFO 1).
constexpr int kOldInputs = 25;
int gPatch[kOldInputs];
int targets(int input, int out[3], double* amt) {
  *amt = 1.0;
  if (input < 16) {
    const int v = voiceOf(input);
    out[0] = isDrum(v) ? drumPort(v, DJ_TRIG) : synthPort(v - LEAD, SJ_GATE);
    return 1;
  }
  int law = 0;
  static const char* const ids[] = {"CLOCK:RST IN", "CLOCK:RUN IN", "CLOCK:CLK IN", "BD1:PITCH", "BD2:PITCH",
                                    "SD:PITCH", "TOM:PITCH", "HAT:DECAY", "SD:SNAPPY"};
  if (input >= kOldInputs) return 0;
  const int n = resolvePort(ids[input - 16], out, &law);
  // The sim's drum PITCH was 1 V/semitone: migrated cables keep that with CV AMT = 1/12 (§12.3).
  if (input >= 19 && input <= 22) *amt = 1.0 / 12.0;
  return n;
}

}  // namespace

// ================================================================ v2 API

EXPORT(sg_init) void sg_init(double fs) {
  if (!(fs > 0)) fs = 48000.0;
  if (!gE) gE = new Engine();
  gE->prepare(fs, 2);  // coefficients at the AudioContext rate (§3.1), 2× OS, latency 23 samples
  gE->loadInit();
  gE->setRunning(false);
  gPattern = gE->pattern();
  for (int i = 0; i < kOldInputs; ++i) gPatch[i] = 0;
  gFresh = true;
}
EXPORT(sg_sample_rate) double sg_sample_rate() { return gE->sampleRate(); }
EXPORT(sg_latency) int sg_latency() { return gE->latencySamples(); }
EXPORT(sg_set_os) void sg_set_os(int os) { gE->prepare(gE->sampleRate(), os == 1 || os == 4 ? os : 2); }
EXPORT(sg_text) char* sg_text() { return gText; }

EXPORT(sg_param_count) int sg_param_count() { return kParamCount; }
EXPORT(sg_param_id) const char* sg_param_id(int i) { return i >= 0 && i < kParamCount ? kParams[i].id : ""; }
EXPORT(sg_param_default) double sg_param_default(int i) { return i >= 0 && i < kParamCount ? kParams[i].def : 0.0; }
EXPORT(sg_param_find) int sg_param_find() { return findParam(gText); }  // id written to sg_text()
EXPORT(sg_set_param) void sg_set_param(int i, double u) {
  if (i >= 0 && i < kParamCount) applyU(i, u);
}
EXPORT(sg_param) double sg_param(int i) { return i >= 0 && i < kParamCount ? gE->param(i) : 0.0; }

EXPORT(sg_port_count) int sg_port_count() { return kPorts; }
EXPORT(sg_port_id) const char* sg_port_id(int i) { return i >= 0 && i < kPorts ? kPortTable[i].id : ""; }
EXPORT(sg_port_role) int sg_port_role(int i) { return i >= 0 && i < kPorts ? static_cast<int>(kPortTable[i].role) : -1; }
EXPORT(sg_port_find) int sg_port_find() { return portFromId(gText); }  // any R6 form, shared parseJackId
EXPORT(sg_port_volts) double sg_port_volts(int i) { return i >= 0 && i < kPorts ? gE->portValues()[i] : 0.0; }
EXPORT(sg_cable) int sg_cable(int from, int to, int on) {
  if (on) return gE->addCable(from, to) ? 1 : 0;
  gE->removeCable(from, to);
  return 1;
}
EXPORT(sg_cable_clear) void sg_cable_clear() { gE->clearCables(); }
EXPORT(sg_set_cv_amt) void sg_set_cv_amt(int port, double amt) { gE->setCvAmt(port, amt); }

EXPORT(sg_track) void sg_track(int t, int len, int scale, double swing, double shift) {
  if (t < 0 || t >= 16) return;
  Track& tr = gPattern.tracks[t];
  tr.len = len < 1 ? 1 : (len > kMaxSteps ? kMaxSteps : len);
  tr.scale = scale < -1 ? -1 : (scale > 3 ? 3 : scale);
  tr.swing = swing;
  tr.shift = shift;
}
EXPORT(sg_step) void sg_step(int t, int s, int on, int acc, double prob, double micro, int flam, int ratchet, double bend,
                             int note, int tie) {
  if (t < 0 || t >= 16 || s < 0 || s >= kMaxSteps) return;
  Step& st = gPattern.tracks[t].steps[s];
  st.on = on != 0;
  st.acc = static_cast<std::uint8_t>(acc < 1 ? 1 : (acc > 3 ? 3 : acc));
  st.prob = static_cast<float>(prob);
  st.micro = static_cast<float>(micro);
  st.flam = static_cast<std::uint8_t>(flam < 0 ? 0 : (flam > 16 ? 16 : flam));
  st.ratchet = static_cast<std::uint8_t>(ratchet < 1 ? 1 : ratchet);
  st.bend = static_cast<float>(bend);
  st.note = static_cast<std::int8_t>(note);
  st.tie = tie != 0;
}
EXPORT(sg_step_lock) int sg_step_lock(int t, int s, int param, double u) {
  if (t < 0 || t >= 16 || s < 0 || s >= kMaxSteps) return 0;
  return gPattern.tracks[t].steps[s].setLock(param, static_cast<float>(u)) ? 1 : 0;
}
EXPORT(sg_pattern_clear) void sg_pattern_clear() { gPattern = Pattern(); }
EXPORT(sg_commit) void sg_commit() { gE->setPattern(gPattern); }

EXPORT(sg_hit) void sg_hit(int v, double velVolts, double bend, int acc) { gE->trigger(v, velVolts, bend, acc); }
EXPORT(sg_note_on) void sg_note_on(int v, double note, double velVolts, int tie) { gE->noteOn(v, note, velVolts, tie != 0); }
EXPORT(sg_note_off) void sg_note_off(int v) { gE->noteOff(v); }
EXPORT(sg_set_running) void sg_set_running(int on) { gE->setRunning(on != 0); }
EXPORT(sg_restart) void sg_restart() { gE->restart(); }
EXPORT(sg_running) int sg_running() { return gE->running() ? 1 : 0; }
EXPORT(sg_counter) double sg_counter() { return static_cast<double>(gE->counter()); }
EXPORT(sg_display_step) int sg_display_step() { return gE->displayStep(); }
EXPORT(sg_out_l) float* sg_out_l() { return gL; }
EXPORT(sg_out_r) float* sg_out_r() { return gR; }
EXPORT(sg_process) void sg_process(int n) {
  if (n > kBlock) n = kBlock;
  if (n > 0) gFresh = false;
  for (int i = 0; i < n; ++i) {
    gE->processSample();
    gL[i] = static_cast<float>(gE->mainL());
    gR[i] = static_cast<float>(gE->mainR());
  }
}

// ================================================================ old page API (compat)

EXPORT(sg_knob_count) int sg_knob_count() { return kKnobCount; }
EXPORT(sg_knob_name) const char* sg_knob_name(int i) { return i >= 0 && i < kKnobCount ? kKnobs[i].name : ""; }
EXPORT(sg_set_knob) void sg_set_knob(int i, int cc) {
  if (i >= 0 && i < kKnobCount) setU(kKnobs[i].id, cc / 127.0);  // §14: u = cc/127
}
// level x: linear gain → LEVEL u (gLevel = 1.4125 u²); master x → MASTER:VOLUME u (2 u²).
EXPORT(sg_set_level) void sg_set_level(int v, double x) {
  const int nv = voiceOf(v);
  if (nv >= 0) setVoiceU(nv, "LEVEL", x > 0 ? std::sqrt(x / 1.4125) : 0.0);
}
EXPORT(sg_set_master) void sg_set_master(double x) { setU("MASTER:VOLUME", x > 0 ? std::sqrt(x / 2.0) : 0.0); }
EXPORT(sg_set_solo) void sg_set_solo(int v) {
  for (int k = 0; k < 16; ++k) setVoiceU(voiceOf(k), "SOLO", k == v ? 1.0 : 0.0);
}
EXPORT(sg_set_mode) void sg_set_mode(int ext) {
  setU("CLOCK:SOURCE", stepU(SRC_INT, 3));
  setU("CLOCK:MODE", stepU(ext ? 1 : 0, 2));
}
EXPORT(sg_set_tempo) void sg_set_tempo(double bpm) {
  setU("CLOCK:SOURCE", stepU(SRC_INT, 3));
  setU("CLOCK:TEMPO", (bpm - 40.0) / 160.0);
}
EXPORT(sg_set_scale) void sg_set_scale(int stepsPerQuarter) {
  const int idx = stepsPerQuarter >= 8 ? 0 : (stepsPerQuarter == 4 ? 1 : (stepsPerQuarter == 3 ? 2 : 3));
  setU("CLOCK:SCALE", stepU(idx, 4));
}
EXPORT(sg_set_bar) void sg_set_bar(int len) { setU("CLOCK:BAR", stepU((len < 1 ? 1 : (len > 32 ? 32 : len)) - 1, 32)); }
// The page LFO is LFO 1: tempo-synced, unipolar, DEPTH = amount, phase 0 at transport start. Its jack is MOD:LFO 1.
EXPORT(sg_set_lfo) void sg_set_lfo(double cyclesPerBeat, double phase, int shape, double amount) {
  int div = 0;
  double best = 1e9;
  const double beats = cyclesPerBeat > 0 ? 1.0 / cyclesPerBeat : 32.0;
  for (int i = 0; i < kLfoDivCount; ++i) {
    const double d = std::fabs(std::log2(kLfoDivBeats[i] / beats));
    if (d < best) {
      best = d;
      div = i;
    }
  }
  static const int kShape[5] = {mod::L_SIN, mod::L_TRI, mod::L_RAMP, mod::L_SQR, mod::L_SH};
  setU("LFO 1:SYNC", 1.0);
  setU("LFO 1:DIV", stepU(div, kLfoDivCount));
  setU("LFO 1:PHASE", phase);
  setU("LFO 1:SHAPE", stepU(kShape[shape < 0 || shape > 4 ? 0 : shape], 6));
  setU("LFO 1:POL", 0.0);
  setU("LFO 1:MODE", stepU(mod::M_FREE_RUN, 3));
  setU("LFO 1:DEPTH", amount);
}
EXPORT(sg_lfo_volts) double sg_lfo_volts() { return gE->portValues()[findPort("MOD:LFO 1")]; }
EXPORT(sg_lfo_phase) double sg_lfo_phase() { return gE->modulation().lfo[0].instanceFor(-1).ph; }

EXPORT(sg_set_track) void sg_set_track(int v, int len, int shuffle, int shiftCc, int mute) {
  const int t = voiceOf(v);
  if (t < 0) return;
  Track& tr = gPattern.tracks[t];
  tr.len = len < 1 ? 1 : (len > kMaxSteps ? kMaxSteps : len);
  tr.swing = swingFromShuffle(shuffle < 0 ? 0 : (shuffle > 15 ? 15 : shuffle));  // §10.2
  tr.shift = shiftCc / 127.0;
  setVoiceU(t, "MUTE", mute ? 1.0 : 0.0);
}
// accent 0..2 → 1..3; flam −1 none / index 0..15; bend −1 none / CC → 12·(2u − 1) semitones.
EXPORT(sg_set_drum) void sg_set_drum(int v, int s, int on, int accent, int flam, int bend) {
  const int t = voiceOf(v);
  if (t < 0 || s < 0 || s >= kMaxSteps) return;
  Step& st = gPattern.tracks[t].steps[s];
  st.on = on != 0;
  st.acc = static_cast<std::uint8_t>(accent < 0 ? 1 : (accent > 2 ? 3 : accent + 1));
  st.flam = static_cast<std::uint8_t>(flam >= 0 ? flam + 1 : 0);
  st.bend = bend >= 0 ? static_cast<float>(12.0 * (2.0 * bend / 127.0 - 1.0)) : 0.0f;
}
EXPORT(sg_set_note) void sg_set_note(int v, int s, int note, int accent, int tie) {
  const int t = voiceOf(v);
  if (t < 0 || s < 0 || s >= kMaxSteps) return;
  Step& st = gPattern.tracks[t].steps[s];
  st.on = note >= 0;
  if (note >= 0) st.note = static_cast<std::int8_t>(note);
  st.acc = static_cast<std::uint8_t>(accent < 0 ? 1 : (accent > 2 ? 3 : accent + 1));
  st.tie = tie != 0;
}
// gain: the old g_vel → VEL volts (g_vel = 0.15 + 0.85·V/5).
EXPORT(sg_trigger) void sg_trigger(int v, double gain, double bend) {
  const int nv = voiceOf(v);
  double volts = 5.0 * (gain - 0.15) / 0.85;
  volts = volts < 0 ? 0 : (volts > 5 ? 5 : volts);
  if (nv >= 0 && isDrum(nv)) gE->trigger(nv, volts, bend, 3);
  else if (nv >= 0) gE->noteOn(nv, 48, volts);
}
EXPORT(sg_trigger_note) void sg_trigger_note(int v, int note, double gain) {
  const int nv = voiceOf(v);
  double volts = 5.0 * (gain - 0.15) / 0.85;
  volts = volts < 0 ? 0 : (volts > 5 ? 5 : volts);
  if (nv >= 0) gE->noteOn(nv, note, volts);
}
EXPORT(sg_release) void sg_release(int v) {
  const int nv = voiceOf(v);
  if (nv >= 0) gE->noteOff(nv);
}
// source mask into an old jack: cables from CLK OUT / ACC OUT / MOD:LFO 1 into the v2 port(s) the old jack aliases.
EXPORT(sg_patch) void sg_patch(int input, int source) {
  if (input < 0 || input >= kOldInputs) return;
  static const char* const src[3] = {"CLOCK:CLK OUT", "CLOCK:ACC OUT", "MOD:LFO 1"};
  int to[3];
  double amt = 1.0;
  const int n = targets(input, to, &amt);
  for (int k = 0; k < n; ++k) {
    for (int b = 0; b < 3; ++b) gE->removeCable(findPort(src[b]), to[k]);
    for (int b = 0; b < 3; ++b)
      if (source & (1 << b)) gE->addCable(findPort(src[b]), to[k]);
    gE->setCvAmt(to[k], amt);
  }
  gPatch[input] = source;
}
