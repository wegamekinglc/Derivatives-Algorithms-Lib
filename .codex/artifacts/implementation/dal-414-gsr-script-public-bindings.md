# DAL-414: GSR instrument public bindings

## Controlling state and design

Started from clean `acadd8ee44fdb6a0bb2e70de9d559db59de066f8`, merged
DAL-407/PR621 and DAL-413/PR622. The current DAL-414 description and parent
DAL-396 authorize this work. DAL-398 sections 6–9 control the new signatures,
required valuation and fixed report schema; DAL-412 and DAL-413 retain their
accepted pricing, derivative, stream and numerical acceptance semantics.

The facade must select existing core drivers and own all inputs before a
Python GIL release. It must not construct a second product, invent sampling
defaults, modify the shared protocol or import unmerged XCCY work. The
explicitly authorized independent core change keeps the script driver's
result alive across failure and publishes actual counters and completed
finite summary metrics through `CalibrationReportSnapshot_`, without further evaluation.
Failed reports retain the original diagnostic, error axes and last complete
candidate. Unavailable metrics stay absent.

## Contract gap requiring coordination

The frozen `GSRInstrumentCalibrationSettings_` in DAL-398 section 6 contains
`solver_: GSRCalibrationSettings_`, `valuation_`, `evaluation_` and optional
`validationSampling_`. Its four-argument facade has no held-out argument.
Actual merged types do not supply an unambiguous mapping to DAL-412:

- `dal-cpp/dal/script/calibrationpreparation.hpp`:
  `CalibrationEvaluationOptions_::sampling_` is
  `optional<CalibrationSamplingSpec_>` with only `paths_`, `seed_`,
  `generator_`. It cannot express fit batches, paths per batch or a stream
  version. Its Python/Excel constructors faithfully expose those three fields.
- `dal-public/src/calibrationinstrument.hpp`:
  `CalibrationMonteCarloSampling_` has mode, paths per batch, batches, seed and
  stream version, but is currently a different type used by MC repricing.
- `dal-cpp/dal/model/gsrscriptcalibration.hpp`:
  `GSRScriptCalibrationSettings_` requires separate fit/validation counts and
  a maximum reserved path budget, plus basket budget and precision floors;
  held-out observations are a separate fifth argument with independent counts.
  Neither the frozen public settings nor signature supplies those observations.

No product mapping is implemented until coordination resolves this gap, as the
launch instruction explicitly freezes existing shared types and requires a
concrete proposal before extending their schema.

Minimal proposed additive mapping for coordination (not a shipped API): keep
the four required facade arguments and the specified solver/valuation/evaluation
fields. Declare the new domain settings as follows:

```cpp
struct GSRInstrumentCalibrationSettings_ {
    GSRCalibrationSettings_ solver_;
    Script::ScriptValuationSettings_ valuation_;
    CalibrationEvaluationOptions_ evaluation_;
    std::optional<CalibrationMonteCarloSampling_> fitSampling_, validationSampling_, heldOutSampling_;
    Vector_<Handle_<CalibrationObservation_>> heldOutObservations_;
    int maxPathsPerView_ = 0, maxBasketEvaluations_ = 1000;
    double validationSigma_ = 3., epsilonValue_ = 1e-10, epsilonGradient_ = 1e-8;
    bool compiled_ = Script::DEFAULT_COMPILED;
};
```

The three sampling fields reuse the already public batching type. ScriptMC
requires explicit fit/validation settings and a positive reserved path budget;
held-out observations additionally require their independent sampling. Seed,
mode and stream version must agree, and the current core retains its common
draw-dimension checks. Existing `evaluation.sampling`, when supplied, must
agree with the fit seed/generator/total paths; it cannot set another batch
protocol. Fast fitting requires no MC fields and rejects unsupported choices
rather than ignoring them. No shared type or report schema changes are proposed.
The fixed kernel remains hard payoff, MRG32/Precise and no Brownian bridge.
Public Auto would resolve to native FD for the existing BondOption fast driver
or native AAD for explicit ScriptMC; explicitly unsupported derivatives fail.
The existing `validationSampling_` slot gets the shared batching type, rather
than another independently defined value. The additional new-domain fields
and this mapping require acceptance under the launch's freeze instruction;
there is no safe implicit split of the existing `paths_` into batches.

## Implemented independent report change

Only `dal-cpp/dal/model/gsrscriptcalibration.cpp` and its existing dedicated test
file have production/test edits. `WorkSnapshot` publishes the current driver's
basket and Jacobian invocation counts and iteration count in existing summary
fields. A Jacobian invocation that reaches its basket budget is counted as
attempted, consistently with `costs_.jacobians_`; it is not claimed completed.
The result now outlives the catch. A marker after successful `Finalize` permits
publishing completed fit/convergence/numerics/objective/rank metrics even when
held-out evaluation subsequently fails. The report retains the failing row's
held-out axes; fit observations are not substituted for them.

Finite zero metrics are preserved. Infinite rank-condition estimates become
unavailable, and invalid input does not produce invented objective/acceptance
values or replace the primary diagnostic. An unfinished solve publishes actual
counts but no completed fit metrics. `Succeeded` requires convergence, fit and
numerics plus requested held-out acceptance; other returned calibrations are
`Candidate`, and execution errors remain `Failed`. Last complete candidate,
exception cause and tape cleanup behavior are unchanged.

This is a partial foundation for DAL-414. Detailed facade observations, selected
g rows, plans, execution, independent fit/validation SE and raw Jacobian assembly
remain with the public adapter once its input mapping is accepted. No new report
schema, matrix placeholder, pricing/derivative/RNG/acceptance change, public
entry point, Python/Excel binding or example is included. Existing public docs
and CHANGELOG receive no feature claim for this unfinished interface.

## RED, GREEN and verification

Commands use a fresh Release workspace at `build/Release-linux`, GCC 15.2.0,
CMake 4.2.3, native AADET, static core, Eigen, native-arch OFF. Public is configured
but public tests, Python, portable Excel, examples and benchmarks are OFF. All
five pinned top-level submodules were initialized without pointer changes.

`cmake --preset Release-linux -S . -B build/Release-linux
-DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_CPP_BUILD_BENCHMARKS=OFF
-DDAL_BUILD_EXCEL_PORTABLE_TESTS=OFF -DDAL_PUBLIC_BUILD_TESTS=OFF
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` succeeds. An initial premature build before
configure completed reported a missing cache; the subsequent configured baseline
build succeeded. This setup failure is separate from behavioral RED.

The attached `verify.py` compiles the dedicated test and canonical test main
using their exact compile-database arguments, and links the fresh canonical
`libdal_cpp.a` with the built `libgtest.a`. This is a scoped runner; it is not
execution of the full `dal_cpp_tests` target.

1. `cmake --build build/Release-linux --target dal_cpp -j 4` builds the baseline.
   `python3 ../dal414-evidence/verify.py compile`, then
   `python3 ../dal414-evidence/verify.py test red
   GSRScriptCalibrationTest.TestBudgetFailureReportRetainsActualWorkAfterInputsDie`
   gives **1/1 expected failure**: the previous report has no GSR summary.
2. After minimum counter/lifetime publication, rebuild `dal_cpp` and use phase
   `green` with the same test: **1/1 passes**.
3. Add `GSRScriptRepairTest.TestHeldOutFailureRetainsCompletedFitSummaryAndItsOwnAxes`.
   Recompile the runner and use phase `held-red`: **1/1 expected failure**,
   completed objective 0 is lost. Preserve a finalized marker and completed
   summary, rebuild core, then use phase `held-green` with both tests:
   **2/2 pass**. The original diagnostics/axis/candidate/tape assertions remain.
4. Add success/Candidate/nonfinite-input coverage, format, rebuild core and runner.
   `python3 ../dal414-evidence/verify.py test final
   'GSRScriptCalibrationTest.*:GSRScriptRepairTest.*-GSRScriptCalibrationTest.TestFrozenNumericalBudgetCanPassWhileFitFails'`
   passes **25/25** (20 existing and five new). The new success-status test uses
   an explicit loose synthetic scale solely to isolate report status and finite
   zero handling; it is not new numerical acceptance evidence. All existing
   tests/thresholds remain unchanged. Full frozen sampling matrices are excluded.

Both touched translation units pass GCC 14/15 syntax checks with the exact
warning policy from `.github/workflows/cmake-linux.yml`. An exploratory stricter
`-Wall -Wextra -Werror` without the repository's exclusions failed in unchanged
AAD interfaces/old test aggregates for unused parameters, sign comparisons and
missing aggregate initializers; those four failed logs remain distinct. The
four actual policy checks pass without editing the policy or shared headers.
Maximum production CCN remains 8; new helper maximum is 6. Formatting, diff and
the repository Markdown check pass. Commands, XML, source/configuration/binary
identities and exploratory failures are preserved in the evidence archive.

## Performance and accepted numerical evidence

The changed caller is the core script driver on normal return and budget error.
The smallest measurement uses one BondOption observation and one selected g,
both FD/AAD modes, and both successful/budget-failure calls. CPU 4, one DAL
thread, 20 warmups and ten measured calls per case remain fixed. Baseline and
rebuilt callers have identical prices, raw derivative, plan identity, basket/
Jacobian/path counts and exception code/candidate availability across all 40
measured rows. Informational before/after median microseconds are FD success
1916.6205/1981.8915, FD failure 424.382/436.9285, AAD success
1693.4095/1761.4440 and AAD failure 580.8205/589.6605. These separate short runs
are not a calibrated paired regression gate or a timing-regression verdict.
No benchmark matrix or path-budget enlargement is justified by this report-only
change. The copied solver/pricing portion of the driver and the shared MC/model
sources are checked against the base; no extra evaluation was added.

Accepted P0 evidence remains in `dal-404-p0-same-source-validation.md` and P3
evidence in `dal-413-gsr-script-numerical-validation.md`, with actual merged
prerequisites `bf4cabea`/PR622 and `acadd8ee`/PR621. These are historical accepted
constraints, not newly executed numerical results: N100 StepStalled with
converged=false remains separate from fit/numerics/held-out; N200
0.0318712375 > .025 and old Swaption 0.0426379743 > .025 remain refusals.
MRG32 seed1024/Precise, 589824 reserved paths/view and their frozen scales,
floors and FD steps are unchanged. No Windows/XLL/workbook, Python GIL, public
facade parity, example build/install matrix or full-suite pass is claimed.

No example/CMake target or dependency changed, so public-disabled/standalone
example build/run/install acceptance is deferred with the still-unimplemented
public examples. The unique base-master PR remains draft and DAL-414 remains
blocked on the precise new-domain sampling/held-out mapping. The coordinator
owns that decision and the later reviewer handoff; no specialist or downstream
task was dispatched, and nothing was merged.
