# F01 contracted-arithmetic replay review

Verdict: Approve. No unresolved correctness or API findings. Exact corrective
head `95f0706d7c906cbbbfe90ae633194134a25532cc` passes all 35 CI checks,
including ARM wheels; capture `aad-dupire-replay-ci-05.jsonl`.

## Findings

The `12dcd09a` Python publication finishes 35 checks with 33 successes, one
ARM wheel failure and one dependent wheel-matrix skip. Seven Python cases fail
at the original 1e-12 primal-surface check. Retained Clang FMA/on RED shows call
roundoff amplified by the discrete strike second difference, including a
2.37e-9 local-vol discrepancy. The focused nonaligned-axis regression first
fails with that existing diagnostic before production implementation.

Read the complete changed kernel/header, CMake fixture and test, active
specification/API/critique, methodology and retained numeric/cost evidence.
The shared five-call formula preserves the old stencil and call ordering.
Original agreeing replay remains in use. A mismatch builds a fresh scalar-call
primal with the old active expression's derivative, first requiring finite
prices and an explicit 8-epsilon discounted-leg bound. It never overrides a
surface with retained expected values. The final grid and relative/absolute
1e-12 surface checks remain before additive seeding and reverse. The discarded
original graph is unseeded. Frozen numeric data, alias copies, direct terms,
nesting rejection and recording recovery remain intact.

The first unconditional implementation passes numerical checks but regresses
all four measured VJPs by 72–98%. Reject it and retain its exact source, binaries
and 480-process trace. Conditional replay passes the unchanged paired protocol
over all twelve mode/case combinations. No numeric or performance assertion
is relaxed. Review the [cost report](../performance/aad-dupire-replay-rounding.md)
for the narrower measurement claim and unmeasured FMA recovery cost.

## Tests

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.

- ARM failure: `aad-dupire-bindings-arm-wheel-failure-01.log`; exact old-head
  final checks: `aad-dupire-bindings-ci-final-01.jsonl`.
- Local control RED: `aad-dupire-fma-clang-on-red-01.log`; actual focused unit
  RED: `aad-dupire-fma-oracle-red-01.log`.
- FMA fixture GREEN: 11 cases, including all fixed-step flat/Merton calibration
  oracles and three new nonaligned-axis rows;
  `aad-dupire-replay-clang-ctest-final-01.log`.
- Final OFF/combined CTest: 2,384/2,398 passes;
  `aad-dupire-replay-{off,combined}-full-final-01.log`. OFF includes 855 Python
  checks and 33 regular examples. The unchanged BS billion-path example passed
  in the accepted Hybrid increment; this correction does not rerun it.
- Fully instrumented GCC ASan/UBSan: 95 relevant cases, including public risk,
  Dupire, IVS, interpolation and native AAD;
  `aad-dupire-replay-sanitized-final-01.log`. Leak detection is disabled.
- Fully instrumented Clang FMA/on with lifetime and profiling enabled: 11
  cases pass, directly exercising conditional replay and its graph ownership;
  `aad-dupire-replay-clang-sanitized-02.log`. The first probe mistakenly supplies
  `ON` to the sanitizer-list option; retain that compile failure and configure
  the documented `address,undefined` list without changing production source.
- Both installed C++ consumers pass in OFF/combined final packages;
  `aad-dupire-replay-{off,combined}-consumer-02.log`.
- Standalone Python rebuilt against the final installed package: 62 passes;
  `aad-dupire-replay-installed-python-tests-02.log`. Both joint-install and
  standalone modules agree bitwise on all 248 cells against the unchanged
  accepted C++ control; `aad-dupire-replay-{installed,standalone}-parity-02.json`.
- All previous 42 core, 321 Hybrid and 84 Python oracle rows are identical;
  `aad-dupire-replay-oracle-identity-02.json`. Initial diagnostic scripts use an
  unsupported matrix iterator and the earlier 300-row Hybrid trace; correct
  the helpers to `to_rows()` and the complete 321-row accepted reference.
  Retain those failed diagnostic logs; production assertions remain unchanged.
- Final CCN-eight/format pass. Generation has zero drift in 497 files; restore
  only 16 identical output timestamps after checking hashes.
- Existing nine OFF gate binaries remain identical. Final paired costs pass
  twelve mode/case combinations over 480 processes with all numeric/hash
  checks; the failed first 480 processes remain separate evidence.

## Open questions and limits

The combined-diagnostic FMA fixture and exact corrective-head CI pass.
This acceptance applies to the correction at `95f0706d`. P01,
Excel/common curve integration, the remaining Stage B/C/D work and final audit
remain open; neither full F01 nor the overall objective is complete.
