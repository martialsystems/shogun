#pragma once

#include "PluginProcessor.h"

class ShogunAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit ShogunAudioProcessorEditor(ShogunAudioProcessor&);
  ~ShogunAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  void timerCallback() override;
  void selectVoice(int voice);
  void refreshMode();
  void refreshSteps();

  struct Plate;

  ShogunAudioProcessor& proc_;
  std::unique_ptr<Plate> plate_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShogunAudioProcessorEditor)
};
