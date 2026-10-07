#include "shogun.h"
#include "wave_folder.h"

namespace shogun {

namespace {

// STAND-IN voice coefficients, set by ear for density. Not a Vermona capture.
constexpr int kBd1ClickSamples = 48;      // 1 ms
constexpr double kBd1NoiseBurst = 0.015;  // noise burst time, under the body envelope
constexpr double kSdDuck = 0.25;          // noise ducks by up to this while the filter rings
constexpr double kSdRes = 1.6;            // ladder feedback, 4 is self-oscillation
constexpr double kHatNoise = 0.35;        // hats: noise share of the metal mix
constexpr double kHatBandHz = 8000.0;
constexpr double kHatBandQ = 1.0;
constexpr double kHatMakeup = 2.5;
constexpr double kCyBandHz = 5500.0;
constexpr double kCyBandQ = 0.8;
constexpr double kCyMakeup = 2.0;
constexpr double kClapBurst = 0.003;      // fixed, never stretched by Decay
constexpr int kClapGaps[7] = {480, 576, 528, 504, 552, 576, 480};  // 10 ms to 12 ms
constexpr int kClapTailGap = 528;
constexpr double kClapTailGain = 1.0;

int clapStart(int i) {
  int at = 0;
  for (int k = 0; k < i && k < 7; ++k) at += kClapGaps[k];
  return at;
}

// The 1 ms click: 16 transient shapes. s % 4 picks the wave, s sets its rate (fTr). The window falls from 1 to 0.
double bd1Click(int s, double fTr, int n) {
  if (n >= kBd1ClickSamples) return 0.0;
  const double w = 1.0 - static_cast<double>(n) / kBd1ClickSamples;
  const double ph = 2.0 * kPi * fTr * static_cast<double>(n) / kFs;
  double x = 0.0;
  switch (s % 4) {
    case 0: x = std::sin(ph); break;                     // sine blip
    case 1: x = shapedSine(ph, 6.0); break;               // square blip
    case 2: x = std::cos(ph); break;                      // starts at full, a hard tick
    default: x = 2.0 * frac(fTr * n / kFs) - 1.0; break;  // saw tick
  }
  return x * w * w;
}

double clampBpm(double bpm) {
  if (bpm < 60.0) return 60.0;
  if (bpm > 180.0) return 180.0;
  return bpm;
}

// Equal-power pan: p = -1 is hard left, p = 0 is kCenterGain on each side, p = 1 is hard right.
double panL(double p) { return std::cos(0.25 * kPi * (1.0 + p)); }
double panR(double p) { return std::sin(0.25 * kPi * (1.0 + p)); }

void addHardLeft(double s, double& pair, double& main) {
  pair += s;
  main += s;
}

void addHardRight(double s, double& pair, double& main) {
  pair += s;
  main += s;
}

void addPanned(double s, double p, double& pairL, double& pairR, double& mainL, double& mainR) {
  const double gL = panL(p);
  const double gR = panR(p);
  pairL += s * gL;
  pairR += s * gR;
  mainL += s * gL;
  mainR += s * gR;
}

// Equal power at the centre, so a main-only voice is not 6 dB under a hard-panned one.
void addCenter(double s, double& mainL, double& mainR) {
  mainL += s * kCenterGain;
  mainR += s * kCenterGain;
}

// A drum voice ends once the largest of its envelopes is under kQuietEnv. From that sample it outputs 0 until the next trigger.
template <class State>
void endIfQuiet(State& st, double env) {
  if (env >= kQuietEnv) return;
  st.active = false;
  st.mono = 0;
  st.left = 0;
  st.right = 0;
}

}  // namespace

// u(cc) = cc / 127. Values below are round(u * 127) from the 909 balance sheet.
Knobs initKit() {
  Knobs k;
  k.bd1Tune = 28;     // 0.22
  k.bd1Pitch = 70;    // Bend 0.55
  k.bd1Decay = 36;    // 0.28
  k.bd1Attack = 57;   // 0.45
  k.bd1Dist = 19;     // 0.15
  k.bd1Noise = 10;    // 0.08
  k.bd1Filter = 44;   // 0.35
  k.sdTune = 61;      // 0.48
  k.sdDTune = 15;     // Detune 0.12
  k.sdPitch = 44;     // Bend 0.35
  k.sdToneDecay = 28; // Decay 0.22
  k.sdSnDecay = 38;   // Dec 2 0.30
  k.sdSnappy = 79;    // 0.62
  k.sdTone = 57;      // 0.45
  k.rsTune = 79;      // 0.62
  k.cpData = 48;      // 4 bursts
  k.cpAttack = 89;    // 0.7
  k.cpDecay = 32;     // 0.25
  k.cpFilter = 70;    // 0.55
  k.hhDecay = 10;     // 0.08
  k.hhTune = 89;      // 0.7, shared by the open hat
  k.ohDecay = 43;     // 0.34
  k.cyDecay = 70;     // 0.55
  k.ltcTune = 38;     // 0.30
  k.ltcDecay = 41;    // 0.32
  k.mtcTune = 53;     // 0.42
  k.mtcDecay = 36;    // 0.28
  k.htcTune = 70;     // 0.55
  k.htcDecay = 30;    // 0.24
  return k;
}

Pattern initPattern() {
  Pattern p;
  p.name = "909";
  p.length = 16;
  for (auto& tr : p.track) tr.length = 16;
  auto on = [&p](Voice v, int s) { p.track[static_cast<int>(v)].drum[s].on = true; };
  for (int s = 0; s < 16; ++s) {
    if (s % 8 == 0) on(Voice::Bd1, s);
    if (s % 8 == 4) {
      on(Voice::Sd, s);
      on(Voice::Cp, s);
    }
    // A closed hat on the same step chokes the open hat, so the hat skips the open-hat steps.
    if (s % 4 == 2) on(Voice::Oh, s);
    else on(Voice::Hh, s);
  }
  return p;
}

Engine::Engine() {
  reset();
  loadInit();
}

void Engine::loadInit() {
  static constexpr double kInitLevel[kVoiceCount] = {
      0.85, 0.0, 0.75, 0.4, 0.35, 0.4, 0.45, 0.0, 0.7, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0};
  knobs_ = initKit();
  pattern_ = initPattern();
  for (int i = 0; i < kVoiceCount; ++i) level_[i] = kInitLevel[i];
  master_ = 0.7;
  mode_ = ClockMode::Int;
  bpm_ = 120.0;
  stepsPerQuarter_ = 4;
  recomputePeriod();
}

void Engine::reset() {
  mode_ = ClockMode::Int;
  bpm_ = 120.0;
  hostBpm_ = 120.0;
  hostPlaying_ = false;
  stepsPerQuarter_ = 4;
  pattern_ = Pattern{};
  knobs_ = Knobs{};
  master_ = 1.0;
  globalShuffle_ = -1;
  for (int i = 0; i < kVoiceCount; ++i) {
    level_[i] = 1.0;
    liveNote_[i] = -1;
    prevGate_[i] = 0.0;
  }
  liveNote_[static_cast<int>(Voice::Lead)] = 60;
  liveNote_[static_cast<int>(Voice::Bass)] = 36;
  for (int i = 0; i < static_cast<int>(Pair::Count); ++i) pairPatched_[i] = false;
  noiseState_ = 1;
  sampleIndex_ = 0;
  counter_ = 0;
  booted_ = false;
  running_ = true;
  extClock_ = false;
  pulse_ = false;
  nextStep_ = 0;
  eventCount_ = 0;
  pendingCount_ = 0;
  clearVoices();
  recomputePeriod();
}

void Engine::setMode(ClockMode mode) {
  // INT to EXT: the pattern no longer reaches lead and bass, so a note it opened must not hang.
  if (mode_ == ClockMode::Int && mode == ClockMode::Ext) {
    fireRest(static_cast<int>(Voice::Lead));
    fireRest(static_cast<int>(Voice::Bass));
  }
  mode_ = mode;
}

void Engine::setRunning(bool running) {
  if (running == running_) return;
  running_ = running;
  if (running) {
    counter_ = 0;
    booted_ = false;
    return;
  }
  for (int i = 0; i < eventCount_; ++i) {
    if (events_[i].pattern) events_[i].live = false;
  }
  compactEvents();
  fireRest(static_cast<int>(Voice::Lead));
  fireRest(static_cast<int>(Voice::Bass));
}

void Engine::restart() {
  counter_ = 0;
  booted_ = false;
}

void Engine::setExternalClock(bool on) {
  if (on == extClock_) return;
  extClock_ = on;
  pulse_ = false;
  if (!on) nextStep_ = static_cast<double>(sampleIndex_) + period_;
}

void Engine::setTempo(double bpm) {
  bpm_ = clampBpm(bpm);
  recomputePeriod();
}

void Engine::setHostTempo(double bpm, bool playing) {
  hostPlaying_ = playing;
  if (playing) hostBpm_ = clampBpm(bpm);
  recomputePeriod();
}

void Engine::setScaleSteps(int stepsPerQuarter) {
  if (stepsPerQuarter == 8 || stepsPerQuarter == 6 || stepsPerQuarter == 4 || stepsPerQuarter == 3) {
    stepsPerQuarter_ = stepsPerQuarter;
  }
  recomputePeriod();
}

void Engine::setPattern(const Pattern& pattern) { pattern_ = pattern; }

void Engine::setKnobs(const Knobs& knobs) { knobs_ = knobs; }

void Engine::setLevel(Voice voice, double level) {
  const int i = static_cast<int>(voice);
  if (i < 0 || i >= kVoiceCount) return;
  level_[i] = level;
}

void Engine::setMaster(double master) { master_ = master; }

void Engine::setSolo(int voice) { solo_ = (voice >= 0 && voice < kVoiceCount) ? voice : -1; }

void Engine::setPairPatched(Pair pair, bool patched) {
  const int i = static_cast<int>(pair);
  if (i < 0 || i >= static_cast<int>(Pair::Count)) return;
  pairPatched_[i] = patched;
}

bool Engine::pairPatched(Pair pair) const {
  const int i = static_cast<int>(pair);
  if (i < 0 || i >= static_cast<int>(Pair::Count)) return false;
  return pairPatched_[i];
}

void Engine::setGlobalShuffle(int s) { globalShuffle_ = s; }

void Engine::setLiveNote(Voice voice, int note) {
  const int i = static_cast<int>(voice);
  if (i < 0 || i >= kVoiceCount) return;
  liveNote_[i] = note;
}

void Engine::trigger(Voice voice, double gain, double bendSt) {
  if (pendingCount_ >= 32) return;
  Pending& p = pending_[pendingCount_++];
  p.voice = static_cast<int>(voice);
  p.gain = gain;
  p.bend = bendSt;
  p.note = -1;
  p.isNote = false;
}

void Engine::triggerNote(Voice voice, int note, double gain) {
  if (pendingCount_ >= 32) return;
  Pending& p = pending_[pendingCount_++];
  p.voice = static_cast<int>(voice);
  p.gain = gain;
  p.bend = 0.0;
  p.note = note;
  p.isNote = true;
}

void Engine::release(Voice voice) { fireRest(static_cast<int>(voice)); }

double Engine::cpBurst(int index) const {
  if (index < 0 || index >= 8) return 0.0;
  return cpBurst_[index];
}

int Engine::displayStep() const {
  const int length = clampLength(pattern_.length);
  const int slot = static_cast<int>(counter_ % length);
  return slot + 1;
}

void Engine::recomputePeriod() {
  const double bpm = hostPlaying_ ? hostBpm_ : bpm_;
  const double old = period_;
  period_ = kFs * 60.0 / (bpm * static_cast<double>(stepsPerQuarter_));
  // A tempo change keeps the step in progress at the same fraction, so the counter neither races nor stalls.
  if (booted_ && old > 0.0 && period_ != old) {
    const double now = static_cast<double>(sampleIndex_);
    const double left = nextStep_ - now;
    if (left > 0.0) nextStep_ = now + left * period_ / old;
  }
}

void Engine::clearVoices() {
  for (int i = 0; i < kVoiceCount; ++i) voice_[i] = VoiceState{};
  for (int i = 0; i < 8; ++i) cpBurst_[i] = 0.0;
  cpCount_ = 1;
  bd1Hz_ = bd1TuneHz_ = bd1TrHz_ = 0;
  bd2Env_ = bd2Tr_ = 0;
  sdF1_ = sdF2_ = sdT1_ = sdT2_ = sdHz_ = sdNoise_ = 0;
  hhStack_ = bd2Hz_ = 0;
  ohEnv_ = ohSample_ = hhEnv_ = 0;
  hhTau_ = 0.008;
  cyA_ = cyB_ = 0;
  ltcHz_ = ltcEnv_ = 0;
  leadSaw_ = maSample_ = 0;
}

int Engine::clampLength(int length) {
  if (length < 1) return 1;
  if (length > kMaxSteps) return kMaxSteps;
  return length;
}

void Engine::compactEvents() {
  int write = 0;
  for (int i = 0; i < eventCount_; ++i) {
    if (events_[i].live) events_[write++] = events_[i];
  }
  eventCount_ = write;
}

void Engine::schedule(double when, int voice, double gain, double bend, int note, EventKind kind, bool tie) {
  if (eventCount_ >= kMaxEvents) compactEvents();
  if (eventCount_ >= kMaxEvents) return;
  Event& e = events_[eventCount_++];
  e.when = static_cast<std::int64_t>(std::floor(when));
  e.voice = voice;
  e.gain = gain;
  e.bend = bend;
  e.note = note;
  e.kind = kind;
  e.tie = tie;
  e.pattern = true;
  e.live = true;
}

void Engine::onStep(std::int64_t c, double start) {
  compactEvents();
  for (int vi = 0; vi < kVoiceCount; ++vi) {
    const Track& tr = pattern_.track[vi];
    const bool noteTrack = vi >= static_cast<int>(Voice::Lead);
    if (tr.mute && !noteTrack) continue;
    const int length = clampLength(tr.length);
    int slot = static_cast<int>(c % length);
    if (slot < 0) slot += length;

    int shuffle = tr.shuffle;
    if (globalShuffle_ >= 0) shuffle = globalShuffle_;
    if (shuffle < 0) shuffle = 0;
    if (shuffle > 15) shuffle = 15;
    double delay = 0.0;
    // Odd clock steps, not odd track steps, so an odd track length keeps the swing on the beat.
    if ((c % 2) == 1) delay = (static_cast<double>(shuffle) / 15.0) * (period_ / 3.0);
    const double shift = std::round(u(tr.shiftCc) * 0.030 * kFs);
    const double base = start + shift + delay;

    if (noteTrack) {
      const NoteStep& step = tr.note[slot];
      const double gain = (vi == static_cast<int>(Voice::Bass)) ? gAccent(step.accent) : 1.0;
      // A muted note track rests, so a note it was holding releases instead of droning.
      if (tr.mute || step.note < 0) {
        schedule(base, vi, gain, 0.0, -1, EventKind::Rest);
      } else {
        schedule(base, vi, gain, 0.0, step.note, EventKind::Note, step.tie);
      }
      continue;
    }

    const DrumStep& step = tr.drum[slot];
    if (!step.on) continue;
    double bend = 0.0;
    if (voiceHasBend(vi) && step.bendCc >= 0) {
      bend = 12.0 * (2.0 * u(step.bendCc) - 1.0);
    }
    const double gain = gAccent(step.accent);
    const bool flam = step.flam && vi != static_cast<int>(Voice::Cp);
    if (!flam) {
      schedule(base, vi, gain, bend, -1, EventKind::Drum);
      continue;
    }
    const int hits = flamHits(step.flamIndex);
    const int gap = flamGap(step.flamIndex);
    for (int h = 0; h < hits; ++h) {
      schedule(base + static_cast<double>(h * gap), vi, gain, bend, -1, EventKind::Drum);
    }
  }
}

void Engine::fireDrum(int voice, double gain, double bend) {
  if (voice < 0 || voice >= kVoiceCount) return;
  if (voice == static_cast<int>(Voice::Hh)) {
    voice_[static_cast<int>(Voice::Oh)].choked = true;
  }
  VoiceState& st = voice_[voice];
  st.active = true;
  st.choked = false;
  st.gate = false;
  st.releasing = false;
  st.n = 0;
  st.phase = 0;
  st.phase2 = 0;
  st.lp = 0;
  for (double& zi : st.z) zi = 0;
  st.follow = 0;
  st.gain = gain;
  st.bend = bend;
  st.env = 1;
  st.mono = 0;
  st.left = 0;
  st.right = 0;
}

void Engine::fireNote(int voice, int note, double gain, bool tie) {
  if (voice != static_cast<int>(Voice::Lead) && voice != static_cast<int>(Voice::Bass)) return;
  if (note < 0) {
    fireRest(voice);
    return;
  }
  VoiceState& st = voice_[voice];
  // Only a step marked tie holds on; a repeated note without it plays again.
  const bool tied = tie && st.gate && !st.releasing;
  st.gain = gain;
  st.note = note;
  st.gate = true;
  st.releasing = false;
  st.active = true;
  st.env = 1;
  if (tied) return;
  st.n = 0;
  st.phase = 0;
  st.phase2 = 0;
  st.lp = 0;
}

void Engine::fireRest(int voice) {
  if (voice < 0 || voice >= kVoiceCount) return;
  VoiceState& st = voice_[voice];
  if (!st.gate) return;
  st.gate = false;
  st.releasing = true;
}

void Engine::fireDue() {
  for (int i = 0; i < eventCount_; ++i) {
    Event& e = events_[i];
    // <= so an event can never be stranded live in the queue.
    if (!e.live || e.when > sampleIndex_) continue;
    e.live = false;
    if (e.pattern && mode_ != ClockMode::Int) continue;
    if (e.kind == EventKind::Drum) fireDrum(e.voice, e.gain, e.bend);
    else if (e.kind == EventKind::Note) fireNote(e.voice, e.note, e.gain, e.tie);
    else fireRest(e.voice);
  }
}

void Engine::applyJacks(const TrigIn& in) {
  for (int v = 0; v < kVoiceCount; ++v) {
    const double gate = in.volts[v];
    const bool rise = prevGate_[v] < kTrigThreshold && gate >= kTrigThreshold;
    const bool fall = prevGate_[v] >= kTrigThreshold && gate < kTrigThreshold;
    // Always store the last gate, including in INT, so a held jack
    // does not become a false edge when the switch later moves to EXT.
    prevGate_[v] = gate;
    if (mode_ != ClockMode::Ext) continue;
    if (!rise && !fall) continue;
    if (fall) {
      if (v == static_cast<int>(Voice::Lead) || v == static_cast<int>(Voice::Bass)) fireRest(v);
      continue;
    }
    const double gain = gVel(in.velocity[v]);
    if (v == static_cast<int>(Voice::Lead) || v == static_cast<int>(Voice::Bass)) {
      fireNote(v, liveNote_[v], gain);
    } else {
      fireDrum(v, gain, 0.0);
    }
  }
}

void Engine::applyPending() {
  // Voice enum order, so a same-sample closed hat runs after the open hat and wins.
  for (int v = 0; v < kVoiceCount; ++v) {
    for (int i = 0; i < pendingCount_; ++i) {
      if (pending_[i].voice != v) continue;
      if (pending_[i].isNote) fireNote(v, pending_[i].note, pending_[i].gain);
      else fireDrum(v, pending_[i].gain, pending_[i].bend);
    }
  }
  pendingCount_ = 0;
}

void Engine::process(const TrigIn& in, Frame& out) {
  const double now = static_cast<double>(sampleIndex_);
  if (running_ && !booted_) {
    booted_ = true;
    pulse_ = false;
    nextStep_ = now + period_;
    if (mode_ == ClockMode::Int) onStep(counter_, now);
  } else if (running_ && extClock_ && pulse_) {
    pulse_ = false;
    ++counter_;
    if (mode_ == ClockMode::Int) onStep(counter_, now);
  }
  fireDue();
  applyJacks(in);
  applyPending();
  hhTau_ = decayTau(u(knobs_.hhDecay));
  renderAll();
  mix(out);
  ++sampleIndex_;
  if (!running_ || !booted_ || extClock_) return;
  // The step starts on the sample that contains its fractional boundary. That sample is the next one,
  // so its events are queued now and fireDue() plays them there.
  while (std::floor(nextStep_) <= static_cast<double>(sampleIndex_)) {
    const double start = nextStep_;
    nextStep_ += period_;
    ++counter_;
    if (mode_ == ClockMode::Int) onStep(counter_, start);
  }
}

void Engine::renderBd1(VoiceState& st) {
  bd1Hz_ = 0;
  bd1TuneHz_ = 0;
  bd1TrHz_ = 0;
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const double fTune = 35.0 * std::pow(140.0 / 35.0, u(knobs_.bd1Tune));
  const double depth = 18.0 * u(knobs_.bd1Pitch);
  const double tauP = 0.012 + 0.25 * u(knobs_.bd1Pitch);
  const double tauB = decayTau(u(knobs_.bd1Decay));
  const int s = soundIndex(knobs_.bd1Trigger);
  const double fTr = 160.0 * std::pow(1.35, s);
  const double fc = 200.0 * std::pow(8000.0 / 200.0, u(knobs_.bd1Filter));
  const double pEnv = expDecay(n, tauP);
  // Step bend is its own drop to Tune, with the same 80 ms floor as the snare.
  const double bEnv = st.bend != 0.0 ? expDecay(n, std::fmax(tauP, kSdBendFloor)) : 0.0;
  const double f = fTune * std::pow(2.0, (depth * pEnv + st.bend * bEnv) / 12.0);
  const double bodyEnv = expDecay(n, tauB);
  // Wave folds the body oscillator before its envelope, so the fold does not change the decay law. Dist is after.
  const double body = wave::fold(std::sin(st.phase), knobs_.bd1Wave) * bodyEnv;
  const double tr = bd1Click(s, fTr, n);
  double noise = 0.0;
  if (knobs_.bd1Noise > 0) {
    // A short burst on the body envelope, then the one-pole. Zero once the body is under 1e-3.
    const double burst = noiseDraw(noiseState_) * bodyEnv * expDecay(n, kBd1NoiseBurst);
    noise = u(knobs_.bd1Noise) * onePole(st.lp, burst, fc);
  }
  const double pre = body + u(knobs_.bd1Attack) * tr + noise;
  double y = pre;
  // Dist CC 0 is a bypass of this stage, y = pre. Above 0, drive = 9u, which starts near y = pre, so CC 1 does not jump.
  if (knobs_.bd1Dist > 0) {
    const double drive = 9.0 * u(knobs_.bd1Dist);
    y = std::tanh(drive * pre) / std::tanh(drive);
  }
  st.mono = y * st.gain;
  bd1Hz_ = f;
  bd1TuneHz_ = fTune;
  bd1TrHz_ = fTr;
  st.phase += 2.0 * kPi * f / kFs;
  st.n = n + 1;
  endIfQuiet(st, n < kBd1ClickSamples ? 1.0 : bodyEnv);
}

void Engine::renderBd2(VoiceState& st) {
  bd2Env_ = 0;
  bd2Tr_ = 0;
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const double fTune = 45.0 * std::pow(100.0 / 45.0, u(knobs_.bd2Tune));
  const double fTr = 2.0 * fTune;
  double sustain = 0.0;
  double tauB = decayTau(u(knobs_.bd2Decay));
  if (knobs_.bd2Decay >= 127) {
    sustain = 0.70;
    tauB = 0.40;
  }
  const double env = sustain + (1.0 - sustain) * expDecay(n, tauB);
  const double pEnv = expDecay(n, 0.08);
  // Slow FM so it can bark: fm 40 Hz to 200 Hz from Tone, a small index.
  const double fm = 40.0 * std::pow(200.0 / 40.0, u(knobs_.bd2Tone));
  const double fmTerm = kBd2FmIndex * fTune * std::sin(2.0 * kPi * fm * static_cast<double>(n) / kFs);
  const double f = fTune * std::pow(2.0, (st.bend * pEnv) / 12.0) + fmTerm;
  const double body = wave::fold(std::sin(st.phase), knobs_.bd2Wave) * env;
  const double trEnv = expDecay(n, 0.005);
  const double tr = std::sin(2.0 * kPi * fTr * static_cast<double>(n) / kFs) * trEnv;
  const double scaled = u(knobs_.bd2Tone) * tr;
  st.mono = (body + scaled) * st.gain;
  bd2Env_ = env;
  bd2Tr_ = scaled;
  bd2Hz_ = f;
  st.phase += 2.0 * kPi * f / kFs;
  st.n = n + 1;
  endIfQuiet(st, std::fmax(env, trEnv));
}

// Four one-poles with feedback from the last, a STAND-IN ladder. The (1 + res) term keeps the pass band at unity.
double Engine::ladder4(VoiceState& st, double x, double fc) {
  const double g = 1.0 - std::exp(-2.0 * kPi * fc / kFs);
  double in = x - kSdRes * st.z[3];
  for (int i = 0; i < 4; ++i) {
    st.z[i] += g * (in - st.z[i]);
    in = st.z[i];
  }
  return (1.0 + kSdRes) * st.z[3];
}

void Engine::renderSd(VoiceState& st) {
  sdF1_ = sdF2_ = sdT1_ = sdT2_ = sdHz_ = sdNoise_ = 0;
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const double f1 = 120.0 * std::pow(400.0 / 120.0, u(knobs_.sdTune));
  const double detune = -8.0 + 16.0 * u(knobs_.sdDTune);
  const double f2 = f1 * std::pow(2.0, detune / 12.0);
  const double depth = 14.0 * u(knobs_.sdPitch);
  const double tauP = 0.01 + 0.12 * u(knobs_.sdPitch);
  const double tauTone = decayTau(u(knobs_.sdToneDecay));
  const double tauN = decayTau(u(knobs_.sdSnDecay));
  const double pEnv = expDecay(n, tauP);
  // Step bend is its own drop to Tune, not a deeper Pitch: its time has an 80 ms floor, so Pitch 0 still swoops.
  const double tauBend = std::fmax(tauP, kSdBendFloor);
  const double bEnv = st.bend != 0.0 ? expDecay(n, tauBend) : 0.0;
  const double st12 = (depth * pEnv + st.bend * bEnv) / 12.0;
  const double f1n = f1 * std::pow(2.0, st12);
  const double f2n = f2 * std::pow(2.0, st12);
  const double t1 = shapedSine(st.phase, kSdWave) * expDecay(n, tauTone);
  const double t2 = shapedSine(st.phase2, kSdWave) * expDecay(n, tauTone);
  double nz = 0.0;
  if (knobs_.sdSnappy > 0) {
    // Noise through a 4-pole low-pass. Tone sets the cutoff. The input ducks a little while the filter rings.
    const double fc = 1000.0 * std::pow(12000.0 / 1000.0, u(knobs_.sdTone));
    const double duck = 1.0 - kSdDuck * (st.follow < 1.0 ? st.follow : 1.0);
    const double filtered = ladder4(st, duck * noiseDraw(noiseState_), fc);
    const double rel = std::exp(-1.0 / (kFs * 0.010));
    const double mag = std::fabs(filtered);
    st.follow = mag > st.follow ? mag : st.follow * rel;
    sdNoise_ = filtered;
    nz = u(knobs_.sdSnappy) * filtered * expDecay(n, tauN);
  }
  const double y = (1.0 - u(knobs_.sdTone)) * t1 + u(knobs_.sdTone) * t2 + nz;
  st.mono = y * st.gain;
  sdF1_ = f1;
  sdF2_ = f2;
  sdHz_ = f1n;
  sdT1_ = t1;
  sdT2_ = t2;
  st.phase += 2.0 * kPi * f1n / kFs;
  st.phase2 += 2.0 * kPi * f2n / kFs;
  st.n = n + 1;
  endIfQuiet(st, std::fmax(expDecay(n, tauTone), knobs_.sdSnappy > 0 ? expDecay(n, tauN) : 0.0));
}

void Engine::renderRs(VoiceState& st) {
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const double f = 250.0 * std::pow(2500.0 / 250.0, u(knobs_.rsTune));
  const double env = expDecay(n, 0.012);
  const double y = std::sin(2.0 * kPi * f * static_cast<double>(n) / kFs) * env;
  st.mono = y * st.gain;
  st.n = n + 1;
  endIfQuiet(st, env);
}

void Engine::renderCy(VoiceState& st) {
  cyA_ = cyB_ = 0;
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const double f0 = 180.0 * std::pow(900.0 / 180.0, u(knobs_.cyTune));
  const double tau = decayTau(u(knobs_.cyDecay));
  const double metal = metalSquares(f0, n);
  const double noise = noiseDraw(noiseState_);
  // Tone is the mix, metal against noise. Tune moves the stack only.
  const double mix = 0.1 + 0.8 * u(knobs_.cyTone);
  const double env = expDecay(n, tau);
  const double x = (1.0 - mix) * metal + mix * noise;
  const double y = kCyMakeup * bandPass(st.z, x, kCyBandHz, kCyBandQ) * env;
  st.mono = y * st.gain;
  cyA_ = metal;
  cyB_ = noise;
  st.n = n + 1;
  endIfQuiet(st, env);
}

void Engine::renderOh(VoiceState& st) {
  ohSample_ = 0;
  ohEnv_ = 0;
  st.mono = 0;
  if (st.choked) st.active = false;
  if (!st.active) return;
  const int n = st.n;
  const double f0 = 250.0 * std::pow(1000.0 / 250.0, u(knobs_.hhTune));
  const double tau = decayTau(u(knobs_.ohDecay));
  const double env = expDecay(n, tau);
  const double metal = metalSquares(f0, n);
  const double x = (1.0 - kHatNoise) * metal + kHatNoise * noiseDraw(noiseState_);
  const double y = kHatMakeup * bandPass(st.z, x, kHatBandHz, kHatBandQ) * env;
  st.mono = y * st.gain;
  ohEnv_ = env;
  ohSample_ = y;
  hhStack_ = metal;
  st.n = n + 1;
  endIfQuiet(st, env);
}

void Engine::renderHh(VoiceState& st) {
  hhEnv_ = 0;
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const double f0 = 250.0 * std::pow(1000.0 / 250.0, u(knobs_.hhTune));
  const double tau = decayTau(u(knobs_.hhDecay));
  const double env = expDecay(n, tau);
  const double metal = metalSquares(f0, n);
  const double x = (1.0 - kHatNoise) * metal + kHatNoise * noiseDraw(noiseState_);
  const double y = kHatMakeup * bandPass(st.z, x, kHatBandHz, kHatBandQ) * env;
  st.mono = y * st.gain;
  hhEnv_ = env;
  hhStack_ = metal;
  st.n = n + 1;
  endIfQuiet(st, env);
}

void Engine::renderCl(VoiceState& st) {
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const double f = 400.0 * std::pow(3000.0 / 400.0, u(knobs_.clTune));
  const double tau = decayTau(u(knobs_.clDecay));
  const double env = expDecay(n, tau);
  const double y = std::sin(2.0 * kPi * f * static_cast<double>(n) / kFs) * env;
  st.mono = y * st.gain;
  st.n = n + 1;
  endIfQuiet(st, env);
}

void Engine::renderCp(VoiceState& st) {
  for (int i = 0; i < 8; ++i) cpBurst_[i] = 0.0;
  cpCount_ = clapCount(knobs_.cpData);
  st.left = 0;
  st.right = 0;
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const int count = cpCount_;
  // Bursts: high-passed noise, 3 ms each, 10 ms to 12 ms apart. Decay never stretches them.
  const double fHp = 300.0 * std::pow(1.2, soundIndex(knobs_.cpTrigger));
  const double fc = 400.0 * std::pow(6000.0 / 400.0, u(knobs_.cpFilter));
  const double tau = decayTau(u(knobs_.cpDecay));
  const double raw = noiseDraw(noiseState_);
  const double hp = raw - onePole(st.z[0], raw, fHp);
  double yL = 0.0;
  double yR = 0.0;
  for (int i = 0; i < count; ++i) {
    const int nb = n - clapStart(i);
    double burst = 0.0;
    if (nb >= 0) burst = hp * expDecay(nb, kClapBurst);
    cpBurst_[i] = burst;
    const double p = (count > 1) ? (-1.0 + 2.0 * static_cast<double>(i) / static_cast<double>(count - 1)) : 0.0;
    yL += burst * panL(p);
    yR += burst * panR(p);
  }
  const double attack = u(knobs_.cpAttack);
  yL *= attack;
  yR *= attack;
  // Tail: one filtered delay of the same high-passed noise. It starts one gap after the last burst and decays on
  // the drum law. The low-pass runs from the hit so it is settled when the tail opens.
  const int tailStart = clapStart(count - 1) + kClapTailGap;
  const double lp = onePole(st.lp, hp, fc);
  const double tailEnv = n >= tailStart ? expDecay(n - tailStart, tau) : 0.0;
  const double tail = kClapTailGain * lp * tailEnv;
  yL += tail * kCenterGain;
  yR += tail * kCenterGain;
  st.left = yL * st.gain;
  st.right = yR * st.gain;
  st.n = n + 1;
  endIfQuiet(st, n >= tailStart ? tailEnv : 1.0);
}

void Engine::renderTom(VoiceState& st, int which) {
  st.mono = 0;
  int tuneCc = knobs_.ltcTune;
  int decayCc = knobs_.ltcDecay;
  int noiseCc = knobs_.ltcNoise;
  int modeCc = knobs_.ltcMode;
  int waveCc = knobs_.ltcWave;
  double fLo = 70.0;
  double fHi = 180.0;
  if (which == 1) {
    tuneCc = knobs_.mtcTune;
    decayCc = knobs_.mtcDecay;
    noiseCc = knobs_.mtcNoise;
    modeCc = knobs_.mtcMode;
    waveCc = knobs_.mtcWave;
    fLo = 100.0;
    fHi = 280.0;
  } else if (which == 2) {
    tuneCc = knobs_.htcTune;
    decayCc = knobs_.htcDecay;
    noiseCc = knobs_.htcNoise;
    modeCc = knobs_.htcMode;
    waveCc = knobs_.htcWave;
    fLo = 140.0;
    fHi = 400.0;
  }
  if (which == 0) {
    ltcHz_ = 0;
    ltcEnv_ = 0;
  }
  if (!st.active) return;
  const int n = st.n;
  const double fTune = fLo * std::pow(fHi / fLo, u(tuneCc));
  double sustain = 0.0;
  double tauB = decayTau(u(decayCc));
  if (decayCc >= 127) {
    sustain = 0.55;
    tauB = 0.35;
  }
  // Decay 127 is a long ring, not a hold: after kTomRingSamples it releases and the voice ends. Only BD2 drones.
  const double ring = sustain + (1.0 - sustain) * expDecay(n < kTomRingSamples ? n : kTomRingSamples, tauB);
  const double env = n <= kTomRingSamples ? ring : ring * expDecay(n - kTomRingSamples, kTomRelease);
  const double pEnv = expDecay(n, 0.08);
  // Quieter cousin of the BD2 FM term, fixed rate.
  const double fmTerm = kTomFmIndex * fTune * std::sin(2.0 * kPi * kTomFmHz * static_cast<double>(n) / kFs);
  const double f = fTune * std::pow(2.0, (st.bend * pEnv) / 12.0) + fmTerm;
  double y = wave::fold(std::sin(st.phase), waveCc) * env;
  // The conga partial rides the body envelope; without it the partial rang forever.
  if (modeCc >= 64) y += 0.35 * std::sin(st.phase2) * env;
  if (noiseCc >= 64) {
    y += u(knobs_.tomNoise) * noiseDraw(noiseState_) * expDecay(n, 0.12);
  }
  st.mono = y * st.gain;
  if (which == 0) {
    ltcHz_ = f;
    ltcEnv_ = env;
  }
  st.phase += 2.0 * kPi * f / kFs;
  st.phase2 += 2.0 * kPi * (2.30 * f) / kFs;
  st.n = n + 1;
  endIfQuiet(st, std::fmax(env, noiseCc >= 64 ? expDecay(n, 0.12) : 0.0));
}

void Engine::renderCb(VoiceState& st) {
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const double f = 300.0 * std::pow(1200.0 / 300.0, u(knobs_.cbTune));
  const double tau = decayTau(u(knobs_.cbDecay));
  const double env = expDecay(n, tau);
  const double y = 0.5 * (squareWave(f, n) + squareWave(1.015 * f, n)) * env;
  st.mono = y * st.gain;
  st.n = n + 1;
  endIfQuiet(st, env);
}

void Engine::renderMa(VoiceState& st) {
  maSample_ = 0;
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const double tau = decayTau(u(knobs_.maDecay));
  const double env = expDecay(n, tau);
  const double y = onePole(st.lp, noiseDraw(noiseState_), 1500.0) * env;
  st.mono = y * st.gain;
  maSample_ = y;
  st.n = n + 1;
  endIfQuiet(st, env);
}

void Engine::renderLeadBass(VoiceState& st, bool bass) {
  if (!bass) leadSaw_ = 0;
  st.mono = 0;
  if (!st.active) return;
  const int n = st.n;
  const double hz = midiHz(st.note);
  const double saw = sawSample(st.phase, hz);
  if (!bass) leadSaw_ = saw;
  const double fc = bass ? 80.0 * std::pow(4000.0 / 80.0, u(knobs_.bassTone))
                         : 200.0 * std::pow(8000.0 / 200.0, u(knobs_.leadTone));
  const double filtered = onePole(st.lp, saw, fc);
  double env = st.env;
  if (st.releasing) {
    env *= std::exp(-1.0 / (kFs * 0.03));
    st.env = env;
    if (env < 1e-12) {
      st.active = false;
      st.env = 0;
      st.releasing = false;
    }
  } else if (st.gate) {
    env = 1.0;
    st.env = 1.0;
  } else {
    env = 0.0;
  }
  st.mono = filtered * env * st.gain;
  st.phase += 2.0 * kPi * hz / kFs;
  st.n = n + 1;
}

void Engine::renderAll() {
  renderBd1(voice_[static_cast<int>(Voice::Bd1)]);
  renderBd2(voice_[static_cast<int>(Voice::Bd2)]);
  renderSd(voice_[static_cast<int>(Voice::Sd)]);
  renderRs(voice_[static_cast<int>(Voice::Rs)]);
  renderCy(voice_[static_cast<int>(Voice::Cy)]);
  renderOh(voice_[static_cast<int>(Voice::Oh)]);
  renderHh(voice_[static_cast<int>(Voice::Hh)]);
  renderCl(voice_[static_cast<int>(Voice::Cl)]);
  renderCp(voice_[static_cast<int>(Voice::Cp)]);
  renderTom(voice_[static_cast<int>(Voice::Ltc)], 0);
  renderTom(voice_[static_cast<int>(Voice::Mtc)], 1);
  renderTom(voice_[static_cast<int>(Voice::Htc)], 2);
  renderCb(voice_[static_cast<int>(Voice::Cb)]);
  renderMa(voice_[static_cast<int>(Voice::Ma)]);
  renderLeadBass(voice_[static_cast<int>(Voice::Lead)], false);
  renderLeadBass(voice_[static_cast<int>(Voice::Bass)], true);
}

void Engine::mix(Frame& out) const {
  out = Frame{};
  // Solo: only the soloed voice reaches the pairs and the main, and only if its track is not muted. Every voice
  // still renders, so the others keep advancing and come back where they are when solo is off.
  double lv[kVoiceCount];
  for (int v = 0; v < kVoiceCount; ++v) {
    const bool heard = solo_ < 0 || (v == solo_ && !pattern_.track[v].mute);
    lv[v] = heard ? level_[v] : 0.0;
  }
  const double bd1 = voice_[static_cast<int>(Voice::Bd1)].mono * lv[static_cast<int>(Voice::Bd1)];
  const double bd2 = voice_[static_cast<int>(Voice::Bd2)].mono * lv[static_cast<int>(Voice::Bd2)];
  const double sd = voice_[static_cast<int>(Voice::Sd)].mono * lv[static_cast<int>(Voice::Sd)];
  const double rs = voice_[static_cast<int>(Voice::Rs)].mono * lv[static_cast<int>(Voice::Rs)];
  const double cy = voice_[static_cast<int>(Voice::Cy)].mono * lv[static_cast<int>(Voice::Cy)];
  const double oh = voice_[static_cast<int>(Voice::Oh)].mono * lv[static_cast<int>(Voice::Oh)];
  const double hh = voice_[static_cast<int>(Voice::Hh)].mono * lv[static_cast<int>(Voice::Hh)];
  const double cl = voice_[static_cast<int>(Voice::Cl)].mono * lv[static_cast<int>(Voice::Cl)];
  const double cpL = voice_[static_cast<int>(Voice::Cp)].left * lv[static_cast<int>(Voice::Cp)];
  const double cpR = voice_[static_cast<int>(Voice::Cp)].right * lv[static_cast<int>(Voice::Cp)];
  const double ltc = voice_[static_cast<int>(Voice::Ltc)].mono * lv[static_cast<int>(Voice::Ltc)];
  const double mtc = voice_[static_cast<int>(Voice::Mtc)].mono * lv[static_cast<int>(Voice::Mtc)];
  const double htc = voice_[static_cast<int>(Voice::Htc)].mono * lv[static_cast<int>(Voice::Htc)];
  const double cb = voice_[static_cast<int>(Voice::Cb)].mono * lv[static_cast<int>(Voice::Cb)];
  const double ma = voice_[static_cast<int>(Voice::Ma)].mono * lv[static_cast<int>(Voice::Ma)];
  const double lead = voice_[static_cast<int>(Voice::Lead)].mono * lv[static_cast<int>(Voice::Lead)];
  const double bass = voice_[static_cast<int>(Voice::Bass)].mono * lv[static_cast<int>(Voice::Bass)];

  addHardLeft(bd1, out.bdL, out.mainL);
  addHardRight(bd2, out.bdR, out.mainR);
  addHardLeft(sd, out.sdL, out.mainL);
  addHardRight(rs, out.rsR, out.mainR);
  addHardLeft(oh + hh, out.hhL, out.mainL);
  addHardRight(cy, out.cyR, out.mainR);
  out.cpL += cpL;
  out.cpR += cpR;
  out.mainL += cpL;
  out.mainR += cpR;
  addPanned(ltc, -0.7, out.toL, out.toR, out.mainL, out.mainR);
  addPanned(mtc, 0.0, out.toL, out.toR, out.mainL, out.mainR);
  addPanned(htc, 0.7, out.toL, out.toR, out.mainL, out.mainR);
  addHardLeft(cl, out.clL, out.mainL);
  addHardRight(cb, out.cbR, out.mainR);
  addCenter(ma, out.mainL, out.mainR);
  addCenter(lead, out.mainL, out.mainR);
  addCenter(bass, out.mainL, out.mainR);

  // Pair flags stay out of this sum. A patched jack is a tap.
  out.mainL *= master_;
  out.mainR *= master_;
  out.cv3 = 5.0 * u(knobs_.bassTone);
}

}  // namespace shogun
