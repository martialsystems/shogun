#!/usr/bin/env python3
"""No dead panel keys: every key, toggle, knob and jack in plugin/Source/PanelLayout.inc must carry a binding that
the editor parses (pres[] in PluginEditor.cpp), treats as clickable (findBound) and handles (click / matrixClick)
with a non-empty handler, and every parameter it names must exist in engine/params_table.h.

Bound is not enough: a CLOCK:/GLOBAL: parameter (read by enum, P_CLOCK_FILL etc.) must be read by the engine or the
plugin, and a CLOCK/MOD/MIX jack must be read or driven by the engine with something other than a constant 0.

Exit 0 when clean; prints every offending op and exits 1 otherwise."""
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
KIND = ["TEXT", "RTEXT", "RULE", "BOX", "KNOB", "LED", "KEY", "LCD", "TOGGLE", "JACK", "RECT", "CIRCLE", "LINE", "PATH"]
INTERACTIVE = {"KEY", "TOGGLE", "KNOB", "JACK"}
TABS = ["MAIN", "VOICE", "GRID", "MOD", "ROUTE", "FX/MIX", "SEQ/MIDI", "GLOBAL"]
STR = r'"((?:[^"\\]|\\.)*)"'


def unesc(s):
    return bytes(s, "utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8")


def function_body(src, signature):
    i = src.find(signature)
    if i < 0:
        sys.exit("check_panel_bindings: cannot find %r" % signature)
    j = src.find("{", i)
    depth = 0
    for k in range(j, len(src)):
        if src[k] == "{":
            depth += 1
        elif src[k] == "}":
            depth -= 1
            if depth == 0:
                return src[j:k + 1]
    sys.exit("check_panel_bindings: unbalanced %r" % signature)


def main():
    ed = open(os.path.join(ROOT, "plugin", "Source", "PluginEditor.cpp"), encoding="utf-8").read()
    inc = open(os.path.join(ROOT, "plugin", "Source", "PanelLayout.inc"), encoding="utf-8").read()
    table = open(os.path.join(ROOT, "engine", "params_table.h"), encoding="utf-8").read()
    params = set(re.findall(r'\{"([^"]+)", ParamKind::', table))
    voices = ["BD1", "BD2", "SD", "RS", "CP", "CL", "MA", "CB", "CH", "OH", "CY", "LTC", "MTC", "HTC", "LEAD", "BASS"]

    build = function_body(ed, "void ShogunPanel::buildBindings()")
    pres = dict(re.findall(r'\{"([a-z0-9]+)", (B_[A-Z0-9]+)\}', build))
    fb = function_body(ed, "int ShogunPanel::findBound(")
    sw = fb[fb.find("switch (b.kind)"):]
    clickable = set(re.findall(r"case (B_[A-Z0-9]+)", sw[:sw.find("return *it;")]))
    click_body = function_body(ed, "void ShogunPanel::click(")
    handled = set(re.findall(r"case (B_[A-Z0-9]+)", click_body))
    inert = set(re.findall(r"case (B_[A-Z0-9]+):\s*break;", click_body))  # a case that does nothing
    handled -= inert
    users = ""
    for d, _, fs in os.walk(os.path.join(ROOT, "engine")):
        for f in fs:
            if f.endswith((".cpp", ".h")) and f != "params_table.h":
                users += open(os.path.join(d, f), encoding="utf-8").read()
    users += open(os.path.join(ROOT, "plugin", "Source", "PluginProcessor.cpp"), encoding="utf-8").read()
    engine_cpp = open(os.path.join(ROOT, "engine", "shogun.cpp"), encoding="utf-8").read()

    def enum_of(pid):
        return "P_" + re.sub(r"[^A-Z0-9]", "_", pid.upper())

    def param_read(pid):
        if pid.split(":")[0] not in ("CLOCK", "GLOBAL"):
            return True  # voice / bus / LFO blocks are read through per-block offsets
        return re.search(r"\b%s\b" % enum_of(pid), users) is not None

    def jack_live(jid):
        sec, _, lab = jid.partition(":")
        if sec not in ("CLOCK", "MOD", "MIX"):
            return True  # voice jacks go through drumPort()/synthPort()
        names = {"PORT_" + lab.replace(" ", "_"), "PORT_" + lab.replace(" ", ""), "PORT_%s_%s" % (sec, lab.replace(" ", "_"))}
        for line in engine_cpp.splitlines():
            for n in names:
                base = re.sub(r"\d$", "1", n)
                if re.search(r"\b%s\b" % n, line) or (base != n and re.search(r"\b%s \+ " % base, line)):
                    if not re.search(r"\]\s*=\s*0(\.0)?f?;", line):
                        return True
        return False
    if not pres or not clickable or not handled:
        sys.exit("check_panel_bindings: could not parse the editor tables")

    rows = re.findall(r"^\s*\{(\d+), (\d+), \d+, ([-\d.]+)f, ([-\d.]+)f,.*?" + STR + ", " + STR + ", " + STR + r", -?\d+, -?\d+\},$",
                      inc, re.M)
    if len(rows) < 1000:
        sys.exit("check_panel_bindings: parsed only %d rows of PanelLayout.inc" % len(rows))
    bad = []
    checked = 0
    for k, tab, x, y, text, text2, bind in rows:
        kind = KIND[int(k)]
        if kind not in INTERACTIVE:
            continue
        checked += 1
        text, bind = unesc(text), unesc(bind)
        where = "%-8s %-6s x=%-7s y=%-6s %-22r" % (TABS[int(tab)], kind, x, y, text)
        if not bind:
            bad.append(where + " no binding")
            continue
        pre, _, rest = bind.partition(":")
        bk = pres.get(pre)
        if bk is None:
            bad.append(where + " bind %r: prefix not parsed by the editor" % bind)
            continue
        if bk not in clickable:
            bad.append(where + " bind %r: %s is not clickable (findBound)" % (bind, bk))
            continue
        if bk not in handled and bk != "B_MATRIX":
            bad.append(where + " bind %r: %s has no click handler" % (bind, bk))
            continue
        pid = rest if pre in ("p", "disp") else (rest.rpartition(":")[0] if pre == "choice" else None)
        if pid is not None and pid not in params:
            bad.append(where + " bind %r: no such parameter" % bind)
        elif pid is not None and not param_read(pid):
            bad.append(where + " bind %r: nothing reads %s" % (bind, enum_of(pid)))
        elif pre == "jack" and not jack_live(rest):
            bad.append(where + " bind %r: the engine never reads or drives this jack" % bind)
        elif pre in ("sp", "wp") and rest not in ("cvamt", "CV AMT") and not any("%s:%s" % (v, rest) in params for v in voices):
            bad.append(where + " bind %r: no voice has this parameter" % bind)
    for b in bad:
        print("DEAD  " + b)
    print("check_panel_bindings: %d interactive ops, %d dead" % (checked, len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
