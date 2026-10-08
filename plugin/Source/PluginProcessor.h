#pragma once
// SHOGUN plugin shell (spec v2.2 §15.0 step 4): the engine at the host rate (no resampler), host lock, float
// parameters with SECTION:LABEL ids, latency 0/23/26, main + 8 aux stereo outputs, state XML SHOGUN v2 with the
// JSON patch (§14) embedded.

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <deque>
#include <memory>

#include "shogun.h"

class ShogunAudioProcessor : public juce::AudioProcessor, private juce::Timer {
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

  // Programs: the factory bank (engine/factory.h), INIT first. A host program change loads the kit and its pattern.
  int getNumPrograms() override;
  int getCurrentProgram() override { return currentProgram_; }
  void setCurrentProgram(int program) override;
  const juce::String getProgramName(int program) override;
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
  // Load report of the last state / program load: saved cables dropped (an end on a removed port such as
  // CLOCK:FILL IN or MOD:LANE A, or an unknown id), those on a removed port, and a one-line text (empty if none).
  int droppedCables() const { return droppedCables_; }
  int droppedRemovedCables() const { return droppedRemoved_; }
  const juce::String& loadReport() const { return loadReport_; }
  std::pair<int, int> cable(int i) const { return {uiCables_[static_cast<size_t>(i)].first, uiCables_[static_cast<size_t>(i)].second}; }
  double cvAmt(int port) const { return uiCvAmt_[static_cast<size_t>(port)]; }
  void setCvAmt(int port, double amt);
  void pushPad(int voice);     // trigger pad (any mode, like MIDI)
  void requestRun(bool run);
  void requestRestart();
  void requestIdeal();
  void initPatch();
  void loadProgram(int program);  // INIT, then the bank document through patchFromJson; keeps CLOCK:SOURCE
  void stepProgram(int delta);    // header ◀ ▶: the next / previous program, wrapping INIT ↔ the last kit

  // Full-state snapshots (message thread): the patch document of patchToJson() plus the program number.
  struct Snapshot {
    juce::String json;
    int program = 0;
    bool empty() const { return json.isEmpty(); }
    bool sameAs(const Snapshot& o) const { return program == o.program && json == o.json; }
  };
  Snapshot captureState() const;
  // keepSource: an A/B recall keeps the current CLOCK:SOURCE; undo/redo (the instance's history) restores it.
  void restoreState(const Snapshot& s, bool keepSource = false);
  void resetParams(bool keepSource);  // every param to its default; SRC too unless keepSource
  juce::String stateJson() const { return captureState().json; }

  // A/B compare: two full-state slots. selectAB stores the live state in the active slot and loads the other one (a
  // slot never used starts as a copy of the live state). copyAB copies one slot onto the other; when the target is the
  // active slot the copy is loaded. Both are undoable.
  int abSlot() const { return abSlot_; }
  bool abFilled(int slot) const { return slot == abSlot_ || !ab_[static_cast<size_t>(slot & 1)].empty(); }
  void selectAB(int slot);
  void copyAB(int from, int to);

  // Undo / redo: bounded stacks of full-state snapshots. An edit is bracketed by beginUndoStep() (the state before)
  // and settleUndoStep(), which records the step only if the state really changed (a click that changes nothing
  // leaves the stacks alone and keeps redo). Knob gestures, step edits, matrix / cable edits and program loads are
  // bracketed by the editor; setCurrentProgram brackets itself.
  static constexpr int kUndoLevels = 64;
  void beginUndoStep();
  bool settleUndoStep();
  bool undoPending() const { return !undoPre_.empty(); }
  bool undo();
  bool redo();
  int undoDepth() const { return static_cast<int>(undo_.size()); }
  int redoDepth() const { return static_cast<int>(redo_.size()); }

  // Unit serial (GLOBAL ▸ RE-ROLL UNIT): the per-voice tolerance seed. Applied on the audio thread; saved in the
  // plugin state ("unit") and restored by setStateInformation.
  void rerollUnit();
  void requestSerial(std::uint32_t serial);
  std::uint32_t unitSerial() const { return unitSerial_.load(std::memory_order_relaxed); }

  // MIDI velocity curve (GLOBAL ▸ VELOCITY CURVE: LINEAR / SOFT / HARD / FIXED), x and result in 0..1.
  static double velCurve(int mode, double x);

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
  void timerCallback() override;
  void applyParams(bool now);
  void handleMidi(const juce::MidiMessage& msg);
  void pickUpEdits();
  juce::var patchToJson() const;
  void patchFromJson(const juce::var& v, bool applySource = true);  // false: CLOCK:SOURCE in the doc is ignored

  std::unique_ptr<shogun::Engine> engine_;
  std::vector<juce::RangedAudioParameter*> params_;
  std::vector<std::atomic<float>*> raw_;
  std::vector<float> last_;
  int currentProgram_ = 0;
  std::atomic<bool> snapParams_{false};  // next block sets parameters without smoothing (program change)
  std::vector<int> ccToParam_[128];

  double sr_ = 48000.0;
  int block_ = 512;
  int osNow_ = 2;
  // Set by processBlock when GLOBAL:OS / OFFLINE asks for a different factor; the message-thread timer
  // re-prepares. The audio thread never posts a message itself (see timerCallback).
  std::atomic<bool> osChangeWanted_{false};
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
  int droppedCables_ = 0, droppedRemoved_ = 0;
  juce::String loadReport_;
  std::array<double, shogun::kPorts> uiCvAmt_{};
  std::array<std::uint8_t, shogun::kPorts> uiLaw_{};

  // UI → audio one-shot requests
  std::atomic<std::uint32_t> padMask_{0};
  std::atomic<int> runReq_{-1};
  std::atomic<bool> restartReq_{false}, idealReq_{false};
  std::atomic<std::uint32_t> serialReq_{0}, unitSerial_{0x5A31C0DEu};

  // A/B and undo (message thread only)
  int abSlot_ = 0;
  std::array<Snapshot, 2> ab_{};
  std::deque<Snapshot> undo_, redo_;
  Snapshot undoPre_;
  void pushBounded(std::deque<Snapshot>& d, Snapshot s);

  juce::AudioBuffer<float> scratch_;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShogunAudioProcessor)
};
