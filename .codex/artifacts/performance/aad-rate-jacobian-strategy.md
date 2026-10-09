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
- repaired head: `e7030e040b5d99c1b8e05aae62023c1505b9d7600273b97abb14ca2253b95840`.

Session evidence lives under
`/home/wegamekinglc/.cache/dal-aad-evidence-20261008/rate-strategy-performance-review-repair/`:
`identity-proof.json`, `selection.json`, `results.json`, both build logs and every
raw stdout/stderr. Selection binds sources, independent references, fixtures,
binaries and archives with before/after hashes. The initial 720-observation
pre-fallback study remains under `rate-strategy-performance/`, with its fixture
reconstructed and verified against the original frozen digest. The initial
760-observation post-fallback study remains under `rate-strategy-performance-final/`.
This final repair addresses operation labels and preserves every original sample.

Both standalone cost binaries use C++17, `-O3 -DNDEBUG -ffp-contract=fast`, native
AAD and diagnostics OFF. Sampling uses CPU affinity 0, `DAL_NUM_THREADS=4`, no
worker tasks, eight warmups and 128 requests per observation. Two independent
rounds alternate base/head order and keep ten process samples per side per round.
The machine is an i9-13900HX under Microsoft virtualization; the calibrated
paired policy accounts for this environment's timing noise.
Reduction is best of ten, with the existing sustained +4% rule. Compilation,
tests and source edits are stopped during sampling. Final sampling completes
760 observations in **9.86 seconds**, with **zero sustained regressions**.

## Comparable complete request results

Values are round-one/round-two minimum microseconds per complete request.

| Request                  | Baseline microseconds | Head microseconds | Head / baseline |
|--------------------------|-----------------------|-------------------|-----------------|
| Existing passive         | 1.833 / 1.918         | 1.835 / 1.816     | 1.001 / 0.947   |
| Existing joint native    | 7.464 / 7.954         | 7.796 / 7.773     | 1.045 / 0.977   |
| Layered 3-by-5 dense     | 6.169 / 6.537         | 5.859 / 5.751     | 0.950 / 0.880   |
| 64-by-8 dense scalar     | 112.654 / 114.319     | 76.528 / 82.881   | 0.679 / 0.725   |
| 64-by-8 dense width four | 124.604 / 119.067     | 89.285 / 83.776   | 0.717 / 0.704   |
| 64-by-2 dense scalar     | 80.168 / 81.346       | 69.559 / 72.983   | 0.868 / 0.897   |

The direct dense planner removes general full-support conflict construction.
The larger 64-by-8 complete request improves approximately 28–32%; the smaller
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
| Layered 3-by-5 scalar    | 13.196 / 13.203         | 5.859 / 5.751      | 32.778 / 32.465   | 18.376 / 18.693     |
| Deposits 64-by-8 scalar  | 89.017 / 89.467         | 76.528 / 82.881    | 218.690 / 216.000 | 124.505 / 122.687   |
| Deposits 64-by-8 width 4 | 89.418 / 87.657         | 89.285 / 83.776    | 227.435 / 223.648 | 129.526 / 128.831   |
| Deposits 64-by-2 scalar  | 79.506 / 82.349         | 69.559 / 72.983    | 205.435 / 212.729 | 124.495 / 120.843   |

Compression reduces scalar directions from 3 to 2, from 64 to 8 and from 64 to
32, but complete reused requests still cost 3.14–3.25, 1.48–1.63 and 1.66–1.79
times dense respectively. Width-four reuse also loses at 1.45–1.54 times dense.
Because reused compression is already slower, no positive repetition count can
amortize the initial plan into a speedup on these shapes.

The new explicit matching cached request costs 18.454 / 19.005 microseconds;
the stale-plan dense fallback costs 19.347 / 19.215. These are informational new
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
