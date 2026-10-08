#!/usr/bin/env python3
"""Generate engine/params_table.h: the single SHOGUN parameter table (spec v2.2 §3.2, §15.2 KnobTable).

Every parameter is a float u in [0,1] with a SECTION:LABEL id; the id is the host automation id, the patch key, the
mod-matrix destination name and the CC-map key. Stepped controls use index = min(N-1, floor(u*N)).
Run after editing this file: python3 scripts/gen_params.py"""
from pathlib import Path

VOICES = ["BD1", "BD2", "SD", "RS", "CP", "CL", "MA", "CB", "CH", "OH", "CY", "LTC", "MTC", "HTC", "LEAD", "BASS"]
WAVE_VOICES = ["BD1", "BD2", "LTC", "MTC", "HTC"]
PAN = {"BD1": 0, "BD2": 0, "SD": 0, "CP": 0, "LEAD": 0, "BASS": 0, "MTC": 0, "RS": .25, "CL": -.5, "MA": .4,
       "CB": .5, "CH": -.25, "OH": -.25, "CY": .25, "LTC": -.7, "HTC": .7}
OUTPUTS = "MAIN|BUS A|BUS B|BUS C|BUS D|AUX 1/2|AUX 3/4|AUX 5/6|AUX 7/8|AUX 9/10|AUX 11/12|AUX 13/14|AUX 15/16|PAIR"
VC_SRC = "BODY|NOISE|LFO 1|LFO 2|LFO 3|LFO 4|" + "|".join(VOICES)
DIVS = []
for name, beats in [("8 BAR", 32), ("4 BAR", 16), ("2 BAR", 8), ("1 BAR", 4), ("1/2", 2), ("1/4", 1), ("1/8", .5),
                    ("1/16", .25), ("1/32", .125), ("1/64", .0625)]:
    DIVS += [(name, beats), (name + ".", beats * 1.5), (name + "T", beats * 2 / 3)]
DIV_NAMES = "|".join(n for n, _ in DIVS)
DELAY_DIVS = "1/16|1/16.|1/16T|1/8|1/8.|1/8T|1/4|1/4.|1/4T|1/2|1/2.|1/2T"

# kinds: C continuous, B bipolar continuous (value 2u-1), S stepped/choice (n options), T toggle (2)
rows = []  # (id, kind, default_u, cc, steps, mod, voice, law, choices)
def P(id_, kind="C", d=0.5, cc=-1, steps=0, mod=None, voice=-1, law="pct", choices=""):
    if kind == "B" and law == "pct": law = "bip"
    if mod is None: mod = kind in ("C", "B")
    if kind in ("S", "T"):
        if kind == "T": mod = False
        if kind == "T": steps = 2; choices = choices or "OFF|ON"
    rows.append((id_, kind, d, cc, steps, mod, voice, law, choices))

def choice_u(i, n):  # centre of option i
    return (i + 0.5) / n

def wave_block(v, vi, preVca):
    for i in (1, 2, 3): P(f"{v}:WAVE {i}", "B", .5, voice=vi)
    for i in (1, 2, 3): P(f"{v}:SYM {i}", "B", .5, voice=vi)
    for i in (1, 2, 3): P(f"{v}:VC>AMT {i}", "B", .5, voice=vi)
    for i in (1, 2, 3): P(f"{v}:VC>SYM {i}", "B", .5, voice=vi)
    P(f"{v}:SHAPE", "C", 0.0, voice=vi, law="shape")
    P(f"{v}:VC LEVEL", "C", 1.0, voice=vi)
    P(f"{v}:ROUTING", "T", 1.0 if preVca else 0.0, voice=vi, choices="POST|PRE-VCA")
    P(f"{v}:LEVEL COMP", "T", 1.0, voice=vi)
    n = len(VC_SRC.split("|"))
    P(f"{v}:VC SRC", "S", choice_u(0, n), steps=n, voice=vi, choices=VC_SRC)

for vi, v in enumerate(VOICES):
    if v == "BD1":
        P("BD1:TUNE", d=.5, cc=3, voice=vi, law="hz 35 140")
        P("BD1:PITCH", d=.5, cc=65, voice=vi, law="st 0 18")
        P("BD1:DECAY", d=.5, cc=64, voice=vi, law="decay")
        P("BD1:ATTACK", d=.5, cc=2, voice=vi)
        P("BD1:SOUND", "S", 0.0, cc=66, steps=16, voice=vi, law="sound", choices="", mod=True)
        P("BD1:NOISE", d=0.0, cc=4, voice=vi)
        P("BD1:FILTER", d=.5, cc=5, voice=vi, law="hz 200 8000")
        P("BD1:DRIVE", d=0.0, cc=6, voice=vi, law="drive")
        P("BD1:WAVE", d=0.0, voice=vi)
        wave_block(v, vi, False)
    elif v == "BD2":
        P("BD2:TUNE", d=.5, voice=vi, law="hz 45 100")
        P("BD2:DECAY", d=.5, voice=vi, law="decay")
        P("BD2:TONE", d=.5, voice=vi)
        P("BD2:WAVE", d=0.0, voice=vi)
        wave_block(v, vi, True)
    elif v == "SD":
        P("SD:TUNE", voice=vi, law="hz 120 400")
        P("SD:DETUNE", voice=vi, law="st -8 8")
        P("SD:PITCH", voice=vi, law="st 0 14")
        P("SD:T.DECAY", voice=vi, law="decay")
        P("SD:TONE", voice=vi)
        P("SD:SNAPPY", voice=vi)
        P("SD:SN.DEC", voice=vi, law="decay")
    elif v == "RS":
        P("RS:TUNE", voice=vi, law="hz 250 2500")
    elif v == "CP":
        P("CP:ATTACK", voice=vi)
        P("CP:SOUND", "S", 0.0, steps=16, voice=vi, law="sound", mod=True)
        P("CP:COUNT", "S", choice_u(3, 8), steps=8, voice=vi, choices="1|2|3|4|5|6|7|8", mod=True)
        P("CP:FILTER", voice=vi, law="hz 400 6000")
        P("CP:DECAY", voice=vi, law="decay")
    elif v == "CL":
        P("CL:TUNE", voice=vi, law="hz 400 3000")
        P("CL:DECAY", voice=vi, law="decay")
    elif v == "MA":
        P("MA:DECAY", voice=vi, law="decay")
    elif v == "CB":
        P("CB:TUNE", cc=85, voice=vi, law="hz 300 1200")
        P("CB:DECAY", cc=86, voice=vi, law="decay")
    elif v == "CH":
        P("CH:TUNE", cc=73, voice=vi, law="ratio 0.5 2")   # HAT TUNE, shared by OH
        P("CH:DECAY", voice=vi, law="decay")
    elif v == "OH":
        P("OH:DECAY", voice=vi, law="decay")
    elif v == "CY":
        P("CY:TUNE", voice=vi, law="ratio 0.5 2")
        P("CY:TONE", voice=vi)
        P("CY:DECAY", voice=vi, law="decay")
    elif v in ("LTC", "MTC", "HTC"):
        lo, hi = {"LTC": (70, 180), "MTC": (100, 280), "HTC": (140, 400)}[v]
        P(f"{v}:TUNE", voice=vi, law=f"hz {lo} {hi}")
        P(f"{v}:DECAY", voice=vi, law="decay")
        P(f"{v}:WAVE", d=0.0, voice=vi)
        P(f"{v}:CONGA", "T", 0.0, voice=vi, choices="TOM|CGA")
        P(f"{v}:NZ", "T", 0.0, voice=vi)
        wave_block(v, vi, False)
        if v == "HTC":
            P("TOM:NOISE", d=.5, cc=84, voice=-1)
    else:  # LEAD, BASS
        lo, hi = (200, 8000) if v == "LEAD" else (80, 4000)
        P(f"{v}:CUTOFF", voice=vi, law=f"hz {lo} {hi}")
        P(f"{v}:RESO", voice=vi)
        P(f"{v}:ENV", voice=vi)
        P(f"{v}:DECAY", voice=vi, law="decay")
        P(f"{v}:SQR", "T", 0.0, voice=vi, choices="SAW|SQR")
        P(f"{v}:OCT", "S", choice_u(1, 3), steps=3, voice=vi, choices="-1|0|+1", mod=True)
        P(f"{v}:TUNE", "B", .5, voice=vi, law="cents")
        P(f"{v}:GLIDE", d=0.0, voice=vi, law="glide")
        P(f"{v}:ACCENT", voice=vi)
    # common per-voice block
    P(f"{v}:LEVEL", d=.5, voice=vi, law="level")
    P(f"{v}:PAN", "B", (PAN[v] + 1) / 2, voice=vi)
    P(f"{v}:SEND", d=0.0, voice=vi)
    P(f"{v}:OUTPUT", "S", choice_u(0, 14), steps=14, voice=vi, choices=OUTPUTS)
    P(f"{v}:MUTE", "T", 0.0, voice=vi)
    P(f"{v}:SOLO", "T", 0.0, voice=vi)
    P(f"{v}:CHOKE", "S", choice_u(0, 5), steps=5, voice=vi, choices="OFF|1|2|3|4")
    P(f"{v}:VEL>LEVEL", d=1.0, voice=vi)
    P(f"{v}:VEL>DECAY", "B", .5, voice=vi)
    P(f"{v}:ACC AMT", d=1.0, voice=vi)
    P(f"{v}:TRIG MERGE", "T", 0.0, voice=vi)

# transport / clock (header INT/EXT switch = CLOCK:MODE, the forge-pinned trigger law)
P("CLOCK:TEMPO", d=.5, law="bpm", mod=False)
P("CLOCK:SWING", d=0.0, law="swing", mod=False)
P("CLOCK:SCALE", "S", choice_u(1, 4), steps=4, choices="1/32|1/16|1/8T|1/8")
P("CLOCK:BAR", "S", choice_u(15, 32), steps=32, choices="|".join(str(i) for i in range(1, 33)))
P("CLOCK:MODE", "S", choice_u(0, 2), steps=2, choices="INT|EXT")
P("CLOCK:SOURCE", "S", choice_u(1, 3), steps=3, choices="HOST|INT|EXT")
P("CLOCK:CLK IN", "S", choice_u(0, 6), steps=6, choices="STEP|1 PPQN|2 PPQN|4 PPQN|24 PPQN|48 PPQN")
P("CLOCK:CLK OUT", "S", choice_u(0, 6), steps=6, choices="STEP|1 PPQN|2 PPQN|4 PPQN|24 PPQN|48 PPQN")
P("CLOCK:RUN OUT", "S", choice_u(0, 2), steps=2, choices="LEVEL|PULSE")
P("CLOCK:FILL", "T", 0.0)
# master
P("MASTER:ACCENT", d=1.0)
P("MASTER:DRIVE", d=0.0, law="drive24")
P("MASTER:GLUE", d=0.0)
P("MASTER:WIDTH", d=.5)
P("MASTER:FX SEND", d=.5)
P("MASTER:CEILING", d=0.95, law="ceiling")
P("MASTER:VOLUME", d=0.70710678, law="volume")
P("MASTER:CLIP", "T", 0.0)
P("FX:DELAY TIME", "S", choice_u(4, 12), steps=12, choices=DELAY_DIVS)
P("FX:DELAY FB", d=.35)
P("FX:DELAY LP", d=.5, law="hz 2000 12000")
# buses A-D
for b in "ABCD":
    P(f"BUS {b}:DRIVE", d=0.0, law="drive24")
    P(f"BUS {b}:TONE", "B", .5)
    P(f"BUS {b}:LEVEL", d=0.70710678, law="volume")
    P(f"BUS {b}:THRESH", d=1.0, law="thresh")
    P(f"BUS {b}:RATIO", d=0.15, law="ratio")
    P(f"BUS {b}:ATTACK", d=0.5, law="attack")
    P(f"BUS {b}:RELEASE", d=0.33, law="release")
    P(f"BUS {b}:MAKEUP", d=0.0, law="makeup")
    P(f"BUS {b}:MIX", d=1.0)
    P(f"BUS {b}:COMP", "T", 0.0)
    P(f"BUS {b}:SC", "S", choice_u(0, 17), steps=17, choices="OFF|" + "|".join(VOICES))
# LFOs
RETRIG = "BAR|ANY TRIG|OWN VOICE|" + "|".join(v + " TRIG" for v in VOICES)
for i in (1, 2, 3, 4):
    P(f"LFO {i}:RATE", d=.5, law="lforate")
    P(f"LFO {i}:SHAPE", "S", choice_u(0, 6), steps=6, choices="SIN|TRI|RAMP|SAW|SQR|S&H")
    P(f"LFO {i}:PHASE", d=0.0, law="deg")
    P(f"LFO {i}:SLEW", d=0.0)
    P(f"LFO {i}:PW", d=.5)
    P(f"LFO {i}:SYNC", "T", 1.0 if i == 1 else 0.0, choices="FREE|SYNC")
    P(f"LFO {i}:DIV", "S", choice_u(15, len(DIVS)), steps=len(DIVS), choices=DIV_NAMES)
    P(f"LFO {i}:POL", "T", 1.0, choices="UNI|BI")
    P(f"LFO {i}:MODE", "S", choice_u(0, 3), steps=3, choices="FREE-RUN|RETRIG|ONE-SHOT")
    P(f"LFO {i}:RETRIG BY", "S", choice_u(0, 3 + 16), steps=3 + 16, choices=RETRIG)
    P(f"LFO {i}:DEPTH", d=1.0)
    P(f"LFO {i}:FADE", d=0.0, law="fade")
# global
P("GLOBAL:OS", "S", choice_u(1, 3), steps=3, choices="1X|2X|4X")
P("GLOBAL:OFFLINE", "S", choice_u(0, 2), steps=2, choices="SAME|4X")
P("GLOBAL:DRIFT", d=.25, mod=False)
P("GLOBAL:TOLERANCE", d=.5, mod=False)
P("GLOBAL:NOISE FLOOR", d=0.0, mod=False)
P("GLOBAL:A4", d=(440 - 415) / 51, law="a4", mod=False)
P("GLOBAL:TRANSPOSE", "S", choice_u(12, 25), steps=25, choices="|".join(f"{i:+d}" for i in range(-12, 13)))
P("GLOBAL:TRIG DYN", "S", choice_u(0, 4), steps=4, choices="GATE|GATE+ACC|GATE+DYN|FULL DYN")
P("GLOBAL:VEL CURVE", "S", choice_u(0, 4), steps=4, choices="LINEAR|SOFT|HARD|FIXED")

def enum_name(id_):
    s = id_.replace(":", "_").replace(" ", "_").replace(">", "_TO_").replace(".", "").replace("&", "N")
    return "k" + "".join(c if c.isalnum() or c == "_" else "_" for c in s)

seen = set()
out = ["#pragma once", "// Generated by scripts/gen_params.py. Do not edit by hand.", "#include <cstdint>",
       "namespace shogun {", "", "enum ParamId : int {"]
for r in rows:
    e = enum_name(r[0]); assert e not in seen, e; seen.add(e)
    out.append(f"  P_{e[1:]},")
out += ["  kParamCount", "};", "",
        "enum class ParamKind : std::uint8_t { Continuous, Bipolar, Stepped, Toggle };",
        "struct ParamInfo {", "  const char* id;", "  ParamKind kind;", "  float def;", "  short cc;",
        "  short steps;", "  bool mod;", "  signed char voice;", "  const char* law;", "  const char* choices;", "};", "",
        f"constexpr int kLfoDivCount = {len(DIVS)};",
        "constexpr double kLfoDivBeats[kLfoDivCount] = {" + ", ".join(f"{b!r}" for _, b in DIVS) + "};", "",
        "constexpr ParamInfo kParams[kParamCount] = {"]
K = {"C": "Continuous", "B": "Bipolar", "S": "Stepped", "T": "Toggle"}
for (id_, kind, d, cc, steps, mod, voice, law, choices) in rows:
    ds = f"{d:.8g}"
    if "." not in ds and "e" not in ds: ds += ".0"
    out.append(f'    {{"{id_}", ParamKind::{K[kind]}, {ds}f, {cc}, {steps}, {"true" if mod else "false"}, {voice}, '
               f'"{law}", "{choices}"}},')
out += ["};", "", "}  // namespace shogun", ""]
Path(__file__).resolve().parents[1].joinpath("engine/params_table.h").write_text("\n".join(out))
print(len(rows), "params")
