# Excel rate curvature scoped acceptance

Status: local correctness and scoped owning boundary costs accepted;
Windows runtime and publication gates pending.
Baseline: merged #536, `f55bc5596f3208c875f7d6b71ce1c51eb3df7b20`.

## Selected scope

The new `dal-excel/src/__ratecurvature.cpp` factory, settings and result
projections are the only new production caller. Its new header is consumed by
that source and focused tests. Common bump/numeric helpers and native/public
algorithms remain unchanged; prove this with source/dependency/archive hashes.

Select two complete deposit-portfolio requests with 1 and 8 quotes/trades,
three signed directions and full initial calibration, snapshot construction,
explicit replay and curvature/result copying. Both sides include initial
calibration; execution counters describe the seven curvature evaluations only.
Retain independent analytical checks of value, gradient/products and counters.
Use fresh rebuilt Excel/bridge binaries, identical native/public archives,
one worker, pinned CPU, calibrated batches of at least 25 ms, two rounds of ten
alternating process pairs per case and round minima. Retain 80 observations.

The C++ baseline and owning worksheet interface have unequal allocation/spill
contracts, so relative costs are informational. Preserve +4% thresholds only
for comparable contracts. Exclude unchanged native tape, PDE, RNG, Dupire,
MC/LSMC and Python timing matrices. Required exact-head CI remains applicable.
Any expanded selection needs a recorded new failure or caller coverage gap.

Review's Windows trade-gap admission repair changes the source/dependency
provenance and the freshly compiled object identity. Repeat only these same
two selected requests for the repair head; preserve the initial observations
outside the checkout. No additional numerical or timing matrix is selected.

## Results and provenance

Timed repair head: `8b77dda7`. Both isolated
worktrees use identical accepted public/core archives, Release `-O3 -DNDEBUG
-fPIC`, diagnostics/profiling OFF and one DAL worker. The core SHA-256 remains
`967f5e721f0ba67354b66f8eb14d8be9c4f7e422463bc8c412864491ee06997c`.
All affected Excel and bridge objects are rebuilt. The
[raw observations and build provenance](aad-excel-rate-curvature-results.json)
retain commits, compiler/configuration, object dependencies, executable hashes,
calibration captures and all process outputs. Native/public/Python source and
the common Excel helper remain unchanged.

| Quotes/trades | Repetitions, C++ / Excel | Round 1 minima, microseconds C++ / Excel | Round 2 minima, microseconds C++ / Excel | Relative cost, rounds 1 / 2 |
|---------------|--------------------------|------------------------------------------|------------------------------------------|-----------------------------|
| 1             | 113 / 88                 | 859.79 / 868.22                          | 867.47 / 867.97                          | +0.98% / +0.06%             |
| 8             | 36 / 39                  | 2676.94 / 2686.59                        | 2678.88 / 2693.18                        | +0.36% / +0.53%             |

All 80 observations pass independent analytical PV, complete quote gradient,
signed finite-step products, replayed points and work/payload counter checks
before and after each timed batch. Batches total 7.658490101 seconds; the
shortest is 76.381149 ms. Processes are pinned to CPU 0; the shared-host final
load is 0.68/0.72/1.13. These deltas are informational costs of unequal owning
interfaces, not a comparable-contract +4% regression verdict or an Excel-host,
COM, UI or raw-export measurement. The original two selected cases alone were
repeated for this repair; zero unrelated native/Python/Dupire cases were repeated.

Later documentation/evidence commits may reuse these costs only after source,
dependency, archive, object and executable identity checks. Inspect actual
Windows runtime, exact-head checks and complete review bodies before merge.
