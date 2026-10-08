# Native structural Jacobian cost acceptance

Status: local acceptance complete; exact publication gates pending.
Base: merged #507, `c11ebd8194fbbd340f99b43019fe1946c70ec618`.
Scope: the new native execution adapter; financial support analysis, identity
invalidation and default strategy selection remain separate P04 work.

## Existing paths

All 177 existing default core objects retain their SHA-256 identities. Archive
membership retains those 177 objects and adds only
`structuraljacobiannative.cpp.o`. Existing production files and CMake inputs are
unchanged. Seven freshly linked accepted callers retain byte-identical binaries.
These callers cover checked/diagnosed solves, coordinates, numeric/native roots,
PDE and passive execution. Zero old timing rows are repeated.

This supports retaining the accepted existing-path evidence. It does not claim
a speedup for a financial request or a default-strategy improvement.

## New complete-request costs

The frozen selection has one independently proved 64-by-64 three-point linear
stencil, three colors and 34,304 bytes of simultaneous numeric payload. Four
rows distinguish scalar width one and vector width four, each with either cold
plan construction or a reused numeric plan and fresh graph. Supports come from
the formula, not measured derivative zeros.

Every timed request includes mode/scope setup, input registration and binding,
fresh recording at a changed numerical point, complete reverse blocks, recovery,
independent verification of every output and all 4,096 Jacobian entries, checksum,
and release. Cold rows additionally normalize/color/build the owning plan.
Caller-supplied supports and analytic reference formula are outside timing.
Reused-plan rows still re-record and bind each request; they do not reuse a graph.

GCC 15 Release, `-O3 -DNDEBUG -ffp-contract=fast`, diagnostics OFF, CPU 0,
`DAL_NUM_THREADS=4`, zero actual workers. Each row has eight unmeasured warmups
and 1,024 requests per observation. Two rounds of ten processes produce eighty
observations in 1.5524 seconds. Round minima in milliseconds per full request:

| Mode           | Plan                | Round 1   | Round 2   |
|----------------|---------------------|-----------|-----------|
| Scalar         | Cold                | 0.0232722 | 0.0229012 |
| Scalar         | Reused, fresh graph | 0.0113935 | 0.0112434 |
| Vector width 4 | Cold                | 0.0225173 | 0.0225057 |
| Vector width 4 | Reused, fresh graph | 0.0120374 | 0.0119380 |

These are informational costs of the new entry. Three versus one reverse sweeps
does not alone establish which mode is faster. This fixture includes no
financial dependency provider or descriptor equality cost and cannot promote a
financial default strategy.

## Evidence

Retained outside the source tree at
`/home/wegamekinglc/.cache/dal-aad-evidence-20261008`:

- `native-structural-performance/identity-proof.json`: immutable accepted/head
  archives, all old object identities and seven fresh caller commands/hashes.
- `native-structural-performance/selection.json` and `results.json`: frozen
  scope, exact commands, environment, source/binary/archive hashes, all eighty
  observations and round minima.
- Per-process stdout/stderr and `sampling.log`; independent full-matrix checksum
  is 65 per request and 66,560 per observation.
- `native-structural-final-tests.log`, `native-structural-strict.json` and
  `native-structural-installed-tests.log`: thirteen cases, six strict checks and
  one installed consumer. The consumer includes only installed DAL headers.

Measured sources and the immutable head archive remain bound by hashes.
Any later production change requires renewed acceptance only for its affected
scope. CI queue/build time is separate from developer effort and timing cost.
