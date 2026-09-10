# DAL-193 B1 implementation

B1 now propagates joint quote risk through the actual consumed curve/base graph, including unregistered XCCY forecast roots. This record covers implementation and local verification; the orchestrator owns the subsequent tester, reviewer, doc-writer, and final CI/merge stages. DAL-193 remains `in_progress`.

## Revisions and approved scope

- Baseline: `1abb3d4b45222f3ce825bec8249f6da938f58104` (master / #344).
- Branch: `agent/dal-implementer/9c96ec6e82b7`.
- RED: `a4bd9a41445e9d9ef22f2131d3022893a4f59a62`.
- Initial GREEN: `060a411734fb5925e3dad685e4c1154ead5f480d`.
- Source protection and surface verification: `129a69f8c1ad95a98543c5a7d86f0494d73f503a`.
- Initial verified code head: `9a847361081ebd3b5450cb9273a1c7597f369e10`.
- First draft head: `fb01602a99dbe694ad68b1ea9eaf97d072eb4fd4`, adding this record only.
- The CI repair below updates the same draft PR; its exact published head and single post-push CI snapshot are recorded in the follow-up evidence archive and Multica handoff comment.
- The subsequent review repair and master integration are separate commits, mapped below. The final record-only head and its single CI snapshot are included in the review-repair handoff.

Controlling inputs were the approved B1 specification, API note, and `Proceed with caveats` critique attached to DAL-193. Their attachment IDs are respectively `01a08bfd-6b19-74bf-afcf-6a80c73d5dd4`, `01a08c0e-1962-74dc-98bf-c139a7159b26`, and `01a08c1b-9290-7c4b-a8e5-68b2e4b777f8`. Q1 and B2 remain excluded.

## Implementation

The only production source changes are in `dal-cpp/dal/curve/ratecashflowpricing.cpp` and its private `ratecashflowpricing_internal.hpp`.

- Joint consumption uses a per-trade closure of actual selected roots and base handles. Path-local identity checks distinguish cycles from shared bases. Exact PWC, PWLF, LogDF, and ZeroRate types are supported; an unknown necessary node makes every potentially affected bound block incomplete.
- Preparation includes consumed paths to the target and the constant roots needed by the XCCY typed view. Intermediate parameters remain constant; the target's own base remains passive in that target's sweep. Aliases do not create extra preparations or sweeps. Unused registered descendants do not enter preparation.
- Joint forward preparation selects an auxiliary anchor before the earliest knot when needed. Rebuilding retains the original PWC/PWLF knots and left/right values, including historical knots; standalone preparation is unchanged.
- Existing family, routing, root-classification, passive-validation, and expired-XCCY gates retain their order. New unknown-graph failures use `AAD_EVALUATION_FAILED`, wrapped by `QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE` at the aggregate boundary.
- The aggregate validates a completed native slice against the bound block width and finite-value requirements before copying it. A failed block discards the whole local trade/provenance gradient. A successfully obtained passive PV is still counted once, and healthy sibling trades continue.
- Invalid v2 sources are checked against actual XCCY root/base paths before passive pricing, even when no provenance remains active. This closes the independently reproduced unsafe unregistered-path evaluation. Pure v1 malformed sources still raise their existing call-level exception; only v2 produces the `INVALID` source marker.
- Two private test hooks distinguish an exception while recorded AAD nodes are live from failures after a completed sweep but before aggregate slice acceptance. The pre-existing early forced-failure hook and its token are retained.

No public signature, result field, token, constructor default, provenance factory validation, or fingerprint encoding changed. The implementation deliberately changes joint eligibility for some custom curve inputs, as detailed below.

## C1–C4 compatibility evidence

All four old-behavior characterizations passed against the unmodified baseline before their assertions were changed for the approved contract. The exact characterization source and log accompany this record.

| Case                                               | Observed baseline behavior                                                                                            | Implemented joint behavior                                                                                           |
|----------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------|
| C1: builtin EUR forecast with opaque unit leaf     | Factory/aggregate accepted; PV and all five registered-control risks were identical to the equivalent builtin market. | Factory still accepts; one incomplete meta at the first declared bound key, original reason `AAD_EVALUATION_FAILED`. |
| C2: opaque forwarding layer hiding H0              | Factory/aggregate accepted, but the tested quote derivative omitted more than one million units of base response.     | Incomplete meta at the first declared bound key; no partial direct risk.                                             |
| C3: empty PWC-derived EUR forecast                 | Factory/aggregate accepted; PV and all five risks equaled the builtin control.                                        | Exact-type joint gate rejects the necessary derived node with `AAD_EVALUATION_FAILED`.                               |
| C4: Future consuming only an opaque root hiding H0 | Eligible structural zero with five zero buckets.                                                                      | Original root representation gate now runs; incomplete meta with `CURVE_REPRESENTATION_NOT_AAD_ENABLED`.             |

C1 and C3 are intentional restrictions of previously numerically correct custom inputs. Compatibility must not be described as merely rejecting previously incorrect inputs. Likewise, prior success of arbitrary non-default `B_` template instances is not established: linking and serialization constraints apply, as the critique notes. The new exact-type graph gate does not add support for those instances.

## RED and GREEN evidence

The RED commit contains five normal CMake-discovered tests: the original unregistered-XCCY defect and C1–C4's approved outcomes. Against baseline, all five failed for the expected reasons. The initial GREEN passed all 22 then-current `JointQuoteRiskTest` cases.

```bash
cmake --build build/Release-linux --target dal_cpp_tests --parallel 4
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='JointQuoteRiskTest.TestUnregisteredXccyBaseMatchesFullRecalibration:JointQuoteRiskTest.TestOpaque*Closed:JointQuoteRiskTest.TestDerivedBuiltinFailsClosed:JointQuoteRiskTest.TestOpaqueOnlyRootKeepsRepresentationGate'
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='JointQuoteRiskTest.*'
```

For the original `curve:0:1` bucket, baseline returned `31228.07347268159` versus full recalibration `-1663168.021141246` dPV/dDecimalQuote. The repaired result is `-1663168.021048957`; DV01 is `-166.3168021048957`. The ±1 bp recalibration oracle is `-166.31680834041799`. Passive EUR PV remains `-31068.298577158537`.

An additional RED case showed one attempted pricing call through a cyclic source reachable only through the unregistered XCCY path. After the protection change the attempted-call count is zero, with one `INVALID` provenance failure, no trade meta, no buckets, and no sweep. Its RED/GREEN logs are included.

## Initial local validation

```bash
NUM_CORES=4 ADDITIONAL_CMAKE_FLAGS='-DDAL_BUILD_EXCEL_PORTABLE_TESTS=ON' bash ./build_linux.sh --python 3.12
cmake --build build/Release-linux --target dal_check_generated --parallel 4
python3 .github/scripts/check_docs.py
python3 -m unittest discover -s .github/scripts/tests -v
```

- Full build/install/CTest: **1552/1552 passed**, including **291 Python tests**, core/public C++, and portable Excel.
- Generated-source check passed without drift; documentation checks passed for 51 Markdown files.
- CI helper suite: 146 tests, successful with 6 environment-dependent skips.
- Changed production translation unit passed `-Wall -Wextra -Wpedantic -Werror` syntax validation using the repository's existing warning-category exclusions. This is a translation-unit check, not a separate full warning-clean build.
- The original oracle validator accepted **992 buckets across 96 fixtures** at the verified code head. The independent B1 validator accepted **80/80** raw observation rows across analytic/bumped, PWC/PWLF, layered/unlayered, and registered/unregistered controls; derivative, ±1 bp DV01, unit identity, and registration/PV invariance all pass.
- Additional tests cover mixed unregistered LogDF/ZeroRate/PWLF chains, historical PWC/PWLF knots, single-currency base-only consumption, standalone fixed-base behavior, aliases, unused descendants, reversed trade order and separate closures, declaration-order failure keys, stale sources, family/root/fixing/expiry gates, and healthy siblings.
- Real completed second-block exception/NaN/width failures discard the first block's slice. A separate exception with live recorded nodes proves tape rewind; the same provenance then produces the original B1 result. Concurrent successful callers retain independent tapes.
- Public C++ and ordinary Python constructors both execute B1 against recalibration. A test-only native Python extension injects the custom opaque fixture; it is not installed or exported by the production package. Python metadata and the portable Excel ten-column failure spill preserve existing fields, positions, and unavailable/INCOMPLETE tokens.

The Python finite-difference fixture uses tighter solve/fit tolerances than the existing cross-language fixture so its strict absolute derivative assertion is reproducible. The existing golden-reference fixture and assertions were retained.

## Performance validation

Baseline and head use detached `dal193-perf/base-source` / `head-source` checkouts and separate `base-build` / `head-build` directories. Both use identical pinned dependencies, GCC 15.2.0, CMake 4.2.3, Unix Makefiles, Release static native AADet, native CPU tuning, and `DAL_NUM_THREADS=4` on the same i9-13900HX WSL2 host. WSL timing can be noisy; the repository's paired minimum reduction is used.

```bash
DAL_NUM_THREADS=4 python3 .github/scripts/check_benchmark_regressions.py \
  --base-root ../dal193-perf/base-build --head-root ../dal193-perf/head-build \
  --output-dir ../dal193-perf/paired-final --samples 10 \
  --confirmation-rounds 2 --threshold-percent 4
```

Final verdict: **no regression under the repository gate**. All **9 targets / 51 cases** passed, with 20 process samples per side. Every raw process output, per-case minimum, both round deltas, and gate decision is retained in `paired-final/results.json` and `paired-final/summary.md` in the evidence archive. An isolated round above 4% is not a failure under the required two-round rule.

### Paired benchmark results

2 independent rounds of 10 interleaved process-level samples; failure requires every round to exceed +4.00%.

| Benchmark      | Case                                                       | Base min      | Head min      | Change | Round changes  | Result |
|----------------|------------------------------------------------------------|---------------|---------------|--------|----------------|--------|
| tape_perf      | Clear + re-record (100K nodes)                             | 0.900797 ms   | 0.906077 ms   | +0.59% | +2.80%, -0.64% | pass   |
| tape_perf      | PropagateToStart (100K nodes)                              | 0.424813 ms   | 0.431848 ms   | +1.66% | +1.66%, +1.99% | pass   |
| tape_perf      | PropagateToStart multi-mode (100K nodes, 10 results)       | 0.556076 ms   | 0.553092 ms   | -0.54% | -0.72%, +1.07% | pass   |
| tape_perf      | Rewind + re-record (100K nodes)                            | 0.582877 ms   | 0.583456 ms   | +0.10% | +0.10%, +0.63% | pass   |
| tape_perf      | ZeroAdjoints sweep (100K nodes)                            | 0.118066 ms   | 0.120115 ms   | +1.74% | +1.74%, -0.03% | pass   |
| jacobian_perf  | AnalyticJacobian dense harvest (24 x 23)                   | 0.006381 ms   | 0.006209 ms   | -2.70% | -0.75%, -2.70% | pass   |
| jacobian_perf  | AnalyticJacobian row-width harvest (24 x 23)               | 0.006197 ms   | 0.006225 ms   | +0.45% | -2.64%, +0.45% | pass   |
| pde_perf       | ThetaScheme rollback (200x200 CN)                          | 0.184057 ms   | 0.192450 ms   | +4.56% | +4.56%, +2.03% | pass   |
| pde_perf       | ThetaScheme rollback (200x200 implicit)                    | 0.184228 ms   | 0.187336 ms   | +1.69% | +1.69%, +1.79% | pass   |
| pde_perf       | ThetaScheme rollback (200x2000 explicit)                   | 0.767762 ms   | 0.772620 ms   | +0.63% | +2.26%, -0.87% | pass   |
| rng_perf       | BrownianBridge FillNormal (100K x 10D)                     | 7.036000 ms   | 7.152000 ms   | +1.65% | +0.77%, +1.65% | pass   |
| rng_perf       | MRG32k3a FillNormal (100K x 10D)                           | 18.920000 ms  | 18.638000 ms  | -1.49% | +2.78%, -1.49% | pass   |
| rng_perf       | ShuffledIRN FillNormal (100K x 10D)                        | 10.911000 ms  | 10.962000 ms  | +0.47% | +0.10%, +0.47% | pass   |
| rng_perf       | Sobol FillNormal fast (100K x 10D)                         | 4.485000 ms   | 4.406000 ms   | -1.76% | +0.44%, -1.76% | pass   |
| rng_perf       | Sobol FillNormal precise opt-in (100K x 10D)               | 37.453000 ms  | 37.481000 ms  | +0.07% | -0.56%, +0.07% | pass   |
| rng_perf       | Sobol FillUniform (100K x 10D)                             | 0.990695 ms   | 1.024000 ms   | +3.36% | -0.38%, +3.36% | pass   |
| interp_perf    | Cubic interp (50 knots, 10K queries)                       | 0.050187 ms   | 0.050353 ms   | +0.33% | +3.54%, +0.33% | pass   |
| interp_perf    | Inlined linear LV-style (1e5 paths x 200 steps, 200 knots) | 100.583000 ms | 100.274000 ms | -0.31% | +0.49%, -0.31% | pass   |
| interp_perf    | Linear interp (50 knots, 10K queries)                      | 0.042217 ms   | 0.042410 ms   | +0.46% | +3.26%, +0.16% | pass   |
| krylov_perf    | BCGSolve (500x500 tridiag)                                 | 0.017723 ms   | 0.017755 ms   | +0.18% | -0.12%, +0.42% | pass   |
| krylov_perf    | CGSolve (500x500 tridiag)                                  | 0.014535 ms   | 0.014500 ms   | -0.24% | -0.33%, -0.03% | pass   |
| banded_perf    | TriDecomp MultiplyLeft (10K)                               | 0.004224 ms   | 0.004221 ms   | -0.07% | -0.07%, -0.07% | pass   |
| banded_perf    | TriDiagonal Decompose (10K)                                | 0.075419 ms   | 0.075270 ms   | -0.20% | +0.04%, -0.20% | pass   |
| banded_perf    | TriDiagonal MultiplyLeft (10K)                             | 0.004203 ms   | 0.004204 ms   | +0.02% | +0.02%, -0.05% | pass   |
| cholesky_perf  | CholeskyDecompose (200x200)                                | 0.356988 ms   | 0.354999 ms   | -0.56% | +0.01%, -0.65% | pass   |
| cholesky_perf  | CholeskyDecompose+Multiply (200x200)                       | 0.359896 ms   | 0.359680 ms   | -0.06% | -0.11%, -0.06% | pass   |
| rate_risk_perf | Quote risk aggregate (joint XCCY)                          | 0.222103 ms   | 0.220635 ms   | -0.66% | -0.18%, -0.85% | pass   |
| rate_risk_perf | Quote risk aggregate (single curve)                        | 0.008200 ms   | 0.008274 ms   | +0.90% | +0.90%, +0.89% | pass   |
| rate_risk_perf | Quote risk aggregate (staged XCCY basis)                   | 0.064616 ms   | 0.064170 ms   | -0.69% | -1.01%, -0.54% | pass   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=10)                  | 2.426000 ms   | 2.389000 ms   | -1.53% | -1.65%, -1.20% | pass   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=16)                  | 3.989000 ms   | 3.960000 ms   | -0.73% | -1.35%, -0.55% | pass   |
| rate_risk_perf | Quote risk generic joint (100 IRS x N=5)                   | 1.633000 ms   | 1.649000 ms   | +0.98% | +1.29%, +0.49% | pass   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=10)                 | 24.288000 ms  | 23.839000 ms  | -1.85% | -2.15%, -1.85% | pass   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=16)                 | 40.115000 ms  | 39.806000 ms  | -0.77% | -0.58%, -1.10% | pass   |
| rate_risk_perf | Quote risk generic joint (1000 IRS x N=5)                  | 16.329000 ms  | 16.265000 ms  | -0.39% | -0.39%, -1.15% | pass   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=10)   | 2.364000 ms   | 2.340000 ms   | -1.02% | -1.02%, -1.26% | pass   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=16)   | 3.950000 ms   | 3.909000 ms   | -1.04% | -0.86%, -1.14% | pass   |
| rate_risk_perf | Quote risk generic joint node reference (100 IRS x N=5)    | 1.574000 ms   | 1.589000 ms   | +0.95% | +0.95%, +0.06% | pass   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=10)  | 23.757000 ms  | 23.618000 ms  | -0.59% | -0.56%, -0.59% | pass   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=16)  | 40.076000 ms  | 39.557000 ms  | -1.30% | -1.53%, -1.23% | pass   |
| rate_risk_perf | Quote risk generic joint node reference (1000 IRS x N=5)   | 16.100000 ms  | 16.069000 ms  | -0.19% | -0.21%, -0.19% | pass   |
| rate_risk_perf | Quote risk portfolio joint ANALYTIC (24 XCCY x N=10/block) | 5.107000 ms   | 5.082000 ms   | -0.49% | -0.88%, -0.49% | pass   |
| rate_risk_perf | Quote risk portfolio joint BUMPED (24 XCCY x N=10/block)   | 5.115000 ms   | 5.084000 ms   | -0.61% | -0.25%, -0.74% | pass   |
| rate_risk_perf | Quote risk portfolio single ANALYTIC (120 deposits x N=5)  | 0.117801 ms   | 0.118710 ms   | +0.77% | +0.31%, +0.96% | pass   |
| rate_risk_perf | Quote risk portfolio single BUMPED (120 deposits x N=16)   | 0.165294 ms   | 0.167775 ms   | +1.50% | +1.27%, +1.52% | pass   |
| rate_risk_perf | Quote risk portfolio staged ANALYTIC (24 XCCY x N=16)      | 4.258000 ms   | 4.218000 ms   | -0.94% | -1.01%, -0.94% | pass   |
| rate_risk_perf | Quote risk portfolio staged BUMPED (24 XCCY x N=5)         | 1.494000 ms   | 1.480000 ms   | -0.94% | -1.20%, -0.40% | pass   |
| rate_risk_perf | Rate OIS daily compounding sweep (5Y quarterly x daily)    | 0.247663 ms   | 0.247254 ms   | -0.17% | -0.29%, +2.45% | pass   |
| rate_risk_perf | Rate XCCY batch serial (24 XCCY x 5 components)            | 0.853243 ms   | 0.847706 ms   | -0.65% | -0.67%, -0.59% | pass   |
| rate_risk_perf | Rate batch serial (120 IRS x 2 components)                 | 3.254000 ms   | 3.230000 ms   | -0.74% | -0.71%, -1.19% | pass   |
| rate_risk_perf | Rate single-trade sweeps (240 IRS calls)                   | 4.307000 ms   | 4.274000 ms   | -0.77% | -0.77%, -1.29% | pass   |

Head Sobol precise opt-in / fast ratio: 8.51x (limit 10.00x).

All performance acceptance checks passed.

The head `rate_risk_perf` smoke run passed the separate generic-joint aggregate-versus-complete-native-reference 20% ceiling for N=5/10/16 and 100/1000 IRS. Observed overheads were respectively 3.953%/0.851%, 1.302%/0.710%, and 1.868%/-1.562%, with zero recalibrations, one provenance preparation, two consumed native preparations, and 200/2000 sweeps.

The benchmark's previous exact preparation count of three included the unused registered third forward curve. Its assertion now requires exactly two, matching the approved consumed-only preparation rule and a dedicated unit test. Numerical reference, workload, widths, quote transform, sweep bound, and timing thresholds are retained. The initial measurement stopped at that stale count assertion; its logs are retained separately and do not constitute the final performance verdict.

The comparable benchmark portfolios already have complete economically relevant base paths on baseline. The old B1 missing-path calculation was not used as an equivalent-work performance baseline. The changed hot path maps to the existing `rate_risk_perf` target; no new benchmark target is introduced.

## CI repair of the first draft

The orchestrator returned `fb01602a99dbe694ad68b1ea9eaf97d072eb4fd4` to implementation for MSVC compilation and three new Codacy complexity findings. This follow-up changes only `dal-cpp/dal/curve/ratecashflowpricing.cpp`, `dal-cpp/tests/curve/test_joint_quote_risk_graph.cpp`, and this record.

The downloaded failed `build (msvc, xad)` log from run `34507960051`, job `102974740264`, confirms C2065 on `typeid(curve)` inside the generic visitor and the following C2338 uniform-return diagnostic from `std::visit`. The log identifies MSVC 14.51.36231. The triggering comment reports the same compile failure in all three Windows builds and Windows Benchmarks; compilation failed before benchmark measurements.

`IsExactJointCurve` now evaluates dynamic RTTI outside the generic lambda, captures the resulting `std::type_info` reference explicitly, and declares the visitor return type as `bool`. Each non-monostate alternative still compares that dynamic RTTI with its exact static curve type; opaque and derived curves keep the approved rejection behavior.

The complexity repair extracts the consumed path preparation and XCCY preparation loops into private helpers. A consumed path stops at the addressed target; a constant XCCY root stops at itself, preserving the previous preparation set, order, cache reuse, and failure handling. The oracle test moves only its per-bucket recalibration assertions into a helper and calls it through `ASSERT_NO_FATAL_FAILURE`, preserving fail-fast propagation. All original assertions, tolerances, test cases, and mode/layout/layer/alias combinations remain present.

Lizard 1.23.0 reproduces the three Codacy counts before repair and reports the following after repair, using the unchanged limit of 8:

| Function                                                       | Before | After |
|----------------------------------------------------------------|--------|-------|
| `JointPreparations`                                            | 9      | 6     |
| `HoistXccy`                                                    | 10     | 8     |
| `TestUnregisteredXccyOracleAcrossModesLayoutsLayersAndAliases` | 10     | 6     |
| `PrepareJointPath`                                             | new    | 5     |
| `PrepareXccyConsumedCurves`                                    | new    | 3     |
| `AssertXccyOracleBuckets`                                      | new    | 5     |

The RED evidence for this repair is the actual published-head MSVC failure and the reproduced static threshold violations. The native GCC baseline already passed all 32 joint tests. The minimum portability change then passed a targeted native build and the same 32 tests before the complexity extraction; no artificial behavioral failure was introduced for this compiler-only repair.

Final directed verification:

```bash
cmake --build build/Release-linux --target dal_cpp_tests --parallel 4
build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='JointQuoteRiskTest.*:RateCashflowPricingTest.*:QuoteRiskAggregationTest.*'
```

- Native Release target build passed; the final directed regression filter passed **144/144** tests (32 joint, 87 pricing, 25 quote aggregation), including exact-type C1–C4, historical knots, aliases/preparation counts, invalid source protection, tape rollback, and standalone XCCY behavior.
- Both changed C++ translation units passed GCC `-Wall -Wextra -Wpedantic -Werror` syntax checks with exactly the repository CI's existing warning-category exclusions.
- The production translation unit also passed Clang 21 syntax validation with `-fdelayed-template-parsing -fms-extensions`. This uses Linux headers and is supplementary portability evidence; it is not a real MSVC build result.
- The static comparison confirms that all original assertion expressions and test case names remain; only the helper failure-propagation assertion was added. Patch whitespace checks passed. No threshold, gate configuration, public contract, numeric assertion, or excluded Q1/B2 scope changed.

Per the explicit follow-up instruction, this turn runs directed build/regression/static checks only. The initial 1552-test full native validation, Python/public/Excel results, independent oracle files, generated-source/docs/helper checks, and nine-target performance evidence above remain reusable prior-head evidence, not new-head full-suite claims. Actual MSVC builds, Windows Benchmarks, hosted Codacy, wheels, the full backend matrix, and final-head complete local/CI acceptance remain for the subsequent pipeline. One CI snapshot will be read after pushing this repair; this implementation phase does not wait for CI completion or merge.

## Review repair of `908b2f0`

Controlling review: attachment `01a08c86-2f03-78ab-9865-a9a71d42869f`, verdict `Request Changes` on `908b2f0ce8f7de7f04091563f6720feb76d572fd`. The tester had completed full native validation and both oracle audits on that head (attachment `01a08c7a-d5e2-77ff-a8b9-46341edd4cc2`); those results did not cover the mixed-source counterexample or the A3/F05 combinations. This follow-up addresses the three findings locally and awaits independent tester/reviewer acceptance.

### P1: mixed v1/joint source order

The new `TestXccyProvenanceOrderPreservesStandaloneAndJointResults` was first built and run with unchanged `908b2f0` production code. RED: one expected failure, with v1 ineligible and `AAD_EVALUATION_FAILED` for `[joint, single]`; `[single, joint]` and each independent source were successful. The test compares every meta field, every bucket field and numerical value, PV totals, and the single passive-pricing count against separate calls.

The minimal fix keys XCCY hoists by both trade identity and coordinate mode, using two private maps. Standalone preparation and joint lazy/historical preparation remain separate; passive pricing stays shared per trade. GREEN: the same regression and three adjacent historical/standalone XCCY controls passed. No test assertion was weakened.

### P2: A3/S03 native-coordinate oracle

Two new tests rebuild `PWLF extra -> ZeroRate -> LogDF -> PWC middle -> H0/H1`, with historical knots and fixed extra-layer parameters. Both PWC/PWLF calibrated layouts and layered/unlayered forward declarations are covered, giving eight topology configurations. For every consumed native coordinate, the test clones passive curves, applies only that coordinate's native `ApplyDX` shift, rebuilds the full dependent chain, and compares the central PV difference with `JointNodeSensitivitiesBatch` using an absolute `1e-3` derivative tolerance. It performs no recalibration or quote-Jacobian projection in this oracle.

The checks also compare all unchanged native parameters, assert the target forward's original base handle remains fixed when its own parameters are shifted, require active/passive PV agreement within `1e-8`, verify native widths, and verify the unused H1 slice and original five quote buckets in the H0-only topology. The four extra-layer representations are exact builtin types; their own parameters and geometry are reconstructed from unchanged fixture values. Both tests passed.

### P2: F05 preparation failures

The necessary private seam runs only after a joint preparation cache miss, inside the existing exception/failure-cache boundary. It throws for selected exact PWC intermediate handles; it does not reuse the early sweep or opaque-graph seams.

The new test covers 12 contexts: H0-only failure, H1-only failure, and simultaneous failures, each with two deterministic relative-address orders and two extra-key orders. An owning array permits reversing which independent base path occupies the lower address, verified with `std::less`; this is not an allocator-order assumption. Bound keys deliberately have reverse lexical order (`z-discount`, `a-forward`). Each individual path is exercised before the dual-failure case. Single failures produce the corresponding complete G(K0)/G(K1); simultaneous failures always produce G(K0).

For each context the test checks no failed-trade buckets, complete failure metadata, retained passive PV, and exact healthy-trade buckets. A repeated failed trade in the same aggregation triggers the failed-preparation cache: the affected preparation hook is attempted only once. H1-only failure proves an earlier H0 sweep occurred and was discarded. Healthy contributions and their sweeps remain intact; a later call without the seam reproduces the original successful result. All contexts passed.

### Commit mapping and integration

- P1 RED test: `78ed201d1dd839096ec45ae9ae5e6ef4c210a344`.
- P1 minimum production fix / focused GREEN: `e356c0d784e6df03b8c317192ffe09702fef2cf5`.
- A3/S03 direct tests / GREEN: `786bef6922eda84084400ae6c133859bbd973030`.
- F05 private seam and direct tests / GREEN: `342dfa860ad1f9c8d0743393e1af199f04c419bf`.
- Separate master merge: `603a2195700dae86386f7b02729d95c7338d2f1d`, parents `342dfa860ad1f9c8d0743393e1af199f04c419bf` and `3ee049462612611cf9b6563e69bf1ecc8bc68db9` (#345). The merge had no conflicts and did not change the repair's three C++ files.
- The following delivery commit updates this record only. Its full head is recorded in the attachment manifest and issue comment.

The self-authored source scope is the B1 hoist cache, its private preparation seam, and `dal-cpp/tests/curve/test_joint_quote_risk_preparation.cpp`. The separate merge imports #345's Python benchmarks and paired CI comparison unchanged. Linux Benchmarks now includes `check_python_benchmark_regressions.py` with 10 samples, two confirmation rounds, and the unchanged 4% threshold. No gate, public signature, numerical/compatibility contract, or Q1/B2 exclusion was changed.

Integrated directed verification on `603a2195700dae86386f7b02729d95c7338d2f1d`:

```bash
cmake --build build/Release-linux --target dal_cpp_tests --parallel 4
build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='JointQuoteRiskTest.*:RateCashflowPricingTest.*:QuoteRiskAggregationTest.*' \
  --gtest_output=xml:build/dal193-review-regressions.xml
```

Result: **148/148 passed** (36 joint, 87 pricing, 25 quote aggregation). Both affected C++ translation units passed GCC warning checks with the existing CI exclusions and Clang delayed-template syntax checks. Lizard reports all newly added functions/tests at complexity 6 or less; the existing `HoistXccy` remains 8 and `PreparationFor` remains at its pre-repair value of 9. Patch checks pass.

As instructed, no full local suite, Python runtime suite, or paired performance gate was rerun in this turn. Prior complete results remain attributed to their prior heads. Final-head complete local/CI validation, the newly integrated Python paired gate, independent review, documentation, and approval/branch-protection acceptance remain required. The follow-up evidence archive contains the RED/GREEN logs, integrated XML and compiler/static evidence, separate repair/integration deltas, commit mapping, and exactly one post-push CI snapshot. DAL-193 stays `in_progress`; PR #346 remains unmerged.

## Handoff

The initial evidence archive contains RED/GREEN and full-test logs, the baseline C1–C4 characterization source, both final oracle CSVs and validators/manifests, generated/docs/helper/warning results, and complete benchmark environment/raw/summary results. The CI repair archive adds the actual MSVC failure log, directed build/regression/static evidence, the source delta, and a single post-push CI snapshot. Source changes are committed; generated logs and the earlier investigation materials are excluded from Git.

Local verification covers native AADet on Linux/WSL2 and portable Excel. Windows XLL and the alternative AAD backend CI matrix have not been run locally. The orchestrator will arrange tester → reviewer → doc-writer and the final published-head CI/merge acceptance. Documentation review should explicitly preserve the C1/C3 compatibility qualification and the restriction to actual consumed exact builtin graphs.
