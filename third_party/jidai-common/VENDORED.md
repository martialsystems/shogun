# Vendored: jidai-common 1.1.1

| | |
| --- | --- |
| Source repo | https://github.com/martialsystems/jidai-collection (local clone `/workspace/jidai-collection`) |
| Path | `jidai-common/` (the whole folder) |
| Branch | `redesign/jidai` |
| Version | **1.1.1** (`project(JidaiCommon VERSION 1.1.1)`) |
| Commit | **24ee621** (24ee62135a824e09e81f823b1eb277d9e0cc87b7), "jidai-common 1.1.1: SHOGUN + RONIN fix batch (re-vendor this commit)" |
| `jidai-common` tree | a3eefc0a7d2bbd22451e2c4f2a889c7e0eb04472 |
| Copied with | `git -C /workspace/jidai-collection archive 24ee621 jidai-common \| tar -x -C third_party` |

Copied from the local clone at exactly that commit; the commit was not yet pushed upstream at copy time. The previous
vendoring was 1.1.0 at 9d6e382. 1.1.1 changes, as used here:

- `pitch::kC3Hz` is exactly 130.8127826502993 Hz; `pitch::lin55ToVoct(V)` is exactly `log2 V − 1.25`.
- `TripleShaper` skips stages per block only (`planBlock(ctl, steady, vcLive)`); without a plan no stage is skipped.
- `AliasTable` carries an input law per alias (`AliasLaw`, `resolve(id)` → id + `conversion.convert(V)`).
- Headers are clean under `-Wfloat-equal`, `-Wconversion` and `-Wdouble-promotion` (upstream `headers` test).
- The `LevelComp` 1e-30 detector floor is documented; the R16 level and the test prefixes use neutral names.

**Do not edit these files.** Fix them upstream in jidai-collection and re-vendor. What SHOGUN still keeps on its side
is listed in `TESTPLAN.md` under "jidai-common" (the alias fan-out and legacy-name shim in `engine/ports.h`).

Include path: `third_party/jidai-common/include`. That path is set in the Makefile (tests, asan, web) and in
`plugin/CMakeLists.txt`. The engine includes single headers (`jidai/jcs/Detect.h`, `Pitch.h`, `Roles.h`, `JackId.h`,
`jidai/dsp/TripleShaper.h`, `jidai/dsp/Halfband.h`). The freestanding WebAssembly build has small `<string>`,
`<string_view>`, `<optional>` and `<vector>` shims for `JackId.h` (load time only). The plugin and the native tests
include the umbrella `<jidai/CableStandard.h>`.
