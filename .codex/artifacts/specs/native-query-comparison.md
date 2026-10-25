# Native Three-Library Discount Query Comparison

Date: 2026-10-25
Status: Approved design; awaiting written-spec review

## Source And Problem

The user approved adding the native comparison after the prior delivery mixed two different measurements: DAL-only C++ microbenchmarks and a DAL/QuantLib/InFine report driven through Python APIs. The latter includes DAL Python/native and QuantLib Python/SWIG overhead while InFine times direct C# calls. It is not a native three-library comparison.

## Goals

1. Measure the existing `discount_queries` workload with direct DAL C++, QuantLib C++, and InFine C# calls.
2. Use exactly the same 22-node log-linear discount curve, 4,096 permuted query dates, and independent reference values for all three backends.
3. Exclude process startup, Python orchestration, JSON parsing/serialization, IPC, and correctness checks from every timed interval.
4. Preserve the existing Python API comparison unchanged as a separately labelled measurement.
5. Produce a repeatable, evidence-rich report that clearly identifies the C++/.NET timing boundary.

## Non-Goals

- No rateslib backend and no cases other than `discount_queries`.
- No curve-calibration, AAD, pricing, or additional public API changes.
- No changes to the existing four-backend Python comparison inventory or defaults.
- No C++/.NET in-process hosting, GPU comparison, or competitor performance gate.
- No claim that C++ and .NET are identical runtimes or that native ratios are calibration-wide results.

## Alternatives Considered

### Selected: isolated native workers and a focused controller

Add an opt-in C++ worker linked to DAL and QuantLib, retain the existing direct C# InFine worker, and use Python only to create fixtures, rotate fresh processes, and aggregate native reports. This gives precise timing boundaries without adding QuantLib to ordinary DAL builds.

### Rejected: extend `curve_calibration_perf` with QuantLib

This would couple the existing DAL-only target to a third-party dependency and would not provide a clean three-backend evidence path.

### Rejected: host C++ and .NET in one process

This adds runtime-hosting and ABI complexity without improving the timed operations; fresh-process startup is already outside the native timers.

## Functional Requirements

### R1: opt-in native C++ worker

Add `DAL_CPP_BUILD_NATIVE_COMPARISONS`, default `OFF`. It requires `DAL_CPP_BUILD_BENCHMARKS=ON` and an exact QuantLib 1.43 CMake package. Ordinary configuration must not search for or require QuantLib.

Add `dal-cpp/benchmarks/native_query_comparison/` with a worker accepting `--backend dal|quantlib`, `--input`, and `--output`. The worker is not added to the DAL benchmark inventory or the nine-target regression gate.

The acceptance build uses QuantLib v1.43 source revision `6b57206e04598f092efee66e3b367efc84771995`. Boost headers may be installed locally inside the workspace; no system package change is required.

### R2: direct native operations

Before timing, construct the curve and query-date containers and perform three checked warmup calls. DAL uses `Tape::DiscountLogDF_<double>` and evaluates `curve(anchor, query)`. QuantLib uses its log-linear interpolated discount curve and evaluates `discount(query)`. InFine retains its existing `InterpolatedDiscountCurve.DiscountFactor(query)` path.

Each timed invocation evaluates all 4,096 queries and creates the returned native array/vector. The timed interval ends before result validation or serialization. Curves and query inputs are reused; curve construction and container parsing are outside timing.

### R3: one shared fixture and independent oracle

Generate one fixture from the existing scenario definitions: valuation date 2025-01-15, annual nodes through 2046-01-15, ACT/365F, the existing deterministic log-DF ladder, and dates `1 + (i * 1777) % 7665` days after the anchor.

The independent Python oracle remains log-linear interpolation in log-DF space. Every backend checks all 4,096 values on every warmup and measured call with `2e-12` absolute plus `1e-10` relative tolerance. The controller validates them again. Missing, duplicated, non-finite, reordered, or incorrect values fail without a successful report.

The C++ fixture includes the anchor and all 22 nodes. The existing InFine fixture retains its separate anchor and 21 non-anchor nodes. Both representations must come from the same source values and queries.

### R4: native evidence controller

Add `run_native_queries.py` and a focused support module. The controller must not import the DAL, QuantLib, or rateslib Python backends. It invokes the C++ executable for DAL/QuantLib and the existing `infine.measure_cases` path for InFine.

Require `--native-worker` and `--native-build-root`. Resolve the worker inside that build root, verify both the executable and `CMakeCache.txt`, and reject a stale or escaped worker path. Parse the cache for the native build identity rather than inferring settings from the active shell.

Use ten fresh processes per backend per round and two rounds. Rotate backend order within each round and reverse it in the second round. Each process contributes one native sample after three warmups. `DAL_NUM_THREADS=1`; existing InFine single-thread and JIT settings remain unchanged.

The output directory must be new. Fail closed on missing binaries, unsupported selections, process errors, changed binaries, invalid evidence, or incomplete sample counts.

### R5: output and provenance

Write `results.json` with schema `dal.native-query-comparison/1` and `summary.md` titled as a native discount-query comparison. Record every raw native duration and returned value, backend order, round minima/median, third-party/DAL ratios, fixture hashes, native worker SHA-256, QuantLib and Boost versions, InFine build/runtime identities, compiler/CMake settings, checkout SHA/status, and thread environment.

Hash the controller, support module, native worker source, and shared fixture definitions separately from the existing Python API comparison hashes. Verify these inputs and native executable identities remain unchanged throughout the run; do not claim that a caller-supplied source path alone proves binary provenance.

The summary must state that DAL and QuantLib use direct C++ calls, InFine uses direct C# calls, and Python/JSON/IPC/startup are outside timing. Ratios are informational, not a performance gate.

## Testing And Acceptance

1. Python unit tests cover fixture equality, argument errors, native worker command construction, process rotation, evidence rejection, aggregation, summary wording, and the prohibition on importing Python backends.
2. A native integration test builds and invokes both C++ backends and the pinned InFine worker. It proves all 4,096 values pass and that only the three requested backends execute.
3. The acceptance command completes 20 native samples per backend across two rounds and produces only `discount_queries` rows.
4. Default CMake configuration still builds without QuantLib. The opt-in build compiles the native worker without dal-public or Python binding dependencies.
5. Existing comparison tests, affected C++ tests, and formatting/diff checks remain green. Performance verification is limited to this query case; unrelated benchmark matrices are excluded.
6. Documentation gives reproducible QuantLib/InFine build and native comparison commands and distinguishes this report from the existing Python API report.

## Error Handling And Compatibility

The new option and files are additive. No production DAL pricing or curve code changes in this task. A worker returns nonzero and retains its log on any input, numerical, or timing failure. A failed controller run retains `status: failed` and never reuses an older output directory. Existing Python comparison schema and output remain unchanged.

## Open Questions

None. The requested scope is the single native `discount_queries` comparison across DAL, QuantLib, and InFine.
