# Native-only AAD local review

Status: read-first local review before frozen D00/D03 performance and publication.
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

- Fresh unchanged nine-target and affected production pairing, with immutable
  source, helper, configuration, dependency and binary identities.
- Exact new-head CI, including complete diagnostic sanitizer libraries, Windows,
  wheels and all remaining protected gates. The 49 green checks at `9b5febc`
  do not validate these changes.
- Final requirement audit before checking D00/D03 off. D01/D02 and the remaining
  production measurement scope also require their own final audits.

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
- Documentation integrity passes for 75 files. All 21 native benchmark smoke
  cases pass serially; smoke is not paired performance acceptance. Staged patch
  verification remains packaging work.

Logs and original failures are retained under `/tmp/dal-aad-evidence/native-only-*`.

## Summary

The source and fresh installed packages contain no supported external AAD type,
adapter, dependency or export. Five retained dependency pins are unchanged. Native
mathematical and lifecycle coverage is preserved; only external-specific tests
and CI legs are removed. Remaining names identify explicit migration rejection or
retained historical evidence.

Verdict: Comment Only until performance and exact-head CI acceptance complete.
