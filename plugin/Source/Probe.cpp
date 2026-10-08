#include "PluginEditor.h"

#include <cmath>
#include <iostream>

namespace {

float peakOf(const juce::AudioBuffer<float>& buffer) {
  float peak = 0.0f;
  for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    peak = std::max(peak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
  return peak;
}

float renderPeak(ShogunAudioProcessor& proc, int totalSamples, juce::MidiBuffer& midi, int tailSamples) {
  const int block = 256;
  juce::AudioBuffer<float> buffer(2, block);
  float peak = 0.0f;
  float tail = 0.0f;
  int seen = 0;
  bool primed = false;
  for (int left = totalSamples; left > 0; left -= block) {
    const int n = std::min(block, left);
    buffer.setSize(2, n, false, false, true);
    buffer.clear();
    juce::MidiBuffer blockMidi;
    if (!primed) {
      blockMidi.swapWith(midi);
      primed = true;
    }
    proc.processBlock(buffer, blockMidi);
    const float blockPeak = peakOf(buffer);
    peak = std::max(peak, blockPeak);
    seen += n;
    if (seen > totalSamples - tailSamples) tail = std::max(tail, blockPeak);
  }
  if (tailSamples > 0) return tail;
  return peak;
}

bool expect(const char* name, bool ok, float value) {
  std::cout << name << (ok ? " pass " : " FAIL ") << value << "\n";
  return ok;
}

void clearSteps(ShogunAudioProcessor& proc) {
  for (int voice = 0; voice < ShogunAudioProcessor::kVoices; ++voice)
    for (int step = 0; step < ShogunAudioProcessor::kSteps; ++step)
      if (proc.isStepOn(voice, step)) proc.toggleStep(voice, step);
}

// A test beat (INIT is empty): kick and bass on 1 and 9, snare on 5 and 13, hats on the eighths.
void setTestBeat(ShogunAudioProcessor& proc) {
  clearSteps(proc);
  const int bd1 = static_cast<int>(shogun::Voice::Bd1);
  const int sd = static_cast<int>(shogun::Voice::Sd);
  const int hh = static_cast<int>(shogun::Voice::Hh);
  const int bass = static_cast<int>(shogun::Voice::Bass);
  for (int step : {0, 8}) {
    proc.toggleStep(bd1, step);
    proc.toggleStep(bass, step);
  }
  for (int step : {4, 12}) proc.toggleStep(sd, step);
  for (int step = 0; step < 16; step += 2) proc.toggleStep(hh, step);
}

}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI gui;
  bool ok = true;
  juce::MidiBuffer none;

  {
    ShogunAudioProcessor proc;
    bool anyOn = false;
    for (int voice = 0; voice < ShogunAudioProcessor::kVoices; ++voice)
      for (int step = 0; step < ShogunAudioProcessor::kSteps; ++step)
        anyOn = anyOn || proc.isStepOn(voice, step);
    ok = expect("init_pattern_empty", ! anyOn, anyOn ? 1.0f : 0.0f) && ok;
    proc.prepareToPlay(48000.0, 256);
    const float peak = renderPeak(proc, 48000, none, 0);
    ok = expect("init_pattern_silent", peak < 1.0e-4f, peak) && ok;
  }

  {
    ShogunAudioProcessor proc;
    setTestBeat(proc);
    proc.prepareToPlay(48000.0, 256);
    const float peak = renderPeak(proc, 48000, none, 0);
    ok = expect("int_pattern", peak > 0.05f, peak) && ok;
    const float clock = proc.apvts.getRawParameterValue("clock")->load();
    ok = expect("int_stays_int", std::lround(clock) == 0, clock) && ok;
  }

  {
    ShogunAudioProcessor proc;
    setTestBeat(proc);
    proc.apvts.getParameter("clock")->setValueNotifyingHost(1.0f);
    proc.prepareToPlay(48000.0, 256);
    const float peak = renderPeak(proc, 48000, none, 0);
    ok = expect("ext_ignores_pattern", peak < 1.0e-4f, peak) && ok;
  }

  {
    ShogunAudioProcessor proc;
    clearSteps(proc);
    proc.apvts.getParameter("clock")->setValueNotifyingHost(1.0f);
    proc.prepareToPlay(48000.0, 256);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 36, (juce::uint8)110), 0);
    const float peak = renderPeak(proc, 24000, midi, 0);
    const float clock = proc.apvts.getRawParameterValue("clock")->load();
    ok = expect("ext_midi_bd1", peak > 0.02f, peak) && ok;
    ok = expect("midi_does_not_force_ext_off", std::lround(clock) == 1, clock) && ok;
  }

  {
    ShogunAudioProcessor proc;
    proc.apvts.getParameter("clock")->setValueNotifyingHost(1.0f);
    clearSteps(proc);
    proc.prepareToPlay(48000.0, 256);
    proc.pushPad(static_cast<int>(shogun::Voice::Bd1));
    const float peak = renderPeak(proc, 24000, none, 0);
    const float clock = proc.apvts.getRawParameterValue("clock")->load();
    ok = expect("trig_button", peak > 0.02f, peak) && ok;
    ok = expect("trig_button_keeps_ext", std::lround(clock) == 1, clock) && ok;
  }

  {
    ShogunAudioProcessor proc;
    setTestBeat(proc);
    proc.prepareToPlay(44100.0, 256);
    const float peak = renderPeak(proc, 44100, none, 0);
    ok = expect("host_44100", peak > 0.05f, peak) && ok;
  }

  {
    ShogunAudioProcessor proc;
    proc.prepareToPlay(48000.0, 256);
    clearSteps(proc);
    // BD1 with the stand-in decay is still moving at two seconds. Eight seconds
    // is past that tail, so a cleared pattern reads as silence.
    const float tail = renderPeak(proc, 48000 * 8, none, 8000);
    ok = expect("steps_off_tail", tail < 0.01f, tail) && ok;
  }

  {
    ShogunAudioProcessor proc;
    clearSteps(proc);
    proc.apvts.getParameter("clock")->setValueNotifyingHost(1.0f);
    proc.prepareToPlay(48000.0, 256);
    juce::MidiBuffer on;
    on.addEvent(juce::MidiMessage::noteOn(1, 51, (juce::uint8)100), 0);
    const float peak = renderPeak(proc, 8000, on, 0);
    juce::MidiBuffer off;
    off.addEvent(juce::MidiMessage::noteOff(1, 51), 0);
    const float tail = renderPeak(proc, 48000, off, 8000);
    const float clock = proc.apvts.getRawParameterValue("clock")->load();
    ok = expect("bass_note", peak > 0.02f, peak) && ok;
    ok = expect("bass_note_off", tail < 0.01f, tail) && ok;
    ok = expect("bass_keeps_ext", std::lround(clock) == 1, clock) && ok;
  }

  {
    ShogunAudioProcessor proc;
    setTestBeat(proc);
    proc.toggleStep(static_cast<int>(shogun::Voice::Bd1), 1);
    proc.apvts.getParameter("clock")->setValueNotifyingHost(1.0f);
    juce::MemoryBlock block;
    proc.getStateInformation(block);
    ShogunAudioProcessor restored;
    restored.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    const bool steps = restored.isStepOn(0, 0) && restored.isStepOn(0, 1) && restored.isStepOn(0, 8) &&
                       !restored.isStepOn(0, 2);
    const float clock = restored.apvts.getRawParameterValue("clock")->load();
    ok = expect("state_steps", steps, steps ? 1.0f : 0.0f) && ok;
    ok = expect("state_clock", std::lround(clock) == 1, clock) && ok;
  }

  ShogunAudioProcessor proc;
  std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
  const int w = editor->getWidth();
  const int h = editor->getHeight();
  editor->setSize(w, h);
  juce::Image image(juce::Image::RGB, w, h, true);
  {
    juce::Graphics g(image);
    editor->paintEntireComponent(g, true);
  }
  int ink = 0;
  for (int y = 0; y < h; y += 8)
    for (int x = 0; x < w; x += 8)
      if (image.getPixelAt(x, y).getBrightness() > 0.2f) ++ink;
  ok = expect("plate_pixels", ink > 50 && w == 980 && h == 640, static_cast<float>(ink)) && ok;

  if (argc > 1) {
    juce::File file(argv[1]);
    if (auto stream = file.createOutputStream()) {
      juce::PNGImageFormat png;
      png.writeImageToStream(image, *stream);
      std::cout << "plate " << file.getFullPathName() << "\n";
    }
  }

  std::cout << (ok ? "placeholder plate ok\n" : "placeholder plate failed\n");
  return ok ? 0 : 1;
}
