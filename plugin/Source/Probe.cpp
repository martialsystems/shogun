// ShogunProbe: headless checks of the plugin shell (spec v2.2 §15.0 step 4) and the 8 tab renders.
//   ShogunProbe <out-dir>   writes tab_0_main.png … tab_7_global.png and prints one line per check.
#include <cmath>
#include <iostream>

#include "PluginEditor.h"

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
      if (id != n || f->range.start != 0.0f || f->range.end != 1.0f || std::fabs(f->get() - kParams[id].def) > 1e-6f) ++bad;
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
    check(q->engine().sampleRate() == 44100.0, "host rate", "engine fs " + juce::String(q->engine().sampleRate(), 1) + " Hz at host 44100");
  }

  // ---- INIT: empty pattern is silent while running; MIDI note 36 plays BD1
  {
    auto p = fresh();
    p->requestRun(true);
    const Render r = render(*p, 48000);
    check(r.peakMain == 0.0f && juce::String(p->editPattern().name) == "001 INIT", "INIT silent",
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
                                                   juce::String(20.0 * std::log10(r.peakMain), 2) + " dBFS");
    auto q = fresh();
    setU(*q, "CLOCK:SOURCE", 0.5f);
    setU(*q, "CLOCK:MODE", 0.75f);  // EXT
    for (int s : {0, 4, 8, 12}) q->editPattern().tracks[BD1].steps[s].on = true;
    q->commitEdits();
    q->requestRun(true);
    const Render r2 = render(*q, 96000);
    check(r2.peakMain == 0.0f, "EXT ignores pattern", "peak " + juce::String(r2.peakMain, 6));
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
  }

  // ---- editor: 1200 x 672, 8 tabs, 153 bay jacks; render each tab to PNG
  {
    auto p = fresh();
    std::unique_ptr<juce::AudioProcessorEditor> ed(p->createEditor());
    auto* se = dynamic_cast<ShogunAudioProcessorEditor*>(ed.get());
    int jacks = 0;
    for (int i = 0; i < kPorts; ++i) {
      juce::Point<float> pt;
      jacks += se->panel().jackPosition(i, pt) ? 1 : 0;
    }
    check(ed->getWidth() == 1200 && ed->getHeight() == 672 && jacks == kPorts, "editor",
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
    p->addCable(findPort("LTC:OUT"), findPort("CLOCK:FILL IN"));
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

  std::cout << (failures == 0 ? "ShogunProbe: all checks passed" : "ShogunProbe: FAILURES " + juce::String(failures).toStdString())
            << std::endl;
  return failures == 0 ? 0 : 1;
}
