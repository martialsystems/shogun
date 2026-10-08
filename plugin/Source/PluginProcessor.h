#pragma once
// SHOGUN plugin shell (spec v2.2 §15.0 step 4): the engine at the host rate (no resampler), host lock, float
// parameters with SECTION:LABEL ids, latency 0/23/26, main + 8 aux stereo outputs, state XML SHOGUN v2 with the
// JSON patch (§14) embedded.

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <memory>

#include "shogun.h"

class ShogunAudioProcessor : public juce::AudioProcessor, private juce::AsyncUpdater {
 public:
  ShogunAudioProcessor();
  ~ShogunAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "SHOGUN"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 2.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return "INIT"; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  // ---------------------------------------------------------------- UI side (message thread)
  juce::AudioProcessorValueTreeState apvts;
  juce::RangedAudioParameter* param(int id) const { return params_[static_cast<size_t>(id)]; }
  float paramU(int id) const { return raw_[static_cast<size_t>(id)]->load(std::memory_order_relaxed); }
  void setParamU(int id, float u);  // gesture-wrapped host notification

  // Pattern, matrix rows, cables and CV AMT live in a UI copy; the audio thread picks a changed copy up at the next
  // block (try-lock, no waiting on the audio thread).
  shogun::Pattern& editPattern() { return uiPattern_; }
  shogun::mod::Row* editRows() { return uiRows_.data(); }
  void commitEdits();  // after editing the UI copies
  bool addCable(int from, int to);
  void removeCablesAt(int port);
  int cableCount() const { return uiCableCount_; }
  std::pair<int, int> cable(int i) const { return {uiCables_[static_cast<size_t>(i)].first, uiCables_[static_cast<size_t>(i)].second}; }
  double cvAmt(int port) const { return uiCvAmt_[static_cast<size_t>(port)]; }
  void setCvAmt(int port, double amt);
  void pushPad(int voice);     // trigger pad (any mode, like MIDI)
  void requestRun(bool run);
  void requestRestart();
  void requestIdeal();
  void initPatch();

  // ---------------------------------------------------------------- meters (written by the audio thread)
  struct Meters {
    std::atomic<float> peakL{0}, peakR{0}, cpu{0};
    std::atomic<int> step{1}, globalStep{0};
    std::atomic<bool> running{false}, clip{false};
    std::array<std::atomic<float>, shogun::kVoices> voicePeak{};
    std::array<std::atomic<bool>, shogun::kVoices> voiceActive{};
    std::array<std::atomic<float>, 4> busGr{};
    std::array<std::atomic<float>, 4> lfo{};
    std::atomic<int> latency{23};
    std::atomic<double> sampleRate{48000.0};
  };
  Meters meters;
  int osFactor() const { return osNow_; }

  // Test/probe access (offline: call only while not processing).
  shogun::Engine& engine() { return *engine_; }

 private:
  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
  void handleAsyncUpdate() override;
  void applyParams(bool now);
  void handleMidi(const juce::MidiMessage& msg);
  void pickUpEdits();
  juce::var patchToJson() const;
  void patchFromJson(const juce::var& v);

  std::unique_ptr<shogun::Engine> engine_;
  std::vector<juce::RangedAudioParameter*> params_;
  std::vector<std::atomic<float>*> raw_;
  std::vector<float> last_;
  std::vector<int> ccToParam_[128];

  double sr_ = 48000.0;
  int block_ = 512;
  int osNow_ = 2;
  bool offlineNow_ = false;
  std::atomic<bool> needPrepare_{false};

  // UI copies and hand-over
  juce::SpinLock editLock_;
  std::atomic<std::uint32_t> editEpoch_{1};
  std::uint32_t appliedEpoch_ = 0;
  shogun::Pattern uiPattern_;
  std::array<shogun::mod::Row, shogun::mod::kRows> uiRows_{};
  static constexpr int kMaxCables = 64;
  std::array<std::pair<int, int>, kMaxCables> uiCables_{};
  int uiCableCount_ = 0;
  std::array<double, shogun::kPorts> uiCvAmt_{};
  std::array<std::uint8_t, shogun::kPorts> uiLaw_{};

  // UI → audio one-shot requests
  std::atomic<std::uint32_t> padMask_{0};
  std::atomic<int> runReq_{-1};
  std::atomic<bool> restartReq_{false}, idealReq_{false};

  juce::AudioBuffer<float> scratch_;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShogunAudioProcessor)
};
