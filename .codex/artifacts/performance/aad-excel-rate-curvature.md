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

## Results and provenance

Timed head: `b6f9d94db75d68af8f51bdeccd48421470a738c8`. Both isolated
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
| 1             | 106 / 105                | 922.06 / 909.43                          | 914.92 / 916.79                          | -1.37% / +0.20%             |
| 8             | 36 / 35                  | 2829.70 / 2816.43                        | 2854.39 / 2838.59                        | -0.47% / -0.55%             |

All 80 observations pass independent analytical PV, complete quote gradient,
signed finite-step products, replayed points and work/payload counter checks
before and after each timed batch. Batches total 8.458444867 seconds; the
shortest is 95.489899 ms. Processes are pinned to CPU 0; the shared-host final
load is 1.41/1.81/1.72. These deltas are informational costs of unequal owning
interfaces, not a comparable-contract +4% regression verdict or an Excel-host,
COM, UI or raw-export measurement. Zero old timing rows were repeated.

Later documentation/evidence commits may reuse these costs only after source,
dependency, archive, object and executable identity checks. Inspect actual
Windows runtime, exact-head checks and complete review bodies before merge.
