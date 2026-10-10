# Excel segmented MC scoped costs

Status: selection before implementation/measurement; no timing acceptance yet.

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
