// C entry points for the SHOGUN web page. One engine, driven from an AudioWorklet.
// The page keeps the pattern and knobs; this file turns them into engine calls and
// patches the panel's own CLK OUT gate into the TRIG, RST IN, RUN IN and CLK IN jacks.

#include "shogun.h"

#ifdef __wasm__
#define EXPORT(name) extern "C" __attribute__((export_name(#name)))

// Freestanding: no libc in the wasm build.
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
inline void* operator new(unsigned long, void* p) noexcept { return p; }
#else
// Native build, for tests/web_parity.cpp.
#include <new>
#define EXPORT(name) extern "C"
#endif

namespace {

using shogun::Knobs;

struct KnobField {
  const char* name;
  int Knobs::*field;
};

#define K(f) {#f, &Knobs::f}
const KnobField kKnobs[] = {
    K(bd1Attack), K(bd1Decay), K(bd1Pitch), K(bd1Tune), K(bd1Noise), K(bd1Filter), K(bd1Dist), K(bd1Trigger),
    K(bd2Decay),  K(bd2Tune),  K(bd2Tone),  K(sdTune),  K(sdDTune),  K(sdSnappy),  K(sdSnDecay), K(sdTone),
    K(sdToneDecay), K(sdPitch), K(rsTune),  K(cyDecay), K(cyTone),   K(cyTune),    K(ohDecay),  K(hhTune),
    K(hhDecay),   K(clTune),   K(clDecay),  K(cpDecay), K(cpFilter), K(cpAttack),  K(cpTrigger), K(cpData),
    K(htcTune),   K(htcDecay), K(htcNoise), K(htcMode), K(mtcTune),  K(mtcDecay),  K(mtcNoise), K(mtcMode),
    K(ltcTune),   K(ltcDecay), K(ltcNoise), K(ltcMode), K(tomNoise), K(cbTune),    K(cbDecay),  K(maDecay),
    K(leadTone),  K(bassTone),  K(bd1Wave),  K(bd2Wave),  K(ltcWave),  K(mtcWave),  K(htcWave)};
#undef K
constexpr int kKnobCount = sizeof(kKnobs) / sizeof(kKnobs[0]);

constexpr int kBlock = 1024;
// 0..15 voice TRIG, 16 RST IN, 17 RUN IN, 18 CLK IN, then the CV inputs: 19 BD1 PITCH, 20 BD2 PITCH, 21 SD PITCH,
// 22 TOM PITCH, 23 HAT DECAY, 24 SD SNAPPY.
constexpr int kInputs = 25;
constexpr int kRst = 16, kRun = 17, kClk = 18;
constexpr int kBd1Pitch = 19, kBd2Pitch = 20, kSdPitch = 21, kTomPitch = 22, kHatDecay = 23, kSdSnappy = 24;
constexpr int kClkPulse = 240;  // CLK OUT gate, 5 ms at 48 kHz

alignas(16) unsigned char gStore[sizeof(shogun::Engine)];
shogun::Engine* gE = nullptr;
shogun::Knobs gKnobs;
shogun::Pattern gPattern;
float gL[kBlock];
float gR[kBlock];
int gPatch[kInputs];
bool gLast[kInputs];
int gClkLeft = 0;
int gAccLeft = 0;  // ACC OUT gate, opened with CLK OUT on an accented step
bool gKick = true;
std::int64_t gLastCounter = -1;

// The page's LFO: tempo-synced, retriggered when the transport starts. Its output is the LFO OUT jack.
double gBpm = 120.0;
double gLfoSpb = 4.0;     // cycles per beat: 1/16 is 4, so 120 BPM gives 8 Hz
double gLfoOffset = 0.0;  // PHASE, 0 to 1
int gLfoShape = 0;        // 0 sine, 1 triangle, 2 saw, 3 square (width 0.5), 4 sample and hold
double gLfoAmount = 0.0;  // 0 to 1
double gLfoPhase = 0.0;
double gLfoLastP = 0.0;
double gLfoHold = 0.0;
double gLfoVolts = 0.0;
unsigned gLfoRng = 0x9E3779B9u;
bool gWasRunning = false;
shogun::Knobs gApplied;   // the knobs the engine has now: the panel's knobs plus the CV inputs

}  // namespace

namespace {
// Source masks: 1 CLK OUT, 2 ACC OUT (gates, 0 or 5 V), 4 LFO OUT (CV).
bool high(int mask, bool clk, bool acc) { return ((mask & 1) && clk) || ((mask & 2) && acc); }
double volts(int mask, bool clk, bool acc) {
  return ((mask & 1) && clk ? 5.0 : 0.0) + ((mask & 2) && acc ? 5.0 : 0.0) + ((mask & 4) ? gLfoVolts : 0.0);
}

// Bipolar shape at phase p in [0, 1).
double lfoShape(double p) {
  switch (gLfoShape) {
    case 1: return 1.0 - 4.0 * std::fabs(p - 0.5);  // -1 at 0, 1 at 0.5
    case 2: return 2.0 * p - 1.0;
    case 3: return p < 0.5 ? 1.0 : -1.0;
    case 4: return gLfoHold;
    default: return std::sin(2.0 * shogun::kPi * p);
  }
}
double lfoNoise() {
  gLfoRng ^= gLfoRng << 13;
  gLfoRng ^= gLfoRng >> 17;
  gLfoRng ^= gLfoRng << 5;
  return static_cast<double>(gLfoRng) / 2147483647.5 - 1.0;
}
void lfoStep() {
  double p = gLfoPhase + gLfoOffset;
  p -= std::floor(p);
  if (p < gLfoLastP) gLfoHold = lfoNoise();  // a new held value each cycle
  gLfoLastP = p;
  gLfoVolts = gLfoAmount * 2.5 * (1.0 + lfoShape(p));  // amount 0 is 0 V; amount 1 is 0 to 5 V around 2.5 V
  gLfoPhase += gBpm / 60.0 * gLfoSpb / shogun::kFs;
  gLfoPhase -= std::floor(gLfoPhase);
}

// CV sums with the knob. Pitch: 1 V is a semitone (5 V about a fourth), turned into knob travel by each voice's
// exponential Tune law. Decay and Snappy: 5 V is the full knob travel, so decay CV follows the same short decay curve.
int addU(int cc, double du) {
  const long long v = std::llround(static_cast<double>(cc) + du * 127.0);
  return v < 0 ? 0 : (v > 127 ? 127 : static_cast<int>(v));
}
double cvVolts(int input, bool clk, bool acc) { return gPatch[input] ? volts(gPatch[input], clk, acc) : 0.0; }
void applyCv(bool clk, bool acc) {
  // semitones per unit of Tune travel: 12 * log2(fHi / fLo) of each voice
  constexpr double kStBd1 = 24.0, kStBd2 = 13.8245, kStSd = 20.8437, kStLtc = 16.3505, kStMtc = 17.8251, kStHtc = 18.1735;
  shogun::Knobs k = gKnobs;
  const double bd1 = cvVolts(kBd1Pitch, clk, acc), bd2 = cvVolts(kBd2Pitch, clk, acc), sd = cvVolts(kSdPitch, clk, acc);
  const double tom = cvVolts(kTomPitch, clk, acc), hat = cvVolts(kHatDecay, clk, acc), snap = cvVolts(kSdSnappy, clk, acc);
  k.bd1Tune = addU(k.bd1Tune, bd1 / kStBd1);
  k.bd2Tune = addU(k.bd2Tune, bd2 / kStBd2);
  k.sdTune = addU(k.sdTune, sd / kStSd);
  k.ltcTune = addU(k.ltcTune, tom / kStLtc);
  k.mtcTune = addU(k.mtcTune, tom / kStMtc);
  k.htcTune = addU(k.htcTune, tom / kStHtc);
  k.ohDecay = addU(k.ohDecay, hat / 5.0);
  k.hhDecay = addU(k.hhDecay, hat / 5.0);
  k.sdSnappy = addU(k.sdSnappy, snap / 5.0);
  if (k.bd1Tune != gApplied.bd1Tune || k.bd2Tune != gApplied.bd2Tune || k.sdTune != gApplied.sdTune ||
      k.ltcTune != gApplied.ltcTune || k.mtcTune != gApplied.mtcTune || k.htcTune != gApplied.htcTune ||
      k.ohDecay != gApplied.ohDecay || k.hhDecay != gApplied.hhDecay || k.sdSnappy != gApplied.sdSnappy) {
    gApplied = k;
    gE->setKnobs(k);
  }
}
// True when an unmuted drum track has a loud step (accent 2) at counter c. A track plays slot c % length.
bool accented(std::int64_t c) {
  if (c < 0) return false;
  const shogun::Pattern& p = gE->pattern();
  for (int v = 0; v < static_cast<int>(shogun::Voice::Lead); ++v) {
    const shogun::Track& t = p.track[v];
    if (t.mute || t.length < 1) continue;
    const shogun::DrumStep& d = t.drum[c % t.length];
    if (d.on && d.accent >= 2) return true;
  }
  return false;
}
}  // namespace

EXPORT(sg_init) void sg_init() {
  gE = new (gStore) shogun::Engine();
  gE->reset();  // the page sends its own kit, levels and steps; start it from the cleared state
  gKnobs = shogun::Knobs{};
  gPattern = shogun::Pattern{};
  for (int i = 0; i < kInputs; ++i) {
    gPatch[i] = 0;
    gLast[i] = false;
  }
  gClkLeft = gAccLeft = 0;
  gApplied = gKnobs;
  gBpm = 120.0;
  gLfoSpb = 4.0;
  gLfoOffset = gLfoPhase = gLfoLastP = gLfoHold = gLfoVolts = gLfoAmount = 0.0;
  gLfoShape = 0;
  gLfoRng = 0x9E3779B9u;
  gWasRunning = false;
  gE->setRunning(false);
}

EXPORT(sg_knob_count) int sg_knob_count() { return kKnobCount; }
EXPORT(sg_knob_name) const char* sg_knob_name(int i) { return i >= 0 && i < kKnobCount ? kKnobs[i].name : ""; }
EXPORT(sg_set_knob) void sg_set_knob(int i, int cc) {
  if (i < 0 || i >= kKnobCount) return;
  gKnobs.*(kKnobs[i].field) = cc;
  gE->setKnobs(gKnobs);
  gApplied = gKnobs;  // the CV is added again on the next sample
}

EXPORT(sg_set_level) void sg_set_level(int v, double x) { gE->setLevel(static_cast<shogun::Voice>(v), x); }
EXPORT(sg_set_master) void sg_set_master(double x) { gE->setMaster(x); }
EXPORT(sg_set_solo) void sg_set_solo(int v) { gE->setSolo(v); }
EXPORT(sg_set_mode) void sg_set_mode(int ext) { gE->setMode(ext ? shogun::ClockMode::Ext : shogun::ClockMode::Int); }
EXPORT(sg_set_tempo) void sg_set_tempo(double bpm) {
  gE->setTempo(bpm);
  gBpm = bpm;
}
// cyclesPerBeat: 1/16 is 4, 1/16 dotted 8/3, 1/16 triplet 6. phase 0..1. shape 0 sine, 1 tri, 2 saw, 3 square, 4 S&H.
EXPORT(sg_set_lfo) void sg_set_lfo(double cyclesPerBeat, double phase, int shape, double amount) {
  gLfoSpb = cyclesPerBeat > 0 ? cyclesPerBeat : 0;
  gLfoOffset = phase < 0 ? 0 : (phase > 1 ? 1 : phase);
  gLfoShape = shape < 0 || shape > 4 ? 0 : shape;
  gLfoAmount = amount < 0 ? 0 : (amount > 1 ? 1 : amount);
}
EXPORT(sg_lfo_volts) double sg_lfo_volts() { return gLfoVolts; }
EXPORT(sg_set_scale) void sg_set_scale(int stepsPerQuarter) { gE->setScaleSteps(stepsPerQuarter); }
EXPORT(sg_set_running) void sg_set_running(int on) {
  if (on && !gE->running()) gKick = true;
  gE->setRunning(on != 0);
}
EXPORT(sg_restart) void sg_restart() {
  gE->restart();
  gKick = true;
}

EXPORT(sg_set_bar) void sg_set_bar(int len) { gPattern.length = len; }
EXPORT(sg_set_track) void sg_set_track(int v, int len, int shuffle, int shiftCc, int mute) {
  if (v < 0 || v >= shogun::kVoiceCount) return;
  shogun::Track& t = gPattern.track[v];
  t.length = len;
  t.shuffle = shuffle;
  t.shiftCc = shiftCc;
  t.mute = mute != 0;
}
// flam: -1 none, else the flam table index 0..15. bend: -1 none, else the bend CC.
EXPORT(sg_set_drum) void sg_set_drum(int v, int s, int on, int accent, int flam, int bend) {
  if (v < 0 || v >= shogun::kVoiceCount || s < 0 || s >= shogun::kMaxSteps) return;
  shogun::DrumStep& d = gPattern.track[v].drum[s];
  d.on = on != 0;
  d.accent = accent;
  d.flam = flam >= 0;
  d.flamIndex = flam >= 0 ? flam : 0;
  d.bendCc = bend;
}
// note: -1 rest.
EXPORT(sg_set_note) void sg_set_note(int v, int s, int note, int accent, int tie) {
  if (v < 0 || v >= shogun::kVoiceCount || s < 0 || s >= shogun::kMaxSteps) return;
  shogun::NoteStep& n = gPattern.track[v].note[s];
  n.note = note;
  n.accent = accent;
  n.tie = tie != 0;
}
EXPORT(sg_commit) void sg_commit() { gE->setPattern(gPattern); }

EXPORT(sg_trigger) void sg_trigger(int v, double gain, double bend) {
  gE->trigger(static_cast<shogun::Voice>(v), gain, bend);
}
EXPORT(sg_trigger_note) void sg_trigger_note(int v, int note, double gain) {
  gE->triggerNote(static_cast<shogun::Voice>(v), note, gain);
}
EXPORT(sg_release) void sg_release(int v) { gE->release(static_cast<shogun::Voice>(v)); }

// source: a mask of the panel's outputs patched into the input: 1 CLK OUT, 2 ACC OUT, 4 LFO OUT. A stacked gate input
// is high while any of its gate sources is high; a CV input sums its sources.
EXPORT(sg_patch) void sg_patch(int input, int source) {
  if (input < 0 || input >= kInputs) return;
  gPatch[input] = source;
  if (input == kClk) gE->setExternalClock(source != 0);
}

EXPORT(sg_out_l) float* sg_out_l() { return gL; }
EXPORT(sg_out_r) float* sg_out_r() { return gR; }
EXPORT(sg_counter) double sg_counter() { return static_cast<double>(gE->counter()); }
EXPORT(sg_running) int sg_running() { return gE->running() ? 1 : 0; }

EXPORT(sg_process) void sg_process(int n) {
  if (n > kBlock) n = kBlock;
  shogun::Frame f;
  for (int i = 0; i < n; ++i) {
    const bool clk = gClkLeft > 0, acc = gAccLeft > 0;
    // the LFO restarts at phase 0 when the transport starts
    const bool run = gE->running();
    if (run && !gWasRunning) {
      gLfoPhase = 0.0;
      gLfoLastP = 1.0;  // so a sample-and-hold draws a new value on the first sample
    }
    gWasRunning = run;
    lfoStep();
    applyCv(clk, acc);
    shogun::TrigIn in;
    for (int v = 0; v < shogun::kVoiceCount; ++v) {
      if (gPatch[v] != 0) {
        in.volts[v] = high(gPatch[v], clk, acc) ? 5.0 : 0.0;
        in.velocity[v] = 127;
      }
    }
    for (int j = kRst; j <= kClk; ++j) {
      const bool high = ::high(gPatch[j], clk, acc);
      if (high && !gLast[j]) {
        if (j == kRst) gE->restart();
        if (j == kRun) gE->setRunning(!gE->running());
        if (j == kClk) gE->clockPulse();
        if (j != kClk) gKick = true;
      }
      gLast[j] = high;
    }
    gE->process(in, f);
    gL[i] = static_cast<float>(f.mainL);
    gR[i] = static_cast<float>(f.mainR);
    if (gClkLeft > 0) --gClkLeft;
    if (gAccLeft > 0) --gAccLeft;
    // CLK OUT opens on each new step while the clock runs; ACC OUT opens with it when that step is accented.
    const std::int64_t c = gE->counter();
    if (gE->running() && (c != gLastCounter || gKick)) {
      gClkLeft = kClkPulse;
      gAccLeft = accented(c) ? kClkPulse : 0;
    }
    gKick = false;
    gLastCounter = c;
  }
}
