#pragma once
// SHOGUN jack table (spec v2.2 §13.2): 153 ports, ids exactly as specified, types as §13.1 (plain-volt device:
// inputs CV except RET = Audio; OUT/MIX Audio; ENV/NOTE OUT/MOD/ACC OUT CV; CLK/RST/RUN OUT and LD/BS GATE Gate).
// Global form: SHOGUN#N/<id>.

#include <jidai/jcs/JackId.h>
#include <jidai/jcs/Roles.h>

#include <cstdint>

namespace shogun {

// JCS R14 roles, colours and glyphs come from the shared header (jidai::jcs::roleInfo / roleArgb).
using Role = jidai::jcs::Role;
// Port types (graph legality, RackGraph kAllowed) and directions: SHOGUN's own table fields.
enum class PortType : std::uint8_t { Audio, CV, Gate };
enum class PortDir : std::uint8_t { In, Out };

struct PortDesc {
  const char* id;
  PortDir dir;
  PortType type;
  float rest;
  Role role;
};

constexpr int kDrumVoices = 14;
// Per-drum jack order (8 per voice, §13.2 row order).
enum DrumJack : int { DJ_TRIG, DJ_VEL, DJ_PITCH, DJ_DECAY, DJ_TONE, DJ_RET, DJ_OUT, DJ_ENV, kDrumJacks };
// Per-synth jack order (LEAD, BASS).
enum SynthJack : int { SJ_GATE, SJ_VEL, SJ_NOTE, SJ_VOCT, SJ_CUTOFF, SJ_RET, SJ_OUT, SJ_NOTE_OUT, kSynthJacks };

constexpr int kSynthBase = kDrumVoices * kDrumJacks;  // 112
constexpr int drumPort(int v, int j) { return v * kDrumJacks + j; }
constexpr int synthPort(int s, int j) { return kSynthBase + s * kSynthJacks + j; }  // s: 0 LEAD, 1 BASS

enum GlobalPort : int {
  PORT_LD_GATE = kSynthBase + 2 * kSynthJacks,  // 128
  PORT_BS_GATE,
  PORT_CLK_IN,
  PORT_RST_IN,
  PORT_RUN_IN,
  PORT_FILL_IN,
  PORT_CLK_OUT,
  PORT_RST_OUT,
  PORT_RUN_OUT,
  PORT_ACC_OUT,
  PORT_LFO1,
  PORT_LFO2,
  PORT_LFO3,
  PORT_LFO4,
  PORT_RND,
  PORT_LANE_A,
  PORT_MIX_L,
  PORT_MIX_R,  // 145
  PORT_BD1_WAVE,
  PORT_BD2_WAVE,
  PORT_FOLD_VC_BD1,
  PORT_FOLD_VC_BD2,
  PORT_FOLD_VC_LTC,
  PORT_FOLD_VC_MTC,
  PORT_FOLD_VC_HTC,
  kPorts  // 153
};
static_assert(kPorts == 153, "§13.2: 153 ports");

#define SG_DRUM(V)                                                     \
  {V ":TRIG", PortDir::In, PortType::CV, 0.f, Role::GateClk},            \
      {V ":VEL", PortDir::In, PortType::CV, 0.f, Role::CV},            \
      {V ":PITCH", PortDir::In, PortType::CV, 0.f, Role::VOct},        \
      {V ":DECAY", PortDir::In, PortType::CV, 0.f, Role::CV},          \
      {V ":TONE", PortDir::In, PortType::CV, 0.f, Role::CV},           \
      {V ":RET", PortDir::In, PortType::Audio, 0.f, Role::Audio},      \
      {V ":OUT", PortDir::Out, PortType::Audio, 0.f, Role::Audio},     \
      {V ":ENV", PortDir::Out, PortType::CV, 0.f, Role::CV}
#define SG_SYNTH(V)                                                    \
  {V ":GATE", PortDir::In, PortType::CV, 0.f, Role::GateClk},          \
      {V ":VEL", PortDir::In, PortType::CV, 0.f, Role::CV},            \
      {V ":NOTE", PortDir::In, PortType::CV, 0.f, Role::VOct},         \
      {V ":V/OCT", PortDir::In, PortType::CV, 0.f, Role::VOct},        \
      {V ":CUTOFF", PortDir::In, PortType::CV, 0.f, Role::CV},         \
      {V ":RET", PortDir::In, PortType::Audio, 0.f, Role::Audio},      \
      {V ":OUT", PortDir::Out, PortType::Audio, 0.f, Role::Audio},     \
      {V ":NOTE OUT", PortDir::Out, PortType::CV, 0.f, Role::VOct}

inline const PortDesc kPortTable[kPorts] = {
    SG_DRUM("BD1"), SG_DRUM("BD2"), SG_DRUM("SD"),  SG_DRUM("RS"),  SG_DRUM("CP"),  SG_DRUM("CL"),  SG_DRUM("MA"),
    SG_DRUM("CB"),  SG_DRUM("CH"),  SG_DRUM("OH"),  SG_DRUM("CY"),  SG_DRUM("LTC"), SG_DRUM("MTC"), SG_DRUM("HTC"),
    SG_SYNTH("LEAD"), SG_SYNTH("BASS"),
    {"MOD:LD GATE", PortDir::Out, PortType::Gate, 0.f, Role::GateClk},
    {"MOD:BS GATE", PortDir::Out, PortType::Gate, 0.f, Role::GateClk},
    {"CLOCK:CLK IN", PortDir::In, PortType::CV, 0.f, Role::GateClk},
    {"CLOCK:RST IN", PortDir::In, PortType::CV, 0.f, Role::GateClk},
    {"CLOCK:RUN IN", PortDir::In, PortType::CV, 0.f, Role::GateClk},
    {"CLOCK:FILL IN", PortDir::In, PortType::CV, 0.f, Role::GateClk},
    {"CLOCK:CLK OUT", PortDir::Out, PortType::Gate, 0.f, Role::GateClk},
    {"CLOCK:RST OUT", PortDir::Out, PortType::Gate, 0.f, Role::GateClk},
    {"CLOCK:RUN OUT", PortDir::Out, PortType::Gate, 0.f, Role::GateClk},
    {"CLOCK:ACC OUT", PortDir::Out, PortType::CV, 0.f, Role::CV},
    {"MOD:LFO 1", PortDir::Out, PortType::CV, 0.f, Role::CV},
    {"MOD:LFO 2", PortDir::Out, PortType::CV, 0.f, Role::CV},
    {"MOD:LFO 3", PortDir::Out, PortType::CV, 0.f, Role::CV},
    {"MOD:LFO 4", PortDir::Out, PortType::CV, 0.f, Role::CV},
    {"MOD:RND", PortDir::Out, PortType::CV, 0.f, Role::CV},
    {"MOD:LANE A", PortDir::Out, PortType::CV, 0.f, Role::CV},
    {"MIX:L", PortDir::Out, PortType::Audio, 0.f, Role::Audio},
    {"MIX:R", PortDir::Out, PortType::Audio, 0.f, Role::Audio},
    {"BD1:WAVE", PortDir::In, PortType::CV, 0.f, Role::CV},
    {"BD2:WAVE", PortDir::In, PortType::CV, 0.f, Role::CV},
    {"BD1:FOLD VC", PortDir::In, PortType::CV, 0.f, Role::CV},
    {"BD2:FOLD VC", PortDir::In, PortType::CV, 0.f, Role::CV},
    {"LTC:FOLD VC", PortDir::In, PortType::CV, 0.f, Role::CV},
    {"MTC:FOLD VC", PortDir::In, PortType::CV, 0.f, Role::CV},
    {"HTC:FOLD VC", PortDir::In, PortType::CV, 0.f, Role::CV},
};
#undef SG_DRUM
#undef SG_SYNTH

// Shogun is a plain-volt device (§13.1, BushidoDevice.cpp L44).
constexpr bool kPlainVoltGates = true;

// Load-time alias table (§12.3). law: 0 = same volts, 1 = lin55 (V' = log2 V − 1.25), 2 = drum PITCH sim migration
// (CV AMT = 1/12, 1 V/semitone → 1 V/oct). An alias may fan out to several new ids (to[] ends at nullptr).
struct PortAlias {
  const char* from;
  const char* to[3];
  int law;
};
inline const PortAlias kPortAliases[] = {
    {"MIX L", {"MIX:L", nullptr, nullptr}, 0},
    {"MIX R", {"MIX:R", nullptr, nullptr}, 0},
    {"LEAD:HZ/V", {"LEAD:NOTE", nullptr, nullptr}, 1},
    {"BASS:HZ/V", {"BASS:NOTE", nullptr, nullptr}, 1},
    {"LEAD:HZ/V OUT", {"LEAD:NOTE OUT", nullptr, nullptr}, 0},
    {"BASS:HZ/V OUT", {"BASS:NOTE OUT", nullptr, nullptr}, 0},
    {"SD:SNAPPY", {"SD:TONE", nullptr, nullptr}, 0},
    {"HAT:DECAY", {"CH:DECAY", "OH:DECAY", nullptr}, 0},
    {"TOM:PITCH", {"LTC:PITCH", "MTC:PITCH", "HTC:PITCH"}, 2},
    {"LFO:OUT", {"MOD:LFO 1", nullptr, nullptr}, 0},
    {"LFO OUT", {"MOD:LFO 1", nullptr, nullptr}, 0},
};

inline int findPort(const char* id) {
  for (int i = 0; i < kPorts; ++i) {
    const char* a = kPortTable[i].id;
    const char* b = id;
    while (*a && *a == *b) {
      ++a;
      ++b;
    }
    if (*a == 0 && *b == 0) return i;
  }
  return -1;
}

// Resolves a saved jack id: a current id, or an alias (§12.3). Returns the number of ports written to out[3]
// (0 = unknown) and the alias law (0 same volts, 1 lin55, 2 drum PITCH AMT 1/12).
// Every SECTION:LABEL id is parsed with the shared JCS R6 parser (jidai::jcs::parseJackId). It accepts the global
// form SHOGUN#N/SECTION:LABEL, the first-instance form SHOGUN/SECTION:LABEL and the bare form. The split is at the
// first '/' before the first ':', so labels keep '/', spaces and digits (LEAD:V/OCT, LEAD:HZ/V OUT, MOD:LD GATE).
// Another device's prefix is refused. The v2.0/2.1 names that are not R6 ids at all (MIX L, MIX R, LFO OUT have no
// SECTION:) cannot parse; they are matched whole against the alias table and never split.
inline int resolvePort(const char* text, int out[3], int* law) {
  if (law) *law = 0;
  std::string local;
  if (const auto j = jidai::jcs::parseJackId(text)) {
    if (!j->prefix.empty() && !(j->prefix == "SHOGUN")) return 0;
    local = j->local();
  } else {
    local = text;
  }
  const char* id = local.c_str();
  const int p = findPort(id);
  if (p >= 0) {
    out[0] = p;
    return 1;
  }
  for (const PortAlias& a : kPortAliases) {
    const char* x = a.from;
    const char* y = id;
    while (*x && *x == *y) {
      ++x;
      ++y;
    }
    if (*x != 0 || *y != 0) continue;
    int n = 0;
    for (int i = 0; i < 3 && a.to[i]; ++i) out[n++] = findPort(a.to[i]);
    if (law) *law = a.law;
    return n;
  }
  return 0;
}

// One port for a jack id in any R6 form (first target of a fan-out alias), or −1.
inline int portFromId(const char* text) {
  int out[3];
  return resolvePort(text, out, nullptr) > 0 ? out[0] : -1;
}

}  // namespace shogun
