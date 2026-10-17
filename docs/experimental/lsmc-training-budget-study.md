# LSMC training-budget study

The default training budget remains the pricing path count. No admissible PV or
Greek error ceilings are supplied. The results below describe explicit-budget
differences and empirical uncertainty; they do not establish an automatic cap
or a guaranteed stopping-value error bound.

The [study tools](../../dal-cpp/benchmarks/script_mc_perf/README.md#lsmc-training-budget-study)
provide the reproducible build, sample matrix, raw records, resume checks and
statistical summaries. Production valuation retains its existing numerical
methods, path layout and defaults.

## Experiment

All budgets use identical held-out Sobol points for each seed comparison. In the
isolated experimental image, training occupies `[0,N)`, validation `[A,A+V)` and
pricing replicate `r` `[A+V+rP,A+V+(r+1)P)`. Only addresses and their admission
checks differ from native production; the fit, replay, seed keys and numeric
reduction are shared. Changing a native training count without this control also
moves held-out points, so equal pricing seeds alone are insufficient.

The representative matrix contains 5632 observations: eleven cases, four
training budgets, eight training seeds, eight independently shifted pricing-seed
groups, two pricing replicates per group, and Frozen/RetrainedBump modes.
Training budgets are 512/2048/8192/32768, each replicate prices 4096 paths, and
validation uses 128 paths with anchor 32768. The large daily-put corner contains
384 observations at training budgets 65536/131072/524288, 32768 pricing paths
per replicate and 1024 validation paths, with anchor 524288 and the same seeds
and modes. These are compiled runs with a four-thread pool, Default normal
precision and Brownian bridge disabled. The ordinary cubic basis, smoothing and bump settings
remain at their native defaults.

Case definitions include two-date puts across moneyness and low volatility,
weekly/daily puts, a dividend-paying weekly call, path-dependent puts with
two/three scalar regression features, and a flat 2x2 local-volatility surface.
The European single-exercise case is a control with zero continuation.
Read the native records for exact model parameters, dates and ordered risk axes.

## Observed budget differences

The representative price table compares 512 with 32768 training paths. Each
entry first subtracts the largest-budget result within the same training and
pricing seed pair, then averages the paired differences. Frozen and
RetrainedBump return exactly the same PV; their Greeks differ in policy response.

| Case             | Mean PV difference | Training SD of difference | Largest conditional paired SE |
|------------------|--------------------|---------------------------|-------------------------------|
| asian_three      | -0.050038269       | 0.050672937               | 0.0052238658                  |
| asian_two        | -0.032662103       | 0.047238589               | 0.0057512142                  |
| bermudan_atm     | 0.0018863751       | 0.0020759206              | 0.0009043719                  |
| bermudan_itm     | -0.01111992        | 0.015916652               | 0.00081471613                 |
| bermudan_low_vol | 0                  | 0                         | 0                             |
| bermudan_otm     | -0.0025206396      | 0.00082678871             | 0.00065451757                 |
| daily_put        | -0.090558649       | 0.056626051               | 0.020140535                   |
| european_put     | 0                  | 0                         | 0                             |
| local_vol_weekly | -0.016432566       | 0.031415944               | 0.011051698                   |
| weekly_call      | -0.056018829       | 0.090745364               | 0.015225028                   |
| weekly_put       | -0.016432566       | 0.031415944               | 0.011051698                   |

The large daily-put table compares the two lower budgets with 524288 training
paths. Its pricing count and validation count differ from the representative
matrix, so its rows are separate comparisons.

| Mode      | Training | Mean PV difference | Training SD of difference | Largest conditional paired SE |
|-----------|----------|--------------------|---------------------------|-------------------------------|
| frozen    | 65536    | -0.0017430872      | 0.0027047991              | 0.0014718444                  |
| frozen    | 131072   | -0.00087545089     | 0.0018022471              | 0.0015514884                  |
| retrained | 65536    | -0.0017430872      | 0.0027047991              | 0.0014718444                  |
| retrained | 131072   | -0.00087545089     | 0.0018022471              | 0.0015514884                  |

Budget sensitivity differs by coordinate. For the daily put, 512 versus 32768 training
paths gives a mean RetrainedBump Vega difference of 7.9548, observed training SD 14.3434,
and largest conditional paired pricing SE 6.8327. Its mean PV difference is -0.09056. For
the weekly put, the RetrainedBump spot difference is 0.14326 with observed training SD
0.35443. The European and low-volatility controls have zero paired budget differences for
every first-order coordinate. These observations support retaining defaults rather than
judging training adequacy from price differences alone.

The [complete paired-coordinate data](data/lsmc-training-budget-paired.csv)
retain PV, every first-order model/script sensitivity, and each training seed's conditional
paired mean and pricing SE for every budget, including the reference itself.
Sensitivities use raw model/script parameter units, matching native `risks_`.
The [observed dispersion data](data/lsmc-training-budget-dispersion.csv)
retain means, standard deviations, ranges and the largest conditional pricing
SE across training seeds. The `diagnostics` rows separately describe hard PV
and fuzzy-minus-hard smoothing differences. An empty relative-difference field
means the reference mean is zero; absolute and scaled differences remain present.

## Interpretation

For each training seed, pricing SE is the sample standard deviation of eight
paired pricing-group differences divided by the square root of eight. Each
group averages two native pricing replicates. This conditions on the fitted
base policy and, for RetrainedBump Greeks, the fixed bumped-policy set. It
excludes changes in those fitted policies when the training seed changes.

Training dispersion is the observed standard deviation and range of the eight
paired training-seed means. It includes residual pricing noise. Pricing groups
are shared across training seeds, so all observations cannot be treated as
independent training-error samples. The largest budget is a comparison
reference, not an independent oracle for the optimal stopping value. Small
paired differences do not prove small basis bias, smoothing bias or total error.
RetrainedBump is a finite-step sensitivity of the fitted smoothed estimator.

## Resource measurements

Ratios use 524288 training paths as the reference; each round contains ten paired runs.

| Mode       | Training | Native time ratio R1/R2 | Process time ratio R1/R2 | Peak RSS ratio R1/R2 | Peak RSS MiB R1/R2 |
|------------|----------|-------------------------|--------------------------|----------------------|--------------------|
| Hard       | 65536    | 0.3524 / 0.3593         | 0.3527 / 0.3596          | 0.1279 / 0.1279      | 369.6 / 369.6      |
| Hard       | 131072   | 0.4621 / 0.4550         | 0.4624 / 0.4553          | 0.2525 / 0.2524      | 729.7 / 729.5      |
| Frozen AAD | 65536    | 0.7937 / 0.7691         | 0.7937 / 0.7691          | 0.1279 / 0.1279      | 369.7 / 369.7      |
| Frozen AAD | 131072   | 0.7966 / 0.7872         | 0.7967 / 0.7872          | 0.2525 / 0.2525      | 729.6 / 729.6      |

Reference median peak RSS ranges from 2889.8 to 2890.0 MiB across these comparisons. All
four compatibility cases match every numerical/metadata output; none has a native or
process increase above 4% in both rounds. The ordinary-double process measurement exceeds
4% in one round only; the other round is flat.

Resource runs use the historical put-exercise/call-maturity mixed-payoff fixture
at 524288 deterministic pricing paths, compiled execution and a four-thread pool.
They measure hard and Frozen AAD valuations without extra hard diagnostics on
the fixed anchor-524288 pricing block. This fixture differs from the true put
accuracy cases. Different budgets are different inputs; resource ratios do not
establish accuracy acceptance. Peak RSS includes all process allocations.

Resource sampling monitors foreign build/test activity throughout each process and
excludes interrupted attempts.

The timing protocol uses two rounds of ten alternating paired process runs on
an otherwise idle local host, with minimum native/process times and median peak
RSS. Separate legacy and ordinary CLI compatibility measurements use the same
sampling controls. `script_mc_perf` timings are informational and belong outside
the repository's official nine-target regression verdict.

Prepare a separate controlled resource image with anchor 524288.
For each mode (`hard` or `aad`) and lower training budget, alternate it with 524288
for ten pairs in each of two rounds; retain stdout, stderr and GNU time output:

```bash
study=dal-cpp/benchmarks/script_mc_perf
python "$study/lsmc_study_prepare.py" --source "$PWD" \
  --output /tmp/dal-lsmc-resource --anchor 524288 --jobs 4
DAL_NUM_THREADS=4 OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1 \
  /usr/bin/time -v taskset -c 0-31 /tmp/dal-lsmc-resource/control-524288/script_mc_perf \
  --lsmc-replay 65536 524288 1CD aad bs compiled
```

Replace 65536 with 131072 or 524288 for the other budgets. Native `time_ms` covers
the valuation, including preparation/training/replay; process time and RSS cover
the complete executable. Small pricing batches can cap active workers below the
configured pool size in the separate accuracy profiles.

## Validation and provenance

Native stock/control admission at the training anchor covers four payoff/state/
model shapes, both engines and hard/Frozen/Retrained modes: 24 pairs. The large
daily corner has three additional native mode controls. A fresh committed-source
preparation repeats all 24 controls against both its stock image and the original
experiment producer; every numerical and policy output matches exactly. Stock
and controlled sample CLI suites, existing production CLI tests and portable
study tests exercise numerical controls, complete risk axes, source/configuration/
dependency identities, failed attempts, resumption and paired statistics.

The producer uses native AAD, GCC 15.2 Release (`-O3`, `-ffp-contract=fast`, Eigen
parallelization disabled) on 13th Gen Intel(R) Core(TM) i9-13900HX. Timing uses a
four-thread pool and CPU affinity 0-31. The immutable core baseline is `582b1fe5`; the
published tool content is `05a24acd`. The parser-callback rename relative to the original
producer is byte-identical in the ordinary object and both controlled-anchor objects. The
final isolated preparation also matches the original producer on all 24 controls.

The [data provenance](data/lsmc-training-budget-provenance.json) identifies the
verified manifests, producer executables, analyzer and exported-file hashes.
Raw stdout, stderr, policy diagnostics, process receipts, patches, objects,
build logs and sampling records are retained with the experiment evidence.
Reproduction on another machine starts with a fresh isolated preparation and
new dataset; local manifests deliberately bind observations to their producer
paths and content identities.
