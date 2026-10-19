# DAL-379 P2 Codacy repair

This record covers the incremental repair of PR #604 on
`agent/dal-orchestrator/7bc6ada900c9`, against exact baseline
`c68fe28e691620b867ade14e869033a71b53ce29`. The original review, validation,
assertions and archived controls are preserved. The parent remains `in_progress`.
Production repair commit: **`486c551ce40da65211c402a567d74bb0ff71ab5b`**.
The following evidence-only commit changes this record/index; its exact delivery
head and the one nonblocking remote snapshot are reported in the issue handoff.

## Scope and invariants

Read the complete paginated annotations for check `117882627399`: 26 warnings,
comprising 25 functions over complexity 8 and the Python registration function
over 200 lines. The check's head SHA matches the baseline. No rule, threshold,
public signature, generated entry point or test assertion was changed.

The repair extracts responsibilities without introducing persistent fields or
allocations. It preserves arithmetic expressions and accumulation order, the
four-start search and line-search order, PRICE error certification, outward
certificate bounds, exact/right-sided carry behavior, raw-quote refit branches,
defaults, work increments, capacities, budget accounting and failure admission.
Python registration statements retain exactly the original token sequence and
order across four helpers. Six generated constructors still validate and
normalize raw input before conversion/storage reads.

| Changed paths                                    | Responsibilities                                                                                                                                                                               | Directed evidence                                                                                                                             |
|--------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| `dal-cpp/dal/model/surface/marketivfit.cpp`      | Eligibility, Jacobian rows, projected bounds, damping/factor validation and triangular solves, rank pivot/orthogonalization, premium bands, line search, start certification, winner signature | P2 core tests; complete output comparison; fit 1/1, 300/12, 2048/64 and refit 8/25 samples                                                    |
| `dal-cpp/dal/model/surface/marketivmath.cpp`     | Interior/flat derivative bounds, theta-axis/interval certification, PRICE normalization, Black intrinsic                                                                                       | P2 core tests, independent high-precision Python derivatives; 71 exact-rational PRICE cases at 64/53 significand bits; same fit/refit samples |
| `dal-cpp/dal/model/surface/marketivevaluate.cpp` | Carry/point preparation and quantity dispatch                                                                                                                                                  | P2 core/public tests; carry-knot/extensions, derivative and Dupire Python tests; refit output comparison/samples                              |
| `dal-cpp/dal/model/surface/marketivsurface.cpp`  | Fit limits, individual quote checks and carry interpolation                                                                                                                                    | P2 invalid/limit/carry tests, independent Python carry/refit tests; same fit/refit samples                                                    |
| `dal-cpp/dal/model/surface/marketivrefit.cpp`    | Successful-direction outputs and request step/output checks                                                                                                                                    | P2 refit/aggregate budget tests; independent full raw refits; refit 8/25 samples                                                              |
| `dal-excel/src/__marketivinput.hpp`              | Raw-cell support/blank scan and cell validation                                                                                                                                                | Portable P2 test, generated-wrapper order/control-flow probe, XLL/raw-test declaration syntax; four native tests pending new-head MSVC CI     |
| `dal-excel/src/__marketivsurface.cpp`            | Additional settings/configuration and refit output rows                                                                                                                                        | Portable P2 test and XLL declaration syntax; native execution boundary as above                                                               |
| `dal-python/src/bindings/marketivsurface.cpp`    | Additional settings and request/diagnostic/result/function registration                                                                                                                        | Existing 10 Python tests, complete output/diagnostic comparison, static registration-token comparison                                         |

No changes to public facade implementation, test-support oracles, benchmarks,
examples, component targets, registration or output protocols. The core benchmark
is rebuilt with `DAL_BUILD_PUBLIC=OFF`; no reverse dependency was introduced.

## Configuration and RED

Linux x86-64, Intel i9-13900HX, GCC 15.2.0, CMake 4.2.3, C++17 Release/static,
`-O3 -DNDEBUG -ffp-contract=fast`, native AAD, Eigen/native-arch/sanitizers off;
Python 3.13.9 / pybind11 3.0.4. Existing build directories and caches are retained:
`build/dal379-market-iv` (core/public/Python/portable Excel) and
`build/dal379-market-iv-core` (standalone core benchmark, public off).

The user requires the original checkout/branch, so no new worktree was created.
An actual rebuild of the baseline static benchmark was copied to
`build/p2-ci-repair-baseline-quote-risk` before production edits. The original
Python binary SHA-256 is
`b2b6a5e3f245ed8aed8554096344c9cce7e89ab8f9f5536ae1ef0c252160d956`.
Baseline 21 selected CTests and 10 Python tests pass; the latter also record
complete returned objects for the numeric/diagnostic comparison.

```bash
gh api repos/wegamekinglc/Derivatives-Algorithms-Lib/check-runs/117882627399
gh api --paginate 'repos/wegamekinglc/Derivatives-Algorithms-Lib/check-runs/117882627399/annotations?per_page=100'
lizard -C8 -L200 -w dal-cpp/dal/model/surface/marketiv*.cpp \
  dal-excel/src/__marketivsurface.cpp dal-excel/src/__marketivinput.hpp \
  dal-python/src/bindings/marketivsurface.cpp
# RED: 25 function rows; Python init also exceeds length (26 remote annotations).
```

The focused RED is the unchanged-head static failure, not a fabricated numeric
failure: this repair changes structure only. Existing numerical assertions are
reused. No new implementation-mirroring unit tests were added.

## GREEN and comparison

Final rebuilt code: 21/21 selected CTests (19 core + public facade + portable
Excel), 10/10 Python tests, zero skips. All 93 complete returned fit/point/grid/
refit objects match the baseline, including date/request and exported surface
content, hexadecimal floats, every iteration/quote/start/certificate, failure
status/message, regime signature, outputs and work/capacity accounting.
Only exception source file/line/function provenance is normalized; raw records
are also retained. The normalized result SHA-256 is
`7cb6da84ee07d029948dae7a66fe86d9ad7bf5b0f6768095fded1a70c8387ee3`.

```bash
cmake --build build/dal379-market-iv --target \
  dal_cpp_tests dal_public_tests dal_excel_portable_tests _dal -j6
ctest --test-dir build/dal379-market-iv -R 'MarketIv' --output-on-failure
PYTHONPATH=build/dal379-market-iv/dal-python \
  python3 build/p2-ci-repair-capture.py build/p2-ci-repair-head-results.json
python3 build/p2-ci-repair-compare.py
lizard -C8 -L200 -w dal-cpp/dal/model/surface/marketiv*.cpp \
  dal-excel/src/__marketivsurface.cpp dal-excel/src/__marketivinput.hpp \
  dal-python/src/bindings/marketivsurface.cpp
# GREEN: zero warnings; max CCN=8, max function length=116.
cmake --build build/dal379-market-iv-core --target quote_risk_perf -j3
python3 build/p2-ci-repair-sample.py
cmake --build build/dal379-market-iv --target dal_check_generated -j3
```

Machinist reports no generated drift. All 44 generated MarketIv XLL entries,
original review/validation and tests match their baseline hashes. The existing
wrapper-order probe, with the new helper dependencies supplied, passes 6/6
constructor-order checks and 5/5 bool/null-storage/integer modes. The full XLL
source and existing raw-test source pass Linux declaration-shim syntax checks
(expected unsupported Windows export-attribute warnings only).
`python3 .github/scripts/check_docs.py` passes for all 75 Markdown files,
including this incremental record and its index entry.

The recovered reviewed 71-case PRICE probe is rebuilt against the current math
source, normally and with `-mlong-double-64`. Its output is byte-identical to
the reviewed original GREEN in both modes: 61 accepted, 10 rejected, zero
accepted errors above `1e-12`; independent exact-rational maximum errors are
`5.5042229562447386e-17` / `6.437791586165177e-16`. The forced-53-bit probe
executes only the PRICE conversion, avoiding mixed long-double math ABI calls.

```bash
g++ -std=c++17 -O3 -DNDEBUG -ffp-contract=fast -I dal-cpp \
  build/p2-ci-repair-price-probe.cpp dal-cpp/dal/model/surface/marketivmath.cpp \
  build/dal379-market-iv/dal-cpp/libdal_cpp.a -pthread \
  -o build/p2-ci-repair-price-native
# Repeat with -mlong-double-64 and output suffix -53; execute each and compare
# its 71 JSONL rows using Fraction(double), independently of product formulas.
python3 build/p2-ci-repair-wrapper-order.py --output-dir build/p2-ci-repair-wrapper
clang++ -std=c++17 -fms-extensions -D_WIN32 -DNOMINMAX \
  -I build/dal379-p2-probe -I . -I dal-cpp -fsyntax-only dal-excel/src/__marketivsurface.cpp
clang++ -std=c++17 -fms-extensions -DNOMINMAX -I build/dal379-p2-probe -I . \
  -I dal-cpp -I dal-cpp/externals/googletest/googletest/include \
  -fsyntax-only build/market-iv-raw-syntax.cpp
```

## Performance repair and final comparison

The first structured refactor passed all directed correctness checks and the
complete result comparison, but its first paired performance measurement showed
2048/64 fit increases of **7.7845% / 8.8423%** in the two confirmation rounds.
That result, all 40 process logs and the tested binary are preserved under
`build/p2-ci-repair-samples-initial` / `build/p2-ci-repair-initial-quote-risk`.
It was not relabeled as noise. The follow-up passes the original once-computed
integer matrix stride into factor/normal-element helpers and the expiry count
into projected-bound checking, instead of rereading them inside extracted loops.
Floating operations and their order remain unchanged. The affected binaries and
directed correctness/output checks are rebuilt before the new paired sample.

The stride mitigation alone remained above threshold (**7.4264% / 6.7011%**);
its full samples and binary are preserved separately under
`build/p2-ci-repair-samples-stride` / `build/p2-ci-repair-stride-quote-risk`.
Temporary localization binaries restored individual original functions: restoring
`Step` reduced the single diagnostic 2048/64 minimum from 140.645 ms to
130.856 ms (baseline 129.794 ms), while restoring `Rank` alone did not. These
single-process runs localize the repair; they are not an acceptance verdict.
The final decomposition retains the hot normal-product/factor loops in `Step`
and extracts damping, factor validity and triangular substitutions. No compiler
threshold, arithmetic, work limit or benchmark workload is relaxed.

Final measurement: two rounds, ten interleaved process pairs per round,
alternating first side, `DAL_NUM_THREADS=4`, one warmup and ten timed repetitions
per existing case. Reduce each round and all samples by minimum; preserve the
strict +4% rule in both rounds. Builds were completed before sampling; this is
a shared development host, with no concurrent task-owned build during samples.
No CPU/governor or benchmark configuration was changed. This target is outside
the repository's nine-target regression gate, so this is a **scoped informational
comparison, with no regression observed**, not a new scheduled-gate verdict.

| Existing case | Baseline min ms | Final min ms | Combined delta | Round 1 / round 2 delta |
|---------------|-----------------|--------------|----------------|-------------------------|
| fit 1/1       | 0.047173        | 0.046159     | -2.1495%       | -2.1495% / -1.4933%     |
| fit 300/12    | 3.323000        | 3.396000     | +2.1968%       | +2.3473% / +1.9208%     |
| fit 2048/64   | 131.041000      | 130.928000   | -0.0862%       | +0.2371% / -0.0862%     |
| refit 8/25    | 55.998000       | 56.130000    | +0.2357%       | -0.3250% / +0.5804%     |

Static benchmark binary SHA-256 identities:

- Baseline: `6d6ea42a97798692d2bc6b540b36277659542cf81a626fc65fa78f6dce979165`.
- Final: `0305a47f390c9b542dea1708cb473acec896a01c21ff09a3aff5d265417b1f31`.

All five capacity/work output lines are identical across all final samples.
The Q=10000/m=64/C=256 capped request returns `WORK_LIMIT` at exactly 10001 rows,
payload `3008776/5317504`, workspace `11166528/23134208`, quote capacity 10000,
history 804, Jacobian 660000. For refit 8/25: rows 162880, conversions 14400,
outputs 425, payload `160216/316288`, workspace `260076/881664`. Smaller shapes
and all raw process outputs/counters are retained in the sample results JSON.

## Evidence and remaining boundaries

Raw check/annotation pages, RED/GREEN complexity logs, source/cache/binary
identities, complete result dumps, comparison scripts, build/test logs, PRICE
probes/oracles, generated-wrapper probe and paired performance outputs are
retained under `build/p2-ci-repair-*` and delivered together as an issue attachment.
These runtime paths are evidence locations, not public download links.

Native Windows execution is unavailable locally. The following four tests must
compile and execute in the **new exact-head native MSVC CMake Windows job**:

```text
ScriptExcelRawTest.TestMarketIvSettingsActualOperBlankAndStrictInputs
ScriptExcelRawTest.TestMarketIvFitActualXllOwningAndTypedRejection
ScriptExcelRawTest.TestMarketIvGeneratedScalarInputsRejectBadStorage
ScriptExcelRawTest.TestMarketIvGeneratedRangeInputsRejectBadStorage
```

Portable, Linux wrapper isolation, declaration shims, 53-bit GCC conversion
probes and wheel jobs cannot certify native OPER lifetime/XLL ABI. Old-head
Windows passes cannot certify this head. No watch/poll, merge, auto-merge or
role dispatch belongs to this handoff.

Unchanged IVS/spread/archive compatibility and documentation/example evidence
remain the original audited results. No unrelated product/performance matrix
was rerun. Independent review/validation conclusions were not modified.
