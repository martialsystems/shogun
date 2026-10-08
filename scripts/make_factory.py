#!/usr/bin/env python3
"""SHOGUN factory bank: writes engine/factory_bank.inc (kits + matching patterns, saved-state format).

Each preset below is a kit (parameter settings in musical units), a pattern and optional mod-matrix rows.
The script builds the patch documents, has tools/factory_fmt measure every kit+pattern from a fresh engine at
44.1 and 48 kHz (default master settings), trims each kit's voice LEVELs so the loudest of 8 bars peaks at
TARGET_DB, and writes the canonical text with tools/factory_fmt canon. Run with `make factory`.

Names are descriptive: no brand, gear, model-number or artist names (tests/engine.cpp checks a banned list).
"""
import json
import math
import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOL = os.path.join(ROOT, "build", "factory_fmt")
OUT = os.path.join(ROOT, "engine", "factory_bank.inc")
TARGET_DB = -8.0      # loudest sample of 8 bars; the bank tests require <= -6 dBFS over 2 bars
TOL_DB = 0.35
LEVEL_SPREAD = 0.4

VOICES = ["BD1", "BD2", "SD", "RS", "CP", "CL", "MA", "CB", "CH", "OH", "CY", "LTC", "MTC", "HTC", "LEAD", "BASS"]
DRUMS = VOICES[:14]

# ---------------------------------------------------------------- parameter laws (engine/params_table.h)
def clamp(u): return max(0.0, min(1.0, u))
def hz(lo, hi, f): return clamp(math.log(f / lo) / math.log(hi / lo))
def dec(ms): return clamp(math.log(ms / 8.0) / 4.5)          # tau = 8 ms * exp(4.5 u)
def lvl(db): return clamp(math.sqrt(10 ** (db / 20.0) / 1.4125))  # g = 1.4125 u^2
def vol(db): return clamp(math.sqrt(10 ** (db / 20.0) / 2.0))     # g = 2 u^2 (bus level)
def st(lo, hi, x): return clamp((x - lo) / (hi - lo))
def pct(x): return clamp(x / 100.0)
def bip(x): return clamp(0.5 + x / 200.0)                     # -100..+100 %
def step(i, n): return (i + 0.5) / n
def tog(b): return 1.0 if b else 0.0
def bpm(x): return clamp((x - 40.0) / 160.0)
def swing(p): return clamp((p - 50.0) / 25.0)
def glide(ms): return clamp(math.log(ms / 2.0) / 5.5)
def ratio(lo, hi, r): return clamp(math.log(r / lo) / math.log(hi / lo))
def drive(x): return clamp(x / 9.0)                           # 0..9
def drive24(db): return clamp(db / 24.0)
def thresh(db): return clamp((db + 40.0) / 40.0)
def cratio(r): return clamp((r - 1.0) / 19.0)
def attack(ms): return clamp(math.log(ms / 0.1) / math.log(1000.0))
def release(ms): return clamp(math.log(ms / 10.0) / math.log(100.0))
def makeup(db): return clamp(db / 24.0)
def lforate(f): return clamp(math.log(f / 0.01) / math.log(4000.0))

OUTPUTS = ["MAIN", "BUS A", "BUS B", "BUS C", "BUS D"]
def out(name): return step(OUTPUTS.index(name), 14)
SC = ["OFF"] + VOICES
def sc(name): return step(SC.index(name), 17)
DIVS = ["8 BAR", "8 BAR.", "8 BART", "4 BAR", "4 BAR.", "4 BART", "2 BAR", "2 BAR.", "2 BART", "1 BAR", "1 BAR.",
        "1 BART", "1/2", "1/2.", "1/2T", "1/4", "1/4.", "1/4T", "1/8", "1/8.", "1/8T", "1/16", "1/16.", "1/16T",
        "1/32", "1/32.", "1/32T", "1/64", "1/64.", "1/64T"]
def div(name): return step(DIVS.index(name), 30)
SHAPES = ["SIN", "TRI", "RAMP", "SAW", "SQR", "S&H"]
def shape(name): return step(SHAPES.index(name), 6)
DELAYS = ["1/16", "1/16.", "1/16T", "1/8", "1/8.", "1/8T", "1/4", "1/4.", "1/4T", "1/2", "1/2.", "1/2T"]
def delay(name): return step(DELAYS.index(name), 12)
SCALES = ["1/32", "1/16", "1/8T", "1/8"]

NOTE = {"C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3, "E": 4, "F": 5, "F#": 6, "Gb": 6, "G": 7, "G#": 8,
        "Ab": 8, "A": 9, "A#": 10, "Bb": 10, "B": 11}
def note(tok):  # "C3" = 48 = 0 V
    name, octv = (tok[:2], tok[2:]) if len(tok) > 2 and tok[1] in "#b" else (tok[:1], tok[1:])
    return 12 * (int(octv) + 1) + NOTE[name]

ACC = {"o": 1, "x": 2, "X": 3}

# ---------------------------------------------------------------- presets
# kit:    {param id: u}; voice LEVELs in dB via lvl() are trimmed together.
# drums:  {voice: "X...x...o"} one char per step (. off, o/x/X accent 1/2/3), repeated to the track length.
# notes:  {voice: "C2 . Eb2X - G2~"} tokens: note (+X/o accent, ~ = tie/glide into it), . rest, - hold the last note.
# len:    {voice: steps} track lengths (default: bar).  steps: {voice: {index: {field: value}}} per-step extras.
# mods:   [(src, dst, depth, curve)]
P = []

def preset(name, tempo, kit, drums, notes=None, bar=16, scale=None, swing_pct=None, lens=None, steps=None, mods=(),
           seed=None, track_swing=None):
    P.append(dict(name=name, tempo=tempo, kit=kit, drums=drums, notes=notes or {}, bar=bar, scale=scale,
                  swing=swing_pct, lens=lens or {}, steps=steps or {}, mods=list(mods), seed=seed,
                  track_swing=track_swing or {}))

preset("Deep Round Kick House", 122, swing_pct=54, kit={
    "BD1:TUNE": hz(35, 140, 47), "BD1:PITCH": st(0, 18, 7), "BD1:DECAY": dec(420), "BD1:ATTACK": pct(30),
    "BD1:FILTER": hz(200, 8000, 700), "BD1:LEVEL": lvl(0),
    "CP:FILTER": hz(400, 6000, 1300), "CP:DECAY": dec(170), "CP:SEND": pct(18), "CP:LEVEL": lvl(-5),
    "CH:DECAY": dec(30), "CH:TUNE": ratio(0.5, 2, 1.1), "CH:LEVEL": lvl(-9),
    "OH:DECAY": dec(200), "OH:LEVEL": lvl(-8),
    "RS:TUNE": hz(250, 2500, 900), "RS:LEVEL": lvl(-12),
    "BASS:CUTOFF": hz(80, 4000, 260), "BASS:RESO": pct(30), "BASS:ENV": pct(35), "BASS:DECAY": dec(240),
    "BASS:LEVEL": lvl(-3), "FX:DELAY TIME": delay("1/8."), "FX:DELAY FB": pct(30)},
    drums={"BD1": "X...X...X...X...", "CP": "....X.......X...", "OH": "..x...x...x...x.",
           "CH": "o.o.o.o.o.o.o.oo", "RS": "..........o....."},
    notes={"BASS": ". . C2 . . C2 . Eb2 . . C2X . G2 . F2 ."})

preset("Peak Time Warehouse", 132, kit={
    "BD1:TUNE": hz(35, 140, 52), "BD1:PITCH": st(0, 18, 10), "BD1:DECAY": dec(230), "BD1:DRIVE": drive(2.0),
    "BD1:FILTER": hz(200, 8000, 1800), "BD1:NOISE": pct(8), "BD1:LEVEL": lvl(-1),
    "BD2:TUNE": hz(45, 100, 50), "BD2:DECAY": dec(520), "BD2:TONE": pct(20), "BD2:LEVEL": lvl(-12),
    "BD2:OUTPUT": out("BUS A"),
    "CH:DECAY": dec(28), "CH:LEVEL": lvl(-10), "CH:OUTPUT": out("BUS A"),
    "OH:DECAY": dec(170), "OH:LEVEL": lvl(-9), "OH:OUTPUT": out("BUS A"),
    "RS:LEVEL": lvl(-11), "RS:OUTPUT": out("BUS A"), "RS:SEND": pct(20),
    "CP:DECAY": dec(150), "CP:SEND": pct(35), "CP:LEVEL": lvl(-6),
    "BUS A:COMP": tog(1), "BUS A:THRESH": thresh(-22), "BUS A:RATIO": cratio(6), "BUS A:ATTACK": attack(0.5),
    "BUS A:RELEASE": release(140), "BUS A:SC": sc("BD1"), "BUS A:MAKEUP": makeup(4),
    "MASTER:GLUE": pct(35), "MASTER:DRIVE": drive24(3), "FX:DELAY TIME": delay("1/8T")},
    drums={"BD1": "X...X...X...X...", "BD2": "..o...o...o...o.", "CH": "xoXoxoXoxoXoxoXo",
           "OH": "..x...x...x...x.", "CP": "....x.......x...", "RS": "...o..o....o..o."})

preset("Metallic Industrial", 128, kit={
    "BD1:TUNE": hz(35, 140, 56), "BD1:DECAY": dec(300), "BD1:WAVE": pct(62), "BD1:SYM 2": pct(70),
    "BD1:SHAPE": pct(35), "BD1:DRIVE": drive(4.0), "BD1:FILTER": hz(200, 8000, 2500), "BD1:LEVEL": lvl(-2),
    "SD:TUNE": hz(120, 400, 260), "SD:TONE": pct(30), "SD:SNAPPY": pct(80), "SD:SN.DEC": dec(180), "SD:LEVEL": lvl(-5),
    "LTC:WAVE": pct(70), "LTC:NZ": tog(1), "LTC:DECAY": dec(260), "LTC:TUNE": hz(70, 180, 85),
    "MTC:WAVE": pct(55), "MTC:NZ": tog(1), "MTC:DECAY": dec(220), "MTC:TUNE": hz(70, 180, 120),
    "HTC:WAVE": pct(45), "HTC:NZ": tog(1), "HTC:DECAY": dec(180), "HTC:TUNE": hz(70, 180, 160),
    "TOM:NOISE": pct(65), "LTC:LEVEL": lvl(-7), "MTC:LEVEL": lvl(-7), "HTC:LEVEL": lvl(-7),
    "LTC:OUTPUT": out("BUS B"), "MTC:OUTPUT": out("BUS B"), "HTC:OUTPUT": out("BUS B"),
    "BUS B:DRIVE": drive24(9), "BUS B:TONE": bip(30),
    "CB:TUNE": hz(300, 1200, 820), "CB:DECAY": dec(260), "CB:LEVEL": lvl(-12),
    "CY:TUNE": ratio(0.5, 2, 0.7), "CY:TONE": pct(80), "CY:DECAY": dec(600), "CY:LEVEL": lvl(-12),
    "CH:DECAY": dec(22), "CH:TUNE": ratio(0.5, 2, 1.4), "CH:LEVEL": lvl(-11)},
    drums={"BD1": "X..x..X...x.X...", "SD": "....X..o....X.o.", "LTC": "..x.......x.....",
           "MTC": ".....x.......x..", "HTC": "x.......x.....x.", "CB": "...x..x....x..x.",
           "CY": "X...............", "CH": "x.xxx.xxx.xxx.xx"},
    steps={"SD": {14: {"ratchet": 3}}, "CH": {2: {"prob": 0.75}, 6: {"prob": 0.75}, 10: {"prob": 0.75}},
           "BD1": {3: {"bend": -5}}})

preset("Electro Body Pop", 126, kit={
    "BD1:TUNE": hz(35, 140, 50), "BD1:PITCH": st(0, 18, 14), "BD1:DECAY": dec(520), "BD1:ATTACK": pct(60),
    "BD1:LEVEL": lvl(-1),
    "SD:TUNE": hz(120, 400, 240), "SD:SNAPPY": pct(65), "SD:LEVEL": lvl(-6),
    "CP:LEVEL": lvl(-7), "CH:DECAY": dec(24), "CH:LEVEL": lvl(-12),
    "CB:TUNE": hz(300, 1200, 560), "CB:DECAY": dec(110), "CB:LEVEL": lvl(-13),
    "CL:TUNE": hz(400, 3000, 1800), "CL:LEVEL": lvl(-13),
    "LEAD:SQR": tog(1), "LEAD:CUTOFF": hz(200, 8000, 2200), "LEAD:RESO": pct(40), "LEAD:DECAY": dec(110),
    "LEAD:ENV": pct(45), "LEAD:LEVEL": lvl(-4), "LEAD:SEND": pct(15),
    "BASS:CUTOFF": hz(80, 4000, 420), "BASS:RESO": pct(45), "BASS:DECAY": dec(150), "BASS:SQR": tog(1),
    "BASS:LEVEL": lvl(-4), "FX:DELAY TIME": delay("1/8")},
    drums={"BD1": "X.....X...X..X..", "SD": "....X.......X...", "CP": "....x.......x...",
           "CH": "xxxXxxxXxxxXxxxX", "CB": "..x.....x....x..", "CL": ".......x.......x"},
    notes={"LEAD": "C4 Eb4 G4 C5 . G4 Eb4 . C4X Eb4 G4 Bb4 . G4 F4 Eb4",
           "BASS": "C2 . C2 . . Bb1 . C2X . . G1 . Bb1 . C2 ."})

preset("Dusty Broken Beat", 98, swing_pct=58, kit={
    "BD1:TUNE": hz(35, 140, 58), "BD1:DECAY": dec(260), "BD1:NOISE": pct(18), "BD1:FILTER": hz(200, 8000, 600),
    "BD1:ATTACK": pct(55), "BD1:LEVEL": lvl(-1),
    "SD:TUNE": hz(120, 400, 210), "SD:TONE": pct(40), "SD:SNAPPY": pct(55), "SD:SN.DEC": dec(160), "SD:LEVEL": lvl(-4),
    "RS:TUNE": hz(250, 2500, 700), "RS:LEVEL": lvl(-11),
    "CH:DECAY": dec(45), "CH:TUNE": ratio(0.5, 2, 0.85), "CH:LEVEL": lvl(-11),
    "OH:DECAY": dec(260), "OH:LEVEL": lvl(-12),
    "GLOBAL:DRIFT": pct(50), "GLOBAL:TOLERANCE": pct(70), "MASTER:GLUE": pct(25)},
    drums={"BD1": "X.....xo..x.....", "SD": "....X..o.o..X..o", "RS": "..o......o....o.",
           "CH": "x.ox.ox.x.oxx.o.", "OH": "..............x."},
    steps={"BD1": {7: {"prob": 0.5}}, "SD": {7: {"prob": 0.6, "micro": 0.12}, 9: {"prob": 0.7, "micro": 0.1},
                                             15: {"prob": 0.5, "micro": 0.15}, 12: {"micro": 0.06}},
           "CH": {3: {"micro": -0.06}, 6: {"micro": 0.08}, 11: {"micro": 0.05}, 12: {"prob": 0.8}}})

preset("Head Nod Boom Bap", 90, swing_pct=62, kit={
    "BD1:TUNE": hz(35, 140, 60), "BD1:DECAY": dec(260), "BD1:ATTACK": pct(72), "BD1:NOISE": pct(14),
    "BD1:FILTER": hz(200, 8000, 1100), "BD1:LEVEL": lvl(0),
    "SD:TUNE": hz(120, 400, 190), "SD:SNAPPY": pct(62), "SD:SN.DEC": dec(210), "SD:TONE": pct(60),
    "SD:T.DECAY": dec(120), "SD:LEVEL": lvl(-3),
    "CH:DECAY": dec(60), "CH:TUNE": ratio(0.5, 2, 0.9), "CH:LEVEL": lvl(-10),
    "BASS:CUTOFF": hz(80, 4000, 200), "BASS:RESO": pct(15), "BASS:DECAY": dec(700), "BASS:ENV": pct(20),
    "BASS:LEVEL": lvl(-4)},
    drums={"BD1": "X.........X.x...", "SD": "....X.......X...", "CH": "x.x.x.x.x.x.x.xo"},
    notes={"BASS": "C2 . . . . . . . Eb2 . . G1 . . . ."},
    steps={"SD": {4: {"micro": 0.08}, 12: {"micro": 0.1}}, "CH": {15: {"prob": 0.5}}})

preset("Minimal Click Groove", 124, kit={
    "BD1:TUNE": hz(35, 140, 62), "BD1:DECAY": dec(120), "BD1:ATTACK": pct(85), "BD1:FILTER": hz(200, 8000, 3000),
    "BD1:LEVEL": lvl(-1),
    "RS:TUNE": hz(250, 2500, 600), "RS:LEVEL": lvl(-9),
    "CL:TUNE": hz(400, 3000, 1400), "CL:DECAY": dec(40), "CL:LEVEL": lvl(-11),
    "MA:DECAY": dec(30), "MA:LEVEL": lvl(-13),
    "CH:DECAY": dec(18), "CH:LEVEL": lvl(-12),
    "LFO 3:RATE": lforate(0.21), "LFO 3:SHAPE": shape("TRI")},
    drums={"BD1": "X...X...X...X...", "RS": "..x..x.o..x...x.", "CL": ".o.....x.o......",
           "MA": "o.o.o.o.o.o.o.o.", "CH": "...x..x....x...x"},
    steps={"RS": {2: {"locks": {"RS:TUNE": 0.35}}, 5: {"locks": {"RS:TUNE": 0.55}},
                  7: {"locks": {"RS:TUNE": 0.25}}, 10: {"locks": {"RS:TUNE": 0.62}},
                  14: {"locks": {"RS:TUNE": 0.45, "RS:PAN": 0.2}}},
           "CL": {1: {"locks": {"CL:TUNE": 0.7}}, 9: {"locks": {"CL:TUNE": 0.3, "CL:PAN": 0.8}}},
           "MA": {i: {"prob": 0.8} for i in (2, 6, 10, 14)}},
    mods=[("LFO 3", "MA:DECAY", 0.25, "LIN")])

preset("Conga Groove", 112, swing_pct=52, kit={
    "LTC:CONGA": tog(1), "MTC:CONGA": tog(1), "HTC:CONGA": tog(1),
    "LTC:TUNE": hz(70, 180, 95), "MTC:TUNE": hz(70, 180, 130), "HTC:TUNE": hz(70, 180, 170),
    "LTC:DECAY": dec(180), "MTC:DECAY": dec(150), "HTC:DECAY": dec(120),
    "LTC:LEVEL": lvl(-5), "MTC:LEVEL": lvl(-5), "HTC:LEVEL": lvl(-6),
    "CL:TUNE": hz(400, 3000, 2100), "CL:DECAY": dec(60), "CL:LEVEL": lvl(-8),
    "CB:TUNE": hz(300, 1200, 700), "CB:DECAY": dec(160), "CB:LEVEL": lvl(-13),
    "MA:DECAY": dec(45), "MA:LEVEL": lvl(-12),
    "BD1:TUNE": hz(35, 140, 55), "BD1:DECAY": dec(200), "BD1:LEVEL": lvl(-5),
    "CY:DECAY": dec(900), "CY:TONE": pct(35), "CY:LEVEL": lvl(-15)},
    drums={"CL": "X..X..X...X.X...", "CB": "x...x...x...x...", "MA": "oxoxoxoxoxoxoxox",
           "BD1": "X......X..X.....", "LTC": "....x.....x..x..", "MTC": "..x..x.......x..",
           "HTC": "x.o...x.o..x.o..", "CY": "X..............."},
    steps={"HTC": {0: {"flam": 1}, 11: {"flam": 2}}, "MTC": {13: {"flam": 5}}, "MA": {i: {"prob": 0.85} for i in (5, 9, 13)}})

preset("Half Time Pressure", 140, kit={
    "BD1:TUNE": hz(35, 140, 50), "BD1:DECAY": dec(300), "BD1:DRIVE": drive(1.5), "BD1:LEVEL": lvl(-1),
    "BD2:TUNE": hz(45, 100, 46), "BD2:DECAY": dec(900), "BD2:TONE": pct(15), "BD2:LEVEL": lvl(-8),
    "SD:TUNE": hz(120, 400, 230), "SD:SNAPPY": pct(70), "SD:SN.DEC": dec(260), "SD:LEVEL": lvl(-3), "SD:SEND": pct(15),
    "CH:DECAY": dec(26), "CH:LEVEL": lvl(-11),
    "BASS:CUTOFF": hz(80, 4000, 180), "BASS:RESO": pct(20), "BASS:DECAY": dec(1500), "BASS:ENV": pct(10),
    "BASS:GLIDE": glide(120), "BASS:LEVEL": lvl(-6), "FX:DELAY TIME": delay("1/4")},
    drums={"BD1": "X.........X.....", "BD2": "X.........x.....", "SD": "........X.......",
           "CH": "x.x.x.x.x.x.xxxx"},
    notes={"BASS": "C2 - - - - - - - Eb2~ - - - G1~ - - -"},
    steps={"CH": {12: {"ratchet": 2}, 13: {"ratchet": 3}, 14: {"ratchet": 4}, 15: {"ratchet": 6, "acc": 1},
                  4: {"ratchet": 2, "prob": 0.5}}})

preset("Slow Ballad Brushes", 72, bar=12, scale="1/8T", swing_pct=50, kit={
    "BD1:TUNE": hz(35, 140, 55), "BD1:DECAY": dec(380), "BD1:ATTACK": pct(20), "BD1:FILTER": hz(200, 8000, 500),
    "BD1:LEVEL": lvl(-4),
    "SD:TUNE": hz(120, 400, 200), "SD:TONE": pct(18), "SD:SNAPPY": pct(88), "SD:SN.DEC": dec(380),
    "SD:T.DECAY": dec(60), "SD:LEVEL": lvl(-6),
    "RS:TUNE": hz(250, 2500, 520), "RS:LEVEL": lvl(-10),
    "CY:TUNE": ratio(0.5, 2, 1.2), "CY:TONE": pct(30), "CY:DECAY": dec(1100), "CY:LEVEL": lvl(-12),
    "CH:DECAY": dec(70), "CH:LEVEL": lvl(-14),
    "LEAD:CUTOFF": hz(200, 8000, 900), "LEAD:RESO": pct(20), "LEAD:ENV": pct(20), "LEAD:DECAY": dec(700),
    "LEAD:GLIDE": glide(60), "LEAD:LEVEL": lvl(-3), "LEAD:SEND": pct(30),
    "BASS:CUTOFF": hz(80, 4000, 220), "BASS:DECAY": dec(900), "BASS:RESO": pct(10), "BASS:LEVEL": lvl(-6),
    "FX:DELAY TIME": delay("1/4"), "FX:DELAY FB": pct(40), "FX:DELAY LP": hz(2000, 12000, 3000)},
    drums={"BD1": "X.....x.....", "SD": "......x.....", "RS": "...o.....o..", "CY": "x.ox.ox.ox.o",
           "CH": "...x.....x.."},
    notes={"LEAD": "E4 . . G4 . A4 B4 - - A4~ - G4", "BASS": "C2 - - - - - A1 - - - - -"},
    steps={"SD": {6: {"micro": 0.08}}, "CY": {2: {"prob": 0.7}, 5: {"prob": 0.7}, 8: {"prob": 0.7}, 11: {"prob": 0.7}}})

preset("Seven Eight Stepper", 118, bar=14, kit={
    "BD1:TUNE": hz(35, 140, 54), "BD1:DECAY": dec(260), "BD1:LEVEL": lvl(-1),
    "SD:TUNE": hz(120, 400, 250), "SD:SNAPPY": pct(60), "SD:LEVEL": lvl(-5),
    "CH:DECAY": dec(32), "CH:LEVEL": lvl(-11), "CB:TUNE": hz(300, 1200, 640), "CB:DECAY": dec(90), "CB:LEVEL": lvl(-14),
    "BASS:CUTOFF": hz(80, 4000, 380), "BASS:RESO": pct(50), "BASS:DECAY": dec(170), "BASS:LEVEL": lvl(-5),
    "BASS:SQR": tog(1)},
    drums={"BD1": "X.......X.....", "SD": "....X.....X..o", "CH": "x.x.x.x.x.x.x.", "CB": "X...X...X....."},
    notes={"BASS": "D2 . D3 . F2 . D2 . A1X . C2 . D2 ."})

preset("Polymeter Drift", 126, kit={
    "BD1:TUNE": hz(35, 140, 50), "BD1:DECAY": dec(240), "BD1:WAVE": pct(25), "BD1:LEVEL": lvl(-1),
    "CH:DECAY": dec(30), "CH:LEVEL": lvl(-11), "RS:TUNE": hz(250, 2500, 800), "RS:LEVEL": lvl(-9),
    "CL:TUNE": hz(400, 3000, 1600), "CL:DECAY": dec(50), "CL:LEVEL": lvl(-11),
    "MA:DECAY": dec(40), "MA:LEVEL": lvl(-13),
    "LTC:WAVE": pct(40), "LTC:TUNE": hz(70, 180, 100), "LTC:DECAY": dec(200), "LTC:LEVEL": lvl(-7),
    "LFO 1:DIV": div("2 BAR"), "LFO 1:SHAPE": shape("TRI"),
    "LFO 2:RATE": lforate(0.13), "LFO 2:SHAPE": shape("SIN"),
    "LFO 3:SYNC": tog(1), "LFO 3:DIV": div("1/4."), "LFO 3:SHAPE": shape("S&H")},
    drums={"BD1": "X...X...X...X...", "CH": "x.o.x.o.x.o.", "RS": "x..o..x...", "CL": "x..x.o.",
           "MA": "xo.o.", "LTC": "..x...x.."},
    lens={"CH": 12, "RS": 10, "CL": 7, "MA": 5, "LTC": 9},
    mods=[("LFO 1", "BD1:WAVE", 0.3, "LIN"), ("LFO 2", "RS:TUNE", 0.2, "LIN"), ("LFO 3", "CL:DECAY", 0.3, "LIN"),
          ("LFO 1", "LTC:WAVE", -0.25, "S")])

preset("Acid Line Workout", 130, kit={
    "BD1:TUNE": hz(35, 140, 52), "BD1:DECAY": dec(260), "BD1:LEVEL": lvl(-1),
    "CH:DECAY": dec(30), "CH:LEVEL": lvl(-11), "OH:DECAY": dec(150), "OH:LEVEL": lvl(-11),
    "CP:LEVEL": lvl(-7),
    "BASS:SQR": tog(0), "BASS:CUTOFF": hz(80, 4000, 300), "BASS:RESO": pct(85), "BASS:ENV": pct(72),
    "BASS:DECAY": dec(180), "BASS:ACCENT": pct(80), "BASS:GLIDE": glide(70), "BASS:LEVEL": lvl(-6),
    "LFO 2:SYNC": tog(1), "LFO 2:DIV": div("2 BAR"), "LFO 2:SHAPE": shape("SIN")},
    drums={"BD1": "X...X...X...X...", "CH": "..x...x...x...x.", "OH": "......o.......o.", "CP": "....x.......x..."},
    notes={"BASS": "C2 C2 C3X C2 Eb2~ C2 . C2X G1 C2 C3~ Bb2 C2 . Eb2X F2~"},
    steps={"BASS": {2: {"locks": {"BASS:CUTOFF": 0.62}}, 7: {"locks": {"BASS:CUTOFF": 0.55}},
                    13: {"locks": {"BASS:CUTOFF": 0.7, "BASS:DECAY": 0.6}}}},
    mods=[("LFO 2", "BASS:CUTOFF", 0.22, "LIN"), ("ACC", "BASS:ENV", 0.15, "LIN")])

preset("Breakbeat Rush", 136, kit={
    "BD1:TUNE": hz(35, 140, 60), "BD1:DECAY": dec(200), "BD1:ATTACK": pct(70), "BD1:LEVEL": lvl(0),
    "SD:TUNE": hz(120, 400, 260), "SD:SNAPPY": pct(70), "SD:SN.DEC": dec(170), "SD:TONE": pct(55), "SD:LEVEL": lvl(-3),
    "CH:DECAY": dec(40), "CH:LEVEL": lvl(-10), "OH:DECAY": dec(240), "OH:LEVEL": lvl(-11),
    "CY:DECAY": dec(800), "CY:LEVEL": lvl(-14), "MASTER:GLUE": pct(30)},
    drums={"BD1": "X.X.......XX....", "SD": "....X..o.o..X..o", "CH": "x.x.x.x.x.x.x.x.",
           "OH": "..............x.", "CY": "X..............."},
    steps={"SD": {12: {"flam": 2}, 7: {"micro": 0.05}, 15: {"micro": 0.08, "prob": 0.7}},
           "BD1": {11: {"micro": 0.04}}})

preset("Two Step Shuffle", 132, swing_pct=66, kit={
    "BD1:TUNE": hz(35, 140, 55), "BD1:DECAY": dec(220), "BD1:LEVEL": lvl(-1),
    "SD:TUNE": hz(120, 400, 280), "SD:SNAPPY": pct(55), "SD:LEVEL": lvl(-5), "CP:LEVEL": lvl(-8),
    "RS:TUNE": hz(250, 2500, 1000), "RS:LEVEL": lvl(-11), "CH:DECAY": dec(28), "CH:LEVEL": lvl(-11),
    "BASS:CUTOFF": hz(80, 4000, 240), "BASS:DECAY": dec(320), "BASS:RESO": pct(25), "BASS:LEVEL": lvl(-5),
    "BASS:GLIDE": glide(40)},
    drums={"BD1": "X.........X..x..", "SD": "....X.......X...", "CP": "....x.......x...",
           "RS": ".......o......o.", "CH": "x.xxx.xxx.xxx.xx"},
    notes={"BASS": "G1 . . G1 . . Bb1~ . . . F1 . . G1 . ."},
    steps={"CH": {i: {"prob": 0.7} for i in (3, 7, 11, 15)}, "BD1": {13: {"prob": 0.6}}})

preset("Lead Arp Sequence", 120, kit={
    "BD1:TUNE": hz(35, 140, 52), "BD1:DECAY": dec(240), "BD1:LEVEL": lvl(-3),
    "CH:DECAY": dec(30), "CH:LEVEL": lvl(-13), "CP:LEVEL": lvl(-9),
    "LEAD:SQR": tog(0), "LEAD:CUTOFF": hz(200, 8000, 1500), "LEAD:RESO": pct(55), "LEAD:ENV": pct(50),
    "LEAD:DECAY": dec(160), "LEAD:LEVEL": lvl(0), "LEAD:SEND": pct(25),
    "BASS:CUTOFF": hz(80, 4000, 300), "BASS:DECAY": dec(400), "BASS:LEVEL": lvl(-6),
    "LFO 1:DIV": div("1 BAR"), "LFO 1:SHAPE": shape("TRI"),
    "LFO 2:RATE": lforate(0.3), "LFO 2:SHAPE": shape("SIN"), "FX:DELAY TIME": delay("1/8.")},
    drums={"BD1": "X...X...X...X...", "CH": "..x...x...x...x.", "CP": "....x.......x..."},
    notes={"LEAD": "A3 C4 E4 A4 E4 C4 A3 E4 F3 A3 C4 F4 G3 B3 D4 G4", "BASS": "A1 - - - . . . . F1 - - - G1 - - -"},
    steps={"LEAD": {3: {"locks": {"LEAD:OCT": step(2, 3)}}, 11: {"locks": {"LEAD:OCT": step(2, 3)}},
                    15: {"locks": {"LEAD:OCT": step(2, 3)}}}},
    mods=[("LFO 1", "LEAD:CUTOFF", 0.35, "LIN"), ("LFO 2", "LEAD:PAN", 0.4, "LIN")])

preset("Dub Echo Chamber", 118, kit={
    "BD1:TUNE": hz(35, 140, 48), "BD1:DECAY": dec(340), "BD1:FILTER": hz(200, 8000, 500), "BD1:LEVEL": lvl(-1),
    "RS:TUNE": hz(250, 2500, 650), "RS:SEND": pct(70), "RS:LEVEL": lvl(-9),
    "CP:SEND": pct(45), "CP:LEVEL": lvl(-8), "CH:DECAY": dec(35), "CH:LEVEL": lvl(-13),
    "LEAD:CUTOFF": hz(200, 8000, 700), "LEAD:RESO": pct(35), "LEAD:DECAY": dec(90), "LEAD:ENV": pct(30),
    "LEAD:SEND": pct(65), "LEAD:LEVEL": lvl(-4),
    "BASS:CUTOFF": hz(80, 4000, 150), "BASS:DECAY": dec(600), "BASS:RESO": pct(10), "BASS:LEVEL": lvl(-5),
    "FX:DELAY TIME": delay("1/8."), "FX:DELAY FB": pct(65), "FX:DELAY LP": hz(2000, 12000, 2800),
    "MASTER:FX SEND": pct(70)},
    drums={"BD1": "X...X...X...X...", "RS": ".......x......o.", "CP": "............x...",
           "CH": "..o...o...o...o."},
    notes={"LEAD": ". . C4 . . . . . . . Eb4 . . . . .", "BASS": "C2 - . . C2 - . . Ab1 - . . Bb1 - . ."})

preset("Rolling Hat Half Step", 70, kit={
    "BD1:TUNE": hz(35, 140, 55), "BD1:DECAY": dec(200), "BD1:LEVEL": lvl(-3),
    "BD2:TUNE": hz(45, 100, 48), "BD2:DECAY": dec(1200), "BD2:TONE": pct(10), "BD2:LEVEL": lvl(-2),
    "CP:DECAY": dec(200), "CP:LEVEL": lvl(-5), "CH:DECAY": dec(25), "CH:LEVEL": lvl(-10),
    "OH:DECAY": dec(300), "OH:LEVEL": lvl(-12)},
    drums={"BD2": "X......x..X.....", "BD1": "X.........x.....", "CP": "........X.......",
           "CH": "x.x.x.xxx.x.x.xx", "OH": "..............o."},
    steps={"CH": {3: {"ratchet": 2}, 6: {"ratchet": 3}, 7: {"ratchet": 4, "acc": 1}, 11: {"ratchet": 6},
                  14: {"ratchet": 8, "acc": 1}, 15: {"ratchet": 3}},
           "BD2": {7: {"bend": -7}, 10: {"bend": 5}}})

preset("Shaker Percussion Circle", 116, kit={
    "BD1:TUNE": hz(35, 140, 58), "BD1:DECAY": dec(180), "BD1:LEVEL": lvl(-4),
    "MA:DECAY": dec(55), "MA:LEVEL": lvl(-9), "CB:TUNE": hz(300, 1200, 540), "CB:DECAY": dec(140), "CB:LEVEL": lvl(-12),
    "CL:TUNE": hz(400, 3000, 1900), "CL:DECAY": dec(55), "CL:LEVEL": lvl(-9),
    "LTC:TUNE": hz(70, 180, 90), "MTC:TUNE": hz(70, 180, 125), "HTC:TUNE": hz(70, 180, 165),
    "LTC:DECAY": dec(260), "MTC:DECAY": dec(220), "HTC:DECAY": dec(180),
    "LTC:LEVEL": lvl(-6), "MTC:LEVEL": lvl(-6), "HTC:LEVEL": lvl(-7), "RS:LEVEL": lvl(-11)},
    drums={"BD1": "X.......X..x....", "MA": "xoxoxoXoxoxoxoXo", "CB": "X..x..x...x.x...", "CL": "..x...x...x..x..",
           "LTC": "......x.......x.", "MTC": "...x.......x....", "HTC": "x.......x.....x.", "RS": "....x.......x..."},
    steps={"MA": {i: {"micro": m} for i, m in ((1, 0.05), (3, -0.04), (5, 0.06), (9, 0.04), (11, -0.05), (13, 0.07))},
           "CB": {3: {"locks": {"CB:TUNE": 0.55}}, 10: {"locks": {"CB:TUNE": 0.55}}},
           "HTC": {14: {"prob": 0.6}}, "MTC": {11: {"prob": 0.7, "micro": 0.06}}})

preset("Jungle Break Roller", 170, kit={
    "BD1:TUNE": hz(35, 140, 58), "BD1:DECAY": dec(180), "BD1:ATTACK": pct(70), "BD1:LEVEL": lvl(-1),
    "SD:TUNE": hz(120, 400, 300), "SD:SNAPPY": pct(72), "SD:SN.DEC": dec(140), "SD:LEVEL": lvl(-3),
    "CH:DECAY": dec(30), "CH:LEVEL": lvl(-11), "OH:DECAY": dec(200), "OH:LEVEL": lvl(-12),
    "RS:LEVEL": lvl(-11),
    "BASS:CUTOFF": hz(80, 4000, 160), "BASS:DECAY": dec(1200), "BASS:RESO": pct(10), "BASS:GLIDE": glide(90),
    "BASS:LEVEL": lvl(-5), "GLOBAL:DRIFT": pct(40)},
    drums={"BD1": "X.X.......X.....", "SD": "....X..o.o..X.xo", "CH": "x.xxx.x.x.xxx.x.", "OH": "..........x.....",
           "RS": "...o.....o......"},
    notes={"BASS": "E1 - - - - - - - G1~ - - - D1~ - - -"},
    steps={"SD": {14: {"ratchet": 2}, 15: {"ratchet": 3, "acc": 1}, 7: {"prob": 0.7}}, "CH": {3: {"prob": 0.6}}})

preset("Lo-Fi Tape Wobble", 84, swing_pct=57, kit={
    "BD1:TUNE": hz(35, 140, 56), "BD1:DECAY": dec(240), "BD1:FILTER": hz(200, 8000, 450), "BD1:NOISE": pct(20),
    "BD1:LEVEL": lvl(-1),
    "SD:TUNE": hz(120, 400, 180), "SD:TONE": pct(35), "SD:SNAPPY": pct(50), "SD:SN.DEC": dec(170), "SD:LEVEL": lvl(-4),
    "CH:DECAY": dec(50), "CH:TUNE": ratio(0.5, 2, 0.8), "CH:LEVEL": lvl(-12),
    "LEAD:CUTOFF": hz(200, 8000, 800), "LEAD:RESO": pct(15), "LEAD:ENV": pct(15), "LEAD:DECAY": dec(600),
    "LEAD:GLIDE": glide(30), "LEAD:LEVEL": lvl(-3),
    "BASS:CUTOFF": hz(80, 4000, 200), "BASS:DECAY": dec(500), "BASS:LEVEL": lvl(-5),
    "LFO 1:SYNC": tog(0), "LFO 1:RATE": lforate(0.45), "LFO 1:SHAPE": shape("SIN"),
    "LFO 2:RATE": lforate(0.31), "LFO 2:SHAPE": shape("TRI"),
    "GLOBAL:DRIFT": pct(75), "GLOBAL:TOLERANCE": pct(80), "GLOBAL:NOISE FLOOR": pct(20)},
    drums={"BD1": "X......x..X.....", "SD": "....X.......X...", "CH": "x.x.x.x.x.x.x.x."},
    notes={"LEAD": "E4 - - - D4 - - - C4 - - - B3 - G3 -", "BASS": "A1 . . . . . . . F1 . . . G1 . . ."},
    steps={"SD": {4: {"micro": 0.07}, 12: {"micro": 0.09}}, "CH": {i: {"prob": 0.85} for i in (2, 6, 10, 14)}},
    mods=[("LFO 1", "LEAD:TUNE", 0.06, "LIN"), ("LFO 2", "BASS:TUNE", 0.04, "LIN"), ("VEL", "SD:TONE", 0.2, "LIN")])

# ---------------------------------------------------------------- building the documents
def levels_db(kit):
    return {k: v for k, v in kit.items() if k.endswith(":LEVEL") and k.split(":")[0] in VOICES}

def build(p, number, trim_db):
    params = {}
    params["CLOCK:TEMPO"] = bpm(p["tempo"])
    params["CLOCK:BAR"] = step(p["bar"] - 1, 32)
    if p["scale"]: params["CLOCK:SCALE"] = step(SCALES.index(p["scale"]), 4)
    if p["swing"] is not None: params["CLOCK:SWING"] = swing(p["swing"])
    for k, u in p["kit"].items():
        if k.endswith(":LEVEL") and k.split(":")[0] in VOICES:
            # The engine's calibration already puts each voice at its role's level at noon (-9 dB); the kits'
            # level offsets from noon are authored generously and applied at 0.4.
            db = 20.0 * math.log10(max(1.4125 * u * u, 1e-9))
            g = 10 ** ((-9.0 + (db + 9.0) * LEVEL_SPREAD + trim_db) / 20.0)
            u = clamp(math.sqrt(g / 1.4125))
        params[k] = round(u, 4)
    params = {k: round(v, 4) for k, v in params.items()}
    tracks = []
    for v in VOICES:
        n = p["lens"].get(v, p["bar"])
        steps = {}
        if v in p["drums"]:
            s = p["drums"][v]
            for i in range(n):
                c = s[i % len(s)]
                if c != ".": steps[i] = {"i": i, "acc": ACC[c]}
        if v in p["notes"]:
            toks = p["notes"][v].split()
            last = 48
            for i in range(n):
                t = toks[i % len(toks)]
                if t == ".": continue
                if t == "-":
                    steps[i] = {"i": i, "note": last, "tie": True}
                    continue
                acc = 2
                tie = t.endswith("~")
                t = t.rstrip("~")
                if t[-1] in ACC: acc, t = ACC[t[-1]], t[:-1]
                last = note(t)
                steps[i] = {"i": i, "acc": acc, "note": last}
                if tie: steps[i]["tie"] = True
        for i, extra in p["steps"].get(v, {}).items():
            st = steps.setdefault(i, {"i": i, "on": False})
            for f, val in extra.items():
                if f == "locks": st["locks"] = {lk: round(lu, 4) for lk, lu in val.items()}
                else: st[f] = val
        tr = {"id": v, "len": n, "steps": [steps[i] for i in sorted(steps)]}
        if v in p["track_swing"]: tr["swing"] = p["track_swing"][v]
        tracks.append(tr)
    mods = [{"src": s, "dst": d, "depth": dp, "via": None, "curve": c, "on": True} for s, d, dp, c in p["mods"]]
    name = "%03d %s" % (number, p["name"])
    assert len(name) <= 31, name
    seq = {"pattern": name, "tracks": tracks}
    if p["seed"] is not None: seq["seed"] = p["seed"]
    return {"format": "shogun-patch", "version": 2, "name": name, "params": params, "mod": mods, "cables": [],
            "cvAmt": {}, "inLaw": {}, "seq": seq}

def run_tool(*args):
    return subprocess.run([TOOL, *args], check=True, capture_output=True, text=True).stdout

def main():
    if not os.path.exists(TOOL):
        sys.exit("build/factory_fmt missing: run `make factory`")
    trims = {p["name"]: 0.0 for p in P}
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "bank.json")
        for it in range(8):
            docs = [build(p, i + 2, trims[p["name"]]) for i, p in enumerate(P)]
            json.dump(docs, open(path, "w"))
            peaks = json.loads(run_tool("peaks", path, "8"))
            worst = max(abs(peaks[n] - TARGET_DB) for n in peaks)
            print("pass %d: peaks %.2f .. %.2f dBFS" % (it + 1, min(peaks.values()), max(peaks.values())))
            if worst <= TOL_DB: break
            for n in peaks: trims[n] += TARGET_DB - peaks[n]
        for n in peaks: print("  %-28s %7.2f dBFS  trim %+.2f dB" % (n, peaks[n], trims[n]))
        print(run_tool("canon", path, OUT).strip())

if __name__ == "__main__":
    main()
