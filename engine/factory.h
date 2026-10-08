#pragma once
// The factory bank (§14.1): INIT, then the kits below, each a kit with its matching pattern, stored as patch documents
// in the saved-state format (engine/patch.h) in engine/factory_bank.inc. The engine, the plugin's program list and
// the web page all read this one table. engine/factory_bank.inc is written by scripts/make_factory.py.

#include "patch.h"

namespace shogun {
namespace factory {

struct Entry {
  const char* name;  // program / kit name; the pattern is "NNN <name>" with NNN = program number (INIT = 001)
  const char* json;  // patch document (sparse: only what differs from INIT)
};

#include "factory_bank.inc"  // kBank[]

constexpr int kCount = static_cast<int>(sizeof kBank / sizeof kBank[0]);  // factory kits, INIT not counted

// Programs: 0 = INIT, 1..kCount = kBank.
constexpr int kPrograms = kCount + 1;
inline const char* programName(int program) {
  if (program == 0) return "INIT";
  return program > 0 && program <= kCount ? kBank[program - 1].name : "";
}
inline const char* programJson(int program) { return program > 0 && program <= kCount ? kBank[program - 1].json : ""; }
// Program into a patch. Program 0 is INIT (the cleared patch).
inline bool loadProgram(int program, Patch& out) {
  if (program == 0) {
    out.clear();
    return true;
  }
  if (program < 0 || program > kCount) return false;
  return parsePatch(kBank[program - 1].json, out);
}

}  // namespace factory
}  // namespace shogun
