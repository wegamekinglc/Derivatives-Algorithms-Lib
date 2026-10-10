# Excel Dupire curvature scoped acceptance

Status: local correctness and scoped paired boundary costs accepted;
Windows runtime and publication gates pending.
Baseline: merged Excel PDE commit `c1068d991c1547c6b7d35bf490b49a61de574003`.

## Selection before measurement

| Changed path                           | Actual caller and selected evidence                                                                                    |
|----------------------------------------|------------------------------------------------------------------------------------------------------------------------|
| `dal-excel/src/__curvaturerequest.cpp` | New typed request construction and copied directions/steps in both complete financial requests                         |
| `dal-excel/src/__curvatureinput.hpp`   | Raw Windows kind guards; real XLL runtime, without claiming portable timing measures coercion                          |
| `dal-excel/src/__curvaturerows.hpp`    | New Dupire result numeric spills and logical shapes; both selected requests                                            |
| `dal-excel/src/__dupirecurvature.cpp`  | Request, plan, execution and owning result projection at 17 and 257 paths, three signed directions and six full quotes |

The new headers have only the two new source modules and their tests as direct
production callers. The added public bump header forwards the existing native
contract without changing any pre-existing public/native/Python source or
Excel header. Reuse accepted installed core/public archives only after matching
their hashes and source provenance; rebuild all affected Excel objects.

Exclude tape, PDE, RNG, interpolation, rate and unrelated Python timing matrices:
none of their implementation or pre-existing callers changes. Tree/compiled,
empty rows, selected reports and failures receive focused correctness coverage;
they do not add a timing Cartesian product. Scheduled monitoring and required
exact-head CI still apply.

## Method

Use isolated detached baseline/head sources, identical installed native/public
archives and compiler settings. Compile the same financial bridge for a public
C++ baseline and typed Excel head. Both construct calibration, model, scalar
product, full directions and plan, then perform seven quote-gradient valuations.
The head additionally constructs Excel handles and copies result spills. Check
all raw gradients/products against the discounted quadratic and verify paths,
evaluation count and 720-byte payload after warmup and every timed batch.

Pin each fresh process to one CPU, use one DAL worker, calibrate batches toward
100 ms, and require at least 25 ms per observation. For each selected case retain
two rounds of ten interleaved process pairs, alternating first position, and
reduce each round with minima. Retain all 80 observations and process captures.
The machine is shared, so record host noise.

These are unequal ownership/spill contracts. Cost differences are informational;
the calibrated +4% regression rule remains applicable only to comparable
contracts. Do not present typed Linux timing as Excel-host, COM or raw-XLL costs.

## CI repair scope

The first CI configure exposed a direct-core include in the common binding.
Add a thin installed public forwarding header, then reproduce RED/GREEN with
the repository's actual CMake boundary function. Codacy also requires extracting
the raw Windows analytic check and the Python observation validator. These
repairs do not change native algorithms. Rebuild affected interface binaries and
repeat only the same two selected requests; retain the initial observations
separately and do not expand the performance case set.

## Results

The timed repaired head is `30e9d9f0`; baseline/head use the same accepted installed
core/public archives. The core SHA-256 is
`967f5e721f0ba67354b66f8eb14d8be9c4f7e422463bc8c412864491ee06997c`.
All changed Excel and bridge objects are rebuilt in isolated worktrees with
`-O3 -DNDEBUG -fPIC`, diagnostics/profiling OFF and one worker. Raw
[results and provenance](aad-excel-dupire-curvature-results.json) retain both
commits, compiler/configuration, every object dependency and executable hash,
all process outputs and the four calibration captures.

| Paths | Repetitions, C++ / Excel | Round 1 minima, microseconds C++ / Excel | Round 2 minima, microseconds C++ / Excel | Relative cost, rounds 1 / 2 |
|-------|--------------------------|------------------------------------------|------------------------------------------|-----------------------------|
| 17    | 43 / 41                  | 2479.64 / 2560.36                        | 2441.42 / 2475.30                        | +3.26% / +1.39%             |
| 257   | 26 / 26                  | 4004.58 / 4023.19                        | 4006.63 / 3995.96                        | +0.46% / -0.27%             |

All 80 fresh-process observations pass the financial/count checks. Their timed
batches total 8.766 seconds; the shortest is 101.487 ms. Measurements use CPU 0
on the shared host, with recorded final load 2.62/1.97/1.54. The deltas are
informational boundary costs, not a comparable-contract regression verdict.
Zero unrelated previously accepted timing rows were repeated.

Inspect completed Windows export/registration runtime and all current-head
checks/reviews before either final audit or merge. A documentation-only final
commit may reuse these costs after verifying unchanged source/dependency hashes.
