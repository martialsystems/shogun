# Repository setup (2026-10-06)

## Tree

Public repository: `https://github.com/martialsystems/shogun`

Working name: SHOGUN. A separate product name has not been set. The engine commit is on branch `feat/engine-voices`, cut from the design pack in this repository. The clock-switch pin is on that branch, in `forge/`.

This tree contains:

| Path | Role |
| --- | --- |
| README.md | Front page and revision list |
| METHODOLOGY.md | Sources and the quoted choices |
| SCHEMATICS.md | Equations and 48 kHz rows |
| BUILD_GUIDE.md | PDF build, and the engine layout |
| TESTPLAN.md | Named tests |
| REPO_SETUP.md | This file |
| engine/dsp.h | Shared STAND-IN helpers |
| engine/shogun.h | Clock, pattern, knobs, and the process call |
| engine/shogun.cpp | Voices and the output sum |
| tests/voices.cpp | Named checks |
| Makefile | Builds `build/shogun_tests` and runs the clock-switch law |
| scripts/build_design_pack_pdf.py | Markdown to PDF |
| docs/SHOGUN_Design_Pack.pdf | The pack as one PDF |
| forge/engine_pin.json | GraphForge pin, clock switch only |
| forge/product_laws.py | Legal INT and legal EXT states passed to require_law |
| forge/graphs/switch_law.json | The switch law. A Trig cable cannot force EXT |
| forge/tests/test_switch_law.py | Allows both switch positions and blocks the cable override |
| LICENSE | All rights reserved, Martial Systems LLC |
| .gitignore | `build/` and `__pycache__/` |

The voice engine is framework-free C++ under `engine/`. `process()` does not allocate. There is no panel SVG and no JUCE target.

The MFB manuals are not in the tree. The German PDF that was read for the quotations stays outside the repository. The pack cites it. It does not redistribute it.

## PDF

```
python3 scripts/build_design_pack_pdf.py
```

The script fails if a source file contains an em dash, an en dash, or a heading that defines the work by absence. The PDF is a flowing render of the six markdown files, in the order README, METHODOLOGY, SCHEMATICS, BUILD_GUIDE, TESTPLAN, REPO_SETUP. A title line on each PDF page carries the date 2026-10-06. That date is the revision date. A clock time inside the PDF metadata is only the build time.

## Collection link

`martialsystems/jidai-collection` names this repository under the SHOGUN heading on `main`. The rack plugin in that repository compiles BUSHIDO and RONIN. It does not fetch SHOGUN. This engine does not change the rack.

## Git

Commits on this repository use author `Martial Systems LLC` and `25778085+martialsystems@users.noreply.github.com`, the same author as the collection repository.

The license grants no right to copy the pack without permission from Martial Systems LLC. Short quotations in METHODOLOGY.md are the disputed parameter sentences, with the source named beside each one.

## Clock switch

INT ignores Trig jacks. EXT ignores the pattern. The switch chooses. A plugged Trig cable does not force EXT.

That rule is `forge/graphs/switch_law.json`, loaded by `forge/product_laws.py`. `forge/engine_pin.json` pins GraphForge for this law. `forge/tests/test_switch_law.py` asks `require_law` to allow both switch positions with a cable plugged, and to block a cable that forces EXT, an INT state that reads Trig jacks, and an EXT state that fires from the pattern.

The printed 48 kHz rows stay in `tests/voices.cpp`. A later fit may change a row when SCHEMATICS.md and that test change together.

INT and EXT are specified in METHODOLOGY.md and SCHEMATICS.md. The engine checks are `testExtBypassIgnoresPattern` and `testIntIgnoresTrigJacks`.
