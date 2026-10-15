# LSMC checkpoint production evidence (DAL-352 / #571)

The opt-in implementation preserves the full-storage default and uses exact
passive BS/script restoration only inside its admitted domain. The measured
compact case reduces charged training capacity and process RSS. Runtime
comparisons remain informational on this shared host; no production speedup,
general memory-saving percentage or financial tolerance is inferred.

## Provenance and scope

- Baseline: `cdf9e267a8028ae23ce04af277acecb7205f45bc` (merged #583).
  A final fetch still found that master.
- Numerical implementation and component tests:
  `be058edc620c730f0732a487475134bd70ec3211`.
- Measured driver/collector:
  `bc96fd149b9d053578396312b7f0748a7ea14466`, clean worktree at collection.
  Later commits add this report and raw results only.
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

The result supports the declared opt-in and complete training admission for
this domain. It provides no general replacement or acceleration claim.
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
