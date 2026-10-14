# Script Monte Carlo research tools

`script_mc_perf` is an optional core benchmark target. Its helpers use core APIs
and build with `DAL_BUILD_PUBLIC=OFF`. Ordinary production profiling and
`--lsmc-replay` retain their existing command lines.

## LSMC training-budget study

Changing the native training count moves the validation and pricing Sobol blocks.
Equal pricing seeds alone therefore do not give equal pricing samples. The study
builds an isolated experimental image with training `[0,N)`, validation `[A,A+V)`
and pricing replicate `r` at `[A+V+rP,A+V+(r+1)P)`, for every `N <= A`.
The original seed/role keys, fit, replay and numeric reduction are preserved.
The address patch is restricted to two recognized native functions and adds
overlap/32-bit-range checks. Unknown source versions fail preparation.

The control compiles the current patched `lsmc.cpp` and matching sample helper
into two replacement objects, linked before the unchanged static stock archive.
The stock executable, archive, reused objects and source files are hashed and
rechecked. No production pricing-offset option or alternate LSMC engine is added.

Use Linux, Python 3.10+, CMake, a C++17 compiler and Unix Makefiles. Start from a
committed checkout with clean initialized Eigen, Google Test, Machinist and
RapidJSON submodules at their committed pins. The preparation command clones
committed sources and dependencies into a new directory outside the supplied
checkout, then makes a fresh static Release core-only build. It retains build
logs, the exact patch, commands, configuration and executable identities.

```bash
study=dal-cpp/benchmarks/script_mc_perf
python "$study/lsmc_study_prepare.py" --source "$PWD" \
  --output /tmp/dal-lsmc-study --anchor 32768 --jobs 4
python tests/lsmc-training-study/check_sample.py \
  /tmp/dal-lsmc-study/stock-build/dal-cpp/benchmarks/script_mc_perf/script_mc_perf \
  --addressing stock
python tests/lsmc-training-study/check_sample.py \
  /tmp/dal-lsmc-study/control-32768/script_mc_perf --addressing controlled
python "$study/lsmc_training_study.py" run \
  --image /tmp/dal-lsmc-study/prepared-image.json --profile accuracy \
  --output /tmp/dal-lsmc-accuracy
python "$study/lsmc_training_study.py" summarize \
  --dataset /tmp/dal-lsmc-accuracy --output /tmp/dal-lsmc-accuracy-summary.json
```

Repeat `run` with the same arguments to resume verified successful observations.
An active dataset has an exclusive process lock. A failure, timeout, partial
record or altered raw output stops execution; `--retry-failed` archives the
previous attempt before retrying. It never silently overwrites failures.
Changing the image, helper identities or job matrix requires a new dataset.
Summary generation rejects missing/duplicate samples, risk-axis changes,
different fitted policies across pricing groups, and price disagreement between
Frozen and RetrainedBump. Every attempt retains stdout, stderr, return code,
elapsed process time and hashes. Do not edit consumed inputs during a run.

The `accuracy` profile contains 5632 observations: 11 cases, training budgets
512/2048/8192/32768, eight training seeds, eight independent pricing-seed groups,
two pricing replicates per group, and both policy-risk modes. Each replicate
prices 4096 paths; validation uses 128 paths. Cases include European and
two-date puts across moneyness/low volatility, weekly/daily puts, a weekly call,
two/three-feature path-dependent puts and a flat local-vol weekly put.
`--case NAME` selects a subset while preserving the profile's seed counts.

The separate `corner` profile needs an image prepared with `--anchor 524288`.
It contains 384 daily-put observations at training budgets
65536/131072/524288, 32768 pricing paths per replicate and 1024 validation paths,
with the same seed/mode counts. `smoke` and `calibration` are explicitly
diagnostic profiles and cannot establish accuracy acceptance.

Each summary retains every price and Greek coordinate. For each training seed,
it subtracts the largest-budget observation on each identical pricing group
before computing the mean difference and conditional pricing standard error.
Across training seeds it reports observed dispersion and ranges of those paired
means. Shared pricing groups and residual pricing noise prevent treating all
observations as independent training-error estimates. Hard prices and
fuzzy-minus-hard smoothing differences appear separately. A larger training
budget is a comparison reference, not the true stopping value.

The study has no supplied admissible price/Greek error ceilings. **The default
training budget remains the pricing path count.** Budget differences, measured
resource savings and conditional pricing error alone do not justify a universal
cap or an accuracy pass. Resource timing must disable extra hard diagnostics and
distinguish native valuation time from total process time and peak RSS.

## Structured native samples

`--lsmc-training-sample` emits one JSON record on stdout and startup/errors on
stderr. Named options are `--case`, `--training N|default`, `--pricing`,
`--validation`, `--training-seed`, `--pricing-seed`, `--replicates`, `--threads`,
`--engine tree|compiled`, `--mode hard|frozen|retrained`,
`--normal-precision Default|Fast|Precise`, `--brownian-bridge true|false`,
`--diagnostics true|false` and optional `--vol`. Replicates must be at least two.
The default sample is a positive single-exercise control; select a named case
for financial research. `mixed_original` retains the historical put-exercise /
call-maturity fixture and is not a plain Bermudan put.

Records include actual block offsets, complete risk axes, stream keys,
preparation/valuation timings and optional hard-policy diagnostics. Only the
verified experimental image reports `addressing: controlled`; stock samples
retain native moving addresses. The supported study is the prespecified runner
matrix, rather than an arbitrary collection of individual native calls.
