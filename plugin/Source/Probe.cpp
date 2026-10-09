// ShogunProbe: headless checks of the plugin shell (spec v2.2 §15.0 step 4) and the 8 tab renders.
//   ShogunProbe <out-dir>   writes tab_0_main.png … tab_7_global.png and prints one line per check.
#include <cstring>
#include <cstdlib>
#include <map>
#include <cmath>
#include <iostream>

#include "PluginEditor.h"
#include "MidiExport.h"
#include "factory.h"

using namespace shogun;

namespace {

int failures = 0;
void check(bool ok, const juce::String& name, const juce::String& numbers) {
  std::cout << (ok ? "PASS " : "FAIL ") << name << "  " << numbers << std::endl;
  if (!ok) ++failures;
}

struct FakeHead : juce::AudioPlayHead {
  double ppq = 0.0, bpm = 120.0;
  bool playing = true;
  juce::Optional<PositionInfo> getPosition() const override {
    PositionInfo p;
    p.setPpqPosition(ppq);
    p.setBpm(bpm);
    p.setIsPlaying(playing);
    return p;
  }
};

struct Render {
  float peakMain = 0.0f, peakAux = 0.0f;
  std::uint64_t hash = 1469598103934665603ull;  // FNV-1a over the main pair's sample bits
};

Render render(ShogunAudioProcessor& proc, int samples, juce::MidiBuffer* midi = nullptr, FakeHead* head = nullptr) {
  const int block = 512;
  const int ch = proc.getTotalNumOutputChannels();
  juce::AudioBuffer<float> buf(ch, block);
  Render r;
  bool first = true;
  for (int done = 0; done < samples; done += block) {
    buf.clear();
    juce::MidiBuffer m;
    if (first && midi) m.swapWith(*midi);
    first = false;
    proc.processBlock(buf, m);
    if (head) head->ppq += head->bpm / 60.0 * block / proc.getSampleRate();
    r.peakMain = std::fmax(r.peakMain, std::fmax(buf.getMagnitude(0, 0, block), buf.getMagnitude(1, 0, block)));
    for (int c = 0; c < std::min(2, ch); ++c)
      for (int i = 0; i < block; ++i) {
        std::uint32_t u;
        const float s = buf.getSample(c, i);
        std::memcpy(&u, &s, 4);
        r.hash = (r.hash ^ u) * 1099511628211ull;
      }
    for (int c = 2; c < ch; ++c) r.peakAux = std::fmax(r.peakAux, buf.getMagnitude(c, 0, block));
  }
  return r;
}

juce::String paramDisplay(int id, float u) {
  char buf[48];
  formatParam(id, static_cast<double>(u), buf, sizeof buf);
  return juce::String::fromUTF8(buf);
}

void setU(ShogunAudioProcessor& p, const char* id, float u) { p.param(findParam(id))->setValueNotifyingHost(u); }

std::unique_ptr<ShogunAudioProcessor> fresh(double sr = 48000.0) {
  auto p = std::make_unique<ShogunAudioProcessor>();
  p->setRateAndBufferSizeDetails(sr, 512);
  p->prepareToPlay(sr, 512);
  return p;
}

}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI gui;
  const juce::File outDir(argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])
                                   : juce::File::getCurrentWorkingDirectory());
  outDir.createDirectory();

  // ---- parameters: float, SECTION:LABEL ids, u ∈ [0, 1], defaults from the table
  {
    auto p = fresh();
    int bad = 0, n = 0;
    for (auto* ap : p->getParameters()) {
      auto* f = dynamic_cast<juce::AudioParameterFloat*>(ap);
      auto* wid = dynamic_cast<juce::AudioProcessorParameterWithID*>(ap);
      if (f == nullptr || wid == nullptr) {
        ++bad;
        continue;
      }
      const int id = findParam(wid->getParameterID().toRawUTF8());
      if (id != n || !dsp::exactEq(f->range.start, 0.0f) || !dsp::exactEq(f->range.end, 1.0f) || std::fabs(f->get() - kParams[id].def) > 1e-6f) ++bad;
      ++n;
    }
    check(n == kParamCount && bad == 0, "params", "count " + juce::String(n) + " (table " + juce::String(kParamCount) +
                                                    "), float 0..1 SECTION:LABEL ids, mismatches " + juce::String(bad));
  }

  // ---- latency 0 / 23 / 26 (GLOBAL:OS 1X/2X/4X), reported to the host
  {
    auto p = fresh();
    const int l2 = p->getLatencySamples();
    setU(*p, "GLOBAL:OS", 0.1f);
    p->prepareToPlay(48000.0, 512);
    const int l1 = p->getLatencySamples();
    setU(*p, "GLOBAL:OS", 0.9f);
    p->prepareToPlay(48000.0, 512);
    const int l4 = p->getLatencySamples();
    check(l1 == 0 && l2 == 23 && l4 == 26, "latency", juce::String(l1) + "/" + juce::String(l2) + "/" + juce::String(l4) + " samples");
    // host rate: the engine runs at the host's rate (no resampler)
    auto q = fresh(44100.0);
    check(dsp::exactEq(q->engine().sampleRate(), 44100.0), "host rate", "engine fs " + juce::String(q->engine().sampleRate(), 1) + " Hz at host 44100");
  }

  // ---- INIT: empty pattern is silent while running; MIDI note 36 plays BD1
  {
    auto p = fresh();
    p->requestRun(true);
    const Render r = render(*p, 48000);
    check(dsp::exactEq(r.peakMain, 0.0f) && juce::String(p->editPattern().name) == "001 INIT", "INIT silent",
          "peak " + juce::String(r.peakMain, 6) + ", pattern '" + juce::String(p->editPattern().name) + "'");
    juce::MidiBuffer m;
    m.addEvent(juce::MidiMessage::noteOn(10, 36, static_cast<juce::uint8>(127)), 0);
    const Render r2 = render(*p, 24000, &m);
    check(r2.peakMain > 0.05f, "MIDI 36 -> BD1", "peak " + juce::String(r2.peakMain, 6));
  }

  // ---- test beat (INT): BD1 on 1/5/9/13 plays; EXT ignores the pattern (forge pin)
  {
    auto p = fresh();
    setU(*p, "CLOCK:SOURCE", 0.5f);  // INT
    for (int s : {0, 4, 8, 12}) p->editPattern().tracks[BD1].steps[s].on = true;
    p->commitEdits();
    p->requestRun(true);
    const Render r = render(*p, 96000);
    check(r.peakMain > 0.05f, "INT test beat", "peak " + juce::String(r.peakMain, 6) + " = " +
                                                   juce::String(20.0 * std::log10(static_cast<double>(r.peakMain)), 2) + " dBFS");
    auto q = fresh();
    setU(*q, "CLOCK:SOURCE", 0.5f);
    setU(*q, "CLOCK:MODE", 0.75f);  // EXT
    for (int s : {0, 4, 8, 12}) q->editPattern().tracks[BD1].steps[s].on = true;
    q->commitEdits();
    q->requestRun(true);
    const Render r2 = render(*q, 96000);
    check(dsp::exactEq(r2.peakMain, 0.0f), "EXT ignores pattern", "peak " + juce::String(r2.peakMain, 6));
  }

  // ---- host lock: SOURCE = HOST follows ppq (7.25 quarters = step 29 at 1/16)
  {
    auto p = fresh();
    setU(*p, "CLOCK:SOURCE", 0.1f);  // HOST
    for (int s = 0; s < 32; ++s) p->editPattern().tracks[BD1].steps[s].on = (s == 29);
    p->editPattern().tracks[BD1].len = 32;
    p->commitEdits();
    FakeHead head;
    head.ppq = 7.25;
    p->setPlayHead(&head);
    p->requestRun(true);
    const Render r = render(*p, 512 * 4, nullptr, &head);
    const int step = p->meters.step.load();
    const int gstep = p->meters.globalStep.load();
    check(r.peakMain > 0.05f && gstep == 29, "host lock",
          "ppq 7.25 -> global step " + juce::String(gstep) + " (bar-of-16 display " + juce::String(step) + "), step-29 hit peak " +
              juce::String(r.peakMain, 6));
    p->setPlayHead(nullptr);
  }

  // ---- aux outputs: BD1 OUTPUT = AUX 1/2 → plugin bus "Aux 1"
  {
    auto p = std::make_unique<ShogunAudioProcessor>();
    auto layout = p->getBusesLayout();
    layout.outputBuses.getReference(1) = juce::AudioChannelSet::stereo();
    const bool ok = p->setBusesLayout(layout);
    p->setRateAndBufferSizeDetails(48000.0, 512);
    p->prepareToPlay(48000.0, 512);
    setU(*p, "BD1:OUTPUT", static_cast<float>(stepU(5, 14)));
    p->prepareToPlay(48000.0, 512);
    juce::MidiBuffer m;
    m.addEvent(juce::MidiMessage::noteOn(10, 36, static_cast<juce::uint8>(127)), 0);
    const Render r = render(*p, 24000, &m);
    check(ok && r.peakAux > 0.05f, "aux 1/2 bus", "channels " + juce::String(p->getTotalNumOutputChannels()) + ", aux peak " +
                                                    juce::String(r.peakAux, 6) + ", main peak " + juce::String(r.peakMain, 6));
  }

  // ---- factory programs (engine/factory.h): the host program list, each program plays 2 bars between -40 and -6 dBFS
  //      at 44.1 and 48 kHz (INT clock), and its saved state reloads to the same document
  {
    auto p = fresh();
    int names = 0;
    for (int i = 0; i < p->getNumPrograms(); ++i)
      if (p->getProgramName(i) == juce::String::fromUTF8(factory::programName(i)) && p->getProgramName(i).isNotEmpty()) ++names;
    check(p->getNumPrograms() == factory::kPrograms && factory::kPrograms > 16 && names == factory::kPrograms &&
              p->getProgramName(0) == "INIT",
          "programs", juce::String(p->getNumPrograms()) + " (INIT + " + juce::String(factory::kCount) + " factory kits), names " +
                          juce::String(names));
    int bad = 0, trips = 0;
    double lo = 0.0, hi = -200.0;
    juce::String worst;
    for (int i = 1; i < factory::kPrograms; ++i) {
      for (double sr : {44100.0, 48000.0}) {
        auto q = fresh(sr);
        q->setCurrentProgram(i);
        if (q->getCurrentProgram() != i || juce::String(q->editPattern().name).substring(4) != q->getProgramName(i)) ++bad;
        if (dsp::exactEq(sr, 44100.0)) {  // saved state of the loaded program reloads to the same document
          juce::MemoryBlock a, b;
          q->getStateInformation(a);
          auto t = fresh(sr);
          t->setStateInformation(a.getData(), static_cast<int>(a.getSize()));
          t->getStateInformation(b);
          if (a == b && t->getCurrentProgram() == i) ++trips;
        }
        setU(*q, "CLOCK:SOURCE", 0.5f);  // INT: no host transport in the probe
        q->requestRun(true);
        const double bpm = 40.0 + 160.0 * static_cast<double>(q->paramU(findParam("CLOCK:TEMPO")));
        const Render r = render(*q, static_cast<int>(std::ceil(2.0 * 4.0 * 60.0 / bpm * sr)));
        const double db = r.peakMain > 0.0f ? 20.0 * std::log10(static_cast<double>(r.peakMain)) : -200.0;
        if (db <= -40.0 || db > -6.0) {
          ++bad;
          worst = q->getProgramName(i) + " " + juce::String(db, 2);
        }
        lo = std::fmin(lo, db);
        hi = std::fmax(hi, db);
      }
    }
    check(bad == 0, "programs play", juce::String(factory::kCount) + " kits x 2 rates, 2 bars, peaks " + juce::String(lo, 2) +
                                         " .. " + juce::String(hi, 2) + " dBFS" + (worst.isEmpty() ? "" : ", out: " + worst));
    check(trips == factory::kCount, "program state", juce::String(trips) + "/" + juce::String(factory::kCount) +
                                                         " programs save and reload to the same state");
  }

  // ---- program load is atomic for the audio thread (B): a host block landing between the INIT reset and the
  // program's values (here: right in the middle of loadProgram) applies nothing, and the next block sets the whole new
  // kit at once. Switching from the kit with the most BD1 DRIVE to a kit without, with that block in between, renders
  // bit-identically to the same switch with no block mid-load, and peaks within the kits' own levels (the switch's
  // overlap of old tails and new hits included) + 1 dB.
  {
    int a = -1, b = -1;
    double most = 0.0;
    for (int i = 1; i < factory::kPrograms; ++i) {
      Patch pt;
      factory::loadProgram(i, pt);
      if (pt.u[P_BD1_DRIVE] > most) { most = pt.u[P_BD1_DRIVE]; a = i; }
      if (b < 0 && dsp::exactEq(pt.u[P_BD1_DRIVE], 0.0)) b = i;
    }
    auto start = [&](int prog) {
      auto q = fresh();
      q->setCurrentProgram(prog);
      setU(*q, "CLOCK:SOURCE", 0.5f);
      q->requestRun(true);
      return q;
    };
    const float pa = render(*start(a), 96000).peakMain, pb = render(*start(b), 96000).peakMain;
    float worst = 0.0f, ref = 0.0f;
    bool skipped = true, same = true;
    for (int offs : {3072, 12288, 24064, 47104}) {
      auto q = start(a);
      render(*q, offs);
      const double before = q->engine().param(P_BD1_DRIVE);
      q->loadProgramWith(b, [&] {
        render(*q, 512);  // the host block in the middle of the load
        skipped = skipped && dsp::exactEq(q->engine().param(P_BD1_DRIVE), before);
      });
      const Render mid = render(*q, 48000);
      skipped = skipped && dsp::exactEq(q->engine().param(P_BD1_DRIVE), 0.0);
      auto r = start(a);  // reference: the same switch after the same blocks, nothing mid-load
      render(*r, offs + 512);
      r->loadProgram(b);
      const Render plain = render(*r, 48000);
      same = same && mid.hash == plain.hash;
      worst = std::fmax(worst, mid.peakMain);
      ref = std::fmax(ref, plain.peakMain);
    }
    auto db = [](float v) { return juce::String(20.0 * std::log10(static_cast<double>(v)), 2); };
    const float lim = std::fmax(std::fmax(pa, pb), ref) * 1.122f;  // +1 dB
    check(a > 0 && b > 0 && skipped && same && worst <= lim, "program load race",
          juce::String::fromUTF8(factory::programName(a)) + " (BD1 DRIVE " + juce::String(most, 3) + ") -> " +
              juce::String::fromUTF8(factory::programName(b)) + ", block mid-load at 4 offsets: peak " + db(worst) +
              " dBFS (same switch without the mid-load block " + db(ref) + ", kits alone " + db(pa) + " / " + db(pb) +
              "); bit-identical to the plain switch " + (same ? "yes" : "NO") + "; mid-load block applied nothing " +
              (skipped ? "yes" : "NO"));
  }

  // ---- state: XML SHOGUN version=2 with the JSON patch; alias resolution on load
  {
    auto p = fresh();
    setU(*p, "BD1:DECAY", 0.25f);
    p->editPattern().tracks[SD].steps[4].on = true;
    p->editPattern().tracks[SD].steps[4].setLock(findParam("SD:TONE"), 0.8f);
    p->editPattern().setName("002 PROBE");
    p->editRows()[0].src = mod::SRC_LFO1;
    p->editRows()[0].dst = findParam("BD1:DECAY");
    p->editRows()[0].depth = 0.18;
    p->commitEdits();
    p->addCable(findPort("MOD:LFO 1"), findPort("BD1:PITCH"));
    juce::MemoryBlock mb;
    p->getStateInformation(mb);
    auto xml = juce::AudioProcessor::getXmlFromBinary(mb.getData(), static_cast<int>(mb.getSize()));
    auto q = fresh();
    q->setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    const bool same = std::fabs(q->paramU(findParam("BD1:DECAY")) - 0.25f) < 1e-6f && q->editPattern().tracks[SD].steps[4].on &&
                      q->editPattern().tracks[SD].steps[4].nLocks == 1 && juce::String(q->editPattern().name) == "002 PROBE" &&
                      q->editRows()[0].src == mod::SRC_LFO1 && std::fabs(q->editRows()[0].depth - 0.18) < 1e-9 && q->cableCount() == 1;
    check(xml && xml->hasTagName("SHOGUN") && xml->getIntAttribute("version") == 2 && same, "state round trip",
          "XML <SHOGUN version=" + juce::String(xml ? xml->getIntAttribute("version") : -1) + "> JSON " +
              juce::String(static_cast<int>(xml ? xml->getAllSubText().length() : 0)) + " chars; params/pattern/lock/mod/cable restored");

    // v2.0 patch with an old jack id: BASS:HZ/V → BASS:NOTE with the lin55 law; TOM:PITCH → 3 toms at CV AMT 1/12
    juce::String json = R"({"format":"shogun-patch","version":2,"params":{},"cables":[["SHOGUN/MOD:LFO 1","SHOGUN/BASS:HZ/V"],["SHOGUN/MOD:LFO 2","SHOGUN/TOM:PITCH"]]})";
    juce::XmlElement x2("SHOGUN");
    x2.setAttribute("version", 2);
    x2.addTextElement(json);
    juce::MemoryBlock mb2;
    juce::AudioProcessor::copyXmlToBinary(x2, mb2);
    auto r = fresh();
    r->setStateInformation(mb2.getData(), static_cast<int>(mb2.getSize()));
    juce::MemoryBlock mb3;
    r->getStateInformation(mb3);
    auto x3 = juce::AudioProcessor::getXmlFromBinary(mb3.getData(), static_cast<int>(mb3.getSize()));
    const juce::var v = juce::JSON::parse(x3->getAllSubText());
    const int cables = r->cableCount();
    const double amt = r->cvAmt(findPort("MTC:PITCH"));
    check(cables == 4 && static_cast<int>(v["inLaw"]["BASS:NOTE"]) == 1 && std::fabs(amt - 1.0 / 12.0) < 1e-12, "alias load",
          "cables 4 (1 + 3 toms) = " + juce::String(cables) + ", BASS:NOTE law " + v["inLaw"]["BASS:NOTE"].toString() +
              ", MTC:PITCH CV AMT " + juce::String(amt, 6));

    // The loaded law plays: 2.0 V into the old BASS:HZ/V cable (now BASS:NOTE, shared AliasLaw::Lin55ToVoct) is 110 Hz.
    juce::AudioBuffer<float> one(r->getTotalNumOutputChannels(), 64);
    juce::MidiBuffer none;
    r->processBlock(one, none);  // applies the loaded cables and laws to the engine
    shogun::Engine& eng = r->engine();
    const int notePort = findPort("BASS:NOTE"), gatePort = findPort("BASS:GATE");
    eng.setParamNow(shogun::P_CLOCK_MODE, shogun::stepU(1, 2));  // EXT: the gate plays the voice
    eng.setParamNow(shogun::P_GLOBAL_TOLERANCE, 0.0);            // no per-voice tolerance or drift: the pure law
    eng.setParamNow(shogun::P_GLOBAL_DRIFT, 0.0);
    std::vector<float> vals(static_cast<size_t>(kPorts), 0.0f);
    std::unique_ptr<bool[]> con(new bool[static_cast<size_t>(kPorts)]());
    vals[static_cast<size_t>(notePort)] = 2.0f;
    vals[static_cast<size_t>(gatePort)] = 5.0f;
    con[static_cast<size_t>(notePort)] = con[static_cast<size_t>(gatePort)] = true;
    for (int n = 0; n < 4; ++n) eng.processSample(vals.data(), con.get());
    const double hz = eng.synth(shogun::BASS).f;
    const double cents = 1200.0 * std::log2(hz / 110.0);
    check(eng.inputLaw(notePort) == 1 && std::fabs(cents) < 0.01, "old HZ/V plays",
          "BASS:HZ/V cable at 2.0 V -> " + juce::String(hz, 6) + " Hz (" + juce::String(cents, 5) + " cents from 110), law " +
              juce::String(eng.inputLaw(notePort)));

    // An old state with cables on the removed CLOCK:FILL IN and MOD:LANE A loads: those cables are dropped and
    // reported (loadReport), the good cable stays, the state saves without them and the processor runs.
    juce::String oldJson = R"({"format":"shogun-patch","version":2,"params":{},"cables":[["SHOGUN/CLOCK:CLK OUT","SHOGUN#1/CLOCK:FILL IN"],["SHOGUN/MOD:LANE A","SHOGUN/BD1:DECAY"],["SHOGUN/MOD:LFO 1","SHOGUN/BD1:PITCH"]]})";
    juce::XmlElement x4("SHOGUN");
    x4.setAttribute("version", 2);
    x4.addTextElement(oldJson);
    juce::MemoryBlock mb4, mb5;
    juce::AudioProcessor::copyXmlToBinary(x4, mb4);
    auto o = fresh();
    o->setStateInformation(mb4.getData(), static_cast<int>(mb4.getSize()));
    juce::AudioBuffer<float> blk(o->getTotalNumOutputChannels(), 256);
    o->processBlock(blk, none);
    o->getStateInformation(mb5);
    auto x5 = juce::AudioProcessor::getXmlFromBinary(mb5.getData(), static_cast<int>(mb5.getSize()));
    const juce::String saved = x5 ? x5->getAllSubText() : juce::String();
    check(o->cableCount() == 1 && o->droppedCables() == 2 && o->droppedRemovedCables() == 2 && o->loadReport().contains("FILL IN") &&
              !saved.contains("FILL IN") && !saved.contains("LANE A") && saved.contains("BD1:PITCH"),
          "removed jacks", "old FILL IN / LANE A cables dropped " + juce::String(o->droppedCables()) + " (removed " +
              juce::String(o->droppedRemovedCables()) + "), kept " + juce::String(o->cableCount()) + "; report: " + o->loadReport());
  }

  // ---- editor: 1200 x 672, 8 tabs, 151 bay jacks (every port); render each tab to PNG
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto* se = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get());
    // Every port has a jack on the ROUTE tab (CLOCK:FILL IN and MOD:LANE A are no longer ports, engine/ports.h).
    int jacks = 0;
    for (int i = 0; i < kPorts; ++i) {
      juce::Point<float> pt;
      jacks += se->panel().jackPosition(i, pt) ? 1 : 0;
    }
    check(ed->getWidth() == 1200 && ed->getHeight() == 672 && jacks == kPorts && kPorts == 151, "editor",
          juce::String(ed->getWidth()) + "x" + juce::String(ed->getHeight()) + ", bay jacks " + juce::String(jacks) + "/" +
              juce::String(kPorts) + ", ops " + juce::String(ShogunPanel::opCount()));
    static const char* const names[8] = {"main", "voice", "grid", "mod", "route", "fxmix", "seqmidi", "global"};
    int written = 0;
    for (int t = 0; t < 8; ++t) {
      se->panel().setTab(t);
      juce::Image img(juce::Image::RGB, 1200, 672, true);
      {
        juce::Graphics g(img);
        se->panel().paintEntireComponent(g, true);
      }
      const juce::File f = outDir.getChildFile(juce::String("tab_") + juce::String(t) + "_" + names[t] + ".png");
      f.deleteFile();
      juce::FileOutputStream os(f);
      juce::PNGImageFormat png;
      if (os.openedOk() && png.writeImageToStream(img, os)) ++written;
    }
    // demo content (dynamic layers: grid, steps, matrix rows, cables)
    Pattern& pat = p->editPattern();
    for (int st : {0, 4, 8, 12}) pat.tracks[BD1].steps[st].on = true;
    for (int st : {4, 12}) pat.tracks[SD].steps[st].on = true;
    for (int st = 0; st < 16; st += 2) pat.tracks[CH].steps[st].on = true;
    pat.tracks[SD].steps[12].acc = 3;
    p->editRows()[0] = {mod::SRC_LFO1, -1, findParam("BD1:DECAY"), 0.18, mod::SRC_NONE, -1, mod::LIN, true};
    p->editRows()[1] = {mod::SRC_ENV, BASS, findParam("BASS:CUTOFF"), 0.30, mod::SRC_VEL, -1, mod::EXP, true};
    p->editRows()[2] = {mod::SRC_LFO2, -1, findParam("CH:DECAY"), -0.16, mod::SRC_NONE, -1, mod::LIN, true};
    p->commitEdits();
    p->addCable(findPort("MOD:LFO 1"), findPort("BD1:PITCH"));
    p->addCable(findPort("LTC:OUT"), findPort("CLOCK:RST IN"));
    p->addCable(findPort("LEAD:NOTE OUT"), findPort("BASS:NOTE"));
    se->panel().setSelectedVoice(SD);
    for (int t : {0, 2, 3, 4}) {
      se->panel().setTab(t);
      juce::Image img(juce::Image::RGB, 1200, 672, true);
      {
        juce::Graphics g(img);
        se->panel().paintEntireComponent(g, true);
      }
      const juce::File f = outDir.getChildFile(juce::String("demo_") + juce::String(t) + "_" + names[t] + ".png");
      f.deleteFile();
      juce::FileOutputStream os(f);
      juce::PNGImageFormat png;
      if (os.openedOk()) png.writeImageToStream(img, os);
    }
    check(written == 8, "tab renders", juce::String(written) + " PNGs in " + outDir.getFullPathName());
  }


  // ---- GRID tab cells (C): every one of the 16 x 32 cells is a row-high rect whose centre hits that cell, the
  // playhead column over step 5 is not a cell, and clicking column 5 of row 2 edits row 2 only (BD1 untouched).
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    pn.setTab(2);
    int cells = 0, bad = 0;
    juce::String firstBad;
    for (int r = 0; r < 16; ++r)
      for (int c = 0; c < 32; ++c) {
        const juce::String name = "grid:" + juce::String(r) + ":" + juce::String(c);
        juce::Rectangle<float> rc;
        const bool found = pn.bindRect(name.toRawUTF8(), rc), second = pn.bindRect(name.toRawUTF8(), rc, 1);
        const bool ok = found && !second && rc.getHeight() > 15.0f && rc.getHeight() < 26.5f &&
                        pn.bindAt(rc.getCentre()) == name;
        cells += found ? 1 : 0;
        if (!ok && bad++ == 0) firstBad = name + " h=" + juce::String(rc.getHeight(), 1);
      }
    Pattern& pat = p->editPattern();
    const bool bd1Before = pat.tracks[0].steps[4].on, r2Before = pat.tracks[1].steps[4].on;
    juce::Rectangle<float> c14;
    pn.bindRect("grid:1:4", c14);
    pn.clickAt(c14.getCentre());
    const bool clickOk = pat.tracks[0].steps[4].on == bd1Before && pat.tracks[1].steps[4].on != r2Before;
    juce::Rectangle<float> ph;
    const bool playhead = pn.bindRect("playhead", ph) && ph.getHeight() > 26.5f * 15.0f;
    check(cells == 512 && bad == 0 && clickOk && playhead, "GRID cells",
          juce::String(cells) + " cells, " + juce::String(bad) + " bad" + (bad ? " (first " + firstBad + ")" : juce::String()) +
              "; row 2 col 5 click edits row 2 only " + (clickOk ? "yes" : "NO") + "; playhead column separate " +
              (playhead ? "yes" : "NO"));
  }

  // ---- VOICE tab group-key corner lights (L): kept as one-click mute toggles (user decision). During playback a click
  // on each light mutes its whole group (the same <V>:MUTE params the GRID and FX/MIX M keys drive), the tooltip
  // flips Mute -> Unmute, a second click unmutes; an M key press on GRID shows on the light; clicking the group's name
  // key selects the voice without muting.
  {
    auto p = fresh();
    p->setCurrentProgram(1);
    setU(*p, "CLOCK:SOURCE", 0.5f);
    p->requestRun(true);
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    render(*p, 24576);
    auto muted = [&](const juce::String& v) { return p->param(findParam((v + ":MUTE").toRawUTF8()))->getValue() > 0.5f; };
    const char* groups[] = {"BD1", "BD2", "SD+RS", "CP+CL", "CB+MA", "CH+OH+CY", "LTC+MTC+HTC", "LEAD", "BASS"};
    const char* names[] = {"sel:BD1", "sel:BD2", "sel:SD", "sel:CP", "sel:CB", "sel:CH", "sel:LTC", "sel:LEAD", "sel:BASS"};
    int okCount = 0;
    juce::String bad;
    for (int g = 0; g < 9; ++g) {
      pn.setTab(1);
      const juce::StringArray vs = juce::StringArray::fromTokens(groups[g], "+", "");
      const juce::String bind = juce::String("vmute:") + groups[g];
      juce::Rectangle<float> led;
      bool ok = pn.bindRect(bind.toRawUTF8(), led) && pn.tooltipAt(led.getCentre()) == "Mute";
      ok = ok && pn.clickAt(led.getCentre());
      render(*p, 4096);
      for (const auto& v : vs) ok = ok && muted(v);
      ok = ok && pn.tooltipAt(led.getCentre()) == "Unmute";
      pn.setTab(2);  // GRID: the M key of the group's first voice shows it and unmutes it
      ok = ok && pn.pressBind(("p:" + vs[0] + ":MUTE").toRawUTF8()) && !muted(vs[0]);
      pn.setTab(1);
      ok = ok && pn.tooltipAt(led.getCentre()) == "Mute";  // no longer all muted -> the light is red again
      ok = ok && pn.clickAt(led.getCentre());              // mutes the whole group again
      for (const auto& v : vs) ok = ok && muted(v);
      ok = ok && pn.clickAt(led.getCentre());              // and unmutes it
      render(*p, 4096);
      for (const auto& v : vs) ok = ok && !muted(v);
      ok = ok && pn.pressBind(names[g]);  // the name key selects without muting
      for (const auto& v : vs) ok = ok && !muted(v);
      if (ok) ++okCount; else bad << " " << groups[g];
    }
    check(okCount == 9, "VOICE corner mute lights",
          juce::String(okCount) + "/9 groups toggle mute during playback, synced with the GRID M keys, tooltip Mute/Unmute, "
                                  "name key selects without muting" + (bad.isEmpty() ? juce::String() : "; failed:" + bad));
  }

  // ---- editor repaint (item 4) and press feedback (item M). Each timer tick repaints only the ops whose drawn state
  // changed; an incremental frame (the previous frame + a paint clipped to the tick's dirty rects) must equal a full
  // repaint pixel for pixel on every tab during playback, with edits in between. The timer is off while the editor
  // is not showing. A step key press then release marks only that key's rect, and its 120 ms flash ends.
  {
    auto p = fresh();
    p->setCurrentProgram(1);
    setU(*p, "CLOCK:SOURCE", 0.5f);
    p->requestRun(true);
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    ed->setSize(1200, 672);
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    const bool timerOff = !pn.timerOn();
    int maxCh = 0;  // largest per-channel difference seen
    auto diffPx = [&maxCh](const juce::Image& a, const juce::Image& b) {
      juce::Image::BitmapData da(a, juce::Image::BitmapData::readOnly), db(b, juce::Image::BitmapData::readOnly);
      long n = 0;
      juce::Rectangle<int> bb;
      for (int y = 0; y < a.getHeight(); ++y)
        for (int x = 0; x < a.getWidth(); ++x)
          if (da.getPixelColour(x, y) != db.getPixelColour(x, y)) {
            const juce::Colour ca = da.getPixelColour(x, y), cb = db.getPixelColour(x, y);
            const int dc = std::max({std::abs(ca.getRed() - cb.getRed()), std::abs(ca.getGreen() - cb.getGreen()),
                                     std::abs(ca.getBlue() - cb.getBlue())});
            maxCh = std::max(maxCh, dc);
            ++n;
            bb = bb.isEmpty() ? juce::Rectangle<int>(x, y, 1, 1) : bb.getUnion({x, y, 1, 1});
          }
      return n;
    };
    juce::Image inc(juce::Image::RGB, 1200, 672, true), full(juce::Image::RGB, 1200, 672, true);
    long worst = 0, frames = 0;
    double area = 0;
    juce::String where;
    for (int tab = 0; tab < 8; ++tab) {
      pn.setTab(tab);
      { juce::Graphics g(inc); pn.paintEntireComponent(g, true); }
      pn.tickForTest();
      { juce::Graphics g(inc); pn.paintEntireComponent(g, true); }
      for (int i = 0; i < 45; ++i) {
        render(*p, 1600);
        if (i == 10) setU(*p, "BD1:DECAY", 0.2f + 0.05f * static_cast<float>(tab));
        if (i == 20) { pn.setSelectedVoice((tab + 3) % 16); }
        if (i == 30) setU(*p, "BD1:MUTE", (tab & 1) ? 1.0f : 0.0f);
        const juce::RectangleList<int> d = pn.tickForTest();
        if (!d.isEmpty()) { juce::Graphics g(inc); g.reduceClipRegion(d); pn.paintEntireComponent(g, true); }
        { juce::Graphics g(full); pn.paintEntireComponent(g, true); }
        const long n = diffPx(inc, full);
        if (n > 50 && std::getenv("SHOGUN_DUMP_REPAINT") != nullptr) {
          juce::PNGImageFormat png;
          for (auto [img, nm] : {std::pair<juce::Image*, const char*>{&inc, "inc"}, {&full, "full"}}) {
            juce::File f("/tmp/repaint_" + juce::String(nm) + ".png");
            f.deleteFile();
            juce::FileOutputStream os(f);
            png.writeImageToStream(*img, os);
          }
          std::cout << "  dumped; dirty " << d.getBounds().toString() << "\n";
          std::exit(3);
        }
        if (n > worst) { worst = n; where = "tab " + juce::String(tab) + " tick " + juce::String(i); }
        for (auto r : d) area += r.getWidth() * r.getHeight();
        ++frames;
      }
    }
    // press feedback on a MAIN step key, transport stopped and silent so nothing else changes
    p->requestRun(false);
    render(*p, 96000);
    pn.setTab(0);
    pn.setTestClockMs(1000.0);
    for (int i = 0; i < 40; ++i) { render(*p, 1600); pn.tickForTest(); }
    juce::Rectangle<float> key;
    pn.bindRect("step:5", key);
    const juce::Rectangle<int> keyPx =
        key.transformedBy(ShogunPanel::panelTransform()).getSmallestIntegerContainer().expanded(8);
    auto inside = [&](const juce::RectangleList<int>& d) {
      for (auto r : d)
        if (!keyPx.contains(r)) return false;
      return true;
    };
    pn.pressAt(key.getCentre());  // (repaints only what the press changed)
    const juce::RectangleList<int> d0 = pn.tickForTest();
    pn.releaseAt(key.getCentre());
    bool flashOk = pn.flashActive() && inside(d0);
    int flashFrames = 0;
    juce::String outside;
    for (double t : {1033.0, 1066.0, 1100.0, 1133.0, 1166.0}) {
      pn.setTestClockMs(t);
      const juce::RectangleList<int> d = pn.tickForTest();
      if (!d.isEmpty()) ++flashFrames;
      if (!inside(d)) { flashOk = false; outside = d.getBounds().toString(); }
    }
    flashOk = flashOk && !pn.flashActive() && pn.tickForTest().isEmpty();
    pn.setTestClockMs(-1.0);
    // The software renderer's anti-aliasing levels can differ by a step or two (of 255) on the pixel columns where a
    // clip edge cuts overlapping soft shapes (seen: 2, on GRID cell borders under the translucent playhead). A stale
    // or missing op differs by far more (the faintest dynamic layer, the playhead's 8% white, is ~15 levels), so
    // anything above 3 fails.
    check(maxCh <= 3 && timerOff && flashOk && flashFrames >= 3, "editor repaint",
          juce::String(frames) + " ticks on 8 tabs: incremental vs full repaint max " + juce::String(worst) +
              " px off by at most " + juce::String(maxCh) + "/255" + (worst ? " (" + where + ")" : juce::String()) + ", mean dirty area " +
              juce::String(100.0 * area / frames / (1200.0 * 672.0), 1) + "% of the panel; timer off while hidden " +
              (timerOff ? "yes" : "NO") + "; step key press/flash repaints only its rect " + (flashOk ? "yes" : "NO") +
              outside + ", flash frames " + juce::String(flashFrames));
  }

  // ---- ▶ PLAY in each SRC mode (item P). Before: with SRC HOST (the default) ▶ only set a flag the host clock
  // ignored, so pressing it with the host stopped played nothing. Now: HOST + host stopped = a preview at the host's
  // tempo that the host takes over when it plays; INT = start / stop; EXT = armed, waiting for clock.
  {
    auto p = fresh();
    p->setCurrentProgram(1);
    setU(*p, "CLOCK:SOURCE", 0.0f);  // HOST
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    FakeHead head;
    head.playing = false;
    head.bpm = 128.0;
    p->setPlayHead(&head);
    juce::String log;
    bool ok = render(*p, 9600).peakMain < 1e-6f;
    juce::Rectangle<float> runKey;
    ok = ok && pn.bindRect("run", runKey) && pn.tooltipAt(runKey.getCentre()).contains("preview");
    pn.pressBind("run");  // HOST, host stopped: preview
    const float prev = render(*p, 96000).peakMain;
    const bool prevOn = p->meters.running.load() && p->meters.previewing.load();
    head.playing = true;  // the host starts: it takes over
    const float hostPk = render(*p, 96000, nullptr, &head).peakMain;
    const bool tookOver = !p->meters.previewing.load() && p->meters.running.load();
    head.playing = false;  // the host stops: SHOGUN stops (no preview resumes by itself)
    render(*p, 96000);
    const int gs0 = p->meters.globalStep.load();
    render(*p, 48000);
    const bool stopped = !p->meters.running.load() && p->meters.globalStep.load() == gs0;  // the sequencer halts
    pn.pressBind("run");
    render(*p, 24000);
    pn.pressBind("run");  // ▶ again stops the preview
    render(*p, 96000);
    const bool prevStop = !p->meters.running.load() && !p->meters.previewing.load();
    ok = ok && prev > 0.05f && prevOn && hostPk > 0.05f && tookOver && stopped && prevStop;
    log << "HOST: preview peak " << juce::String(prev, 3) << (prevOn ? " (preview)" : " (NO preview)") << ", host takes over "
        << (tookOver ? "yes" : "NO") << ", stops with host " << (stopped ? "yes" : "NO") << ", ▶ stops preview "
        << (prevStop ? "yes" : "NO");
    p->setPlayHead(nullptr);
    setU(*p, "CLOCK:SOURCE", 0.5f);  // INT
    render(*p, 96000);
    const bool idle = !p->meters.running.load();
    pn.pressBind("run");
    const float intPk = render(*p, 96000).peakMain;
    const bool intOn = p->meters.running.load();
    pn.pressBind("run");
    render(*p, 96000);
    const bool intOff = !p->meters.running.load();
    ok = ok && idle && intPk > 0.05f && intOn && intOff;
    log << "; INT: peak " << juce::String(intPk, 3) << ", start/stop " << (intOn && intOff ? "yes" : "NO");
    setU(*p, "CLOCK:SOURCE", 1.0f);  // EXT, no clock patched
    render(*p, 96000);
    pn.pressBind("run");
    const float extPk = render(*p, 96000).peakMain;
    const bool armed = p->meters.running.load() && pn.tooltipAt(runKey.getCentre()).startsWith("Armed");
    ok = ok && armed && extPk < 1e-3f;
    log << "; EXT: armed " << (armed ? "yes" : "NO") << ", silent until clock " << (extPk < 1e-3f ? "yes" : "NO");
    check(ok, "PLAY in each SRC mode", log);
  }

  // ---- keys (items G / N / P): Cmd/Ctrl-Z undo, Cmd/Ctrl-Shift-Z and Cmd/Ctrl-Y redo; space toggles SHOGUN's ▶
  // when the host transport is stopped and is passed on (not consumed) while the host plays; other keys pass on.
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto* se = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get());
    const int id = findParam("BD1:DECAY");
    const float u0 = p->paramU(id);
    p->beginUndoStep();
    setU(*p, "BD1:DECAY", 0.9f);
    p->settleUndoStep();
    const float u1 = p->paramU(id);
    const juce::ModifierKeys cmd(juce::ModifierKeys::commandModifier),
        cmdShift(juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier);
    bool ok = !juce::approximatelyEqual(u0, u1);
    ok = ok && se->handleKey(juce::KeyPress('Z', cmd, 0)) && juce::approximatelyEqual(p->paramU(id), u0);
    ok = ok && se->handleKey(juce::KeyPress('Z', cmdShift, 0)) && juce::approximatelyEqual(p->paramU(id), u1);
    ok = ok && se->handleKey(juce::KeyPress('Z', cmd, 0)) && juce::approximatelyEqual(p->paramU(id), u0);
    ok = ok && se->handleKey(juce::KeyPress('Y', cmd, 0)) && juce::approximatelyEqual(p->paramU(id), u1);
    const bool undoOk = ok;
    FakeHead head;
    head.playing = true;
    p->setPlayHead(&head);
    render(*p, 4800, nullptr, &head);
    const bool passWhileHost = !se->handleKey(juce::KeyPress(juce::KeyPress::spaceKey));
    head.playing = false;
    render(*p, 4800);
    const bool consumed = se->handleKey(juce::KeyPress(juce::KeyPress::spaceKey));
    render(*p, 9600);
    const bool started = p->meters.running.load();
    se->handleKey(juce::KeyPress(juce::KeyPress::spaceKey));
    render(*p, 9600);
    const bool stoppedAgain = !p->meters.running.load();
    const bool othersPass = !se->handleKey(juce::KeyPress('A')) && !se->handleKey(juce::KeyPress(juce::KeyPress::returnKey)) &&
                            !se->handleKey(juce::KeyPress('S', cmd, 0));
    const bool focus = !se->getWantsKeyboardFocus() && !se->getMouseClickGrabsKeyboardFocus() &&
                       !se->panel().getWantsKeyboardFocus() && !se->panel().getMouseClickGrabsKeyboardFocus();
    p->setPlayHead(nullptr);
    check(undoOk && passWhileHost && consumed && started && stoppedAgain && othersPass && focus, "keys",
          juce::String("undo/redo via Cmd/Ctrl-Z, -Shift-Z, -Y ") + (undoOk ? "yes" : "NO") +
              "; space passed to the host while it plays " + (passWhileHost ? "yes" : "NO") +
              ", toggles PLAY when stopped " + (consumed && started && stoppedAgain ? "yes" : "NO") +
              "; other keys passed on " + (othersPass ? "yes" : "NO") + "; click never grabs focus " + (focus ? "yes" : "NO"));
  }

  // ---- host view moves / collapse-restore (item Q): the editor goes back to 0,0 in its host view and re-runs the
  // UI-scale layout path after a move, a constrainer re-centre (what a collapse did), hide / show and re-parenting.
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto* se = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get());
    juce::Component hostView;
    hostView.setSize(1200, 672);
    hostView.addAndMakeVisible(*ed);
    auto scaleOk = [&] {
      const float s = static_cast<float>(ed->getWidth()) / 1200.0f;
      return se->panel().getTransform() == juce::AffineTransform::scale(s);
    };
    bool ok = ed->getPosition() == juce::Point<int>();
    ed->setTopLeftPosition(300, 40);  // a host drag that moved the editor inside its view
    ok = ok && ed->getPosition() == juce::Point<int>() && scaleOk();
    ed->setBounds(300, 168, 600, 336);  // the fixed-aspect re-centre a collapse produced
    ok = ok && ed->getPosition() == juce::Point<int>() && ed->getWidth() == 600 && scaleOk();
    ed->setVisible(false);
    ed->setVisible(true);
    ok = ok && ed->getPosition() == juce::Point<int>();
    juce::Component other;
    other.setSize(1200, 672);
    other.addAndMakeVisible(*ed);  // re-parented (reopen)
    ok = ok && ed->getPosition() == juce::Point<int>() && scaleOk();
    ed->setSize(1200, 672);
    ok = ok && ed->getPosition() == juce::Point<int>() && scaleOk();
    other.removeChildComponent(ed.get());
    check(ok, "host view move / collapse", juce::String("editor back at 0,0 with the UI-scale layout after move, re-centre, hide/show, re-parent: ") +
                                             (ok ? "yes" : "NO"));
  }

  // ---- list controls (rule 3 + items D / H / O): on every tab, every control that steps through a list opens the
  // whole list on right-click with the current item ticked (all items, in order, as the engine shows them); picking
  // an item sets it; shift-click steps back; left-click steps forward (long lists, 10+ items, open the menu instead).
  // TEMPO / A4 readouts open their value menus on any click, LEN its length menu; no readout steps between 2 values.
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    pn.setCaptureMenus(true);
    int lists = 0, bad = 0, longs = 0, readouts = 0;
    juce::String firstBad, perTab;
    for (int tab = 0; tab < 8; ++tab) {
      pn.setTab(tab);
      pn.setSelectedVoice(0);
      std::map<juce::String, int> seen;
      int tabLists = 0;
      for (const juce::String& entry : pn.clickableBinds()) {
        if (entry.endsWith("|K")) continue;
        const juce::String bind = entry;
        const int nth = seen[bind]++;
        int pid = -1;
        if (bind.startsWith("p:") || bind.startsWith("disp:")) pid = findParam(bind.fromFirstOccurrenceOf(":", false, false).toRawUTF8());
        else if (bind.startsWith("sp:")) pid = findParam(("BD1:" + bind.substring(3)).toRawUTF8());
        else if (bind == "src") pid = P_CLOCK_SOURCE;
        else if (bind == "osbadge") pid = P_GLOBAL_OS;
        const bool tscale = bind.startsWith("tscale:");
        bool isList = tscale;
        int n = tscale ? 5 : 0;
        if (pid >= 0) {
          const ParamInfo& pi = kParams[pid];
          const bool named = pi.choices != nullptr && pi.choices[0] != 0 && std::strcmp(pi.choices, "OFF|ON") != 0;
          isList = pi.kind == ParamKind::Stepped || (pi.kind == ParamKind::Toggle && named);
          n = pi.steps;
          if (pi.kind == ParamKind::Continuous && !bind.startsWith("p:")) ++readouts;
          if (pi.kind == ParamKind::Continuous && (bind.startsWith("disp:") || pid == P_CLOCK_TEMPO || pid == P_GLOBAL_A4)) {
            const int m0 = pn.menusOpened();
            const float u0 = p->paramU(pid);
            pn.pressBind(bind.toRawUTF8(), false, false, nth);  // a left-click: a menu, never a 2-step jump
            const bool okR = pn.menusOpened() == m0 + 1 && juce::approximatelyEqual(p->paramU(pid), u0);
            if (!okR && bad++ == 0) firstBad = bind + " (readout)";
            continue;
          }
        }
        if (!isList || n < 2) continue;
        ++lists;
        ++tabLists;
        auto cur = [&] {
          if (tscale) return p->editPattern().tracks[bind.fromFirstOccurrenceOf(":", false, false).getIntValue()].scale + 1;
          return stepIndex(static_cast<double>(p->paramU(pid)), n);
        };
        const int c0 = cur();
        const int m0 = pn.menusOpened();
        pn.pressBind(bind.toRawUTF8(), true, false, nth);  // right-click: the list
        const auto& menu = pn.lastMenu();
        bool ok = pn.menusOpened() == m0 + 1 && menu.items.size() == n;
        int ticks = 0;
        for (int i = 0; ok && i < n; ++i) {
          if (menu.ticked[static_cast<size_t>(i)]) ++ticks;
          if (pid >= 0 && menu.items[i] != paramDisplay(pid, static_cast<float>(stepU(i, n)))) ok = false;
        }
        ok = ok && ticks == 1 && menu.ticked[static_cast<size_t>(c0)];
        const int target = (c0 + 2) % n;
        ok = ok && pn.pickMenuItem(menu.items[target]) && cur() == target;
        pn.pressBind(bind.toRawUTF8(), false, true, nth);  // shift-click: back one
        ok = ok && cur() == (target + n - 1) % n;
        const int m1 = pn.menusOpened();
        pn.pressBind(bind.toRawUTF8(), false, false, nth);  // left-click: forward, or the menu for a long list
        if (n >= 10) {
          ++longs;
          ok = ok && pn.menusOpened() == m1 + 1 && cur() == (target + n - 1) % n;
        } else {
          ok = ok && pn.menusOpened() == m1 && cur() == target;
        }
        if (!ok && bad++ == 0) firstBad = "tab " + juce::String(tab) + " " + bind;
      }
      perTab << (tab ? " " : "") << tabLists;
    }
    // matrix CURVE (MOD tab) and the readout menus
    pn.setTab(3);
    p->editRows()[0] = {mod::SRC_LFO1, -1, findParam("BD1:DECAY"), 0.5, mod::SRC_NONE, -1, mod::LIN, true};
    p->commitEdits();
    const juce::Point<float> curve(620.0f, 366.0f + 11.0f);
    pn.clickAt(curve, true);
    bool curveOk = pn.lastMenu().items.size() == mod::kCurveCount && pn.lastMenu().ticked[0] &&
                   pn.pickMenuItem(pn.lastMenu().items[2]) && p->editRows()[0].curve == 2;
    pn.setTab(0);
    pn.pressBind("disp:CLOCK:TEMPO");
    const auto tm = pn.lastMenu();
    bool tempoOk = tm.items.size() == 17 && tm.items.contains("174 BPM") && tm.items.contains(juce::String::fromUTF8("Type tempo\xE2\x80\xA6")) &&
                   tm.items.contains("Sync to DAW");
    tempoOk = tempoOk && pn.pickMenuItem("128 BPM") && std::fabs(tempoBpm(static_cast<double>(p->paramU(P_CLOCK_TEMPO))) - 128.0) < 0.01;
    pn.pressBind("disp:CLOCK:TEMPO");
    tempoOk = tempoOk && pn.lastMenu().ticked[static_cast<size_t>(pn.lastMenu().items.indexOf("128 BPM"))];
    tempoOk = tempoOk && pn.pickMenuItem(juce::String::fromUTF8("Type tempo\xE2\x80\xA6")) && pn.typeForTest(97.0) &&
              std::fabs(tempoBpm(static_cast<double>(p->paramU(P_CLOCK_TEMPO))) - 97.0) < 0.01;
    pn.pressBind("disp:CLOCK:TEMPO");
    tempoOk = tempoOk && pn.pickMenuItem("Sync to DAW") && stepIndex(static_cast<double>(p->paramU(P_CLOCK_SOURCE)), 3) == SRC_HOST;
    pn.pressBind("disp:CLOCK:TEMPO");
    tempoOk = tempoOk && pn.lastMenu().ticked.back();
    pn.setTab(7);
    bool a4Ok = pn.pressBind("disp:GLOBAL:A4") || pn.pressBind("p:GLOBAL:A4");
    a4Ok = a4Ok && pn.lastMenu().items.size() == 7 && pn.pickMenuItem("432 Hz") &&
           std::fabs(a4Hz(static_cast<double>(p->paramU(P_GLOBAL_A4))) - 432.0) < 0.01;
    pn.setTab(2);
    juce::Rectangle<float> lenR;
    bool lenOk = pn.bindRect("len:3", lenR);
    pn.clickAt(lenR.getCentre());
    lenOk = lenOk && pn.lastMenu().items.size() == kMaxSteps + 2 && pn.lastMenu().ticked[static_cast<size_t>(p->editPattern().tracks[3].len - 1)];
    lenOk = lenOk && pn.pickMenuItem("12 steps") && p->editPattern().tracks[3].len == 12;
    pn.clickAt(lenR.getCentre());
    lenOk = lenOk && pn.pickMenuItem("Apply to all tracks");
    for (const auto& tr : p->editPattern().tracks) lenOk = lenOk && tr.len == 12;
    pn.pressBind("len:3", false, true);
    lenOk = lenOk && p->editPattern().tracks[3].len == 11;
    pn.clickAt(lenR.getCentre());
    lenOk = lenOk && pn.pickMenuItem(juce::String::fromUTF8("Type length\xE2\x80\xA6")) && pn.typeForTest(7) && p->editPattern().tracks[3].len == 7;
    check(lists > 0 && bad == 0 && curveOk && tempoOk && a4Ok && lenOk, "list controls",
          juce::String(lists) + " list controls (per tab " + perTab + "; " + juce::String(longs) + " long lists open on left-click), " +
              juce::String(bad) + " bad" + (bad ? " (first " + firstBad + ")" : juce::String()) + "; matrix CURVE " +
              (curveOk ? "yes" : "NO") + "; TEMPO menu " + (tempoOk ? "yes" : "NO") + "; A4 menu " + (a4Ok ? "yes" : "NO") +
              "; LEN menu " + (lenOk ? "yes" : "NO"));
  }

  // ---- step past the track length (E): on MAIN and GRID the click turns the step on and the track grows to it.
  // MAIN activity lights (F): one per voice under its name key.
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    Pattern& pat = p->editPattern();
    pat.tracks[0].len = 8;
    pat.tracks[0].steps[11] = Step();
    p->commitEdits();
    pn.setTab(0);
    pn.setSelectedVoice(0);
    pn.pressBind("step:11");
    const bool mainOk = pat.tracks[0].steps[11].on && pat.tracks[0].len == 12;
    pat.tracks[2].len = 16;
    pat.tracks[2].steps[20] = Step();
    p->commitEdits();
    pn.setTab(2);
    pn.pressBind("grid:2:20");
    const bool gridOk = pat.tracks[2].steps[20].on && pat.tracks[2].len == 21;
    pn.pressBind("grid:2:5");  // inside the length: the usual cycle, the length stays
    const bool inside = pat.tracks[2].len == 21;
    check(mainOk && gridOk && inside, "step past length",
          juce::String("MAIN step 12 of an 8-step track: on, length 12 ") + (mainOk ? "yes" : "NO") +
              "; GRID step 21 of a 16-step track: on, length 21 " + (gridOk ? "yes" : "NO"));
    pn.setTab(0);
    int leds = 0;
    for (int v = 0; v < kVoices; ++v) {
      juce::Rectangle<float> key, led;
      const juce::String name(kVoiceNames[v]);
      if (pn.bindRect(("sel:" + name).toRawUTF8(), key) && pn.bindRect(("act:" + name).toRawUTF8(), led) &&
          std::fabs(led.getX() - key.getCentreX()) < 0.01f && led.getY() > key.getBottom() && led.getY() < key.getBottom() + 10.0f)
        ++leds;
    }
    check(leds == kVoices, "MAIN activity lights", juce::String(leds) + "/16 under their name keys");
  }

  // ---- ⌕ program search (J): the popup filters the 22 programs by name; Enter loads the first match.
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    pn.setCaptureMenus(true);
    const int m0 = pn.menusOpened();
    pn.pressBind("browse");
    const bool opens = pn.menusOpened() == m0 + 1;
    ProgramSearch s(*p);
    const int all = s.matches();
    s.setQuery("metal");
    const int metal = s.matches();
    const bool loaded = s.pressReturn() && juce::String::fromUTF8(factory::programName(p->getCurrentProgram())).containsIgnoreCase("metal");
    s.setQuery("zzzz");
    const bool none = s.matches() == 0 && !s.pressReturn();
    s.setQuery("DUB");
    const bool caseless = s.matches() >= 1;
    check(opens && all == factory::kPrograms && metal >= 1 && loaded && none && caseless, "program search",
          juce::String("⌕ opens the search ") + (opens ? "yes" : "NO") + "; " + juce::String(all) + " listed, 'metal' -> " +
              juce::String(metal) + ", Enter loads " + juce::String::fromUTF8(factory::programName(p->getCurrentProgram())) +
              "; no match = nothing loads " + (none ? "yes" : "NO"));
  }

  // ---- item K: help / description text at 9 pt at least, a 0.5 s hover zoom that repaints only its bubble, no
  // overlap or frame crossing at the new size, and popup menus in the plain face (not the LCD face).
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    int lines = 0;
    float minPt = 0.0f;
    const auto bad = pn.helpAudit(lines, minPt);
    if (const char* f = std::getenv("SHOGUN_HELP_RECTS")) {  // the areas the label pixel diff may differ in
      juce::String s;
      for (const auto& a : pn.helpAreas())
        s << a.first << " " << a.second.getX() << " " << a.second.getY() << " " << a.second.getWidth() << " " << a.second.getHeight() << "\n";
      juce::File(juce::String::fromUTF8(f)).replaceWithText(s);
    }
    pn.setTab(3);
    int op = -1;
    for (int y = 540; y < 620 && op < 0; ++y) op = pn.helpOpAt({812.0f + 40.0f, static_cast<float>(y)});
    const juce::Rectangle<float> line = op >= 0 ? pn.helpRect(op) : juce::Rectangle<float>();
    pn.setTestClockMs(1000.0);
    pn.tickForTest();
    pn.hoverAt(line.getCentre());
    pn.setTestClockMs(1400.0);
    const bool early = pn.tickForTest().isEmpty() && pn.zoomOp() < 0;
    pn.setTestClockMs(1501.0);
    const auto d = pn.tickForTest();
    const juce::Rectangle<int> bubble = op >= 0 ? pn.zoomDirtyRect(op) : juce::Rectangle<int>();
    const int zoomedOp = pn.zoomOp();
    const bool zoomed = zoomedOp == op && op >= 0 && !d.isEmpty() && bubble.contains(d.getBounds());
    pn.hoverAt({600.0f, 5.0f});
    const bool cleared = pn.zoomOp() < 0;
    pn.setTestClockMs(-1.0);
    const juce::Font mf = pn.getLookAndFeel().getPopupMenuFont();
    const bool plain = !mf.getTypefaceName().containsIgnoreCase("mono") && mf.getHeight() >= 12.0f;
    check(lines >= 25 && minPt >= 9.0f && bad.empty() && early && zoomed && cleared && plain, "help text",
          juce::String(lines) + " help lines, smallest " + juce::String(minPt, 1) + " pt, overlaps / off-front / frame crossings " +
              juce::String(static_cast<int>(bad.size())) + (bad.empty() ? juce::String() : " (" + juce::StringArray(bad.data(), static_cast<int>(bad.size())).joinIntoString(" | ") + ")") +
              "; hover zoom not before 0.5 s " + (early ? "yes" : "NO") + ", at 0.5 s repaints only its bubble " +
              (zoomed ? "yes" : "NO [dirty " + d.getBounds().toString() + " bubble " + bubble.toString() + " op " + juce::String(op) + "/" + juce::String(zoomedOp) + "]") + ", gone on leaving " + (cleared ? "yes" : "NO") + "; menu face " + mf.getTypefaceName() +
              " " + juce::String(mf.getHeight(), 1));
  }

  // ---- item K screenshots (only with SHOGUN_K_SHOTS=<dir>): the editor at 100 % on a 1366 × 768 screen, a tab with
  // help text and an open popup menu drawn by the look-and-feel (the same calls a real menu window makes). "before" is
  // the panel with help text at its old laid-out size and JUCE's default look-and-feel (as before item K; the panel
  // part is checked pixel-equal to the pre-K tab renders), "after" the current panel and SHOGUN's look-and-feel.
  if (const char* kdir = std::getenv("SHOGUN_K_SHOTS")) {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    pn.setCaptureMenus(true);
    const juce::File dir(juce::String::fromUTF8(kdir));
    auto save = [&](const juce::Image& img, const juce::String& name) {
      const juce::File f = dir.getChildFile(name);
      f.getParentDirectory().createDirectory();
      f.deleteFile();
      juce::FileOutputStream os(f);
      juce::PNGImageFormat png;
      png.writeImageToStream(img, os);
      std::printf("K shot %s\n", f.getFullPathName().toRawUTF8());
    };
    auto panelImage = [&]() {
      juce::Image img(juce::Image::RGB, 1200, 672, true);
      juce::Graphics pg(img);
      pn.paintEntireComponent(pg, true);
      return img;
    };
    struct Shot { int tab; const char* bind; const char* name; };
    for (const Shot& s : {Shot{3, "disp:CLOCK:TEMPO", "k_mod_tempo_menu"}, Shot{7, "src", "k_global_src_menu"}, Shot{2, "len:0", "k_grid_len_menu"}}) {
      pn.setTab(s.tab);
      pn.setLegacyTextForTest(true);
      const juce::Image before = panelImage();
      save(before, juce::String("before/") + s.name + "_panel.png");
      pn.setLegacyTextForTest(false);
      if (s.tab == 7) {  // a help line held under the mouse past 0.5 s: its zoom bubble (after only)
        const int hop = pn.helpOpAt({60.0f, 556.0f});
        pn.setTestClockMs(0.0);
        if (hop >= 0) pn.hoverAt(pn.helpRect(hop).getCentre());
        pn.setTestClockMs(600.0);
        pn.tickForTest();
      }
      const juce::Image after = panelImage();
      pn.hoverAt({600.0f, 2.0f});
      pn.setTestClockMs(-1.0);
      pn.pressBind(s.bind, true);  // right-click: the full list as a menu
      const auto m = pn.lastMenu();
      pn.releaseAt({});
      juce::Rectangle<float> anchor;
      pn.bindRect(s.bind, anchor);
      for (int phase = 0; phase < 2; ++phase) {
        juce::LookAndFeel& lf = phase == 0 ? juce::LookAndFeel::getDefaultLookAndFeel() : pn.getLookAndFeel();
        juce::Image img(juce::Image::RGB, 1366, 768, true);
        juce::Graphics g(img);
        g.fillAll(juce::Colour(0xFF3A3D42));
        const int ox = (1366 - 1200) / 2, oy = 48;
        g.drawImageAt(phase == 0 ? before : after, ox, oy);
        int w = 0, total = 0;
        std::vector<int> hs;
        for (const auto& it : m.items) {
          int iw = 0, ih = 0;
          lf.getIdealPopupMenuItemSize(it, false, -1, iw, ih);
          w = std::max(w, iw);
          hs.push_back(ih);
          total += ih;
        }
        w += 24;
        g.saveState();
        g.setOrigin(ox + static_cast<int>(anchor.getX()), oy + static_cast<int>(anchor.getBottom()) + 2);
        g.reduceClipRegion(0, 0, w, total + 8);  // the look-and-feel fills its whole clip, as in a real menu window
        lf.drawPopupMenuBackground(g, w, total + 8);
        int y = 4;
        for (int k = 0; k < m.items.size(); ++k) {
          const auto ks = static_cast<size_t>(k);
          lf.drawPopupMenuItem(g, {0, y, w, hs[ks]}, false, true, k == 2, m.ticked[ks], false, m.items[k], {}, nullptr, nullptr);
          y += hs[ks];
        }
        g.restoreState();
        save(img, juce::String(phase == 0 ? "before/" : "after/") + s.name + ".png");
      }
    }
  }

  // ---- MIDI out (item 2b): every played step leaves on the plugin's MIDI out at its exact sample (drums ch 10 note
  // 36 + voice, LEAD ch 1 / BASS ch 2 at the step's note; velocity 70 / 100 / 127 from the accent), each with its
  // note-off. A test pattern at 120 BPM (a 1/16 step = 6000 samples at 48 kHz) with an accent, a ratchet, micro-timing
  // and a synth line, checked against the step times.
  {
    auto p = fresh();
    p->setCurrentProgram(0);
    setU(*p, "CLOCK:SOURCE", 0.5f);
    setU(*p, "CLOCK:TEMPO", static_cast<float>((120.0 - 40.0) / 160.0));
    setU(*p, "CLOCK:SWING", 0.0f);
    Pattern& pat = p->editPattern();
    for (auto& tr : pat.tracks) { tr.len = 16; for (auto& st : tr.steps) st = Step(); }
    for (int s : {0, 4, 8, 12}) pat.tracks[BD1].steps[s].on = true;
    pat.tracks[BD1].steps[2].on = true;
    pat.tracks[BD1].steps[2].acc = 3;
    pat.tracks[BD1].steps[2].ratchet = 2;
    pat.tracks[SD].steps[4].on = true;
    pat.tracks[SD].steps[4].micro = 0.25f;
    pat.tracks[SD].steps[4].acc = 1;
    pat.tracks[LEAD].steps[0].on = true;
    pat.tracks[LEAD].steps[0].note = 60;
    pat.tracks[LEAD].steps[6].on = true;
    pat.tracks[LEAD].steps[6].note = 67;
    p->commitEdits();
    render(*p, 4800);
    p->requestRun(true);
    struct Ev { std::int64_t at; int ch, note, vel; bool on; };
    std::vector<Ev> evs;
    std::int64_t pos = 0;
    juce::AudioBuffer<float> buf(p->getTotalNumOutputChannels(), 512);
    for (int b = 0; b < 200; ++b) {  // ~2.1 s: a 16-step bar (96000 samples) and a bit
      buf.clear();
      juce::MidiBuffer m;
      p->processBlock(buf, m);
      for (const auto meta : m) {
        const auto msg = meta.getMessage();
        if (msg.isNoteOnOrOff()) evs.push_back({pos + meta.samplePosition, msg.getChannel(), msg.getNoteNumber(), msg.getVelocity(), msg.isNoteOn()});
      }
      pos += 512;
    }
    p->requestRun(false);
    { juce::MidiBuffer m; buf.clear(); p->processBlock(buf, m); for (const auto meta : m) { const auto msg = meta.getMessage(); if (msg.isNoteOff()) evs.push_back({pos + meta.samplePosition, msg.getChannel(), msg.getNoteNumber(), 0, false}); } }
    std::int64_t s0 = -1;
    for (const auto& e : evs) if (e.on && e.ch == 10 && e.note == 36) { s0 = e.at; break; }
    // expected note-ons in the first bar: (sample offset from step 0, ch, note, vel)
    struct Exp { std::int64_t at; int ch, note, vel; };
    const std::vector<Exp> want = {{0, 10, 36, 100}, {12000, 10, 36, 127}, {15000, 10, 36, 127}, {24000, 10, 36, 100},
                                   {48000, 10, 36, 100}, {72000, 10, 36, 100}, {25500, 10, 38, 70},
                                   {0, 1, 60, 100}, {36000, 1, 67, 100}};
    int found = 0;
    juce::String miss;
    for (const auto& w : want) {
      bool hit = false;
      for (const auto& e : evs) hit = hit || (e.on && e.ch == w.ch && e.note == w.note && e.vel == w.vel && e.at == s0 + w.at);
      if (hit) ++found; else miss << " ch" << w.ch << "/" << w.note << "@" << juce::String(static_cast<juce::int64>(w.at));
    }
    int ons = 0, offs = 0;
    bool paired = true;
    for (size_t i = 0; i < evs.size(); ++i) {
      if (!evs[i].on) { ++offs; continue; }
      ++ons;
      bool off = false;
      for (size_t j = i + 1; j < evs.size() && !off; ++j) off = !evs[j].on && evs[j].ch == evs[i].ch && evs[j].note == evs[i].note && evs[j].at >= evs[i].at;
      paired = paired && off;
    }
    int firstBar = 0;
    for (const auto& e : evs) if (e.on && s0 >= 0 && e.at - s0 < 96000) ++firstBar;
    check(s0 >= 0 && found == static_cast<int>(want.size()) && firstBar == static_cast<int>(want.size()) && paired && ons == offs,
          "MIDI out", juce::String(found) + "/" + juce::String(static_cast<int>(want.size())) + " note-ons at their exact step samples" +
                          (miss.isEmpty() ? juce::String() : " (missing" + miss + ")") + ", " + juce::String(firstBar) +
                          " in bar 1; " + juce::String(ons) + " ons / " + juce::String(offs) + " offs, every on has its off " +
                          (paired ? "yes" : "NO") + "; producesMidi " + (p->producesMidi() ? "yes" : "NO"));
  }

  // ---- MIDI drag-out (item 2a): the GRID handle's file for a factory pattern: one cycle, every lane on its note,
  // velocity from the accent, ratchets / flams as repeated notes, each note-on with its note-off.
  {
    auto p = fresh();
    p->setCurrentProgram(2);
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    pn.setTab(2);
    juce::Rectangle<float> handle;
    const bool hasHandle = pn.bindRect("mididrag", handle);
    const juce::File f = pn.exportPatternMidi();
    juce::MidiFile mf;
    bool read = false;
    { juce::FileInputStream in(f); read = in.openedOk() && mf.readFrom(in); }
    const Pattern& pat = p->editPattern();
    const int gScale = stepIndex(static_cast<double>(p->paramU(P_CLOCK_SCALE)), 4);
    const double cycle = shogun::midiexport::cycleQuarters(pat, gScale) * 96.0;
    // expected note-ons per (channel, note) from the pattern data
    std::map<std::pair<int, int>, int> want, got;
    std::map<std::pair<int, int>, int> wantVelSum, gotVelSum;
    for (int t = 0; t < 16; ++t) {
      const Track& tr = pat.tracks[t];
      const double stepT = 96.0 / stepsPerQuarter(tr.scale < 0 ? gScale : tr.scale);
      const long steps = static_cast<long>(std::floor(cycle / stepT + 1e-9));
      for (long k = 0; k < steps; ++k) {
        const Step& st = tr.steps[k % std::clamp(tr.len, 1, kMaxSteps)];
        if (!st.on) continue;
        if (t == LEAD || t == BASS) {
          const Step& pv = tr.steps[(k + tr.len - 1) % tr.len];
          if (st.tie && k > 0 && pv.on && pv.note == st.note) continue;
          ++want[{t == LEAD ? 1 : 2, st.note}];
          wantVelSum[{t == LEAD ? 1 : 2, st.note}] += shogun::midiexport::velocityFor(st.acc);
          continue;
        }
        int hits = 1;
        if (st.flam > 0 && t != CP) hits = 2 + (((st.flam - 1) & 15) % 4);
        else for (int x : kRatchets) if (x == st.ratchet) hits = x;
        want[{10, 36 + t}] += hits;
        wantVelSum[{10, 36 + t}] += hits * shogun::midiexport::velocityFor(st.acc);
      }
    }
    int ons = 0, offs = 0;
    bool paired = true;
    if (read && mf.getNumTracks() == 1) {
      auto seq = *mf.getTrack(0);
      seq.updateMatchedPairs();
      for (int i = 0; i < seq.getNumEvents(); ++i) {
        const auto& m = seq.getEventPointer(i)->message;
        if (m.isNoteOn()) {
          ++ons;
          ++got[{m.getChannel(), m.getNoteNumber()}];
          gotVelSum[{m.getChannel(), m.getNoteNumber()}] += m.getVelocity();
          paired = paired && seq.getEventPointer(i)->noteOffObject != nullptr;
        } else if (m.isNoteOff()) {
          ++offs;
        }
      }
    }
    const bool match = want == got && wantVelSum == gotVelSum && ons > 0;
    check(hasHandle && read && match && paired && ons == offs, "MIDI drag-out",
          juce::String::fromUTF8(factory::programName(2)) + ": " + f.getFileName() + ", " + juce::String(ons) + " notes on " +
              juce::String(static_cast<int>(got.size())) + " lane notes over " + juce::String(cycle / 96.0, 0) +
              " beats; per-lane counts and accents match the pattern " + (match ? "yes" : "NO") + ", note-offs " +
              (paired && ons == offs ? "yes" : "NO") + "; GRID handle " + (hasHandle ? "yes" : "NO"));
    f.deleteFile();
  }

  // ---- MOD tab matrix past ten rows: all 32 slots in use; the view shows 10 and scrolls (▲ ▼ keys = a page, wheel =
  // a row). Every slot is reached by scrolling and edited through the panel (CURVE, ON, DEPTH drag), then rows are
  // removed with ✕ (one only reachable after scrolling) and the "+ add" row is reachable at the end of the list.
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    pn.setTab(3);
    const int dst = findParam("BD1:DECAY");
    for (int r = 0; r < mod::kRows; ++r)
      p->editRows()[r] = {mod::SRC_LFO1 + r % 4, -1, dst, 0.01 * (r + 1), mod::SRC_NONE, -1, mod::LIN, true};
    p->commitEdits();
    constexpr float y0 = 366.0f, rh = 28.0f;
    auto rowY = [&](int i) { return juce::Point<float>(0.0f, y0 + rh * static_cast<float>(i) + 11.0f); };
    auto at = [&](float x, int i) { return juce::Point<float>(x, rowY(i).y); };
    const juce::Point<float> pageDown = at(768.0f, 9), pageUp = at(768.0f, 0), over = at(250.0f, 4);
    bool ok = pn.matrixEntryCount() == 32 && pn.matrixTop() == 0;
    // ▼ ▼ ▼ then ▲ ▲ ▲: pages 0 → 10 → 20 → 22 (clamped: the last ten rows) and back 12 → 2 → 0.
    juce::String pages = "0";
    for (int k = 0; k < 3; ++k) { ok = ok && pn.clickAt(pageDown); pages << " " << pn.matrixTop(); }
    for (int k = 0; k < 3; ++k) { ok = ok && pn.clickAt(pageUp); pages << " " << pn.matrixTop(); }
    ok = ok && pages == "0 10 20 22 12 2 0";
    // Every slot: scroll it into view with the wheel, then CURVE, ON and DEPTH on its row; nothing else changes.
    int reached = 0, edited = 0, wheelSteps = 0;
    for (int s = 0; s < mod::kRows; ++s) {
      while (s >= pn.matrixTop() + 10 && wheelSteps < 100) { pn.wheelAt(over, -1.0f); ++wheelSteps; }
      const int i = s - pn.matrixTop();
      if (i < 0 || i >= 10) continue;
      ++reached;
      mod::Row before[mod::kRows];
      std::copy(p->editRows(), p->editRows() + mod::kRows, before);
      pn.clickAt(at(620.0f, i));       // CURVE: LIN → EXP
      pn.clickAt(at(680.0f, i));       // ON: off
      pn.dragMatrixDepth(at(400.0f, i), 33.0f);  // DEPTH: drag right
      bool rowOk = true;
      for (int r = 0; r < mod::kRows; ++r) {
        const mod::Row& a = p->editRows()[r];
        if (r == s) rowOk = rowOk && a.curve == mod::EXP && !a.on && a.depth > before[r].depth + 0.4;
        else rowOk = rowOk && a.curve == before[r].curve && a.on == before[r].on && dsp::exactEq(a.depth, before[r].depth);
      }
      edited += rowOk ? 1 : 0;
    }
    ok = ok && reached == 32 && edited == 32 && pn.matrixTop() == 22 && wheelSteps == 22;
    pn.wheelAt(over, -1.0f);  // past the end: stays on the last ten rows
    ok = ok && pn.matrixTop() == 22;
    // Render the scrolled view (rows 23-32) for the docs/review.
    {
      juce::Image img(juce::Image::RGB, 1200, 672, true);
      {
        juce::Graphics g(img);
        pn.paintEntireComponent(g, true);
      }
      const juce::File f = outDir.getChildFile("demo_3_mod_32rows.png");
      f.deleteFile();
      juce::FileOutputStream os(f);
      juce::PNGImageFormat png;
      if (os.openedOk()) png.writeImageToStream(img, os);
    }
    // ✕ on slot 32 (last visible row) and slot 21 (row 0 of the 22.. view), then page up and ✕ slot 6.
    ok = ok && pn.clickAt(at(735.0f, 9)) && p->editRows()[31].src == mod::SRC_NONE;
    const int t1 = pn.matrixTop();  // 32 entries again (31 used + "+ add"): view stays at 22
    const int i21 = 20 - t1;
    ok = ok && t1 == 22 && i21 < 0;  // slot 21 is above the view: scroll one row up to reach it
    pn.wheelAt(over, 1.0f);
    pn.wheelAt(over, 1.0f);
    ok = ok && pn.matrixTop() == 20 && pn.clickAt(at(735.0f, 0)) && p->editRows()[20].src == mod::SRC_NONE;
    pn.clickAt(pageUp);
    pn.clickAt(pageUp);
    ok = ok && pn.matrixTop() == 0 && pn.clickAt(at(735.0f, 5)) && p->editRows()[5].src == mod::SRC_NONE;
    int used = 0;
    for (int r = 0; r < mod::kRows; ++r) used += p->editRows()[r].src != mod::SRC_NONE ? 1 : 0;
    // 29 used + "+ add" (the first free slot, 6) = 30 entries; the last page ends on "+ add".
    for (int k = 0; k < 5; ++k) pn.clickAt(pageDown);
    ok = ok && used == 29 && pn.matrixEntryCount() == 30 && pn.matrixTop() == 20;
    // Remove the rest through the panel: ✕ on whatever row 0 shows, scrolled to the top.
    int removed = 3;
    for (int k = 0; k < 40 && pn.matrixEntryCount() > 1; ++k) {
      pn.clickAt(pageUp);
      pn.clickAt(pageUp);
      pn.clickAt(pageUp);
      if (pn.clickAt(at(735.0f, 0))) ++removed;
    }
    used = 0;
    for (int r = 0; r < mod::kRows; ++r) used += p->editRows()[r].src != mod::SRC_NONE ? 1 : 0;
    ok = ok && used == 0 && removed == 32 && pn.matrixEntryCount() == 1 && pn.matrixTop() == 0;
    // Eleven rows: the list is 12 entries, the view scrolls by two and ends on "+ add".
    for (int r = 0; r < 11; ++r)
      p->editRows()[r] = {mod::SRC_LFO1, -1, dst, 0.5, mod::SRC_NONE, -1, mod::LIN, true};
    p->commitEdits();
    for (int k = 0; k < 5; ++k) pn.wheelAt(over, -1.0f);
    ok = ok && pn.matrixEntryCount() == 12 && pn.matrixTop() == 2;
    pn.clickAt(at(620.0f, 8));  // row 8 of the view = slot 11
    ok = ok && p->editRows()[10].curve == mod::EXP;
    check(ok, "matrix 32 rows", "pages " + pages + "; " + juce::String(reached) + "/32 reached by wheel, " +
                                    juce::String(edited) + " edited (CURVE/ON/DEPTH), " + juce::String(removed) +
                                    " removed with X; 11 rows scroll to " + juce::String(pn.matrixTop()));
  }

  // ---- header: KIT / PATTERN ◀ ▶ step through the bank (INIT + kits) with wraparound; SRC cycles CLOCK:SOURCE
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    const int n = factory::kPrograms;
    bool ok = p->getCurrentProgram() == 0;
    int visited = 0;
    for (int tab = 0; tab < 8; ++tab) {  // the header is on every tab
      pn.setTab(tab);
      for (int which = 0; which < 2; ++which) {  // 0 = KIT ▶ ◀, 1 = PATTERN ▶ ◀
        for (int i = 1; i <= n; ++i) {
          ok = ok && pn.pressBind("prog:1", false, false, which) && p->getCurrentProgram() == i % n;
          ok = ok && pn.boundText("kit") == juce::String::fromUTF8(factory::programName(i % n)).toUpperCase();
          ok = ok && pn.boundText("pattern") == juce::String(p->editPattern().name);
          ++visited;
        }
        ok = ok && pn.pressBind("prog:-1", false, false, which) && p->getCurrentProgram() == n - 1;  // INIT ◀ = last
        ok = ok && pn.pressBind("prog:1", false, false, which) && p->getCurrentProgram() == 0;       // last ▶ = INIT
      }
    }
    pn.setTab(0);
    pn.pressBind("prog:1");
    pn.pressBind("prog:1");
    const bool loaded = p->getCurrentProgram() == 2 && p->stateJson() == [&] {
      auto q = fresh();
      q->setCurrentProgram(2);
      return q->stateJson();
    }();
    check(ok && loaded && visited == 8 * 2 * n, "KIT/PATTERN arrows",
          juce::String(visited) + " steps over " + juce::String(n) + " programs on 8 tabs, both arrow pairs wrap INIT <-> " +
              juce::String::fromUTF8(factory::programName(n - 1)) + ", ▶▶ = program 2 state");

    // SRC: HOST → INT → EXT → HOST (from the instance's current SRC), labelled with the source; right-click steps back.
    // The engine follows it: the host tempo under HOST, the TEMPO knob otherwise.
    FakeHead head;
    head.bpm = 100.0;
    p->setPlayHead(&head);
    setU(*p, "CLOCK:TEMPO", 0.5f);
    const juce::StringArray order("SRC HOST", "SRC INT", "SRC EXT");
    juce::StringArray labels;
    bool srcOk = true;
    juce::String tempos;
    for (int i = 0; i < 4; ++i) {
      const juce::String l = pn.boundText("src");
      const int k = stepIndex(static_cast<double>(p->paramU(P_CLOCK_SOURCE)), 3);
      render(*p, 512, nullptr, &head);
      const double bpm = p->engine().tempo();
      tempos << (i ? " / " : "") << juce::String(bpm, 1);
      srcOk = srcOk && l == order[k] && std::fabs(bpm - (k == 0 ? 100.0 : tempoBpm(0.5))) < 1e-9;
      if (i > 0) srcOk = srcOk && order.indexOf(l) == (order.indexOf(labels[i - 1]) + 1) % 3;
      labels.add(l);
      pn.pressBind("src");
    }
    const juce::String before = pn.boundText("src");
    pn.pressBind("src", false, true);  // shift-click steps back (right-click opens the list)
    const juce::String back = pn.boundText("src");
    srcOk = srcOk && order.indexOf(back) == (order.indexOf(before) + 2) % 3;
    check(srcOk, "SRC key", labels.joinIntoString(" > ") + ", shift-click " + before + " > " + back + "; engine tempo " +
                               tempos + " BPM (host 100, knob " + juce::String(tempoBpm(0.5), 1) + ")");
    p->setPlayHead(nullptr);
  }

  // ---- SRC is an instance setting (as on the rack): loading any program (the host's program change, the header ◀ ▶,
  // INIT PATCH) and an A/B recall keep HOST / INT / EXT. The instance's own state still carries it: undo of the SRC
  // key and the host's save/restore bring it back.
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    pn.setTab(0);
    const int n = factory::kPrograms;
    const char* const names[3] = {"HOST", "INT", "EXT"};
    auto srcIdx = [&] { return stepIndex(static_cast<double>(p->paramU(P_CLOCK_SOURCE)), 3); };
    bool ok = true;
    int loads = 0;
    juce::String per;
    for (int k = 0; k < 3; ++k) {
      p->setParamU(P_CLOCK_SOURCE, static_cast<float>(stepU(k, 3)));
      bool kOk = srcIdx() == k;
      for (int i = 0; i < n; ++i) {  // host program change, every program
        p->setCurrentProgram(i);
        kOk = kOk && p->getCurrentProgram() == i && srcIdx() == k;
        ++loads;
      }
      for (int i = 0; i < n; ++i) {  // header KIT ▶ through the bank (wraps to INIT)
        kOk = kOk && pn.pressBind("prog:1") && srcIdx() == k;
        ++loads;
      }
      kOk = kOk && pn.pressBind("prog:-1") && srcIdx() == k;
      p->setCurrentProgram(7);
      pn.setTab(7);
      kOk = kOk && pn.pressBind("initpatch") && p->getCurrentProgram() == 0 && srcIdx() == k;  // INIT PATCH
      pn.setTab(0);
      // a program loaded under this SRC is the INT load of the same program in everything but SRC
      auto q = fresh();
      q->setCurrentProgram(9);
      p->setCurrentProgram(9);
      juce::var a = juce::JSON::parse(p->stateJson()), b = juce::JSON::parse(q->stateJson());
      a["params"].getDynamicObject()->removeProperty("CLOCK:SOURCE");
      b["params"].getDynamicObject()->removeProperty("CLOCK:SOURCE");
      kOk = kOk && juce::JSON::toString(a) == juce::JSON::toString(b);
      per << (k ? ", " : "") << names[k] << (kOk ? " kept" : " CHANGED");
      ok = ok && kOk;
    }
    // A/B: B under EXT, A was saved under INT: recalling A keeps EXT.
    p->setParamU(P_CLOCK_SOURCE, static_cast<float>(stepU(1, 3)));
    pn.pressBind("ab:1");
    p->setParamU(P_CLOCK_SOURCE, static_cast<float>(stepU(2, 3)));
    pn.pressBind("ab:0");
    const bool abKeeps = srcIdx() == 2;
    pn.pressBind("ab:1");
    // Undo of the SRC key restores the previous SRC (the key is an undo step).
    p->setParamU(P_CLOCK_SOURCE, static_cast<float>(stepU(0, 3)));
    pn.pressBind("src");
    p->settleUndoStep();
    const int afterKey = srcIdx();
    pn.pressBind("undo");
    const bool undoRestores = afterKey == 1 && srcIdx() == 0;
    // Host save/restore: a state saved under EXT restores EXT into an instance at INT (the default).
    p->setParamU(P_CLOCK_SOURCE, static_cast<float>(stepU(2, 3)));
    juce::MemoryBlock mb;
    p->getStateInformation(mb);
    auto r = fresh();
    const int before = stepIndex(static_cast<double>(r->paramU(P_CLOCK_SOURCE)), 3);
    r->setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    const bool stateRestores = before == 1 && stepIndex(static_cast<double>(r->paramU(P_CLOCK_SOURCE)), 3) == 2;
    check(ok && abKeeps && undoRestores && stateRestores, "SRC on program load",
          per + " over " + juce::String(loads) + " program loads (" + juce::String(n) +
              " programs, host change + KIT arrows, INIT PATCH); A/B keeps " + juce::String(abKeeps ? "yes" : "NO") +
              "; undo of SRC key restores " + juce::String(undoRestores ? "yes" : "NO") + "; saved state restores EXT " +
              juce::String(stateRestores ? "yes" : "NO"));
  }

  // ---- A/B compare: two full-state slots; B starts as a copy; right-click B copies A onto B
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    p->setCurrentProgram(5);
    const int dec = findParam("BD1:DECAY");
    const juce::String a = p->stateJson();
    const float aDec = p->paramU(dec);
    pn.pressBind("ab:1");
    bool ok = p->abSlot() == 1 && p->stateJson() == a;  // first visit: B = copy of A
    p->setParamU(dec, 0.9f);
    p->editPattern().tracks[SD].steps[3].on = !p->editPattern().tracks[SD].steps[3].on;
    p->commitEdits();
    const juce::String b = p->stateJson();
    pn.pressBind("ab:0");
    ok = ok && p->abSlot() == 0 && p->stateJson() == a && dsp::exactEq(p->paramU(dec), aDec);
    pn.pressBind("ab:1");
    ok = ok && p->abSlot() == 1 && p->stateJson() == b && dsp::exactEq(p->paramU(dec), 0.9f);
    pn.pressBind("ab:1", true);  // copy A → B (B is live: A's state loads)
    ok = ok && p->abSlot() == 1 && p->stateJson() == a;
    pn.pressBind("ab:0");
    ok = ok && p->stateJson() == a;
    // a different program in A, then right-click A copies B (the program-5 state) back onto A
    p->setCurrentProgram(9);
    pn.pressBind("ab:0", true);
    ok = ok && p->stateJson() == a && p->getCurrentProgram() == 5;
    check(ok, "A/B compare", "B = copy on first visit, A/B recall exact (BD1:DECAY " + juce::String(aDec, 3) +
                                 " / 0.900, SD step 4), copy A>B and B>A");
  }

  // ---- undo / redo: kit loads, knob gestures and step edits; no-op clicks leave the stacks alone; 64 levels
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    pn.setTab(0);
    const int dec = findParam("BD1:DECAY");
    juce::StringArray st;
    st.add(p->stateJson());
    pn.pressBind("prog:1");  // kit load
    st.add(p->stateJson());
    pn.pressBind("p:BD1:DECAY");  // knob gesture (press, drag, release)
    p->param(dec)->setValueNotifyingHost(0.123f);
    p->settleUndoStep();
    st.add(p->stateJson());
    pn.pressBind("step:0");  // step edit (BD1 step 1)
    p->settleUndoStep();
    st.add(p->stateJson());
    bool ok = p->undoDepth() == 3 && p->redoDepth() == 0;
    for (int i = 2; i >= 0; --i) {
      pn.pressBind("undo");
      ok = ok && p->stateJson() == st[i];
    }
    ok = ok && p->getCurrentProgram() == 0 && !p->undo() && p->redoDepth() == 3;
    for (int i = 1; i <= 3; ++i) {
      pn.pressBind("redo");
      ok = ok && p->stateJson() == st[i];
    }
    ok = ok && p->getCurrentProgram() == 1 && !p->redo();
    pn.pressBind("undo");
    pn.pressBind("p:BD1:DECAY");  // a click that changes nothing: no step, redo kept
    p->settleUndoStep();
    ok = ok && p->undoDepth() == 2 && p->redoDepth() == 1;
    pn.pressBind("redo");
    ok = ok && p->stateJson() == st[3];
    for (int i = 0; i < 70; ++i) {  // bounded: 64 levels
      pn.pressBind("p:BD1:DECAY");
      p->param(dec)->setValueNotifyingHost(0.2f + 0.01f * static_cast<float>(i));
      p->settleUndoStep();
    }
    const int depth = p->undoDepth();
    int undone = 0;
    while (p->undo()) ++undone;
    ok = ok && depth == ShogunAudioProcessor::kUndoLevels && undone == ShogunAudioProcessor::kUndoLevels;
    check(ok, "undo/redo", "kit load + knob gesture + step edit undone and redone exactly; no-op click keeps redo; " +
                               juce::String(depth) + " levels after 70 edits");
  }

  // ---- ASSIGN (MOD ◉ keys), UI SCALE, OS badge, offline 4×, RE-ROLL UNIT, VELOCITY CURVE
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto& pn = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get())->panel();
    pn.setTab(3);
    pn.pressBind("assign:5");
    pn.pressBind("assign:5");
    bool okA = pn.armedSource() == 0;  // a second press cancels
    pn.pressBind("assign:1");
    okA = okA && pn.armedSource() == mod::SRC_LFO1;
    pn.setTab(0);
    pn.pressBind("p:BD1:DECAY");
    p->settleUndoStep();
    const mod::Row r0 = p->editRows()[0];
    okA = okA && pn.armedSource() == 0 && r0.src == mod::SRC_LFO1 && r0.dst == findParam("BD1:DECAY") &&
          std::fabs(r0.depth - 0.5) < 1e-12 && r0.on && p->undoDepth() == 1;
    pn.setTab(3);
    pn.pressBind("assign:13");  // AT, global
    pn.setTab(0);
    pn.pressBind("p:BD2:TUNE");
    okA = okA && p->editRows()[1].src == mod::SRC_AT && p->editRows()[1].dst == findParam("BD2:TUNE");
    check(okA, "ASSIGN keys", "LFO 1 > BD1:DECAY +50 % in slot 1, AT > BD2:TUNE in slot 2, second press cancels");

    pn.setTab(7);
    pn.pressBind("uiscale:150");
    const int w150 = ed->getWidth();
    const juce::String lit150 = pn.boundText("uiscale:150");
    pn.pressBind("uiscale:75");
    const int w75 = ed->getWidth(), h75 = ed->getHeight();
    pn.pressBind("uiscale:100");
    check(w150 == 1800 && w75 == 900 && h75 == 504 && ed->getWidth() == 1200, "UI scale keys",
          "150% = " + juce::String(w150) + " px, 75% = " + juce::String(w75) + "x" + juce::String(h75) + ", 100% = " +
              juce::String(ed->getWidth()));

    const int os0 = stepIndex(static_cast<double>(p->paramU(P_GLOBAL_OS)), 3);
    pn.setTab(5);
    pn.pressBind("osbadge");
    const int os1 = stepIndex(static_cast<double>(p->paramU(P_GLOBAL_OS)), 3);
    pn.pressBind("osbadge", false, true);  // shift-click steps back
    const int os2 = stepIndex(static_cast<double>(p->paramU(P_GLOBAL_OS)), 3);
    pn.setTab(7);
    pn.pressBind("choice:GLOBAL:OFFLINE:1");
    const int off = stepIndex(static_cast<double>(p->paramU(P_GLOBAL_OFFLINE)), 2);
    const int os3 = stepIndex(static_cast<double>(p->paramU(P_GLOBAL_OS)), 3);
    check(os1 == (os0 + 1) % 3 && os2 == os0 && off == 1 && os3 == os0, "OS keys",
          "badge " + juce::String(1 << os0) + "x > " + juce::String(1 << os1) + "x, shift-click back; offline 4x sets OFFLINE (OS stays " +
              juce::String(1 << os3) + "x)");

    const std::uint32_t s0 = p->unitSerial();
    pn.pressBind("reroll");
    const std::uint32_t s1 = p->unitSerial();
    render(*p, 512);
    juce::MemoryBlock mb;
    p->getStateInformation(mb);
    auto q = fresh();
    q->setStateInformation(mb.getData(), static_cast<int>(mb.getSize()));
    render(*q, 512);
    const juce::String serialText = pn.boundText("serial");
    check(s1 != s0 && p->engine().serial() == s1 && q->engine().serial() == s1 && serialText.contains(juce::String::toHexString(static_cast<juce::int64>(s1)).toUpperCase().substring(4)),
          "RE-ROLL UNIT", "0x" + juce::String::toHexString(static_cast<juce::int64>(s0)).toUpperCase() + " > 0x" +
                              juce::String::toHexString(static_cast<juce::int64>(s1)).toUpperCase() + ", engine + saved state follow (" + serialText + ")");

    // VELOCITY CURVE: LINEAR / SOFT / HARD / FIXED shape MIDI velocity (BD1 from note 36 at velocity 40)
    auto peakAt = [](int curve) {
      auto r = fresh();
      setU(*r, "GLOBAL:VEL CURVE", static_cast<float>(stepU(curve, 4)));
      render(*r, 512);
      juce::MidiBuffer m;
      m.addEvent(juce::MidiMessage::noteOn(10, 36, static_cast<juce::uint8>(40)), 0);
      return render(*r, 48000, &m).peakMain;
    };
    const float pl = peakAt(0), ps = peakAt(1), ph = peakAt(2), pf = peakAt(3);
    const bool curveFn = std::fabs(ShogunAudioProcessor::velCurve(0, 0.25) - 0.25) < 1e-12 &&
                         std::fabs(ShogunAudioProcessor::velCurve(1, 0.25) - 0.5) < 1e-12 &&
                         std::fabs(ShogunAudioProcessor::velCurve(2, 0.25) - 0.0625) < 1e-12 &&
                         std::fabs(ShogunAudioProcessor::velCurve(3, 0.25) - 1.0) < 1e-12;
    check(curveFn && ph < pl && pl < ps && ps <= pf, "VELOCITY CURVE",
          "velocity 40 peaks HARD " + juce::String(ph, 3) + " < LINEAR " + juce::String(pl, 3) + " < SOFT " + juce::String(ps, 3) +
              " <= FIXED " + juce::String(pf, 3));
  }

  std::cout << (failures == 0 ? "ShogunProbe: all checks passed" : "ShogunProbe: FAILURES " + juce::String(failures).toStdString())
            << std::endl;
  return failures == 0 ? 0 : 1;
}
