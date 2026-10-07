#pragma once

// Placeholder parameter table. One row per voice. CC defaults are audible
// stand-ins. The clock switch lives in forge/graphs/switch_law.json.

#include "shogun.h"

#include <array>

namespace shogun_ui {

struct KnobSpec {
  const char* id;
  const char* label;
  int shogun::Knobs::* field;
  int initial;
};

struct VoiceSpec {
  const char* id;
  const char* name;
  const char* levelId;
  shogun::Voice voice;
  std::array<KnobSpec, 8> knobs;
  int count;
};

inline const std::array<VoiceSpec, 16>& voices() {
  static const std::array<VoiceSpec, 16> rows = {{
      {"bd1", "BD1", "lvl_bd1", shogun::Voice::Bd1,
       {{{"bd1_attack", "ATK", &shogun::Knobs::bd1Attack, 64},
         {"bd1_decay", "DEC", &shogun::Knobs::bd1Decay, 80},
         {"bd1_pitch", "PIT", &shogun::Knobs::bd1Pitch, 40},
         {"bd1_tune", "TUN", &shogun::Knobs::bd1Tune, 50},
         {"bd1_noise", "NOI", &shogun::Knobs::bd1Noise, 0},
         {"bd1_filter", "FLT", &shogun::Knobs::bd1Filter, 64},
         {"bd1_dist", "DST", &shogun::Knobs::bd1Dist, 0},
         {"bd1_trigger", "TRG", &shogun::Knobs::bd1Trigger, 0}}},
       8},
      {"bd2", "BD2", "lvl_bd2", shogun::Voice::Bd2,
       {{{"bd2_decay", "DEC", &shogun::Knobs::bd2Decay, 80},
         {"bd2_tune", "TUN", &shogun::Knobs::bd2Tune, 64},
         {"bd2_tone", "TON", &shogun::Knobs::bd2Tone, 40}}},
       3},
      {"sd", "SD", "lvl_sd", shogun::Voice::Sd,
       {{{"sd_tune", "TUN", &shogun::Knobs::sdTune, 70},
         {"sd_dtune", "DTN", &shogun::Knobs::sdDTune, 90},
         {"sd_snappy", "SNP", &shogun::Knobs::sdSnappy, 40},
         {"sd_sndecay", "SND", &shogun::Knobs::sdSnDecay, 50},
         {"sd_tone", "TON", &shogun::Knobs::sdTone, 64},
         {"sd_tonedecay", "TDC", &shogun::Knobs::sdToneDecay, 60},
         {"sd_pitch", "PIT", &shogun::Knobs::sdPitch, 30}}},
       7},
      {"rs", "RS", "lvl_rs", shogun::Voice::Rs, {{{"rs_tune", "TUN", &shogun::Knobs::rsTune, 64}}}, 1},
      {"cy", "CY", "lvl_cy", shogun::Voice::Cy,
       {{{"cy_decay", "DEC", &shogun::Knobs::cyDecay, 80},
         {"cy_tone", "TON", &shogun::Knobs::cyTone, 64},
         {"cy_tune", "TUN", &shogun::Knobs::cyTune, 60}}},
       3},
      {"oh", "OH", "lvl_oh", shogun::Voice::Oh, {{{"oh_decay", "DEC", &shogun::Knobs::ohDecay, 100}}}, 1},
      {"hh", "HH", "lvl_hh", shogun::Voice::Hh,
       {{{"hh_tune", "TUN", &shogun::Knobs::hhTune, 60},
         {"hh_decay", "DEC", &shogun::Knobs::hhDecay, 40}}},
       2},
      {"cl", "CL", "lvl_cl", shogun::Voice::Cl,
       {{{"cl_tune", "TUN", &shogun::Knobs::clTune, 64},
         {"cl_decay", "DEC", &shogun::Knobs::clDecay, 40}}},
       2},
      {"cp", "CP", "lvl_cp", shogun::Voice::Cp,
       {{{"cp_decay", "DEC", &shogun::Knobs::cpDecay, 64},
         {"cp_filter", "FLT", &shogun::Knobs::cpFilter, 64},
         {"cp_attack", "ATK", &shogun::Knobs::cpAttack, 0},
         {"cp_trigger", "TRG", &shogun::Knobs::cpTrigger, 0},
         {"cp_data", "DAT", &shogun::Knobs::cpData, 48}}},
       5},
      {"ltc", "LTC", "lvl_ltc", shogun::Voice::Ltc,
       {{{"ltc_tune", "TUN", &shogun::Knobs::ltcTune, 64},
         {"ltc_decay", "DEC", &shogun::Knobs::ltcDecay, 64},
         {"ltc_noise", "NOI", &shogun::Knobs::ltcNoise, 0},
         {"ltc_mode", "MOD", &shogun::Knobs::ltcMode, 0},
         {"tom_noise", "NZ", &shogun::Knobs::tomNoise, 64}}},
       5},
      {"mtc", "MTC", "lvl_mtc", shogun::Voice::Mtc,
       {{{"mtc_tune", "TUN", &shogun::Knobs::mtcTune, 64},
         {"mtc_decay", "DEC", &shogun::Knobs::mtcDecay, 64},
         {"mtc_noise", "NOI", &shogun::Knobs::mtcNoise, 0},
         {"mtc_mode", "MOD", &shogun::Knobs::mtcMode, 0}}},
       4},
      {"htc", "HTC", "lvl_htc", shogun::Voice::Htc,
       {{{"htc_tune", "TUN", &shogun::Knobs::htcTune, 64},
         {"htc_decay", "DEC", &shogun::Knobs::htcDecay, 64},
         {"htc_noise", "NOI", &shogun::Knobs::htcNoise, 0},
         {"htc_mode", "MOD", &shogun::Knobs::htcMode, 0}}},
       4},
      {"cb", "CB", "lvl_cb", shogun::Voice::Cb,
       {{{"cb_tune", "TUN", &shogun::Knobs::cbTune, 64},
         {"cb_decay", "DEC", &shogun::Knobs::cbDecay, 64}}},
       2},
      {"ma", "MA", "lvl_ma", shogun::Voice::Ma, {{{"ma_decay", "DEC", &shogun::Knobs::maDecay, 64}}}, 1},
      {"lead", "LEAD", "lvl_lead", shogun::Voice::Lead, {{{"lead_tone", "TON", &shogun::Knobs::leadTone, 64}}}, 1},
      {"bass", "BASS", "lvl_bass", shogun::Voice::Bass, {{{"bass_tone", "TON", &shogun::Knobs::bassTone, 64}}}, 1},
  }};
  return rows;
}

}  // namespace shogun_ui
