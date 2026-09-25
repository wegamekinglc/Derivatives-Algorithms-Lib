# Least-Squares Monte Carlo (LSM)

LSM approximates the value of waiting at each exercise date from simulated
continuation cashflows. It fits a decision policy on one path block, then
applies that frozen policy to a disjoint pricing block. This avoids valuing a
policy on the same paths that were used to fit it, although it does not remove
basis approximation or finite-training bias. The implementation prices
`EXERCISE` statements in the C++ script engine; see
[simulation](simulation.md) for ordinary non-exercise products.

Products containing `EXERCISE` divert from the plain double driver to the LSMC
driver (`MCLsmcSimulation` in `dal-cpp/dal/script/lsmc.cpp`), in both tree and
compiled execution. `nPaths` is the pricing count; `nTrainingPaths` is
`simulation.lsmcTrainingPaths_` when set, otherwise `nPaths`. Training can use fewer or
more paths than pricing. `simulation.lsmcValidationPaths_`, when set, adds a separate
held-out block for selecting degree; otherwise the requested degree is used directly.
Fixing both counts keeps the fitted policy unchanged when the pricing count changes.
Valuation runs in three phases over a fixed,
thread-count-independent batch layout: batches of at most 8192 paths
indexed by batch order, with per-batch contributions reduced in batch-index
sequence, so PV, the frozen coefficients, and every exercise rate are bitwise
invariant across thread counts.

## C++ Example and Path Counts

The runnable [American put comparison](../../dal-cpp/examples/american_put_mc/american_put_mc.cpp)
constructs a product with `EXERCISE` on each allowed date and calls the core
C++ simulation for both hard pricing and AAD. This excerpt follows its
settings and call pattern:

```cpp
#include <dal/platform/platform.hpp>
#include <dal/script/simulation.hpp>

Dal::Script::MonteCarloSettings_ simulation;
simulation.rsg_ = "sobol";
simulation.lsmcBasisDegree_ = 3;
simulation.lsmcTrainingPaths_ = 4096;
const auto hard = Dal::Script::MCSimulation<double>(
    product, modelData, 32768, Dal::Script::ScriptValuationSettings_(), simulation);
simulation.enableAad_ = true;
const auto aad = Dal::Script::MCSimulation<Dal::AAD::Number_>(
    product, modelData, 32768, Dal::Script::ScriptValuationSettings_(), simulation);
const double hardPv = hard.aggregated_ / 32768.0;
const double aadPv = aad.aggregated_ / 32768.0;
```

`product` is a `ScriptProductData_` and `modelData` is a BS model handle as in
the executable example. Core `aggregated_` is a sum over pricing paths; its
`risks_` are already normalized. The public `ValueByMonteCarlo` facade returns
the normalized `PV` and `d_` values. The API default for an unset `lsmcTrainingPaths_`
is **the pricing path count**. The example's command-line convenience default
is different: it computes `max(4096, pricing_paths / 8)` and passes that
number explicitly into the setting. These are separate contracts.

| Path block | Setting | Default in core API | Used for |
|------------|---------|---------------------|----------|
| Training | `lsmcTrainingPaths_` | `nPaths` | Regression coefficients |
| Validation | `lsmcValidationPaths_` | Absent | Optional degree selection |
| Pricing | `nPaths` | Required argument | Reported PV and exercise rates |

All three blocks occupy distinct Sobol index ranges. With RQMC replicates,
`nPaths` is per pricing replicate; see
[special techniques](special-techniques.md#randomized-qmc-for-exercise-products).

- **Phase A** generates the first `nTrainingPaths` Sobol paths and evaluates the script forward;
  `EXERCISE` is a no-op forward. Each batch records, per path, the payments of
  each event's `PAYS` into the selected payoff receiver and, per exercise
  date, the regressor observation, the exercise value, and the condition
  indicator (hard 0/1 in double mode).
- **Phase B** walks the events backward. The holding value is
  $H_k = p_k + D_{k,k+1} W_{k+1}$ — the day's `PAYS` enter the hold side — and
  on each exercise date the driver regresses $H_k$ on the in-the-money
  ($h_k > 0$) condition-true path subset over the z-normalized monomial basis
  $z=(x-\hat\mu)/\hat\sigma$ of
  degree `simulation.lsmcBasisDegree_` (default 3). The normal equations carry
  an explicit relative ridge $A + \lambda\,\mathrm{diag}(A)$, $\lambda=10^{-12}$.
  The Gram matrix is assembled from the `2d+1` scalar moments, reducing the
  path-dependent assembly work from $O(Md^2)$ to $O(Md)$ for $M$ training paths. Before applying
  ridge, a Cholesky pivot check on the column-scaled Gram matrix detects
  collinearity that diagonal ratios alone cannot detect. Too few condition-true
  paths ($M < 10(d{+}1)$) or the $\hat\sigma$ floor yield a constant fit. A Gram
  diagonal ratio above $10^{12}$ or a scaled Cholesky pivot at most $10^{-12}$
  triggers a rank-revealing fallback: a
  column-pivoted, twice-reorthogonalized QR fit on the original design rows
  avoids squaring the condition number. Rank loss lowers the effective degree
  to the supported polynomial subspace before falling back to a constant.
  Only an unrecoverable fallback reports `IllConditioned` and a constant fit.
  These guards are not an exact condition-number estimate. Included non-finite observations and targets
  are rejected. Constant fits bypass normalization during prediction. A path
  exercises when its condition holds, $h_k > 0$, and $h_k > C_k(z_k)$
  strictly; exercise replaces the day's and all later payments.
- **Optional held-out selection** records the next `lsmcValidationPaths_` paths
  and applies already selected later-date policies during backward induction.
  For each exercise date, candidate degrees 1 through `lsmcBasisDegree_` are
  fitted on training paths. The smallest degree within one standard error of
  the best held-out continuation mean-squared error is selected. This is a
  descriptive dispersion heuristic on deterministic QMC points, not a
  calibrated statistical confidence interval. Validation
  targets and final pricing payoffs never enter the training fit.
  If no validation path is eligible on a date, the selector fits the requested degree
  and reports a null validation loss rather than a fabricated zero.
- **Phase C** values the frozen policy on the final `nPaths` Sobol paths in
  deterministic mode,
  starting at `SkipTo(nTrainingPaths + nValidationPaths)`. Training, validation,
  and pricing blocks do not overlap; the deterministic total generated path count is
  `nTrainingPaths + nValidationPaths + nPaths`, plus one
  numeraire probe. Training and validation storage is released
  first. Hard pricing shares the immutable compiled program and reuses one
  evaluator, path generator, and scalar decision state per worker across batches;
  it does not allocate per-exercise-date recording rows.
  Exercise dates are scanned
  in order; the earliest date whose condition holds with $h_k > 0$ and
  $h_k > C_k(z_k)$ under the
  frozen coefficients pays $h_k$ discounted at that date's numeraire on top
  of the payments accumulated before it. A path that never exercises keeps
  its terminal payoff — zero for `EXERCISE`-only products — with
  every payment discounted at its own event date. Script evaluation stops after
  the first exercised event; model path generation still covers the full grid.
  PV is the mean over pricing paths only.

The combined training, validation, and pricing count must fit the 32-bit Sobol sequence
(at most `2^32 - 1` paths). The core `size_t` entry points reject larger
ranges with `InvalidPathCount` before allocating training storage.

The separate blocks avoid evaluating the policy on its own regression samples.
By default they remain deterministic QMC blocks, not statistically independent
randomized replicates. `LsmcDiagnostics_::StandardError()` is a descriptive
payoff-dispersion measure, not a calibrated QMC confidence interval.

Setting `lsmcRqmcReplicates_` to at least 2 freezes one policy from a digitally
shifted training Sobol stream and prices it on that many independently shifted
pricing streams. `nPaths` is the pricing count **per replicate**; the total
pricing budget is `lsmcRqmcReplicates_ * nPaths`. The training and optional
validation budgets are paid once. Every stream uses a distinct, non-overlapping
Sobol index block; the training/validation stream shares a shift but has
disjoint blocks. A role bit, a 31-bit user seed, and the replicate index form
a unique 64-bit scramble key; SplitMix64 expands each key into one 32-bit XOR
mask per Sobol coordinate. Shifted integer points are mapped to bin midpoints
before inverse-normal transformation, avoiding 0 and 1. Omitted training and
pricing seeds use zero, but role separation keeps their stream identities
distinct. Explicit seeds without a replicate count, and non-Sobol RQMC settings,
are rejected. Unset RQMC settings retain the original deterministic Sobol path
and numerical results.

The RQMC price is the mean of replicate prices. Its
`ReplicateMeanStandardError()` is the sample standard deviation of those prices
divided by the square root of the replicate count. This error concerns pricing
**conditional on the single frozen policy**; it excludes retraining variation,
continuation approximation error, and optimal-policy bias. AAD replays the
same pricing replicates against the same policy and averages their risks.
Digital shifts are a reproducible pseudorandomization, not nested Owen
scrambling. A finite number of replicates gives an estimated error bar, not a
guaranteed confidence interval.

The standalone [coverage experiment](../../dal-python/benchmarks/lsmc_rqmc_coverage.py)
uses 256 pricing-seed groups, eight replicates per group, 8,192 fixed-policy
training paths, and 2,048 pricing paths per replicate. With a mean ± two
replicate-standard-error band, 233/256 European-put intervals covered the
analytic value and 226/256 two-date Bermudan intervals covered an independent
Crank–Nicolson PDE value. The refined PDE grid changed that value by 0.000179,
and the one-date PDE differed from the analytic European price by 0.000056;
the mean Bermudan price exceeded it by 0.000541. These are observed coverages
for this policy, budget, and band, not universal nominal-coverage claims.

## Script Liveness and Recording

Preparation runs `LsmcProcessor_`, a backward liveness visitor with local
dispatch, before compiling the recording program. It does not extend the
global visitor hierarchy or add storage to ordinary script nodes. It removes
overwritten or unused assignments and empty branches, and unions the
dependencies of both IF arms. Exercise expressions,
conditions, and the selected receiver's cashflows remain roots. Other receivers
remain live when the script reads them, but their payments do not enter the
continuation value. Observation dates and the historical program are retained.
After liveness, unused future model observation outputs are removed from the
sample definitions and live request slots are compacted; the model is
reallocated and reinitialized when this happens. Keeping all dates preserves
Sobol dimensions and path-dependent model evolution. Historical fixings and
model-index support are resolved before pruning, including references in dead
statements.
The payoff receiver must accumulate `PAYS` only, with an optional literal-zero
initialization before any future payment. Historical receiver assignments
must also be literal zero: a parameter-dependent expression can have zero
value while carrying nonzero risk. Other assignments raise
`UnsupportedExercisePayoff`, since they invalidate the additive cashflow
recursion. Historical `PAYS` remain expired and do not seed the receiver.

## Basis and Regression Choice

The default cubic standardized polynomial is a small one-dimensional approximation,
not a universally optimal basis. Unweighted polynomial families of the same
degree span the same space; changing their names does not add information.
Increasing degree can increase variance and worsen conditioning. QR orthogonalizes
the design columns for the solve but does not change the approximation span.
The held-out selector is opt-in because it needs additional simulated paths and
up to eight fits per exercise date; the fixed-degree moment solve remains the
default. Use independent final pricing and a suitable benchmark to assess the
selected policy. In particular, a single spot
regressor does not capture all relevant state for general path-dependent claims
(for example, a running average or barrier state). Such scripts can be evaluated,
but their continuation approximation omits that additional state.

The regressor is the product's single model-sourced future observation (the
same index binding as `FIX`); an unbound `SPOT()` regressor keeps a null
`regressor_index` in diagnostics. Exercise dates must be strictly after the
evaluation date (`UnsupportedExerciseDate`), and history-only preparation
cannot value an exercise product (`UnsupportedExecutionMode`). Preparation
allows only `rsg = "sobol"`
for exercise products (`UnsupportedRsgForExercise`): the driver seeks each
batch's first path with `SkipTo`, and only Sobol's `SkipTo` reconstructs it
exactly — see
[Random and path generation](sampling.md#path-seeking).

Training memory is roughly `nTrainingPaths × (nPaysEvents + 2 × nExerciseDates + 1) × 8B`
for payments, regressor/exercise rows, and the backward working vector, plus
one byte per path for each conditional exercise date and the inclusion mask.
Enabling selection adds analogous storage for `nValidationPaths` until backward
induction finishes.
Hard pricing keeps the pre-exercise receiver value and terminal payoff in its
worker-local evaluator state. Reduce the training path count or event count to
stay inside a memory budget.

AAD valuation of exercise products uses the fuzzy driver described below. The
per-exercise-date statistics are observable through
the [simulation diagnostic](../methodology/script_engine.md#simulation-diagnostic-full-valuation-with-exercise-statistics),
and the acceptance suite anchors both engines against a test-only Bermudan
PDE pricer (`dal-cpp/test-support/bermudan_pde.hpp`; the library PDE itself
stays European-only). The runnable
[`dal-cpp/examples/american_put_mc/`](../../dal-cpp/examples/american_put_mc)
compares the European closed form with ordinary Monte Carlo and its AAD
version, and prices two-date Bermudan and weekly-exercise puts with both
hard valuation and AAD. It also prints the diagnostic.
Run `american_put_mc [pricing_paths [training_paths]]`
to set the two counts independently. Pricing defaults to 131,072 paths; when
`training_paths` is omitted, it defaults to `max(4096, pricing_paths / 8)`
using integer division (16,384 paths with default pricing). For example,
`american_put_mc 262144` uses 32,768 training paths, while
`american_put_mc 262144 4096` explicitly uses 4,096. The result table reports
`Pricing paths` and `Training paths` for each exercise Monte Carlo row.
European Monte Carlo
uses a terminal payment without regression, so its training count is `-`;
closed-form and PDE rows show `-` for both counts. AAD rows report spot,
volatility, rate, and dividend sensitivities. AAD and hard valuation use the
same pricing count; both exercise AAD runs and the diagnostic also use the
configured training count.

## Early-Exercise AAD

Products containing `EXERCISE` divert to the fuzzy LSMC driver
(`MCLsmcAadSimulation` in `dal-cpp/dal/script/lsmc.cpp`), in both tree and
compiled execution. The forward storage and backward regression phases run
exactly as the double driver, so the continuation coefficients `C_k` are the
thread-count independent hard-decision artifact. Each replay worker then
generates its batch from the disjoint pricing block on its own tape, records the per-event payments and the
per-date exercise values and fuzzy condition degrees, and prices the path with
the recursive blend

$$V_k = d_k h_k + (1 - d_k)(p_k + D_{k,k+1} V_{k+1}),\qquad d_k = \mathrm{CSpr}(h_k - C_k(z_k), \varepsilon)\cdot\mathrm{CSpr}(h_k, 0, \varepsilon)\cdot c_k,$$

where $c_k$ is the fuzzy condition degree (1 when unconditional), the second
factor is the one-sided $h_k > 0$ exercise gate (degree 0 on the $h_k = 0$
atom, matching the hard rule), the discount
ratios come from the path's own numeraires, and $\varepsilon$ is the exercise
statement's smoothing width resolved against `simulation.smooth_`. The blend
is carried in event-date units, so the recursion ends with the explicit
division by the first event's numeraire. As $\varepsilon \to 0$ the decision
degrees degenerate to hard indicators and the fuzzy path value converges to
the hard-mode payoff.

By default (`lsmcPolicyRiskMode_ = "Frozen"`), the regression coefficients and
normalization enter the replay as passive tape constants. The harvested adjoint
is the exact pathwise gradient of the *frozen-policy* fuzzy price functional.
It excludes the response of the fitted continuation to market parameters.
`lsmcPolicyRiskMode_ = "RetrainedBump"` adds that response for model parameters
and script constants: for each input, it retrains the hard policy at positive and negative bumps
on the same training and optional validation paths, then reprices both policies
with the unchanged base model and identical disjoint fuzzy pricing paths. The
common-path policy secant is added to the frozen-policy adjoint. The step is
`lsmcPolicyBumpRelative_ * max(1, abs(input))` (default `1e-3`); constrained
spot/volatility parameters at their lower boundary use a forward policy secant.
The price itself is identical in both modes. Historical script state is replayed
when a script constant is bumped. This is a finite-step sensitivity of the fitted,
smoothed estimator, not an analytic derivative through the regression solver.
Changing path inclusion, selected degree, rank fallback, or a hard exercise
decision can make the policy response nonsmooth and bump dependent. The policy
retraining and value-only replays add roughly two training fits and two pricing
passes per unconstrained input; they do not add an AAD tape for those
passes. Bump tests that retrain the entire fuzzy valuation measure the full
finite difference, which also includes finite-step curvature of the direct
payoff response. An envelope argument at an optimal stopping rule does not
provide a general second-order error guarantee for an approximate, smoothed
policy. Adjoint accumulation and reduction follow the
same thread-count independent batch layout and batch-index ordering as the
double driver, so PV and every `d_<param>` are bitwise invariant across
thread counts in AAD mode as well, and historical fixings replay into the
seed exactly as for non-exercise products.

## LSMC Policy Sensitivity Validation

`dal-python/benchmarks/compare_lsmc_policy_sensitivities.py` reproduces a
six-case study with disjoint training/pricing streams, two independently
digitally shifted pricing replicates per outer run, and eight independent
training/pricing seed pairs. Each full-retraining finite difference shares
its seed pair between bumps. The European case uses analytic Black-Scholes
Greeks; regular Bermudan cases use an independent Crank-Nicolson PDE with
exercise-date projection and CRR cross-check. The low-volatility case uses
CRR as its reference because the PDE's central spatial stencil becomes
convection dominated there. The script reports the hard-price error to the
oracle (fitting plus residual QMC), fuzzy-minus-hard smoothing bias, policy
contribution, empirical standard error across independent seed pairs, separate
hard-price variation when only training or only pricing seeds change, and the
change when the spot bump is halved. In the ATM case the training-only and
pricing-only hard-price standard errors were `0.00022` and `0.00150`;
the latter is close to the observed `0.00180` hard-price difference from the PDE.
These quantities should be read separately: a closer Greek to the full-retraining bump is not necessarily
closer to the continuous-model stopping oracle.

For the default study (4,096 pricing paths per replicate, 4,096 training paths
except 256 in the sparse case, smoothing `0.1`, relative bump `0.001`, compiled
execution), representative spot sensitivities are:

| Case                     | Frozen   | RetrainedBump | Full-retrain bump | Independent reference | Bump SE |
|--------------------------|----------|---------------|-------------------|-----------------------|---------|
| European                 | -0.33418 | -0.33418      | -0.33416          | -0.33416 analytic     | 0.00001 |
| Bermudan ATM             | -0.36389 | -0.36404      | -0.36464          | -0.36246 PDE          | 0.00066 |
| Bermudan deep ITM        | -0.89919 | -0.90067      | -0.89927          | -0.89615 PDE          | 0.00371 |
| Bermudan deep OTM        | -0.07172 | -0.07120      | -0.06951          | -0.06965 PDE          | 0.00066 |
| Bermudan sparse training | -0.36311 | -0.36269      | -0.36236          | -0.36246 PDE          | 0.00333 |
| Bermudan low volatility  | -0.74700 | -0.74700      | -0.73789          | -0.75503 CRR          | 0.00002 |

`dal-cpp/benchmarks/script_mc_perf/script_mc_perf.cpp` accepts `retrained` on
the `--lsmc-replay` AAD profile. On a Linux i9-13900HX, GCC 15.2 Release,
four DAL threads, 1,024 training paths, 2,048 pricing paths, the 1CD
Black-Scholes compiled profile gave these best-of-five interleaved process
measurements (milliseconds and peak RSS in KiB):

| AAD backend | Frozen ms | Retrained ms | Time ratio | Frozen RSS | Retrained RSS |
|-------------|-----------|--------------|------------|------------|---------------|
| Native      | 282       | 741          | 2.62       | 16,424     | 18,576        |
| XAD         | 288       | 742          | 2.58       | 46,892     | 46,752        |
| Adept       | 479       | 945          | 1.97       | 16,752     | 16,932        |
| CoDiPack    | 358       | 828          | 2.32       | 36,740     | 41,392        |

Both modes record the same 2,048 AAD pricing paths. This profile has five
live inputs (four model parameters and one script constant), so the retrained
mode adds ten hard-policy fits and ten value-only pricing passes, with no
additional AAD tape recordings. The peak RSS comparison includes tape and
ordinary process allocations; it is not a backend-specific tape-byte count.
Tree and compiled runs returned matching PV and all five risks to the printed
precision on every backend.
