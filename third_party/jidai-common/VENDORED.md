# Vendored: jidai-common

| | |
| --- | --- |
| Source repo | https://github.com/martialsystems/jidai-collection (local clone `/workspace/jidai-collection`) |
| Path | `jidai-common/` |
| Branch | `redesign/jidai` |
| Commit | **9d6e382** (9d6e382843ec998016d37ab5a76af3150a92e9c7), "Phase 2: shared TripleShaper + halfband, ORIGAMI core, plugin and rack device" |
| `jidai-common` tree | 8dc7775b57f6cc5bffa8fcec3d5dbb9e45766ac9 |
| Copied with | `git -C /workspace/jidai-collection archive 9d6e382 jidai-common \| tar -x -C third_party` |

**Why 9d6e382.** 9d6e382 is the last commit that touches `jidai-common/`. Its `jidai-common` tree (8dc7775b) is
the same as the branch tip at copy time (2e1cef5), so the headers match the current head. The earlier commit
829ebd2 has the JCS headers only, with no `include/jidai/dsp/` (no `TripleShaper.h`, no `Halfband.h`). Compared with
829ebd2, 9d6e382 adds `dsp/TripleShaper.h`, `dsp/Halfband.h`, `tests/DspTests.cpp` and `tests/TestFft.h`, and it
changes one R16 comment and enum name in `jcs/Volts.h` (`Serge2V5` → `Level2V5`).

**Do not edit these files.** Fix them upstream in jidai-collection and re-vendor. SHOGUN works around the differences
in its own code (`engine/wave_shaper.h`, `engine/ports.h`, `engine/shogun.cpp`). The differences are listed in
`TESTPLAN.md` under "jidai-common".

Include path: `third_party/jidai-common/include`. That path is set in the Makefile (tests, asan, web) and in
`plugin/CMakeLists.txt`. The engine includes single headers (`jidai/jcs/Detect.h`, `Pitch.h`, `Roles.h`,
`jidai/dsp/TripleShaper.h`, `jidai/dsp/Halfband.h`), not the umbrella `jidai/CableStandard.h`. The umbrella pulls in
`JackId.h`, `State.h` and `Graph.h`, which need `<optional>`, `<string>` and `<vector>`, and the freestanding
WebAssembly build has none of those. The plugin and the native tests include `<jidai/CableStandard.h>`.
