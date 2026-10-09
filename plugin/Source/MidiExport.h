#pragma once
// Pattern → standard MIDI file (item 2a): one cycle of the current pattern on the same note map as MIDI in / out —
// drums on channel 10 at 36 + voice (BD1 36 … HTC 49), LEAD on channel 1 and BASS on channel 2 at the step's note.
// Represented: each lane's own length and scale, velocity from the accent (70 / 100 / 127), ratchets and flams as
// repeated notes, micro-timing, swing and the lane SHIFT. Not representable in a plain file: step probability (every
// step that can play is written), parameter locks and pitch bends.

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>

#include "seq.h"
#include "shogun.h"

namespace shogun::midiexport {

constexpr int kPpq = 96;

inline int velocityFor(int acc) {
  static constexpr int v[4] = {0, 70, 100, 127};
  return v[std::clamp(acc, 1, 3)];
}

// One cycle: the longest lane's length (in quarter notes, rounded up to a whole beat); shorter lanes repeat.
inline double cycleQuarters(const Pattern& pat, int globalScale) {
  double q = 1.0;
  for (const Track& tr : pat.tracks)
    q = std::max(q, std::clamp(tr.len, 1, kMaxSteps) / stepsPerQuarter(tr.scale < 0 ? globalScale : tr.scale));
  return std::ceil(q - 1e-9);
}

inline juce::MidiFile patternToMidi(const Pattern& pat, double bpm, double globalSwing, int globalScale) {
  juce::MidiMessageSequence seq;
  seq.addEvent(juce::MidiMessage::textMetaEvent(3, pat.name), 0.0);
  seq.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(std::lround(60000000.0 / bpm))), 0.0);
  seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0.0);
  const double cycle = cycleQuarters(pat, globalScale) * kPpq;
  const double ticksPerMs = bpm / 60000.0 * kPpq;
  for (int t = 0; t < 16; ++t) {
    const Track& tr = pat.tracks[t];
    const int len = std::clamp(tr.len, 1, kMaxSteps);
    const double stepT = kPpq / stepsPerQuarter(tr.scale < 0 ? globalScale : tr.scale);
    const double sw = tr.swing < 0.0 ? globalSwing : tr.swing;
    const bool synth = t == LEAD || t == BASS;
    const int ch = synth ? (t == LEAD ? 1 : 2) : 10;
    const long steps = static_cast<long>(std::floor(cycle / stepT + 1e-9));
    for (long k = 0; k < steps; ++k) {
      const Step& st = tr.steps[k % len];
      if (!st.on) continue;
      double at = static_cast<double>(k) * stepT + st.micro * stepT + tr.shift * 30.0 * ticksPerMs;
      if ((k & 1) == 1) at += (2.0 * sw - 1.0) * stepT;
      at = std::max(0.0, at);
      const auto vel = static_cast<juce::uint8>(velocityFor(st.acc));
      if (synth) {
        if (st.tie && k > 0 && tr.steps[(k - 1) % len].on && tr.steps[(k - 1) % len].note == st.note) continue;  // carried
        long j = k + 1;  // a tie on the following steps extends the note
        while (j < steps && tr.steps[j % len].on && tr.steps[j % len].tie && tr.steps[j % len].note == st.note) ++j;
        const int note = std::clamp<int>(st.note, 0, 127);
        seq.addEvent(juce::MidiMessage::noteOn(ch, note, vel), at);
        seq.addEvent(juce::MidiMessage::noteOff(ch, note), at + static_cast<double>(j - k) * stepT - 1.0);
        continue;
      }
      const int note = 36 + t;
      int hits = 1;
      double gap = stepT;
      if (st.flam > 0 && t != CP) {  // flam (not on clap): 2..5 hits, 3.75 ms × (1 + k/4) apart
        const int f = (st.flam - 1) & 15;
        hits = 2 + (f % 4);
        gap = 3.75 * (1 + f / 4) * ticksPerMs;
      } else {
        int r = st.ratchet;
        bool ok = false;
        for (int x : kRatchets) ok = ok || x == r;
        hits = ok ? r : 1;
        gap = stepT / hits;
      }
      const double dur = std::max(1.0, std::min(gap, stepT) * 0.5);
      for (int h = 0; h < hits; ++h) {
        seq.addEvent(juce::MidiMessage::noteOn(ch, note, vel), at + h * gap);
        seq.addEvent(juce::MidiMessage::noteOff(ch, note), at + h * gap + dur);
      }
    }
  }
  seq.updateMatchedPairs();
  seq.sort();
  juce::MidiFile f;
  f.setTicksPerQuarterNote(kPpq);
  f.addTrack(seq);
  return f;
}

inline bool writePatternFile(const Pattern& pat, double bpm, double globalSwing, int globalScale, const juce::File& out) {
  out.deleteFile();
  juce::FileOutputStream os(out);
  return os.openedOk() && patternToMidi(pat, bpm, globalSwing, globalScale).writeTo(os, 1);
}

}  // namespace shogun::midiexport
