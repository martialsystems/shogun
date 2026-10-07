# Repository setup (2026-10-06)

## Tree

Public repository: `https://github.com/martialsystems/shogun`

Working name: SHOGUN. A separate product name has not been set. The local checkout used for this pack is `/Users/samw/shogun`, branch `main`.

This commit contains:

| Path | Role |
| --- | --- |
| README.md | Front page and revision list |
| METHODOLOGY.md | Sources and the quoted choices |
| SCHEMATICS.md | Equations and 48 kHz rows |
| BUILD_GUIDE.md | PDF build, and the shape of a later program |
| TESTPLAN.md | Named tests |
| REPO_SETUP.md | This file |
| scripts/build_design_pack_pdf.py | Markdown to PDF |
| docs/SHOGUN_Design_Pack.pdf | The pack as one PDF |
| LICENSE | All rights reserved, Martial Systems LLC |
| .gitignore | `build/` and `__pycache__/` |

There is no `src/` audio module, no panel SVG, and no JUCE target.

The MFB manuals are not in the tree. The German PDF that was read for the quotations stays outside the repository. The pack cites it. It does not redistribute it.

## PDF

```
python3 scripts/build_design_pack_pdf.py
```

The script fails if a source file contains an em dash, an en dash, or a heading that defines the work by absence. The PDF is a flowing render of the six markdown files, in the order README, METHODOLOGY, SCHEMATICS, BUILD_GUIDE, TESTPLAN, REPO_SETUP. A title line on each PDF page carries the date 2026-10-06. That date is the revision date. A clock time inside the PDF metadata is only the build time.

## Collection link

`martialsystems/jidai-collection` names this repository under the SHOGUN heading on `main`. The rack plugin in that repository compiles BUSHIDO and RONIN. It does not fetch SHOGUN. Leave it that way until an engine exists and the tests in TESTPLAN.md have somewhere to run.

## Git

Commits on this repository use author `Martial Systems LLC` and `25778085+martialsystems@users.noreply.github.com`, the same author as the collection repository.

The license grants no right to copy the pack without permission from Martial Systems LLC. Short quotations in METHODOLOGY.md are the disputed parameter sentences, with the source named beside each one.

## Clock switch, as a repository fact

INT and EXT are specified in METHODOLOGY.md and SCHEMATICS.md. They are not a control from the Tanzbär manuals. A later patch against this repository that removes the switch, or that lets a Trig cable force EXT, breaks `testExtBypassIgnoresPattern` and `testIntIgnoresTrigJacks`.
