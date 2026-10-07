#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace {

constexpr int kMidiBase = 36;
constexpr int kScaleSteps[4] = {8, 6, 4, 3};

int ccOf(const std::atomic<float>* value) {
  if (value == nullptr) return 0;
  const int cc = static_cast<int>(std::lround(value->load(std::memory_order_relaxed)));
  if (cc < 0) return 0;
  if (cc > 127) return 127;
  return cc;
}

}  // namespace

ShogunAudioProcessor::ShogunAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createLayout()) {
  // Default beat: kick and bass on 1 and 9, snare on 5 and 13, hats on the eighths.
  stepOn_[static_cast<int>(shogun::Voice::Bd1)].store((1u << 0) | (1u << 8));
  stepOn_[static_cast<int>(shogun::Voice::Sd)].store((1u << 4) | (1u << 12));
  stepOn_[static_cast<int>(shogun::Voice::Hh)].store(0x5555u);
  stepOn_[static_cast<int>(shogun::Voice::Bass)].store((1u << 0) | (1u << 8));

  knobCount_ = 0;
  for (const auto& voice : shogun_ui::voices()) {
    for (int i = 0; i < voice.count; ++i) {
      knobs_[static_cast<size_t>(knobCount_++)] =
          BoundKnob{voice.knobs[static_cast<size_t>(i)].field,
                    apvts.getRawParameterValue(voice.knobs[static_cast<size_t>(i)].id)};
    }
    levels_[static_cast<size_t>(voice.voice)] = apvts.getRawParameterValue(voice.levelId);
  }
  clock_ = apvts.getRawParameterValue("clock");
  tempo_ = apvts.getRawParameterValue("tempo");
  master_ = apvts.getRawParameterValue("master");
  scale_ = apvts.getRawParameterValue("scale");
}

juce::AudioProcessorValueTreeState::ParameterLayout ShogunAudioProcessor::createLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"clock", 1}, "Clock", juce::StringArray{"INT", "EXT"}, 0));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"tempo", 1}, "Tempo",
      juce::NormalisableRange<float>(60.0f, 180.0f, 0.1f), 120.0f));
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"master", 1}, "Master",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.45f));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"scale", 1}, "Scale", juce::StringArray{"8", "6", "4", "3"}, 2));

  for (const auto& voice : shogun_ui::voices()) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{voice.levelId, 1}, juce::String(voice.name) + " level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    for (int i = 0; i < voice.count; ++i) {
      const auto& knob = voice.knobs[static_cast<size_t>(i)];
      layout.add(std::make_unique<juce::AudioParameterInt>(
          juce::ParameterID{knob.id, 1}, juce::String(voice.name) + " " + knob.label, 0, 127,
          knob.initial));
    }
  }
  return layout;
}

void ShogunAudioProcessor::prepareToPlay(double sampleRate, int) {
  hostSr_ = sampleRate > 0.0 ? sampleRate : shogun::kFs;
  engine_.reset();
  appliedEpoch_ = 0;
  applyControls(false, 120.0);
  prime();
}

bool ShogunAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto& out = layouts.getMainOutputChannelSet();
  return layouts.getMainInputChannelSet().isDisabled() && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

shogun::Pattern ShogunAudioProcessor::makePattern() const {
  shogun::Pattern pattern;
  pattern.length = kSteps;
  for (int vi = 0; vi < kVoices; ++vi) {
    shogun::Track& track = pattern.track[vi];
    track.length = kSteps;
    const std::uint32_t bits = stepOn_[static_cast<size_t>(vi)].load(std::memory_order_relaxed);
    const bool notes = vi >= static_cast<int>(shogun::Voice::Lead);
    const int held = (vi == static_cast<int>(shogun::Voice::Bass)) ? 36 : 60;
    for (int step = 0; step < kSteps; ++step) {
      const bool on = (bits & (1u << step)) != 0;
      if (notes) {
        track.note[step].note = on ? held : -1;
        track.note[step].accent = 2;
      } else if (on) {
        track.drum[step].on = true;
        track.drum[step].accent = 2;
      }
    }
  }
  return pattern;
}

void ShogunAudioProcessor::applyControls(bool hostPlaying, double hostBpm) {
  shogun::Knobs knobs;
  for (int i = 0; i < knobCount_; ++i) {
    if (knobs_[static_cast<size_t>(i)].field == nullptr) continue;
    knobs.*(knobs_[static_cast<size_t>(i)].field) = ccOf(knobs_[static_cast<size_t>(i)].value);
  }
  engine_.setKnobs(knobs);
  for (int i = 0; i < kVoices; ++i) {
    const float level = levels_[static_cast<size_t>(i)] == nullptr
                            ? 1.0f
                            : levels_[static_cast<size_t>(i)]->load(std::memory_order_relaxed);
    engine_.setLevel(static_cast<shogun::Voice>(i), static_cast<double>(level));
  }
  const float master = master_ == nullptr ? 0.45f : master_->load(std::memory_order_relaxed);
  engine_.setMaster(static_cast<double>(master));

  const int clock = clock_ == nullptr ? 0 : static_cast<int>(std::lround(clock_->load(std::memory_order_relaxed)));
  engine_.setMode(clock == 0 ? shogun::ClockMode::Int : shogun::ClockMode::Ext);

  const float tempo = tempo_ == nullptr ? 120.0f : tempo_->load(std::memory_order_relaxed);
  engine_.setTempo(static_cast<double>(tempo));
  engine_.setHostTempo(hostBpm, hostPlaying);

  int scaleIndex = scale_ == nullptr ? 2 : static_cast<int>(std::lround(scale_->load(std::memory_order_relaxed)));
  if (scaleIndex < 0 || scaleIndex > 3) scaleIndex = 2;
  engine_.setScaleSteps(kScaleSteps[scaleIndex]);

  const std::uint32_t epoch = patternEpoch_.load(std::memory_order_acquire);
  if (epoch != appliedEpoch_) {
    engine_.setPattern(makePattern());
    appliedEpoch_ = epoch;
  }
}

void ShogunAudioProcessor::prime() {
  shogun::TrigIn silent;
  engine_.process(silent, newer_);
  older_ = shogun::Frame{};
  frac_ = 0.0;
}

void ShogunAudioProcessor::drainPads() {
  int read = padRead_.load(std::memory_order_relaxed);
  const int write = padWrite_.load(std::memory_order_acquire);
  while (read != write) {
    const Pad& pad = pads_[static_cast<size_t>(read)];
    if (pad.voice >= 0 && pad.voice < kVoices) {
      if (pad.note < 0) engine_.trigger(static_cast<shogun::Voice>(pad.voice), pad.gain, 0.0);
      else engine_.triggerNote(static_cast<shogun::Voice>(pad.voice), pad.note, pad.gain);
    }
    read = (read + 1) % kPadCap;
  }
  padRead_.store(read, std::memory_order_release);
}

void ShogunAudioProcessor::handleMidi(const juce::MidiMessage& msg) {
  if (msg.isNoteOn()) {
    const int voice = msg.getNoteNumber() - kMidiBase;
    if (voice < 0 || voice >= kVoices) return;
    const double gain = shogun::gVel(msg.getVelocity());
    if (voice >= static_cast<int>(shogun::Voice::Lead))
      engine_.triggerNote(static_cast<shogun::Voice>(voice), msg.getNoteNumber(), gain);
    else
      engine_.trigger(static_cast<shogun::Voice>(voice), gain, 0.0);
    return;
  }
  if (msg.isNoteOff()) {
    const int voice = msg.getNoteNumber() - kMidiBase;
    if (voice == static_cast<int>(shogun::Voice::Lead) || voice == static_cast<int>(shogun::Voice::Bass))
      engine_.release(static_cast<shogun::Voice>(voice));
  }
}

void ShogunAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  buffer.clear();

  bool hostPlaying = false;
  double hostBpm = tempo_ == nullptr ? 120.0 : static_cast<double>(tempo_->load(std::memory_order_relaxed));
  if (auto* head = getPlayHead()) {
    if (auto pos = head->getPosition()) {
      hostPlaying = pos->getIsPlaying();
      if (auto bpm = pos->getBpm()) hostBpm = *bpm;
    }
  }
  applyControls(hostPlaying, hostBpm);
  drainPads();

  const int n = buffer.getNumSamples();
  const int channels = buffer.getNumChannels();
  float* left = channels > 0 ? buffer.getWritePointer(0) : nullptr;
  float* right = channels > 1 ? buffer.getWritePointer(1) : left;
  if (left == nullptr) return;

  const double advance = shogun::kFs / hostSr_;
  shogun::TrigIn silent;
  auto midiIt = midi.begin();
  const auto midiEnd = midi.end();

  for (int i = 0; i < n; ++i) {
    while (midiIt != midiEnd && (*midiIt).samplePosition <= i) {
      handleMidi((*midiIt).getMessage());
      ++midiIt;
    }
    const double t = frac_;
    left[i] = static_cast<float>((1.0 - t) * older_.mainL + t * newer_.mainL);
    if (right != nullptr && right != left)
      right[i] = static_cast<float>((1.0 - t) * older_.mainR + t * newer_.mainR);
    else if (right != nullptr)
      right[i] = left[i];

    frac_ += advance;
    while (frac_ >= 1.0) {
      frac_ -= 1.0;
      older_ = newer_;
      engine_.process(silent, newer_);
    }
  }
  shownStep_.store(engine_.displayStep(), std::memory_order_relaxed);
}

void ShogunAudioProcessor::toggleStep(int voice, int step) {
  if (voice < 0 || voice >= kVoices || step < 0 || step >= kSteps) return;
  stepOn_[static_cast<size_t>(voice)].fetch_xor(1u << step, std::memory_order_relaxed);
  patternEpoch_.fetch_add(1, std::memory_order_release);
}

bool ShogunAudioProcessor::isStepOn(int voice, int step) const {
  if (voice < 0 || voice >= kVoices || step < 0 || step >= kSteps) return false;
  const std::uint32_t bits = stepOn_[static_cast<size_t>(voice)].load(std::memory_order_relaxed);
  return (bits & (1u << step)) != 0;
}

void ShogunAudioProcessor::pushPad(int voice) {
  if (voice < 0 || voice >= kVoices) return;
  int note = -1;
  if (voice == static_cast<int>(shogun::Voice::Lead)) note = 60;
  if (voice == static_cast<int>(shogun::Voice::Bass)) note = 36;
  const int write = padWrite_.load(std::memory_order_relaxed);
  const int next = (write + 1) % kPadCap;
  if (next == padRead_.load(std::memory_order_acquire)) return;
  pads_[static_cast<size_t>(write)] = Pad{voice, note, 1.0f};
  padWrite_.store(next, std::memory_order_release);
}

void ShogunAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
  juce::XmlElement root("SHOGUN");
  if (auto params = apvts.copyState().createXml()) root.addChildElement(params.release());
  auto* steps = root.createNewChildElement("Steps");
  juce::String masks;
  for (int i = 0; i < kVoices; ++i) {
    if (i > 0) masks << ",";
    masks << static_cast<int>(stepOn_[static_cast<size_t>(i)].load(std::memory_order_relaxed));
  }
  steps->setAttribute("masks", masks);
  copyXmlToBinary(root, destData);
}

void ShogunAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> root(getXmlFromBinary(data, sizeInBytes));
  if (root == nullptr || !root->hasTagName("SHOGUN")) return;
  if (auto* params = root->getChildByName(apvts.state.getType()))
    apvts.replaceState(juce::ValueTree::fromXml(*params));
  if (auto* steps = root->getChildByName("Steps")) {
    const auto parts = juce::StringArray::fromTokens(steps->getStringAttribute("masks"), ",", "");
    for (int i = 0; i < kVoices && i < parts.size(); ++i)
      stepOn_[static_cast<size_t>(i)].store(static_cast<std::uint32_t>(parts[i].getIntValue()),
                                             std::memory_order_relaxed);
    patternEpoch_.fetch_add(1, std::memory_order_release);
  }
}

juce::AudioProcessorEditor* ShogunAudioProcessor::createEditor() {
  return new ShogunAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ShogunAudioProcessor(); }
