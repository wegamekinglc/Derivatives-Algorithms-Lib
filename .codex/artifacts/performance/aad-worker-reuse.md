# Request-local portfolio worker reuse acceptance

P02 in [PR #488](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/488)
passes the local numerical and performance gates. Final-head platform, sanitizer,
Codacy and review acceptance is still required before merge. P03 starts afterward.

## Baseline and protocol

The baseline is merged #487, `bf52e3868a3e72a5f43b8c023a12aeee095a46cd`.
The cached accepted build was verified against 760 production, CMake and workload
Git blobs from that merge; dependency revisions, compiler flags and executable
hashes are recorded in `aad-worker-reuse-performance-environment-01.json`.
Both sides use GCC 15.2 Release, C++17, native AAD, Eigen enabled, native architecture
flags disabled, and lifetime/profiling diagnostics disabled. Measurements use
CPU affinity 0,2,4,6.

Every case uses two rounds of ten alternating baseline/candidate pairs and the
minimum duration in each round. Failure means a slowdown above 4% in both rounds.
This fixed rule allows one noisy round; it does not assert that every sample is
faster or prove a universal speedup. Pricing work, values, risks, widths and finite
capacity limits are checked independently from duration.

## Existing workloads

Nine formal benchmarks, curve calibration, existing scalar risk, weighted risk and
production MC/GSR/LSM gates all passed. After the final core and public changes,
eleven benchmark executables and both standalone API workloads were freshly
linked. All thirteen executable hashes match the originally measured candidates,
so their accepted timings remain applicable without repeating the measurements.

The relink records are `aad-worker-reuse-final-public-binary-reuse-01.json` and
`aad-worker-reuse-final-api-binary-reuse-01.json`; original gate commands and exit
statuses are in `aad-worker-reuse-performance-results-01.json`.

## Complete portfolio requests

The final matrix passes **37/37 cases**. Each process warms one complete request
and then times one complete request, including preparation and replay. It covers
257-path startup and 131,072-path throughput for one trade, compatible groups,
mixed meshes, distinct model owners and historical private state; native/passive
weighted and Jacobian routes; one/four workers; Jacobian widths one/three/eight;
five additional model families at 16,384 paths; and tree evaluation at 32,785 paths.
Every paired result matches values and risks within the existing `1e-10` check,
with matching requested paths, actual scenarios, evaluator calls and block widths.

Representative complete-request duration changes versus #487 follow. Negative
percentages indicate a faster candidate; these are case-specific observations.

| Case                                | Paths  | Workers | Round 1 | Round 2 |
|-------------------------------------|--------|---------|---------|---------|
| Compatible native weighted          | 257    | 4       | -13.41% | -7.77%  |
| Historical native weighted          | 257    | 4       | +2.83%  | +9.49%  |
| One-trade native weighted           | 131072 | 1       | -5.92%  | -5.16%  |
| Compatible native Jacobian, width 1 | 131072 | 4       | -16.20% | -8.46%  |
| Compatible native Jacobian, width 3 | 131072 | 4       | -1.58%  | -4.07%  |
| Compatible native Jacobian, width 8 | 131072 | 4       | -4.90%  | -6.42%  |
| Compatible passive weighted         | 131072 | 4       | -8.59%  | -4.16%  |

Raw pairs and results are retained under
`aad-worker-reuse-small-request-portfolio-all-01/results.json`, with the measured
working patch and source hash in `aad-worker-reuse-small-request-all-environment-01.json`.
Evidence resides in the persistent local `dal-aad-evidence` directory; source and
acceptance records are linked to the final published commit before merging.

## Failures and corrections

The initial full matrix failed four cases: two small requests and two large
Jacobian blocks. Intermediate runs remain retained, including two additional
small-request failures after the first corrections. The final implementation
shares the native numerical kernel and evaluator policies between entry routes,
uses fresh batch entry for jobs assigned one batch, dispatches small requests
directly in the coordinator, and uses the existing scalar adjoint channel for
width-one Jacobians. Matrix shape, root semantics, original batch reductions and
conservative preflight capacity are preserved.

All 39 affected core and 43 public cases pass, including scalar/vector agreement
for payoffs, direct aliases and historical prefix aliases across reused batches.
No existing tolerance or regression threshold was relaxed. Timing does not
separate every allocation/setup phase, and diagnostics/platform performance is
not inferred from the Release measurements.
