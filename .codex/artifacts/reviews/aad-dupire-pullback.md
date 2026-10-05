# F01 frozen Dupire core review

Verdict: Comment Only. No unresolved correctness findings in the reviewed core
increment; the remaining F01 integration is open.

## Findings

No unresolved findings in the local scalar calibration boundary. The read-first
review inspected the complete new header/source/tests, IVS and installed consumer,
changed workflow, method documentation and controlling specification/API/critique.
It identified a callback-visible name-copy defect before acceptance: an IVS callback
could mutate the display-name argument after settings were frozen. The focused
test fails before the fix and passes after copying the name before sampling.

Earlier RED/GREEN evidence also covers unrepresentable supplementary-grid spacing
(the unfixed loop times out), mutable `RiskView_` iterator instantiation and IVS
header self-containment. Fixes are confined to the checked entry and its required
header/iterator contracts; existing calibration and simulation kernels are unchanged.

First publication `d740a4c6` receives a Codacy complexity finding in the comprehensive
quote-oracle test: CCN 12 against the unchanged limit of eight. Extracting the
bucket/direction difference helpers reduces that test to eight; the complete core
and test file pass the original local threshold. OFF and combined focused suites
each pass 15 cases, with every one of the 42 oracle rows bitwise identical to its
pre-refactor trace. The rebuilt default production archive is also SHA-256 identical
to the measured installed archive. The sanitizer refactor rerun passes 15 cases,
with all 42 oracle rows identical. Exact corrective `e1ff0dd6` passes all 35 CI
checks. That acceptance precedes the public Hybrid implementation.

The passive snapshot owns fixed base stencil/ATM samples, carry, quote axes/values,
inclusion/completed grids and surface values. It retains no caller IVS pointer,
callback, active number or tape. Full content establishes parameter identity;
direct contributions require matching quote axes/values. Numeric validation occurs
before recording, replay checks the primal, boundary output aliases receive
additive seeds, and passive quote results are extracted before explicit close.
Curvature and variance errors reject rather than clip or floor.

## Open questions and remaining scope

- Exact `e1ff0dd6` CI accepts this core increment. Subsequent public source needs
  its own local review and exact publication-head CI.
- The subsequent Hybrid adapter has local full-chain/identity, diagnostic,
  installed-consumer and request-cost evidence in its
  [separate review](aad-dupire-hybrid-pullback.md). Its exact-head CI is independent
  of this accepted core increment.
- Python/Excel surfaces, common curve pullback adaptation and complete F01
  acceptance remain open.
- P01 production performance remains inconclusive. Core new-entry measurements
  and unchanged gate binaries cannot establish the missing production verdict.

## Tests

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- Final OFF complete non-benchmark CTest: 2,364 passes, including Python and examples;
  `aad-dupire-final-off-ctest-01.log`.
- Final combined lifetime/profiling non-benchmark CTest: 2,377 passes;
  `aad-dupire-final-combined-ctest-01.log`.
- Ten new Dupire and five interpolation contracts pass each normal configuration;
  `aad-dupire-name-snapshot-green-{off,combined}-01.log`.
- Fully instrumented ASan/UBSan core: 75 passes; public: 10 passes;
  `aad-dupire-name-snapshot-green-sanitized-01.log` and
  `aad-dupire-sanitized-public-01.log`. Leak detection is disabled; no leak claim.
- Installed consumers: two passes in each OFF/combined configuration;
  `aad-dupire-final-{off,combined}-installed-consumer-01.log`.
- Every quote bucket plus a direction for flat/Merton bases passes all three
  predeclared steps: 42 raw rows. Numeric snapshots agree bitwise with the old
  calibrator. Tests include negative/zero/boundary-alias seeds, direct addition,
  callback mutation/destruction, mismatches and success/failure/success recovery.
- CCN-eight threshold passes unchanged; `aad-dupire-complexity-02.log`.
- Test complexity RED/GREEN is retained in
  `aad-dupire-test-complexity-{red,green}-01.log`; focused OFF/combined reruns and
  exact oracle-row identities are in `aad-dupire-test-refactor-*` evidence.
- Final documentation integrity passes 92 Markdown files and generated-source
  verification has zero drift; `aad-dupire-docs-final-01.log` and
  `aad-dupire-generated-final-01.log`. New core and consumer formatting passes.
- Nine old gate executables remain bitwise identical;
  `aad-dupire-final-off-gate-identity-01.json`.
- Core opt-in measurement completes 240 processes with frozen source/artifact
  identities and bitwise references; [cost report](../performance/aad-dupire-entry-cost.md).

## Summary

The core increment and its test-only complexity correction are published as a
draft and accepted OFF/combined, under instrumented sanitizers and by all 35
exact `e1ff0dd6` CI checks. It is a required part of F01 and does not close the
remaining Hybrid/binding/curve requirements, Stage A or the full Stage B/C/D
objective.
