# LSMC checkpoint production evidence (DAL-352 / #571)

## Excel metadata and spill schema repair

This narrow repair starts at `d32f582bd7c46e94252024774155e1f1e92de703`.
It changes Excel help, direct schema tests and the schema description only.
The settings parser, numerical implementation and admission formula are unchanged.

- `MonteCarloSettings_New` argument help is **254 characters**, down from 305.
  It preserves the two-column format, setting categories, defaults, positive
  checkpoint/budget dependency, MiB unit and compiled/single-worker/exact-BS
  restriction. The complete supported key list remains in the Excel guide.
  Both `.inc` and `.htm` were regenerated from Machinist markup.
- The configuration spill is **16 rows by 2 columns, without a header**.
  The Windows raw-export regression now verifies every ordered key, both
  booleans, three numeric defaults, method/risk/precision strings and all eight
  absent optional values, including the two appended checkpoint fields.
  The portable projection test checks the same schema and native cell types.
- `dal_generate` and staged `dal_check_generated` passed. Python extraction of
  the generated argument-help literals and signatures for all **11** contracts
  in `TestScriptSettingsAndLegacyContracts` confirmed unchanged argument names
  and types and maximum argument-help lengths at most 255. This is a static
  metadata check, not execution of Windows registration.

The first portable CMake build was stopped when regenerated-file timestamps
scheduled unrelated dependency recompilation; it is not reported as a completed
build. Instead, the updated `test_lsmc_curvature.cpp` was compiled with the
existing Release portable-test flags:

```bash
c++ -std=c++17 -O3 -DNDEBUG -fPIE -DDAL_EXCEL_API_TESTS_PORTABLE \
  -I . -I dal-cpp -I dal-cpp/externals/rapidjson/include -I dal-cpp/test-support \
  -isystem dal-cpp/externals/googletest/googletest/include \
  -c dal-excel/tests/test_lsmc_curvature.cpp -o build/excel-contract-test_lsmc_curvature.o
```

The existing CMake `dal_excel_portable_tests` link command was replayed with that
test object substituted and output `build/excel-contract-tests`. The unchanged
binding objects were reused; the LSMC binding, public LSMC implementation and
test runtime sources match their recorded `bc96fd14` versions. The core archive
matches the repaired `e6b6d134` identity below. Under the shared issue486 flock,
`DAL_NUM_THREADS=1 build/excel-contract-tests` ran only
`ExcelLsmcCurvatureTest.TestCompletePassivePlanResultSettingsAndPolicyProjectionsDoNoWork`
and `ExcelLsmcCurvatureTest.TestCompleteSimulationOptionalZeroAndValidationPolicyMetadataRoundTrip`:
**2 passed**. Executable SHA-256:
`9de8bb67026a8fba38c49bd4e9a1b0b2671f60b0fedd8d229435f97a760e608a`.
Public archive SHA-256:
`17e01a9791c5e580959934eb78a46dfb52bacb9409aea64d4aef4843f1277a22`.

Clang-format, whitespace and 75-file documentation checks passed. Windows XLL
registration and raw-export execution remain with CI; they were not run on this
Linux host. No full regression, nine-target benchmark or 60/200-process matrix
was repeated. Earlier numerical evidence retains its original identities.

The opt-in implementation preserves the full-storage default and uses exact
passive BS/script restoration only inside its admitted domain. The measured
compact case reduces charged training capacity and process RSS. Runtime
comparisons remain informational on this shared host; no production speedup,
general memory-saving percentage or financial tolerance is inferred.

The archived allocation counters below predate the historical-bytecode budget
correction. They retain their original binary identity and are not complete
current admission bounds; the repair and its focused evidence are recorded below.

## Provenance and scope

- Baseline: `cdf9e267a8028ae23ce04af277acecb7205f45bc` (merged #583).
  A final fetch still found that master.
- Numerical implementation and component tests:
  `be058edc620c730f0732a487475134bd70ec3211`.
- Measured driver/collector:
  `bc96fd149b9d053578396312b7f0748a7ea14466`, clean worktree at collection.
  Subsequent implementation repairs have separate provenance below.
- Driver SHA-256:
  `3f7cc97ab016d96a8d939813c6aa0ee4482a475d04640ba519a854aaf9a4cd6e`.
- Core library SHA-256:
  `c828dad927c2a81113b1c30f88bb9605861686aa6a5f203e4cba04465a744bd3`.
- Core test executable SHA-256:
  `f1ae236238544d3bdc8b7bb422502462071f83db2544bb4c3b0e3a45bba85244`.
  Library and core test identities remained unchanged during driver corrections
  and measurement; all three final identities were checked after sampling.
- GCC 15.2.0, CMake 4.2, Unix Makefiles, C++17 Release, native AAD, Eigen enabled,
  native-architecture tuning disabled, `-O3 -fPIC -ffp-contract=fast`.
  Dependencies remain the pinned submodule revisions from the baseline.
- Intel Core i9-13900HX, 32 logical CPUs, WSL2 Linux 5.15.167.4, glibc 2.43.
  `DAL_NUM_THREADS=1`; the complete measurement round held the shared issue486
  flock. External Rust/Clippy compilation and the IDE remained active and were
  left undisturbed. This is a noisy environment.

Changed paths map to LSMC training, native BS restart, cold tape-capacity lookup,
and C++/Python/Excel settings and projections. Selected checks therefore cover
LSMC policies/replay, BS behavior, tape lifecycle/capacity, grouping contracts,
binding settings and provenance. Ordinary model arithmetic, other models,
parallel restoration and tape segmentation are not optimization targets here.
The nine-target benchmark gate and full regression matrix were not rerun.

The original 200-process prototype study retains its original source/executable
provenance and inconclusive timing assessment. These are separate production
measurements.

## Correctness and recovery

The Release component build completed for `dal_cpp_tests`,
`dal_public_tests`, `dal_excel_portable_tests` and `_dal`.

Executed filters:

```bash
DAL_NUM_THREADS=1 build/checkpoint-production/dal-cpp/dal_cpp_tests \
  --gtest_filter='LsmcCheckpointTest.*:ScriptExerciseLSMCTest.*:ModelTest.*BlackScholes*:AADRecordingTest.*:AADTapeCapacityTest.*:PortfolioGroupingTest.*'
DAL_NUM_THREADS=1 build/checkpoint-production/dal-public/dal_public_tests \
  --gtest_filter='HybridValueTest.*:ScriptContractTest.*'
DAL_NUM_THREADS=1 build/checkpoint-production/dal-excel/dal_excel_portable_tests \
  --gtest_filter='ScriptExcelContractTest.*:ExcelLsmcCurvatureTest.*'
DAL_NUM_THREADS=1 build/checkpoint-production/dal-excel/dal_excel_portable_tests \
  --gtest_filter='ExcelRiskTest.*:ExcelWeightedRiskTest.*'
```

Results: **176 core, 27 public, 36 Excel settings/LSMC and 14 Excel provenance
tests passed**. Python build-tree tests
`test_lsmc_checkpoint_settings.py` and `test_script_settings.py`, selected with
`-k 'lsmc or checkpoint'`, passed **92**, with 187 unrelated tests deselected.
The Python interpreter was an isolated local 3.12.13 environment with pytest
9.1.1. Documentation checks and staged `dal_check_generated` passed. The two
Excel generated help files were regenerated through Machinist.

Coverage includes:

- Exact hard policy, normalization, coefficients, held-out selection, price and
  exercise rates versus native full storage; bridge off/on; blocks 1, 4, 8, 65
  and 79 with a 65-event grid. Admission may select full storage for shapes with
  no conservative storage benefit.
- Bitwise FrozenAAD/RetrainedBump prices and every named risk:
  `spot`, `vol`, `rate`, `div`, `K`.
- Scalar regression across 8,193 training paths (the native 8,192-path chunk
  boundary), no validation, and multivariate historical/vector state.
- Sealed historical fixing plus constant-dependent historical scalar/vector
  seeds, including retrained constant fits.
- Tail blocks and pending decisions consumed before row release.
- Growing APPEND history fallback, insufficient budget, invalid-path unwinding
  and successful subsequent requests.
- Cold-thread 2 MiB admission followed by rejection of bumped fitting after
  pricing creates retained tape; the same thread succeeds with 64 MiB.
- Explicit rejection of tree execution, zero-time event grids, retained future
  fixing samples, multiple workers and one-sided model bumps. Valid two-sided
  requests do not require unused further bumps at their bumped points.

The production verification driver compared **98,945 raw payment/feature/
exercise/condition cells per block-size case** against native rows and checked
four rejected restores: model point, constant point, stream and boundary.
It also checked policy metadata and hard exercise counts. An additional
`--schedule verify --mode retrained` run matched `MCLsmcAadSimulation`
bit for bit.

The observation admission check maps original event IDs through the sealed
plan's live-event IDs, so historical rows do not shift future sample matching.

## Allocation and process measurements

[Raw outputs, resource records, samples and provenance](../../docs/experimental/data/lsmc-checkpoint-production.json)
contain ten process samples per side and mode, alternating side order.

These archived charged peaks and bounds belong to `bc96fd14`; that implementation
omitted the simultaneously retained historical compiled program. Its RSS, raw
prices/risks and work counts remain observations of that binary, but its charged
figures do not prove complete training admission. Current admission adds the
historical holder and all nested program capacities to each fit's reserve.

One case was selected: 65 events, block length 8 (nine blocks, one-event tail),
4,097 training, 1,023 validation, 2,049 pricing paths per replicate, two pricing
replicates, bridge enabled, quadratic three-feature basis, seeds 17/29.
The compact historical-state script uses BS inputs
`(100, 0.2, 0.05, 0.02)`, `K=100`, smoothing width 2 and a 512 MiB training
admission budget. The driver times preparation, fitting and native pricing.
RetrainedBump retains the up-policy through the down fit and follows native
model/constant fit lifetimes.

| Mode          | Full charged peak (bytes) | Checkpoint charged peak (bytes) | Full minimum RSS (KiB) | Checkpoint minimum RSS (KiB) | Full minimum seconds | Checkpoint minimum seconds |
|---------------|---------------------------|---------------------------------|------------------------|------------------------------|----------------------|----------------------------|
| Hard          | 14,132,460                | 9,035,416                       | 22,228                 | 19,452                       | 0.1957               | 0.3752                     |
| FrozenAAD     | 14,132,460                | 9,035,416                       | 22,160                 | 19,392                       | 0.3052               | 0.3760                     |
| RetrainedBump | 16,103,594                | 11,006,550                      | 24,768                 | 21,788                       | 1.6136               | 2.2472                     |

Charged peaks combine exact tracked vector capacities, raw-row reserves and
conservative concurrent fixed reserves. They are not process RSS or logical
entry counts. Actual allocation hooks include old/new capacity overlap,
checkpoint payloads/headers, scalar/vector history, normal vectors, path/RNG
buffers, holding/masks, QR columns and the copied QR targets. Fixed reserves
cover QR/construction frames, immutable numeric programs/plans, model state and
base/up/down policies. The single-worker account has constant-size control
state; it does not create an allocation-address ledger.

For Hard/FrozenAAD the checkpoint admission bound was **10,860,680 bytes**.
For RetrainedBump it was **12,827,128 bytes**, including the retained tape.
Every observed charged peak stayed below its bound. The full reference is
unrestricted; its raw `allocation_bound_bytes=0` means no reference limit.

Frozen pricing retained **1,966,448 bytes** of native tape capacity. It was
absent during a fresh base training fit, then present alongside rows and QR in
subsequent retrained fits. Retrained peak accounting includes that overlap and
the live policies, rather than summing eleven sequential training populations.
Pricing keeps its ordinary full-path AAD recording. This training budget does
not cap future pricing tape growth, executable/runtime memory, the caller's
source contract graph, allocator bookkeeping or retained allocator pages.
RSS is recorded separately for that reason.

All paired prices and all five active named risks matched exactly in every
sample. Hard mode has no AAD risks; its raw zero slots are the passive result's
unused risk vector.

## Work and interpretation

Counters cover training fits; unchanged pricing work is excluded. For each
base fit, `L=5,120`, `E=65`, `K=9`:

| Counter             | Native full reference | Checkpoint |
|---------------------|-----------------------|------------|
| Full normal vectors | 5,121                 | 51,200     |
| Model steps         | 332,865               | 665,600    |
| Script events       | 332,800               | 665,600    |

The native reference includes its one deterministic-numeraire probe. The
budgeted full provider needs no such probe. Checkpoint work matches
`L*(K+1)` complete normal vectors and `2*L*E` model/script steps.
RetrainedBump performs eleven sequential fits: 56,331 versus 563,200 training
normal vectors and 3,661,515 versus 7,321,600 model steps. Every sampled count
was checked by the collector.

The original compact-case measurements support the declared opt-in's numerical
and work behavior. Complete admission additionally requires the historical-program
correction verified below. They provide no general replacement or acceleration claim.
The reported best-of-ten times are informational; shared-host variability
prevents a stable regression verdict. The earlier prototype's 40.60% compact-state
reduction and 46.58% growing-history increase are not relabelled as these
measurements.

Reproduce the standalone driver with the isolated CMake project under
`dal-cpp/experimental/lsmccheckpoint/`, target
`lsmc_checkpoint_production`, or with the same Release compiler flags against
the freshly built static core library. Collect the paired case with:

```bash
flock <shared-issue486-lock> python3 \
  dal-cpp/experimental/lsmccheckpoint/runproduction.py \
  build/checkpoint-production/lsmc_checkpoint_production \
  --output docs/experimental/data/lsmc-checkpoint-production.json
```

## Collector repair and master integration

The follow-up uses the same feature branch and retains the reviewed
`8226c5eb17a748c7bacb5310828be4c2d846ce8b` object. Merge commit
`3a7ced3afe38797ee02d41da409e7eedb0ef5a87` integrates master
`01dcadace1180629e3b4467f5334c021c530054a` without conflicts. That master
change affects benchmark diagnostics, CI and documentation; the measured core,
public/binding implementation, standalone production driver, build definitions
and pinned dependencies are unchanged. The three executable/library hashes
listed above were checked again and match. The 60-process production study,
component coverage and earlier prototype study retain their original provenance;
they are not new-head measurements.

The Linux collector resolves its operator-selected, trusted local executable to
an existing executable file. The timer, Git and compiler come from fixed
`/usr/bin/` paths, resolved before use. Every spawn uses an argument list and
`shell=False`; modes, schedules, sample indices and metadata queries are
allowlisted, and workload sizes remain fixed. Raw output and Git provenance
use the collector's repository root. The narrowly audited B404/B603 annotations
describe these boundaries, following the existing study runner; no rule or gate
is disabled. Collection, numerical checks and summaries are separate functions:
Radon reports `main=1`, with maximum complexity 8. Bandit 1.9.4 reports no
findings with the audited annotations.

The added collector regression suite runs with:

```bash
build/python-checks/bin/python -m unittest discover \
  -s tests/lsmc-training-study -p test_production_collector.py -v
build/python-checks/bin/python -O -m unittest discover \
  -s tests/lsmc-training-study -p test_production_collector.py -v
```

Both runs pass **9 tests**. A lightweight local driver exercises all 60 process
positions through the real GNU timer, alternating order, stdout/stderr retention,
fixed workload arguments and summary output. Separate cases reject invalid
executables/arguments, retain failed-process diagnostics, enforce every
price/risk/work/allocation assertion under optimization, and reject identity
drift. The executable name contains spaces and shell metacharacters to verify
literal argv handling. Replaying the archived production samples reproduces the
entire original payload exactly; the fixtures are not production measurements.
The collector's Linux-specific process tests are skipped on other platforms.

Old-head CI additionally exposed two stale exact-schema expectations:
`ScriptApiTest.TestSimulationEchoFieldSetMatchesAcrossSchemas` and
`test_diagnostics_keep_actual_ids_and_legacy_schema_boundary`. Both were
reproduced locally. Their assertions now explicitly include the two opt-in
settings and continue checking the complete field set and default null values.
This fixes the expectations without changing the diagnostic or numerical API.

Follow-up verification passed: rebuild `dal_public_tests` with the original
Release configuration, then run only
`--gtest_filter=ScriptApiTest.TestSimulationEchoFieldSetMatchesAcrossSchemas`
(1 passed); run `test_fix_valuation.py::test_diagnostics_keep_actual_ids_and_legacy_schema_boundary`
against the existing matching Python extension (1 passed). The rebuild and
Python request held the shared issue486 lock. Final driver/core archive/core
test hashes still match the original identities. Documentation checks pass for
75 Markdown files, and `git diff --check` is clean. Full regression and
cross-platform checks remain with current-head CI.

## Historical-program budget and source-admission repair

Implementation/test commit: `e6b6d134c0614a9c48558d0fa8fa1cbda0b1b7dd`,
on the original feature branch after `2830f17f`. Admission now borrows the
prepared historical program and charges its optional holder, outer vector
capacities and each opcode/constant row capacity through the existing checked
addition/multiplication helpers. This reserve is shared by the full and checkpoint
envelopes and repeated for bumped fits. Existing future-program, state, row,
checkpoint, normal, QR, policy and retained-tape charges remain in place.

The new regression preserves variable/constant dimensions and the 65-event
future grid while adding 100,000 historical `seed = seed + K` statements.
Against the original archive (`c828dad9...`), its 1 MiB rejection assertion
fails because the long-history request succeeds. With the correction, short
history still fits 1 MiB; long history is rejected, and a subsequent short request
succeeds. A 64 MiB retry matches unrestricted Hard policy metadata and price,
and Frozen/RetrainedBump prices plus `spot`, `vol`, `rate`, `div`, `K` risks match
bit for bit. Historical compiled capacity grows by exactly **2,800,000 bytes**;
the admitted bound grows from **736,748 to 3,536,748 bytes**, by that same amount.
The long-history charged peak is **3,425,693 bytes**. A fresh native thread isolates
the 1 MiB control from tape retained by preceding tests; retained tape continues
to be charged in production and in the existing bumped-fit recovery case.

The core target was rebuilt with its existing Release configuration under the
shared issue486 lock. The focused executable links only
`test_lsmc_checkpoint.cpp`, the existing test main, the rebuilt core archive and
the matching Google Test archive. This avoids rebuilding unrelated test objects:

```bash
cmake --build build/checkpoint-production --target dal_cpp -j 4
c++ -std=c++17 -O3 -DNDEBUG -fPIE -ffp-contract=fast \
  -I dal-cpp -I dal-cpp/externals/rapidjson/include \
  -isystem dal-cpp/externals/googletest/googletest/include \
  dal-cpp/tests/script/test_lsmc_checkpoint.cpp dal-cpp/tests/test_main.cpp \
  build/checkpoint-production/dal-cpp/libdal_cpp.a \
  build/checkpoint-production/lib/libgtest.a -pthread -o build/history-budget-tests
DAL_NUM_THREADS=1 build/history-budget-tests --gtest_filter='LsmcCheckpointTest.*'
```

All **9 checkpoint tests passed** at implementation/test commit `e6b6d134`,
including the existing block/tail, mapping/policy, every named risk, growing-state
fallback, retained-tape rejection/recovery, unsupported-domain and 8,193-path
boundary cases. The test XML records the capacity/bound/peak values above.
New core archive SHA-256:
`e1e5a1aac0dc5789fe1a11cfe8af0af2406a75f40ddef88adf58cc4e92b00a05`.
New focused test executable SHA-256:
`450500df9d64ec18465d9ad3e8efeb54f556e6184c4816ff2de2edaceec41469`.
The original standalone driver and original full test executable were not
relinked as new-head artifacts. Source/dependency/configuration checks,
clang-format, whitespace checks and the 75-file documentation checks passed.

The original whole-test target rebuild was cancelled before completion when it
started recompiling unrelated objects; it is not reported as a successful build
or test run. No 60/200-process sampling or full component matrix was repeated.
The numerical training/replay algorithms are unchanged; this repair adds the
missing fixed reserve and traverses historical event containers for capacities.
Prior numerical, mapping, public-binding and measured-process evidence remains
at its original identity, rather than being relabelled as new-head execution.

The research overlays remain historical tools. Their complete-source hashes,
function-shape checks, address replacements and width checks are unchanged.
Portable tests now consume four exact sources from immutable tool commit
`05a24acd3457e0d42ce396f02d88572872d14095`, with every existing native hash
verified after deterministic gzip decoding. Both overlays explicitly reject the
current production source. The UTF-8/CRLF, both-width, unknown/mutated source and
repeated-application tests remain active. The original 62-test group plus these
two rejection cases passes **64 tests in normal and optimized Python modes**;
this includes all nine collector cases and exact archived-payload replay.
Neither the binary32 nor training-budget studies certify current checkpoint code.

The collector now invokes literal absolute system executable paths
(`/usr/bin/git`, `/usr/bin/c++`, `/usr/bin/time`) after validating their resolved
targets. The operator-selected local driver remains resolved and checked; workload
arguments remain fixed/allowlisted and every spawn still uses `shell=False`.
No subprocess API was hidden or replaced, and no new suppression/configuration
exception was added. The matching upstream Semgrep families are
`dangerous-subprocess-use-audit`, `dangerous-subprocess-use-tainted-env-args`
and `dangerous-subprocess-use`, in the Python security/audit rules.
[The inspected Codacy wrapper's rule-source pin](https://github.com/codacy/codacy-semgrep/blob/b32599900a45557911bd75425c8d30bc9eec3985/internal/docgen/parsing.go)
is `4ccd3b9cce2321a5fe3793868e4c2d4cfa5e9c43`. Local Semgrep 1.180.0
with those three pinned rules reports three static-command findings on the old
source and zero on the new source. Local old-source lines are 52/59/75; this is
not claimed as an exact reproduction of hosted Codacy's 52/75/76 annotations
or its engine/configuration. Current-head hosted checks remain authoritative.
