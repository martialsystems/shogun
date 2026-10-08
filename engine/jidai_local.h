#pragma once

// LOCAL STAND-INS for the shared Jidai Cable Standard header (JCS v1.1, jidai-audit/JIDAI_Cross_Unit_Patching.md).
// Jidai Commander is writing the collection-wide header (detector, volt/pitch helpers, jack ids and role colours) in
// jidai-collection. Until it lands, SHOGUN keeps its own copies HERE ONLY, so swapping is one include and a namespace
// alias. Nothing else in the engine re-implements these.

#include <cmath>
#include <cstdint>

namespace shogun {
namespace jcs {

// JCS R1: host float x = 5·x V; volts leave as 0.2·V.
constexpr double kVoltsPerUnit = 5.0;
constexpr double kRail = 5.0;  // JCS R4.4 pitch rail

// JCS R3: Schmitt V-trig detector. High when V > 1.0, low when V < 0.5; the edge counts on the sample where V first
// exceeds 1.0. State persists across blocks.
struct TriggerDetector {
  static constexpr double kHigh = 1.0;
  static constexpr double kLow = 0.5;
  bool high = false;
  // Returns true on the rising edge.
  bool process(double v) {
    if (!high && v > kHigh) {
      high = true;
      return true;
    }
    if (high && v < kLow) high = false;
    return false;
  }
  void reset() { high = false; }
};

// JCS R4: 1 V/oct, 0 V = C3 = MIDI 48 = 130.8128 Hz.
constexpr double kC3Hz = 130.8127826502993;
inline double noteToVolts(double note) { return (note - 48.0) / 12.0; }
inline double voltsToNote(double v) { return 48.0 + 12.0 * v; }
// Pitch output with the hard ±5 V rail; sets *over when the rail is hit (JCS R4.4 / R15).
inline double pitchOut(double note, bool* over) {
  const double v = noteToVolts(note);
  if (v > kRail) {
    if (over) *over = true;
    return kRail;
  }
  if (v < -kRail) {
    if (over) *over = true;
    return -kRail;
  }
  return v;
}
// lin55 migration (v2.0/v2.1 HZ/V cables, 1 V = 55 Hz): V' = log2(max(V, 1e-3)) − log2(130.8128/55), with
// log2(130.8128/55) = 1.25 exactly (JCS R4.7, SHOGUN §12.3).
inline double lin55ToVoct(double v) { return std::log2(v > 1e-3 ? v : 1e-3) - 1.25; }

// JCS R2: gate outputs 0.0 / 5.0 V; trigger pulses at least 1 ms.
constexpr double kGateHigh = 5.0;
inline int pulseSamples(double sr) {
  const int n = static_cast<int>(std::lround(0.001 * sr));
  return n > 1 ? n : 1;
}

// Port types (graph legality, RackGraph kAllowed) and roles (colour, glyph, warnings) — JCS R14.
enum class PortType : std::uint8_t { Audio, CV, Gate };
enum class PortDir : std::uint8_t { In, Out };
enum class Role : std::uint8_t { STrig, Audio, VOct, GateClk, HzVLin, CV };

struct RoleStyle {
  const char* name;
  std::uint32_t rgb;
  const char* glyph;  // UTF-8
};
inline const RoleStyle& roleStyle(Role r) {
  static const RoleStyle kStyles[6] = {
      {"S-TRIG", 0xb129b1u, "\xE2\x8A\x94"},   // ⊔
      {"AUDIO", 0xe53b2fu, "\xE2\x88\xBF"},    // ∿
      {"V/OCT", 0x6590f3u, "\xE2\x99\xAA"},    // ♪
      {"GATE/CLK", 0x2ec554u, "\xE2\x8A\x93"}, // ⊓
      {"HZ/V LIN", 0x5cd5edu, "\xC6\x92"},     // ƒ
      {"CV", 0xf7e77du, "\xE2\x89\x88"},       // ≈
  };
  return kStyles[static_cast<int>(r)];
}

}  // namespace jcs
}  // namespace shogun
