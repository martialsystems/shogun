#!/usr/bin/env python3
"""Generates the SHOGUN panel mockups (SVG in HTML) at the default editor size, 1200 x 672 logical px.
Render: google-chrome --headless --screenshot --window-size=1200,672 page.html"""
import math, os, random
W, H = 1200, 672
EAR = 30                         # rack ears with screws, both sides (same language as BUSHIDO/RONIN and the Jidai rack)
PANEL_K = (W - 2*(EAR + 2)) / W  # the panel is drawn in 1200-wide design units and scaled into the 1136 px between the ears
INK = "#f2f1ea"; DIM = "#9a9a90"; GRN = "#4aa862"; RED = "#e0402e"; AMB = "#f0b030"
OUT = os.path.dirname(os.path.abspath(__file__))
random.seed(5)

def esc(s): return s.replace("&", "&amp;").replace("<", "&lt;")
class Svg:
    def __init__(s): s.p = []
    def add(s, x): s.p.append(x)
    def text(s, x, y, t, z=9, a="middle", fill=INK, w=700, ls=0.4):
        s.add(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{z}" font-weight="{w}" letter-spacing="{ls}" text-anchor="{a}" fill="{fill}">{esc(t)}</text>')
    def rtext(s, x, y, t, z=8.5, pad=3):   # reverse lettering (outputs)
        w = len(t)*z*0.62 + 2*pad
        s.add(f'<rect x="{x-w/2:.1f}" y="{y-z+0.5:.1f}" width="{w:.1f}" height="{z+3:.1f}" rx="1.5" fill="{INK}"/>')
        s.text(x, y, t, z, fill="#0b0c0b")
    def rule(s, x1, y1, x2, y2, w=1.2, c=GRN): s.add(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{c}" stroke-width="{w}"/>')
    def box(s, x, y, w, h, title=None, tz=9.5):
        s.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="none" stroke="{GRN}" stroke-width="1.1" rx="2"/>')
        if title:
            s.add(f'<rect x="{x+6}" y="{y-6}" width="{len(title)*tz*0.66+10:.1f}" height="12" fill="#121412"/>')
            s.text(x+11, y+3.5, title, tz, "start", INK, 800, 1.2)
    def knob(s, cx, cy, r=14, label=None, v=None, lz=8.5, ticks=11, steps=None, ring=None):
        v = random.uniform(0.15, 0.85) if v is None else v
        n = steps or ticks
        for i in range(n):
            a = math.radians(-135 + 270*i/(n-1) - 90); l = 3.2 if i in (0, n-1, (n-1)//2) else 2
            s.add(f'<line x1="{cx+(r+2.5)*math.cos(a):.1f}" y1="{cy+(r+2.5)*math.sin(a):.1f}" x2="{cx+(r+2.5+l)*math.cos(a):.1f}" y2="{cy+(r+2.5+l)*math.sin(a):.1f}" stroke="#bdbcb2" stroke-width=".9"/>')
        if ring is not None:   # modulation ring, green arc
            a0 = -135 + 270*v; a1 = a0 + 270*ring
            def pt(a): return cx+(r+1.2)*math.cos(math.radians(a-90)), cy+(r+1.2)*math.sin(math.radians(a-90))
            x0, y0 = pt(a0); x1, y1 = pt(a1)
            s.add(f'<path d="M{x0:.1f},{y0:.1f} A{r+1.2},{r+1.2} 0 {1 if abs(a1-a0)>180 else 0} {1 if a1>a0 else 0} {x1:.1f},{y1:.1f}" stroke="{GRN}" stroke-width="2.4" fill="none"/>')
        s.add(f'<circle cx="{cx}" cy="{cy+1.5}" r="{r}" fill="#000" opacity=".55"/>')
        s.add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#kb)" stroke="#050505" stroke-width="1"/>')
        s.add(f'<circle cx="{cx}" cy="{cy}" r="{r*0.68:.1f}" fill="url(#kc)" stroke="#2b2b2b" stroke-width=".6"/>')
        a = math.radians(-135 + 270*v - 90)
        s.add(f'<line x1="{cx+r*0.15*math.cos(a):.1f}" y1="{cy+r*0.15*math.sin(a):.1f}" x2="{cx+r*0.92*math.cos(a):.1f}" y2="{cy+r*0.92*math.sin(a):.1f}" stroke="#f5f4ee" stroke-width="2" stroke-linecap="round"/>')
        if label: s.text(cx, cy+r+11, label, lz)
    def led(s, cx, cy, on=False, c=RED, r=3.2):
        s.add(f'<circle cx="{cx}" cy="{cy}" r="{r+1.6}" fill="#050505" stroke="#2a2c2a" stroke-width=".8"/>')
        s.add(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{c if on else "#3a1612" if c==RED else "#173a20" if c==GRN else "#3a2a10"}"/>')
        if on: s.add(f'<circle cx="{cx}" cy="{cy}" r="{r*2.6}" fill="{c}" opacity=".18"/>')
    def key(s, x, y, w, h, label=None, on=False, lit=None, z=8.5, fill="#0a0a0a"):
        s.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="{fill}" stroke="#2f322f" stroke-width="1"/>')
        s.add(f'<rect x="{x+1.5}" y="{y+1.5}" width="{w-3}" height="{h*0.42:.1f}" rx="2" fill="#ffffff" opacity=".05"/>')
        if lit: s.add(f'<rect x="{x+2}" y="{y+2}" width="{w-4}" height="{h-4}" rx="2" fill="{lit}" opacity=".85"/>')
        if label: s.text(x+w/2, y+h/2+z*0.36, label, z, fill="#0b0c0b" if lit else INK)
    def lcd(s, x, y, w, h, t, z=11, a="start", c="#7ee08c"):
        s.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="2" fill="#071008" stroke="#2c3a2c" stroke-width="1"/>')
        s.add(f'<text x="{x+6 if a=="start" else x+w/2:.1f}" y="{y+h/2+z*0.36:.1f}" font-family="DejaVu Sans Mono,monospace" font-size="{z}" text-anchor="{a}" fill="{c}">{esc(t)}</text>')
    def toggle(s, cx, cy, l, r, on_right=False, z=7.5):
        s.add(f'<rect x="{cx-9}" y="{cy-4.5}" width="18" height="9" rx="4.5" fill="#050505" stroke="#3a3d3a"/>')
        s.add(f'<circle cx="{cx+(5 if on_right else -5)}" cy="{cy}" r="3.6" fill="url(#kc)"/>')
        s.text(cx-12, cy+2.8, l, z, "end"); s.text(cx+12, cy+2.8, r, z, "start")
    def jack(s, cx, cy, label, out=False, normal=None, patched=False, z=8):
        s.add(f'<circle cx="{cx}" cy="{cy}" r="10.5" fill="#000" opacity=".5"/><circle cx="{cx}" cy="{cy}" r="9.5" fill="url(#js)" stroke="#2a2a2c" stroke-width=".9"/><circle cx="{cx}" cy="{cy}" r="6.4" fill="url(#jn)"/><circle cx="{cx}" cy="{cy}" r="3.8" fill="#030303"/>')
        if out: s.rtext(cx, cy+21, label, z)
        else: s.text(cx, cy+21, label, z)
        if normal: s.text(cx, cy-13, "▸"+normal, 6.5, fill=DIM, w=600)
    def svg(s):
        defs = f'''<defs>
<linearGradient id="pl" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#1b1d1b"/><stop offset="1" stop-color="#0f110f"/></linearGradient>
<radialGradient id="kb" cx=".4" cy=".3" r=".9"><stop offset="0" stop-color="#3a3a3a"/><stop offset=".6" stop-color="#151515"/><stop offset="1" stop-color="#080808"/></radialGradient>
<radialGradient id="kc" cx=".38" cy=".3" r=".9"><stop offset="0" stop-color="#d9d8d2"/><stop offset=".5" stop-color="#8d8c86"/><stop offset="1" stop-color="#3b3a37"/></radialGradient>
<radialGradient id="js" cx=".4" cy=".3" r=".9"><stop offset="0" stop-color="#e2e2dc"/><stop offset=".55" stop-color="#8f8f8a"/><stop offset="1" stop-color="#3a3a38"/></radialGradient>
<radialGradient id="jn" cx=".4" cy=".35" r=".9"><stop offset="0" stop-color="#5a5a58"/><stop offset="1" stop-color="#161616"/></radialGradient>
<linearGradient id="ear" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#2c2e2c"/><stop offset=".5" stop-color="#4a4d4a"/><stop offset="1" stop-color="#262826"/></linearGradient>
<pattern id="grain" width="4" height="4" patternUnits="userSpaceOnUse"><rect width="4" height="4" fill="#141614"/><circle cx="1" cy="1" r=".35" fill="#1c1f1c"/><circle cx="3" cy="2.6" r=".3" fill="#101210"/></pattern>
</defs>'''
        k = PANEL_K; ox = EAR + 2; oy = (H - H*k)/2
        if getattr(s, "bare", False):   # native panel only (no window ears): used by the Jidai rack, which supplies its own ears
            return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="Helvetica Neue,Arial,DejaVu Sans,sans-serif">{defs}'
                    + "".join(s.p) + "</svg>")
        frame = [f'<rect width="{W}" height="{H}" fill="#0a0b0a"/>']
        for side in (0, 1):
            x = 0 if side == 0 else W - EAR
            frame.append(f'<rect x="{x}" y="0" width="{EAR}" height="{H}" fill="url(#ear)" stroke="#000"/>')
            frame.append(f'<rect x="{x+EAR/2-1}" y="0" width="2" height="{H}" fill="#000" opacity=".18"/>')
            for yy in (24, H/2, H-24):
                frame.append(f'<circle cx="{x+EAR/2}" cy="{yy}" r="7" fill="url(#js)" stroke="#000"/>'
                             f'<line x1="{x+EAR/2-4.5}" y1="{yy-4.5}" x2="{x+EAR/2+4.5}" y2="{yy+4.5}" stroke="#2a2a28" stroke-width="1.6"/>'
                             f'<line x1="{x+EAR/2-4.5}" y1="{yy+4.5}" x2="{x+EAR/2+4.5}" y2="{yy-4.5}" stroke="#2a2a28" stroke-width="1.6"/>')
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="Helvetica Neue,Arial,DejaVu Sans,sans-serif">{defs}'
                + "".join(frame) + f'<g transform="translate({ox:.2f},{oy:.2f}) scale({k:.5f})">' + "".join(s.p) + "</g></svg>")

TABS = ["MAIN", "VOICE", "GRID", "MOD", "ROUTE", "FX/MIX", "SEQ/MIDI", "GLOBAL"]

def chrome(s, tab):
    """Header (y 0-36) and transport strip (y 36-88) are the same on every tab."""
    s.add(f'<rect width="{W}" height="{H}" fill="url(#grain)"/>')
    s.add(f'<rect x="4" y="4" width="{W-8}" height="{H-8}" rx="5" fill="url(#pl)" opacity=".92" stroke="#2b2e2b"/>')
    s.text(22, 25, "SHOGUN", 16, "start", "#fff", 800, 4.5)
    # 8 tabs x 132..620 (58 wide, 61 pitch)
    for i, t in enumerate(TABS):
        x = 132 + i*61; on = t == tab
        s.add(f'<rect x="{x}" y="8" width="58" height="22" rx="3" fill="{GRN if on else "#0b0c0b"}" stroke="{GRN if on else "#343834"}"/>')
        s.text(x+29, 23, t, 8.5, fill="#071008" if on else INK, w=800, ls=0.4)
    # kit + pattern browser (always visible), A/B, undo. Quality (2×) lives on GLOBAL.
    s.text(632, 23, "KIT", 8.5, "start", DIM)
    s.lcd(654, 8, 118, 22, "INIT", 10)
    s.key(774, 8, 18, 22, "◀", z=8); s.key(794, 8, 18, 22, "▶", z=8); s.key(814, 8, 22, 22, "⌕", z=10)
    s.text(846, 23, "PATTERN", 8.5, "start", DIM)
    s.lcd(892, 8, 118, 22, "001 USER  A", 10)
    s.key(1012, 8, 18, 22, "◀", z=8); s.key(1032, 8, 18, 22, "▶", z=8); s.key(1052, 8, 22, 22, "⌕", z=10)
    s.key(1082, 8, 20, 22, "A", lit=AMB, z=9); s.key(1104, 8, 20, 22, "B", z=9)
    s.key(1132, 8, 22, 22, "↶", z=10); s.key(1158, 8, 22, 22, "↷", z=10)
    s.rule(10, 36, W-10, 36)
    # transport strip y 36..88
    y0 = 36
    s.text(16, y0+12, "CLOCK", 8.5, "start", DIM, 800, 1.2)
    s.key(16, y0+18, 30, 26, "▶"); s.led(52, y0+22, True, GRN)
    s.key(60, y0+18, 30, 26, "RST", z=7.5)
    s.toggle(124, y0+31, "INT", "EXT")
    s.text(124, y0+48, "SWITCH", 6.5, fill=DIM)
    s.knob(172, y0+30, 13, None, 0.5); s.text(172, y0+50, "TEMPO", 7.5)
    s.lcd(192, y0+19, 50, 22, "120.0", 11, "middle")
    s.knob(266, y0+30, 13, None, 0.3); s.text(266, y0+50, "SWING", 7.5)
    s.knob(310, y0+30, 13, None, 0.66, steps=4); s.text(310, y0+50, "SCALE", 7.5)
    s.lcd(330, y0+19, 34, 22, "1/16", 9.5, "middle")
    s.knob(388, y0+30, 13, None, 0.47); s.text(388, y0+50, "BAR", 7.5)
    s.lcd(408, y0+19, 34, 22, "16", 11, "middle")
    s.lcd(452, y0+19, 70, 22, "05:16 ▮", 10.5, "middle")
    s.text(487, y0+50, "STEP / BAR", 6.5, fill=DIM)
    s.rule(534, y0+6, 534, y0+48, 1)
    s.text(546, y0+12, "PERFORM", 8.5, "start", DIM, 800, 1.2)
    for i, (lb, c) in enumerate([("FILL", None), ("ROLL", None), ("MUTE GRP", None), ("SCENE A", AMB)]):
        s.key(546+i*62, y0+18, 56, 24, lb, lit=c, z=7.5)
    s.rule(800, y0+6, 800, y0+48, 1)
    s.text(812, y0+12, "MASTER", 8.5, "start", DIM, 800, 1.2)
    s.knob(838, y0+30, 13, None, 0.55); s.text(838, y0+50, "ACCENT", 7.5)
    s.knob(884, y0+30, 13, None, 0.2); s.text(884, y0+50, "DRIVE", 7.5)
    s.knob(930, y0+30, 13, None, 0.62); s.text(930, y0+50, "VOLUME", 7.5)
    # stereo meter
    for k, yy in enumerate([y0+20, y0+31]):
        s.add(f'<rect x="958" y="{yy}" width="170" height="7" rx="1" fill="#050505" stroke="#262826"/>')
        for i in range(34):
            c = "#37c25a" if i < 24 else AMB if i < 30 else RED
            if i < (23 if k == 0 else 21): s.add(f'<rect x="{959.5+i*5}" y="{yy+1.2}" width="3.6" height="4.6" fill="{c}"/>')
    s.text(958, y0+50, "-48", 6.5, "start", DIM); s.text(1043, y0+50, "-12", 6.5, fill=DIM); s.text(1128, y0+50, "0 dBFS", 6.5, "end", DIM)
    s.led(1140, y0+26, False); s.text(1150, y0+29, "CLIP", 7.5, "start")
    s.led(1140, y0+40, True, GRN, 2.6); s.text(1150, y0+43, "CPU 7%", 7.5, "start")
    s.rule(10, 88, W-10, 88)

# ---------------------------------------------------------------- MAIN
VOICE_SECTIONS = [
    ("BD 1", [["TUNE", "PITCH", "DECAY", "ATTACK", "SOUND#16"], ["NOISE", "FILTER", "DRIVE", "WAVE", "LEVEL"]]),
    ("BD 2", [["TUNE", "DECAY", "TONE", "WAVE", "LEVEL"]]),
    ("SNARE · RIM", [["TUNE", "DETUNE", "PITCH", "T.DECAY", "TONE"], ["SNAPPY", "SN.DEC", "LEVEL", "|RIM TUNE", "RIM LVL"]]),
    ("CLAP · CLAVES", [["ATTACK", "SOUND#16", "COUNT#8", "FILTER", "DECAY"], ["LEVEL", None, "|CL TUNE", "CL DECAY", "CL LEVEL"]]),
    ("CB · MA", [["CB TUNE", "CB DECAY", "CB LEVEL", "|MA DECAY", "MA LEVEL"]]),
    ("HATS · CYMBAL", [["HAT TUNE", "CH DECAY", "OH DECAY", "CH LEVEL", "OH LEVEL"], ["CY TUNE", "CY TONE", "CY DECAY", "CY LEVEL", None]]),
    ("TOMS · CONGAS", [["TUNE", "DECAY", "WAVE", "LEVEL", "~LTC"], ["TUNE", "DECAY", "WAVE", "LEVEL", "NOISE"], ["TUNE", "DECAY", "WAVE", "LEVEL", "~HTC"]]),
    ("LEAD", [["CUTOFF", "RESO", "ENV", "DECAY", "LEVEL"], ["@SAW/SQR", "@OCT", "TUNE", "GLIDE", "ACCENT"]]),
    ("BASS", [["CUTOFF", "RESO", "ENV", "DECAY", "LEVEL"], ["@SAW/SQR", "@OCT", "TUNE", "GLIDE", "ACCENT"]]),
]
VY0, VY1 = 94, 372          # voice block
RINGS = {("BD 1", 0, "DECAY"): 0.18, ("HATS · CYMBAL", 0, "CH DECAY"): -0.16, ("SNARE · RIM", 0, "TUNE"): 0.10,
         ("BASS", 0, "CUTOFF"): 0.30, ("CLAP · CLAVES", 0, "FILTER"): -0.2, ("TOMS · CONGAS", 1, "WAVE"): 0.25}
def voice_block(s, VY0, VY1):
    ncols = sum(len(c) for _, c in VOICE_SECTIONS); gaps = len(VOICE_SECTIONS) - 1
    gap = 6; pitch = (W - 20 - gaps*gap)/ncols
    x = 10; rowp = (VY1 - VY0 - 20)/5
    geo = []
    for si, (name, cols) in enumerate(VOICE_SECTIONS):
        w = pitch*len(cols)
        s.box(x, VY0+6, w, VY1-VY0-6)
        # name key = trigger pad + track select
        s.key(x+4, VY0+1, min(w-8, len(name)*7+20), 14, name, lit="#2c6b3c" if name == "SNARE · RIM" else None, z=8)
        s.led(x+w-9, VY0+8, name in ("BD 1",), RED, 2.5)
        if name.startswith("TOMS"):
            for c in range(3):
                s.text(x+pitch*c+pitch/2, VY0+30, ["LTC", "MTC", "HTC"][c], 7, fill=DIM)
        for ci, col in enumerate(cols):
            cx = x + pitch*ci + pitch/2
            for ri, lab in enumerate(col):
                cy = VY0 + 30 + rowp*ri + rowp/2 - 6
                if lab is None: continue
                if lab.startswith("|"):
                    s.rule(cx-pitch/2+4, cy-rowp/2+3, cx+pitch/2-4, cy-rowp/2+3, .7, "#2f5c3a"); lab = lab[1:]
                if lab.startswith("~"):
                    s.toggle(cx, cy-6, "TOM", "CGA", z=6.5); s.toggle(cx, cy+12, "", "NZ", z=6.5); continue
                if lab.startswith("@"):
                    l = lab[1:]
                    if l == "OCT": s.knob(cx, cy, 11, "OCT −1 0 +1", 0.5, lz=7, steps=3)
                    else: s.toggle(cx, cy, "SAW", "SQR", z=6.5)
                    continue
                if "#" in lab:
                    l, n = lab.split("#"); s.knob(cx-9, cy, 12, None, 0.3, steps=int(n))
                    s.lcd(cx+6, cy-7, 20, 14, "07" if n == "16" else "4", 8.5, "middle"); s.text(cx, cy+23, l, 7.5)
                    continue
                if name.startswith("TOMS") and ci == 1 and lab == "NOISE":
                    s.knob(cx, cy, 13, "TOM NZ", lz=7.5); continue
                s.knob(cx, cy, 14 if "LEVEL" not in lab and "LVL" not in lab else 13, lab, lz=7.5,
                       ring=RINGS.get((name, ci, lab)))
        x += w + gap

def voice_page():
    s = Svg(); chrome(s, "VOICE")
    voice_block(s, 100, 500)
    # selected voice: general block (left) and the triple wave shaper (right), mirrored widths
    s.box(10, 512, 380, 148, "SELECTED VOICE · BD 1")
    for i, (lb, v) in enumerate([("VEL → LEVEL", .7), ("VEL → DECAY", .3), ("ACC AMT", .6), ("PAN", .5), ("CHOKE", .0), ("CV AMT", .5)]):
        s.knob(70 + (i % 3)*125, 556 + (i//3)*56, 13, lb, v, lz=7.5)
    s.box(400, 512, 790, 148, "WAVE · TRIPLE WAVE SHAPER · stages in series, each adds a fold · front WAVE = macro")
    # left column: source shaping and routing
    s.knob(452, 556, 13, "SHAPE", 0.35, lz=7.5, ring=0.25)
    s.text(452, 590, "SIN · TRI · SAW", 6.5, fill=DIM, w=600)
    s.toggle(452, 616, "POST", "PRE-VCA", z=6.5)
    s.text(452, 640, "pre-VCA: ENV drives the folds", 6.2, fill=DIM, w=600)
    # three stage columns
    for k in range(3):
        x0 = 510 + k*190
        s.rule(x0, 530, x0, 652, .7, "#2f5c3a")
        s.text(x0+95, 532, f"STAGE {k+1}", 8, w=800)
        s.knob(x0+52, 562, 12, f"WAVE {k+1}", [0.62, 0.5, 0.42][k], lz=7, ring=[None, 0.15, None][k])
        s.knob(x0+138, 562, 12, f"SYM {k+1}", [0.5, 0.5, 0.75][k], lz=7)
        s.knob(x0+52, 618, 12, "VC → AMT", [0.6, 0.5, 0.5][k], lz=7)
        s.knob(x0+138, 618, 12, "VC → SYM", [0.5, 0.65, 0.5][k], lz=7)
    s.rule(1080, 530, 1080, 652, .7, "#2f5c3a")
    # right column: VC source and level
    s.text(1135, 544, "VC SOURCE", 7, w=800)
    s.lcd(1095, 552, 80, 16, "BODY", 8.5, "middle")
    s.text(1135, 582, "audio rate, unsmoothed", 6.2, fill=DIM, w=600)
    s.toggle(1135, 606, "", "LEVEL COMP", z=6.5)
    s.text(1135, 640, "WAVE 0 = true bypass", 6.2, fill=DIM, w=600)
    return s

def grid_page():
    s = Svg(); chrome(s, "GRID")
    grid_block(s, 100)
    return s

def grid_block(s, SY0):
    SY = SY0; rows = ["BD1", "BD2", "SD", "RS", "CP", "CL", "MA", "CB", "CH", "OH", "CY", "LTC", "MTC", "HTC", "LEAD", "BASS"]
    rh = 26.5; gx0 = 150; cw = (W - 12 - gx0)/32
    s.box(10, SY, W-20, rh*16+20)
    s.text(18, SY+11, "TRACK", 7, "start", DIM); s.text(78, SY+11, "M  S  LEN", 7, "start", DIM)
    for st in range(32):
        cx = gx0 + cw*st + cw/2
        s.text(cx, SY+11, str(st+1), 6.8, fill=INK if st % 4 == 0 else DIM)
        if st % 4 == 0 and st: s.rule(gx0+cw*st, SY+3, gx0+cw*st, SY+rh*16+18, .7, "#2f5c3a")
    s.rule(gx0+cw*16, SY+2, gx0+cw*16, SY+rh*16+19, 1.6)
    pat = {
        "BD1": [0, 4, 8, 12, 16, 20, 24, 28], "BD2": [], "SD": [4, 12, 20, 28], "RS": [7, 23, 30], "CP": [4, 12, 20, 28],
        "CL": [3, 11, 19], "MA": [], "CB": [], "CH": [0, 1, 3, 4, 5, 7, 8, 9, 11, 12, 13, 15, 16, 17, 19, 20, 21, 23, 24, 25, 27, 28, 29, 31],
        "OH": [2, 6, 10, 14, 18, 22, 26, 30], "CY": [0], "LTC": [], "MTC": [], "HTC": [], "LEAD": [], "BASS": [0, 3, 6, 8, 10, 14, 16, 19, 22, 24, 26, 30]}
    for k in list(pat):
        if k not in ("BASS",): pat[k] = sorted(set(pat[k] + [x+16 for x in pat[k] if x < 16]))
    lens = {r: 32 for r in rows}; lens.update({"CL": 12, "RS": 28, "MA": 16, "CB": 16})
    for ri, r in enumerate(rows):
        y = SY + 16 + rh*ri
        sel = r == "SD"
        if sel: s.add(f'<rect x="12" y="{y-0.5}" width="{W-24}" height="{rh}" fill="#1f3a26" opacity=".55"/>')
        s.text(18, y+10, r, 8, "start", INK if r not in ("MA", "CB") else DIM)
        s.key(80, y+1, 14, rh-2.5, "M" if False else "", lit=RED if r == "BD2" else None); s.text(87, y+10, "M", 6.5, fill="#0b0c0b" if r == "BD2" else DIM)
        s.key(97, y+1, 14, rh-2.5, "", lit=AMB if False else None); s.text(104, y+10, "S", 6.5, fill=DIM)
        L = lens.get(r, 16); s.lcd(115, y+1, 30, rh-2.5, f"{L:>2}", 7.5, "middle")
        for st in range(32):
            x0 = gx0 + cw*st + 1.2; on = st in pat[r]
            past = st >= L
            acc = (st % 8 == 0)
            fill = "#d84a34" if (on and acc) else "#e9e4cf" if on else "#0b0c0b"
            if on and r in ("CH",) and st % 2: fill = "#7e7a6c"   # soft accent
            op = 0.35 if past else 1
            s.add(f'<rect x="{x0:.1f}" y="{y+1}" width="{cw-2.4:.1f}" height="{rh-2.6}" rx="1.6" fill="{fill}" stroke="#2c2f2c" stroke-width=".7" opacity="{op}"/>')
            if on and r == "SD" and st == 12: s.add(f'<circle cx="{x0+cw-6:.1f}" cy="{y+4}" r="1.8" fill="{GRN}"/>')   # p-lock dot
            if on and r == "CH" and st in (15, 31): s.text(x0+(cw-2.4)/2, y+rh-4.2, "×3", 6.5, fill="#0b0c0b")  # ratchet
            if on and r == "SD" and st == 28: s.text(x0+(cw-2.4)/2, y+rh-4.2, "fl", 6.5, fill="#0b0c0b")
        if r == "BASS":
            for st in pat[r]:
                pass
    # playhead
    ph = gx0 + cw*4
    s.add(f'<rect x="{ph:.1f}" y="{SY+14}" width="{cw:.1f}" height="{rh*16+3}" fill="#ffffff" opacity=".08"/>')
    # ---- step strip y 632..664
    TY = SY + rh*16 + 25
    s.box(10, TY, W-20, 664-TY)
    s.lcd(26, TY+20, 120, 24, "SD · STEP 13", 10)
    s.lcd(W-146, TY+20, 120, 24, "LOCKS 2 · TIE off", 10)
    items = [("ACCENT", 3, .5), ("FLAM", 17, 0), ("RATCHET", 5, 0), ("PROB", 0, 1), ("MICRO", 0, .5), ("BEND", 0, .5), ("NOTE", 37, .6)]
    for i, (lb, n, v) in enumerate(items):
        s.knob(W/2 + (i-3)*100, TY+30, 13, lb, v, steps=n if n else None, ticks=9, lz=7.5)
    for i, lb in enumerate(["TIE", "LOCKS ✕", "COPY", "PASTE", "CLEAR", "SHIFT ◀", "SHIFT ▶", "RANDOM", "LOCK RND"]):
        s.key(W/2 - 4.5*112 + 6 + i*112, TY+72, 100, 26, lb, z=8)
    s.text(W-14, TY-3, "hold a step + turn any knob = parameter lock  ·  right-click = menu", 6.8, "end", DIM, 600)

MAIN_STRIPS = [(v, ["TUNE", "DECAY", "LEVEL"]) for v in ["BD1", "BD2", "SD", "RS", "CP", "CL", "MA", "CB", "CH", "OH", "CY", "LTC", "MTC", "HTC"]] + \
              [("LEAD", ["CUTOFF", "RESO", "LEVEL"]), ("BASS", ["CUTOFF", "RESO", "LEVEL"])]
def main_page():
    """MAIN keeps breathing room and mirror symmetry: 3 knobs per voice on one grid, one step row, one lock strip.
    Everything else moved: full voice params -> VOICE, 16 x 32 grid -> GRID, routing -> ROUTE, mix/FX -> FX/MIX."""
    s = Svg(); chrome(s, "MAIN")
    X0, X1, Y0, Y1 = 10, W-10, 102, 338
    pitch = (X1 - X0)/16
    s.box(X0, Y0, X1-X0, Y1-Y0, "VOICES")
    s.rule(X0 + pitch*14, Y0+10, X0 + pitch*14, Y1-8, 1.4)
    for i, (v, ks) in enumerate(MAIN_STRIPS):
        cx = X0 + pitch*i + pitch/2
        if i and i != 14: s.rule(X0+pitch*i, Y0+16, X0+pitch*i, Y1-12, .5, "#24452d")
        s.key(cx-26, Y0+14, 52, 18, v, lit="#2c6b3c" if v == "SD" else None, z=8.5)
        for k, lb in enumerate(ks):
            s.knob(cx, Y0+70+k*66, 15, lb, lz=7.5)
    SY0, SY1 = 352, 522
    s.box(X0, SY0, X1-X0, SY1-SY0, "STEPS · SD")
    s.lcd(26, SY0+28, 120, 24, "SD · 32 STEPS", 10)
    for i, lb in enumerate(["1–16", "17–32"]):
        s.key(26 + i*64, SY0+60, 56, 22, lb, lit=AMB if i == 0 else None, z=8)
    s.lcd(W-146, SY0+28, 120, 24, "LEN 32 · 1/16", 10)
    for i, lb in enumerate(["◀ TRK", "TRK ▶"]):
        s.key(W-146 + i*64, SY0+60, 56, 22, lb, z=8)
    kw, kg = 40, 10
    total = 16*kw + 15*kg + 3*10
    kx0 = (W - total)/2
    pat = [0, 4, 8, 12]; acc = [0, 8]
    for st in range(16):
        x = kx0 + st*(kw+kg) + (st//4)*10
        s.key(x, SY0+28, kw, 52, None, lit="#d84a34" if st in acc else "#e9e4cf" if st in pat else None)
        s.led(x+kw/2, SY0+96, st == 4, RED, 3)
        s.text(x+kw/2, SY0+118, str(st+1), 8.5, fill=INK if st % 4 == 0 else DIM)
        if st == 12: s.add(f'<circle cx="{x+kw-7}" cy="{SY0+35}" r="2.4" fill="{GRN}"/>')
    s.text(W/2, SY0+150, "click = step on/off  ·  shift-click = accent  ·  hold + turn a knob = lock  ·  full 16-track grid on GRID", 7.5, fill=DIM, w=600)
    LY0, LY1 = 536, 660
    s.box(X0, LY0, X1-X0, LY1-LY0, "STEP 13 · LOCKS")
    items = [("ACCENT", 3, .5), ("FLAM", 17, 0), ("RATCHET", 5, 0), ("PROB", 0, 1), ("MICRO", 0, .5), ("BEND", 0, .5), ("NOTE", 37, .6)]
    for i, (lb, n, v) in enumerate(items):
        s.knob(W/2 + (i-3)*112, LY0+56, 16, lb, v, steps=n if n else None, ticks=9, lz=7.5)
    for i, lb in enumerate(["COPY", "PASTE"]):
        s.key(26, LY0+26 + i*42, 130, 32, lb, z=8.5)
    for i, lb in enumerate(["CLEAR", "RANDOM"]):
        s.key(W-156, LY0+26 + i*42, 130, 32, lb, z=8.5)
    return s

# ---------------------------------------------------------------- ROUTE
VOICES = ["BD1", "BD2", "SD", "RS", "CP", "CL", "MA", "CB", "CH", "OH", "CY", "LTC", "MTC", "HTC"]
def route_page():
    s = Svg(); chrome(s, "ROUTE")
    s.box(10, 96, 948, 380, "PATCH BAY · ins plain, outs reverse · ▸ normalled · CV ±5 V adds to knob · NOTE 0 V = C3 · FOLD VC audio rate · colour = role")
    colw = 948/16.0
    rows_in = [("TRIG", "SEQ"), ("VEL", "ACC"), ("PITCH", None), ("DECAY", None), ("TONE", None)]
    rows_out = [("OUT", None), ("RET", "OUT"), ("ENV", None)]
    patched = {("BD1", "PITCH"), ("SD", "TONE"), ("CH", "DECAY"), ("BASS", "OUT"), ("LTC", "TRIG")}
    cols = VOICES + ["LEAD", "BASS"]
    for i, v in enumerate(cols):
        cx = 10 + colw*i + colw/2
        s.text(cx, 116, v, 9, w=800)
        if i: s.rule(10+colw*i, 104, 10+colw*i, 470, .6, "#2f5c3a")
        ins = rows_in if v not in ("LEAD", "BASS") else [("GATE", "SEQ"), ("VEL", "ACC"), ("NOTE", "SEQ"), ("V/OCT", None), ("CUTOFF", None)]
        if v in ("BD1", "BD2"): ins = ins + [("WAVE", None), ("FOLD VC", None)]
        elif v in ("LTC", "MTC", "HTC"): ins = ins + [(None, None), ("FOLD VC", None)]
        for ri, (lb, nm) in enumerate(ins):
            if lb: s.jack(cx, 136 + ri*31, lb, False, nm)
        outs = rows_out if v not in ("LEAD", "BASS") else [("OUT", None), ("RET", "OUT"), ("NOTE OUT", None)]
        for ri, (lb, nm) in enumerate(outs):
            y = 368 + ri*40
            if lb == "RET": s.jack(cx, y, lb, False, nm)
            else: s.jack(cx, y, lb, True)
    s.rule(14, 350, 954, 350, .8)
    pass
    # clock + mod sources column
    s.box(966, 96, 224, 380, "CLOCK · MOD · MIX")
    cj = [("CLK IN", 0), ("RST IN", 0), ("RUN IN", 0), ("FILL IN", 0), ("CLK OUT", 1), ("RST OUT", 1), ("RUN OUT", 1), ("ACC OUT", 1)]
    for i, (lb, o) in enumerate(cj):
        s.jack(990 + (i % 4)*52, 132 + (i//4)*46, lb, bool(o))
    mj = [("LFO 1", 1), ("LFO 2", 1), ("LFO 3", 1), ("LFO 4", 1), ("RND", 1), ("LANE A", 1), ("LD GATE", 1), ("BS GATE", 1), ("MIX L", 1), ("MIX R", 1)]
    for i, (lb, o) in enumerate(mj):
        s.jack(990 + (i % 4)*52, 246 + (i//4)*46, lb, bool(o))
    s.text(1078, 410, "LFO/RND/LANE jacks are extras:", 7, fill=DIM, w=600)
    s.text(1078, 422, "they route internally on the MOD tab", 7, fill=DIM, w=600)
    s.text(1078, 434, "with no cable. Jacks copy them out", 7, fill=DIM, w=600)
    s.text(1078, 446, "for other Jidai devices (0–5 V / ±5 V).", 7, fill=DIM, w=600)
    # cables drawn (Jidai rope look)
    def cable(x1, y1, x2, y2, c):
        mx = (x1+x2)/2; my = max(y1, y2) + 40
        s.add(f'<path d="M{x1},{y1} Q{mx},{my} {x2},{y2}" stroke="#000" stroke-width="7" fill="none" opacity=".4"/>')
        s.add(f'<path d="M{x1},{y1} Q{mx},{my} {x2},{y2}" stroke="{c}" stroke-width="5" fill="none" stroke-linecap="round"/>')
        for (x, y) in [(x1, y1), (x2, y2)]:
            s.add(f'<circle cx="{x}" cy="{y}" r="8.5" fill="{c}" stroke="#000" stroke-opacity=".5"/><circle cx="{x}" cy="{y}" r="4" fill="#111"/>')
    # colours follow the source jack's role (JCS R14 role table)
    cable(990, 246, 10+colw*0+colw/2, 198, "#f7e77d")              # LFO 1 (CV) -> BD1 PITCH
    cable(1094, 292, 10+colw*11+colw/2, 136, "#2ec554")            # LD GATE (GATE/CLK) -> LTC TRIG
    cable(10+colw*14+colw/2, 448, 10+colw*15+colw/2, 229, "#6590f3")
    cable(10+colw*2+colw/2, 368, 10+colw*0+colw/2, 322, "#e53b2f")   # SD OUT (AUDIO) -> BD1 FOLD VC (audio-rate VC)  # LEAD NOTE OUT (V/OCT) -> BASS V/OCT
    # bottom: per-voice routing table
    s.box(10, 486, 1180, 174, "PER-VOICE ROUTING")
    s.text(18, 510, "", 7)
    labels = ["PAN", "OUTPUT", "CHOKE", "CV AMT"]
    for ri, lb in enumerate(labels):
        s.text(20, 530 + ri*36, lb, 8, "start", INK, 800)
    for i, v in enumerate(cols):
        cx = 110 + i*67.5
        s.text(cx, 506, v, 8, w=800)
        s.knob(cx, 528, 11, None, [0.5, 0.5, 0.5, 0.62, 0.5, 0.25, 0.7, 0.75, 0.38, 0.38, 0.62, 0.15, 0.5, 0.85, 0.5, 0.5][i], ticks=9)
        s.lcd(cx-24, 552, 48, 16, ["MAIN", "MAIN", "3/4", "3/4", "5/6", "MAIN", "MAIN", "MAIN", "7/8", "7/8", "7/8", "9/10", "9/10", "9/10", "MAIN", "11/12"][i], 8, "middle")
        s.lcd(cx-12, 586, 24, 16, {"CH": "1", "OH": "1", "LTC": "–", "BASS": "–"}.get(v, "–"), 8, "middle")
        s.knob(cx, 632, 11, None, 1.0 if v != "SD" else 0.75, ticks=9)
    s.text(1186, 652, "right-click any jack: unplug · stack · set CV AMT", 6.8, "end", DIM, 600)
    return s

# ---------------------------------------------------------------- FX/MIX
def fx_page():
    s = Svg(); chrome(s, "FX/MIX")
    cols = VOICES + ["LEAD", "BASS"]
    s.box(10, 96, 1180, 300, "CHANNELS")
    cw = 1180/16
    for i, v in enumerate(cols):
        x = 10 + cw*i; cx = x + cw/2
        if i: s.rule(x, 104, x, 392, .6, "#2f5c3a")
        s.text(cx, 116, v, 9, w=800)
        s.knob(cx, 140, 11, "PAN", lz=7)
        s.lcd(cx-15, 168, 30, 15, "ABCD"[[0, 0, 1, 1, 1, 3, 3, 3, 2, 2, 2, 3, 3, 3, 0, 0][i]], 9, "middle"); s.text(cx, 194, "BUS", 7)
        s.knob(cx, 218, 11, "SEND FX", lz=6.8)
        # fader
        s.add(f'<rect x="{cx-2}" y="250" width="4" height="110" rx="2" fill="#050505" stroke="#303430"/>')
        fy = 250 + 110*(1 - random.uniform(0.55, 0.85))
        s.add(f'<rect x="{cx-11}" y="{fy-5:.1f}" width="22" height="10" rx="2" fill="url(#kc)" stroke="#111"/>')
        # meter
        lv = random.uniform(0.2, 0.8)
        s.add(f'<rect x="{cx+14}" y="250" width="4" height="110" fill="#050505"/><rect x="{cx+14}" y="{250+110*(1-lv):.1f}" width="4" height="{110*lv:.1f}" fill="#37c25a"/>')
        s.key(cx-21, 368, 20, 16, "M", z=7); s.key(cx+1, 368, 20, 16, "S", z=7)
    names = [("BUS A · KICKS", "A"), ("BUS B · SNARE/CLAP", "B"), ("BUS C · METAL", "C"), ("BUS D · PERC/SYNTH", "D")]
    for i, (n, _) in enumerate(names):
        x = 10 + i*236; s.box(x, 410, 228, 250, n)
        for j, lb in enumerate(["DRIVE", "TONE", "LEVEL"]):
            s.knob(x+40+j*74, 452, 15, lb)
        s.text(x+12, 500, "COMPRESSOR", 8, "start", DIM, 800, 1)
        for j, lb in enumerate(["THRESH", "RATIO", "ATTACK", "RELEASE"]):
            s.knob(x+32+j*55, 534, 12, lb, lz=7)
        s.knob(x+32, 594, 12, "MAKEUP", lz=7); s.knob(x+87, 594, 12, "MIX", lz=7)
        s.text(x+130, 584, "GR", 7, "start", DIM)
        s.add(f'<rect x="{x+146}" y="578" width="70" height="6" fill="#050505"/><rect x="{x+186}" y="578" width="30" height="6" fill="{AMB}"/>')
        s.toggle(x+170, 604, "OFF", "ON", True, 7)
        s.key(x+12, 632, 60, 18, "2× OS ✓", z=7); s.key(x+78, 632, 60, 18, "SC: BD1", z=7)
    x = 954; s.box(x, 410, 236, 250, "MASTER")
    for j, lb in enumerate(["DRIVE", "GLUE", "WIDTH"]):
        s.knob(x+40+j*76, 452, 15, lb)
    for j, lb in enumerate(["FX SEND", "CEILING", "VOLUME"]):
        s.knob(x+40+j*76, 526, 15, lb)
    s.toggle(x+60, 584, "CLIP OFF", "ON", False, 7); s.led(x+150, 584, False); s.text(x+160, 587, "OVER", 7, "start")
    s.text(x+12, 612, "FX SEND = one stereo delay/room, kept clean", 6.8, "start", DIM, 600)
    s.key(x+12, 626, 100, 22, "DELAY  1/8D", z=7.5); s.key(x+120, 626, 100, 22, "ROOM  1.2 s", z=7.5)
    return s

# ---------------------------------------------------------------- SEQ/MIDI
def seq_page():
    s = Svg(); chrome(s, "SEQ/MIDI")
    s.box(10, 96, 760, 564, "TRACKS")
    hdr = ["TRACK", "LEN", "SCALE", "SWING", "SHIFT", "MUTE GRP", "MIDI NOTE", "CH", "LEARN", "OUT NOTE"]
    xs = [20, 120, 180, 250, 330, 410, 490, 580, 630, 700]
    for x, h in zip(xs, hdr): s.text(x, 120, h, 7.5, "start", DIM, 800)
    cols = VOICES + ["LEAD", "BASS"]
    for i, v in enumerate(cols):
        y = 130 + i*32
        if i % 2 == 0: s.add(f'<rect x="12" y="{y}" width="756" height="32" fill="#ffffff" opacity=".025"/>')
        s.text(20, y+20, v, 9, "start", INK, 800)
        s.lcd(118, y+7, 34, 18, ["32", "32", "32", "28", "32", "12", "16", "16", "32", "32", "32", "32", "32", "32", "32", "32"][i], 9, "middle")
        s.lcd(178, y+7, 48, 18, "1/16", 9, "middle")
        s.knob(266, y+16, 11, None, ticks=9); s.text(281, y+20, f"{random.choice([50, 54, 58, 62])}%", 7.5, "start")
        s.knob(346, y+16, 11, None, 0.1, ticks=9); s.text(361, y+20, "0 ms", 7.5, "start")
        s.lcd(408, y+7, 40, 18, random.choice(["–", "1", "2"]), 9, "middle")
        s.lcd(488, y+7, 60, 18, f"{36+i} {['C','C#','D','D#','E','F','F#','G','G#','A','A#','B'][(36+i)%12]}{(36+i)//12-1}", 9, "middle")
        s.lcd(578, y+7, 30, 18, "10" if i < 14 else "1" if i == 14 else "2", 9, "middle")
        s.key(628, y+7, 46, 18, "LEARN", z=7)
        s.toggle(722, y+16, "", "ON", i < 14, 7)
    s.box(780, 96, 410, 250, "CLOCK & SYNC")
    rows = [("SOURCE", "HOST  (locks to bar)"), ("CLK IN", "STEP  (1 pulse = 1 step)"), ("CLK OUT", "1 pulse / step, 50 % duty"), ("RUN OUT", "LEVEL (high = run)"),
            ("RST OUT", "5 ms pulse at step 1"), ("MIDI CLOCK OUT", "24 PPQN · on"), ("START ON", "host play  ·  CLK IN")]
    for i, (a, b) in enumerate(rows):
        y = 120 + i*31
        s.text(792, y+14, a, 8, "start", INK, 800); s.lcd(900, y, 280, 22, b, 9.5)
    s.box(780, 356, 410, 304, "CC MAP  (factory defaults)")
    cc = [("BD1 ATTACK", 2), ("BD1 DECAY", 64), ("BD1 PITCH", 65), ("BD1 TUNE", 3), ("BD1 NOISE", 4), ("BD1 FILTER", 5), ("BD1 DRIVE", 6), ("BD1 SOUND", 66),
          ("CP DECAY", 75), ("CP FILTER", 18), ("CP ATTACK", 76), ("CP SOUND", 77), ("TOM NOISE", 84), ("CB TUNE", 85), ("CB DECAY", 86), ("MA DECAY", 87)]
    for i, (n, c) in enumerate(cc):
        col = i // 8; row = i % 8
        x = 792 + col*200; y = 376 + row*33
        s.text(x, y+14, n, 8, "start"); s.lcd(x+104, y, 34, 20, str(c), 9, "middle"); s.key(x+142, y, 46, 20, "LEARN", z=7)
    return s

# ---------------------------------------------------------------- GLOBAL
def global_page():
    s = Svg(); chrome(s, "GLOBAL")
    s.box(10, 96, 384, 270, "QUALITY")
    s.text(24, 128, "OVERSAMPLING (realtime)", 8, "start", DIM, 800)
    for i, t in enumerate(["1×", "2×", "4×"]): s.key(24+i*60, 136, 54, 24, t, lit=GRN if t == "2×" else None)
    s.text(24, 184, "OVERSAMPLING (offline render)", 8, "start", DIM, 800)
    for i, t in enumerate(["SAME", "4×"]): s.key(24+i*60, 192, 54, 24, t, lit=GRN if t == "4×" else None)
    s.text(24, 240, "LATENCY", 8, "start", DIM, 800); s.lcd(24, 248, 170, 22, "23 smp · 0.48 ms @48k", 9.5)
    s.text(24, 296, "ENGINE RATE", 8, "start", DIM, 800); s.lcd(24, 304, 170, 22, "host 48 000 Hz native", 9.5)
    s.text(220, 240, "VOICES ACTIVE", 8, "start", DIM, 800)
    for i in range(16): s.led(226+i*10, 258, i in (0, 2, 4, 8, 9, 15), GRN, 2.5)
    s.box(404, 96, 384, 270, "ANALOG")
    for i, lb in enumerate(["DRIFT", "TOLERANCE", "NOISE FLOOR"]): s.knob(460+i*120, 152, 18, lb, [0.35, 0.5, 0.0][i])
    s.text(420, 226, "UNIT", 8, "start", DIM, 800); s.lcd(420, 234, 150, 24, "SN 0x5A31-C0DE", 10)
    s.key(578, 234, 96, 24, "RE-ROLL UNIT", z=7.5); s.key(680, 234, 96, 24, "IDEAL (0 %)", z=7.5)
    s.text(420, 290, "Tolerance = fixed per-voice offsets (saved in the", 7.5, "start", DIM, 600)
    s.text(420, 304, "patch). Drift = slow wander while playing.", 7.5, "start", DIM, 600)
    s.box(798, 96, 392, 270, "TUNING & DYNAMICS")
    s.knob(840, 152, 18, "A4 REF", .5); s.lcd(866, 140, 66, 22, "440.0 Hz", 9.5)
    s.knob(980, 152, 18, "TRANSPOSE", .5); s.lcd(1006, 140, 60, 22, "0 st", 9.5)
    s.text(812, 218, "TRIG DYNAMICS", 7.5, "start", DIM, 800)
    for i, t in enumerate(["GATE", "GATE+ACC", "GATE+DYN", "FULL DYN"]): s.key(812+i*92, 226, 86, 22, t, lit=GRN if i == 0 else None, z=7.5)
    s.text(812, 276, "VELOCITY CURVE", 7.5, "start", DIM, 800)
    for i, t in enumerate(["LINEAR", "SOFT", "HARD", "FIXED"]): s.key(812+i*92, 284, 86, 22, t, lit=GRN if i == 0 else None, z=7.5)
    s.box(10, 376, 580, 284, "INTERFACE")
    s.text(24, 404, "UI SCALE", 8, "start", DIM, 800)
    for i, t in enumerate(["75%", "100%", "125%", "150%", "200%"]): s.key(24+i*70, 412, 64, 26, t, lit=GRN if t == "100%" else None)
    s.text(24, 462, "100% = 1200 × 672 px  ·  knobs 22–36 px  ·  every hit target ≥ 28 × 28 px", 8, "start", INK, 600)
    for i, (a, b) in enumerate([("TOOLTIPS", True), ("KNOB DRAG: VERTICAL", True), ("SHOW VALUES ON HOVER", True), ("REMEMBER SCALE PER MACHINE", True)]):
        s.toggle(40, 494+i*30, "", "", b); s.text(56, 497+i*30, a, 8.5, "start")
    s.box(600, 376, 590, 284, "CPU & ENGINE")
    s.text(614, 404, "measured on this machine, last 2 s", 7.5, "start", DIM, 600)
    bars = [("VOICES (14 drums)", .22), ("LEAD + BASS @2×", .18), ("BUS FX @2×", .12), ("MASTER", .05)]
    for i, (a, v) in enumerate(bars):
        y = 420 + i*36
        s.text(614, y+12, a, 8.5, "start")
        s.add(f'<rect x="780" y="{y+2}" width="390" height="12" fill="#050505" stroke="#262826"/><rect x="781" y="{y+3}" width="{388*v:.1f}" height="10" fill="#37c25a"/>')
    s.key(614, 600, 120, 24, "PANIC / ALL OFF", z=8); s.key(744, 600, 120, 24, "INIT PATCH", z=8); s.key(874, 600, 150, 24, "SAVE AS DEFAULT", z=8)
    return s


# ---------------------------------------------------------------- MOD
def lfo_shape(kind, x, y, w, h, ph=0.0):
    pts = []
    for i in range(121):
        t = (i/120*2 + ph) % 1.0
        if kind == "SIN": v = math.sin(2*math.pi*t)
        elif kind == "TRI": v = 1 - 4*abs(t - 0.5) if True else 0
        elif kind == "SAW": v = 1 - 2*t
        elif kind == "SQR": v = 1 if t < 0.5 else -1
        else:
            k = int((i/120*2 + ph)*4); random.seed(k*7+3); v = random.uniform(-1, 1)
        pts.append(f"{x + w*i/120:.1f},{y + h/2 - v*h*0.42:.1f}")
    return "M" + " L".join(pts)
def mod_page():
    s = Svg(); chrome(s, "MOD")
    lf = [("LFO 1", "SIN", "1/4", "SYNC", "BI", "FREE-RUN", "—"), ("LFO 2", "TRI", "1/16", "SYNC", "UNI", "RETRIG", "CH TRIG"),
          ("LFO 3", "S&H", "1/32", "SYNC", "BI", "RETRIG", "SD TRIG"), ("LFO 4", "SAW", "0.35 Hz", "FREE", "BI", "FREE-RUN", "—")]
    for i, (nm, sh, rate, sy, pol, mode, src) in enumerate(lf):
        x = 10 + i*296; s.box(x, 96, 288, 232, nm)
        s.add(f'<rect x="{x+10}" y="112" width="268" height="62" rx="2" fill="#050705" stroke="#263026"/>')
        s.add(f'<line x1="{x+10}" y1="143" x2="{x+278}" y2="143" stroke="#1d2a1f" stroke-width=".8"/>')
        s.add(f'<path d="{lfo_shape(sh, x+14, 114, 260, 58, 0.1*i)}" stroke="{GRN}" stroke-width="1.6" fill="none"/>')
        s.text(x+270, 126, sh, 8, "end", GRN, 700)
        for k, (lb, v) in enumerate([("RATE", .45), ("SHAPE", .3), ("PHASE", .0), ("SLEW", .2 if sh == "S&H" else 0), ("PW / SKEW", .5)]):
            s.knob(x+32+k*56, 206, 13, lb, v, lz=7, steps=6 if lb == "SHAPE" else None)
        s.lcd(x+12, 236, 70, 18, rate, 9, "middle")
        s.toggle(x+124, 245, "FREE", "SYNC", sy == "SYNC", 7); s.toggle(x+222, 245, "UNI", "BI", pol == "BI", 7)
        for k, m in enumerate(["FREE-RUN", "RETRIG", "ONE-SHOT"]):
            s.key(x+12+k*72, 266, 66, 20, m, lit=GRN if m == mode else None, z=7)
        s.text(x+14, 310, "RETRIG BY", 7, "start", DIM, 700); s.lcd(x+68, 298, 90, 18, src, 8.5, "middle")
        s.text(x+168, 310, "DRAG ▸", 7, "start", DIM, 700)
        s.key(x+204, 298, 74, 20, "◉ " + nm, lit="#2c6b3c", z=7.5)
    # matrix
    s.box(10, 336, 780, 328, "MOD MATRIX  ·  32 slots  ·  every row is also a ring on its knob")
    hd = [("#", 22), ("SOURCE", 48), ("DESTINATION", 168), ("DEPTH  −100 … +100 %", 330), ("VIA", 520), ("CURVE", 590), ("ON", 668), ("", 730)]
    for t, xx in hd: s.text(xx, 356, t, 7.5, "start", DIM, 800)
    rows = [("LFO 1", "BD1 · DECAY", .18, "—", "LIN"), ("LFO 2", "CH · DECAY", -.16, "—", "LIN"), ("LFO 3", "SD · TUNE", .10, "ACC", "LIN"),
            ("ENV BASS", "BASS · CUTOFF", .30, "VEL", "EXP"), ("RND/HIT", "CP · FILTER", -.20, "—", "LIN"), ("LANE A", "MTC · WAVE", .25, "—", "LIN"),
            ("VEL", "OH · DECAY", .12, "—", "LIN"), ("LFO 4", "LEAD · CUTOFF", .22, "MOD W", "S-CRV"), ("ACC", "BD1 · DRIVE", .35, "—", "LIN"),
            ("LFO 1", "MASTER · WIDTH", -.08, "—", "LIN")]
    for r, (src, dst, d, via, cv) in enumerate(rows):
        y = 366 + r*28
        if r % 2 == 0: s.add(f'<rect x="14" y="{y-2}" width="772" height="26" fill="#ffffff" opacity=".025"/>')
        s.text(26, y+14, str(r+1), 8, "middle", DIM)
        s.lcd(46, y+2, 110, 18, src, 8.5); s.lcd(166, y+2, 150, 18, dst, 8.5)
        bx = 330; bw = 140
        s.add(f'<rect x="{bx}" y="{y+6}" width="{bw}" height="10" fill="#050505" stroke="#262826"/>')
        c0 = bx + bw/2; c1 = c0 + d*bw/2*2.0
        s.add(f'<rect x="{min(c0, c1):.1f}" y="{y+7}" width="{abs(c1-c0):.1f}" height="8" fill="{GRN if d > 0 else AMB}"/>')
        s.add(f'<line x1="{c0}" y1="{y+4}" x2="{c0}" y2="{y+18}" stroke="#bdbcb2" stroke-width=".8"/>')
        s.text(bx+bw+8, y+14, f"{d*100:+.0f} %", 8, "start", INK, 700)
        s.lcd(518, y+2, 60, 18, via, 8.5, "middle"); s.lcd(588, y+2, 64, 18, cv, 8.5, "middle")
        s.toggle(678, y+11, "", "", True, 6)
        s.key(724, y+1, 22, 20, "✕", z=8); s.key(750, y+1, 30, 20, "≡", z=8)
    s.text(400, 656, "right-click any knob ▸ Add modulation / Remove / Bypass · drag a source chip onto a knob · drag ≡ to reorder", 7, "middle", DIM, 600)
    # per-voice sources
    s.box(798, 336, 392, 328, "PER-VOICE SOURCES  ·  one set per voice")
    s.text(812, 360, "Each voice owns these. Used on its own knobs they follow its own hits;", 7, "start", DIM, 600)
    s.text(812, 372, "used on another voice they come from the voice named in SOURCE.", 7, "start", DIM, 600)
    chips = ["ENV", "VEL", "ACC", "RND/HIT", "LANE A", "LANE B", "PITCH ENV", "NOTE"]
    for k, c in enumerate(chips):
        s.key(812 + (k % 4)*94, 384 + (k//4)*28, 88, 22, "◉ " + c, lit="#2c6b3c" if k < 6 else None, z=7.5)
    s.text(812, 458, "LANE A · SD  (step values, −100 … +100 %)", 7.5, "start", INK, 800)
    s.add('<rect x="812" y="466" width="364" height="96" fill="#050705" stroke="#263026"/>')
    random.seed(11)
    for st in range(16):
        v = random.uniform(-1, 1); x0 = 816 + st*22.5; mid = 514
        h = v*42
        s.add(f'<rect x="{x0:.1f}" y="{min(mid, mid-h):.1f}" width="18" height="{abs(h):.1f}" fill="{GRN if v > 0 else AMB}" opacity=".9"/>')
    s.add('<line x1="812" y1="514" x2="1176" y2="514" stroke="#3a4a3c" stroke-width=".8"/>')
    for k, (lb, v) in enumerate([("SLEW", .2), ("LEN", .5), ("DEPTH", .7)]):
        s.knob(840 + k*64, 600, 12, lb, v, lz=7)
    s.key(1036, 584, 66, 20, "DRAW", lit=GRN, z=7.5); s.key(1108, 584, 66, 20, "CLEAR", z=7.5)
    s.key(1036, 610, 66, 20, "RANDOM", z=7.5); s.key(1108, 610, 66, 20, "LANE B ▸", z=7.5)
    return s

def render(name, svgdoc):
    html = f'<!doctype html><html><head><meta charset="utf-8"><style>html,body{{margin:0;padding:0;background:#0b0c0b;overflow:hidden}}</style></head><body>{svgdoc}</body></html>'
    p = os.path.join(OUT, name + ".html"); open(p, "w").write(html); return p

if __name__ == "__main__":
    pages = {"main": main_page, "voice": voice_page, "grid": grid_page, "mod": mod_page, "route": route_page, "fxmix": fx_page, "seqmidi": seq_page, "global": global_page}
    for k, f in pages.items():
        random.seed(5); render("panel_" + k, f().svg())
    print("ok")
