#pragma once

#include "KnobTable.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>

class ShogunAudioProcessor : public juce::AudioProcessor {
 public:
  ShogunAudioProcessor();
  ~ShogunAudioProcessor() override = default;

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
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return "INIT"; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  static constexpr int kVoices = 16;
  static constexpr int kSteps = 16;

  void toggleStep(int voice, int step);
  bool isStepOn(int voice, int step) const;
  void pushPad(int voice);
  int currentStep() const { return shownStep_.load(std::memory_order_relaxed); }

  juce::AudioProcessorValueTreeState apvts;

 private:
  struct Pad {
    int voice = 0;
    int note = -1;
    float gain = 1.0f;
  };

  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
  void applyControls(bool hostPlaying, double hostBpm);
  void drainPads();
  void handleMidi(const juce::MidiMessage& msg);
  shogun::Pattern makePattern() const;
  void prime();

  shogun::Engine engine_;
  shogun::Frame older_{};
  shogun::Frame newer_{};
  double frac_ = 0.0;
  double hostSr_ = shogun::kFs;

  std::array<std::atomic<std::uint32_t>, kVoices> stepOn_{};
  std::atomic<std::uint32_t> patternEpoch_{1};
  std::uint32_t appliedEpoch_ = 0;
  std::atomic<int> shownStep_{1};

  static constexpr int kPadCap = 64;
  std::array<Pad, kPadCap> pads_{};
  std::atomic<int> padRead_{0};
  std::atomic<int> padWrite_{0};

  struct BoundKnob {
    int shogun::Knobs::* field = nullptr;
    std::atomic<float>* value = nullptr;
  };
  std::array<BoundKnob, 64> knobs_{};
  int knobCount_ = 0;
  std::array<std::atomic<float>*, kVoices> levels_{};
  std::atomic<float>* clock_ = nullptr;
  std::atomic<float>* tempo_ = nullptr;
  std::atomic<float>* master_ = nullptr;
  std::atomic<float>* scale_ = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShogunAudioProcessor)
};
