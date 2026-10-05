# Native-only AAD local review

Status: read-first local review of measured head `8816951` and its evidence-only
publication update. Fresh local functionality and paired performance pass;
all 28 exact `b42f9eb` CI checks and final D00/D01/D02/D03 audits pass.
The controlling [native-only specification](../specs/aad-native-only.md) replaces
earlier external-backend compatibility requirements. The full AAD goal remains active.

## Findings

No unresolved correctness or compatibility finding in the local native-only diff.
Review covered the production number/node/tape and scoped operations, their native
callers and mathematical tests, configuration and exports, examples, dependency
removals, installed consumers, scripts, CI gates and current-state documentation.

Resolved during verification:

- Diagnostic ON exposed `examples/vanilla` rewinding away its registered inputs.
  Narrow CTest reproduced the epoch failure. Scoped registration and reverse now
  preserve the graph; independent price/six-partial formulas verify the result.
  Labels follow the actual volatility/rate/dividend risk order. The changed include
  order initially omitted required platform declarations; the explicit include
  corrects both OFF/ON builds. Original failure logs remain.
- Removing `DAL_RATE_RISK_NATIVE_AAD` required removing its remaining benchmark
  guards. Native tape high-water instrumentation and its existing observations are
  retained unconditionally, before smoke or paired acceptance. Timed workloads,
  counters, result assertions and regression thresholds are unchanged.
- External adapters and selection scaffolding are removed. `native.hpp` retains
  seed replacement/addition, passive channel reads, mode/storage validation and
  compiled native capabilities. Scoped recording calls native functions directly.
  Default number/node/tape/scope layouts remain 16/40/368/72 bytes on this host.

## Open acceptance work

- Remaining P01 production phase/resource/scaling measurement and subsequent
  full-goal stages. No overall completion follows from this accepted increment.

Protected documentation follow-up: `CLAUDE.md:35` still lists legacy AAD selection
options. AGENTS.md explicitly forbids changing Claude originals without separate
authorization. Editable docs and Codex contracts describe native-only behavior;
reporting this exact protected location fulfills D00 R10.

## Tests

- Final fresh Release CTest: OFF 2,330/2,330; ON 2,359/2,359. Includes all 34
  examples, core/public, portable Excel and 793 Python tests in each configuration.
- Both final installed prefixes: 2/2 independent consumers. Scalar weighted and
  repeated VJPs, three vector channels including zero, diagnostic ABI propagation
  and supported closed-recording rejection pass.
- Ten native/legacy configuration and header cases pass. External ON configuration
  and direct compile macros reject explicitly; legacy OFF remains harmless.
- The final focused 60-case native/lifecycle/lifetime ASan/UBSan run passes with
  leak detection, using the fresh native ON support archive for other symbols;
  it does not claim that every library translation unit is locally instrumented.
- Full generation/drift check passes with zero generated files changed. CI helper
  suite passes 172 tests with six existing skips; YAML needs and matrix/gates pass.
- Documentation integrity passes for 75 files at the measured head and 78
  after the final audit/numeric evidence update. All 21 native benchmark smoke
  cases pass serially; smoke is not paired performance acceptance. Staged patch
  verification remains packaging work.
- Frozen performance: 65 formal comparable cases, 25 curve cases and the
  predefined two-by-thirty production confirmation's 44 cases pass. Initial
  pairing fails two GSR cases; both remain in the full
  [report](../perf/aad-native-only.md). The four-node fit confirmation remains
  +3.52%/+5.24%; its passing policy result is not an identical-runtime claim.
  The full published-native-head control also passes. All LSM numeric results
  agree, resources have zero major faults, and all source/configuration/pin/
  helper/22 binary identities remain unchanged. The control metadata's built-at
  annotation correction is explicit and its original record retained.

Logs and original failures are retained under `/tmp/dal-aad-evidence/native-only-*`.

## Summary

The source and fresh installed packages contain no supported external AAD type,
adapter, dependency or export. Five retained dependency pins are unchanged. Native
mathematical and lifecycle coverage is preserved; only external-specific tests
and CI legs are removed. Remaining names identify explicit migration rejection or
retained historical evidence.

All 28 exact `b42f9eb` checks succeed. Complete diagnostic sanitizer libraries,
Windows/consumers, all native compilers and wheel platforms, static analysis and
both gates are included. Its matrix helper correction preserves every assertion
and current interpreter. Library/build/benchmark source equivalence to `8816951`
and all 22 measured binary digests are rechecked. The final contract audit accepts
D00/D01/D02/D03 with the report's limits. The supplementary numeric comparison
passes ten ordinary MC plus 25 curve cases and 34,028 finite numeric cells,
including all required risk vectors/residual/Jacobian/inverse values. It closes
D01's A12 evidence gap without changing production code or timed workloads.
P01 remains open.

Verdict: Comment Only for the full unfinished Stage A and draft development PR.
