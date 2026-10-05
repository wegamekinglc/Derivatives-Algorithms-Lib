# F01 common calibration Excel performance

Status: bounded Linux Excel-wrapper increment passes its unchanged regression
policy. The publication head's own cross-platform CI remains pending. Full F01
request integration and unresolved P01 production MC acceptance remain open.

## Inputs and protocol

Baseline is the frozen Excel wrapper object from accepted Python publication
`fb1a116bc8672e2b1698467e2d60b19de44babc3`, retained before any Excel edit in
`aad-calibration-excel-baseline-01/manifest.json`. Head production objects are
frozen in `aad-calibration-excel-head-objects-01/manifest.json`. Both consumers
use the same source, compiler/configuration and separately installed accepted
public library. Linking frozen wrapper objects isolates this binding increment;
no private inverse or replacement numerical implementation is used.

Evidence root:
`/home/wegamekinglc/.cache/dal-aad-evidence-20261004-8886c083/evidence`.
The consumer source/build configurations, runner, 40 raw processes and final
JSON are retained under the `aad-calibration-excel-` prefix. Canonical results
are `cost-paired-01/results.json`. All 1,477 pinned inputs remain unchanged
before/after sampling; all 640 numeric preflight checks pass. Nine accepted C++
gate executables and both current native archives remain byte-identical in
`native-identity-01.json`; do not describe that identity as a new gate run.

WSL2 Linux 5.15.167.4, x86-64 Intel i9-13900HX, GCC 15.2/C++17 Release,
Eigen ON, native architecture OFF, native AAD, lifetime/profiling OFF. Each
process uses CPU 4 and DAL_NUM_THREADS=4. No local build, correctness test,
source edit or Git/PR mutation runs during measurement. This is portable wrapper
measurement; actual MSVC registration compilation and Windows runtime CI are
separate correctness requirements, not Windows performance measurements.

Two rounds each run ten alternating processes per side (40 total). Each ordinary
row has ten warmup calls then five 100-call inner samples; legacy exclusion uses
20,000 calls and prepared common mapping uses 10,000. Reduce by minima within
process and within each round. Keep the original failure rule: both rounds above
+4%. All 48 old/default-entry comparisons pass. The 32 capture/common rows are
new-entry informational costs without a legacy no-regression counterpart.

Four providers (single, generic joint, joint XCCY and staged XCCY) cover N=8/16
and ANALYTIC/BUMPED. Each process checks complete native axes/state, tolerance
and every effective-inverse cell. Capture preserves exact canonical record
bytes; common mapping preserves all three contributions against the accepted
native C++ operation. Default old and blank-option constructors do not retain
records. Generic legacy dispatch retains its unavailable reason. Calibration,
fixture preparation and native references are outside timing. These checks do
not replace the independent pricing/recalibration oracles of the accepted core.

After sampling, the Windows module-state review fixes test utilities and removes
an unnecessary valid-text identifier allocation in a Windows-only validator.
Both measured Linux production objects remain byte-identical after rebuilding,
in `module-object-identity-01.json`. No measured production math changes.
The initial Excel head's SDK max-macro CI failure and reproduced RED remain.
Protecting the three affected test calls changes no production object or
performance input; compile corrected tests without the extra local NOMINMAX
define before inspecting the repaired head's own CI.
Retain original capture/common RED logs, fixture compile errors, the wrong native
coordinate-error expectation and first MSVC SDK include-order failures. The
first two functionality consumer runs overlapped a build and are not timing
acceptance evidence. No samples, widths, steps or acceptance tolerances change.

## Complete old/default-entry results

Times are ns/call, separately reduced for each confirmation round. The worksheet
blank-option entry compares against the corresponding old typed constructor.

| Case | Round 1 base | Round 1 head | Round 1 % | Round 2 base | Round 2 head | Round 2 % | Verdict |
|------|-------------:|-------------:|----------:|-------------:|-------------:|----------:|---------|
| generic/N16/analytic/old-exclusion | 134.78 | 139.87 | 3.772 | 134.95 | 137.89 | 2.179 | Pass |
| generic/N16/analytic/old-typed | 616648.58 | 616423.84 | -0.036 | 615095.62 | 616539.16 | 0.235 | Pass |
| generic/N16/analytic/worksheet-default | 616648.58 | 615613.01 | -0.168 | 615095.62 | 614489.78 | -0.098 | Pass |
| generic/N16/bumped/old-exclusion | 135.00 | 137.89 | 2.146 | 134.03 | 138.76 | 3.531 | Pass |
| generic/N16/bumped/old-typed | 596359.16 | 591162.67 | -0.871 | 592825.58 | 587772.67 | -0.852 | Pass |
| generic/N16/bumped/worksheet-default | 596359.16 | 590718.22 | -0.946 | 592825.58 | 588879.98 | -0.666 | Pass |
| generic/N8/analytic/old-exclusion | 134.16 | 138.48 | 3.227 | 134.21 | 138.63 | 3.295 | Pass |
| generic/N8/analytic/old-typed | 309093.43 | 313707.83 | 1.493 | 312469.58 | 310134.70 | -0.747 | Pass |
| generic/N8/analytic/worksheet-default | 309093.43 | 316370.70 | 2.354 | 312469.58 | 310860.04 | -0.515 | Pass |
| generic/N8/bumped/old-exclusion | 136.02 | 138.51 | 1.834 | 136.24 | 139.46 | 2.366 | Pass |
| generic/N8/bumped/old-typed | 303124.84 | 302834.02 | -0.096 | 303299.06 | 303255.16 | -0.014 | Pass |
| generic/N8/bumped/worksheet-default | 303124.84 | 303166.72 | 0.014 | 303299.06 | 303497.46 | 0.065 | Pass |
| single/N16/analytic/old-dispatch | 456216.97 | 458765.27 | 0.559 | 453256.19 | 453339.89 | 0.018 | Pass |
| single/N16/analytic/old-typed | 455491.20 | 455817.18 | 0.072 | 455181.20 | 454979.31 | -0.044 | Pass |
| single/N16/analytic/worksheet-default | 455491.20 | 460387.80 | 1.075 | 455181.20 | 456636.04 | 0.320 | Pass |
| single/N16/bumped/old-dispatch | 454366.52 | 455918.85 | 0.342 | 452368.97 | 453751.52 | 0.306 | Pass |
| single/N16/bumped/old-typed | 458938.75 | 455086.93 | -0.839 | 456480.45 | 453429.77 | -0.668 | Pass |
| single/N16/bumped/worksheet-default | 458938.75 | 454772.52 | -0.908 | 456480.45 | 453184.73 | -0.722 | Pass |
| single/N8/analytic/old-dispatch | 227388.79 | 229494.47 | 0.926 | 229634.45 | 230877.10 | 0.541 | Pass |
| single/N8/analytic/old-typed | 228452.64 | 233309.46 | 2.126 | 229832.12 | 229025.32 | -0.351 | Pass |
| single/N8/analytic/worksheet-default | 228452.64 | 230025.93 | 0.689 | 229832.12 | 229783.04 | -0.021 | Pass |
| single/N8/bumped/old-dispatch | 226564.61 | 228813.96 | 0.993 | 228424.30 | 227229.53 | -0.523 | Pass |
| single/N8/bumped/old-typed | 231335.05 | 228044.36 | -1.422 | 226869.65 | 228927.48 | 0.907 | Pass |
| single/N8/bumped/worksheet-default | 231335.05 | 228357.21 | -1.287 | 226869.65 | 227058.47 | 0.083 | Pass |
| staged/N16/analytic/old-dispatch | 1334260.64 | 1331466.07 | -0.209 | 1324023.87 | 1322285.97 | -0.131 | Pass |
| staged/N16/analytic/old-typed | 1322981.60 | 1326011.29 | 0.229 | 1318364.85 | 1325982.24 | 0.578 | Pass |
| staged/N16/analytic/worksheet-default | 1322981.60 | 1331743.71 | 0.662 | 1318364.85 | 1320299.76 | 0.147 | Pass |
| staged/N16/bumped/old-dispatch | 1345017.60 | 1330990.43 | -1.043 | 1327645.96 | 1330968.49 | 0.250 | Pass |
| staged/N16/bumped/old-typed | 1332509.57 | 1336382.29 | 0.291 | 1330533.31 | 1329730.72 | -0.060 | Pass |
| staged/N16/bumped/worksheet-default | 1332509.57 | 1328009.31 | -0.338 | 1330533.31 | 1326771.07 | -0.283 | Pass |
| staged/N8/analytic/old-dispatch | 753545.48 | 755263.96 | 0.228 | 751058.50 | 755420.83 | 0.581 | Pass |
| staged/N8/analytic/old-typed | 753995.72 | 758108.77 | 0.546 | 750718.57 | 751771.74 | 0.140 | Pass |
| staged/N8/analytic/worksheet-default | 753995.72 | 756419.47 | 0.321 | 750718.57 | 751668.80 | 0.127 | Pass |
| staged/N8/bumped/old-dispatch | 761276.86 | 759035.04 | -0.294 | 753490.60 | 754801.89 | 0.174 | Pass |
| staged/N8/bumped/old-typed | 753488.79 | 757692.13 | 0.558 | 751562.23 | 751562.45 | 0.000 | Pass |
| staged/N8/bumped/worksheet-default | 753488.79 | 759668.39 | 0.820 | 751562.23 | 754149.25 | 0.344 | Pass |
| xccy/N16/analytic/old-dispatch | 5999437.51 | 5965631.19 | -0.563 | 5972995.88 | 5946412.05 | -0.445 | Pass |
| xccy/N16/analytic/old-typed | 5934419.72 | 5985684.85 | 0.864 | 5951478.61 | 5961320.32 | 0.165 | Pass |
| xccy/N16/analytic/worksheet-default | 5934419.72 | 5978748.52 | 0.747 | 5951478.61 | 5959380.24 | 0.133 | Pass |
| xccy/N16/bumped/old-dispatch | 6018116.74 | 6027508.15 | 0.156 | 5972666.53 | 5975937.55 | 0.055 | Pass |
| xccy/N16/bumped/old-typed | 6043021.89 | 5992838.98 | -0.830 | 5964412.78 | 5965313.57 | 0.015 | Pass |
| xccy/N16/bumped/worksheet-default | 6043021.89 | 5972207.13 | -1.172 | 5964412.78 | 5986565.35 | 0.371 | Pass |
| xccy/N8/analytic/old-dispatch | 2322046.54 | 2330111.31 | 0.347 | 2325757.75 | 2314785.76 | -0.472 | Pass |
| xccy/N8/analytic/old-typed | 2321381.06 | 2330179.87 | 0.379 | 2320467.79 | 2322702.03 | 0.096 | Pass |
| xccy/N8/analytic/worksheet-default | 2321381.06 | 2331070.86 | 0.417 | 2320467.79 | 2318660.16 | -0.078 | Pass |
| xccy/N8/bumped/old-dispatch | 2290065.30 | 2295309.74 | 0.229 | 2295360.19 | 2310228.40 | 0.648 | Pass |
| xccy/N8/bumped/old-typed | 2308920.45 | 2298984.60 | -0.430 | 2290740.62 | 2307061.87 | 0.712 | Pass |
| xccy/N8/bumped/worksheet-default | 2308920.45 | 2302508.18 | -0.278 | 2290740.62 | 2294385.64 | 0.159 | Pass |

## New-entry costs

Capture times include native provenance construction and canonical record capture.
Prepared common mapping includes the owning wrapper result, and excludes
calibration, source/seed construction and numeric getters. This table measures
curve common mapping; accepted Dupire native costs remain separately documented.

| Case | Round 1 ns/call | Round 2 ns/call |
|------|----------------:|----------------:|
| generic/N16/analytic/capture | 615343.13 | 616245.01 |
| generic/N16/analytic/common | 276.55 | 273.70 |
| generic/N16/bumped/capture | 590536.68 | 588622.96 |
| generic/N16/bumped/common | 272.20 | 277.51 |
| generic/N8/analytic/capture | 311500.56 | 311435.44 |
| generic/N8/analytic/common | 205.01 | 205.51 |
| generic/N8/bumped/capture | 305204.94 | 304120.33 |
| generic/N8/bumped/common | 206.56 | 204.52 |
| single/N16/analytic/capture | 457006.20 | 456381.71 |
| single/N16/analytic/common | 284.93 | 281.22 |
| single/N16/bumped/capture | 455590.88 | 457391.24 |
| single/N16/bumped/common | 282.25 | 282.90 |
| single/N8/analytic/capture | 230190.03 | 229436.91 |
| single/N8/analytic/common | 207.76 | 208.14 |
| single/N8/bumped/capture | 227750.49 | 227060.49 |
| single/N8/bumped/common | 207.98 | 208.95 |
| staged/N16/analytic/capture | 1324843.71 | 1328305.27 |
| staged/N16/analytic/common | 297.89 | 300.08 |
| staged/N16/bumped/capture | 1323967.96 | 1328096.45 |
| staged/N16/bumped/common | 299.06 | 298.73 |
| staged/N8/analytic/capture | 758183.73 | 754177.27 |
| staged/N8/analytic/common | 205.96 | 205.70 |
| staged/N8/bumped/capture | 757292.02 | 761539.51 |
| staged/N8/bumped/common | 205.58 | 206.49 |
| xccy/N16/analytic/capture | 5827847.51 | 5781494.86 |
| xccy/N16/analytic/common | 1362.07 | 1350.50 |
| xccy/N16/bumped/capture | 5838279.98 | 5810396.95 |
| xccy/N16/bumped/common | 1360.72 | 1345.80 |
| xccy/N8/analytic/capture | 2332311.14 | 2315466.29 |
| xccy/N8/analytic/common | 543.27 | 550.70 |
| xccy/N8/bumped/capture | 2306921.30 | 2296808.52 |
| xccy/N8/bumped/common | 547.43 | 550.64 |

## Limits

The no-regression verdict is bounded to changed portable wrapper paths relative
to accepted fb1a116. It is not a comparison of the whole PR against current master,
a Windows timing claim or closure of production MC host-noise investigation.
Actual currency grouping and independently priced joint gradients are exercised
by the binding tests; full native quote bump/recalibration acceptance remains
unchanged. Final publication requires the new exact head's CI.
