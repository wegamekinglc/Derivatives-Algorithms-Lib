# Request-local portfolio worker reuse review

Verdict: **Comment Only** pending performance and final-head CI acceptance.
Scope is P02 in [PR #488](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/488);
P03 follows after this PR merges.

## Findings

No outstanding local correctness or style finding. The initial PR review found
an old F02 status passage contradicting the accepted ledger. That passage now
explicitly identifies intermediate acceptance and the completed #484/#487 work.
Remote thread resolution and re-audit remain publication gates.

## Design and verification

The coordinator keeps one owning result slot per original batch, submits at most
one job per admitted worker, drains all accepted work, and reduces original slots
in order. Each executing job owns model/RNG/scenario/evaluator capacity. Native
recordings and historical seeds restart per batch; prefix reversals remain per
original batch. Future admission uses the actual worker count, while gradient
slots continue to account for every original batch. Fixed admission includes the
new native optional state and passive model pointer.

RED: 32,785 paths split into five batches; the one-worker baseline submitted
five tasks instead of one. The new bound passes with one/four workers, both
interpreters, native/passive weighted calls and native widths one/three.

Release local checks pass 42 core cases selected by `*Portfolio*` and 43 public
portfolio cases. Six model families match fresh independent batch execution for
every model/private gradient and blocked lane, including the short last batch
and width-three padding. Historical private vectors cover Sobol/MRG32 and
Brownian bridge with non-contiguous worker-owned batches. Multi-batch requests
cover partial submission, worker failure, finite aggregate quotas and recovery.
Existing scalar-risk oracle tolerances and production numerics remain unchanged.
The new constant-price fixture uses `ASSERT_DOUBLE_EQ` for normalized doubles;
exact vector comparison had incorrectly rejected a one-ULP normalization result.

## Open acceptance

Run lifetime/profiling configurations and sanitizer/platform CI at the stable
head. Measure complete startup/multi-batch cost against merged #487 and retain
the frozen two-round, ten-pair, four-percent regression rule. Verify relevant
bindings and installed consumers. Reinspect all paginated Codacy annotations,
review threads and checks for the exact final SHA twice before merging.

The worker APIs are private implementation surfaces. Public requests, default
block policy and numerical coordinate shapes remain compatible. This routine
capacity/scheduling refactor does not meet the changelog's major-change criteria.
