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
  // Test hooks (probe): press the nth control (0 = first) on the current tab whose binding is `bind`, as a click at its
  // centre (right button / shift optional), and read the text a bound op draws now.
  bool pressBind(const char* bind, bool right = false, bool shift = false, int nth = 0);
  juce::String boundText(const char* bind);
  // Click / wheel at a panel point as the mouse would (probe), and a DEPTH drag of dx panel units on a matrix row.
  bool clickAt(juce::Point<float> p, bool right = false);
  // Probe: the rect of the nth op on the current tab bound to `bind`, and the binding the hit test finds at a point
  // ("" when none).
  bool bindRect(const char* bind, juce::Rectangle<float>& r, int nth = 0) const;
  juce::String bindAt(juce::Point<float> p) const;
  void wheelAt(juce::Point<float> p, float deltaY);
  void dragMatrixDepth(juce::Point<float> p, float dx);
  // MOD tab matrix view: entries in the list (used rows + "+ add"), the first one in view, and scrolling by rows.
  int matrixEntryCount() const;
  int matrixTop() const;
  void scrollMatrix(int rows);
  int armedSource() const { return armedSrc_; }

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
  void click(int bi, juce::ModifierKeys mods, juce::Point<float> p);
  void matrixClick(const LayoutOp& o, juce::Point<float> p, juce::ModifierKeys mods);
  void showProgramMenu();
  bool assignTo(int pid);
  int uiScalePercent() const;
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
  int matrixTop_ = 0;  // first matrix list entry in view (scroll)
  double matrixDragDepth_ = 0.0;
  float meterL_ = 0, meterR_ = 0;
  float vPeak_[shogun::kVoices] = {};
  std::array<shogun::Step, shogun::kMaxSteps> clipboard_{};
  int clipLen_ = 0;
  int armedSrc_ = 0;   // ASSIGN: the armed mod source (mod::SRC_*), 0 = none
  int menuOpen_ = 0;   // async popup menus still open (their callbacks settle the undo step)
  juce::String lastText_;  // the text paintOp drew last (boundText)
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
