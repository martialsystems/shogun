#!/usr/bin/env python3
"""Exports the SHOGUN panel layout (spec v2.2 mockups, design units 1200 x 672) to plugin/Source/PanelLayout.inc.

make_mockups.py (copied unchanged from the spec bundle) draws each tab with a small set of primitives. This script
records those primitive calls instead of SVG, attaches a binding to every control (parameter id, step, jack, live
display), drops the mockup's illustrative data (fake pattern, fake matrix rows, fake cables, lane bars), and writes a
C++ op table the editor draws and hit-tests. Re-run after a mockup change:  python3 plugin/layout/export_layout.py
"""
import math, os, random, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_mockups as mm  # noqa: E402

GRAD = {"url(#kb)": 1, "url(#kc)": 2, "url(#js)": 3, "url(#jn)": 4, "url(#pl)": 5, "url(#grain)": 6, "url(#ear)": 7}
KIND = ["TEXT", "RTEXT", "RULE", "BOX", "KNOB", "LED", "KEY", "LCD", "TOGGLE", "JACK", "RECT", "CIRCLE", "LINE", "PATH"]
VOICES = ["BD1", "BD2", "SD", "RS", "CP", "CL", "MA", "CB", "CH", "OH", "CY", "LTC", "MTC", "HTC", "LEAD", "BASS"]


def colour(c, opacity=1.0):
    if c is None or c == "none":
        return 0
    if c in GRAD:
        return GRAD[c]
    c = c.strip()
    if c.startswith("#"):
        h = c[1:]
        if len(h) == 3:
            h = "".join(ch * 2 for ch in h)
        a = int(round(255 * opacity))
        return (a << 24) | int(h, 16)
    if c in ("#fff", "white"):
        return 0xFFFFFFFF
    if c == "black":
        return 0xFF000000
    raise ValueError(c)


class Rec(mm.Svg):
    """Records primitive calls. Primitives draw through add(); while one runs, add() is ignored."""

    def __init__(s, tab):
        super().__init__()
        s.tab = tab
        s.ops = []
        s.depth = 0

    def op(s, kind, **kw):
        d = dict(kind=kind, tab=s.tab, bind="", hide=False)
        d.update(kw)
        s.ops.append(d)
        return d

    def add(s, x):
        if s.depth:
            return
        for m in re.finditer(r"<(rect|circle|line|path)\b([^>]*)/?>", x):
            tag, attrs = m.group(1), dict(re.findall(r'([\w-]+)="([^"]*)"', m.group(2)))
            f = lambda k, d=0.0: float(attrs.get(k, d))
            op_ = float(attrs.get("opacity", 1.0))
            fill = attrs.get("fill", "#000" if tag != "line" else "none")
            common = dict(fill=colour(fill), stroke=colour(attrs.get("stroke")), sw=f("stroke-width", 1.0), opacity=op_)
            if attrs.get("stroke-opacity"):
                common["stroke"] = colour(attrs.get("stroke"), float(attrs["stroke-opacity"]))
            if tag == "rect":
                s.op("RECT", x=f("x"), y=f("y"), w=f("width"), h=f("height"), r=f("rx"), **common)
            elif tag == "circle":
                s.op("CIRCLE", x=f("cx"), y=f("cy"), r=f("r"), **common)
            elif tag == "line":
                s.op("LINE", x=f("x1"), y=f("y1"), w=f("x2"), h=f("y2"), round=attrs.get("stroke-linecap") == "round", **common)
            else:
                nums = re.findall(r"-?\d+\.?\d*", attrs["d"])
                s.op("PATH", text=attrs["d"], x=float(nums[0]), y=float(nums[1]), round=attrs.get("stroke-linecap") == "round", **common)

    def _wrap(name):
        base = getattr(mm.Svg, name)

        def fn(s, *a, **k):
            s.depth += 1
            try:
                base(s, *a, **k)
            finally:
                s.depth -= 1
        return fn

    def text(s, x, y, t, z=9, a="middle", fill=mm.INK, w=700, ls=0.4):
        if s.depth:
            return
        s.op("TEXT", x=x, y=y, text=t, z=z, anchor={"start": 0, "middle": 1, "end": 2}[a], fill=colour(fill), weight=w, ls=ls)

    def rtext(s, x, y, t, z=8.5, pad=3):
        s.op("RTEXT", x=x, y=y, text=t, z=z)

    def rule(s, x1, y1, x2, y2, w=1.2, c=mm.GRN):
        s.op("RULE", x=x1, y=y1, w=x2, h=y2, sw=w, stroke=colour(c))

    def box(s, x, y, w, h, title=None, tz=9.5):
        s.op("BOX", x=x, y=y, w=w, h=h, text=title or "", z=tz)

    def knob(s, cx, cy, r=14, label=None, v=None, lz=8.5, ticks=11, steps=None, ring=None):
        v = random.uniform(0.15, 0.85) if v is None else v
        s.op("KNOB", x=cx, y=cy, r=r, text=label or "", z=lz, v=v, ticks=ticks, steps=steps or 0,
             ring=ring if ring is not None else float("nan"))

    def led(s, cx, cy, on=False, c=mm.RED, r=3.2):
        s.op("LED", x=cx, y=cy, r=r, on=on, fill=colour(c))

    def key(s, x, y, w, h, label=None, on=False, lit=None, z=8.5, fill="#0a0a0a"):
        s.op("KEY", x=x, y=y, w=w, h=h, text=label or "", lit=colour(lit) if lit else 0, z=z, fill=colour(fill))

    def lcd(s, x, y, w, h, t, z=11, a="start", c="#7ee08c"):
        s.op("LCD", x=x, y=y, w=w, h=h, text=t, z=z, anchor={"start": 0, "middle": 1, "end": 2}[a], fill=colour(c))

    def toggle(s, cx, cy, l, r, on_right=False, z=7.5):
        s.op("TOGGLE", x=cx, y=cy, text=l, text2=r, on=on_right, z=z)

    def jack(s, cx, cy, label, out=False, normal=None, patched=False, z=8):
        s.op("JACK", x=cx, y=cy, text=label, text2=normal or "", out=out, z=z)


# ---------------------------------------------------------------- bindings
VOICE_LABELS = {  # VOICE tab / voice block: (section, column, label) → param (column only matters where labels repeat)
    "BD 1": {"TUNE": "BD1:TUNE", "PITCH": "BD1:PITCH", "DECAY": "BD1:DECAY", "ATTACK": "BD1:ATTACK", "SOUND": "BD1:SOUND",
             "NOISE": "BD1:NOISE", "FILTER": "BD1:FILTER", "DRIVE": "BD1:DRIVE", "WAVE": "BD1:WAVE", "LEVEL": "BD1:LEVEL"},
    "BD 2": {"TUNE": "BD2:TUNE", "DECAY": "BD2:DECAY", "TONE": "BD2:TONE", "WAVE": "BD2:WAVE", "LEVEL": "BD2:LEVEL"},
    "SNARE · RIM": {"TUNE": "SD:TUNE", "DETUNE": "SD:DETUNE", "PITCH": "SD:PITCH", "T.DECAY": "SD:T.DECAY", "TONE": "SD:TONE",
                    "SNAPPY": "SD:SNAPPY", "SN.DEC": "SD:SN.DEC", "LEVEL": "SD:LEVEL", "RIM TUNE": "RS:TUNE", "RIM LVL": "RS:LEVEL"},
    "CLAP · CLAVES": {"ATTACK": "CP:ATTACK", "SOUND": "CP:SOUND", "COUNT": "CP:COUNT", "FILTER": "CP:FILTER", "DECAY": "CP:DECAY",
                      "LEVEL": "CP:LEVEL", "CL TUNE": "CL:TUNE", "CL DECAY": "CL:DECAY", "CL LEVEL": "CL:LEVEL"},
    "CB · MA": {"CB TUNE": "CB:TUNE", "CB DECAY": "CB:DECAY", "CB LEVEL": "CB:LEVEL", "MA DECAY": "MA:DECAY", "MA LEVEL": "MA:LEVEL"},
    "HATS · CYMBAL": {"HAT TUNE": "CH:TUNE", "CH DECAY": "CH:DECAY", "OH DECAY": "OH:DECAY", "CH LEVEL": "CH:LEVEL",
                      "OH LEVEL": "OH:LEVEL", "CY TUNE": "CY:TUNE", "CY TONE": "CY:TONE", "CY DECAY": "CY:DECAY", "CY LEVEL": "CY:LEVEL"},
    "LEAD": {"CUTOFF": "LEAD:CUTOFF", "RESO": "LEAD:RESO", "ENV": "LEAD:ENV", "DECAY": "LEAD:DECAY", "LEVEL": "LEAD:LEVEL",
             "OCT −1 0 +1": "LEAD:OCT", "TUNE": "LEAD:TUNE", "GLIDE": "LEAD:GLIDE", "ACCENT": "LEAD:ACCENT", "SAW/SQR": "LEAD:SQR"},
    "BASS": {"CUTOFF": "BASS:CUTOFF", "RESO": "BASS:RESO", "ENV": "BASS:ENV", "DECAY": "BASS:DECAY", "LEVEL": "BASS:LEVEL",
             "OCT −1 0 +1": "BASS:OCT", "TUNE": "BASS:TUNE", "GLIDE": "BASS:GLIDE", "ACCENT": "BASS:ACCENT", "SAW/SQR": "BASS:SQR"},
}
STEP_KNOBS = {"ACCENT": "acc", "FLAM": "flam", "RATCHET": "ratchet", "PROB": "prob", "MICRO": "micro", "BEND": "bend", "NOTE": "note"}


def label_below(ops, k):
    """A knob drawn without a label (stepped knobs with an LCD) is named by the text right under it."""
    best = None
    for o in ops:
        if o["kind"] == "TEXT" and 10 < o["y"] - k["y"] < 40 and abs(o["x"] - k["x"]) < 14:
            if best is None or o["y"] < best["y"]:
                best = o
    return best


def bind_chrome(ops):
    # Item P: the transport (PLAY, its light, RST) gets its own TRANSPORT label and a divider from the CLOCK settings.
    extra = []
    for o in ops:
        if o["kind"] == "TEXT" and o.get("text") == "CLOCK" and o["x"] == 16 and o["y"] == 48:
            clk = dict(o)
            clk["x"] = 101
            extra.append(clk)
            o["text"] = "TRANSPORT"
        elif o["kind"] == "RULE" and o["x"] == 534 and o["w"] == 534:
            div = dict(o)
            div["x"] = div["w"] = 95
            extra.append(div)
    ops.extend(extra)
    for o in ops:
        if o["y"] > 90:
            continue
        k, t = o["kind"], o.get("text", "")
        if k == "KNOB":
            lab = label_below(ops, o)
            name = lab["text"] if lab else ""
            m = {"TEMPO": "CLOCK:TEMPO", "SWING": "CLOCK:SWING", "SCALE": "CLOCK:SCALE", "BAR": "CLOCK:BAR",
                 "ACCENT": "MASTER:ACCENT", "DRIVE": "MASTER:DRIVE", "VOLUME": "MASTER:VOLUME"}
            if name in m:
                o["bind"] = "p:" + m[name]
        elif k == "LCD":
            m = {"120.0": "disp:CLOCK:TEMPO", "1/16": "disp:CLOCK:SCALE", "16": "disp:CLOCK:BAR", "05:16 ▮": "pos",
                 "INIT": "kit", "001 USER  A": "pattern"}
            o["bind"] = m.get(t, "")
        elif k == "TOGGLE" and t == "INT":
            o["bind"] = "p:CLOCK:MODE"
        elif k == "KEY" and o["y"] < 30:
            # program row: KIT ◀ ▶ ⌕ (x 774/794/814) and PATTERN ◀ ▶ ⌕ (x 1012/1032/1052) step / browse the factory
            # bank (a program is a kit and its pattern); A/B compare; undo / redo
            m = {"◀": "prog:-1", "▶": "prog:1", "⌕": "browse", "A": "ab:0", "B": "ab:1", "↶": "undo", "↷": "redo"}
            o["bind"] = m.get(t, "")
            o["lit"] = 0
        elif k == "KEY":
            # transport and the SYNC box (was PERFORM: FILL / ROLL / MUTE GRP / SCENE A). The engine has no fill, roll,
            # mute groups or scenes, so the box carries the two clock controls instead: CLK IN and SRC.
            if t == "▶":  # a labelled PLAY key (was a bare ▶ glyph the user could not find)
                o.update(text="▶ PLAY", x=14, w=40, z=7.5, bind="run")
            elif t == "RST":
                o.update(x=65, w=26, bind="rst")
            elif t == "FILL":
                o.update(text="CLK IN: STEP", x=546, w=118, bind="disp:CLOCK:CLK IN")
            elif t == "ROLL":
                o.update(text="SRC HOST", x=670, w=118, bind="src")
            elif t in ("MUTE GRP", "SCENE A"):
                o["hide"] = True
        elif k == "TEXT" and t == "PERFORM":
            o["text"] = "SYNC"
        elif k == "LED" and o["x"] == 52:
            o.update(x=59, bind="runled")
        elif k == "LED" and o["x"] == 1140 and abs(o["y"] - 62) < 1:
            o["bind"] = "clipled"
        elif k == "RECT" and o["x"] >= 959 and o["x"] < 1130 and o["w"] < 5 and o["y"] < 80:
            o["hide"] = True  # fake meter segments; the live meter is drawn by the editor
        elif k == "TEXT" and t == "CPU 7%":
            o["bind"] = "cpu"
        elif k == "LED" and o["x"] == 1140:
            o["bind"] = "cpuled"
    # the live meter frames
    for o in ops:
        if o["kind"] == "RECT" and o["x"] == 958 and o["w"] == 170:
            o["bind"] = "meter:%d" % (0 if o["y"] < 60 else 1)


def bind_voice_block(ops, y0, y1):
    """Sections are announced by their box + name key; a knob's column inside the section picks LTC/MTC/HTC."""
    sec = None
    secx = 0.0
    pitch = (mm.W - 20 - 8 * 6) / sum(len(c) for _, c in mm.VOICE_SECTIONS)
    for i, o in enumerate(ops):
        if not (y0 <= o.get("y", 0) <= y1 + 10):
            continue
        k, t = o["kind"], o.get("text", "")
        if k == "BOX" and abs(o["y"] - (y0 + 6)) < 0.5:
            secx = o["x"]
        if k == "KEY" and abs(o["y"] - (y0 + 1)) < 0.5:
            sec = t
            o["bind"] = "sel:" + {"BD 1": "BD1", "BD 2": "BD2", "SNARE · RIM": "SD", "CLAP · CLAVES": "CP", "CB · MA": "CB",
                                  "HATS · CYMBAL": "CH", "TOMS · CONGAS": "LTC", "LEAD": "LEAD", "BASS": "BASS"}[t]
            o["lit"] = 0
            continue
        if k == "LED" and sec and abs(o["y"] - (y0 + 8)) < 0.5:
            # the group key's corner light: red = activity, steady green = muted; a click toggles the group's mute
            o["bind"] = "vmute:" + "+".join(VOICE_GROUPS[sec])
            continue
        if sec is None:
            continue
        col = int((o.get("x", 0) - secx) // pitch)
        if k == "KNOB":
            name = t
            if not name:
                lab = label_below(ops, o)
                name = lab["text"] if lab else ""
            if sec == "TOMS · CONGAS":
                v = ["LTC", "MTC", "HTC"][min(col, 2)]
                pid = {"TUNE": v + ":TUNE", "DECAY": v + ":DECAY", "WAVE": v + ":WAVE", "LEVEL": v + ":LEVEL", "TOM NZ": "TOM:NOISE"}.get(name, "")
            else:
                pid = VOICE_LABELS.get(sec, {}).get(name, "")
            if pid:
                o["bind"] = "p:" + pid
                o["ring"] = float("nan")
        elif k == "LCD" and sec:
            # stepped knob read-out (SOUND / COUNT) sits right of its knob
            for kk in ops[max(0, i - 3):i]:
                if kk["kind"] == "KNOB" and kk["bind"]:
                    o["bind"] = "disp:" + kk["bind"][2:]
        elif k == "TOGGLE":
            if t == "SAW":
                o["bind"] = "p:" + VOICE_LABELS[sec]["SAW/SQR"]
            elif t == "TOM":
                o["bind"] = "p:%s:CONGA" % ["LTC", "MTC", "HTC"][min(col, 2)]
            elif t2(o) == "NZ":
                o["bind"] = "p:%s:NZ" % ["LTC", "MTC", "HTC"][min(col, 2)]


def t2(o):
    return o.get("text2", "")


VOICE_GROUPS = {"BD 1": ["BD1"], "BD 2": ["BD2"], "SNARE · RIM": ["SD", "RS"], "CLAP · CLAVES": ["CP", "CL"],
                "CB · MA": ["CB", "MA"], "HATS · CYMBAL": ["CH", "OH", "CY"], "TOMS · CONGAS": ["LTC", "MTC", "HTC"],
                "LEAD": ["LEAD"], "BASS": ["BASS"]}


def o_bind_voice(sec):
    return {"BD 1": "BD1", "BD 2": "BD2", "SNARE · RIM": "SD", "CLAP · CLAVES": "CP", "CB · MA": "CB",
            "HATS · CYMBAL": "CH", "TOMS · CONGAS": "LTC", "LEAD": "LEAD", "BASS": "BASS"}[sec]


def bind_main(ops):
    bind_chrome(ops)
    # Item F: an activity light under each drum / synth name key (red, as on the VOICE group keys)
    led = next(o for o in ops if o["kind"] == "LED" and o.get("bind") == "runled")
    for o in list(ops):
        if o["kind"] == "KEY" and o.get("text") in VOICES and 102 < o["y"] < 140:
            a = dict(led)
            a.update(x=o["x"] + o["w"] / 2, y=o["y"] + o["h"] + 7, r=2.5, fill=colour("#E0402E"), bind="act:" + o["text"])
            ops.append(a)
    X0, X1 = 10, mm.W - 10
    pitch = (X1 - X0) / 16
    steps = 0
    for o in ops:
        k, t, y = o["kind"], o.get("text", ""), o.get("y", 0)
        if 102 < y < 340:
            if k == "KEY" and t in VOICES:
                o["bind"] = "sel:" + t
                o["lit"] = 0
            elif k == "KNOB":
                v = VOICES[int((o["x"] - X0) // pitch)]
                # MAIN strip labels that are not that voice's knob names (§11.4 strips are TUNE/DECAY/LEVEL for all)
                remap = {("SD", "DECAY"): ("SD:T.DECAY", "T.DECAY"), ("OH", "TUNE"): ("CH:TUNE", "HAT TUNE"),
                         ("CP", "TUNE"): ("CP:FILTER", "FILTER"), ("RS", "DECAY"): ("RS:PAN", "PAN"), ("MA", "TUNE"): ("MA:PAN", "PAN")}
                if (v, t) in remap:
                    pid, lab = remap[(v, t)]
                    o["text"] = lab
                    o["bind"] = "p:" + pid if pid else "off"
                else:
                    o["bind"] = "p:%s:%s" % (v, t)
            elif k == "BOX" and t == "VOICES":
                pass
        elif 352 <= y < 522:
            if k == "BOX":
                o["text"] = "STEPS · %s"
                o["bind"] = "title:sel"
            elif k == "LCD" and "STEPS" in t:
                o["bind"] = "trackinfo"
            elif k == "LCD" and "LEN" in t:
                o["bind"] = "trackscale"
            elif k == "KEY" and t in ("1–16", "17–32"):
                o["bind"] = "page:%d" % (0 if t == "1–16" else 1)
                o["lit"] = 0
            elif k == "KEY" and t in ("◀ TRK", "TRK ▶"):
                o["bind"] = "trk:%d" % (-1 if t.startswith("◀") else 1)
            elif k == "KEY" and not t and o["h"] == 52:
                o["bind"] = "step:%d" % steps
                o["lit"] = 0
                steps += 1
            elif k == "LED":
                o["bind"] = "playled:%d" % (steps - 1)
                o["on"] = False
            elif k == "CIRCLE" and o.get("r") == 2.4:
                o["hide"] = True
            elif k == "TEXT" and t.isdigit() and o["y"] > 460:
                o["bind"] = "stepnum:%d" % (int(t) - 1)
            elif k == "TEXT" and t.startswith("click = step on/off"):
                o["text"] = "click = step on/off  ·  shift-click = accent  ·  right-click = select  ·  full 16-track grid on GRID"
        elif y >= 536:
            if k == "BOX":
                o["text"] = "STEP %s · LOCKS"
                o["bind"] = "title:step"
            elif k == "KNOB" and t in STEP_KNOBS:
                o["bind"] = "sk:" + STEP_KNOBS[t]
            elif k == "KEY":
                o["bind"] = {"COPY": "copy", "PASTE": "paste", "CLEAR": "clear", "RANDOM": "random"}.get(t, "")


def bind_voice(ops):
    bind_chrome(ops)
    bind_voice_block(ops, 100, 500)
    stage = {"WAVE 1": "WAVE 1", "WAVE 2": "WAVE 2", "WAVE 3": "WAVE 3", "SYM 1": "SYM 1", "SYM 2": "SYM 2", "SYM 3": "SYM 3"}
    for o in ops:
        k, t, y = o["kind"], o.get("text", ""), o.get("y", 0)
        if y < 505:
            continue
        if k == "BOX" and t.startswith("SELECTED VOICE"):
            o["text"] = "SELECTED VOICE · %s"
            o["bind"] = "title:sel"
        elif k == "BOX" and t.startswith("WAVE"):
            o["text"] = "WAVE · TRIPLE WAVE SHAPER · %s · stages in series, each adds a fold · front WAVE = macro"
            o["bind"] = "title:wave"
        elif k == "KNOB" and o["x"] < 400:
            o["bind"] = "sp:" + {"VEL → LEVEL": "VEL>LEVEL", "VEL → DECAY": "VEL>DECAY", "ACC AMT": "ACC AMT", "PAN": "PAN",
                                 "CHOKE": "CHOKE", "CV AMT": "cvamt"}[t]
        elif k == "KNOB":
            col = 0 if o["x"] < 510 else 1 + int((o["x"] - 510) // 190)
            if t == "SHAPE":
                o["bind"] = "wp:SHAPE"
            elif t in stage:
                o["bind"] = "wp:" + stage[t]
            elif t in ("VC → AMT", "VC → SYM"):
                o["bind"] = "wp:%s %d" % ("VC>AMT" if "AMT" in t else "VC>SYM", col)
            o["ring"] = float("nan")
        elif k == "TOGGLE" and t == "POST":
            o["bind"] = "wp:ROUTING"
        elif k == "TOGGLE" and t2(o) == "LEVEL COMP":
            o["bind"] = "wp:LEVEL COMP"
            o["on"] = True
        elif k == "LCD" and t == "BODY":
            o["bind"] = "wp:VC SRC"


def bind_grid(ops):
    bind_chrome(ops)
    rows = VOICES
    SY = 100
    rh = 26.5
    gx0 = 150
    cw = (mm.W - 12 - gx0) / 32
    for o in ops:
        k, t, y = o["kind"], o.get("text", ""), o.get("y", 0)
        if SY + 14 <= y < SY + 16 + rh * 16:
            ri = int((y - (SY + 16) + 1) // rh)
            ri = max(0, min(15, ri))
            # The playhead column (over step 5, the full grid height) first: it also starts at x >= gx0 and is
            # narrower than a cell, and the step-cell rule used to bind it as one giant step-5 cell of row 1.
            if k == "RECT" and abs(o["x"] - (gx0 + cw * 4)) < 0.1 and o.get("h", 0) > rh:
                o["bind"] = "playhead"
            elif k == "RECT" and o["x"] >= gx0 and o["w"] < cw and o.get("h", 0) < rh:
                o["bind"] = "grid:%d:%d" % (ri, int((o["x"] - gx0) // cw))
                o["fill"] = colour("#0b0c0b")
                o["opacity"] = 1.0
            elif k == "RECT" and o["x"] == 12:
                o["bind"] = "gridsel:%d" % ri
            elif k == "KEY" and o["x"] == 80:
                o["bind"] = "p:%s:MUTE" % rows[ri]
                o["lit"] = 0
            elif k == "KEY" and o["x"] == 97:
                o["bind"] = "p:%s:SOLO" % rows[ri]
            elif k == "LCD":
                o["bind"] = "len:%d" % ri
            elif k == "TEXT" and o["x"] == 18:
                o["bind"] = "gridname:%d" % ri
                o["fill"] = colour(mm.INK)
            elif k == "TEXT" and t in ("×3", "fl"):
                o["hide"] = True
            elif k == "CIRCLE" and o.get("r") == 1.8:
                o["hide"] = True
        elif y > SY + rh * 16 + 20:
            if k == "LCD" and "STEP" in t:
                o["bind"] = "stepinfo"
            elif k == "LCD" and "LOCKS" in t:
                o["bind"] = "lockinfo"
            elif k == "KNOB" and t in STEP_KNOBS:
                o["bind"] = "sk:" + STEP_KNOBS[t]
            elif k == "KEY" and t == "LOCK RND":
                # no lock randomiser in the engine: the slot carries the pattern's MIDI drag-out handle (item 2a)
                o.update(text="DRAG MIDI", bind="mididrag")
            elif k == "KEY":
                o["bind"] = {"TIE": "sk:tie", "LOCKS ✕": "clearlocks", "COPY": "copy", "PASTE": "paste", "CLEAR": "clear",
                             "SHIFT ◀": "shiftl", "SHIFT ▶": "shiftr", "RANDOM": "random"}.get(t, "")
            elif k == "TEXT" and t.startswith("hold a step"):
                o["text"] = "click = on → accent → off  ·  shift / right-click = select  ·  knobs edit the selected step"


def bind_mod(ops):
    bind_chrome(ops)
    for o in ops:
        k, t, x, y = o["kind"], o.get("text", ""), o.get("x", 0), o.get("y", 0)
        if 96 <= y < 330:
            li = int((x - 10) // 296)
            L = "LFO %d" % (li + 1)
            if k == "KNOB":
                o["bind"] = "p:%s:%s" % (L, {"RATE": "RATE", "SHAPE": "SHAPE", "PHASE": "PHASE", "SLEW": "SLEW", "PW / SKEW": "PW"}[t])
            elif k == "LCD" and y < 260:
                o["bind"] = "lforate:%d" % li
            elif k == "LCD":
                o["bind"] = "disp:%s:RETRIG BY" % L
            elif k == "TOGGLE":
                o["bind"] = "p:%s:%s" % (L, "SYNC" if t == "FREE" else "POL")
            elif k == "KEY" and t in ("FREE-RUN", "RETRIG", "ONE-SHOT"):
                o["bind"] = "lfomode:%d:%d" % (li, ["FREE-RUN", "RETRIG", "ONE-SHOT"].index(t))
                o["lit"] = 0
            elif k == "PATH":
                o["bind"] = "lfoscope:%d" % li
            elif k == "TEXT" and t in ("SIN", "TRI", "SAW", "SQR", "S&H"):
                o["bind"] = "disp:%s:SHAPE" % L
            elif k == "KEY" and t.startswith("◉ LFO"):
                o["bind"] = "assign:%d" % (li + 1)  # mod::SRC_LFO1 + li
                o["lit"] = 0
            elif k == "TEXT" and t == "DRAG ▸":
                o["text"] = "ASSIGN ▸"
        elif 360 <= y < 650 and x < 790:
            if not (k == "TEXT" and abs(y - 356) < 1):
                o["hide"] = True  # illustrative rows: the editor draws the engine's rows
        elif x >= 798 and y >= 336:
            # PER-VOICE SOURCES: the six engine per-voice sources are assign keys. LANE A/B (and the lane editor
            # below them) are not in the engine, so they are gone; the freed space holds the global sources.
            src = {"◉ ENV": 5, "◉ PITCH ENV": 6, "◉ VEL": 7, "◉ ACC": 8, "◉ RND/HIT": 9, "◉ NOTE": 10}  # mod::SRC_*
            if k == "BOX":
                o["h"] = 116
            elif k == "KEY" and t in src:
                o["bind"] = "assign:%d" % src[t]
                o["lit"] = 0
                if t == "◉ PITCH ENV":
                    o["x"] = 812
                elif t == "◉ NOTE":
                    o["x"] = 906
            elif k == "TEXT" and y < 380:
                pass
            else:
                o["hide"] = True  # LANE A/B keys, lane title, bars, SLEW/LEN/DEPTH, DRAW/CLEAR/RANDOM/LANE B
        elif k == "TEXT" and t.startswith("right-click any knob"):
            o["text"] = ("ASSIGN: click a ◉ source, then a knob = new row at +50 %  ·  "
                         "drag DEPTH sideways  ·  click SOURCE / DEST / VIA / CURVE to change  ·  ✕ clears")
    ops.append(dict(kind="RECT", tab="MOD", x=14, y=364, w=772, h=284, r=0, fill=0, stroke=0, sw=0, opacity=1.0,
                    bind="matrix", hide=False))
    gl = dict(kind="BOX", tab="MOD", x=798, y=462, w=392, h=202, text="GLOBAL SOURCES  ·  one set for the machine", z=9.5,
              bind="", hide=False)
    ops.append(gl)
    for i, (lab, s) in enumerate((("◉ RND", 11), ("◉ MOD W", 12), ("◉ AT", 13))):
        ops.append(dict(kind="KEY", tab="MOD", x=812 + 94 * i, y=496, w=88, h=22, text=lab, lit=0, z=8.5,
                        fill=colour("#0a0a0a"), bind="assign:%d" % s, hide=False))
    for i, line in enumerate(("RND = a new random value every step.", "MOD W = MIDI CC 1.  AT = channel aftertouch.",
                              "Click any ◉ key, then click a knob: the matrix gains",
                              "a row from that source to that knob at +50 %.",
                              "Click the lit key again to cancel.")):
        ops.append(dict(kind="TEXT", tab="MOD", x=812, y=548 + 16 * i, text=line, z=8, anchor=0,
                        fill=colour(mm.DIM if i < 2 else mm.INK), weight=600, ls=0.4, bind="", hide=False))


def bind_route(ops):
    bind_chrome(ops)
    colw = 948 / 16.0
    for o in ops:
        k, t, x, y = o["kind"], o.get("text", ""), o.get("x", 0), o.get("y", 0)
        if k == "JACK" and t in ("FILL IN", "LANE A"):  # mockup jacks that are not SHOGUN ports (engine/ports.h)
            o["hide"] = True
            continue
        if k == "JACK" and x > 960 and abs(y - 292) < 1 and t in ("LD GATE", "BS GATE"):
            o["x"] = x = x - 52
        if k == "TEXT" and t.startswith("LFO/RND/LANE"):
            o["text"] = "LFO/RND jacks are extras:"
        if k == "TEXT" and t.startswith("right-click any jack"):
            # item I (text only): outside the rack the cables patch SHOGUN to itself
            o["text"] = ("outside the rack, cables patch SHOGUN to itself: drag an output to an input  ·  "
                         "right-click a jack = unplug  ·  CV AMT knobs below")
        if k == "JACK":
            if x < 960:
                v = VOICES[int((x - 10) // colw)]
                o["bind"] = "jack:%s:%s" % (v, t)
            else:
                pid = {"CLK IN": "CLOCK:CLK IN", "RST IN": "CLOCK:RST IN", "RUN IN": "CLOCK:RUN IN",
                       "CLK OUT": "CLOCK:CLK OUT", "RST OUT": "CLOCK:RST OUT", "RUN OUT": "CLOCK:RUN OUT", "ACC OUT": "CLOCK:ACC OUT",
                       "LFO 1": "MOD:LFO 1", "LFO 2": "MOD:LFO 2", "LFO 3": "MOD:LFO 3", "LFO 4": "MOD:LFO 4", "RND": "MOD:RND",
                       "LD GATE": "MOD:LD GATE", "BS GATE": "MOD:BS GATE", "MIX L": "MIX:L", "MIX R": "MIX:R"}[t]
                o["bind"] = "jack:" + pid
        elif k in ("PATH",) and "Q" in o.get("text", ""):
            o["hide"] = True  # illustrative cables; the editor draws the engine's cables
        elif k == "CIRCLE" and o.get("r") in (8.5, 4.0) and y < 480:
            o["hide"] = True
        elif y > 486:
            vi = int(round((x - 110) / 67.5)) if k in ("KNOB",) else int(round((x + o.get("w", 0) / 2 - 110) / 67.5))
            vi = max(0, min(15, vi))
            v = VOICES[vi]
            if k == "KNOB" and abs(y - 528) < 1:
                o["bind"] = "p:%s:PAN" % v
            elif k == "KNOB":
                o["bind"] = "cvamt:%s" % v
            elif k == "LCD" and abs(y - 552) < 1:
                o["bind"] = "disp:%s:OUTPUT" % v
            elif k == "LCD":
                o["bind"] = "disp:%s:CHOKE" % v
    ops.append(dict(kind="RECT", tab="ROUTE", x=0, y=0, w=1, h=1, r=0, fill=0, stroke=0, sw=0, opacity=1.0, bind="cables", hide=False))


def bind_fx(ops):
    bind_chrome(ops)
    cw = 1180 / 16
    for o in ops:
        k, t, x, y = o["kind"], o.get("text", ""), o.get("x", 0), o.get("y", 0)
        if 96 < y < 396:
            v = VOICES[max(0, min(15, int((x + o.get("w", 0) / 2 - 10) // cw)))]
            if k == "KNOB":
                o["bind"] = "p:%s:%s" % (v, "PAN" if t == "PAN" else "SEND")
            elif k == "LCD":
                o["bind"] = "bus:%s" % v
            elif k == "RECT" and o["w"] == 22:
                o["bind"] = "fader:%s" % v
            elif k == "RECT" and o["w"] == 4 and o["x"] > x - 1 and o["fill"] == colour("#37c25a"):
                o["bind"] = "vmeter:%s" % v
            elif k == "KEY":
                o["bind"] = "p:%s:%s" % (v, "MUTE" if t == "M" else "SOLO")
        elif y >= 410:
            if x < 950:
                b = "ABCD"[int((x - 10) // 236)] if k != "BOX" else "ABCD"[int((x - 10) // 236)]
                B = "BUS " + b
                if k == "KNOB":
                    o["bind"] = "p:%s:%s" % (B, t)
                elif k == "TOGGLE":
                    o["bind"] = "p:%s:COMP" % B
                    o["on"] = False
                elif k == "KEY" and t.startswith("SC"):
                    o["bind"] = "disp:%s:SC" % B
                elif k == "RECT" and abs(o.get("y", 0) - 578) < 1 and o["w"] == 30:
                    o["bind"] = "gr:%s" % b
                elif k == "KEY" and t.startswith("2×"):
                    o["bind"] = "osbadge"
            else:
                if k == "KNOB":
                    o["bind"] = "p:MASTER:%s" % t
                elif k == "TOGGLE":
                    o["bind"] = "p:MASTER:CLIP"
                elif k == "LED":
                    o["bind"] = "clipled"
                elif k == "KEY" and t.startswith("DELAY"):
                    o["bind"] = "disp:FX:DELAY TIME"
                    o["text"] = "DELAY: 1/8D"
                    o["w"] = 208
                elif k == "KEY" and t.startswith("ROOM"):
                    o["hide"] = True  # the FX send is a delay only
                elif k == "TEXT" and t.startswith("FX SEND ="):
                    o["text"] = "FX SEND = one stereo delay, kept clean"


def bind_seq(ops):
    bind_chrome(ops)
    for o in ops:
        k, t, x, y = o["kind"], o.get("text", ""), o.get("x", 0), o.get("y", 0)
        if 130 <= y < 660 and x < 770:
            i = int((y - 130) // 32)
            if i > 15:
                continue
            v = VOICES[i]
            if k == "LCD" and abs(x - 118) < 1:
                o["bind"] = "len:%d" % i
            elif k == "LCD" and abs(x - 178) < 1:
                o["bind"] = "tscale:%d" % i
            elif k == "KNOB" and abs(x - 266) < 1:
                o["bind"] = "tswing:%d" % i
            elif k == "TEXT" and abs(x - 281) < 1:
                o["bind"] = "tswingtxt:%d" % i
            elif k == "KNOB" and abs(x - 346) < 1:
                o["bind"] = "tshift:%d" % i
            elif k == "TEXT" and abs(x - 361) < 1:
                o["bind"] = "tshifttxt:%d" % i
            elif k == "LCD" and abs(x - 408) < 1:
                o["bind"] = "disp:%s:CHOKE" % v
            elif k == "LCD" and abs(x - 488) < 1:
                o["bind"] = "midinote:%d" % i
            elif k == "LCD" and abs(x - 578) < 1:
                o["bind"] = "midich:%d" % i
            elif k == "KEY" and t == "LEARN":
                o["hide"] = True  # the note map is fixed (36–49 drums, ch 1 LEAD, ch 2 BASS): nothing to learn
            elif k == "TOGGLE" and i < 14:
                o["bind"] = "p:%s:TRIG MERGE" % v
                o["on"] = False
                o["x"] = 652
            elif k == "TOGGLE":
                o["hide"] = True  # LEAD/BASS: their GATE jacks always play (no TRIG MERGE parameter)
        elif x >= 780 and 96 <= y < 346:
            row = int((y - (120 if k == "LCD" else 134)) // 31)
            if row == 5:  # MIDI CLOCK OUT: the plugin sends no MIDI
                o["hide"] = True
            elif row == 6 and k in ("LCD", "TEXT"):
                o["y"] -= 31  # START ON moves up into the freed row
            if k == "LCD":
                o["bind"] = ["disp:CLOCK:SOURCE", "disp:CLOCK:CLK IN", "disp:CLOCK:CLK OUT", "disp:CLOCK:RUN OUT", "", "", ""][row]
        elif x >= 780 and y >= 356:
            # CC MAP: the engine's fixed CC table (kParams[].cc); learn is not supported, so no LEARN keys
            col = 0 if x < 990 else 1
            row = int((y - (376 if k in ("LCD", "KEY") else 390)) // 33 + 0.01)
            cc = [[("BD1 ATTACK", 2), ("BD1 DECAY", 64), ("BD1 PITCH", 65), ("BD1 TUNE", 3), ("BD1 NOISE", 4), ("BD1 FILTER", 5)],
                  [("BD1 DRIVE", 6), ("BD1 SOUND", 66), ("HAT TUNE", 73), ("TOM NOISE", 84), ("CB TUNE", 85), ("CB DECAY", 86)]]
            if k == "BOX":
                o["text"] = "CC MAP  (fixed)"
            elif k == "KEY":
                o["hide"] = True
            elif k in ("LCD", "TEXT") and 0 <= row < 8:
                if row >= 6:
                    o["hide"] = True
                elif k == "LCD":
                    o["text"] = str(cc[col][row][1])
                    o["x"] += 50
                else:
                    o["text"] = cc[col][row][0]
    for o in ops:  # the "OUT NOTE" header reads "TRIG MERGE" (the per-jack opt-in, §12.2)
        if o["kind"] == "TEXT" and o.get("text") == "OUT NOTE":
            o["text"] = "MERGE"
        if o["kind"] == "TEXT" and o.get("text") == "MERGE" and abs(o["y"] - 120) < 1:
            o["x"] = 630
        if o["kind"] == "TEXT" and o.get("text") == "LEARN" and abs(o["y"] - 120) < 1:
            o["hide"] = True
        if o["kind"] == "TEXT" and o.get("text") == "MUTE GRP" and abs(o["y"] - 120) < 1:
            o["text"] = "CHOKE"  # the column shows each voice's CHOKE group
    ops.append(dict(kind="TEXT", tab="SEQ/MIDI", x=792, y=320, text="MIDI IN: drums 36–49 (ch ≠ 1/2)  ·  ch 1 LEAD  ·  ch 2 BASS",
                    z=8, anchor=0, fill=colour(mm.DIM), weight=600, ls=0.4, bind="", hide=False))
    ops.append(dict(kind="TEXT", tab="SEQ/MIDI", x=792, y=600, text="CC 1 = MOD W source  ·  aftertouch = AT source",
                    z=8, anchor=0, fill=colour(mm.DIM), weight=600, ls=0.4, bind="", hide=False))
    ops.append(dict(kind="TEXT", tab="SEQ/MIDI", x=792, y=616, text="CC value 0–127 sets the parameter at once",
                    z=8, anchor=0, fill=colour(mm.DIM), weight=600, ls=0.4, bind="", hide=False))


def bind_global(ops):
    bind_chrome(ops)
    for o in ops:
        k, t, x, y = o["kind"], o.get("text", ""), o.get("x", 0), o.get("y", 0)
        if y < 90:
            continue
        if k == "KEY" and (t == "SAME" or (t == "4×" and y > 180)):  # the offline row first: its 4× is OFFLINE:1
            o["bind"] = "choice:GLOBAL:OFFLINE:%d" % (0 if t == "SAME" else 1)
            o["lit"] = 0
        elif k == "KEY" and t in ("1×", "2×", "4×"):
            o["bind"] = "choice:GLOBAL:OS:%d" % ["1×", "2×", "4×"].index(t)
            o["lit"] = 0
        elif k == "KNOB" and t in ("NOISE FLOOR", "TRANSPOSE"):
            o["hide"] = True  # the engine has no noise floor or transpose
        elif k == "LCD" and "st" in t and x > 990:
            o["hide"] = True  # the TRANSPOSE readout
        elif k == "KEY" and t.endswith("%") and t[:-1].isdigit():
            o["bind"] = "uiscale:%s" % t[:-1]
            o["lit"] = 0
        elif k == "TOGGLE" and 480 < y < 600:
            o["hide"] = True  # TOOLTIPS / KNOB DRAG / SHOW VALUES / REMEMBER SCALE: not settings; the facts are listed
        elif k == "TEXT" and 480 < y < 600 and x == 56:
            o["x"] = 24
            o["text"] = {497: "KNOBS: drag up / down  ·  shift = fine  ·  wheel = nudge",
                         527: "DOUBLE-CLICK a knob = its default (the noon kit value)",
                         557: "KEYS and LCDs: click = next  ·  shift-click = previous  ·  right-click = list",
                         587: "UI SCALE resizes this window; the corner drag works too"}[int(round(y))]
            o["fill"] = colour(mm.DIM)
        elif k == "KEY" and t == "SAVE AS DEFAULT":
            o["hide"] = True  # no user default store
        elif k == "LCD" and "smp" in t:
            o["bind"] = "latency"
        elif k == "LCD" and "host" in t:
            o["bind"] = "rate"
        elif k == "LED" and y == 258:
            o["bind"] = "act:%s" % VOICES[int(round((x - 226) / 10))]
            o["on"] = False
        elif k == "KNOB":
            o["bind"] = "p:" + {"DRIFT": "GLOBAL:DRIFT", "TOLERANCE": "GLOBAL:TOLERANCE", "A4 REF": "GLOBAL:A4"}[t]
            o["x"] += {"DRIFT": 76, "TOLERANCE": 76, "A4 REF": 121}[t]  # recentred without NOISE FLOOR / TRANSPOSE
        elif k == "LCD" and "Hz" in t:
            o["bind"] = "disp:GLOBAL:A4"
            o["x"] += 121
        elif k == "LCD" and t.startswith("SN"):
            o["bind"] = "serial"
        elif k == "KEY" and t in ("GATE", "GATE+ACC", "GATE+DYN", "FULL DYN"):
            o["bind"] = "choice:GLOBAL:TRIG DYN:%d" % ["GATE", "GATE+ACC", "GATE+DYN", "FULL DYN"].index(t)
            o["lit"] = 0
        elif k == "KEY" and t in ("LINEAR", "SOFT", "HARD", "FIXED"):
            o["bind"] = "choice:GLOBAL:VEL CURVE:%d" % ["LINEAR", "SOFT", "HARD", "FIXED"].index(t)
            o["lit"] = 0
        elif k == "KEY" and t == "IDEAL (0 %)":
            o["bind"] = "ideal"
        elif k == "KEY" and t == "RE-ROLL UNIT":
            o["bind"] = "reroll"
        elif k == "KEY" and t == "PANIC / ALL OFF":
            o["bind"] = "panic"
        elif k == "KEY" and t == "INIT PATCH":
            o["bind"] = "initpatch"
        elif k == "RECT" and x == 781:
            o["bind"] = "cpubar:%d" % int((y - 423) // 36)


TABS = [("MAIN", mm.main_page, bind_main), ("VOICE", mm.voice_page, bind_voice), ("GRID", mm.grid_page, bind_grid),
        ("MOD", mm.mod_page, bind_mod), ("ROUTE", mm.route_page, bind_route), ("FX/MIX", mm.fx_page, bind_fx),
        ("SEQ/MIDI", mm.seq_page, bind_seq), ("GLOBAL", mm.global_page, bind_global)]


def capture(tab, fn):
    rec = Rec(tab)
    orig = mm.Svg
    mm.Svg = lambda: rec
    try:
        random.seed(5)
        fn()
    finally:
        mm.Svg = orig
    return rec.ops


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


HELP_NOT = {"OVERSAMPLING (offline render)", "VOICES (14 drums)"}  # section labels, not help


def is_help(o):
    """Item K: paragraph help, hint lines, tab notes, the matrix footer and the text under graphs: unbound plain text
    with lower-case words (3 or more). Knob, jack, key and value labels never match (caps, short, or bound)."""
    t = o.get("text", "")
    return (o["kind"] == "TEXT" and not o.get("bind") and t not in HELP_NOT and re.search("[a-z]", t) is not None
            and len(t.split()) >= 3)


def fnum(v):
    if isinstance(v, bool):
        return "1" if v else "0"
    if v is None:
        return "0"
    if isinstance(v, float) and math.isnan(v):
        return "kNoRing"
    t = ("%.4f" % float(v)).rstrip("0")
    return (t + "0" if t.endswith(".") else t) + "f"


def main():
    out = []
    count = 0
    for ti, (tab, fn, binder) in enumerate(TABS):
        ops = capture(tab, fn)
        # header: the active tab key becomes dynamic; tab keys switch tabs
        binder(ops)
        for o in ops:
            if o["kind"] == "RECT" and abs(o.get("y", 0) - 8) < 0.1 and abs(o.get("h", 0) - 22) < 0.1 and 132 <= o["x"] < 640:
                o["bind"] = "tab:%d" % int(round((o["x"] - 132) / 61))
            if o["kind"] == "TEXT" and abs(o.get("y", 0) - 23) < 0.1 and o.get("text") in mm.TABS:
                o["bind"] = "tabtext:%d" % mm.TABS.index(o["text"])
        for o in ops:
            if o["hide"]:
                continue
            k = KIND.index(o["kind"])
            flags = (o.get("anchor", 0) & 3) | (4 if o.get("on") else 0) | (8 if o.get("out") else 0) | (16 if o.get("round") else 0)
            if o.get("weight", 700) >= 700:
                flags |= 32
            if is_help(o):
                flags |= 64
            out.append("  {%d, %d, %d, %s, %s, %s, %s, %s, %s, %s, 0x%08Xu, 0x%08Xu, %s, %s, %s, %s, %s, %d, %d},"
                       % (k, ti, flags, fnum(float(o.get("x", 0))), fnum(float(o.get("y", 0))), fnum(float(o.get("w", 0))),
                          fnum(float(o.get("h", 0))), fnum(float(o.get("r", 0))), fnum(float(o.get("z", 0))),
                          fnum(float(o.get("v", o.get("ls", 0)))), o.get("fill", 0) or o.get("lit", 0) if o["kind"] != "KEY" else o.get("lit", 0),
                          o.get("stroke", 0) if o["kind"] != "KEY" else o.get("fill", 0), fnum(float(o.get("sw", 0))),
                          fnum(float(o.get("opacity", 1.0))), cstr(o.get("text", "")), cstr(o.get("text2", "")), cstr(o.get("bind", "")),
                          int(o.get("steps", 0)), int(o.get("ticks", 0))))
            out[-1] = out[-1].replace("kNoRingf", "kNoRing")
            if not math.isnan(o.get("ring", float("nan"))) and o["kind"] == "KNOB":
                pass
            count += 1
    hdr = ("// GENERATED by plugin/layout/export_layout.py from the spec mockups (make_mockups.py). Do not edit.\n"
           "// Design units: 1200 x 672 panel; the editor scales them by k = 1136/1200 between the 30 px rack ears.\n"
           "// {kind, tab, flags, x, y, w, h, r, z, v, fill, stroke, sw, opacity, text, text2, bind, steps, ticks}\n")
    path = os.path.join(HERE, "..", "Source", "PanelLayout.inc")
    with open(path, "w") as f:
        f.write(hdr + "\n".join(out) + "\n")
    print("ops", count, "->", os.path.normpath(path))


if __name__ == "__main__":
    main()
