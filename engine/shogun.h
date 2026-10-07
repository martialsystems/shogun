#pragma once

// SHOGUN voice engine. Framework-free. No JUCE.
//
// Voice samples follow SCHEMATICS.md (printed y, about ±1). Dist CC 0
// leaves y = pre, because that is the printed BD1 row.
// Trig jacks are gates in a 5 V domain. A trigger is a rising edge
// through 1 V. process() does not allocate.

#include "dsp.h"

#include <cstdint>

namespace shogun {

enum class Voice : int {
  Bd1 = 0,
  Bd2,
  Sd,
  Rs,
  Cy,
  Oh,
  Hh,
  Cl,
  Cp,
  Ltc,
  Mtc,
  Htc,
  Cb,
  Ma,
  Lead,
  Bass
};

enum class ClockMode { Int, Ext };

struct DrumStep {
  bool on = false;
  int accent = 2;
  bool flam = false;
  int flamIndex = 0;
  int bendCc = -1;  // -1: bend_st 0. A stored CC of 64 is not zero bend.
};

struct NoteStep {
  int note = -1;  // -1: rest
  int accent = 2;
  bool tie = false;  // keep the envelope and phase of the note before; the pitch may change
};

struct Track {
  int length = 4;
  int shuffle = 0;
  int shiftCc = 0;
  bool mute = false;
  DrumStep drum[kMaxSteps]{};
  NoteStep note[kMaxSteps]{};
};

struct Pattern {
  int length = 4;
  Track track[kVoiceCount]{};
};

struct Knobs {
  int bd1Attack = 0;
  int bd1Decay = 0;
  int bd1Pitch = 0;
  int bd1Tune = 0;
  int bd1Noise = 0;
  int bd1Filter = 64;
  int bd1Dist = 0;
  int bd1Trigger = 0;
  int bd2Decay = 0;
  int bd2Tune = 0;
  int bd2Tone = 0;
  int sdTune = 0;
  int sdDTune = 64;
  int sdSnappy = 0;
  int sdSnDecay = 0;
  int sdTone = 0;
  int sdToneDecay = 0;
  int sdPitch = 0;
  int rsTune = 0;
  int cyDecay = 0;
  int cyTone = 0;
  int cyTune = 0;
  int ohDecay = 0;
  int hhTune = 0;
  int hhDecay = 0;
  int clTune = 0;
  int clDecay = 0;
  int cpDecay = 0;
  int cpFilter = 64;
  int cpAttack = 0;
  int cpTrigger = 0;
  int cpData = 0;  // burst count, no published CC
  int htcTune = 0;
  int htcDecay = 0;
  int htcNoise = 0;  // >= 64 enables this voice's noise
  int htcMode = 0;   // >= 64 conga
  int mtcTune = 0;
  int mtcDecay = 0;
  int mtcNoise = 0;
  int mtcMode = 0;
  int ltcTune = 0;
  int ltcDecay = 0;
  int ltcNoise = 0;
  int ltcMode = 0;
  int tomNoise = 0;  // shared level, CC 84
  int cbTune = 0;
  int cbDecay = 0;
  int maDecay = 0;
  int leadTone = 0;
  int bassTone = 0;
};

struct TrigIn {
  double volts[kVoiceCount];
  int velocity[kVoiceCount];  // < 0: no velocity byte, treated as 127
  TrigIn() {
    for (int i = 0; i < kVoiceCount; ++i) {
      volts[i] = 0.0;
      velocity[i] = -1;
    }
  }
};

struct Frame {
  double bdL = 0;
  double bdR = 0;
  double sdL = 0;
  double rsR = 0;
  double hhL = 0;
  double cyR = 0;
  double cpL = 0;
  double cpR = 0;
  double toL = 0;
  double toR = 0;
  double clL = 0;
  double cbR = 0;
  double mainL = 0;
  double mainR = 0;
  double cv3 = 0;  // 0 V to 5 V from the bass tone knob
};

// Six pair jacks. A patched flag is stored and is not read by the summer.
enum class Pair : int { Bd = 0, SdRs, HhCy, Cp, ToCo, CbCl, Count };

class Engine {
 public:
  Engine();

  void reset();
  void setMode(ClockMode mode);
  void setTempo(double bpm);
  // While playing is true, this BPM is the clock. It does not write the internal tempo.
  void setHostTempo(double bpm, bool playing);
  void setScaleSteps(int stepsPerQuarter);
  void setPattern(const Pattern& pattern);
  void setKnobs(const Knobs& knobs);
  void setLevel(Voice voice, double level);
  void setMaster(double master);
  // Solo one voice (a Voice index), or -1 for off. Not part of the pattern; the clock switch is not touched.
  void setSolo(int voice);
  int solo() const { return solo_; }
  void setPairPatched(Pair pair, bool patched);
  // s in 0..15 overrides every track. Pass -1 to use the pattern values again.
  void setGlobalShuffle(int s);
  void setLiveNote(Voice voice, int note);

  // Transport. A new engine runs. Stop drops the pattern's queued triggers and releases lead and bass.
  // Start counts again from step 1 on the next process().
  void setRunning(bool running);
  bool running() const { return running_; }
  // Back to step 1 on the next process() without stopping.
  void restart();
  // While on, the period no longer moves the counter; each clockPulse() is one step on the next process().
  void setExternalClock(bool on);
  void clockPulse() { pulse_ = true; }

  // Applied on the next process(), before render, at the current sample.
  // Hats: a closed trigger on that sample chokes the open hat first.
  void trigger(Voice voice, double gain = 1.0, double bendSt = 0.0);
  void triggerNote(Voice voice, int note, double gain = 1.0);
  // Note-off for lead and bass. Drums ignore it.
  void release(Voice voice);

  void process(const TrigIn& in, Frame& out);

  bool pairPatched(Pair pair) const;

  double bd1Hz() const { return bd1Hz_; }
  double bd1TuneHz() const { return bd1TuneHz_; }
  double bd1TransientHz() const { return bd1TrHz_; }
  double bd2Env() const { return bd2Env_; }
  double bd2ScaledTransient() const { return bd2Tr_; }
  double sdF1() const { return sdF1_; }
  double sdF2() const { return sdF2_; }
  double sdHz() const { return sdHz_; }   // tone 1 after the Pitch and bend envelopes
  double sdT1() const { return sdT1_; }
  double sdT2() const { return sdT2_; }
  double ohEnv() const { return ohEnv_; }
  double ohSample() const { return ohSample_; }
  double hhEnv() const { return hhEnv_; }
  double hhTau() const { return hhTau_; }
  double cpBurst(int index) const;
  int cpCount() const { return cpCount_; }
  double cyStackA() const { return cyA_; }
  double cyStackB() const { return cyB_; }
  double ltcHz() const { return ltcHz_; }
  double ltcEnv() const { return ltcEnv_; }
  double leadSaw() const { return leadSaw_; }
  double maSample() const { return maSample_; }
  std::int64_t counter() const { return counter_; }
  int displayStep() const;
  std::int64_t sampleIndex() const { return sampleIndex_; }
  double periodSamples() const { return period_; }
  // False before the first trigger and again once a drum voice has gone quiet.
  bool voiceActive(Voice voice) const { return voice_[static_cast<int>(voice)].active; }

 private:
  struct VoiceState {
    bool active = false;
    bool choked = false;
    bool gate = false;
    bool releasing = false;
    int n = 0;
    int note = -1;
    double phase = 0;
    double phase2 = 0;
    double lp = 0;
    double gain = 1;
    double bend = 0;
    double env = 0;
    double mono = 0;
    double left = 0;
    double right = 0;
  };

  enum class EventKind : int { Drum, Note, Rest };

  struct Event {
    std::int64_t when = 0;
    int voice = 0;
    double gain = 1;
    double bend = 0;
    int note = -1;
    EventKind kind = EventKind::Drum;
    bool tie = false;
    bool pattern = false;
    bool live = false;
  };

  struct Pending {
    int voice = 0;
    double gain = 1;
    double bend = 0;
    int note = -1;
    bool isNote = false;
  };

  void recomputePeriod();
  void clearVoices();
  void onStep(std::int64_t c, double start);
  void schedule(double when, int voice, double gain, double bend, int note, EventKind kind, bool tie = false);
  void compactEvents();
  void fireDue();
  void applyJacks(const TrigIn& in);
  void applyPending();
  void fireDrum(int voice, double gain, double bend);
  void fireNote(int voice, int note, double gain, bool tie = false);
  void fireRest(int voice);
  void renderAll();
  void renderBd1(VoiceState& st);
  void renderBd2(VoiceState& st);
  void renderSd(VoiceState& st);
  void renderRs(VoiceState& st);
  void renderCy(VoiceState& st);
  void renderOh(VoiceState& st);
  void renderHh(VoiceState& st);
  void renderCl(VoiceState& st);
  void renderCp(VoiceState& st);
  void renderTom(VoiceState& st, int which);
  void renderCb(VoiceState& st);
  void renderMa(VoiceState& st);
  void renderLeadBass(VoiceState& st, bool bass);
  void mix(Frame& out) const;

  static int clampLength(int length);

  ClockMode mode_ = ClockMode::Int;
  double bpm_ = 120.0;
  double hostBpm_ = 120.0;
  bool hostPlaying_ = false;
  int stepsPerQuarter_ = 4;
  double period_ = 6000.0;
  Pattern pattern_{};
  Knobs knobs_{};
  double level_[kVoiceCount]{};
  int solo_ = -1;
  double master_ = 1.0;
  bool pairPatched_[static_cast<int>(Pair::Count)]{};
  int globalShuffle_ = -1;
  int liveNote_[kVoiceCount]{};

  VoiceState voice_[kVoiceCount]{};
  double prevGate_[kVoiceCount]{};
  std::uint32_t noiseState_ = 1;
  std::int64_t sampleIndex_ = 0;
  std::int64_t counter_ = 0;
  bool booted_ = false;
  bool running_ = true;
  bool extClock_ = false;
  bool pulse_ = false;
  double nextStep_ = 0;  // sample time of the next step boundary, fractional

  Event events_[kMaxEvents]{};
  int eventCount_ = 0;
  Pending pending_[32]{};
  int pendingCount_ = 0;

  double cpBurst_[8]{};
  int cpCount_ = 1;

  double bd1Hz_ = 0;
  double bd1TuneHz_ = 0;
  double bd1TrHz_ = 0;
  double bd2Env_ = 0;
  double bd2Tr_ = 0;
  double sdF1_ = 0;
  double sdHz_ = 0;
  double sdF2_ = 0;
  double sdT1_ = 0;
  double sdT2_ = 0;
  double ohEnv_ = 0;
  double ohSample_ = 0;
  double hhEnv_ = 0;
  double hhTau_ = 0;
  double cyA_ = 0;
  double cyB_ = 0;
  double ltcHz_ = 0;
  double ltcEnv_ = 0;
  double leadSaw_ = 0;
  double maSample_ = 0;
};

}  // namespace shogun
