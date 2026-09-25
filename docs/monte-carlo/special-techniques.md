# Monte Carlo Special Techniques

These techniques change path construction, the continuation-policy estimate,
or risk interpretation. The script Monte Carlo settings expose some of them;
the standalone Sobol generator has additional options. See
[simulation](simulation.md) and [LSM](lsm.md) for the valuation flows.

## Brownian Bridge and Sobol

Sobol is the default generator. Its low-discrepancy coordinates are turned
into normal draws by an inverse CDF. The standalone `NewSobolRSG` constructor
accepts `precise` and `polish` flags for the CDF correction. Script Monte Carlo
does not expose those flags through `MonteCarloSettings_`; its Sobol paths use
the generator defaults (`false, false`).
A Brownian bridge (`useBb_ = true`) assigns early Sobol dimensions to coarse
time-scale variation and fills intermediate points conditionally. This may
help a path-dependent payoff when its effective dimension is lower than the
raw time-step count, but payoff discontinuities and model details determine
the actual benefit. The [sampling guide](sampling.md) gives the construction
and path-seeking contracts.

## Randomized QMC for Exercise Products

The LSM driver can set `lsmcRqmcReplicates_ >= 2`. It fits one continuation
policy using a digitally shifted Sobol training stream, then prices that
policy on independently shifted pricing streams. Training and optional
validation are paid once; `nPaths` counts pricing paths **per replicate**.
For example, 4096 training paths, 1024 validation paths, 8192 pricing paths,
and four replicates require `4096 + 1024 + 4*8192` Sobol indices.

```cpp
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>

Dal::Script::MonteCarloSettings_ simulation;
simulation.lsmcTrainingPaths_ = 4096;
simulation.lsmcValidationPaths_ = 1024;
simulation.lsmcRqmcReplicates_ = 4;
simulation.lsmcTrainingSeed_ = 17;
simulation.lsmcPricingSeed_ = 29;
const auto result = Dal::Script::MCSimulation<double>(
    product, modelData, 8192, Dal::Script::ScriptValuationSettings_(), simulation);
```

`product` and `modelData` are the C++ script and model objects from the
[LSM example](lsm.md#c-example-and-path-counts). A seed requires replicate
mode. Digitally shifted integer points are mapped to bin midpoints before
inverse-normal conversion, so exact 0 and 1 are avoided. The shifts are
reproducible and are **not** nested Owen scrambling. `MCSimulation<double>`
returns a `SimResults_` with payoff sum and risks, not an error estimate.
To obtain the replicate-mean standard error, run `ExplainScriptSimulation`
with the same inputs and read `uncertainty.replicate_mean_se`, or use
`LsmcDiagnostics_::ReplicateMeanStandardError()` in the core driver. This
standard error measures pricing variation conditional on one fitted policy;
it excludes policy-fitting variability and approximation bias. With
deterministic Sobol, the payoff-dispersion standard error is descriptive,
not a calibrated QMC confidence interval.

## Held-Out Basis Selection

`lsmcBasisDegree_` defaults to 3 and is bounded to 1..8. Without
`lsmcValidationPaths_`, each exercise date uses that requested degree subject
to the regression's rank and sample-size guards. Setting a positive held-out
count makes the driver fit candidates of degree 1 through the maximum on
training paths, evaluate their continuation errors on disjoint validation
paths, and choose the smallest degree within one standard error of the best
validation loss. That standard error is a descriptive heuristic over the
validation values; it is not a QMC coverage guarantee. Final pricing paths
are never included in either fit or selection. Higher degree cannot recover
state variables omitted by the single-regressor basis. See the
[basis discussion](lsm.md#basis-and-regression-choice).

## Early-Exercise AAD

The default `Frozen` mode differentiates a fuzzy replay while treating fitted
continuation coefficients as constants. `RetrainedBump` retrains a hard policy
at positive and negative input bumps on common training paths, reprices those
policies on the unchanged base model and common pricing paths, and adds the
resulting policy secant to the frozen-policy adjoint. The PV is the same in
both modes; the latter costs roughly two extra fits and two value-only replays
per unconstrained input. Neither mode is an analytic derivative through the
regression solver. See [LSM AAD](lsm.md#early-exercise-aad) for the exact
recursion, bump rule, and nonsmooth cases.

## Execution and Validation Choices

Compiled evaluation lowers script nodes to an opcode stream. For LSM training,
`LsmcProcessor_` first removes dead assignments and unused future
observations, while retaining model dates and both IF-arm dependencies. Hard
pricing reuses a worker-local evaluator and stops script evaluation at the
first exercised event. The fixed-degree moment solve is the fast regression
path; rank loss invokes a pivoted QR fallback. The
[LSM guide](lsm.md#basis-and-regression-choice) gives its guards and memory
cost. Use the runnable
[American put comparison](../../dal-cpp/examples/american_put_mc/) and the
independent [Bermudan PDE test oracle](../../dal-cpp/test-support/bermudan_pde.hpp)
to assess pricing error separately from simulation dispersion. Benchmark
runs are handled by the separate scheduled workflow, not ordinary PR CI.
