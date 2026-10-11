# Scoped LSMC worksheet cost acceptance

Status: local acceptance passed; exact-head Windows/CI/review acceptance remains open.

Producer: 655a615701894859b4782fb0a3c17f647a7ea8dc, based on merged #538
061e4673440898ef59c20a091f5c70309985c1f4. Raw observations, commands,
source/dependency/object/executable hashes and normal provenance are retained in
[results](aad-excel-lsmc-curvature-results.json). Reproduction drivers are the
[LSMC bridge](aad-excel-lsmc-curvature-bridge.cpp) and
[observation driver](aad-excel-lsmc-curvature-cost.py).

## Changed paths and selected callers

The new LSMC adapter is covered by one Frozen and one RetrainedBump request:
one strictly future positive polynomial EXERCISE, explicit 64 training paths,
33 pricing paths, two signed mixed directions and actual outer steps .01/.005.
RetrainedBump retains its distinct inner relative policy step .002. Both use
the complete declared estimator. Native/public production is unchanged.

The focused shared MC input/observation header affects only the MC and LSMC
adapters. Its two existing MC cases are one-path mean and 33-path,
two-observation curvature. Both fresh head adapters were rebuilt. The accepted
owning-Excel baseline executable remains applicable to merged #538: every
production dependency, compiler and native/public archive hash was checked;
the final #538 repair changed tests only. Native-versus-Excel prior MC timings
are not used as a same-contract regression baseline.

## Results

Two rounds each use ten alternating fresh-process pairs, one pinned CPU and
one DAL worker. Every batch is calibrated to at least 25 ms. All 160 retained
observations pass independent value, every gradient coordinate and actual-step
product checks before and after timing. Actual pricing normals start after the
training block. The single positive final exercise has zero continuation, so
its policy-only contribution is zero in the independent RetrainedBump oracle.

| Case                  | Round | Base minimum us | Head minimum us | Change  | Acceptance                       |
|-----------------------|-------|-----------------|-----------------|---------|----------------------------------|
| LSMC Frozen           | 1     | 89.6205         | 93.9244         | +4.802% | Informational, unequal ownership |
| LSMC Frozen           | 2     | 91.4080         | 93.2042         | +1.965% | Informational, unequal ownership |
| LSMC RetrainedBump    | 1     | 614.7136        | 608.6628        | -0.984% | Informational, unequal ownership |
| LSMC RetrainedBump    | 2     | 602.4122        | 615.7456        | +2.213% | Informational, unequal ownership |
| Existing MC mean      | 1     | 4.8430          | 4.8660          | +0.474% | Pass, same contract within +4%   |
| Existing MC mean      | 2     | 4.6763          | 4.7332          | +1.216% | Pass, same contract within +4%   |
| Existing MC curvature | 1     | 661.8420        | 667.2807        | +0.822% | Pass, same contract within +4%   |
| Existing MC curvature | 2     | 665.0076        | 666.8217        | +0.273% | Pass, same contract within +4%   |

Measured total: 15.012276119 seconds; shortest retained observation:
34.040945 ms. No failed batch or omitted selected case is claimed as passing.
LSMC comparisons include the owning worksheet result and passive numeric
projections versus the accepted public owning evaluator; the +4% regression
gate applies only to the two equal MC contracts.

## Scope exclusions and limitations

Unchanged tape, RNG, rate, Dupire, PDE, generic reverse and Python matrices are
excluded locally because they include neither the new adapter nor the focused
helper. Shared-host minima mitigate noise; they do not establish universal
speedups. Portable calls exclude raw XLL/Excel/COM/UI. Required exact-head CI
still applies. Repeat only affected cases after a production repair; expand
only for a recorded failure or concrete caller gap. Documentation/test-only
repairs may reuse observations after dependency and executable checks.
