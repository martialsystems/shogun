#pragma once
// SHOGUN editor (spec v2.2 §11): 1200 × 672 window, rack ears with screws, the 8-tab panel drawn from the op table
// exported from the spec mockups (PanelLayout.inc, design units scaled by k = 1136/1200 between the ears).

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "PluginProcessor.h"

// Item K: popup menus, dropdowns and lists use a plain readable face (the LCD face stays on the hardware displays).
class ShogunLookAndFeel : public juce::LookAndFeel_V4 {
 public:
  juce::Font getPopupMenuFont() override;
};

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

// Item J: the ⌕ program search, a small popup: a text box filters the factory programs by name (or number); Enter
// or a click loads one.
class ProgramSearch : public juce::Component, private juce::ListBoxModel, private juce::TextEditor::Listener {
 public:
  explicit ProgramSearch(ShogunAudioProcessor& p);
  static std::vector<int> filter(const juce::String& query);  // matching program indices, bank order
  void setQuery(const juce::String& q);                        // (probe) as if typed
  bool pressReturn();                                          // (probe) Enter: loads the selected / first match
  int matches() const { return static_cast<int>(rows_.size()); }
  void resized() override;

 private:
  int getNumRows() override { return static_cast<int>(rows_.size()); }
  void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
  void listBoxItemClicked(int row, const juce::MouseEvent&) override { load(row); }
  void returnKeyPressed(int) override { pressReturn(); }
  void textEditorTextChanged(juce::TextEditor&) override { setQuery(box_.getText()); }
  void textEditorReturnKeyPressed(juce::TextEditor&) override { pressReturn(); }
  void load(int row);
  ShogunAudioProcessor& proc_;
  juce::TextEditor box_;
  juce::ListBox list_;
  std::vector<int> rows_;
};

class ShogunPanel : public juce::Component, public juce::TooltipClient, private juce::Timer {
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
  // Tooltip of the control at a panel point ("" when none); getTooltip() asks it for the mouse position.
  juce::String tooltipAt(juce::Point<float> p) const;
  // Repaint-only-what-changed (probe / bench): run the timer's work once and return the dirty rects (component px) it
  // repainted; an empty list when nothing on the tab changed. refreshDirty() is the same, used after edits.
  juce::RectangleList<int> tickForTest();
  juce::RectangleList<int> refreshDirty();
  // Press feedback (probe): mouse down / up on a panel point as mouseDown / mouseUp would, and a test clock (ms, -1 =
  // the real one) so the 120 ms release flash can be stepped.
  void pressAt(juce::Point<float> p, juce::ModifierKeys mods = {});
  void releaseAt(juce::Point<float> p);
  void setTestClockMs(double ms) { testClockMs_ = ms; }
  bool flashActive() const { return !flashes_.empty(); }
  bool timerOn() const { return isTimerRunning(); }
  // item K: help text hover zoom (and probe hooks)
  void mouseMove(const juce::MouseEvent& e) override;
  void mouseExit(const juce::MouseEvent& e) override;
  void hoverAt(juce::Point<float> p);
  int zoomOp() const { return zoomOp_; }
  void setLegacyTextForTest(bool on) {  // probe: draw help text at its old laid-out size (the 'before' shots)
    legacyText_ = on;
    for (auto& c : cache_) c = juce::Image();
  }
  juce::Rectangle<float> helpRect(int op, bool asLaidOut = false) const;
  std::vector<std::pair<int, juce::Rectangle<float>>> helpAreas() const;
  juce::Rectangle<float> zoomRect(int op) const;
  juce::Rectangle<int> zoomDirtyRect(int op) const;
  int helpOpAt(juce::Point<float> p) const;
  std::vector<juce::String> helpAudit(int& lines, float& minPt) const;
  juce::File exportPatternMidi();  // the current pattern as a .mid in the temp folder (drag-out, probe)
  // Probe: record menus instead of showing them; the last one's items / ticks; pick an item by its text; answer the
  // pending "Type …" prompt.
  struct MenuCapture {
    juce::StringArray items;
    std::vector<int> ids;
    std::vector<bool> ticked;
    std::function<void(int)> cb;
  };
  void setCaptureMenus(bool on) { captureMenus_ = on; }
  const MenuCapture& lastMenu() const { return lastMenu_; }
  int menusOpened() const { return menusOpened_; }
  bool pickMenuItem(const juce::String& text);
  bool typeForTest(double v);
  std::vector<juce::String> clickableBinds() const;  // every clickable bound on this tab (bind, "|K" = knob)
  void flashBind(int kind);  // item M flash on the first bound of a kind on this tab (keyboard undo / redo / ▶)
  static int kindUndo(), kindRedo(), kindRun();
  void visibilityChanged() override;
  void parentHierarchyChanged() override;
  juce::String getTooltip() override;
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
  int findBound(juce::Point<float> p) const;
  static int muteParam(int v);  // <V>:MUTE param id
  bool voiceMuted(int v) const;  // index into bounds_, or −1
  void click(int bi, juce::ModifierKeys mods, juce::Point<float> p);
  void matrixClick(const LayoutOp& o, juce::Point<float> p, juce::ModifierKeys mods);
  void showProgramMenu();
  void showSearch();
  // List controls (rule 3): right-click = the whole list as a menu (current ticked), left = next, shift-left = back;
  // long lists (10+ items) open the menu on left-click too. TEMPO / A4 / LEN readouts open their value menus.
  struct ListCtl {
    juce::StringArray items;
    int cur = -1;
    std::function<void(int)> set;
    bool longList = false;
  };
  bool listFor(const Bound& b, int pid, const LayoutOp& o, juce::Point<float> p, ListCtl& L);
  bool handleListClick(int bi, int pid, juce::ModifierKeys mods, juce::Point<float> p);
  void showList(const ListCtl& L);
  void showTempoMenu();
  void showA4Menu();
  void showValueMenu(int pid);
  void showLenMenu(int track);
  void typeValue(const juce::String& title, const juce::String& range, std::function<void(double)> apply);
  void openMenu(juce::PopupMenu m, std::function<void(int)> cb);
  std::function<void(double)> pendingType_;
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
  bool captureMenus_ = false;
  int hoverOp_ = -1, zoomOp_ = -1;
  bool legacyText_ = false;
  double hoverSince_ = 0.0;
  void paintZoom(juce::Graphics& g);
  std::unique_ptr<juce::BubbleMessageComponent> dragBubble_;
  void dragHint(const Bound& b);
  MenuCapture lastMenu_;
  int menusOpened_ = 0;
  int menuOpen_ = 0;   // async popup menus still open (their callbacks settle the undo step)
  juce::String lastText_;  // the text paintOp drew last (boundText)
  // Dirty tracking: paintOp in resolve mode (sig_ set) hashes what it would draw and where, without drawing.
  struct Sig {
    std::uint64_t h = 1469598103934665603ull;
    juce::Rectangle<float> r;  // panel units
    bool full = false;         // change repaints the whole panel (cables)
  };
  Sig* sig_ = nullptr;
  std::vector<std::uint64_t> sigH_;
  std::vector<juce::Rectangle<int>> sigR_;
  std::vector<char> sigOk_;
  int sigTab_ = -1;
  juce::Image sigImg_{juce::Image::ARGB, 1, 1, true};
  juce::RectangleList<int> collectDirty();
  juce::Rectangle<float> opExtent(const LayoutOp& o, float x, float y, float w, float h, const juce::String& t) const;
  // Press feedback (item M): the bound held down, and release flashes (bound, start ms) decaying over 120 ms.
  int pressedBi_ = -1;
  std::vector<std::pair<int, double>> flashes_;
  double testClockMs_ = -1.0;
  double nowMs() const;
  int pressLevel(int bi) const;  // -1 pressed, 1..8 flash, 0 none
  void updateTimer();
};

class ShogunAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::KeyListener {
 public:
  explicit ShogunAudioProcessorEditor(ShogunAudioProcessor& p);
  ~ShogunAudioProcessorEditor() override;
  void resized() override;
  void paint(juce::Graphics&) override {}
  ShogunPanel& panel() { return panel_; }
  // Item Q: after the host window is moved, collapsed / restored or the editor re-parented, the editor goes back to
  // 0,0 in its host view and re-runs the layout path the UI-scale keys use (recursion-guarded).
  void moved() override;
  void parentSizeChanged() override;
  void visibilityChanged() override;
  void parentHierarchyChanged() override;
  void reanchor();
  // Keys (items G / N / P), heard through a listener on the top-level window so the editor never needs to grab
  // keyboard focus: Cmd/Ctrl-Z undo, Cmd/Ctrl-Shift-Z or -Y redo, space = SHOGUN's ▶ unless the host's transport is
  // rolling. Everything else returns false and goes on to the host. Public for the probe.
  bool handleKey(const juce::KeyPress& key);

 private:
  using juce::Component::keyPressed;  // the KeyListener overload below does not hide Component's (strict)
  bool keyPressed(const juce::KeyPress& key, juce::Component*) override { return handleKey(key); }
  juce::Component* keyTop_ = nullptr;
  bool anchoring_ = false;
  ShogunLookAndFeel lnf_;  // before the panel: outlives it
  ShogunPanel panel_;
  juce::TooltipWindow tips_{this, 500};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShogunAudioProcessorEditor)
};
