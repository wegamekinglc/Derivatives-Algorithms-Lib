# Request-local portfolio worker reuse review

Verdict: **Comment Only** pending final-head CI acceptance.
Scope is P02 in [PR #488](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/pull/488);
P03 follows after this PR merges.

## Findings

No outstanding local correctness or style finding. The initial PR review found
an old F02 status passage contradicting the accepted ledger. That passage now
explicitly identifies intermediate acceptance and the completed #484/#487 work.
The remote thread is resolved; final-head re-audits remain publication gates.

Codacy flagged complexity in the two new public test bodies at `323cd4be`.
Common numerical, work-count and fresh-batch gradient assertions are now focused
helpers; neither production code nor coverage is changed. Both affected cases
pass after the refactor. Codacy accepted `a1c196f5` with zero annotations;
the final performance-repair head must be checked again.

MSVC rejected the generic-lambda variable templates at `4b560eeb` with C3376
in all four Windows configurations and the Windows wheel build. The three shared policies now use
constexpr function templates returning the same stateless lambda types for each
scalar type. The common native kernel and numerical operations remain shared.
Width one also avoids allocating its unused vector root buffer. Additional
dispatch/passive-kernel extractions failed performance acceptance and were removed;
their failed/intermediate evidence is retained. All 82 affected cases and the
complete 37-case matrix pass on the portable final candidate. All thirteen legacy
executables match accepted hashes after fresh final linking. Final-head Windows
compilation and runtime acceptance remains required.

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

Release local checks pass 39 core cases selected by
`Portfolio*Test.*:ScriptPortfolioTest.*` and 43 public portfolio cases.
Six model families match fresh independent batch execution for
every model/private gradient and blocked lane, including the short last batch
and width-three padding. Historical private vectors cover Sobol/MRG32 and
Brownian bridge with non-contiguous worker-owned batches. Multi-batch requests
cover partial submission, worker failure, finite aggregate quotas and recovery.
Existing scalar-risk oracle tolerances and production numerics remain unchanged.
The new constant-price fixture uses `ASSERT_DOUBLE_EQ` for normalized doubles;
exact vector comparison had incorrectly rejected a one-ULP normalization result.

Initial complete-request timing failed four portfolio cases. The native batch
kernel and evaluator policies are now shared between direct and worker entries;
single-batch jobs use the direct entry. Width one uses the existing scalar adjoint
channel with the same row shape and root semantics. A new cross-batch case compares
its payoff, direct constant alias and historical prefix risks with width-two
vector recordings, including signed/zero output weights. Existing preflight
reserves conservative vector-width-one capacity; runtime allocates no additional
storage for this route. All 82 affected numerical/resource cases pass.

Failed and intermediate timing runs are retained. The final full 37-case portfolio
matrix passes after the corrections, including the two additional small-request
failures found by the wider run. All thirteen legacy executables match their
accepted measured hashes after fresh relinking. See the active
[performance acceptance](../performance/aad-worker-reuse.md) for protocol,
representative durations and the limits of the result.

## Open acceptance

Run lifetime/profiling configurations and sanitizer/platform CI at the stable
head. Complete startup/multi-batch cost passes against merged #487 under
the frozen two-round, ten-pair, four-percent regression rule. Verify relevant
bindings and installed consumers. Reinspect all paginated Codacy annotations,
review threads and checks for the exact final SHA twice before merging.

The worker APIs are private implementation surfaces. Public requests, default
block policy and numerical coordinate shapes remain compatible. This routine
capacity/scheduling refactor does not meet the changelog's major-change criteria.
