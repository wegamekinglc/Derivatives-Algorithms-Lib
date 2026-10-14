# LSMC training-storage precision study

Binary32 training rows reduce stored row bytes, while observed price and Greek
changes depend on the fitted stopping policy. The ordinary DAL storage type
remains double. No admissible price/Greek error ceilings are supplied, so this
study reports differences and uncertainty without an accuracy pass or a change
to the default training count.

## Method

The experiment changes training and validation storage for payments, regression
features, immediate exercise values and stochastic discount ratios. Each write,
including each payment accumulation, rounds to its storage type. Reads promote
to double. Model evolution, regression moments/QR/coefficients, backward holding
values, pricing and AAD replay arithmetic retain native double precision.
Condition masks retain char storage. Finite underflow is allowed; nonfinite or
out-of-range writes fail before conversion.

Complete isolated builds keep every consumer of the experimental header layouts
consistent. A binary64 overlay controls for the row views, checks and allocation
counters. Across 88 representative, 4 large-budget and 24 conditioning triple-image
controls, all binary64-overlay price/risk/metadata/policy outputs match the native
image exactly. Binary32 floating-row allocation peaks are exactly half the 64-bit
control, and live row counters return to zero. These controls use one training
and pricing seed pair and establish implementation admission for those cases.

The financial matrix uses the same disjoint Sobol addresses and stream keys for
both widths: training `[0,N)`, validation `[A,A+V)` and pricing replicate `r` at
`[A+V+rP,A+V+(r+1)P)`. It retains Default normal precision, compiled evaluation,
Brownian bridge disabled and a configured four-thread pool; small native batches
can cap active workers.

- Representative matrix: 2816 binary32/native64 pairs, 11 financial cases,
  training 512/32768, P4096 per replicate, R2, V128, A32768.
- Large corner: 256 pairs, daily put, training 65536/524288, P32768, R2, V1024,
  A524288.
- Both matrices use eight training seeds (17/41/73/101/149/211/307/401) and eight
  pricing groups (1009/2027/3001/4051/5003/6089/7013/8087), with Frozen and
  RetrainedBump.

The native64 reference reuses exact accepted jobs from the
[training-budget study](lsmc-training-budget-study.md). A complete fresh native
rebuild reproduces the stock executable and archive byte for byte; both controlled
objects and fully relinked controlled executables also match at each anchor.
Expanded source/dependency inputs, configuration and the immutable image/job
identities are verified before reuse. The original reference manifests and raw
outputs remain unchanged.

For each training seed, differences are formed on identical pricing groups at
the same training budget before estimating conditional paired pricing SE.
Across training seeds the report gives observed dispersion and ranges of paired
means. This dispersion includes residual pricing noise. Shared pricing groups
across training seeds prevent treating every observation as independent.
Greek pricing SE conditions on the fitted base and bumped policies. The study
covers first-order sensitivities; it does not establish second-order accuracy.

## Price and first-order sensitivities

Frozen and RetrainedBump return identical PVs for each job; the price table uses
Frozen rows to avoid duplicating them. Frozen differentiates with the fitted
policy held fixed. RetrainedBump adds the native common-path finite-step policy
response to that frozen adjoint. All risk coordinates retain the ordinary
smoothing and bump settings and describe the fitted native estimators.

| Profile/case              | Training | Mean PV difference | Training SD   | Max conditional paired SE |
|---------------------------|----------|--------------------|---------------|---------------------------|
| accuracy/asian_three      | 512      | 2.4929542e-08      | 5.3094733e-08 | 8.6737349e-08             |
| accuracy/asian_three      | 32768    | -3.0663797e-07     | 8.695945e-07  | 5.4244391e-06             |
| accuracy/asian_two        | 512      | 1.0440042e-08      | 4.5506651e-08 | 5.0276221e-08             |
| accuracy/asian_two        | 32768    | -5.2270125e-08     | 1.2567948e-07 | 2.064491e-07              |
| accuracy/bermudan_atm     | 512      | 2.9507469e-09      | 8.0973718e-09 | 1.7050756e-08             |
| accuracy/bermudan_atm     | 32768    | 6.2899325e-11      | 4.2000714e-10 | 1.6296957e-09             |
| accuracy/bermudan_itm     | 512      | 2.152828e-08       | 4.5903969e-08 | 4.7610922e-08             |
| accuracy/bermudan_itm     | 32768    | 6.4987082e-10      | 2.2580708e-09 | 2.9713282e-09             |
| accuracy/bermudan_low_vol | 512      | 0                  | 0             | 0                         |
| accuracy/bermudan_low_vol | 32768    | 0                  | 0             | 0                         |
| accuracy/bermudan_otm     | 512      | 6.6441687e-10      | 1.2042266e-09 | 2.1190057e-09             |
| accuracy/bermudan_otm     | 32768    | 4.535916e-10       | 3.1125125e-09 | 7.2747795e-09             |
| accuracy/daily_put        | 512      | 9.3187142e-08      | 9.1075356e-08 | 1.9209595e-07             |
| accuracy/daily_put        | 32768    | -0.00067953343     | 0.00096296815 | 0.00068446155             |
| accuracy/european_put     | 512      | 0                  | 0             | 0                         |
| accuracy/european_put     | 32768    | 0                  | 0             | 0                         |
| accuracy/local_vol_weekly | 512      | 8.7615345e-09      | 8.5816904e-08 | 6.2948578e-08             |
| accuracy/local_vol_weekly | 32768    | 3.6909508e-05      | 0.00021349575 | 0.00058972224             |
| accuracy/weekly_call      | 512      | -1.2318392e-08     | 6.3579926e-08 | 1.147694e-07              |
| accuracy/weekly_call      | 32768    | 0.00017401458      | 0.0013245477  | 0.0024040148              |
| accuracy/weekly_put       | 512      | 8.7615342e-09      | 8.5816904e-08 | 6.2948578e-08             |
| accuracy/weekly_put       | 32768    | 3.6909508e-05      | 0.00021349575 | 0.00058972224             |
| corner/daily_put          | 65536    | 4.3594791e-05      | 0.00019668011 | 0.00014714817             |
| corner/daily_put          | 524288   | -1.2978358e-05     | 8.9485202e-06 | 2.1297532e-05             |

The largest absolute average representative PV shift is daily_put at training
32768: -0.00067953343, with observed training SD 0.00096296815 and largest
conditional paired pricing SE 0.00068446155. A price difference alone does not
bound the Greek difference. For daily_put at training 512 in RetrainedBump,
the rate-sensitivity difference averages 2.6767621 per unit rate, with observed
training SD 7.5709205. Its seed-mean range is [-0.00041984333, 21.413834] and the
largest paired pricing SE is 4.4279135. The largest shift occurs at training
seed 101: native64 -69.1611387 versus binary32 -47.7473044, averaged over the eight
common pricing groups. In that budget's price comparison, mean PV shift is only
0.000000093187142. These are measured paired effects, without a supplied
acceptance ceiling or a confidence claim about total training error.

Frozen daily_put at 32768 also changes Vega: mean difference 0.82851485 per unit
volatility, observed training SD 2.2821793 and maximum paired pricing SE 4.0813799.
Normal-case degree/rank/eligible/solver/fallback fields remain equal across widths;
the largest representative exercise-rate change is 0.000732421875.

At training 65536 and 524288, the daily-put mean PV differences are respectively
0.000043594791 and -0.000012978358. Their observed training SDs are
0.00019668011 and 0.0000089485202; maximum conditional paired pricing SEs are
0.00014714817 and 0.000021297532. Notable corner Greek shifts include
0.08829465 per unit dividend at 65536 in RetrainedBump (training
SD 0.25295, maximum paired SE 0.26563), and -0.03239423 per unit dividend at
524288 in RetrainedBump (SD 0.079003, maximum paired SE 0.082398). Normal-case
structural policy fields remain equal, while the maximum exercise-rate change
is 0.000274658203125. These observations do not define an acceptable error bound.

The paired coordinate CSV ([paired data](data/lsmc-storage-precision-paired.csv)) retains every
price and risk coordinate for each training seed. The
dispersion CSV ([dispersion data](data/lsmc-storage-precision-dispersion.csv)) retains observed
ranges and conditional pricing SE. Hard PV and fuzzy-minus-hard values remain
separate coordinates. Rate/dividend/volatility sensitivities are per unit input,
and script strike sensitivity is per unit strike. Native64 is a comparison
reference at an equal budget, rather than the true stopping value.
An empty relative-difference field means the reference mean is zero; absolute
and scaled differences remain present.

## Conditioning diagnostics

Separate controls use bermudan_low_vol and asian_three at volatility 0/1e-10/1e-7,
N512, P257, V16, R2, tree/compiled and both policy-risk modes. They use one training
and pricing seed pair. Binary32 changes fitted ranks in four of 24 controls; price and
first-order risks remain equal in these particular diagnostic cases. This
demonstrates a conditioning change without establishing general boundary-case
accuracy. The conditioning CSV ([conditioning data](data/lsmc-storage-precision-conditioning.csv))
records each case and its native/float fallback sets. Stochastic discount-row
storage has primitive coverage; the financial matrix uses deterministic rates
and does not establish stochastic-rate product accuracy.

## Resource measurements

Ratios are candidate divided by reference. Width cases compare the binary32
overlay with the binary64 overlay. Native overhead cases compare the binary64
overlay with ordinary native64. Sampling stopped at the user's request to draw
conclusions from finite available samples instead of repeatedly retrying
individual interruptions. The table contains only complete pairs.

| Case                  | Complete pairs, rounds 1 / 2 | Valuation time ratio, rounds 1 / 2 | Process time ratio, rounds 1 / 2 | RSS ratio, rounds 1 / 2 |
|-----------------------|------------------------------|------------------------------------|----------------------------------|-------------------------|
| mixed-frozen-overhead | 10 / 4                       | 1.0076 / 0.9945                    | 1.0075 / 0.9956                  | 1.0000 / 1.0000         |
| mixed-frozen-width    | 10 / 10                      | 0.9683 / 0.9737                    | 0.9682 / 0.9739                  | 0.5025 / 0.5024         |
| mixed-hard-overhead   | 10 / 10                      | 0.9712 / 1.1485                    | 0.9702 / 1.1485                  | 1.0000 / 1.0000         |
| mixed-hard-width      | 10 / 10                      | 0.9274 / 0.9068                    | 0.9281 / 0.9070                  | 0.5024 / 0.5024         |

Measured native cases cover daily hard pricing and FrozenAAD replay.
Width comparisons hold inputs and path addresses fixed
except storage width. Binary64-overlay/native comparisons separately quantify
experiment overhead. Three cases completed two rounds of ten alternating process
pairs; Frozen overhead completed ten and four pairs. Ratios use native/process
minimum durations and median peak RSS from those actual counts. The shorter
second round has less opportunity to observe a minimum and is not equivalent to
the planned ten-pair round. Extra hard diagnostics
are disabled. Row bytes describe floating training/validation allocations;
overall RSS also includes model, holding, regression and pricing workspaces.
The [resource CSV](data/lsmc-storage-precision-resources.csv) also retains per-round minima, medians, ranges and measured row
peak bytes. The ordinary native image has no row counter, so its counter field
is empty; its RSS and timings remain measured. Timing ratios
can vary between rounds; the controls quantify measured experiment overhead
without establishing a universal speedup or a production adoption decision.
These script_mc_perf measurements are informational and remain outside the
official nine-target benchmark verdict.

In the completed width cases, binary32 reduces median peak RSS by approximately
49.8% and floating-row allocation peaks by exactly 50%. Native valuation ratios
range from 0.9068 to 0.9737 across the four measured rounds. This is a workload
observation, not a universal speedup. Hard-mode overlay overhead changes direction
between rounds (0.9712 and 1.1485), so it does not establish a stable overhead or
speed advantage. The table reports the partial Frozen overhead evidence without
an acceptance verdict.

The planned Asian-three width/overhead cases and expired-Dupire Python binding
overhead case were not sampled under the final monitor. Their performance is
unmeasured in this report. Existing PDE/LSMC synchronization paths and the changed
Dupire GIL behavior have correctness-test coverage; that coverage does not
establish their runtime overhead. No further resource retries are required for
this finite-sample report.

The resource CSV's `native_seconds` columns contain C++ valuation time. Process
times include startup. All 148 measured processes are retained as 74 complete
pairs; the original plan called for 280 processes across seven cases.

The daily resource case retains the historical mixed fixture: put intrinsic at
exercise and a call payoff at maturity. Financial accuracy tables use the
ordinary put/call/Asian contracts. The mixed fixture's resource results do not
establish ordinary Bermudan-put accuracy.

## Reproducibility and scope

The [provenance record](data/lsmc-storage-precision-provenance.json) identifies the financial summaries, complete
profile manifests, reference rebuild proof and native executables. It also
records the selected resource receipts and the exact collector, process monitor
and Python driver hashes. The CSV hashes bind the published tables to those
verified summaries. Raw outputs, commands, failed attempts and build/source
identities are retained in the research evidence archive.

Resource sampling monitored foreign native C/C++ and Rust compiler, build-driver
and focused test processes once per second. Interrupted whole pairs are excluded
and retained. Earlier measurements made with a narrower monitor are diagnostic
only. Sampling was stopped with three complete cases, one partial case and three
unmeasured cases; the report makes no overall performance acceptance claim.
Other builds continued independently. No financial observations were repeated
or replaced by timing samples.

The [benchmark tool guide](../../dal-cpp/benchmarks/script_mc_perf/README.md#lsmc-storage-precision-experiment) documents preparation, strict storage-width admission
and paired analysis. The experiment includes primitive stochastic-discount
storage coverage, but its financial claims are limited to the deterministic-rate
products, budgets, seeds and first-order risk modes described above. The native
double reference at the same budget is not a bound on the true exercise value.

The numerical shifts and measured resource effects provide research evidence.
Retain native double storage and explicit training budgets pending product/risk
accuracy criteria and additional coverage for any intended adoption domain.
