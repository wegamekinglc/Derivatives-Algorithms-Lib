# F01 common calibration Python performance

Status: local incremental verification passes; exact Python publication-head CI
remains required. Complete F01 and P01 production MC acceptance remain open.

## Inputs and protocol

Baseline is the frozen Python module from accepted common C++ publication
`12b3d7d1a9e43ac008c257c24b6365e424b458e5`, retained before Python edits in
`aad-calibration-python-baseline-01`. Its manifest pins six package/archive files.
The head is the rebuilt OFF module copied to
`aad-calibration-python-measured-head-01`. Both core/public archives and all nine
C++ gate executables remain byte-identical to accepted C++. This increment
changes binding construction/registration and adds passive Python projections.

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
Inputs are frozen in `aad-calibration-python-cost-inputs-02.json` (640 hashes),
with worker/run scripts and all preflight numeric inventories. Results are in
`aad-calibration-python-cost-paired-02/results.json`; every process has retained
JSON and stdout/stderr. Native module hashes identify the loaded package.

WSL2 Linux 5.15.167.4, x86-64 Intel i9-13900HX, 32 visible CPUs, CPython 3.13,
GCC 15.2/C++17 Release, Eigen ON, native architecture OFF, native AAD, lifetime/
profiling OFF. Each process is pinned to CPU 4 with DAL_NUM_THREADS=4.
The head and baseline use the same interpreter and worker/workloads. No other
local build, correctness test, benchmark, source edit or Git/PR mutation runs
during measurement. The shared host's noise limits remain.

Correctness precedes timing: 919 workspace Python cases, 918 cases plus one
workspace-only skip in each standalone OFF/combined package, and exact installed
C++ parity for 48 numeric cells/three metadata rows in all three modules.
Thirty-five old-entry workloads cover config construction at one/eight bindings,
all four providers at N=8/16 in ANALYTIC/BUMPED, provenance construction and
10-trade quote aggregation, plus typed 9x2 Dupire pullback. Calibration and fixture
preparation are outside timing. Each row keeps fixed loop counts and performs
numeric validation before warmups and after the timed batch; all retained
projection digests match base/head and every process exactly.

Two independent rounds each run ten alternating processes per side (40 total).
Use per-round minima and retain the unchanged 4% rule: fail when both rounds
exceed +4%. All 35 cases pass; a movement above 4% in only one round remains in
the table. This is bounded incremental changed-binding verification, not a fresh
master comparison or final whole-plan production performance acceptance.

Keep the first run's path-alias KeyError and its single raw process in
`aad-calibration-python-cost-run-01.log` and `cost-paired-01`. It cannot produce
an acceptance result. Run 02 canonicalizes the native path for hash lookup only;
worker, workload, loop counts, warmups, binaries, schedule and policy are unchanged.
No initial evidence is overwritten. All 640 inputs remain unchanged afterward.

## Complete old-entry results

Times are ns per call; each pair of minima is from its own ten-process round.

| Case | Round 1 base ns | Round 1 head ns | Round 1 % | Round 2 base ns | Round 2 head ns | Round 2 % | Verdict |
|------|----------------:|----------------:|----------:|----------------:|----------------:|----------:|---------|
| config_1 | 563.19 | 575.61 | 2.205 | 573.41 | 581.17 | 1.353 | Pass |
| config_8 | 1243.55 | 1238.80 | -0.382 | 1246.15 | 1249.89 | 0.300 | Pass |
| single_8_ANALYTIC_provenance | 240394.35 | 241873.58 | 0.615 | 242247.38 | 244392.64 | 0.886 | Pass |
| single_8_ANALYTIC_aggregate_10_trades | 27072.05 | 26734.85 | -1.246 | 27177.05 | 27057.80 | -0.439 | Pass |
| generic_8_ANALYTIC_provenance | 425265.47 | 423425.83 | -0.433 | 431388.89 | 429468.10 | -0.445 | Pass |
| generic_8_ANALYTIC_aggregate_10_trades | 163364.85 | 162378.65 | -0.604 | 163254.50 | 162979.55 | -0.168 | Pass |
| joint_xccy_8_ANALYTIC_provenance | 2541130.44 | 2558409.15 | 0.680 | 2568566.85 | 2573511.94 | 0.193 | Pass |
| joint_xccy_8_ANALYTIC_aggregate_10_trades | 1923264.60 | 1914166.05 | -0.473 | 1926933.50 | 1924529.75 | -0.125 | Pass |
| staged_xccy_8_ANALYTIC_provenance | 811802.13 | 809413.11 | -0.294 | 819261.38 | 820393.23 | 0.138 | Pass |
| staged_xccy_8_ANALYTIC_aggregate_10_trades | 712324.80 | 708904.70 | -0.480 | 712121.25 | 721487.60 | 1.315 | Pass |
| single_8_BUMPED_provenance | 244286.24 | 241424.67 | -1.171 | 248858.13 | 241282.47 | -3.044 | Pass |
| single_8_BUMPED_aggregate_10_trades | 26897.50 | 27326.75 | 1.596 | 26909.45 | 28222.25 | 4.879 | Pass |
| generic_8_BUMPED_provenance | 422811.27 | 414155.81 | -2.047 | 422086.57 | 424081.62 | 0.473 | Pass |
| generic_8_BUMPED_aggregate_10_trades | 161823.75 | 162152.90 | 0.203 | 166671.05 | 167274.55 | 0.362 | Pass |
| joint_xccy_8_BUMPED_provenance | 2535318.94 | 2525304.29 | -0.395 | 2550820.89 | 2552529.03 | 0.067 | Pass |
| joint_xccy_8_BUMPED_aggregate_10_trades | 1921716.55 | 1917971.50 | -0.195 | 1917140.40 | 1938865.50 | 1.133 | Pass |
| staged_xccy_8_BUMPED_provenance | 817375.09 | 806348.04 | -1.349 | 835421.65 | 812676.38 | -2.723 | Pass |
| staged_xccy_8_BUMPED_aggregate_10_trades | 714110.15 | 714127.20 | 0.002 | 715689.60 | 709492.75 | -0.866 | Pass |
| single_16_ANALYTIC_provenance | 488509.62 | 489437.67 | 0.190 | 490186.44 | 495548.70 | 1.094 | Pass |
| single_16_ANALYTIC_aggregate_10_trades | 38345.35 | 39162.15 | 2.130 | 39587.05 | 39475.75 | -0.281 | Pass |
| generic_16_ANALYTIC_provenance | 764291.64 | 762421.71 | -0.245 | 776277.46 | 771833.63 | -0.572 | Pass |
| generic_16_ANALYTIC_aggregate_10_trades | 243591.30 | 242644.85 | -0.389 | 244700.25 | 242779.40 | -0.785 | Pass |
| joint_xccy_16_ANALYTIC_provenance | 6720832.39 | 6721326.35 | 0.007 | 6774475.16 | 6797774.17 | 0.344 | Pass |
| joint_xccy_16_ANALYTIC_aggregate_10_trades | 3598396.25 | 3598220.55 | -0.005 | 3620788.95 | 3630464.30 | 0.267 | Pass |
| staged_xccy_16_ANALYTIC_provenance | 1424748.88 | 1433457.43 | 0.611 | 1449017.24 | 1446441.41 | -0.178 | Pass |
| staged_xccy_16_ANALYTIC_aggregate_10_trades | 1309139.40 | 1322122.40 | 0.992 | 1316019.00 | 1323678.45 | 0.582 | Pass |
| single_16_BUMPED_provenance | 493317.75 | 481541.95 | -2.387 | 492950.72 | 489118.13 | -0.777 | Pass |
| single_16_BUMPED_aggregate_10_trades | 37960.00 | 37732.90 | -0.598 | 38870.10 | 39437.75 | 1.460 | Pass |
| generic_16_BUMPED_provenance | 745238.78 | 739907.29 | -0.715 | 738985.94 | 747903.52 | 1.207 | Pass |
| generic_16_BUMPED_aggregate_10_trades | 242021.00 | 245139.25 | 1.288 | 248018.35 | 243091.40 | -1.987 | Pass |
| joint_xccy_16_BUMPED_provenance | 6741264.31 | 6741334.86 | 0.001 | 6745232.75 | 6843575.85 | 1.458 | Pass |
| joint_xccy_16_BUMPED_aggregate_10_trades | 3588727.30 | 3607911.70 | 0.535 | 3598724.30 | 3639828.90 | 1.142 | Pass |
| staged_xccy_16_BUMPED_provenance | 1435964.67 | 1429417.12 | -0.456 | 1442877.77 | 1434054.17 | -0.612 | Pass |
| staged_xccy_16_BUMPED_aggregate_10_trades | 1308671.70 | 1314381.90 | 0.436 | 1310735.40 | 1308579.10 | -0.165 | Pass |
| typed_dupire_9x2 | 24116.14 | 24251.79 | 0.562 | 24518.12 | 24502.31 | -0.064 | Pass |

## Additional common-entry costs

Twenty separate informational head processes use the same CPU/thread settings,
with two best-of-ten rounds. Prepared boundaries, source capture/calibration,
seed construction and numeric getter projections are outside timing. Calls
include the Python checked owning argument copies, GIL boundary, native mapping
and returned common result. Direct seed input is supplied. These are added-entry
costs; they are not an old-entry regression gate or a statistically isolated
wrapper-overhead comparison. All six cases retain identical numeric digests.

| Prepared source | Calls per process | Round 1 minimum ns | Round 2 minimum ns |
|-----------------|------------------:|-------------------:|-------------------:|
| single | 1000 | 643.22 | 653.86 |
| generic | 1000 | 650.98 | 656.47 |
| layered | 1000 | 657.18 | 644.97 |
| joint_xccy | 1000 | 632.33 | 636.22 |
| staged_xccy | 1000 | 608.35 | 629.41 |
| dupire_9x2 | 1000 | 25989.47 | 26446.94 |

This evidence accepts local performance of the Python increment only. Exact
publication-head CI, Excel common bindings, full request/budget integration,
production MC measurement and the remaining AAD stages still require acceptance.
