# Excel Dupire curvature scoped acceptance

Status: correctness accepted locally; paired boundary measurement pending.
Baseline: merged Excel PDE commit `c1068d991c1547c6b7d35bf490b49a61de574003`.

## Selection before measurement

| Changed path                           | Actual caller and selected evidence                                                                                    |
|----------------------------------------|------------------------------------------------------------------------------------------------------------------------|
| `dal-excel/src/__curvaturerequest.cpp` | New typed request construction and copied directions/steps in both complete financial requests                         |
| `dal-excel/src/__curvatureinput.hpp`   | Raw Windows kind guards; real XLL runtime, without claiming portable timing measures coercion                          |
| `dal-excel/src/__curvaturerows.hpp`    | New Dupire result numeric spills and logical shapes; both selected requests                                            |
| `dal-excel/src/__dupirecurvature.cpp`  | Request, plan, execution and owning result projection at 17 and 257 paths, three signed directions and six full quotes |

The new headers have only the two new source modules and their tests as direct
production callers. Native/public/Python sources and existing Excel headers are
unchanged. Reuse the accepted installed core/public archives only after matching
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

## Pending acceptance

Record source/executable/dependency hashes, calibrated counts and raw minima
before publication. Inspect completed Windows export/registration runtime and
all current-head checks/reviews before either final audit or merge.
