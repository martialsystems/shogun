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
    K(leadTone),  K(bassTone)};
#undef K
constexpr int kKnobCount = sizeof(kKnobs) / sizeof(kKnobs[0]);

constexpr int kBlock = 1024;
constexpr int kInputs = 19;  // 0..15 voice TRIG, 16 RST IN, 17 RUN IN, 18 CLK IN
constexpr int kRst = 16, kRun = 17, kClk = 18;
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
bool gKick = true;
std::int64_t gLastCounter = -1;

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
  gE->setRunning(false);
}

EXPORT(sg_knob_count) int sg_knob_count() { return kKnobCount; }
EXPORT(sg_knob_name) const char* sg_knob_name(int i) { return i >= 0 && i < kKnobCount ? kKnobs[i].name : ""; }
EXPORT(sg_set_knob) void sg_set_knob(int i, int cc) {
  if (i < 0 || i >= kKnobCount) return;
  gKnobs.*(kKnobs[i].field) = cc;
  gE->setKnobs(gKnobs);
}

EXPORT(sg_set_level) void sg_set_level(int v, double x) { gE->setLevel(static_cast<shogun::Voice>(v), x); }
EXPORT(sg_set_master) void sg_set_master(double x) { gE->setMaster(x); }
EXPORT(sg_set_solo) void sg_set_solo(int v) { gE->setSolo(v); }
EXPORT(sg_set_mode) void sg_set_mode(int ext) { gE->setMode(ext ? shogun::ClockMode::Ext : shogun::ClockMode::Int); }
EXPORT(sg_set_tempo) void sg_set_tempo(double bpm) { gE->setTempo(bpm); }
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

// source: 0 nothing, 1 the panel's CLK OUT.
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
    const bool clk = gClkLeft > 0;
    shogun::TrigIn in;
    for (int v = 0; v < shogun::kVoiceCount; ++v) {
      if (gPatch[v] == 1) {
        in.volts[v] = clk ? 5.0 : 0.0;
        in.velocity[v] = 127;
      }
    }
    for (int j = kRst; j <= kClk; ++j) {
      const bool high = gPatch[j] == 1 && clk;
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
    // CLK OUT opens on each new step while the clock runs.
    const std::int64_t c = gE->counter();
    if (gE->running() && (c != gLastCounter || gKick)) gClkLeft = kClkPulse;
    gKick = false;
    gLastCounter = c;
  }
}
