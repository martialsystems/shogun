// ShogunProbe: headless checks of the plugin shell (spec v2.2 §15.0 step 4) and the 8 tab renders.
//   ShogunProbe <out-dir>   writes tab_0_main.png … tab_7_global.png and prints one line per check.
#include <cstring>
#include <cmath>
#include <iostream>

#include "PluginEditor.h"
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
    pn.pressBind("src", true);
    const juce::String back = pn.boundText("src");
    srcOk = srcOk && order.indexOf(back) == (order.indexOf(before) + 2) % 3;
    check(srcOk, "SRC key", labels.joinIntoString(" > ") + ", right-click " + before + " > " + back + "; engine tempo " +
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
    pn.pressBind("osbadge", true);
    const int os2 = stepIndex(static_cast<double>(p->paramU(P_GLOBAL_OS)), 3);
    pn.setTab(7);
    pn.pressBind("choice:GLOBAL:OFFLINE:1");
    const int off = stepIndex(static_cast<double>(p->paramU(P_GLOBAL_OFFLINE)), 2);
    const int os3 = stepIndex(static_cast<double>(p->paramU(P_GLOBAL_OS)), 3);
    check(os1 == (os0 + 1) % 3 && os2 == os0 && off == 1 && os3 == os0, "OS keys",
          "badge " + juce::String(1 << os0) + "x > " + juce::String(1 << os1) + "x, right-click back; offline 4x sets OFFLINE (OS stays " +
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
