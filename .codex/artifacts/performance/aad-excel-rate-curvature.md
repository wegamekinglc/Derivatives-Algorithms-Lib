# Excel rate curvature scoped acceptance

Status: selection recorded before implementation; no measurements accepted yet.
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
