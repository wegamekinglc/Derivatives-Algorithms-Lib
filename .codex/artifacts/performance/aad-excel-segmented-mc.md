# Excel segmented MC scoped costs

Status: local acceptance passes; remote exact-head CI/reviews remain open.

Changed production paths are the new `dal-excel/src/__mccurvature.hpp/.cpp`.
The two new evaluation callers delegate to accepted public mean/curvature
entries. Shared native/public algorithms and existing bindings do not change.
Rebuild the affected adapter/bridge and prove archive/dependency applicability.

| Selected case                                                  | Changed caller covered                                          | Boundary                                                       |
|----------------------------------------------------------------|-----------------------------------------------------------------|----------------------------------------------------------------|
| Mean, one path, one future observation                         | Owning mean factory and detached numerical getters              | Single batch/segment                                           |
| Curvature, 33 paths, two future observations, segment size one | Curvature factory, stored base projection and detached products | Partial 32-path batch, multiple segments and signed directions |

Compare complete native public requests with owning typed worksheet requests,
including the same planned contracts, points, settings and numerical extraction.
Different ownership keeps both costs informational. Check independent finite-path
quadratic-stock values, all gradients and actual-step gradient secants before
and after every batch. Reuse the accepted #531 analytic construction, extending
Gaussian increments to the two observation dates.

Only after correctness passes, use isolated immutable source worktrees, identical
accepted native/public archives, fresh processes, one DAL worker and pinned CPU.
Calibrate batches to at least 25 ms. Retain two rounds of ten alternating process
pairs per case, minima, every raw sample and compiler/source/dependency/executable
identities. Reduce case count, not evidence per case.

Unchanged native tape, RNG, Python, rate, Dupire, PDE and LSMC matrices are excluded
locally. Invalid admission/history/empty shapes are correctness boundaries.
Actual raw XLL, Excel host, COM and UI costs are outside the portable timing.
Required applicable exact-head CI still applies. Repairs repeat affected selected
cases only when producer/dependency/executable identity changes.

## Accepted scoped observations

The [raw results](aad-excel-segmented-mc-results.json) retain 80 observations:
two cases, two rounds, ten alternating fresh-process pairs per round. Total
measured time is 6.198 seconds; the shortest batch is 43.625 ms. Every observation
passes its independent finite-path value, full gradient and actual-step secants
before and after timing. The one-worker process is pinned to CPU 0.

| Case              | Round | Native minimum (us) | Excel minimum (us) | Delta   |
|-------------------|-------|---------------------|--------------------|---------|
| One-path mean     | 1     | 3.503               | 5.030              | +43.61% |
| One-path mean     | 2     | 3.475               | 5.207              | +49.83% |
| 33-path curvature | 1     | 701.567             | 713.750            | +1.74%  |
| 33-path curvature | 2     | 686.997             | 711.135            | +3.51%  |

These comparisons quantify distinct ownership/projection costs. The mean adds
roughly 1.5–1.7 us per request; neither case supports an equal-contract regression
or speedup claim. Shared-host noise remains a limitation.

The frozen producer is `5d893852`, compared with merged `7d80c545`.
Both isolated builds reuse byte-identical accepted core/public archives and
compile the affected bridge/worksheet sources afresh. Compiler, dependency,
object, executable and RNG-oracle identities are retained with each capture.
The mathematical oracle extends accepted #531 to one/two annual observations,
with absolute Sobol first path 7 and no Brownian bridge. Normal input arrays and
their accepted Python module provenance are in the results JSON; they are drawn
outside measurement in a separate process.

To reproduce the driver, extract `normal_inputs` to a JSON file, build the
[bridge](aad-excel-segmented-mc-bridge.cpp) in native and
`DAL_EXCEL_MC_BOUNDARY` modes, and invoke the
[driver](aad-excel-segmented-mc-cost.py) with `--case 0` or `--case 1`,
`--bridge`, `--normals-file` and calibrated `--repetitions`.
