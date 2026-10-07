# Copyright (c) 2026 Martial Systems LLC. All rights reserved.
"""Refuse a Trig cable that forces EXT. Allow the switch to choose.

Run:
  PYTHONPATH=~/graphforge/src python3 forge/tests/test_switch_law.py

The printed 48 kHz rows stay in tests/voices.cpp. This file checks that
those coefficients are absent from the law, and that require_law allows
the two switch positions and blocks the cable override.
"""

from __future__ import annotations

import importlib.util
import json
import logging
import os
import sys
from pathlib import Path

FORGE = Path(__file__).resolve().parents[1]
LAW_PATH = FORGE / "graphs" / "switch_law.json"
PIN_PATH = FORGE / "engine_pin.json"

# Placeholder coefficients from the voice tests. They must not enter the law.
BANNED_ROW_TOKENS = (
    "0.208884",
    "0.832547",
    "0.819291",
    "83.814360",
    "1e-5",
    "48000",
)

CHANNELS = {
    "switch",
    "cable_plugged",
    "cable_forces_ext",
    "pattern_fires_voices",
    "trig_jacks_fire_voices",
    "decision",
    "violations",
}


def _load_product_laws():
    path = FORGE / "product_laws.py"
    spec = importlib.util.spec_from_file_location("shogun_product_laws", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def _version_tuple(text: str) -> tuple:
    parts = []
    for piece in str(text).split(".")[:3]:
        digits = ""
        for ch in piece:
            if ch.isdigit():
                digits += ch
            else:
                break
        parts.append(int(digits or "0"))
    while len(parts) < 3:
        parts.append(0)
    return tuple(parts)


def main() -> int:
    os.environ.pop("GRAPHFORGE_AUDIT_LOG", None)
    os.environ.pop("GRAPHFORGE_AUDIT_STDOUT", None)
    os.environ.pop("GRAPHFORGE_AUDIT_WEBHOOK_URL", None)

    from graphforge import __version__
    from graphforge.consumer_gate import gate_consumer
    from graphforge.errors import InvalidUpdateError
    from graphforge.product_law import LawBlockedError, require_law

    # configure_logging() runs on import and sets INFO. Drop it after that.
    logging.getLogger("graphforge").setLevel(logging.ERROR)

    failures = []

    def fail(label: str) -> None:
        failures.append(label)

    doc = json.loads(LAW_PATH.read_text(encoding="utf-8"))
    pin = json.loads(PIN_PATH.read_text(encoding="utf-8"))
    rules = doc.get("rules") or []

    if doc.get("name") != "shogun.clock_switch":
        fail(f"law name {doc.get('name')!r}")
    if set(doc.get("channels") or {}) != CHANNELS:
        fail(f"channels {sorted((doc.get('channels') or {}))}")
    if any(isinstance(rule, dict) and rule.get("else") is True for rule in rules):
        fail("unconditional else would allow an unknown switch")
    if not rules or rules[0].get("when") != {"cable_forces_ext": True}:
        fail(f"first rule is not cable_forces_ext: {rules[:1]}")
    allow_rules = [r for r in rules if r.get("decision") == "allow"]
    if len(allow_rules) != 2:
        fail(f"allow rule count {len(allow_rules)}")
    for rule in allow_rules:
        if "cable_plugged" in (rule.get("when") or {}):
            fail("allow rule matches cable_plugged, so one jack state cannot pass")
    if _version_tuple(__version__) < _version_tuple(pin.get("min_version")):
        fail(f"engine {__version__} older than pin {pin.get('min_version')}")
    if pin.get("engine") != "graphforge" or pin.get("policy") != "report_only":
        fail(f"pin engine/policy {pin.get('engine')!r} {pin.get('policy')!r}")
    if pin.get("require_product_laws") is not True:
        fail("require_product_laws is not true")

    for path in FORGE.rglob("*"):
        if not path.is_file() or "tests" in path.parts:
            continue
        if path.suffix not in {".json", ".py", ".md"}:
            continue
        text = path.read_text(encoding="utf-8")
        for token in BANNED_ROW_TOKENS:
            if token in text:
                fail(f"{path.name} contains sample token {token}")

    laws_mod = _load_product_laws()
    graph = laws_mod.build_clock_switch()
    law_rows = laws_mod.laws()
    ids = [row.get("id") for row in law_rows]
    if ids != ["shogun.clock_switch.int", "shogun.clock_switch.ext"]:
        fail(f"law ids {ids}")

    calls = {"n": 0}

    def ask(state: dict):
        calls["n"] += 1
        return require_law(
            graph,
            state,
            allow_decisions={"allow"},
            law_id="shogun.clock_switch",
            thread_id=f"shogun-switch-{calls['n']}",
            audit=False,
        )

    def expect_allow(label: str, state: dict) -> None:
        try:
            result = ask(state)
        except LawBlockedError as exc:
            fail(f"{label}: blocked {exc.result.violations}")
            return
        if not result.ok or result.decision != "allow" or result.violations:
            fail(f"{label}: ok={result.ok} decision={result.decision} {result.violations}")

    def expect_block(label: str, state: dict, violation: str) -> None:
        try:
            result = ask(state)
        except LawBlockedError as exc:
            result = exc.result
        else:
            fail(f"{label}: allowed decision={result.decision}")
            return
        if result.ok or result.decision != "block" or violation not in result.violations:
            fail(
                f"{label}: ok={result.ok} decision={result.decision} "
                f"violations={result.violations} want {violation}"
            )

    base_int = {
        "switch": "int",
        "cable_forces_ext": False,
        "pattern_fires_voices": True,
        "trig_jacks_fire_voices": False,
    }
    base_ext = {
        "switch": "ext",
        "cable_forces_ext": False,
        "pattern_fires_voices": False,
        "trig_jacks_fire_voices": True,
    }

    expect_allow("int cable plugged", {**base_int, "cable_plugged": True})
    expect_allow("int cable open", {**base_int, "cable_plugged": False})
    expect_allow("ext cable plugged", {**base_ext, "cable_plugged": True})
    expect_allow("ext cable open", {**base_ext, "cable_plugged": False})

    # The helpful rewrite: a plugged jack selects EXT, including when the
    # presented switch has already been overwritten to ext.
    expect_block(
        "cable forces ext while switch reads int",
        {
            "switch": "int",
            "cable_plugged": True,
            "cable_forces_ext": True,
            "pattern_fires_voices": False,
            "trig_jacks_fire_voices": True,
        },
        "cable_forces_ext",
    )
    expect_block(
        "cable forces ext while switch reads ext",
        {
            "switch": "ext",
            "cable_plugged": True,
            "cable_forces_ext": True,
            "pattern_fires_voices": False,
            "trig_jacks_fire_voices": True,
        },
        "cable_forces_ext",
    )
    expect_block(
        "cable forces ext flag alone",
        {**base_int, "cable_plugged": False, "cable_forces_ext": True},
        "cable_forces_ext",
    )
    expect_block(
        "int reads jacks with cable plugged",
        {
            "switch": "int",
            "cable_plugged": True,
            "cable_forces_ext": False,
            "pattern_fires_voices": True,
            "trig_jacks_fire_voices": True,
        },
        "int_reads_trig_jacks",
    )
    expect_block(
        "int drops pattern",
        {
            "switch": "int",
            "cable_plugged": False,
            "cable_forces_ext": False,
            "pattern_fires_voices": False,
            "trig_jacks_fire_voices": False,
        },
        "int_drops_pattern",
    )
    expect_block(
        "plugged jack wins without the flag",
        {
            "switch": "int",
            "cable_plugged": True,
            "cable_forces_ext": False,
            "pattern_fires_voices": False,
            "trig_jacks_fire_voices": True,
        },
        "int_reads_trig_jacks",
    )
    expect_block(
        "ext reads pattern",
        {
            "switch": "ext",
            "cable_plugged": True,
            "cable_forces_ext": False,
            "pattern_fires_voices": True,
            "trig_jacks_fire_voices": True,
        },
        "ext_reads_pattern",
    )
    expect_block(
        "ext ignores jacks",
        {
            "switch": "ext",
            "cable_plugged": True,
            "cable_forces_ext": False,
            "pattern_fires_voices": False,
            "trig_jacks_fire_voices": False,
        },
        "ext_ignores_trig_jacks",
    )
    expect_block(
        "unknown switch spelling INT",
        {**base_int, "switch": "INT", "cable_plugged": False},
        "no_rule_matched",
    )
    expect_block(
        "unknown switch spelling EXT",
        {**base_ext, "switch": "EXT", "cable_plugged": True},
        "no_rule_matched",
    )
    expect_block(
        "missing switch",
        {
            "cable_plugged": True,
            "cable_forces_ext": False,
            "pattern_fires_voices": True,
            "trig_jacks_fire_voices": False,
        },
        "no_rule_matched",
    )
    expect_block("switch only", {"switch": "int"}, "no_rule_matched")
    expect_block("empty state", {}, "no_rule_matched")

    try:
        ask({**base_int, "cable_plugged": True, "y": 0.208884})
    except InvalidUpdateError:
        pass
    except LawBlockedError as exc:
        fail(f"sample key y was a law decision {exc.result.violations}")
    else:
        fail("sample key y was accepted")

    for row in law_rows:
        if row.get("allow_decisions") != ["allow"]:
            fail(f"{row.get('id')} allow_decisions {row.get('allow_decisions')}")
        expect_allow(f"gate state {row.get('id')}", dict(row.get("state") or {}))

    report = gate_consumer(
        {"id": "shogun", "local_path": str(FORGE)},
        run_verify=False,
    )
    if not report.ok:
        fail(f"gate_consumer {report.errors} {report.detail}")
    seen = {result.law_id for result in report.law_results}
    for lid in ("shogun.clock_switch.int", "shogun.clock_switch.ext"):
        if lid not in seen:
            fail(f"gate missed {lid}; saw {sorted(seen)}")
    for result in report.law_results:
        if result.law_id.startswith("shogun.clock_switch") and (
            not result.ok or result.decision != "allow"
        ):
            fail(f"gate {result.law_id} decision={result.decision} {result.violations}")

    if failures:
        print(f"clock switch law: {len(failures)} failed")
        for item in failures:
            print(f"  {item}")
        return 1
    print(f"clock switch law: {calls['n']} require_law calls, gate ok")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"clock switch law: error {exc}", file=sys.stderr)
        raise
