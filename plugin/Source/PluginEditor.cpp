#include "PluginEditor.h"

#include <jidai/CableStandard.h>

#include "factory.h"

#include <cmath>
#include <cstring>

using namespace shogun;

namespace {

const LayoutOp kOps[] = {
#include "PanelLayout.inc"
};
constexpr int kOpCount = static_cast<int>(sizeof kOps / sizeof kOps[0]);

enum OpKind { TEXT, RTEXT, RULE, BOX, KNOB, LED, KEY, LCD, TOGGLE, JACK, RECT, CIRCLE, LINE, PATH };
enum Flag { kAnchor = 3, kOn = 4, kOut = 8, kRound = 16, kBold = 32 };

const juce::Colour INK(0xFFF2F1EA), DIM(0xFF9A9A90), GRN(0xFF4AA862), RED(0xFFE0402E), AMB(0xFFF0B030);
const juce::Colour LCDGRN(0xFF7EE08C), METERGRN(0xFF37C25A);

// Bind kinds.
enum B {
  B_NONE, B_PARAM, B_DISP, B_CHOICE, B_SEL, B_ACT, B_STEP, B_PLAYLED, B_STEPNUM, B_SK, B_GRID, B_GRIDSEL, B_LEN,
  B_TAB, B_TABTEXT, B_RUN, B_RST, B_RUNLED, B_POS, B_KIT, B_PATTERN, B_CPU, B_CPULED, B_CPUBAR, B_CLIPLED, B_METER,
  B_VMETER, B_FADER, B_BUS, B_GR, B_CVAMT, B_TSCALE, B_TSWING, B_TSWINGTXT, B_TSHIFT, B_TSHIFTTXT, B_LFOMODE,
  B_LFORATE, B_LFOSCOPE, B_MATRIX, B_CABLES, B_JACK, B_TITLE, B_STEPINFO, B_TRACKINFO, B_TRACKSCALE, B_LOCKINFO,
  B_TRK, B_PAGE, B_COPY, B_PASTE, B_CLEAR, B_RANDOM, B_SHIFTL, B_SHIFTR, B_CLEARLOCKS, B_IDEAL, B_INITPATCH, B_PANIC,
  B_REROLL, B_SERIAL, B_LATENCY, B_RATE, B_OSBADGE, B_SP, B_WP, B_OFF, B_PROG, B_BROWSE, B_AB, B_UNDO, B_REDO, B_SRC,
  B_ASSIGN, B_UISCALE, B_PLAYHEAD, B_STATIC
};

int voiceIndex(const juce::String& s) {
  for (int v = 0; v < kVoices; ++v)
    if (s == kVoiceNames[v]) return v;
  return -1;
}

juce::String u8(const char* s) { return juce::String::fromUTF8(s); }

juce::Font font(float z, bool bold, bool mono = false, float ls = 0.0f) {
#if JUCE_MAC
  const char* sans = "Helvetica Neue";
#else
  const char* sans = "DejaVu Sans";
#endif
  juce::Font f(juce::FontOptions().withName(mono ? "DejaVu Sans Mono" : sans).withPointHeight(z).withStyle(
      bold ? "Bold" : "Regular"));
  if (!shogun::dsp::exactEq(ls, 0.0f) && z > 0.0f) f.setExtraKerningFactor(ls / z);
  return f;
}

void text(juce::Graphics& g, float x, float y, const juce::String& t, float z, int anchor, juce::Colour c, bool bold,
          float ls = 0.4f, bool mono = false) {
  if (t.isEmpty()) return;
  g.setColour(c);
  const juce::Font f = font(z, bold, mono, ls);
  juce::GlyphArrangement ga;
  ga.addLineOfText(f, t, 0.0f, 0.0f);
  const float w = ga.getBoundingBox(0, -1, true).getRight();
  const float x0 = anchor == 0 ? x : (anchor == 2 ? x - w : x - 0.5f * w);
  ga.draw(g, juce::AffineTransform::translation(x0, y));
}

juce::ColourGradient radial(juce::Rectangle<float> b, float cx, float cy, float r,
                            std::initializer_list<std::pair<float, std::uint32_t>> stops) {
  const juce::Point<float> c(b.getX() + cx * b.getWidth(), b.getY() + cy * b.getHeight());
  const float rad = r * b.getWidth();
  auto it = stops.begin();
  juce::ColourGradient grad(juce::Colour(it->second), c, juce::Colour((stops.end() - 1)->second),
                            c.translated(rad, 0.0f), true);
  for (const auto& s : stops)
    if (s.first > 0.0f && s.first < 1.0f) grad.addColour(static_cast<double>(s.first), juce::Colour(s.second));
  return grad;
}

// Fill codes 1–7 are the mockup gradients (kb, kc, js, jn, pl, grain, ear).
juce::FillType fillFor(std::uint32_t code, juce::Rectangle<float> b) {
  switch (code) {
    case 1: return radial(b, .4f, .3f, .9f, {{0.f, 0xFF3A3A3A}, {.6f, 0xFF151515}, {1.f, 0xFF080808}});
    case 2: return radial(b, .38f, .3f, .9f, {{0.f, 0xFFD9D8D2}, {.5f, 0xFF8D8C86}, {1.f, 0xFF3B3A37}});
    case 3: return radial(b, .4f, .3f, .9f, {{0.f, 0xFFE2E2DC}, {.55f, 0xFF8F8F8A}, {1.f, 0xFF3A3A38}});
    case 4: return radial(b, .4f, .35f, .9f, {{0.f, 0xFF5A5A58}, {1.f, 0xFF161616}});
    case 5: return juce::ColourGradient(juce::Colour(0xFF1B1D1B), b.getX(), b.getY(), juce::Colour(0xFF0F110F),
                                        b.getX(), b.getBottom(), false);
    case 6: return juce::FillType(juce::Colour(0xFF141614));
    case 7: {
      juce::ColourGradient grad(juce::Colour(0xFF2C2E2C), b.getX(), b.getY(), juce::Colour(0xFF262826), b.getRight(),
                                b.getY(), false);
      grad.addColour(.5, juce::Colour(0xFF4A4D4A));
      return grad;
    }
    default: return juce::FillType(juce::Colour(code));
  }
}

void fillShape(juce::Graphics& g, std::uint32_t fill, float opacity, juce::Rectangle<float> b,
               const std::function<void()>& draw) {
  if (fill == 0) return;
  juce::FillType f = fillFor(fill, b);
  f.setOpacity(f.getOpacity() * opacity);
  g.setFillType(f);
  draw();
}

void circle(juce::Graphics& g, float cx, float cy, float r, std::uint32_t fill, std::uint32_t stroke = 0,
            float sw = 1.0f, float opacity = 1.0f) {
  const juce::Rectangle<float> b(cx - r, cy - r, 2 * r, 2 * r);
  fillShape(g, fill, opacity, b, [&] { g.fillEllipse(b); });
  if (stroke) {
    g.setColour(juce::Colour(stroke).withMultipliedAlpha(opacity));
    g.drawEllipse(b, sw);
  }
}

void rect(juce::Graphics& g, float x, float y, float w, float h, float rx, std::uint32_t fill, std::uint32_t stroke = 0,
          float sw = 1.0f, float opacity = 1.0f) {
  const juce::Rectangle<float> b(x, y, w, h);
  fillShape(g, fill, opacity, b, [&] {
    if (rx > 0) g.fillRoundedRectangle(b, rx);
    else g.fillRect(b);
  });
  if (stroke) {
    g.setColour(juce::Colour(stroke).withMultipliedAlpha(opacity));
    if (rx > 0) g.drawRoundedRectangle(b, rx, sw);
    else g.drawRect(b, sw);
  }
}

void line(juce::Graphics& g, float x1, float y1, float x2, float y2, juce::Colour c, float sw, bool round = false) {
  g.setColour(c);
  juce::Path p;
  p.startNewSubPath(x1, y1);
  p.lineTo(x2, y2);
  g.strokePath(p, juce::PathStrokeType(sw, juce::PathStrokeType::mitered,
                                       round ? juce::PathStrokeType::rounded : juce::PathStrokeType::butt));
}

// make_mockups.py primitives, drawn with live values.
void drawKnob(juce::Graphics& g, float cx, float cy, float r, const juce::String& label, float v, float lz, int ticks,
              int steps, float ring, bool dim) {
  const int n = steps ? steps : ticks;
  for (int i = 0; i < n && n > 1; ++i) {
    const float a = juce::degreesToRadians(-135.0f + 270.0f * static_cast<float>(i) / static_cast<float>(n - 1) - 90.0f);
    const float l = (i == 0 || i == n - 1 || i == (n - 1) / 2) ? 3.2f : 2.0f;
    line(g, cx + (r + 2.5f) * std::cos(a), cy + (r + 2.5f) * std::sin(a), cx + (r + 2.5f + l) * std::cos(a),
         cy + (r + 2.5f + l) * std::sin(a), juce::Colour(0xFFBDBCB2), 0.9f);
  }
  if (!shogun::dsp::exactEq(ring, 0.0f)) {  // modulation ring: green arc from the knob value over the summed depth
    const float a0 = -135.0f + 270.0f * v, a1 = juce::jlimit(-135.0f, 135.0f, a0 + 270.0f * ring);
    juce::Path p;
    p.addCentredArc(cx, cy, r + 1.2f, r + 1.2f, 0.0f, juce::degreesToRadians(a0), juce::degreesToRadians(a1), true);
    g.setColour(GRN);
    g.strokePath(p, juce::PathStrokeType(2.4f));
  }
  circle(g, cx, cy + 1.5f, r, 0xFF000000, 0, 1, .55f);
  circle(g, cx, cy, r, 1, 0xFF050505, 1.0f);
  circle(g, cx, cy, r * 0.68f, 2, 0xFF2B2B2B, 0.6f);
  const float a = juce::degreesToRadians(-135.0f + 270.0f * v - 90.0f);
  line(g, cx + r * 0.15f * std::cos(a), cy + r * 0.15f * std::sin(a), cx + r * 0.92f * std::cos(a),
       cy + r * 0.92f * std::sin(a), juce::Colour(0xFFF5F4EE), 2.0f, true);
  if (dim) circle(g, cx, cy, r + 1, 0xA0121412);
  if (label.isNotEmpty()) text(g, cx, cy + r + 11, label, lz, 1, dim ? DIM : INK, true);
}

void drawLed(juce::Graphics& g, float cx, float cy, float r, bool on, std::uint32_t c) {
  circle(g, cx, cy, r + 1.6f, 0xFF050505, 0xFF2A2C2A, 0.8f);
  const std::uint32_t off = (c & 0xFFFFFF) == 0xE0402E ? 0xFF3A1612u : ((c & 0xFFFFFF) == 0x4AA862 ? 0xFF173A20u : 0xFF3A2A10u);
  circle(g, cx, cy, r, on ? c : off);
  if (on) circle(g, cx, cy, r * 2.6f, c, 0, 1, .18f);
}

void drawKey(juce::Graphics& g, float x, float y, float w, float h, const juce::String& label, std::uint32_t lit,
             float z, std::uint32_t fill) {
  rect(g, x, y, w, h, 3, fill ? fill : 0xFF0A0A0A, 0xFF2F322F, 1.0f);
  rect(g, x + 1.5f, y + 1.5f, w - 3, h * 0.42f, 2, 0xFFFFFFFF, 0, 1, .05f);
  if (lit) rect(g, x + 2, y + 2, w - 4, h - 4, 2, lit, 0, 1, .85f);
  if (label.isNotEmpty()) text(g, x + w / 2, y + h / 2 + z * 0.36f, label, z, 1, lit ? juce::Colour(0xFF0B0C0B) : INK, true);
}

void drawLcd(juce::Graphics& g, float x, float y, float w, float h, const juce::String& t, float z, int anchor,
             std::uint32_t c) {
  rect(g, x, y, w, h, 2, 0xFF071008, 0xFF2C3A2C, 1.0f);
  g.saveState();
  g.reduceClipRegion(juce::Rectangle<float>(x + 1, y + 1, w - 2, h - 2).getSmallestIntegerContainer());
  text(g, anchor == 0 ? x + 6 : x + w / 2, y + h / 2 + z * 0.36f, t, z, anchor == 0 ? 0 : 1, juce::Colour(c), false,
       0.0f, true);
  g.restoreState();
}

void drawToggle(juce::Graphics& g, float cx, float cy, const juce::String& l, const juce::String& r, bool onRight,
                float z) {
  rect(g, cx - 9, cy - 4.5f, 18, 9, 4.5f, 0xFF050505, 0xFF3A3D3A, 1.0f);
  circle(g, cx + (onRight ? 5.0f : -5.0f), cy, 3.6f, 2);
  if (l.isNotEmpty()) text(g, cx - 12, cy + 2.8f, l, z, 2, INK, true);
  if (r.isNotEmpty()) text(g, cx + 12, cy + 2.8f, r, z, 0, INK, true);
}

void drawRtext(juce::Graphics& g, float x, float y, const juce::String& t, float z) {
  const float w = static_cast<float>(t.length()) * z * 0.62f + 6.0f;
  rect(g, x - w / 2, y - z + 0.5f, w, z + 3, 1.5f, 0xFFF2F1EA);
  text(g, x, y, t, z, 1, juce::Colour(0xFF0B0C0B), true);
}

void drawJack(juce::Graphics& g, float cx, float cy, const juce::String& label, bool out, const juce::String& normal,
              float z, juce::Colour role, bool patched) {
  circle(g, cx, cy, 10.5f, 0xFF000000, 0, 1, .5f);
  circle(g, cx, cy, 9.5f, 3, 0xFF2A2A2C, 0.9f);
  // JCS R14: the jack ring carries the role colour on the ROUTE bay.
  g.setColour(role);
  g.drawEllipse(cx - 9.5f, cy - 9.5f, 19.0f, 19.0f, patched ? 2.2f : 1.4f);
  circle(g, cx, cy, 6.4f, 4);
  circle(g, cx, cy, 3.8f, 0xFF030303);
  if (out) drawRtext(g, cx, cy + 21, label, z);
  else text(g, cx, cy + 21, label, z, 1, INK, true);
  if (normal.isNotEmpty()) text(g, cx, cy - 13, u8("\xE2\x96\xB8") + normal, 6.5f, 1, DIM, false);
}

void drawBox(juce::Graphics& g, float x, float y, float w, float h, const juce::String& title, float tz) {
  rect(g, x, y, w, h, 2, 0, 0xFF4AA862, 1.1f);
  if (title.isNotEmpty()) {
    rect(g, x + 6, y - 6, static_cast<float>(title.length()) * tz * 0.66f + 10.0f, 12, 0, 0xFF121412);
    text(g, x + 11, y + 3.5f, title, tz, 0, INK, true, 1.2f);
  }
}


juce::String paramDisplay(int id, float u) {
  char buf[48];
  formatParam(id, static_cast<double>(u), buf, sizeof buf);  // the engine's display law (same text as host automation)
  return u8(buf);
}

juce::Colour roleColour(int port) { return juce::Colour(jidai::jcs::roleArgb(kPortTable[port].role)); }  // JCS R14

}  // namespace

// ================================================================ panel

int ShogunPanel::opCount() { return kOpCount; }

juce::AffineTransform ShogunPanel::panelTransform() {
  const float k = static_cast<float>(kW - 2 * (kEar + 2)) / kW;
  return juce::AffineTransform::scale(k).translated(static_cast<float>(kEar + 2), (kH - kH * k) / 2.0f);
}

ShogunPanel::ShogunPanel(ShogunAudioProcessor& p) : proc_(p) {
  setSize(kW, kH);
  setOpaque(true);
  buildBindings();
  startTimerHz(30);
}

ShogunPanel::~ShogunPanel() { stopTimer(); }

void ShogunPanel::buildBindings() {
  for (auto& j : jackOp_) j = -1;
  struct Pre {
    const char* p;
    int k;
  };
  static const Pre pres[] = {
      {"p", B_PARAM}, {"disp", B_DISP}, {"choice", B_CHOICE}, {"sel", B_SEL}, {"act", B_ACT}, {"step", B_STEP},
      {"playled", B_PLAYLED}, {"stepnum", B_STEPNUM}, {"sk", B_SK}, {"grid", B_GRID}, {"gridsel", B_GRIDSEL},
      {"len", B_LEN}, {"tab", B_TAB}, {"tabtext", B_TABTEXT}, {"run", B_RUN}, {"rst", B_RST}, {"runled", B_RUNLED},
      {"pos", B_POS}, {"kit", B_KIT}, {"pattern", B_PATTERN}, {"cpu", B_CPU}, {"cpuled", B_CPULED},
      {"cpubar", B_CPUBAR}, {"clipled", B_CLIPLED}, {"meter", B_METER}, {"vmeter", B_VMETER}, {"fader", B_FADER},
      {"bus", B_BUS}, {"gr", B_GR}, {"cvamt", B_CVAMT}, {"tscale", B_TSCALE}, {"tswing", B_TSWING},
      {"tswingtxt", B_TSWINGTXT}, {"tshift", B_TSHIFT}, {"tshifttxt", B_TSHIFTTXT}, {"lfomode", B_LFOMODE},
      {"lforate", B_LFORATE}, {"lfoscope", B_LFOSCOPE}, {"matrix", B_MATRIX}, {"cables", B_CABLES}, {"jack", B_JACK},
      {"title", B_TITLE}, {"stepinfo", B_STEPINFO}, {"trackinfo", B_TRACKINFO}, {"trackscale", B_TRACKSCALE},
      {"lockinfo", B_LOCKINFO}, {"trk", B_TRK}, {"page", B_PAGE}, {"copy", B_COPY}, {"paste", B_PASTE},
      {"clear", B_CLEAR}, {"random", B_RANDOM}, {"shiftl", B_SHIFTL}, {"shiftr", B_SHIFTR},
      {"clearlocks", B_CLEARLOCKS}, {"ideal", B_IDEAL}, {"initpatch", B_INITPATCH}, {"panic", B_PANIC},
      {"reroll", B_REROLL}, {"serial", B_SERIAL}, {"latency", B_LATENCY}, {"rate", B_RATE}, {"osbadge", B_OSBADGE},
      {"sp", B_SP}, {"wp", B_WP}, {"off", B_OFF}, {"prog", B_PROG}, {"browse", B_BROWSE}, {"ab", B_AB},
      {"undo", B_UNDO}, {"redo", B_REDO}, {"src", B_SRC}, {"assign", B_ASSIGN}, {"uiscale", B_UISCALE}, {"playhead", B_PLAYHEAD},
  };
  for (int i = 0; i < kOpCount; ++i) {
    const LayoutOp& o = kOps[i];
    const juce::String bind = u8(o.bind);
    if (bind.isEmpty()) {
      tabStaticOps_[o.tab].push_back(i);
      continue;
    }
    const juce::String pre = bind.upToFirstOccurrenceOf(":", false, false);
    const juce::String rest = bind.fromFirstOccurrenceOf(":", false, false);
    Bound b{i, B_STATIC, -1, -1};
    for (const auto& p : pres)
      if (pre == p.p) b.kind = p.k;
    switch (b.kind) {
      case B_PARAM:
      case B_DISP: b.a = findParam(rest.toRawUTF8()); break;
      case B_CHOICE:
        b.a = findParam(rest.upToLastOccurrenceOf(":", false, false).toRawUTF8());
        b.b = rest.fromLastOccurrenceOf(":", false, false).getIntValue();
        break;
      case B_SEL:
      case B_ACT:
      case B_VMETER:
      case B_FADER:
      case B_BUS:
      case B_CVAMT: b.a = voiceIndex(rest); break;
      case B_GR: b.a = rest[0] - 'A'; break;
      case B_GRID:
        b.a = rest.upToFirstOccurrenceOf(":", false, false).getIntValue();
        b.b = rest.fromFirstOccurrenceOf(":", false, false).getIntValue();
        break;
      case B_LFOMODE:
        b.a = rest.upToFirstOccurrenceOf(":", false, false).getIntValue();
        b.b = rest.fromFirstOccurrenceOf(":", false, false).getIntValue();
        break;
      case B_SK: {
        static const char* const f[] = {"acc", "flam", "ratchet", "prob", "micro", "bend", "note", "tie"};
        for (int k = 0; k < 8; ++k)
          if (rest == f[k]) b.a = k;
        break;
      }
      case B_JACK:
        b.a = portFromId(rest.toRawUTF8());  // shared R6 parser (labels keep '/', spaces, digits)
        if (b.a >= 0) jackOp_[b.a] = i;
        break;
      case B_TITLE: b.a = rest == "sel" ? 0 : (rest == "step" ? 1 : 2); break;
      case B_SP:
      case B_WP: b.a = -1; break;
      default: b.a = rest.getIntValue(); break;
    }
    if (b.kind == B_PARAM && b.a < 0) b.kind = B_STATIC;
    tabBounds_[o.tab].push_back(static_cast<int>(bounds_.size()));
    bounds_.push_back(b);
  }
}

bool ShogunPanel::jackPosition(int port, juce::Point<float>& out) const {
  if (port < 0 || port >= kPorts || jackOp_[port] < 0) return false;
  out = {kOps[jackOp_[port]].x, kOps[jackOp_[port]].y};
  return true;
}

void ShogunPanel::setTab(int t) {
  tab_ = juce::jlimit(0, 7, t);
  repaint();
}

int ShogunPanel::selWaveVoice() const { return isWaveVoice(selVoice_) ? selVoice_ : BD1; }

Step& ShogunPanel::selStep() { return proc_.editPattern().tracks[selVoice_].steps[selStep_]; }

void ShogunPanel::timerCallback() {
  auto decay = [](float& m, std::atomic<float>& a) {
    const float v = a.exchange(0.0f, std::memory_order_relaxed);
    m = std::fmax(v, m * 0.82f);
  };
  decay(meterL_, proc_.meters.peakL);
  decay(meterR_, proc_.meters.peakR);
  for (int v = 0; v < kVoices; ++v) decay(vPeak_[v], proc_.meters.voicePeak[static_cast<size_t>(v)]);
  // An open undo step (begun at mouse-down) is settled once the gesture is over; a menu settles it in its callback.
  if (menuOpen_ == 0 && proc_.undoPending() && !juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
    proc_.settleUndoStep();
  repaint();
}

// ---------------------------------------------------------------- painting

void ShogunPanel::paintStatic(juce::Graphics& g, int tab) {
  // Window: background and rack ears with screws (§11, BUSHIDO/RONIN language).
  g.fillAll(juce::Colour(0xFF0A0B0A));
  for (int side = 0; side < 2; ++side) {
    const float x = side == 0 ? 0.0f : static_cast<float>(kW - kEar);
    rect(g, x, 0, kEar, kH, 0, 7, 0xFF000000, 1.0f);
    rect(g, x + kEar / 2.0f - 1, 0, 2, kH, 0, 0xFF000000, 0, 1, .18f);
    for (float yy : {24.0f, kH / 2.0f, kH - 24.0f}) {
      circle(g, x + kEar / 2.0f, yy, 7, 3, 0xFF000000, 1.0f);
      line(g, x + kEar / 2.0f - 4.5f, yy - 4.5f, x + kEar / 2.0f + 4.5f, yy + 4.5f, juce::Colour(0xFF2A2A28), 1.6f);
      line(g, x + kEar / 2.0f - 4.5f, yy + 4.5f, x + kEar / 2.0f + 4.5f, yy - 4.5f, juce::Colour(0xFF2A2A28), 1.6f);
    }
  }
  g.saveState();
  g.addTransform(panelTransform());
  for (int i : tabStaticOps_[tab]) paintOp(g, kOps[i], nullptr);
  g.restoreState();
}

void ShogunPanel::paint(juce::Graphics& g) {
  const float scale = juce::jmax(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
  if (!cache_[tab_].isValid() || !shogun::dsp::exactEq(cacheScale_[tab_], scale)) {
    cache_[tab_] = juce::Image(juce::Image::RGB, juce::roundToInt(kW * scale), juce::roundToInt(kH * scale), true);
    juce::Graphics cg(cache_[tab_]);
    cg.addTransform(juce::AffineTransform::scale(scale));
    paintStatic(cg, tab_);
    cacheScale_[tab_] = scale;
  }
  g.drawImage(cache_[tab_], getLocalBounds().toFloat());
  g.saveState();
  g.addTransform(panelTransform());
  for (int bi : tabBounds_[tab_]) {
    const Bound& b = bounds_[static_cast<size_t>(bi)];
    if (b.kind == B_CABLES) continue;
    paintOp(g, kOps[b.op], &b);
  }
  for (int bi : tabBounds_[tab_])
    if (bounds_[static_cast<size_t>(bi)].kind == B_CABLES) paintCables(g);
  g.restoreState();
}

int ShogunPanel::trackOf(const Bound& b) const { return b.kind == B_GRID || b.kind == B_LEN ? b.a : selVoice_; }

void ShogunPanel::paintOp(juce::Graphics& g, const LayoutOp& o, const Bound* b) {
  juce::String t = u8(o.text);
  const juce::String t2 = u8(o.text2);
  const int anchor = o.flags & kAnchor;
  const bool bold = (o.flags & kBold) != 0;
  float v = o.v;
  float ring = 0.0f;
  bool on = (o.flags & kOn) != 0;
  std::uint32_t fill = o.fill, stroke = o.stroke;
  bool dim = false;
  float x = o.x, y = o.y, w = o.w, h = o.h;
  const int k = b ? b->kind : B_NONE;
  Pattern& pat = proc_.editPattern();
  const int curStep = proc_.meters.step.load() - 1;
  const bool running = proc_.meters.running.load();

  auto paramOf = [&](const char* suffix, int voice) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s:%s", kVoiceNames[voice], suffix);
    return findParam(buf);
  };
  int pid = -1;
  switch (k) {
    case B_PARAM: pid = b->a; break;
    case B_SP: pid = paramOf(o.bind + 3, selVoice_); break;
    case B_WP: pid = paramOf(o.bind + 3, selWaveVoice()); break;
    default: break;
  }
  if (k == B_SP && juce::String(o.bind + 3) == "CV AMT") pid = -2;

  switch (k) {
    case B_PARAM:
    case B_SP:
    case B_WP: {
      if (pid == -2) {  // SELECTED VOICE CV AMT: mirrors the PITCH jack's attenuverter
        v = static_cast<float>(0.5 * (proc_.cvAmt(isDrum(selVoice_) ? drumPort(selVoice_, DJ_PITCH) : synthPort(selVoice_ - LEAD, SJ_NOTE)) + 1.0));
        break;
      }
      if (pid < 0) {
        dim = true;
        break;
      }
      const float u = proc_.paramU(pid);
      const ParamInfo& pi = kParams[pid];
      v = u;
      if (o.kind == KNOB && (pi.kind == ParamKind::Stepped) && pi.steps > 1)
        v = static_cast<float>(stepIndex(static_cast<double>(u), pi.steps)) / static_cast<float>(pi.steps - 1);
      on = stepIndex(static_cast<double>(u), pi.steps > 0 ? pi.steps : 2) > 0;
      if (o.kind == KEY) {
        fill = on ? (t == "M" || t.containsIgnoreCase("MUTE") ? RED.getARGB() : (fill ? fill : AMB.getARGB())) : 0;
        stroke = o.stroke;
      }
      if (o.kind == KNOB)
        for (int r = 0; r < mod::kRows; ++r) {
          const mod::Row& row = proc_.editRows()[r];
          if (row.on && row.src != mod::SRC_NONE && row.dst == pid) ring += static_cast<float>(row.depth);
        }
      if (o.kind == TOGGLE) t = u8(o.text);
      if (o.kind == LCD) t = paramDisplay(pid, u);
      break;
    }
    case B_DISP:
      if (b->a >= 0) {
        juce::String d = paramDisplay(b->a, proc_.paramU(b->a));
        if (b->a == P_CLOCK_TEMPO && o.w < 60) d = d.upToFirstOccurrenceOf(" ", false, false);  // header LCD: digits only
        t = t.contains(": ") ? t.upToFirstOccurrenceOf(": ", true, false) + d : d;
      }
      break;
    case B_CHOICE: {
      const ParamInfo& pi = kParams[b->a];
      const bool sel = stepIndex(static_cast<double>(proc_.paramU(b->a)), pi.steps) == b->b;
      fill = sel ? GRN.getARGB() : 0;
      break;
    }
    case B_SEL: fill = b->a == selVoice_ ? GRN.getARGB() : 0; break;
    case B_ACT: on = b->a >= 0 && vPeak_[b->a] > 1e-3f; break;
    case B_STEP: {
      const int s = page_ * 16 + b->a;
      const Step& st = pat.tracks[selVoice_].steps[s];
      const bool beyond = s >= pat.tracks[selVoice_].len;
      fill = st.on ? (st.acc >= 3 ? AMB.getARGB() : RED.getARGB()) : 0;
      if (running && s == curStep % juce::jmax(1, pat.tracks[selVoice_].len)) fill = INK.getARGB();
      if (s == selStep_ && !st.on && fill == 0) fill = 0xFF3A3D3A;
      dim = beyond;
      break;
    }
    case B_STEPNUM: t = juce::String(page_ * 16 + b->a + 1); break;
    case B_PLAYLED: on = running && (page_ * 16 + b->a) == curStep % juce::jmax(1, pat.tracks[selVoice_].len); break;
    case B_GRID: {
      const Track& tr = pat.tracks[b->a];
      const Step& st = tr.steps[b->b];
      if (b->b >= tr.len) fill = 0xFF060706;
      if (st.on) fill = st.acc >= 3 ? 0xFFF0B030u : (st.acc == 2 ? 0xFF4AA862u : 0xFF2F6B3Fu);
      if (running && b->b == curStep % juce::jmax(1, tr.len)) stroke = 0xFFF2F1EAu;
      if (b->a == selVoice_ && b->b == selStep_) stroke = 0xFFF0B030u;
      break;
    }
    case B_PLAYHEAD: {  // GRID column highlight: follows the step while running, gone when stopped
      if (!running || curStep < 0) return;
      x += (1200.0f - 12.0f - 150.0f) / 32.0f * static_cast<float>(curStep % 32 - 4);  // layout column 5 + n
      break;
    }
    case B_GRIDSEL: {
      const float dy = 26.5f * static_cast<float>(selVoice_ - b->a);
      y += dy;
      break;
    }
    case B_LEN: t = juce::String(pat.tracks[b->a].len); break;
    case B_TAB:
      fill = b->a == tab_ ? 0xFF4AA862u : 0xFF0B0C0Bu;
      stroke = b->a == tab_ ? 0xFF4AA862u : 0xFF343834u;
      break;
    case B_TABTEXT: fill = b->a == tab_ ? 0xFF071008u : 0xFFF2F1EAu; break;
    case B_RUN: fill = running ? GRN.getARGB() : 0; break;
    case B_RUNLED: on = running; break;
    case B_POS: {
      const int gs = proc_.meters.globalStep.load();
      t = juce::String(gs / 16 + 1).paddedLeft('0', 2) + ":" + juce::String(curStep + 1).paddedLeft('0', 2) +
          (running ? u8(" \xE2\x96\xB6") : u8(" \xE2\x96\xA0"));
      break;
    }
    case B_KIT: t = u8(factory::programName(proc_.getCurrentProgram())).toUpperCase(); break;
    case B_AB: fill = proc_.abSlot() == b->a ? AMB.getARGB() : 0; dim = !proc_.abFilled(b->a); break;
    case B_UNDO: dim = proc_.undoDepth() == 0; break;
    case B_REDO: dim = proc_.redoDepth() == 0; break;
    case B_SRC: t = "SRC " + paramDisplay(P_CLOCK_SOURCE, proc_.paramU(P_CLOCK_SOURCE)); break;
    case B_ASSIGN: fill = armedSrc_ == b->a ? AMB.getARGB() : 0; break;
    case B_UISCALE: fill = uiScalePercent() == b->a ? GRN.getARGB() : 0; break;
    case B_PATTERN: t = u8(pat.name); break;
    case B_CPU: t = "CPU " + juce::String(juce::roundToInt(100.0f * proc_.meters.cpu.load())) + "%"; break;
    case B_CPULED: on = proc_.meters.cpu.load() < 0.8f; fill = on ? GRN.getARGB() : RED.getARGB(); on = true; break;
    case B_CPUBAR: w = (b->a == 0 ? 388.0f * juce::jlimit(0.0f, 1.0f, proc_.meters.cpu.load()) : 0.0f); break;
    case B_CLIPLED: on = proc_.meters.clip.load(); break;
    case B_METER: {
      const float pk = b->a == 0 ? meterL_ : meterR_;
      rect(g, x, y, w, h, o.r, fill, stroke, o.sw);
      const float db = pk > 1e-6f ? 20.0f * std::log10(pk) : -120.0f;
      const int lit = juce::jlimit(0, 34, juce::roundToInt((db + 48.0f) / 48.0f * 34.0f));
      for (int i = 0; i < lit; ++i)
        rect(g, 959.5f + static_cast<float>(i) * 5.0f, y + 1.2f, 3.6f, 4.6f, 0, i < 24 ? METERGRN.getARGB() : (i < 30 ? AMB.getARGB() : RED.getARGB()));
      return;
    }
    case B_VMETER: {
      const float pk = b->a >= 0 ? vPeak_[b->a] : 0.0f;
      const float db = pk > 1e-6f ? 20.0f * std::log10(pk) : -120.0f;
      const float lv = juce::jlimit(0.0f, 1.0f, (db + 48.0f) / 48.0f);
      h = 110.0f * lv;
      y = 250.0f + 110.0f * (1.0f - lv);
      break;
    }
    case B_FADER: {
      const int lid = paramOf("LEVEL", b->a);
      const float u = lid >= 0 ? proc_.paramU(lid) : 0.5f;
      y = 250.0f + 110.0f * (1.0f - u) - 5.0f;
      break;
    }
    case B_BUS: {
      const int oid = paramOf("OUTPUT", b->a);
      const int i = oid >= 0 ? stepIndex(static_cast<double>(proc_.paramU(oid)), 14) : 0;
      static const char* const s[] = {"M", "A", "B", "C", "D", "1/2", "3/4", "5/6", "7/8", "9/10", "11/12", "13/14", "15/16", "PR"};
      t = s[i];
      break;
    }
    case B_GR: {
      const float gain = proc_.meters.busGr[static_cast<size_t>(juce::jlimit(0, 3, b->a))].load();
      const float grDb = gain > 0 ? -20.0f * std::log10(gain) : 0.0f;
      w = 70.0f * juce::jlimit(0.0f, 1.0f, grDb / 24.0f);
      x = o.x + 30.0f - w;  // the bar grows leftwards from the right end of the 70-wide track
      break;
    }
    case B_CVAMT:
      v = b->a >= 0 ? static_cast<float>(0.5 * (proc_.cvAmt(isDrum(b->a) ? drumPort(b->a, DJ_PITCH) : synthPort(b->a - LEAD, SJ_NOTE)) + 1.0)) : 0.5f;
      break;
    case B_TSCALE: {
      const int sc = pat.tracks[b->a].scale;
      static const char* const s[] = {"1/32", "1/16", "1/8T", "1/8"};
      t = sc < 0 ? juce::String("GLOBAL") : juce::String(s[sc & 3]);
      break;
    }
    case B_TSWING:
    case B_TSWINGTXT: {
      const double sw = pat.tracks[b->a].swing;
      v = sw < 0 ? 0.0f : static_cast<float>((sw - 0.5) / 0.25);
      if (k == B_TSWINGTXT) t = sw < 0 ? juce::String("GLOBAL") : juce::String(juce::roundToInt(100.0 * sw)) + "%";
      break;
    }
    case B_TSHIFT:
    case B_TSHIFTTXT:
      v = static_cast<float>(pat.tracks[b->a].shift);
      if (k == B_TSHIFTTXT) t = juce::String(juce::roundToInt(30.0f * v)) + " ms";
      break;
    case B_LFOMODE: {
      char buf[32];
      std::snprintf(buf, sizeof buf, "LFO %d:MODE", b->a + 1);
      const int id = findParam(buf);
      fill = id >= 0 && stepIndex(static_cast<double>(proc_.paramU(id)), 3) == b->b ? GRN.getARGB() : 0;
      break;
    }
    case B_LFORATE: {
      char buf[32];
      std::snprintf(buf, sizeof buf, "LFO %d:SYNC", b->a + 1);
      const int sy = findParam(buf);
      std::snprintf(buf, sizeof buf, "LFO %d:%s", b->a + 1, sy >= 0 && stepIndex(static_cast<double>(proc_.paramU(sy)), 2) == 1 ? "DIV" : "RATE");
      const int id = findParam(buf);
      if (id >= 0) t = paramDisplay(id, proc_.paramU(id));
      break;
    }
    case B_LFOSCOPE: {
      // The shape of LFO n (2 cycles) in the mockup's scope box.
      char buf[32];
      std::snprintf(buf, sizeof buf, "LFO %d:SHAPE", b->a + 1);
      const int sid = findParam(buf);
      const int shape = sid >= 0 ? stepIndex(static_cast<double>(proc_.paramU(sid)), 6) : 0;
      const float x0 = o.x, yc = o.y, ww = 260.0f, hh = 24.4f;
      juce::Path p;
      juce::Random rnd(7);
      float hold = 0.0f;
      for (int i = 0; i <= 120; ++i) {
        const float ph = std::fmod(2.0f * static_cast<float>(i) / 120.0f, 1.0f);
        float s = 0.0f;
        switch (shape) {
          case 0: s = std::sin(juce::MathConstants<float>::twoPi * ph); break;
          case 1: s = ph < 0.25f ? 4 * ph : (ph < 0.75f ? 2 - 4 * ph : 4 * ph - 4); break;
          case 2: s = 2 * ph - 1; break;
          case 3: s = 1 - 2 * ph; break;
          case 4: s = ph < 0.5f ? 1.0f : -1.0f; break;
          default:
            if (i % 15 == 0) hold = rnd.nextFloat() * 2 - 1;
            s = hold;
            break;
        }
        const float px = x0 + ww * static_cast<float>(i) / 120.0f, py = yc - hh * s;
        if (i == 0) p.startNewSubPath(px, py);
        else p.lineTo(px, py);
      }
      g.setColour(juce::Colour(stroke));
      g.strokePath(p, juce::PathStrokeType(o.sw));
      return;
    }
    case B_MATRIX: paintMatrix(g, o); return;
    case B_JACK: {
      bool patched = false;
      for (int c = 0; c < proc_.cableCount(); ++c)
        patched = patched || proc_.cable(c).first == b->a || proc_.cable(c).second == b->a;
      drawJack(g, x, y, t, (o.flags & kOut) != 0, t2, o.z, b->a >= 0 ? roleColour(b->a) : DIM, patched);
      return;
    }
    case B_TITLE: {
      const juce::String arg = b->a == 0 ? juce::String(kVoiceNames[selVoice_])
                                         : (b->a == 1 ? juce::String(selStep_ + 1) : juce::String(kVoiceNames[selWaveVoice()]));
      t = t.replace("%s", arg);
      break;
    }
    case B_STEPINFO: t = juce::String(kVoiceNames[selVoice_]) + u8(" \xC2\xB7 STEP ") + juce::String(selStep_ + 1); break;
    case B_TRACKINFO:
      t = juce::String(kVoiceNames[selVoice_]) + u8(" \xC2\xB7 ") + juce::String(pat.tracks[selVoice_].len) + " STEPS";
      break;
    case B_TRACKSCALE: {
      const Track& tr = pat.tracks[selVoice_];
      static const char* const s[] = {"1/32", "1/16", "1/8T", "1/8"};
      t = "LEN " + juce::String(tr.len) + u8(" \xC2\xB7 ") + (tr.scale < 0 ? juce::String("GLOBAL") : juce::String(s[tr.scale & 3]));
      break;
    }
    case B_LOCKINFO: {
      const Step& st = selStep();
      t = "LOCKS " + juce::String(st.nLocks) + u8(" \xC2\xB7 TIE ") + (st.tie ? "on" : "off");
      break;
    }
    case B_SK: {
      const Step& st = selStep();
      switch (b->a) {
        case 0: v = (st.acc - 1) / 2.0f; break;
        case 1: v = st.flam / 16.0f; break;
        case 2: {
          int ri = 0;
          for (int q = 0; q < 6; ++q)
            if (kRatchets[q] == st.ratchet) ri = q;
          v = static_cast<float>(ri) / 5.0f;
          break;
        }
        case 3: v = st.prob; break;
        case 4: v = st.micro + 0.5f; break;
        case 5: v = (st.bend + 12.0f) / 24.0f; break;
        case 6: v = (st.note - 24) / 72.0f; break;
        case 7: fill = st.tie ? AMB.getARGB() : 0; break;
        default: break;
      }
      dim = (b->a == 6 || b->a == 7) ? isDrum(selVoice_) : (b->a == 5 ? !(selVoice_ <= SD || (selVoice_ >= LTC && selVoice_ <= HTC)) : false);
      break;
    }
    case B_PAGE: fill = b->a == page_ ? GRN.getARGB() : 0; break;
    case B_SERIAL: {
      const auto s = juce::String::toHexString(static_cast<juce::int64>(proc_.unitSerial())).toUpperCase().paddedLeft('0', 8);
      t = "SN 0x" + s.substring(0, 4) + "-" + s.substring(4);
      break;
    }
    case B_LATENCY: {
      const int lat = proc_.meters.latency.load();
      const double sr = proc_.meters.sampleRate.load();
      t = juce::String(lat) + u8(" smp \xC2\xB7 ") + juce::String(1000.0 * lat / sr, 2) + " ms @" +
          juce::String(juce::roundToInt(sr / 1000.0)) + "k";
      break;
    }
    case B_RATE: t = "host " + juce::String(juce::roundToInt(proc_.meters.sampleRate.load())) + " Hz native"; break;
    case B_OSBADGE: t = juce::String(proc_.osFactor()) + u8("\xC3\x97 OS \xE2\x9C\x93"); break;
    case B_OFF: dim = true; break;
    default: break;
  }

  lastText_ = t;
  switch (o.kind) {
    case TEXT: text(g, x, y, t, o.z, anchor, juce::Colour(fill), bold, v); break;
    case RTEXT: drawRtext(g, x, y, t, o.z); break;
    case RULE: line(g, x, y, w, h, juce::Colour(stroke), o.sw); break;
    case BOX: drawBox(g, x, y, w, h, t, o.z); break;
    case KNOB: drawKnob(g, x, y, o.r, t, juce::jlimit(0.0f, 1.0f, v), o.z, o.ticks, o.steps, ring, dim); break;
    case LED: drawLed(g, x, y, o.r, on, fill); break;
    case KEY:
      drawKey(g, x, y, w, h, t, fill, o.z, stroke);
      if (dim) rect(g, x, y, w, h, 3, 0x90121412);
      break;
    case LCD: drawLcd(g, x, y, w, h, t, o.z, anchor, fill); break;
    case TOGGLE: drawToggle(g, x, y, t, t2, b ? on : (o.flags & kOn) != 0, o.z); break;
    case JACK: drawJack(g, x, y, t, (o.flags & kOut) != 0, t2, o.z, DIM, false); break;
    case RECT: rect(g, x, y, w, h, o.r, fill, stroke, o.sw, o.opacity); break;
    case CIRCLE: circle(g, x, y, o.r, fill, stroke, o.sw, o.opacity); break;
    case LINE: line(g, x, y, w, h, juce::Colour(stroke).withMultipliedAlpha(o.opacity), o.sw, (o.flags & kRound) != 0); break;
    case PATH: {
      juce::Path p = juce::Drawable::parseSVGPath(t);
      if (fill > 7 || (fill >= 1 && fill <= 7)) {
        juce::FillType f = fillFor(fill, p.getBounds());
        f.setOpacity(o.opacity);
        g.setFillType(f);
        g.fillPath(p);
      }
      if (stroke) {
        g.setColour(juce::Colour(stroke).withMultipliedAlpha(o.opacity));
        g.strokePath(p, juce::PathStrokeType(o.sw, juce::PathStrokeType::curved,
                                             (o.flags & kRound) ? juce::PathStrokeType::rounded : juce::PathStrokeType::butt));
      }
      break;
    }
    default: break;
  }
}

void ShogunPanel::paintCables(juce::Graphics& g) {
  // Jidai rope style: a sagging curve in the source jack's role colour, plug collars at both ends (JCS R14).
  auto drawCable = [&](juce::Point<float> a, juce::Point<float> c, juce::Colour col) {
    juce::Path p;
    const float sag = 30.0f + 0.15f * a.getDistanceFrom(c);
    p.startNewSubPath(a);
    p.cubicTo(a.translated(0, sag), c.translated(0, sag), c);
    g.setColour(juce::Colour(0x80000000));
    g.strokePath(p, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                 juce::AffineTransform::translation(0, 2));
    g.setColour(col.withAlpha(0.92f));
    g.strokePath(p, juce::PathStrokeType(3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    for (auto e : {a, c}) {
      circle(g, e.x, e.y, 6.0f, col.getARGB(), 0xFF000000, 1.0f);
      circle(g, e.x, e.y, 2.6f, 0xFF050505);
    }
  };
  for (int i = 0; i < proc_.cableCount(); ++i) {
    const auto c = proc_.cable(i);
    juce::Point<float> a, e;
    if (jackPosition(c.first, a) && jackPosition(c.second, e)) drawCable(a, e, roleColour(c.first));
  }
  if (cableFrom_ >= 0) {
    juce::Point<float> a;
    if (jackPosition(cableFrom_, a)) drawCable(a, cableEnd_, roleColour(cableFrom_));
  }
}

// Mod matrix (MOD tab), the mockup's row style: # · SOURCE · DESTINATION · DEPTH · VIA · CURVE · ON · ✕. Used rows in
// slot order, then one "+ add" row. SOURCE/DESTINATION/VIA open menus, DEPTH drags horizontally, CURVE cycles,
// ON toggles, ✕ clears the slot. Ten rows show at a time; past ten, the list scrolls (mouse wheel = one row, the ▲ ▼
// keys at the right edge = one page) so all 32 slots and the "+ add" row are reachable.
namespace {
const char* const kSrcLabel[] = {"—", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "ENV", "PENV", "VEL", "ACC", "RND/HIT", "NOTE", "RND", "MOD W", "AT"};
constexpr int kSrcLabelCount = static_cast<int>(sizeof kSrcLabel / sizeof kSrcLabel[0]);
const char* const kCurveLabel[] = {"LIN", "EXP", "LOG", "S-CRV"};
constexpr float kRowY0 = 366.0f, kRowH = 28.0f;
constexpr int kRowsShown = 10;
constexpr int kMatrixEntries = mod::kRows + 1;  // every slot used, or the used ones plus "+ add"
constexpr float kScrollX = 756.0f, kScrollW = 24.0f;

juce::String srcLabel(int src, int voice) {
  juce::String s(kSrcLabel[juce::jlimit(0, kSrcLabelCount - 1, src)]);
  if (mod::perVoiceSource(src) && voice >= 0) s << " " << kVoiceNames[voice];
  return s;
}
juce::String dstLabel(int dst) {
  if (dst < 0) return u8("\xE2\x80\x94");
  return u8(kParams[dst].id).replace(":", u8(" \xC2\xB7 "));
}
// The matrix list: used slots in order, then the first free slot ("+ add"). No cap: the view scrolls through it.
int matrixRows(const mod::Row* rows, int out[kMatrixEntries]) {
  int n = 0, firstFree = -1;
  for (int r = 0; r < mod::kRows; ++r) {
    if (rows[r].src == mod::SRC_NONE) {
      if (firstFree < 0) firstFree = r;
      continue;
    }
    out[n++] = r;
  }
  if (firstFree >= 0) out[n++] = firstFree;
  return n;
}
}  // namespace

int ShogunPanel::matrixEntryCount() const {
  int slots[kMatrixEntries];
  return matrixRows(proc_.editRows(), slots);
}

// The first list entry in view, clamped to the list (rows can vanish under it: ✕, undo, a program load).
int ShogunPanel::matrixTop() const { return juce::jlimit(0, std::max(0, matrixEntryCount() - kRowsShown), matrixTop_); }

void ShogunPanel::scrollMatrix(int rows) {
  const int top = juce::jlimit(0, std::max(0, matrixEntryCount() - kRowsShown), matrixTop() + rows);
  if (top != matrixTop_) {
    matrixTop_ = top;
    repaint();
  }
}

void ShogunPanel::paintMatrix(juce::Graphics& g, const LayoutOp&) {
  int slots[kMatrixEntries];
  const int total = matrixRows(proc_.editRows(), slots);
  const int top = matrixTop();
  const int n = std::min(kRowsShown, total - top);
  if (total > kRowsShown) {  // scroll column: ▲ page up, track + thumb, ▼ page down; header readout "11-20 / 23"
    text(g, 786, 356, juce::String(top + 1) + "-" + juce::String(top + n) + " / " + juce::String(total), 7.5f, 2,
         juce::Colour(0xFF9A9A90), false);
    drawKey(g, kScrollX, kRowY0 + 1, kScrollW, 20, u8("\xE2\x96\xB2"), 0, 7, top > 0 ? 0xFF0A0A0A : 0xFF050505);
    drawKey(g, kScrollX, kRowY0 + (kRowsShown - 1) * kRowH + 1, kScrollW, 20, u8("\xE2\x96\xBC"), 0, 7,
            top + n < total ? 0xFF0A0A0A : 0xFF050505);
    const float t0 = kRowY0 + 26, t1 = kRowY0 + (kRowsShown - 1) * kRowH - 4, th = t1 - t0;
    rect(g, kScrollX + kScrollW / 2 - 3, t0, 6, th, 3, 0xFF050505, 0xFF262826, 1.0f);
    const float ty = t0 + th * static_cast<float>(top) / static_cast<float>(total),
                tl = std::max(10.0f, th * static_cast<float>(n) / static_cast<float>(total));
    rect(g, kScrollX + kScrollW / 2 - 2, std::min(ty, t1 - tl), 4, tl, 2, GRN.getARGB());
  }
  for (int i = 0; i < n; ++i) {
    const int slot = slots[top + i];
    const mod::Row& row = proc_.editRows()[slot];
    const float y = kRowY0 + static_cast<float>(i) * kRowH;
    const bool empty = row.src == mod::SRC_NONE;
    if (i % 2 == 0) rect(g, 14, y - 2, total > kRowsShown ? 736 : 772, 26, 0, 0xFFFFFFFF, 0, 1, .025f);
    text(g, 26, y + 14, juce::String(slot + 1), 8, 1, DIM, false);
    drawLcd(g, 46, y + 2, 110, 18, empty ? juce::String("+ add") : srcLabel(row.src, row.srcVoice), 8.5f, 0,
            empty ? 0xFF4A6A4Au : LCDGRN.getARGB());
    if (empty) continue;
    drawLcd(g, 166, y + 2, 150, 18, dstLabel(row.dst), 8.5f, 0, LCDGRN.getARGB());
    const float bx = 330, bw = 140, c0 = bx + bw / 2, c1 = c0 + static_cast<float>(row.depth) * bw / 2;
    rect(g, bx, y + 6, bw, 10, 0, 0xFF050505, 0xFF262826, 1.0f);
    rect(g, std::fmin(c0, c1), y + 7, std::fabs(c1 - c0), 8, 0, row.depth > 0 ? GRN.getARGB() : AMB.getARGB());
    line(g, c0, y + 4, c0, y + 18, juce::Colour(0xFFBDBCB2), 0.8f);
    text(g, bx + bw + 8, y + 14, (row.depth >= 0 ? "+" : "") + juce::String(juce::roundToInt(100.0 * row.depth)) + " %", 8, 0, INK, true);
    drawLcd(g, 518, y + 2, 60, 18, row.via == mod::SRC_NONE ? u8("\xE2\x80\x94") : srcLabel(row.via, row.viaVoice), 8.5f, 1, LCDGRN.getARGB());
    drawLcd(g, 588, y + 2, 64, 18, kCurveLabel[row.curve & 3], 8.5f, 1, LCDGRN.getARGB());
    drawToggle(g, 678, y + 11, "", "", row.on, 6);
    drawKey(g, 724, y + 1, 22, 20, u8("\xE2\x9C\x95"), 0, 8, 0xFF0A0A0A);
  }
}

void ShogunPanel::matrixClick(const LayoutOp&, juce::Point<float> p, juce::ModifierKeys) {
  int slots[kMatrixEntries];
  const int total = matrixRows(proc_.editRows(), slots);
  const int top = matrixTop();
  const int n = std::min(kRowsShown, total - top);
  const int i = static_cast<int>(std::floor((p.y - (kRowY0 - 2)) / kRowH));
  if (total > kRowsShown && p.x >= kScrollX && p.x < kScrollX + kScrollW) {  // ▲ / ▼ page keys
    if (i == 0) scrollMatrix(-kRowsShown);
    else if (i == kRowsShown - 1) scrollMatrix(kRowsShown);
    return;
  }
  if (i < 0 || i >= n) return;
  const int row = slots[top + i];
  mod::Row& r = proc_.editRows()[row];
  auto commit = [this] { proc_.commitEdits(); repaint(); };
  auto sourceMenu = [](bool perVoice) {
    juce::PopupMenu menu;
    menu.addItem(1000, "(none)");
    for (int s = 1; s < kSrcLabelCount; ++s) {
      if (perVoice && mod::perVoiceSource(s)) {
        juce::PopupMenu sub;
        sub.addItem(s * 100 + 99, "OWN VOICE");
        for (int v = 0; v < kVoices; ++v) sub.addItem(s * 100 + v, kVoiceNames[v]);
        menu.addSubMenu(kSrcLabel[s], sub);
      } else {
        menu.addItem(s * 100 + 99, kSrcLabel[s]);
      }
    }
    return menu;
  };
  if (p.x >= 46 && p.x < 156) {
    ++menuOpen_;
    sourceMenu(true).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [safe = juce::Component::SafePointer<ShogunPanel>(this), row](int res) {
      ShogunPanel* self = safe.getComponent();  // the editor may be gone by the time the menu closes
      if (self == nullptr) return;
      --self->menuOpen_;  // the timer settles the undo step now
      if (res <= 0) return;
      mod::Row& rr = self->proc_.editRows()[row];
      if (res == 1000) {
        rr = mod::Row();
      } else {
        rr.src = res / 100;
        rr.srcVoice = res % 100 == 99 ? -1 : res % 100;
        if (rr.dst < 0) rr.dst = P_BD1_DECAY;
      }
      self->proc_.commitEdits();
      self->repaint();
    });
    return;
  }
  if (r.src == mod::SRC_NONE) return;
  if (p.x >= 166 && p.x < 316) {
    juce::PopupMenu menu, sec;
    juce::String cur;
    for (int k = 0; k < kParamCount; ++k) {
      if (!kParams[k].mod) continue;
      const juce::String id = u8(kParams[k].id);
      const auto pj = jidai::jcs::parseJackId(kParams[k].id);  // shared R6 SECTION:LABEL split
      const juce::String sname = pj ? u8(pj->section.c_str()) : id;
      if (sname != cur) {
        if (cur.isNotEmpty()) menu.addSubMenu(cur, sec);
        sec = juce::PopupMenu();
        cur = sname;
      }
      sec.addItem(k + 1, pj ? u8(pj->label.c_str()) : id, true, r.dst == k);
    }
    if (cur.isNotEmpty()) menu.addSubMenu(cur, sec);
    ++menuOpen_;
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [safe = juce::Component::SafePointer<ShogunPanel>(this), row](int res) {
      ShogunPanel* self = safe.getComponent();  // the editor may be gone by the time the menu closes
      if (self == nullptr) return;
      --self->menuOpen_;  // the timer settles the undo step now
      if (res <= 0) return;
      self->proc_.editRows()[row].dst = res - 1;
      self->proc_.commitEdits();
      self->repaint();
    });
  } else if (p.x >= 320 && p.x < 510) {
    matrixDragRow_ = row;
    matrixDragDepth_ = r.depth;
  } else if (p.x >= 518 && p.x < 578) {
    ++menuOpen_;
    sourceMenu(false).showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [safe = juce::Component::SafePointer<ShogunPanel>(this), row](int res) {
      ShogunPanel* self = safe.getComponent();  // the editor may be gone by the time the menu closes
      if (self == nullptr) return;
      --self->menuOpen_;  // the timer settles the undo step now
      if (res <= 0) return;
      mod::Row& rr = self->proc_.editRows()[row];
      rr.via = res == 1000 ? mod::SRC_NONE : res / 100;
      rr.viaVoice = -1;
      self->proc_.commitEdits();
      self->repaint();
    });
  } else if (p.x >= 588 && p.x < 652) {
    r.curve = (r.curve + 1) % mod::kCurveCount;
    commit();
  } else if (p.x >= 660 && p.x < 700) {
    r.on = !r.on;
    commit();
  } else if (p.x >= 724 && p.x < 746) {
    r = mod::Row();
    commit();
  }
}

// ---------------------------------------------------------------- interaction

int ShogunPanel::findBound(juce::Point<float> p) const {
  const auto& list = tabBounds_[tab_];
  for (auto it = list.rbegin(); it != list.rend(); ++it) {
    const Bound& b = bounds_[static_cast<size_t>(*it)];
    const LayoutOp& o = kOps[b.op];
    bool hit = false;
    switch (o.kind) {
      case KNOB: hit = p.getDistanceFrom({o.x, o.y}) <= o.r + 4; break;
      case JACK: hit = p.getDistanceFrom({o.x, o.y}) <= 11; break;
      case TOGGLE: hit = std::fabs(p.x - o.x) <= 26 && std::fabs(p.y - o.y) <= 7; break;
      case KEY:
      case LCD:
      case RECT: hit = juce::Rectangle<float>(o.x, o.y, o.w, o.h).contains(p); break;
      case TEXT:
        hit = (b.kind == B_TABTEXT) && std::fabs(p.x - o.x) < 28 && p.y > o.y - 14 && p.y < o.y + 6;
        break;
      default: break;
    }
    if (!hit) continue;
    switch (b.kind) {
      case B_PARAM: case B_CHOICE: case B_SEL: case B_STEP: case B_SK: case B_GRID: case B_LEN: case B_TAB:
      case B_TABTEXT: case B_RUN: case B_RST: case B_CVAMT: case B_TSCALE: case B_TSWING: case B_TSHIFT:
      case B_LFOMODE: case B_MATRIX: case B_JACK: case B_TRK: case B_PAGE: case B_COPY: case B_PASTE: case B_CLEAR:
      case B_RANDOM: case B_SHIFTL: case B_SHIFTR: case B_CLEARLOCKS: case B_IDEAL: case B_INITPATCH: case B_PANIC:
      case B_REROLL: case B_SP: case B_WP: case B_CLIPLED: case B_FADER: case B_DISP: case B_KIT: case B_PATTERN:
      case B_OSBADGE: case B_PROG: case B_BROWSE: case B_AB: case B_UNDO: case B_REDO: case B_SRC: case B_ASSIGN:
      case B_UISCALE:
        return *it;
      default: break;
    }
  }
  return -1;
}

void ShogunPanel::mouseDown(const juce::MouseEvent& e) {
  const auto p = e.position.transformedBy(panelTransform().inverted());
  dragBound_ = findBound(p);
  dragStart_ = e.position;
  matrixDragRow_ = -1;
  if (dragBound_ < 0) return;
  click(dragBound_, e.mods, p);
}

static int pidForBound(int kind, const char* bind, int a, int selVoice, int waveVoice) {
  if (kind == B_PARAM || kind == B_DISP) return a;
  if (kind == B_SP || kind == B_WP) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s:%s", kVoiceNames[kind == B_SP ? selVoice : waveVoice], bind + 3);
    return findParam(buf);
  }
  return -1;
}

bool ShogunPanel::pressBind(const char* bind, bool right, bool shift, int nth) {
  for (int bi : tabBounds_[tab_]) {
    const LayoutOp& o = kOps[bounds_[static_cast<size_t>(bi)].op];
    if (std::strcmp(o.bind, bind) != 0 || nth-- > 0) continue;
    juce::ModifierKeys m;
    if (right) m = m.withFlags(juce::ModifierKeys::rightButtonModifier);
    if (shift) m = m.withFlags(juce::ModifierKeys::shiftModifier);
    const juce::Point<float> c = o.kind == KNOB || o.kind == TOGGLE || o.kind == JACK ? juce::Point<float>(o.x, o.y)
                                                                                     : juce::Point<float>(o.x + o.w / 2, o.y + o.h / 2);
    click(bi, m, c);
    if (o.kind == KNOB && dragBound_ == bi) {  // a press without a drag: close the gesture like mouseUp would
      const int pid = pidForBound(bounds_[static_cast<size_t>(bi)].kind, o.bind, bounds_[static_cast<size_t>(bi)].a,
                                  selVoice_, selWaveVoice());
      if (pid >= 0) proc_.param(pid)->endChangeGesture();
    }
    dragBound_ = -1;
    return true;
  }
  return false;
}

bool ShogunPanel::bindRect(const char* bind, juce::Rectangle<float>& r, int nth) const {
  for (int bi : tabBounds_[tab_]) {
    const LayoutOp& o = kOps[bounds_[static_cast<size_t>(bi)].op];
    if (std::strcmp(o.bind, bind) != 0 || nth-- > 0) continue;
    r = {o.x, o.y, o.w, o.h};
    return true;
  }
  return false;
}

juce::String ShogunPanel::bindAt(juce::Point<float> p) const {
  const int bi = findBound(p);
  return bi < 0 ? juce::String() : u8(kOps[bounds_[static_cast<size_t>(bi)].op].bind);
}

bool ShogunPanel::clickAt(juce::Point<float> p, bool right) {
  const int bi = findBound(p);
  if (bi < 0) return false;
  juce::ModifierKeys m;
  if (right) m = m.withFlags(juce::ModifierKeys::rightButtonModifier);
  click(bi, m, p);
  dragBound_ = -1;
  matrixDragRow_ = -1;
  return true;
}

void ShogunPanel::dragMatrixDepth(juce::Point<float> p, float dx) {
  const int bi = findBound(p);
  if (bi < 0 || bounds_[static_cast<size_t>(bi)].kind != B_MATRIX) return;
  matrixDragRow_ = -1;
  click(bi, juce::ModifierKeys(), p);
  if (matrixDragRow_ >= 0) {
    proc_.editRows()[matrixDragRow_].depth = juce::jlimit(-1.0, 1.0, matrixDragDepth_ + static_cast<double>(dx) / (70.0 * 0.9467));
    proc_.commitEdits();
  }
  matrixDragRow_ = -1;
  dragBound_ = -1;
}

juce::String ShogunPanel::boundText(const char* bind) {
  for (int bi : tabBounds_[tab_]) {
    const Bound& b = bounds_[static_cast<size_t>(bi)];
    if (std::strcmp(kOps[b.op].bind, bind) != 0) continue;
    juce::Image img(juce::Image::ARGB, 8, 8, true);
    juce::Graphics g(img);
    lastText_.clear();
    paintOp(g, kOps[b.op], &b);
    return lastText_;
  }
  return {};
}

int ShogunPanel::uiScalePercent() const {
  const auto* ed = getParentComponent();
  return ed != nullptr ? juce::roundToInt(100.0 * ed->getWidth() / kW) : 100;
}

void ShogunPanel::showProgramMenu() {
  juce::PopupMenu m;
  for (int i = 0; i < factory::kPrograms; ++i)
    m.addItem(i + 1, juce::String(i + 1).paddedLeft('0', 3) + " " + u8(factory::programName(i)), true,
              i == proc_.getCurrentProgram());
  juce::Component::SafePointer<ShogunPanel> safe(this);
  ++menuOpen_;
  m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [safe](int r) {
    if (safe == nullptr) return;
    --safe->menuOpen_;
    if (r > 0) safe->proc_.setCurrentProgram(r - 1);  // brackets its own undo step
    safe->repaint();
  });
}

bool ShogunPanel::assignTo(int pid) {
  // ASSIGN: the armed source gets a matrix row onto this knob's parameter (own voice, +50 %), in the first free slot.
  if (armedSrc_ <= 0 || pid < 0 || !kParams[pid].mod) return false;
  mod::Row* rows = proc_.editRows();
  int slot = -1;
  for (int r = 0; r < mod::kRows; ++r) {
    if (rows[r].src == armedSrc_ && rows[r].srcVoice < 0 && rows[r].dst == pid) {
      slot = -2;  // already there
      break;
    }
    if (slot == -1 && (rows[r].src == mod::SRC_NONE || rows[r].dst < 0)) slot = r;
  }
  if (slot >= 0) {
    mod::Row r;
    r.src = armedSrc_;
    r.srcVoice = -1;
    r.dst = pid;
    r.depth = 0.5;
    r.on = true;
    rows[slot] = r;
    proc_.commitEdits();
  }
  armedSrc_ = 0;
  repaint();
  return slot != -1;
}

namespace {
bool editsState(int kind) {
  switch (kind) {
    case B_PARAM: case B_SP: case B_WP: case B_DISP: case B_CHOICE: case B_STEP: case B_GRID: case B_LEN:
    case B_CVAMT: case B_TSCALE: case B_TSWING: case B_TSHIFT: case B_LFOMODE: case B_MATRIX: case B_JACK:
    case B_PASTE: case B_CLEAR: case B_RANDOM: case B_SHIFTL: case B_SHIFTR: case B_CLEARLOCKS: case B_IDEAL:
    case B_FADER: case B_SK: case B_SRC: case B_OSBADGE:
      return true;
    default: return false;
  }
}
}  // namespace

void ShogunPanel::click(int bi, juce::ModifierKeys mods, juce::Point<float> p) {
  const Bound& b = bounds_[static_cast<size_t>(bi)];
  const LayoutOp& o = kOps[b.op];
  Pattern& pat = proc_.editPattern();
  auto commit = [this] { proc_.commitEdits(); repaint(); };
  const int pid = pidForBound(b.kind, o.bind, b.a, selVoice_, selWaveVoice());
  if (editsState(b.kind)) proc_.beginUndoStep();  // settled when the gesture is over (timer) or by the menu callback
  switch (b.kind) {
    case B_PARAM:
    case B_SP:
    case B_WP:
    case B_DISP: {
      if (o.kind == KNOB && armedSrc_ > 0) {  // ASSIGN armed: this knob becomes the row's destination
        assignTo(pid);
        dragBound_ = -1;
        return;
      }
      if (pid < 0) {
        if (b.kind == B_SP) dragStartU_ = static_cast<float>(0.5 * (proc_.cvAmt(isDrum(selVoice_) ? drumPort(selVoice_, DJ_PITCH) : synthPort(selVoice_ - LEAD, SJ_NOTE)) + 1.0));
        return;
      }
      const ParamInfo& pi = kParams[pid];
      if (o.kind == KNOB) {
        dragStartU_ = proc_.paramU(pid);
        proc_.param(pid)->beginChangeGesture();
      } else {
        // keys, toggles, LCDs: step to the next choice (toggles flip)
        const int n = pi.steps > 0 ? pi.steps : 2;
        const int i = (stepIndex(static_cast<double>(proc_.paramU(pid)), n) + (mods.isRightButtonDown() ? n - 1 : 1)) % n;
        proc_.setParamU(pid, pi.kind == ParamKind::Toggle ? static_cast<float>(i) : static_cast<float>(stepU(i, n)));
        dragBound_ = -1;
      }
      return;
    }
    case B_CHOICE:
      proc_.setParamU(b.a, static_cast<float>(stepU(b.b, kParams[b.a].steps)));
      break;
    case B_SEL:
      selVoice_ = b.a;
      if (mods.isAltDown() || tab_ == 0) proc_.pushPad(b.a);  // MAIN select keys also audition
      break;
    case B_STEP: {
      const int s = page_ * 16 + b.a;
      Step& st = pat.tracks[selVoice_].steps[s];
      if (mods.isRightButtonDown()) {
        selStep_ = s;
      } else if (mods.isShiftDown()) {  // panel legend: shift-click = accent
        st.on = true;
        st.acc = st.acc >= 3 ? 2 : 3;
        selStep_ = s;
        commit();
      } else {
        st.on = !st.on;
        selStep_ = s;
        commit();
      }
      break;
    }
    case B_GRID: {
      Step& st = pat.tracks[b.a].steps[b.b];
      selVoice_ = b.a;
      selStep_ = b.b;
      if (!(mods.isRightButtonDown() || mods.isShiftDown())) {
        if (!st.on) {
          st.on = true;
          st.acc = 2;
        } else if (st.acc == 2) {
          st.acc = 3;  // click cycles: off → on → accent → off
        } else {
          st.on = false;
        }
        commit();
      }
      break;
    }
    case B_LEN: {
      Track& tr = pat.tracks[b.a];
      const bool up = p.x > o.x + o.w / 2;
      tr.len = juce::jlimit(1, kMaxSteps, tr.len + (up ? 1 : -1));
      commit();
      break;
    }
    case B_TAB:
    case B_TABTEXT: setTab(b.a); break;
    case B_RUN: proc_.requestRun(!proc_.meters.running.load()); break;
    case B_RST: proc_.requestRestart(); break;
    case B_CLIPLED: proc_.meters.clip.store(false); break;
    case B_CVAMT: dragStartU_ = static_cast<float>(0.5 * (proc_.cvAmt(isDrum(b.a) ? drumPort(b.a, DJ_PITCH) : synthPort(b.a - LEAD, SJ_NOTE)) + 1.0)); break;
    case B_TSCALE: {
      Track& tr = pat.tracks[b.a];
      tr.scale = tr.scale >= 3 ? -1 : tr.scale + 1;
      commit();
      break;
    }
    case B_TSWING: dragStartU_ = pat.tracks[b.a].swing < 0 ? 0.0f : static_cast<float>((pat.tracks[b.a].swing - 0.5) / 0.25); break;
    case B_TSHIFT: dragStartU_ = static_cast<float>(pat.tracks[b.a].shift); break;
    case B_LFOMODE: {
      char buf[32];
      std::snprintf(buf, sizeof buf, "LFO %d:MODE", b.a + 1);
      const int id = findParam(buf);
      if (id >= 0) proc_.setParamU(id, static_cast<float>(stepU(b.b, 3)));
      break;
    }
    case B_MATRIX: matrixClick(o, p, mods); break;
    case B_JACK:
      if (mods.isRightButtonDown() || mods.isAltDown()) {
        proc_.removeCablesAt(b.a);
        dragBound_ = -1;
      } else {
        cableFrom_ = b.a;
        cableEnd_ = p;
      }
      break;
    case B_TRK: selVoice_ = (selVoice_ + (b.a < 0 ? kVoices - 1 : 1)) % kVoices; break;
    case B_PAGE: page_ = b.a; break;
    case B_COPY:
      for (int s = 0; s < kMaxSteps; ++s) clipboard_[static_cast<size_t>(s)] = pat.tracks[selVoice_].steps[s];
      clipLen_ = pat.tracks[selVoice_].len;
      break;
    case B_PASTE:
      if (clipLen_ > 0) {
        for (int s = 0; s < kMaxSteps; ++s) pat.tracks[selVoice_].steps[s] = clipboard_[static_cast<size_t>(s)];
        pat.tracks[selVoice_].len = clipLen_;
        commit();
      }
      break;
    case B_CLEAR:
      for (auto& st : pat.tracks[selVoice_].steps) st = Step();
      commit();
      break;
    case B_RANDOM: {
      juce::Random r;
      for (int s = 0; s < pat.tracks[selVoice_].len; ++s) pat.tracks[selVoice_].steps[s].on = r.nextFloat() < 0.3f;
      commit();
      break;
    }
    case B_SHIFTL:
    case B_SHIFTR: {
      Track& tr = pat.tracks[selVoice_];
      Step tmp[kMaxSteps];
      for (int s = 0; s < tr.len; ++s) tmp[s] = tr.steps[s];
      for (int s = 0; s < tr.len; ++s) tr.steps[s] = tmp[(s + (b.kind == B_SHIFTL ? 1 : tr.len - 1)) % tr.len];
      commit();
      break;
    }
    case B_CLEARLOCKS: selStep().clearLocks(); commit(); break;
    case B_IDEAL: proc_.requestIdeal(); break;
    case B_INITPATCH: proc_.setCurrentProgram(0); break;
    case B_KIT:
    case B_PATTERN:
    case B_BROWSE: showProgramMenu(); break;  // the factory bank browser: INIT and the kits, each with its pattern
    case B_PROG: proc_.stepProgram(b.a); break;
    case B_AB:
      if (mods.isRightButtonDown() || mods.isShiftDown()) proc_.copyAB(1 - b.a, b.a);  // right-click B = copy A → B
      else proc_.selectAB(b.a);
      break;
    case B_UNDO: proc_.undo(); break;
    case B_REDO: proc_.redo(); break;
    case B_SRC: {  // CLOCK:SOURCE  HOST → INT → EXT (right-click steps back)
      const int i = (stepIndex(static_cast<double>(proc_.paramU(P_CLOCK_SOURCE)), 3) + (mods.isRightButtonDown() ? 2 : 1)) % 3;
      proc_.setParamU(P_CLOCK_SOURCE, static_cast<float>(stepU(i, 3)));
      break;
    }
    case B_OSBADGE: {  // the bus badge shows the realtime oversampling; click cycles GLOBAL:OS 1× → 2× → 4×
      const int i = (stepIndex(static_cast<double>(proc_.paramU(P_GLOBAL_OS)), 3) + (mods.isRightButtonDown() ? 2 : 1)) % 3;
      proc_.setParamU(P_GLOBAL_OS, static_cast<float>(stepU(i, 3)));
      break;
    }
    case B_ASSIGN: armedSrc_ = armedSrc_ == b.a ? 0 : b.a; break;
    case B_UISCALE:
      if (auto* ed = getParentComponent()) ed->setSize(kW * b.a / 100, kH * b.a / 100);
      break;
    case B_PANIC: proc_.requestRestart(); proc_.requestRun(false); break;
    case B_REROLL: proc_.rerollUnit(); break;  // new tolerance seed, applied on the audio thread
    case B_FADER: {
      char buf[48];
      std::snprintf(buf, sizeof buf, "%s:LEVEL", kVoiceNames[b.a]);
      const int id = findParam(buf);
      if (id >= 0) {
        dragStartU_ = proc_.paramU(id);
        proc_.param(id)->beginChangeGesture();
      }
      break;
    }
    case B_SK:
      if (b.a == 7) {
        selStep().tie = !selStep().tie;
        commit();
      }
      break;
    default: break;
  }
  repaint();
}

void ShogunPanel::setStepField(const Bound& b, float u) {
  Step& st = selStep();
  u = juce::jlimit(0.0f, 1.0f, u);
  switch (b.a) {
    case 0: st.acc = static_cast<std::uint8_t>(1 + juce::roundToInt(2.0f * u)); break;
    case 1: st.flam = static_cast<std::uint8_t>(juce::roundToInt(16.0f * u)); break;
    case 2: st.ratchet = static_cast<std::uint8_t>(kRatchets[juce::roundToInt(5.0f * u)]); break;
    case 3: st.prob = u; break;
    case 4: st.micro = u - 0.5f; break;
    case 5: st.bend = static_cast<float>(juce::roundToInt(24.0f * u) - 12); break;
    case 6: st.note = static_cast<std::int8_t>(24 + juce::roundToInt(72.0f * u)); break;
    default: break;
  }
  proc_.commitEdits();
}

void ShogunPanel::mouseDrag(const juce::MouseEvent& e) {
  const auto p = e.position.transformedBy(panelTransform().inverted());
  if (cableFrom_ >= 0) {
    cableEnd_ = p;
    repaint();
    return;
  }
  const float dy = (dragStart_.y - e.position.y) / (e.mods.isShiftDown() ? 800.0f : 200.0f);
  if (matrixDragRow_ >= 0) {
    proc_.editRows()[matrixDragRow_].depth =
        juce::jlimit(-1.0, 1.0, matrixDragDepth_ + static_cast<double>(e.position.x - dragStart_.x) / (70.0 * 0.9467));
    proc_.commitEdits();
    repaint();
    return;
  }
  if (dragBound_ < 0) return;
  const Bound& b = bounds_[static_cast<size_t>(dragBound_)];
  const LayoutOp& o = kOps[b.op];
  Pattern& pat = proc_.editPattern();
  const float u = juce::jlimit(0.0f, 1.0f, dragStartU_ + dy);
  switch (b.kind) {
    case B_PARAM:
    case B_SP:
    case B_WP: {
      const int pid = pidForBound(b.kind, o.bind, b.a, selVoice_, selWaveVoice());
      if (pid >= 0 && o.kind == KNOB) proc_.param(pid)->setValueNotifyingHost(u);
      else if (b.kind == B_SP && pid < 0) {
        const double amt = 2.0 * static_cast<double>(u) - 1.0;
        const int v = selVoice_;
        if (isDrum(v)) for (int j : {DJ_PITCH, DJ_DECAY, DJ_TONE}) proc_.setCvAmt(drumPort(v, j), amt);
        else proc_.setCvAmt(synthPort(v - LEAD, SJ_NOTE), amt);
      }
      break;
    }
    case B_FADER: {
      char buf[48];
      std::snprintf(buf, sizeof buf, "%s:LEVEL", kVoiceNames[b.a]);
      const int id = findParam(buf);
      const float fu = juce::jlimit(0.0f, 1.0f, dragStartU_ + (dragStart_.y - e.position.y) / (110.0f * 0.9467f));
      if (id >= 0) proc_.param(id)->setValueNotifyingHost(fu);
      break;
    }
    case B_CVAMT: {
      // CV AMT (§11 ROUTE): drums = PITCH/DECAY/TONE; LEAD/BASS = NOTE (and V/OCT, read from the NOTE port's AMT).
      // Right-click menus can set each jack's AMT separately.
      const double amt = 2.0 * static_cast<double>(u) - 1.0;
      if (isDrum(b.a)) for (int j : {DJ_PITCH, DJ_DECAY, DJ_TONE}) proc_.setCvAmt(drumPort(b.a, j), amt);
      else proc_.setCvAmt(synthPort(b.a - LEAD, SJ_NOTE), amt);
      break;
    }
    case B_TSWING: pat.tracks[b.a].swing = u <= 0.0f ? -1.0 : 0.5 + 0.25 * static_cast<double>(u); proc_.commitEdits(); break;
    case B_TSHIFT: pat.tracks[b.a].shift = static_cast<double>(u); proc_.commitEdits(); break;
    case B_SK: {
      float cur = 0.0f;
      const Step& st = selStep();
      switch (b.a) {
        case 0: cur = (st.acc - 1) / 2.0f; break;
        case 1: cur = st.flam / 16.0f; break;
        case 3: cur = st.prob; break;
        case 4: cur = st.micro + 0.5f; break;
        case 5: cur = (st.bend + 12.0f) / 24.0f; break;
        case 6: cur = (st.note - 24) / 72.0f; break;
        default: break;
      }
      juce::ignoreUnused(cur);
      if (e.mouseWasDraggedSinceMouseDown()) setStepField(b, u);
      break;
    }
    default: break;
  }
  repaint();
}

void ShogunPanel::mouseUp(const juce::MouseEvent& e) {
  if (cableFrom_ >= 0) {
    const auto p = e.position.transformedBy(panelTransform().inverted());
    const int hit = findBound(p);
    if (hit >= 0 && bounds_[static_cast<size_t>(hit)].kind == B_JACK) {
      const int to = bounds_[static_cast<size_t>(hit)].a;
      int a = cableFrom_, c = to;
      if (kPortTable[a].dir == PortDir::In) std::swap(a, c);
      if (a != c) proc_.addCable(a, c);
    }
    cableFrom_ = -1;
    repaint();
  }
  if (dragBound_ >= 0) {
    const Bound& b = bounds_[static_cast<size_t>(dragBound_)];
    const LayoutOp& o = kOps[b.op];
    const int pid = pidForBound(b.kind, o.bind, b.a, selVoice_, selWaveVoice());
    if (pid >= 0 && o.kind == KNOB) proc_.param(pid)->endChangeGesture();
    if (b.kind == B_FADER) {
      char buf[48];
      std::snprintf(buf, sizeof buf, "%s:LEVEL", kVoiceNames[b.a]);
      const int id = findParam(buf);
      if (id >= 0) proc_.param(id)->endChangeGesture();
    }
  }
  dragBound_ = -1;
  matrixDragRow_ = -1;
}

void ShogunPanel::mouseDoubleClick(const juce::MouseEvent& e) {
  const auto p = e.position.transformedBy(panelTransform().inverted());
  const int bi = findBound(p);
  if (bi < 0) return;
  const Bound& b = bounds_[static_cast<size_t>(bi)];
  const LayoutOp& o = kOps[b.op];
  const int pid = pidForBound(b.kind, o.bind, b.a, selVoice_, selWaveVoice());
  if (pid >= 0 && o.kind == KNOB) {
    proc_.beginUndoStep();
    proc_.setParamU(pid, kParams[pid].def);  // double-click = default (noon kit value)
  }
}

void ShogunPanel::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) {
  wheelAt(e.position.transformedBy(panelTransform().inverted()), w.deltaY);
}

void ShogunPanel::wheelAt(juce::Point<float> p, float deltaY) {
  const int bi = findBound(p);
  if (bi < 0) return;
  const Bound& b = bounds_[static_cast<size_t>(bi)];
  const LayoutOp& o = kOps[b.op];
  if (b.kind == B_MATRIX) {  // wheel over the matrix scrolls it a row per notch (down = later rows)
    if (deltaY < 0.0f) scrollMatrix(1);
    else if (deltaY > 0.0f) scrollMatrix(-1);
    return;
  }
  const int pid = pidForBound(b.kind, o.bind, b.a, selVoice_, selWaveVoice());
  if (pid >= 0 && o.kind == KNOB) {
    if (!proc_.undoPending()) proc_.beginUndoStep();  // a wheel burst is one step (settled by the timer)
    proc_.setParamU(pid, proc_.paramU(pid) + deltaY * 0.05f);
  }
}

// ================================================================ editor

ShogunAudioProcessorEditor::ShogunAudioProcessorEditor(ShogunAudioProcessor& p) : AudioProcessorEditor(p), panel_(p) {
  addAndMakeVisible(panel_);
  setResizable(true, true);
  setResizeLimits(ShogunPanel::kW / 2, ShogunPanel::kH / 2, ShogunPanel::kW * 2, ShogunPanel::kH * 2);
  getConstrainer()->setFixedAspectRatio(static_cast<double>(ShogunPanel::kW) / ShogunPanel::kH);
  setSize(ShogunPanel::kW, ShogunPanel::kH);
}

void ShogunAudioProcessorEditor::resized() {
  const float s = static_cast<float>(getWidth()) / ShogunPanel::kW;
  panel_.setTransform(juce::AffineTransform::scale(s));
  panel_.setBounds(0, 0, ShogunPanel::kW, ShogunPanel::kH);
}
