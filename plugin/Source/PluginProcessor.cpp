#include "PluginProcessor.h"

#include <jidai/CableStandard.h>

#include <cmath>
#include <cstring>

#include "PluginEditor.h"

using namespace shogun;

namespace {

const char* const kSrcNames[] = {"", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "ENV", "PENV", "VEL",
                                 "ACC", "RND HIT", "NOTE", "RND", "MOD W", "AT"};
constexpr int kSrcCount = static_cast<int>(sizeof kSrcNames / sizeof kSrcNames[0]);
const char* const kCurveNames[] = {"LIN", "EXP", "LOG", "S"};

juce::String srcToString(int src, int voice) {
  if (src <= mod::SRC_NONE || src >= kSrcCount) return {};
  juce::String s(kSrcNames[src]);
  if (mod::perVoiceSource(src) && voice >= 0) s << "@" << kVoiceNames[voice];
  return s;
}
void srcFromString(const juce::String& s, int& src, int& voice) {
  src = mod::SRC_NONE;
  voice = -1;
  if (s.isEmpty()) return;
  const juce::String name = s.upToFirstOccurrenceOf("@", false, false);
  const juce::String v = s.fromFirstOccurrenceOf("@", false, false);
  for (int i = 1; i < kSrcCount; ++i)
    if (name == kSrcNames[i]) src = i;
  for (int i = 0; i < kVoices; ++i)
    if (v == kVoiceNames[i]) voice = i;
}

// Display text (host automation lanes): the engine's formatParam, the same text the panel LCDs show.
juce::String paramText(int id, float u) {
  char buf[48];
  formatParam(id, static_cast<double>(u), buf, sizeof buf);
  return juce::String::fromUTF8(buf);
}

int osFromParam(double u) { return 1 << stepIndex(u, 3); }  // 1X / 2X / 4X

}  // namespace

ShogunAudioProcessor::ShogunAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Main", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Aux 1", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Aux 2", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Aux 3", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Aux 4", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Aux 5", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Aux 6", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Aux 7", juce::AudioChannelSet::stereo(), false)
                         .withOutput("Aux 8", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, nullptr, "SHOGUN", createLayout()),
      engine_(std::make_unique<Engine>()) {
  params_.resize(kParamCount);
  raw_.resize(kParamCount);
  last_.assign(kParamCount, -1.0f);
  for (int i = 0; i < kParamCount; ++i) {
    params_[static_cast<size_t>(i)] = apvts.getParameter(kParams[i].id);
    raw_[static_cast<size_t>(i)] = apvts.getRawParameterValue(kParams[i].id);
    jassert(params_[static_cast<size_t>(i)] != nullptr);
    if (kParams[i].cc >= 0 && kParams[i].cc < 128) ccToParam_[kParams[i].cc].push_back(i);
  }
  engine_->prepare(sr_, osNow_);
  engine_->loadInit();
  uiPattern_ = engine_->pattern();
  for (int i = 0; i < kPorts; ++i) uiCvAmt_[static_cast<size_t>(i)] = 1.0;
  for (auto& gr : meters.busGr) gr.store(1.0f);  // compressor gain 1 = no reduction
  setLatencySamples(engine_->latencySamples());
}

ShogunAudioProcessor::~ShogunAudioProcessor() { cancelPendingUpdate(); }

juce::AudioProcessorValueTreeState::ParameterLayout ShogunAudioProcessor::createLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  for (int i = 0; i < kParamCount; ++i) {
    const ParamInfo& p = kParams[i];
    const int id = i;
    auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction(
        [id](float v, int) { return paramText(id, v); });
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{p.id, 1}, p.id,
                                                           juce::NormalisableRange<float>(0.0f, 1.0f), p.def,
                                                           attrs));
  }
  return layout;
}

bool ShogunAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
  for (int b = 1; b < layouts.outputBuses.size(); ++b) {
    const auto& set = layouts.outputBuses.getReference(b);
    if (!set.isDisabled() && set != juce::AudioChannelSet::stereo()) return false;
  }
  return layouts.inputBuses.isEmpty();
}

void ShogunAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  sr_ = sampleRate;
  block_ = samplesPerBlock;
  offlineNow_ = isNonRealtime();
  int os = osFromParam(static_cast<double>(paramU(P_GLOBAL_OS)));
  if (offlineNow_ && stepIndex(static_cast<double>(paramU(P_GLOBAL_OFFLINE)), 2) == 1) os = 4;
  osNow_ = os;
  // Coefficients at the host rate (§3.1): prepare() recomputes every filter for fs = sampleRate, no resampler.
  engine_->prepare(sr_, osNow_);
  applyParams(true);
  appliedEpoch_ = 0;  // re-apply pattern / matrix / cables
  pickUpEdits();
  meters.latency.store(engine_->latencySamples());
  meters.sampleRate.store(sr_);
  setLatencySamples(engine_->latencySamples());
}

void ShogunAudioProcessor::handleAsyncUpdate() {
  // GLOBAL:OS changed: re-prepare off the audio thread and report the new latency (0 / 23 / 26).
  suspendProcessing(true);
  prepareToPlay(sr_, block_);
  suspendProcessing(false);
  updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withLatencyChanged(true));
}

void ShogunAudioProcessor::applyParams(bool now) {
  for (int i = 0; i < kParamCount; ++i) {
    const float u = raw_[static_cast<size_t>(i)]->load(std::memory_order_relaxed);
    if (shogun::dsp::exactEq(u, last_[static_cast<size_t>(i)])) continue;
    last_[static_cast<size_t>(i)] = u;
    if (now)
      engine_->setParamNow(i, static_cast<double>(u));
    else
      engine_->setParam(i, static_cast<double>(u));
  }
}

void ShogunAudioProcessor::setParamU(int id, float u) {
  auto* p = param(id);
  p->beginChangeGesture();
  p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, u));
  p->endChangeGesture();
}

// ---------------------------------------------------------------- UI copies

void ShogunAudioProcessor::commitEdits() { editEpoch_.fetch_add(1, std::memory_order_release); }

bool ShogunAudioProcessor::addCable(int from, int to) {
  if (from < 0 || to < 0 || from >= kPorts || to >= kPorts) return false;
  if (kPortTable[from].dir != PortDir::Out || kPortTable[to].dir != PortDir::In) return false;
  if (kPortTable[to].type == PortType::Gate && kPortTable[from].type != PortType::Gate) return false;
  const juce::SpinLock::ScopedLockType sl(editLock_);
  for (int i = 0; i < uiCableCount_; ++i)
    if (uiCables_[static_cast<size_t>(i)] == std::make_pair(from, to)) return true;
  if (uiCableCount_ >= kMaxCables) return false;
  uiCables_[static_cast<size_t>(uiCableCount_++)] = {from, to};
  commitEdits();
  return true;
}

void ShogunAudioProcessor::removeCablesAt(int port) {
  const juce::SpinLock::ScopedLockType sl(editLock_);
  int n = 0;
  for (int i = 0; i < uiCableCount_; ++i) {
    const auto c = uiCables_[static_cast<size_t>(i)];
    if (c.first != port && c.second != port) uiCables_[static_cast<size_t>(n++)] = c;
  }
  uiCableCount_ = n;
  commitEdits();
}

void ShogunAudioProcessor::setCvAmt(int port, double amt) {
  uiCvAmt_[static_cast<size_t>(port)] = juce::jlimit(-1.0, 1.0, amt);
  commitEdits();
}

void ShogunAudioProcessor::pushPad(int voice) {
  if (voice >= 0 && voice < kVoices) padMask_.fetch_or(1u << voice);
}
void ShogunAudioProcessor::requestRun(bool run) { runReq_.store(run ? 1 : 0); }
void ShogunAudioProcessor::requestRestart() { restartReq_.store(true); }
void ShogunAudioProcessor::requestIdeal() {
  setParamU(P_GLOBAL_TOLERANCE, 0.0f);
  setParamU(P_GLOBAL_DRIFT, 0.0f);
}

void ShogunAudioProcessor::initPatch() {
  for (int i = 0; i < kParamCount; ++i) param(i)->setValueNotifyingHost(kParams[i].def);
  {
    const juce::SpinLock::ScopedLockType sl(editLock_);
    uiPattern_ = Pattern();
    uiRows_ = {};
    uiCableCount_ = 0;
    for (int i = 0; i < kPorts; ++i) {
      uiCvAmt_[static_cast<size_t>(i)] = 1.0;
      uiLaw_[static_cast<size_t>(i)] = 0;
    }
  }
  commitEdits();
}

void ShogunAudioProcessor::pickUpEdits() {
  const std::uint32_t e = editEpoch_.load(std::memory_order_acquire);
  if (e == appliedEpoch_) return;
  const juce::SpinLock::ScopedTryLockType tl(editLock_);
  if (!tl.isLocked()) return;  // the UI is mid-edit: take it next block
  engine_->setPattern(uiPattern_);
  for (int i = 0; i < mod::kRows; ++i) engine_->modulation().rows[i] = uiRows_[static_cast<size_t>(i)];
  engine_->clearCables();
  for (int i = 0; i < uiCableCount_; ++i)
    engine_->addCable(uiCables_[static_cast<size_t>(i)].first, uiCables_[static_cast<size_t>(i)].second);
  for (int i = 0; i < kPorts; ++i) {
    engine_->setCvAmt(i, uiCvAmt_[static_cast<size_t>(i)]);
    engine_->setInputLaw(i, uiLaw_[static_cast<size_t>(i)]);
  }
  appliedEpoch_ = e;
}

// ---------------------------------------------------------------- audio

void ShogunAudioProcessor::handleMidi(const juce::MidiMessage& m) {
  // Drums: notes 36–49 (BD1 … HTC) on any channel but 1/2. LEAD on channel 1, BASS on channel 2 (MIDI note = SHOGUN
  // note, 48 = C3 = 0 V). Notes 50/51 on other channels play LEAD/BASS at C3 (old map).
  const int ch = m.getChannel();
  if (m.isNoteOn()) {
    const double vel = 5.0 * m.getVelocity() / 127.0;
    const int n = m.getNoteNumber();
    if (ch == 1 || ch == 2) {
      engine_->noteOn(ch == 1 ? LEAD : BASS, n, vel);
      return;
    }
    const int v = n - 36;
    if (v >= 0 && v < LEAD) engine_->trigger(v, vel, 0.0, 3);
    else if (v == LEAD || v == BASS) engine_->noteOn(v, 48, vel);
  } else if (m.isNoteOff()) {
    const int n = m.getNoteNumber();
    if (ch == 1 || ch == 2) engine_->noteOff(ch == 1 ? LEAD : BASS);
    else if (n - 36 == LEAD || n - 36 == BASS) engine_->noteOff(n - 36);
  } else if (m.isController()) {
    const int cc = m.getControllerNumber();
    const double u = m.getControllerValue() / 127.0;  // old CC integer → u = cc/127 (§14)
    if (cc == 1) {
      engine_->setModWheel(u);
      return;
    }
    for (int id : ccToParam_[cc]) {
      engine_->setParam(id, u);
      last_[static_cast<size_t>(id)] = -1.0f;  // the host parameter wins again at its next change
    }
  } else if (m.isChannelPressure()) {
    engine_->setAftertouch(m.getChannelPressureValue() / 127.0);
  }
}

void ShogunAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  const int n = buffer.getNumSamples();
  const double t0 = juce::Time::getMillisecondCounterHiRes();

  // OS / OFFLINE changes re-prepare off the audio thread.
  int wantOs = osFromParam(static_cast<double>(paramU(P_GLOBAL_OS)));
  if (isNonRealtime() && stepIndex(static_cast<double>(paramU(P_GLOBAL_OFFLINE)), 2) == 1) wantOs = 4;
  if (wantOs != osNow_ && !isUpdatePending()) triggerAsyncUpdate();

  // Host lock (§10.1): ppq / bpm / playing at the first sample of the block.
  HostTransport ht;
  if (auto* ph = getPlayHead()) {
    if (auto pos = ph->getPosition()) {
      if (auto ppq = pos->getPpqPosition()) {
        ht.valid = true;
        ht.ppq = *ppq;
      }
      if (auto bpm = pos->getBpm()) ht.bpm = *bpm;
      ht.playing = pos->getIsPlaying();
    }
  }
  engine_->setHostTransport(ht);
  applyParams(false);
  pickUpEdits();

  const int rq = runReq_.exchange(-1);
  if (rq >= 0) engine_->setRunning(rq == 1);
  if (restartReq_.exchange(false)) engine_->restart();
  const std::uint32_t pads = padMask_.exchange(0);
  for (int v = 0; v < kVoices; ++v)
    if (pads & (1u << v)) {
      if (isDrum(v)) engine_->trigger(v, 5.0, 0.0, 3);
      else engine_->noteOn(v, 48, 5.0);
    }

  // Outputs: main pair + the enabled aux pairs.
  auto mainBus = getBusBuffer(buffer, false, 0);
  float* L = mainBus.getWritePointer(0);
  float* R = mainBus.getNumChannels() > 1 ? mainBus.getWritePointer(1) : nullptr;
  float* aux[8][2] = {};
  const int nBuses = getBusCount(false);
  for (int b = 1; b < nBuses && b <= 8; ++b) {
    auto* bus = getBus(false, b);
    if (bus == nullptr || !bus->isEnabled()) continue;
    auto bb = getBusBuffer(buffer, false, b);
    if (bb.getNumChannels() < 2) continue;
    aux[b - 1][0] = bb.getWritePointer(0);
    aux[b - 1][1] = bb.getWritePointer(1);
  }

  float pkL = 0.0f, pkR = 0.0f;
  float vpk[kVoices] = {};
  auto it = midi.cbegin();
  for (int i = 0; i < n; ++i) {
    while (it != midi.cend() && (*it).samplePosition <= i) {
      handleMidi((*it).getMessage());
      ++it;
    }
    engine_->processSample();
    const float l = static_cast<float>(engine_->mainL());
    const float r = static_cast<float>(engine_->mainR());
    L[i] = l;
    if (R) R[i] = r;
    pkL = std::fmax(pkL, std::fabs(l));
    pkR = std::fmax(pkR, std::fabs(r));
    const double* ax = engine_->aux();
    for (int b = 0; b < 8; ++b)
      if (aux[b][0]) {
        aux[b][0][i] = static_cast<float>(ax[2 * b]);
        aux[b][1][i] = static_cast<float>(ax[2 * b + 1]);
      }
    for (int v = 0; v < kVoices; ++v) vpk[v] = std::fmax(vpk[v], static_cast<float>(std::fabs(engine_->voiceOut(v))));
  }
  while (it != midi.cend()) {
    handleMidi((*it).getMessage());
    ++it;
  }
  midi.clear();

  // Meters (relaxed; the editor decays them).
  auto maxStore = [](std::atomic<float>& a, float v) {
    if (v > a.load(std::memory_order_relaxed)) a.store(v, std::memory_order_relaxed);
  };
  maxStore(meters.peakL, pkL);
  maxStore(meters.peakR, pkR);
  for (int v = 0; v < kVoices; ++v) {
    maxStore(meters.voicePeak[static_cast<size_t>(v)], vpk[v]);
    meters.voiceActive[static_cast<size_t>(v)].store(engine_->voiceActive(v), std::memory_order_relaxed);
  }
  for (int b = 0; b < 4; ++b) meters.busGr[static_cast<size_t>(b)].store(static_cast<float>(engine_->busGain(b)));
  for (int l = 0; l < 4; ++l)
    meters.lfo[static_cast<size_t>(l)].store(static_cast<float>(engine_->modulation().lfo[l].value(-1)));
  meters.step.store(engine_->displayStep(), std::memory_order_relaxed);
  meters.globalStep.store(static_cast<int>(engine_->globalStep()), std::memory_order_relaxed);
  meters.running.store(engine_->running(), std::memory_order_relaxed);
  if (engine_->clipOver()) meters.clip.store(true, std::memory_order_relaxed);
  const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
  const double budget = 1000.0 * n / sr_;
  meters.cpu.store(static_cast<float>(budget > 0 ? ms / budget : 0.0), std::memory_order_relaxed);
}

// ---------------------------------------------------------------- state (§14)

juce::var ShogunAudioProcessor::patchToJson() const {
  auto* root = new juce::DynamicObject();
  root->setProperty("format", "shogun-patch");
  root->setProperty("version", 2);
  root->setProperty("name", juce::String(uiPattern_.name));
  root->setProperty("unit", "0x" + juce::String::toHexString(static_cast<juce::int64>(engine_->serial())).toUpperCase());
  root->setProperty("tolerance", static_cast<double>(paramU(P_GLOBAL_TOLERANCE)));
  root->setProperty("drift", static_cast<double>(paramU(P_GLOBAL_DRIFT)));

  auto* params = new juce::DynamicObject();
  for (int i = 0; i < kParamCount; ++i) params->setProperty(kParams[i].id, static_cast<double>(paramU(i)));
  root->setProperty("params", juce::var(params));

  juce::Array<juce::var> mods;
  for (const auto& r : uiRows_) {
    if (r.src == mod::SRC_NONE || r.dst < 0) continue;
    auto* o = new juce::DynamicObject();
    o->setProperty("src", srcToString(r.src, r.srcVoice));
    o->setProperty("dst", kParams[r.dst].id);
    o->setProperty("depth", r.depth);
    o->setProperty("via", r.via == mod::SRC_NONE ? juce::var() : juce::var(srcToString(r.via, r.viaVoice)));
    o->setProperty("curve", kCurveNames[r.curve & 3]);
    o->setProperty("on", r.on);
    mods.add(juce::var(o));
  }
  root->setProperty("mod", mods);

  juce::Array<juce::var> cables;
  for (int i = 0; i < uiCableCount_; ++i) {
    juce::Array<juce::var> c;
    c.add(juce::String("SHOGUN/") + kPortTable[uiCables_[static_cast<size_t>(i)].first].id);
    c.add(juce::String("SHOGUN/") + kPortTable[uiCables_[static_cast<size_t>(i)].second].id);
    // no colour entry: the cable takes its source jack's role colour (JCS R14); an entry would be an override
    cables.add(c);
  }
  root->setProperty("cables", cables);

  auto* cv = new juce::DynamicObject();
  for (int i = 0; i < kPorts; ++i)
    if (kPortTable[i].dir == PortDir::In && !shogun::dsp::exactEq(uiCvAmt_[static_cast<size_t>(i)], 1.0))
      cv->setProperty(kPortTable[i].id, uiCvAmt_[static_cast<size_t>(i)]);
  root->setProperty("cvAmt", juce::var(cv));
  auto* laws = new juce::DynamicObject();  // alias laws of migrated inputs (lin55 / drum PITCH 1/12)
  for (int i = 0; i < kPorts; ++i)
    if (uiLaw_[static_cast<size_t>(i)] != 0) laws->setProperty(kPortTable[i].id, uiLaw_[static_cast<size_t>(i)]);
  root->setProperty("inLaw", juce::var(laws));

  static const char* const kScale[] = {"1/32", "1/16", "1/8T", "1/8"};
  auto* seq = new juce::DynamicObject();
  seq->setProperty("pattern", juce::String(uiPattern_.name));
  seq->setProperty("seed", static_cast<juce::int64>(uiPattern_.seed));
  juce::Array<juce::var> tracks;
  for (int t = 0; t < 16; ++t) {
    const Track& tr = uiPattern_.tracks[t];
    auto* to = new juce::DynamicObject();
    to->setProperty("id", kVoiceNames[t]);
    to->setProperty("len", tr.len);
    to->setProperty("scale", tr.scale < 0 ? juce::var() : juce::var(kScale[tr.scale & 3]));
    to->setProperty("swing", tr.swing < 0 ? juce::var() : juce::var(tr.swing));
    to->setProperty("shift", tr.shift);
    juce::Array<juce::var> steps;
    for (int s = 0; s < kMaxSteps; ++s) {
      const Step& st = tr.steps[s];
      if (!st.on && st.nLocks == 0) continue;
      auto* so = new juce::DynamicObject();
      so->setProperty("i", s);
      so->setProperty("on", st.on);
      so->setProperty("acc", st.acc);
      so->setProperty("prob", static_cast<double>(st.prob));
      so->setProperty("micro", static_cast<double>(st.micro));
      so->setProperty("flam", st.flam);
      so->setProperty("ratchet", st.ratchet);
      if (!shogun::dsp::exactEq(st.bend, 0.0f)) so->setProperty("bend", static_cast<double>(st.bend));
      if (!isDrum(t)) {
        so->setProperty("note", st.note);
        so->setProperty("tie", st.tie);
      }
      auto* locks = new juce::DynamicObject();
      for (int k = 0; k < st.nLocks; ++k) locks->setProperty(kParams[st.locks[k].param].id, static_cast<double>(st.locks[k].u));
      so->setProperty("locks", juce::var(locks));
      steps.add(juce::var(so));
    }
    to->setProperty("steps", steps);
    tracks.add(juce::var(to));
  }
  seq->setProperty("tracks", tracks);
  root->setProperty("seq", juce::var(seq));
  return juce::var(root);
}

void ShogunAudioProcessor::patchFromJson(const juce::var& v) {
  if (!v.isObject()) return;
  if (auto* params = v["params"].getDynamicObject())
    for (const auto& kv : params->getProperties()) {
      const int id = findParam(kv.name.toString().toRawUTF8());
      if (id >= 0) param(id)->setValueNotifyingHost(static_cast<float>(static_cast<double>(kv.value)));
    }
  const juce::SpinLock::ScopedLockType sl(editLock_);
  uiRows_ = {};
  if (auto* mods = v["mod"].getArray()) {
    int i = 0;
    for (const auto& m : *mods) {
      if (i >= mod::kRows) break;
      mod::Row r;
      srcFromString(m["src"].toString(), r.src, r.srcVoice);
      r.dst = findParam(m["dst"].toString().toRawUTF8());
      r.depth = static_cast<double>(m["depth"]);
      if (!m["via"].isVoid()) srcFromString(m["via"].toString(), r.via, r.viaVoice);
      const juce::String c = m["curve"].toString();
      for (int k = 0; k < 4; ++k)
        if (c == kCurveNames[k]) r.curve = k;
      r.on = m.hasProperty("on") ? static_cast<bool>(m["on"]) : true;
      if (r.src != mod::SRC_NONE && r.dst >= 0) uiRows_[static_cast<size_t>(i++)] = r;
    }
  }
  for (int i = 0; i < kPorts; ++i) {
    uiCvAmt_[static_cast<size_t>(i)] = 1.0;
    uiLaw_[static_cast<size_t>(i)] = 0;
  }
  // Saved jack ids resolve through the alias table (§12.3): HZ/V → NOTE (lin55), TOM:PITCH → 3 toms (AMT 1/12).
  // resolvePort parses every form (SHOGUN#N/…, SHOGUN/…, bare) with the shared JCS R6 parseJackId (engine/ports.h).
  auto resolve = [](const juce::String& full, int out[3], int* law) { return resolvePort(full.toRawUTF8(), out, law); };
  uiCableCount_ = 0;
  if (auto* cables = v["cables"].getArray())
    for (const auto& c : *cables) {
      if (!c.isArray() || c.size() < 2) continue;
      int from[3], to[3], lf = 0, lt = 0;
      const int nf = resolve(c[0].toString(), from, &lf);
      const int nt = resolve(c[1].toString(), to, &lt);
      if (nf < 1) continue;
      for (int k = 0; k < nt && uiCableCount_ < kMaxCables; ++k) {
        if (to[k] < 0) continue;
        uiCables_[static_cast<size_t>(uiCableCount_++)] = {from[0], to[k]};
        if (lt == 1) uiLaw_[static_cast<size_t>(to[k])] = 1;
        if (lt == 2) uiCvAmt_[static_cast<size_t>(to[k])] = 1.0 / 12.0;
      }
    }
  if (auto* cv = v["cvAmt"].getDynamicObject())
    for (const auto& kv : cv->getProperties()) {
      int p[3], law = 0;
      const int np = resolve(kv.name.toString(), p, &law);
      for (int k = 0; k < np; ++k)
        if (p[k] >= 0) uiCvAmt_[static_cast<size_t>(p[k])] = static_cast<double>(kv.value) * (law == 2 ? 1.0 / 12.0 : 1.0);
    }
  if (auto* laws = v["inLaw"].getDynamicObject())
    for (const auto& kv : laws->getProperties()) {
      const int p = portFromId(kv.name.toString().toRawUTF8());
      if (p >= 0) uiLaw_[static_cast<size_t>(p)] = static_cast<std::uint8_t>(static_cast<int>(kv.value));
    }

  uiPattern_ = Pattern();
  const juce::var seq = v["seq"];
  if (seq.isObject()) {
    uiPattern_.setName(seq["pattern"].toString().toRawUTF8());
    if (seq.hasProperty("seed")) uiPattern_.seed = static_cast<std::uint32_t>(static_cast<juce::int64>(seq["seed"]));
    if (auto* tracks = seq["tracks"].getArray())
      for (const auto& to : *tracks) {
        int t = -1;
        for (int k = 0; k < 16; ++k)
          if (to["id"].toString() == kVoiceNames[k]) t = k;
        if (t < 0) continue;
        Track& tr = uiPattern_.tracks[t];
        tr.len = juce::jlimit(1, kMaxSteps, static_cast<int>(to.getProperty("len", 16)));
        const juce::String sc = to["scale"].toString();
        static const char* const kScale[] = {"1/32", "1/16", "1/8T", "1/8"};
        tr.scale = -1;
        for (int k = 0; k < 4; ++k)
          if (sc == kScale[k]) tr.scale = k;
        tr.swing = to["swing"].isVoid() ? -1.0 : static_cast<double>(to["swing"]);
        tr.shift = static_cast<double>(to.getProperty("shift", 0.0));
        if (auto* steps = to["steps"].getArray())
          for (const auto& so : *steps) {
            const int s = static_cast<int>(so["i"]);
            if (s < 0 || s >= kMaxSteps) continue;
            Step& st = tr.steps[s];
            st.on = so.hasProperty("on") ? static_cast<bool>(so["on"]) : true;
            st.acc = static_cast<std::uint8_t>(juce::jlimit(1, 3, static_cast<int>(so.getProperty("acc", 2))));
            st.prob = static_cast<float>(static_cast<double>(so.getProperty("prob", 1.0)));
            st.micro = static_cast<float>(static_cast<double>(so.getProperty("micro", 0.0)));
            st.flam = static_cast<std::uint8_t>(static_cast<int>(so.getProperty("flam", 0)));
            st.ratchet = static_cast<std::uint8_t>(static_cast<int>(so.getProperty("ratchet", 1)));
            st.bend = static_cast<float>(static_cast<double>(so.getProperty("bend", 0.0)));
            st.note = static_cast<std::int8_t>(static_cast<int>(so.getProperty("note", 48)));
            st.tie = static_cast<bool>(so.getProperty("tie", false));
            if (auto* locks = so["locks"].getDynamicObject())
              for (const auto& kv : locks->getProperties()) {
                const int id = findParam(kv.name.toString().toRawUTF8());
                if (id >= 0) st.setLock(id, static_cast<float>(static_cast<double>(kv.value)));
              }
          }
      }
  }
  commitEdits();
}

void ShogunAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  juce::XmlElement xml("SHOGUN");
  xml.setAttribute("version", 2);
  xml.setAttribute("running", engine_->running() ? 1 : 0);
  xml.addTextElement(juce::JSON::toString(patchToJson(), true));
  copyXmlToBinary(xml, destData);
}

void ShogunAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
  if (xml == nullptr || !xml->hasTagName("SHOGUN")) return;
  if (xml->getIntAttribute("version", 1) < 2) return;  // v1 plugin states (old AudioParameterInt ids) are not migrated
  patchFromJson(juce::JSON::parse(xml->getAllSubText()));
}

juce::AudioProcessorEditor* ShogunAudioProcessor::createEditor() { return new ShogunAudioProcessorEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShogunAudioProcessor(); }
