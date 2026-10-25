# Native Curve Suite And Python InFine Removal

Date: 2026-10-26
Status: Approved by the user's continuous benchmark and PR directive

## Directive

The user authorized continuous completion without per-case approval, requested a native C++ benchmark set with DAL, QuantLib, and InFine, removal of InFine from the Python benchmark set, and a complete PR with all discovered PR issues fixed.

## Selected Scope

Deliver one native curve suite with these five cases:

1. `discount_queries`.
2. `calibration_single_5`.
3. `calibration_single_15`.
4. `calibration_multi_staged_5`.
5. `calibration_multi_staged_15`.

The Python API comparison retains all 43 existing workloads but only DAL, QuantLib, and rateslib backends. InFine moves out of that backend inventory and remains available only to the native C++/.NET comparison. This task does not merge the PR; it publishes it, reaches green exact-head checks, and resolves every actionable review comment found.

## Non-Goals

- No native equity/barrier/early-exercise MC, GSR, simultaneous joint, or XCCY cases in this change.
- No production DAL pricing/calibration algorithm changes, new public financial API, or competitor performance gate.
- No removal of DAL, QuantLib, or rateslib workloads, or changes to their available algorithms.
- No claim that native C++ and .NET runtimes or solver iterations are identical.

## Alternatives Considered

### Selected: complete the native curve suite and remove the Python InFine backend

Generalize the existing worker/controller to support single and staged calibrations at both sizes and a multi-case report. This gives a coherent curve set without duplicating infrastructure.

### Rejected: add only `calibration_single_5`

That would leave the collection without the already supported staged calibration path and would not be a complete curve suite.

### Rejected: migrate every InFine-supported workload in the 43-case suite

That requires unrelated MC/GSR implementations and review scope. Those workloads remain in the Python comparison only for DAL/QuantLib/rateslib and are not falsely presented as native InFine coverage.

## Functional Requirements

### R1: native worker coverage

Extend the C++ worker with optional case selection for all five cases. Existing omitted-case behavior remains `discount_queries`.

Single calibration builds and solves one curve with 5 or 15 annual IRS. Staged calibration builds and solves the USD discount curve, then builds and solves the USD forward curve using the solved discount curve. DAL uses the existing `CalibrateYieldCurve` settings with `calibrateDiscountCurve_` and projection routing matching `calibration_dal.py`. QuantLib matches `calibration_quantlib.py`, including the exogenous discount handle in the staged solve. InFine retains its native staged builder and `ForecastInstrument` path.

Every invocation constructs fresh instruments and curves and returns all annual and mid-year DFs in declaration order. The 5-quote single/staged cases emit 10/20 values; the 15-quote cases emit 30/60 values. Raw quotes, independent oracles, JSON, Python, IPC, startup, and validation stay outside timing. Instrument construction, initial curves, solves, DF conversion, and native output allocation stay inside timing.

Use `2e-8` absolute and `1e-10` relative tolerance with the existing max convention. Every warmup and measured result must pass.

### R2: multi-case controller and report

Allow repeated `--case` options with all five choices. Omission still selects only `discount_queries`. A single selected case retains its existing schema. A multi-case report uses `dal.native-curve-comparison/1` and includes every selected case in deterministic inventory order.

Each backend/case pair runs in a fresh process after three warmups. Use two rounds of ten samples, backend rotation, second-round reversal, and one thread. Preserve full worker rows and aggregate minima/median/ratios per case. Output directories must be new and paths include the case when needed to avoid collisions.

The full five-case acceptance has 300 worker invocations and 252,960 validated output values: 245,760 discount values, 600 single-5, 1,800 single-15, 1,200 staged-5, and 3,600 staged-15 values. Every case/backend has 20 samples.

### R3: remove InFine from the Python comparison

Change the Python `BACKENDS` inventory to DAL, QuantLib, and rateslib. Remove the Python controller/worker's InFine CLI/build plumbing, timing columns, and static report claims while preserving dynamic summaries and numerical evidence for the remaining backends.

Retain the C# source, pinned builder, fixture generator, and native-only build/oracle tests needed by the native suite. Update affected tests rather than deleting native InFine coverage. Update Python documentation, counts, CI commands, and artifact labels so no Python report contains an InFine timing or unsupported row.

### R4: CI and PR delivery

Add native QuantLib/Boost and native DAL worker build steps to the existing benchmark workflow. Run the five-case native acceptance and retain its artifacts. The Python API comparison runs without `--infine-build`; native integration tests use the published InFine worker and native C++ worker.

Publish the complete feature branch to origin and create one coherent PR against `master` with a category-prefixed title and all branch work summarized. Run relevant local tests first, then inspect exact-head CI and all review threads. Fix failures and actionable comments, restart stale evidence after every new head, and finish only when exact-head checks are green and no actionable review thread remains. Do not merge unless separately requested.

## Acceptance Evidence

- Unit/integration tests for single/staged fixtures, both sizes, staged discount routing, dynamic schemas, multi-case ordering, sample completeness, and old Python default behavior.
- Full Python benchmark tests with no InFine timing rows and retained native InFine oracle coverage.
- Native five-case acceptance with 300 workers and 252,960 validated values, current binary/source/CMake identities, and clean Git provenance.
- Existing native discount acceptance and affected C++ tests remain green; default DAL configuration does not require QuantLib.
- CI workflow/static tests, final diff checks, exact-head PR checks, and review-thread audit all pass.

## Open Questions

None. The authorized scope is the five-case native curve suite plus Python InFine removal and PR delivery.
