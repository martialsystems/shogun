# Copyright (c) 2026 Martial Systems LLC. All rights reserved.
"""Clock-switch law for SHOGUN.

The panel switch chooses INT or EXT. A Trig cable cannot force EXT.
INT fires from the pattern and ignores Trig jacks. EXT fires from the
Trig jacks and ignores the pattern.

The two states below are the legal modes with a cable plugged. The gate
must allow both. Printed sample rows stay named tests in tests/voices.cpp.
Channels here are the switch, the cable claim, and which source fires.
"""

from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List

from graphforge.declarative import load_law_graph

_LAW = Path(__file__).resolve().parent / "graphs" / "switch_law.json"

# Plugged cable, switch still chooses. The gate allows this state.
_INT_CABLE_PLUGGED: Dict[str, Any] = {
    "switch": "int",
    "cable_plugged": True,
    "cable_forces_ext": False,
    "pattern_fires_voices": True,
    "trig_jacks_fire_voices": False,
}

_EXT_CABLE_PLUGGED: Dict[str, Any] = {
    "switch": "ext",
    "cable_plugged": True,
    "cable_forces_ext": False,
    "pattern_fires_voices": False,
    "trig_jacks_fire_voices": True,
}


def build_clock_switch():
    """Load the declarative clock-switch graph."""
    return load_law_graph(_LAW)


def laws() -> List[Dict[str, Any]]:
    """Legal INT and legal EXT. Refusals are checked by the switch-law test."""
    return [
        {
            "id": "shogun.clock_switch.int",
            "build": build_clock_switch,
            "state": dict(_INT_CABLE_PLUGGED),
            "allow_decisions": ["allow"],
        },
        {
            "id": "shogun.clock_switch.ext",
            "build": build_clock_switch,
            "state": dict(_EXT_CABLE_PLUGGED),
            "allow_decisions": ["allow"],
        },
    ]
