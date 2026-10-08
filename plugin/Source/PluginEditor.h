#pragma once
// SHOGUN editor (spec v2.2 §11): 1200 × 672 window, rack ears with screws, the 8-tab panel drawn from the op table
// exported from the spec mockups (PanelLayout.inc, design units scaled by k = 1136/1200 between the ears).

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

struct LayoutOp {
  int kind, tab, flags;
  float x, y, w, h, r, z, v;
  std::uint32_t fill, stroke;
  float sw, opacity;
  const char* text;
  const char* text2;
  const char* bind;
  int steps, ticks;
};

class ShogunPanel : public juce::Component, private juce::Timer {
 public:
  static constexpr int kW = 1200, kH = 672, kEar = 30;
  explicit ShogunPanel(ShogunAudioProcessor& p);
  ~ShogunPanel() override;

  void paint(juce::Graphics& g) override;
  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;
  void mouseDoubleClick(const juce::MouseEvent& e) override;
  void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

  void setTab(int t);
  int tab() const { return tab_; }
  void setSelectedVoice(int v) { selVoice_ = v; repaint(); }
  static juce::AffineTransform panelTransform();
  // Panel-unit position of a jack (probe / tests); returns false when the id is not on the ROUTE tab.
  bool jackPosition(int port, juce::Point<float>& out) const;
  static int opCount();

 private:
  struct Bound {
    int op;     // index in the op table
    int kind;   // bind kind (enum in the .cpp)
    int a, b;   // parsed arguments (param id, step, voice, …)
  };
  void timerCallback() override;
  void buildBindings();
  void paintStatic(juce::Graphics& g, int tab);
  void paintOp(juce::Graphics& g, const LayoutOp& o, const Bound* b);
  void paintCables(juce::Graphics& g);
  void paintMatrix(juce::Graphics& g, const LayoutOp& o);
  int findBound(juce::Point<float> p) const;  // index into bounds_, or −1
  void click(int bi, const juce::MouseEvent& e, juce::Point<float> p);
  void matrixClick(const LayoutOp& o, juce::Point<float> p, const juce::MouseEvent& e);
  int selWaveVoice() const;
  int stepParamU(const Bound& b, float& u) const;
  void setStepField(const Bound& b, float u);
  shogun::Step& selStep();
  int trackOf(const Bound& b) const;

  ShogunAudioProcessor& proc_;
  int tab_ = 0;
  int selVoice_ = 0;
  int selStep_ = 0;
  int page_ = 0;
  std::vector<Bound> bounds_;
  std::vector<int> tabBounds_[8];
  std::vector<int> tabStaticOps_[8];
  int jackOp_[shogun::kPorts];
  juce::Image cache_[8];
  float cacheScale_[8] = {};

  // drag state
  int dragBound_ = -1;
  float dragStartU_ = 0.0f;
  juce::Point<float> dragStart_;
  int cableFrom_ = -1;
  juce::Point<float> cableEnd_;
  int matrixDragRow_ = -1;
  double matrixDragDepth_ = 0.0;
  float meterL_ = 0, meterR_ = 0;
  float vPeak_[shogun::kVoices] = {};
  std::array<shogun::Step, shogun::kMaxSteps> clipboard_{};
  int clipLen_ = 0;
};

class ShogunAudioProcessorEditor : public juce::AudioProcessorEditor {
 public:
  explicit ShogunAudioProcessorEditor(ShogunAudioProcessor& p);
  void resized() override;
  void paint(juce::Graphics&) override {}
  ShogunPanel& panel() { return panel_; }

 private:
  ShogunPanel panel_;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShogunAudioProcessorEditor)
};
