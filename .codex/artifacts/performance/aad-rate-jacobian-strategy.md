# Dense planning and financial strategy costs

Status: local acceptance complete; publication gates pending.
Baseline: accepted #510 squash `d18ebe4e70e7c2cb3a42cbcdc32341ac1b18e270`.
Current branch: `feature/rate-jacobian-measured-strategy`.

## Frozen scope and provenance

The changed production paths are direct dense numeric planning and the explicit
cached financial request. Compare two affected existing passive/joint requests,
four complete dense shapes/modes, their strict compressed cold/reuse alternatives,
four standalone plan-build costs and two new matching/stale cached requests.
The fourth mode is only width four for the 64-by-8 shape; there is no Cartesian
parameter matrix. MC, PDE, linear solves, quote risk, RNG and other modules reuse
accepted evidence. This is change-scoped financial evidence, not a rerun of the
scheduled whole-suite benchmark gate.

Baseline headers come from an isolated `git archive` of the accepted commit.
Baseline/head archives each contain 179 members; only `structuraljacobian.cpp.o`
and `ratecashflowpricing.cpp.o` differ. The other 177 members retain accepted bytes.
Archive SHA-256 values are:

- baseline: `300f22e9af384b28feafad5994bd47aba2a22ad98a0d3b96818b04ca5c677597`;
- head: `8799a707be4c5623478b466a0ba99b5d44ba312a78c44b7968e6c2d552be631c`.

Session evidence lives under
`/home/wegamekinglc/.cache/dal-aad-evidence-20261008/rate-strategy-performance-final/`:
`identity-proof.json`, `selection.json`, `results.json`, both build logs and every
raw stdout/stderr. Selection binds sources, independent references, fixtures,
binaries and archives with before/after hashes. The initial 720-observation
pre-fallback study remains under `rate-strategy-performance/`, with its fixture
reconstructed and verified against the original frozen digest.

Both standalone cost binaries use C++17, `-O3 -DNDEBUG -ffp-contract=fast`, native
AAD and diagnostics OFF. Sampling uses CPU affinity 0, `DAL_NUM_THREADS=4`, no
worker tasks, eight warmups and 128 requests per observation. Two independent
rounds alternate base/head order and keep ten process samples per side per round.
The machine is an i9-13900HX under Microsoft virtualization; the calibrated
paired policy accounts for this environment's timing noise.
Reduction is best of ten, with the existing sustained +4% rule. Compilation,
tests and source edits are stopped during sampling. Final sampling completes
760 observations in **8.95 seconds**, with **zero sustained regressions**.

## Comparable complete request results

Values are round-one/round-two minimum microseconds per complete request.

| Request                  | Baseline microseconds | Head microseconds | Head / baseline |
|--------------------------|-----------------------|-------------------|-----------------|
| Existing passive         | 1.728 / 1.729         | 1.715 / 1.738     | 0.993 / 1.005   |
| Existing joint native    | 7.426 / 7.185         | 7.247 / 7.130     | 0.976 / 0.992   |
| Layered 3-by-5 dense     | 5.753 / 5.780         | 5.472 / 5.455     | 0.951 / 0.944   |
| 64-by-8 dense scalar     | 104.661 / 106.076     | 75.581 / 74.651   | 0.722 / 0.704   |
| 64-by-8 dense width four | 112.367 / 113.390     | 80.284 / 79.969   | 0.714 / 0.705   |
| 64-by-2 dense scalar     | 77.857 / 75.987       | 65.195 / 66.326   | 0.837 / 0.873   |

The direct dense planner removes general full-support conflict construction.
The larger 64-by-8 complete request improves approximately 28–30%; the smaller
boundaries also improve. All eighteen baseline-comparable rows pass the sustained
gate, including the compressed and build-cost controls in the raw result.

## Strategy selection evidence

The layered fixture uses the accepted independent 3-by-5 analytic reference,
interleaved input coordinates and transitive bases. Its initial zero contribution
becomes nonzero at the fresh numeric point. The grouped 64-output fixtures use
eight or two independent flat curves, permuted input coordinates and varied
notionals, contract rates and lend/borrow signs. Every price and derivative is
checked against an independent discounted deposit cashflow formula; the other
matrix entries are checked as structural zeros.

Complete costs include current validation, cashflow/curve preparation, fresh
recording, every reverse block, recovery, result storage, numerical checks and
destruction. Cold costs additionally include capture and coloring inside the
request. Reused initial plans and market snapshots are prepared outside timing;
current identity is still recaptured inside every reused request.

| Shape / mode             | Plan build microseconds | Dense microseconds | Cold microseconds | Reused microseconds |
|--------------------------|-------------------------|--------------------|-------------------|---------------------|
| Layered 3-by-5 scalar    | 12.131 / 12.515         | 5.472 / 5.455      | 31.142 / 30.338   | 17.463 / 17.797     |
| Deposits 64-by-8 scalar  | 84.618 / 83.613         | 75.581 / 74.651    | 202.852 / 201.300 | 115.753 / 112.367   |
| Deposits 64-by-8 width 4 | 82.974 / 84.457         | 80.284 / 79.969    | 207.185 / 201.549 | 116.689 / 117.632   |
| Deposits 64-by-2 scalar  | 76.048 / 76.454         | 65.195 / 66.326    | 196.468 / 196.174 | 111.470 / 113.855   |

Compression reduces scalar directions from 3 to 2, from 64 to 8 and from 64 to
32, but complete reused requests still cost 3.19–3.26, 1.51–1.53 and 1.71–1.72
times dense respectively. Width-four reuse also loses at 1.45–1.47 times dense.
Because reused compression is already slower, no positive repetition count can
amortize the initial plan into a speedup on these shapes.

The new explicit matching cached request costs 17.853 / 17.236 microseconds;
the stale-plan dense fallback costs 17.506 / 17.859. These are informational new
paths without a historical baseline. Both validate current identity once and
return the correct complete matrix; their overhead is disclosed.

## Decision and limitations

Keep the ordinary dense default and explicit caller choice. These lightweight
cases do not justify an automatic compression cutoff, a universal claim across
all pricing/curve families, or vector width selection from direction counts.
Other workloads need their own complete-cost evidence before opting in.

Native `Number_` stores primal values and tape-node bindings, and records local
reverse derivatives. Its curve implementations explicitly instantiate double
and native reverse types. No executable tangent/forward adapter exists for this
closed path. Keep reverse capability; do not select theoretical two-direction
forward for the 64-by-2 fixture. Later mixed-mode work remains a separate stage.

The explicit cached overload supplies safe structural invalidation and dense
fallback rather than cost-based automatic tuning. This study establishes a
dense optimization and an honest strategy decision; it does not establish a
compressed production speedup. Numeric budgets cover result/direction payloads,
not tape capacity, descriptor metadata or RSS. CI/review remain pending.
