# Changelog

Notable, fundamentally-important changes to the DAL C++ quantitative finance library.
This file records **breaking changes, major new capabilities, new methodologies, and
significant methodology shifts only** — not every commit or minor fix. Routine refactors,
test work, formatting, and build/CI changes are deliberately omitted.

The library is documented as a single current version; the docs under `docs/` always
describe the latest state. This changelog is the only place historical context is kept.

## Entry format

Each entry is a short bullet under a dated heading, in the form:

- `<area>: <one-line description>` — link to the relevant doc or PR where useful.

Only add a heading when a qualifying change ships. Do not create empty future headings.

## 2026-10-06

- **Budgeted C++/Python/Excel script Jacobians** — ordered output blocks replay a
  single frozen preparation and return owning raw/report matrices, axes and
  execution diagnostics. Result payload, aggregate native tape and numeric
  scratch budgets admit known startup capacity before history and guard growth
  during execution. Python adds read-only requests/results; Excel adds immutable
  handles and spill tables. See [script Jacobians](docs/public-api.md#budgeted-script-jacobians).
- **Tracked DAL numeric allocation** — native scratch budgets use a stateless
  allocator across shared-library boundaries. This changes underlying standard
  iterator types; C++ consumers must rebuild and use DAL iterator aliases. See
  [capacity semantics](docs/methodology/aad.md#budgeted-script-jacobians).
- **C++/Python/Excel weighted script risk** — ordered scalar output choices and passive
  weights produce one native AAD objective, with owning component means,
  weighted mean/gradient, reporting and preparation provenance. Exact numeric
  payload preflight precedes history and workers; price-only execution is
  explicit. Python supplies strict keyword requests, an output-axis query and
  detached read-only results while releasing the GIL for native work. Excel
  supplies immutable handles and component/gradient/snapshot tables. See
  [weighted script risk](docs/public-api.md#weighted-script-risk) and the
  [Python entry](docs/python/README.md#weighted-script-risk) and
  [Excel entry](docs/excel/README.md#weighted-script-risk).
- **Excel calibration and automatic Dupire requests** — immutable worksheet
  requests and sealed plans expose quote selections, exact retained payload
  budgets, explicit constant/direct dependencies and native AAD results. Passive
  getters retain native source metadata and raw/report projections. See
  [worksheet quote requests](docs/excel/script-settings.md#calibration-quote-requests).

## 2026-10-05

- **Python automatic Dupire script requests** — owning read-only requests and
  sealed plans expose mandatory surface/direct inputs, complete payload budgets
  and native Monte Carlo quote results. Typed direct bindings and callback-free
  GIL release reuse the accepted C++ planner and execution. See
  [automatic Python requests](docs/python/README.md#automatic-dupire-script-risk-requests).
- **Python calibration-coordinate requests** — owning read-only requests,
  plans and results expose native quote metadata, full/subset/empty selections,
  complete retained payload budgets and detached raw/report projections. Strict
  input parsing and callback-free GIL release reuse the native planner and VJP.
  See [Python quote requests](docs/python/README.md#common-calibration-quote-requests).
- **Automatic C++ Dupire script risk** — an owning request plan seals a native
  flat-rate Hybrid, exposes mandatory surface/direct input coordinates and checks
  the combined valuation/quote payload before execution. Explicit constant
  dependencies add fixed-surface quote partials once; immutable results preserve
  native estimator, provenance and report projections. See
  [automatic requests](docs/methodology/aad.md#automatic-c-dupire-risk-requests).
- **C++ calibration-coordinate requests** — passive owning plans select source-scoped
  quote coordinates, expose native axes and preflight the complete retained
  contribution payload. Immutable results preserve raw native derivatives and
  provide detached report projections with finite scaling checks. See
  [quote-coordinate requests](docs/yield-curves/jacobian-risk.md#c-quote-coordinate-requests).
- **Common C++, Python and Excel calibration pullback** — an owning boundary maps passive
  parameter adjoints from frozen Dupire or captured native curve provenance
  into one immutable result with separate calibration, direct and total quote
  contributions. Complete source checks preserve canonical coordinates and
  existing inverse scaling. Python adds strict optional curve-record capture,
  owning readonly values, detached matrices and callback-free GIL release.
  Excel adds optional native record capture, immutable common handles and
  detached contribution/source getters. See the
  [common calibration interface](docs/yield-curves/jacobian-risk.md#common-passive-c-calibration-pullback).

- **Frozen Dupire calibration quote pullback** — C++ snapshots retain numeric
  base IVS samples and complete calibration identity; native scalar reverse maps
  local-volatility node adjoints to spread quotes, with additive boundary seeds,
  separate direct contributions and explicit domain validation. The public
  Hybrid adapter checks retained model coordinates and typed component layout,
  requires every surface seed and returns quote risk alongside the passive
  valuation without rerunning Monte Carlo. See
  [AAD methodology](docs/methodology/aad.md#discrete-dupire-calibration-pullback).
  Python exposes the full chain with keyword configuration, custom/Merton IVS,
  detached surfaces and readonly passive results; the public C++ flat-BS
  convenience preserves deterministic carry. Excel exposes immutable calibration,
  seed and result handles with detached getters and complete model/valuation
  quote pullback; a shared model factory preserves frozen carry in both bindings.

- **Structured script risk across C++, Python and Excel** —
  `ValueByMonteCarloWithRisk` / `MonteCarlo_ValueWithRisk` return passive scalar
  results with ordered model/script IDs, mean derivatives, report factors and
  retained execution/model/history snapshots. Requests validate numeric payload
  budgets before preparation; getters extract existing data and diagnose legacy
  display collisions. See [AAD methodology](docs/methodology/aad.md#structured-scalar-risk-results).

## 2026-10-04

- **Explicit native AAD production profiling** — opt-in request and task
  collectors expose phase wall time, actual thread CPU time, sampled tape
  storage, successful block allocations, and selected array payloads. The
  default-OFF build removes hot-loop instrumentation. `script_mc_perf` adds
  explicit cold, warm, and phase modes with fixed-path price/risk validation.
  See [AAD methodology](docs/methodology/aad.md#native-production-profiling).

- **Native AAD only** — remove XAD, CoDiPack and Adept implementations,
  submodule dependencies, build selections and installation exports. Enabled
  legacy backend options fail explicitly; rebuild libraries and consumers with
  native AAD. See [installation](docs/installation.md#native-aad-configuration).

- **Optional native AAD lifetime diagnostics** — the default-OFF
  `DAL_ENABLE_AAD_LIFETIME_DIAGNOSTICS` build checks active operands and adjoints
  for foreign tapes, discarded recordings/suffixes and reused slots before node
  access. Its ABI definition propagates to installed consumers and bindings.
  See [AAD methodology](docs/methodology/aad.md#native-active-number-lifetime-diagnostics).

- **Scoped AAD recordings and checkpoints** — recording phases, opaque checkpoint
  handles, and owner/mode boundaries diagnose invalid use before tape mutation.
  Independent curve, MC, and LSM replay recordings reject nesting and recover
  after backend/cleanup failures. Native full clearing includes every vector
  channel and leaf. See [AAD methodology](docs/methodology/aad.md#independent-recording-ownership).

- **Native AAD propagation precision** — nonzero adjoints are no longer truncated
  by an absolute threshold, preserving derivatives under intermediate rescaling.
  Multi-result zero seeds are isolated from non-finite local derivatives, and
  public Monte Carlo valuation diagnoses non-finite risk results. See
  [AAD methodology](docs/methodology/aad.md).

## 2026-10-03

- **perf: shared SIMD kernels and Eigen-backed dense product** —
  double-precision reductions (dot/sum/axpy) now run through AVX2/SSE2 kernels
  in `dal/math/simdkernels.cpp`, `Matrix::Multiply` dispatches to a pinned
  Eigen 3.4.0 submodule behind `DAL_USE_EIGEN`, and `Matrix_` drops per-row
  hook indirection for offset arithmetic. Kernel benchmarks: 3–5x on matrix
  products and inner products, ~2x on Cholesky factorization. See
  [Installation](docs/installation.md).

- **Gaussian hybrid bank-account integration** — plain GSR hybrid components
  sample the full rate integral with an independent internal bridge driver and
  insert rate-parameter knots into the shared grid. Constant-volatility equity
  option prices now retain the continuous-model Gaussian variance across event
  grids. `NumFactors()` and `SimDim()` include the internal driver; the named
  correlation matrix retains only its registered factors. Dated rate hybrids
  reject maximum steps below one day. See [Hybrid](docs/models/hybrid-model.md).

- **Low-rank Gaussian swaption pricing** — the analytic pricer reduces cashflow
  loadings to their independent Gaussian directions before applying its
  three-direction integration limit, allowing higher-factor models with a
  low-rank payoff. See [GSR](docs/models/gaussian-short-rate.md).

- **Delayed script payments (`PAYS expr ON date`)** — script payments may settle
  on a later literal date. Preparation binds each live delayed payment to a
  discount-factor slot on its event sample (`SampleDef_::discountMats_` /
  `Sample_::discounts_`, previously unwired plumbing), the model provides
  `P(t_event, t_payment)` (BS, correlated BS, GSR including hybrid rate
  components; GSR-SLV rate components fail preparation with
  `UnsupportedDelayedPayment`), and tree, compiled, fuzzy-AAD, and LSMC
  recording evaluators discount the payment through that slot, keeping AAD
  rate sensitivity through the discount factor. Same-date and settled payments
  behave like plain `PAYS`; a past event with a payment still outstanding on
  the evaluation date fails with `UnsettledDelayedPayment` (settlement follows
  the payment date, never silently dropped). `ON` became a reserved script
  keyword. See [Script engine](docs/methodology/script_engine.md).

- **Standard rate instruments and quotes** — bond options, caplets/floorlets,
  physically settled swaptions and dated coupons are model-agnostic protocol
  types (`BondOption_`, `Caplet_`, `Swaption_`, `EuropeanRateOption_`), and
  calibration/market quotes moved to standard `CalibrationQuote_` and `VolQuote_`
  with a `VolConvention_` enum (NORMAL, BLACK, SHIFTED_BLACK). GSR pricing and
  calibration consume the standard types; the previously GSR-prefixed Python and
  Excel bindings were renamed accordingly (for example `Caplet_New`,
  `CalibrationQuote_New`, `VolQuote_New`, `VolQuotes_Get_Prices`). See
  [GSR](docs/models/gaussian-short-rate.md).

- **Hybrid stochastic rate components** — the hybrid model accepts multi-factor
  GSR rate components and the GSR stochastic-local-volatility model as a single
  component. Standalone GSR and GSR-SLV models are unchanged; the hybrid adapter
  adds co-evolution with named-factor correlations (validated against the kernel)
  and the SLV Euler breakpoints on the shared timeline. See
  [Hybrid](docs/models/hybrid-model.md#stochastic-rate-components).

- **Shared bounded Gauss-Newton solver** — the bounded, damped Gauss-Newton fit
  used by GSR calibrations moved to `dal-cpp/dal/math/optimization/boundedgn.hpp`
  as a reusable component for any bounded least-squares problem.

## 2026-10-02

- **GSR-SLV market calibration** — physical swaptions support conditional valuation
  of future floating lags and retained fixings since evaluation. Normal, Black and
  shifted-Black market quotes, volatility/curve recalibration risk and selectable
  AAD calibration Jacobians are available through dal-public, Python and Excel.
  Excel SLV option prices now return three columns: price, pair standard error and
  conditional refinement difference. See [GSR](docs/models/gaussian-short-rate.md#market-volatility-quotes).

- **GSR-SLV calibration and quote risk** — bounded stochastic-volatility/leverage
  fitting, independent-path and finer-grid validation, held-out diagnostics and
  recalibrated price/curve quote sensitivities are available through dal-public,
  Python and Excel. See [GSR](docs/models/gaussian-short-rate.md#european-pricing-and-calibration).

- **GSR stochastic local volatility** — multi-factor Markovian HJM with normalized
  CIR variance, rate-shift leverage, bank-account discounting and model-input AAD
  is available through dal-public, Python and Excel. See [GSR](docs/models/gaussian-short-rate.md#stochastic-local-volatility).
- **European GSR calibration** — analytic bond options and caplets, multi-factor
  physically settled European swaptions, and bounded regularized g-bucket
  calibration with fit, numerical-error and rank diagnostics are available
  through dal-public, Python and Excel. See [GSR](docs/models/gaussian-short-rate.md).
- **Multi-factor Gaussian short rates** — named factors with dated g/H matrices,
  PSD correlations, exact Gaussian event transitions and conditional discounting,
  factor-qualified AAD risks, and archives are available through dal-public,
  Python, and Excel. One-factor factories and risk labels remain compatible.
  See [GSR](docs/models/gaussian-short-rate.md).

## 2026-10-01

- **Pseudo-random stream positioning** — `Random_::SkipNormalTo` positions
  normal paths independently of antithetic uniform paths. Ordinary IRN and
  MRG32 Monte Carlo batches reproduce the sequential stream; multi-batch
  estimates and risks can therefore change. `SkipTo` reconstructs odd and even
  uniform offsets on reused generators, and pseudo-random clones preserve the
  current state and precision. See [sampling](docs/methodology/monte-carlo/sampling.md).

## 2026-09-30

- **Composable local volatility** — serializable spot/time local-vol surfaces
  and equity components now run inside the Hybrid model with deterministic
  or stochastic GSR domestic rates, shared factor correlation, internal
  time stepping, and AAD grid risks. Dupire now calibrates surfaces only;
  its standalone model and C++, Python, and Excel construction APIs are removed.
  See [local volatility in hybrid models](docs/models/local-volatility.md).

## 2026-09-29

- **Gaussian Short Rate model naming** — the one-factor rate model and its
  curve and volatility data are now named GSR across C++, Python, Excel, and
  serialized type identifiers. Existing code using VHW names must use the GSR
  names; archives with VHW type identifiers require migration before loading. See
  [Gaussian Short Rate model](docs/models/gaussian-short-rate.md).
- **GSR benchmark comparison** — the standard swap is priced through DAL,
  QuantLib, and rateslib with reused and fresh cashflows. The GSR European
  swaption retains both QuantLib Sobol Monte Carlo and Gaussian1d integration
  comparisons. Independent curve and Gaussian option oracles check results. See the
  [comparison benchmark](dal-python/benchmarks/README.md#gsr-swap-and-swaption).

## 2026-09-28

- **One-factor Vasicek–Hull–White interest-rate Monte Carlo** — dated OIS and
  optional projection snapshots, piecewise `g/H`, exact event-grid conditional
  bond and discount calculations, and curve/volatility AAD risks are available
  through C++, Python, and Excel. Script `FIX` accepts rate indices, and the
  existing LSM engine now discounts each stochastic-rate path for Bermudan
  bond options and cash-settled swaptions. See
  [Gaussian Short Rate model](docs/models/gaussian-short-rate.md).

## 2026-09-27

- **Hybrid Monte Carlo accepts deterministic rate term structures** — a
  serializable logDF rate component drives multi-asset equity carry and the
  domestic numeraire, with node-level AAD risk, dated-curve snapshot factories,
  and C++, Python, and Excel interfaces. See [hybrid model](docs/models/hybrid-model.md).
- **Fuzzy AAD supports vector mutation inside `IF`** — tree and compiled
  script valuation now blend branch writes and `APPEND` results entry by entry,
  padding the shorter branch with zeros. See [script engine](docs/methodology/script_engine.md).
- **Multi-state Monte Carlo exercise policies** — script products can select
  up to three named equity outputs or scalar script variables for Bermudan
  continuation regression. Standardized cross-term bases, pivoted QR, held-out
  degree selection, frozen hard pricing, and fuzzy AAD work across C++, Python,
  and Excel; the product archive now preserves selected states in v3. See
  [LSM](docs/methodology/monte-carlo/lsm.md).
- **Named multi-asset scripts and deterministic-rate LSM** — correlated BS and
  hybrid models now value separate `FIX(EQ[...])` observations through public
  C++, Python, and Excel interfaces. Multi-asset `SPOT()` requires an explicit
  default index, which selects the single LSM regressor. LSM accepts models
  declaring a deterministic numeraire and uses model-declared parameter bounds
  for retrained-policy bumps. See [hybrid model](docs/models/hybrid-model.md)
  and [LSM](docs/methodology/monte-carlo/lsm.md).
- **Core Monte Carlo composes named factors in a hybrid model** — typed,
  serializable BS equity and deterministic-rate components share a named
  correlation provider and one numeraire; factor-aware Brownian bridging
  now supports multiple factors. Script and LSM multi-index integration
  remains a later stage. See [hybrid model](docs/models/hybrid-model.md).
- **Core Monte Carlo supports correlated equity Black-Scholes paths** — a
  serializable multi-asset model with named sample outputs, ordered factors,
  deterministic domestic numeraire, and AAD spot/vol/div/rate risks. Multi-factor
  Brownian bridge support was added in the subsequent hybrid-model stage;
  script/public multi-index valuation is not yet wired. See
  [correlated equity Black-Scholes](docs/models/correlated-bs.md).

## 2026-09-26

- **LSMC training is fused and parallel** — the backward phase takes one
  branch-free sweep per event and one sum-of-squares and one moment pass per
  regression, over fixed 8192-path chunks shared by idle pool workers. Training
  rows are no longer zero-filled on the calling thread. The weekly Bermudan put
  with 100k training and 100k pricing paths values in about 56-65 ms instead
  of 170-185 ms. Results stay independent of the thread count; regression sums
  over more than 8192 training paths now combine per chunk, so fitted
  coefficients can differ from earlier releases in the last bits (hard-mode
  PVs and exercise rates were unchanged on every configuration checked). See
  [LSM](docs/methodology/monte-carlo/lsm.md#c-example-and-path-counts).
- **Script construction skips regex compilation for identifier macros** —
  macro and `PeriodBegin`/`PeriodEnd` substitution with identifier names uses a
  case-insensitive literal replace, which cuts preprocessing of a weekly
  three-year schedule from about 450 us to 40 us with byte-identical output.
- **Day counts and dates are corrected at their edges** — `BOND`/`30_360` is
  now the 30/360 Bond Basis (ISDA 30/360), so accruals from or to the 31st
  change (for example 31 March to 30 June is 90 days, not 89). The new
  `THIRTY_360_US` basis, which now owns the `30_360_US` name, adds the 30/360
  US end-of-February rules. Annual `ACT_365L` periods no longer hang and use
  366 only when a 29 February falls in the period, and reversed `ACT_ACT`
  periods return the negated forward value. The bases match QuantLib's
  `Thirty360` BondBasis and USA conventions and `ActualActual` ISDA, and
  OpenGamma Strata's Act/365L. `Date_` construction and `AddDays`/`++`/`--`
  now throw outside 1970-01-01 to 2149-06-05 instead of returning invalid or
  wrapped dates, and `Date::Maximum()` is 2149-06-05. `String::FromDouble` and
  `Cell::ToString` use the shortest round-trip form (`1.5e-07`, not
  `0.000000`). See
  [dates and day counts](docs/methodology/dates.md#schedules-and-day-counts).
- **Date differences are inlined** — `Date::ToExcel` and `Date_` subtraction
  are now header-inline. In `ycinstrument_perf`, `curve_calibration_perf`,
  and `xccy_perf`, instrument pricing and discount-factor queries run about
  20-35% faster and curve calibrations about 5-30% faster.
- **Script adds numeric vectors and bounded loops** — mutable path-local vectors
  support indexed entries, `APPEND`, and reductions; product tables can define
  immutable numeric vectors. `FOR(index, start, end) ... END` expands bounded,
  half-open ranges during parsing. Tree and compiled valuation support double
  and AAD execution, historical replay, and LSMC. See
  [vectors and bounded loops](docs/methodology/script_engine.md#vectors-and-bounded-loops).

## 2026-09-25

- **LSMC exposes policy-sensitive AAD model risks** — the opt-in
  `RetrainedBump` mode adds a common-path policy-retraining secant to the
  frozen-policy adjoint for model parameters and script constants while preserving
  the default. See [early-exercise AAD](docs/methodology/script_engine.md#early-exercise-aad).
- **LSMC adds conditional randomized-QMC error estimates** — opt-in,
  independently digitally shifted Sobol pricing replicates use configurable
  seeds and report a standard error across replicate means. Deterministic
  Sobol remains the default. See
  [early-exercise valuation](docs/methodology/script_engine.md#early-exercise-valuation-lsmc).
- **LSMC gains an opt-in held-out degree selector and rank-revealing fallback** —
  `lsmc_validation_paths` selects a separate Sobol block for bounded degree
  selection before final pricing. Ill-conditioned moment fits use pivoted QR
  and retain the supported polynomial degree where possible. Diagnostics report
  the solver, effective rank, fallback reason, and validation loss. See
  [early-exercise valuation](docs/methodology/script_engine.md#early-exercise-valuation-lsmc).
- **Python binary releases add CPython 3.14 and macOS** — `dal-python` now
  supports CPython 3.9-3.14 (`Requires-Python: >=3.9,<3.15`) and publishes
  24 CPython-specific wheels across Linux x86-64, Windows AMD64, macOS Intel,
  and macOS Apple Silicon. See `dal-python/README.md`.

## 2026-09-23


- **LSMC trains and prices on disjoint path blocks** — hard and fuzzy/AAD
  valuation fit on the first M Sobol paths and value the frozen policy on the
  next N. `lsmc_training_paths` configures M independently of the pricing
  count N and defaults to N. Hard pricing reuses buffers per active batch after
  releasing training storage. A dedicated liveness visitor removes irrelevant script statements;
  continuation cashflows now follow the selected payoff receiver. Regression
  uses shared polynomial moments and checks rank before regularization. See
  [early-exercise valuation](docs/methodology/script_engine.md#early-exercise-valuation-lsmc)
  for the QMC, state-basis, and frozen-policy Greek limitations.

## 2026-09-20

- **Early-exercise (Bermudan/American) valuation is live end to end** — the
  `EXERCISE` statement reserved on 2026-09-19 now values: products divert to
  the LSMC driver (`dal-cpp/dal/script/lsmc.cpp`) in both tree-walk and
  compiled execution, with hard-decision double valuation (forward storage,
  backward induction with z-normalized monomial regression, explicit ridge,
  and degenerate guards, then frozen-policy valuation from the recorded
  paths)
  and fuzzy AAD valuation (recursive blend over frozen coefficients; the
  adjoint is the exact frozen-policy gradient, with the envelope remainder
  quantified in bump tests). Same-seed PV, coefficients, exercise rates, and
  AAD risks are bitwise invariant across thread counts. Exercise products
  accept only `rsg = "sobol"` (`UnsupportedRsgForExercise`). A simulation
  diagnostic joins the two existing entry points on all three ends:
  C++ `ExplainScriptSimulation`, Python `ScriptSimulation_Explain`, and Excel
  `SCRIPTSIMULATION.EXPLAIN`, all emitting the `dal.script-simulation/1`
  schema with per-exercise-event regression coefficients and exercise rates.
  Python gains the keyword-only `lsmc_basis_degree` setting; Excel gains the
  `MONTECARLOSETTINGS.NEW` `lsmc_basis_degree` key. Runnable examples:
  `dal-cpp/examples/american_put_mc/` and
  `dal-python/examples/013.exercise_bermudan.py`. See
  [early-exercise valuation](docs/methodology/script_engine.md#early-exercise-valuation-lsmc)
  and the
  [simulation diagnostic](docs/methodology/script_engine.md#product-archive-and-diagnostics).
- **Script: LSMC exercise correctness fixes** — the continuation regression and
  the exercise decision (hard and fuzzy) now use in-the-money paths only
  ($h_k > 0$), so a regression undershoot can no longer "exercise" worthless
  options; `num_cond_true_paths` in the simulation diagnostic accordingly
  reports the ITM condition-true count. The `;eps`/`:eps` smoothing suffix
  rejects non-positive widths at parse time (`InvalidSmoothing`), and the
  probe path behind the backward discount ratios is validated like every
  worker path. The Bermudan PDE test oracle's s=0 boundary anchors at the
  next exercise date (zero measured delta on the suite benchmarks).
- **Script: reuse recorded LSMC paths instead of replaying simulation** — the
  double driver's frozen-policy pass no longer regenerates paths: Phase A
  closes each path's rows with the terminal payoff value, and Phase C values
  the policy from storage (exercise-date numeraires come from the probe grid
  the backward induction already trusts). Black-Scholes products generate the
  forward pass through the fused checked-path fast path shared with the plain
  driver. PVs, exercise rates, diagnostics, and thread-count invariance are
  bitwise unchanged; the fuzzy/AAD driver still regenerates on tape.
- **Curve calibration hardening** — every calibration driver now rejects a
  finite-but-wrong solver effective inverse: the joint driver's
  J/T x eff ~ I mapping guard is shared, and single-curve, staged XCCY basis,
  and joint XCCY results publish an empty inverse
  (`not_available_for_mapping`, or `QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE`
  downstream) instead of silent garbage DV01s. The staged XCCY basis driver
  enforces the same post-solve convergence bar as its siblings
  (`ConvergenceError_` above ten times tolerance), and the joint batch node
  sweep answers an unknown component key with
  `CURVE_COMPONENT_UNAVAILABLE` instead of throwing `std::out_of_range`
  out of the batch. Quote-risk aggregation converts a non-finite running
  gradient to a `QUOTE_RISK_NON_FINITE_GRADIENT` provenance failure rather
  than throwing mid-batch.
- **Script settings validation single-sourced in dal-cpp** — the smoothing and
  LSMC basis-degree field checks (`ValidateSmoothing`,
  `ValidateLsmcBasisDegree`) join `ValidateRNG` as shared dal-cpp validators
  called from the Python and Excel bindings, the bindings' deliberately
  case-sensitive `today_fixing` parse moves to
  `Script::TryParseTodayFixingPolicy` (the Machinist enum parse stays
  case-insensitive), and the `smooth` / `lsmc_basis_degree` defaults are
  exported constants; error tokens and messages are unchanged on every layer.
  Both diagnostic schemas now emit the same six-field simulation echo:
  `dal.script-valuation/1` gains `lsmc_basis_degree`.
- **Python test coverage for early exercise and examples** — the bindings now
  value an `EXERCISE` product in-tree: a two-date Bermudan put pins the
  European/Bermudan ordering against the Black oracle and checks the fuzzy-AAD
  `d_spot` against a central finite difference in both engines, the simulation
  diagnostic's `num_cond_true_paths` is pinned to the exact Sobol counts
  (1926/1891 on the ATM put), and the numbered examples 010-013 run as
  subprocess smoke tests (`013` accepts `DAL_EXAMPLE_NPATHS` to shrink its
  path count, default unchanged). The single-curve benchmark fixture is
  repaired to the canonical well-posed shape (knot anchored at today, one more
  knot than instruments) so its provenance clears the calibration driver's
  effective-inverse guard.
- **C++ examples run in CI as CTest smoke tests; Excel suite values an
  EXERCISE product** — 22 self-contained `dal-cpp/examples/` binaries are
  registered under the `examples` CTest label and exercised at runtime on the
  gcc-14/AADet build leg (`european_mc` sits behind the off-CI
  `examples_slow` label), closing the compile-only gap behind commit
  05cd72c0's runtime abort; the `quote_risk` example's rank-deficient
  calibration fixture is repaired to the canonical anchored-knot shape so it
  joins the label. The Excel tests value a two-date Bermudan put against its
  European leg (European ≤ Bermudan PV) and pin the fuzzy-AAD result table's
  finite `d_spot`/`d_vol`/`d_rate`/`d_div` keys.
- **Review sweep: diagnostic cost, dead metadata, and benchmark hygiene** — the
  simulation diagnostic no longer burns a Monte Carlo run for products without
  `EXERCISE` (the LSMC driver is the only source of exercise statistics, so the
  discarded double simulation is skipped; the JSON contract is byte-identical).
  `NodeExercise_` drops its never-written discrete metadata and the debugger's
  unreachable discrete branch, model `Allocate` parses each distinct observed
  index once per timeline instead of once per sample date, and the LSMC
  recording seam is shared between the tree-walk and compiled engines through
  one set of sink kernels per mode (numerics bitwise unchanged). The Python
  comparison suite single-sources its valuation-date/day-count constants,
  labels third-party `prepared_pv` rows "passive (no prepared API)", and
  carries each backend's solver tolerances on calibration rows.

## 2026-09-19

- **BREAKING: `EXERCISE` is a reserved script keyword** — the statement grammar
  gains `EXERCISE <value> [IF <condition>]` for early-exercise (Bermudan,
  American) products, parsed and validated end to end: one top-level statement
  per event (`DuplicateExercise`, `UnsupportedExerciseNesting`), a dedicated
  dangling-IF error (`InvalidExerciseCondition`), exercise dates strictly after
  the evaluation date (`UnsupportedExerciseDate`), and sobol-only replay
  (`UnsupportedRsgForExercise`), all with input row/line context. Scripts and
  archived products that used `exercise` as a variable, macro, or
  constant-variable name now fail to parse with a `ReservedIdentifier` error
  carrying the source location and a rename hint; products that do not use the
  reserved word are unaffected. EXERCISE valuation (LSMC driver) is not enabled
  yet: evaluation reports `UnsupportedExecutionMode` until it lands. (The
  driver landed the next day; see the 2026-09-20 early-exercise entry.) See
  [the script engine grammar](docs/methodology/script_engine.md#reserved-keywords-and-variables).

- **Script settings gain `lsmcBasisDegree_`** — `MonteCarloSettings_` carries
  the LSMC regression basis degree (default 3), validated by
  `ValidateSimulationSettings` as an integer between 1 and 8
  (`InvalidSetting: InvalidLsmcBasisDegree`). The Python and Excel projections
  of the field land with the EXERCISE valuation driver.

- **BREAKING: model bindings removed; FIX is managed by index name** — deleted
  `ScriptValuationSettings_::modelBindings_` and `Dal::ModelIndexBinding_` along
  with the `UnknownModelAsset`/`DuplicateModelBinding`/`ConflictingModelBinding`/
  `MissingModelBinding`/`AmbiguousModelBinding` errors. Every model-sourced
  `FIX` now binds the model's `spot` output to the script's own future FIX
  index; several distinct future indices fail with `MultipleModelIndices`.
  The Python `model_bindings` settings argument is removed (passing it raises
  `TypeError`), and Excel `SCRIPTVALUATIONSETTINGS.NEW` loses its third
  argument, becoming `name, [settings], [fixings]`. Explain diagnostics keep
  the `model_bindings` field as the effective model index. See
  [the script model index](docs/methodology/script_engine.md#the-script-model-index-and-legacy-spot).

## 2026-09-15

- **Excel FIX settings and diagnostics** — added immutable product, valuation,
  and simulation settings handles, `PRODUCT.NEWWITHSETTINGS`, and
  `MONTECARLO.VALUEWITHSETTINGS` for explicit dates, today policy, model
  bindings, snapshots, and compiled/AAD execution. Strict two-column ranges
  retain row/column errors; `MARKETFIXINGSNAPSHOT.NEW(,,)` creates an
  authoritative empty snapshot. `PRODUCT.DESCRIBE` and `SCRIPTVALUATION.EXPLAIN`
  return complete native JSON in text-column chunks. Existing product and
  seven-input Value formulas keep their signatures and two-column PV/risk
  output. See the [Excel FIX guide](docs/excel/script-settings.md).

- **Python FIX settings and diagnostics** — added keyword-only product settings,
  `MonteCarlo_ValueWithSettings`, and three native settings classes with validated
  properties and copy/deepcopy support. Explicit dates, exact today policies,
  model-binding dictionaries and immutable snapshots feed common C++ preparation.
  High-level `Product_Describe` / `ScriptValuation_Explain` return dictionaries;
  low-level bindings return the corresponding JSON strings. Explain performs
  independent default price preparation without workers or a subsequent Value
  cache. Valid legacy product and three-to-eight-argument valuation calls remain
  compatible. **Input validation:** both Python valuation entries require an
  integer path count in `1..2147483647`, excluding bool/enums, and finite positive
  smoothing; invalid inputs retain field/constraint errors. See the
  [Python FIX reference](dal-python/README.md#historical-and-future-fix).

- **Public C++ script settings and diagnostics** — exposed named FIX valuation
  with typed contract/valuation/simulation settings, explicit dates and immutable
  snapshots, while preserving valid product-three and valuation-three-to-eight
  argument calls. Both valuation overloads use fresh common preparation;
  `smooth` must be finite and strictly positive. Archive v2 preserves original
  contract text and optional default-index identity, retains the v1 reader, and
  excludes runtime market state. Describe /2 inspects syntax without history,
  model, or valuation-date access; Explain /1 performs default price preparation
  without workers or a cache for subsequent Value. Legacy debug /1 rejects FIX
  and nonempty defaults with a Describe migration hint. Rebuild consumers and
  bindings against matching core/public libraries; old binaries are not promised
  v2 compatibility. Python and Excel project these settings and diagnostics as
  described in the two binding entries above. See the
  [public settings](docs/methodology/script_engine.md#public-c-settings) and
  [archive/diagnostic contracts](docs/methodology/script_engine.md#product-archive-and-diagnostics).

- **Script compiled observations** — added named compiled double and fuzzy AAD
  valuation using the same sealed observation plan as tree execution. Hard
  historical bytecode preserves typed parameter-dependent state and discards
  settled payments; future optimization retains live parameters and fractional
  fuzzy weights. Exact and fuzzy model-aware preparation retain future branches
  and valid tiny-divisor arithmetic without tolerance-domain processing;
  literal/history constant arithmetic can still fold. Preparation
  fixes execution mode and smoothing, prefetches history before optimization,
  and builds requested bytecode before workers start.
  Named valuation settings remain core-only; public C++ facade, Python, Excel,
  archive, and JSON debug projections are not extended. See the
  [compiled evaluation contract](docs/methodology/script_engine.md#tree-walk-and-compiled-evaluation).

## 2026-09-14

- **Script historical AAD state** — added model-aware prepared AAD tree
  valuation that preserves script-parameter risk through historical fixing
  expressions. Each worker recording rebuilds a typed historical seed from
  sealed doubles, with hard historical decisions and fuzzy future conditions.
  Fresh path-local payoff roots preserve accumulated risk for direct seed and
  constant payoffs; batch contributions are normalized once by total paths.
  At introduction, named compiled and prepared AAD compiled valuation remain
  unsupported, as do named valuation settings in the public C++ facade,
  Python, and Excel.
  See the [AAD tree contract](docs/methodology/script_engine.md#core-aadtree-fixing-valuation).

- **Script core FIX valuation** — added model-aware double/tree valuation with
  an explicit `spot` to one ordinary EQ binding in Black-Scholes or Dupire.
  Fixings are retained across events, payments use their own numeraires, and
  sealed historical values seed past state without adding settled payments.
  Today-only zero-dimensional paths and wholly expired zero returns are
  supported. At introduction, named AAD, compiled, fuzzy, and
  public-facade/Python/Excel valuation remain unavailable.
  **Compatibility:** unbound historical
  `SPOT()` now raises `UnboundHistoricalSpot` instead of using the placeholder
  value 30; model-aware preparation accepts an explicit default index, shared
  by matching SPOT/FIX requests. Legacy AAD rejects nonexpired products with
  past events because historical AAD state reconstruction is unavailable.
  Future-only legacy SPOT modes and defaults are preserved. See the
  [core valuation contract](docs/methodology/script_engine.md#core-doubletree-fixing-valuation).

## 2026-09-13

- **Script historical preparation** — added core `PrepareScript` with a
  captured evaluation date, exact-midnight observation keys, and deduplicated
  EQ/FX history resolved through virtual index fixings into immutable values.
  Explicit snapshots are authoritative; sequential global capture is not
  atomic and does not support concurrent fixing writes. Today defaults to
  model, with an explicit historical policy. At introduction, prepared
  simulation supported only the wholly expired zero path; nonexpired execution
  raised `UnsupportedExecutionMode`. The public C++ facade, Python, and Excel
  valuation entries rejected FIX. **Compatibility:** event partitioning
  now occurs at preparation rather than core parsing. Public debug wrappers
  explicitly partition fresh copies at one captured date, preserving JSON/tree
  phases and live-only legacy text. See the
  [preparation contract](docs/methodology/script_engine.md#historical-fixing-preparation).

- **Fixing-history C++ identities** — renamed the map wrapper in
  `dal-cpp/dal/indice/fixings.hpp` from `Dal::FixHistory_` to
  `Dal::IndexFixHistory_`, removing a conflicting definition that could cause
  cross-translation-unit destructor corruption. **Breaking source/ABI change
  for direct core C++ consumers:** migrate map-wrapper type names and nested
  `vals_t` references to `IndexFixHistory_`, including explicitly typed results
  of `FixHistory::Empty()`; callers using `auto` for that result need no source
  edit. The vector aggregate `Dal::FixHistory_` in
  `dal-cpp/dal/storage/globals.hpp` retains its name and layout; there is no
  compatibility alias for the map wrapper. Cleanly rebuild DAL and all
  dependent C++ objects and binaries, including bindings and executables.
  Do not mix objects built against the old conflicting definitions with
  repaired objects. Lookup behavior, public-facade/Python/Excel signatures,
  and serialized fixing records are unchanged. See the
  [current container contract](docs/methodology/index_parsing.md#fixing-history-containers).

## 2026-09-12

- **Script FIX syntax** — added parsing for unquoted `FIX(index[,date])`,
  preserving complete EQ/FX names, delivery suffixes, and source context through
  macro and schedule expansion. **Breaking:** `FIX` is reserved; existing
  variables or definitions with that name must be renamed. At introduction,
  named execution raised `PreparationRequired` and no fixing-preparation API
  was available. See the [syntax and execution limit](docs/methodology/script_engine.md#named-fixing-syntax).

- **Prepared rate-trade pricing** — C++ and Python can retain immutable IRS/OIS/basis
  coupon geometry across PV and AAD node-risk calls while evaluating current
  markets and fixings on every call. Third-party performance comparisons include
  prepared, market-update and cold-construction PV cases. See the
  [prepared pricing contract](docs/yield-curves/node-risk.md#repeated-pricing-with-prepared-trades).

## 2026-09-11

- **Joint quote-risk base graphs and eligibility** — joint risk follows actual
  consumed base paths, including unregistered XCCY forecast curves, with
  historical native coordinates preserved. Necessary nodes must be exact
  builtin curve types. This intentionally makes opaque unit-discount leaves
  and builtin subclasses ineligible even when their previous numerical results
  were correct; standalone node risk and v1 quote-risk semantics are unchanged.
  See the [graph contract](docs/yield-curves/joint-quote-risk.md#aggregation-and-failures).

## 2026-09-08

- **Generic joint quote-space DV01** — exact same-currency joint calibration can
  retain a full coupled effective inverse and expose immutable v2 quote-risk
  provenance through C++, Python, and Excel. Explicit inverse requests define
  a fixed initial-Jacobian subspace for underdetermined systems; default solves
  and existing v1 domains retain their behavior. See the
  [mapping and units contract](docs/yield-curves/joint-quote-risk.md).

## Existing methodology and capabilities

These are documented today and represent the current documented surface; they are listed
here as the baseline rather than dated releases:

- **Automatic Adjoint Differentiation (AAD)** — built-in native reverse-mode AD
  for risk sensitivities. See `docs/methodology/aad.md`.
- **Yield Curve Construction** — discount-factor / forward-rate parameterised curves
  calibrated to market instruments. See `docs/yield-curves/construction.md`.
- **Underdetermined Search** — constrained least-change solver for over-parameterised
  nonlinear calibration. See `docs/methodology/underdetermined_search.md`.
- **Cross-Currency Pricing and Calibration** — fixed, resettable, and mark-to-market
  swap pricing with immutable timestamped rate/FX fixing snapshots, staged basis
  fitting, simultaneous domestic/foreign/basis calibration, and named joint
  parameter/residual ranges. See `docs/ccy-curves/pricing-calibration.md`.
- **Interpolation** — linear, log-linear, cubic-spline, and mixed 1D interpolators plus
  bilinear 2D interpolation. See `docs/methodology/interpolation.md`.
- **Log-Discount Curve** — node log-discount-factor parameterisation with `LogDfScheme_`
  interpolation schemes and scalar-generic passive/AAD evaluation. See
  `docs/yield-curves/log-discount.md`.
- **Yield-Curve Jacobian and Inverse-Jacobian Risk** — AAD forward Jacobians for every
  implemented curve representation subject to the normal eligibility gates, plus the
  inverse-Jacobian IR-risk transform and its `effJacobianInverse_` unit convention. See
  `docs/yield-curves/jacobian-risk.md`.
- **Script Engine** — events-table to AST pipeline, visitor passes (domain analysis,
  constant-condition folding), and the fuzzy evaluator for pathwise AAD through
  discontinuous payoffs. See `docs/methodology/script_engine.md`.

## 2026-09

- `curve`: Added production quote-space portfolio DV01 for exact single-curve,
  simultaneous XCCY, and staged XCCY basis calibrations. Immutable provenance
  publishes ordered axes and SHA-256 state fingerprints; aggregation produces
  price-per-decimal sensitivities and DV01 by actual PV currency without quote
  bumps, FX conversion, or recalibration. C++, Python, and Excel expose the same
  supported domains, units, and explicit unavailable/failure states. Independent
  full-recalibration oracles gate every quote for 5/10/16-width ANALYTIC and
  BUMPED fixtures, and `rate_risk_perf` gates single, joint-XCCY, and staged-basis
  steady-state aggregation. See `docs/public-api.md` and
  `docs/yield-curves/jacobian-risk.md`.

## 2026-08

- `curve`: XCCY node-risk failure token corrected for unresolvable markets: when the
  cross-currency market cannot be resolved (no `xccyMarket_`, or a block the config cannot
  route), `RateTradeNodeSensitivities` now reports the passive-pricing failure as
  `TRADE_VALIDATION_FAILED` instead of `TRADE_DOES_NOT_DEPEND_ON_COMPONENT`. Expired XCCY
  trades are unaffected (they keep the dependency token). The `rate_risk_perf` benchmark
  gains the contract-8 XCCY case (24 XCCY x 5 consumed components).

- `curve`: batch and portfolio-aggregation node-risk APIs close out the seven-family AAD node
  risk feature. `RateTradeNodeSensitivitiesBatch(trades, market, componentKeys)` sweeps the
  Cartesian product of a trade list and one shared component key list serially and
  deterministically, returning exactly the single-trade result shape per (trade, component) cell
  with per-entry failure isolation and nothing thrown; passive pricing is hoisted to one passive
  PV per trade and classification/preparation to one per curve. `AggregateRatePortfolioNodeRisk`
  sums the successful entries into one dense `Report_` per component (node count and order from
  `BuildCurveParameterLayout`, header rows from `DescribeCurveFreeParameters`), groups PV totals
  by each trade's actual PV currency under the `UnconvertedByActualPvCcy` policy (no FX
  conversion), and carries failures and currencies in a parallel meta table. The Python bindings
  expose both keyword-only with read-only results and one GIL release per batch; the Excel add-in
  gains the rate trade/market builders and long-form spills
  (`trade, component, reason, pv, node, value`, plus currency on aggregate rows). A
  `rate_risk_perf` benchmark joins the paired regression-gate subset.

- `curve`: `RateTradeNodeSensitivities` success domain widened to XCCY trades (any consumed curve
  registered under a component key — the collateral/tenor-selected domestic/foreign discount and
  forecast curves plus the basis curve), completing the seven-family P0 success domain. XCCY
  addressing locates block slots by pointer identity against `curveComponents_`; the active stage
  rebuilds every consumed curve as `AAD::Number_` and registers only the addressed component's
  parameters (the FX spot stays a constant, so XCCY node risk ships rate axes only). A market-aware
  `BuildRateCashflowPlan(trade, market)` overload emits the XCCY dependency keys, and `PriceXccy`
  now carries the fixing accounting so a missing fixing returns `TRADE_VALIDATION_FAILED`. The
  six-token failure set and its priority are unchanged.

- `curve`: `RateTradeNodeSensitivities` success domain widened to basis swap trades (any of the three
  dependencies: spread forecast, reference forecast, or discount), on top of Deposit/FRA/Future/OIS/IRS;
  the generalized multi-component stage prices them unchanged, holding the two non-target dependencies
  as passive `double` curves. XCCY remains gated by `TRADE_FAMILY_NOT_AAD_ENABLED`; the six-token
  failure set and its priority are unchanged.

- `curve`: `RateTradeNodeSensitivities` success domain widened to OIS and IRS trades (either
  dependency, forecast or discount), on top of Deposit/FRA/Future; the stage-1 generalized
  multi-component machinery prices them unchanged. OIS overnight compounding runs on the
  active tape only through the forecast curve's parameters. Basis/XCCY remain gated by
  `TRADE_FAMILY_NOT_AAD_ENABLED`; the six-token failure set and its priority are unchanged.

- `curve`: `RateTradeNodeSensitivities` success domain widened from Deposit-only to FRA and
  Future trades (per requested dependency: FRA forecast/discount, Future forecast), via a
  multi-component AAD stage that registers only the target component's parameters and holds
  every other dependency as a passive `double` curve. OIS/IRS/Basis/XCCY remain gated by
  `TRADE_FAMILY_NOT_AAD_ENABLED`; the six-token failure set and its priority are unchanged.

- `script`: Added two script-product debug dumps alongside the legacy
  s-expression one: `DebugScriptProductJson` / `Product_DebugJson` emit a
  versioned JSON AST (schema `dal.script-product/1`) for machine consumers
  such as web front ends, and `DebugScriptProductTree` / `Product_DebugTree`
  render a width-aware Unicode (or ASCII) tree for humans. Both include past
  events and resolved variable/constant tables. See
  `docs/methodology/script_engine.md` § Product Debug Outputs.

- `script`: Added runnable demos for the tree debug dump:
  `dal-cpp/examples/script_tree/` (C++, via `ScriptProduct_::DebugTree`) and
  `dal-python/examples/008.script_tree.py` (via `Product_DebugTree`). Each
  renders a UOC script product at the default and a narrow width -- showing
  inline statements expanding into box-drawing branches -- plus the ASCII
  fallback for constrained consoles. See
  `docs/methodology/script_engine.md` § Product Debug Outputs.

- `web`: Removed `dal-web/` from this repository. The portfolio management web
  UI now lives at [wegamekinglc/dal-web](https://github.com/wegamekinglc/dal-web)
  and depends on the published `dal-python` PyPI package. The
  `web-calibration-performance` workflow and the web CI jobs were removed with it.

- `python`: Expanded the public `dal-python` compatibility contract to
  CPython 3.9-3.13 (`Requires-Python: >=3.9,<3.14`) and the complete Linux x86-64/Windows
  AMD64 release from eight to ten CPython-specific wheels. See
  `dal-python/README.md` and `docs/installation.md`.

- `matrix`: Hardened the BCG Krylov solver internals. The exact scaled-`alpha`
  limb arithmetic moved into shared helpers under
  `dal-cpp/dal/math/matrix/bcg_scaled_alpha.inc`, scaled-norm square sums now
  share a single reliability check with a slow exact fallback, and solver
  workspace construction is validated against boundary allocation sizes. JSON
  archive string validation (NUL and UTF-8 checks) is unified across raw and
  decoded strings, and the Excel joint-XCCY result getter validates attribute
  names through one view table instead of a per-branch if-chain. Public
  signatures and bindings are unchanged. See `docs/methodology/matrix.md`.

- `script`: The script parser now rejects extra `DCF` arguments. The parser
  previously skipped every token after the third `DCF` argument, so malformed
  calls passed validation; the argument list must now be consumed completely,
  and excess arguments throw `ScriptError_`. Correct usage is unaffected.

## 2026-07

- `curve`: Added immutable native rate-cashflow planning and pricing for
  `DEPOSIT`, `FRA`, `FUTURE`, `OIS`, `IRS`, `BASIS_SWAP`, and `XCCY`,
  including explicit historical rate/FX fixing demand, snapshot admission,
  passive and AAD valuation, and first-order node sensitivities. The same
  typed batch surface is additive in public C++ and Python. See
  `docs/public-api.md` and `docs/curve-lab.md` (moved to
  https://github.com/wegamekinglc/dal-web).

- `web`: Added the Curve Lab DAL-WEB workflow for visual seven-family
  authoring with latest-request-wins canonical quote application and
  server-authoritative Decimal/round-half-even display rendering, immutable
  asynchronous build/import/risk runs, native `Storable_` JSON and `Bag_`
  version persistence, dependency/fixing provenance, exact quote axes,
  PV/DV01/KRD results, and replayable sensitivity matrices. See
  `docs/curve-lab.md` (moved to https://github.com/wegamekinglc/dal-web) and
  `dal-web/README.md` (now at https://github.com/wegamekinglc/dal-web).

- `matrix`: Added exact scaled-`alpha` candidate combination for `Sparse::CGSolve`
  and `Sparse::BCGSolve`. When the standalone binary64 coefficient is unsafe, the
  stored quotient remains exact until each complete solution, residual, or BCG
  shadow-residual expression is rounded once; finite cancellation and subnormal
  candidates are accepted, while genuinely non-finite candidates still fail before
  the atomic commit. The existing `beta/betaPrev` direction-ratio path is unchanged,
  and solver-level FTZ validation is limited to the S3/S5 first-iteration cases.
  Public signatures and bindings are unchanged. See `docs/methodology/matrix.md`.

- `matrix`: Made `Sparse::CGSolve` and `Sparse::BCGSolve` scale-safe across the
  finite binary64 range. Norm and convergence classification no longer relies
  on overflowed or underflowed intermediates, and ambiguous signed dot products
  use exact accumulation. Candidate updates are committed only after callback
  validation; candidates that appear converged additionally require direct
  residual confirmation. Public signatures and bindings are unchanged; extreme
  finite systems that previously reported false convergence or avoidable
  breakdown now solve or fail closed. See `docs/methodology/matrix.md`.

- `curve`: Added staged XCCY sensitivity diagnostics across public C++, Python,
  and Excel. The additive options overload selects analytic or bumped
  Jacobians and independently controls the forward and effective-inverse
  matrices while preserving the one-argument defaults. Diagnostics retain the
  instrument and basis-knot axes plus explicit availability, tolerance, and
  scaling metadata; the effective inverse is `solver_scaled`, so raw decimal
  quote bumps map as `dx = E * dq / tolerance`. Public C++ and Excel also expose
  the retained joint XCCY effective inverse. See
  `docs/ccy-curves/pricing-calibration.md`,
  `docs/yield-curves/jacobian-risk.md`, and `docs/public-api.md`.

- `curve`: Made calibration settings dictionaries strict on the Python and Excel
  surfaces. `dal.calibrate_curve` raises `ValueError` on an unknown settings key,
  and the Excel single-curve, staged-XCCY, and joint-XCCY settings parsers throw
  via `RequireKnownSettingsKey`; both name the offending key and the accepted set.
  Unknown keys were previously ignored silently, so a misspelled key now fails
  loudly instead of calibrating with defaults. Correct usage is unaffected.

- `core`: Migrated owning factory returns from raw pointers to `std::unique_ptr`
  across the `dal-cpp` core headers: the interpolation factories
  (`Interp::NewLinear`/`NewLinear2`/`NewLogLinear`/`NewCubic`, `NewMixedLogDF`),
  the PDE coordinate-map, coefficient, and derivative-operator factories, the
  random generators (`Random_::Clone`, `PseudoRandom_::Branch`/`New`,
  `SequenceSet_::TakeAway`, `NewSobol`), the direct curve factories
  (`NewDiscountPWC`/`ZeroRate`/`PWLF`/`LogDF`, `YCComponent_::Clone`,
  `BuildCurveCalibrationWeights`), the sparse decompositions, the matrix-writer
  helpers, the index parsers, `Underdetermined::Function_::Gradient`,
  `ModelData_::MutantModel`, and `Environment_`/`Composite_` iteration and
  cloning. `Handle_` gained a converting constructor from `std::unique_ptr`.
  **Breaking** for direct consumers of the `dal-cpp` core headers — code holding
  raw results or calling `.reset()`/`.release()` must now take the `unique_ptr`;
  `dal-public`, Python, and Excel signatures are unchanged. The
  Machinist-generated `Archive::Reader_::Build()` interface keeps its raw return
  and migrates in a follow-up.
- `matrix`: Fixed the `Matrix_` move constructor to value-initialize `cols_` —
  a moved-from matrix previously carried an indeterminate column count (latent
  UB present since 2022) and now has defined zero dimensions.
- `model`: Fixed Black-Scholes clone construction to initialize its
  pre-allocation timeline state; cloning before `Allocate()` no longer copies
  indeterminate state. Public constructor and binding signatures are unchanged.

- `aad`: Fixed multi-result adjoint propagation on the native backend. Reverse
  sweeps now dispatch to the vector-adjoint path (`TapNode_::PropagateAll`)
  whenever `SetNumResultsForAAD(true, m)` is active; previously every sweep took
  the scalar `PropagateOne` path, so all $m$ result adjoints silently stayed
  zero. Consumed multi-mode adjoints are zeroed after propagation — the same
  discipline `PropagateOne` already applied to scalar adjoints — so repeated
  sweeps no longer re-propagate stale slots, and the multi-mode state
  (`Tape_::multi_`, `Tape_::numAdj_`) moved from process-global statics to
  per-tape members so tapes configured with different result arities no longer
  interfere. The `SetNumResultsForAAD` scope guard now restores the enclosing
  mode on destruction instead of forcing scalar defaults. The Adept, XAD, and
  CoDiPack backends are unaffected. See `docs/methodology/aad.md`.

- `time`: Made `Date_` construction strict. The year must lie in [1900, 2199],
  the month in [1, 12], and the day within the actual length of the given month
  (leap years included). Invalid triples such as `Date_(2023, 2, 30)` previously
  normalized silently through serial-date arithmetic; they now throw
  `Exception_` via `REQUIRE`. **Breaking behavior change** visible identically
  through C++, the Python `dal.Date_` binding, and all date-taking Excel
  functions.

- `curve`: Added reset-aware cross-currency pricing and simultaneous domestic,
  foreign, and basis calibration. Fixed, resettable, and mark-to-market notional
  modes replace the prior boolean configuration; explicit rate/FX fixing identities
  and one immutable timestamped snapshot support already-started swaps; and the
  joint solver exposes named parameter/residual ranges plus analytic or bumped
  Jacobians. Public C++ and joint Python expose both retained joint matrices. Excel
  exposes the joint forward Jacobian and ranges, but has no worksheet getter for the
  effective inverse. **Breaking:** the two
  `CrossCurrencyConvention_` booleans `resettableNotional_` and
  `markToMarketNotional_` are replaced by the enum in
  `CrossCurrencySwapConfig_`. The legacy fixed-notional convenience constructor
  and builder remain compatible. See `docs/ccy-curves/pricing-calibration.md`,
  `docs/yield-curves/jacobian-risk.md`, and `docs/public-api.md`.

- `curve`: Added persistent continuously compounded `ZERO_RATE` curves. Future-node
  rates map to `logDF = -z * YearFrac(anchor,node)` and reuse all shared log-DF
  interpolation/extrapolation schemes; the anchor has no free zero-rate parameter.
  Single, staged, and joint calibration support ZERO_RATE with passive or active base
  layering and AAD analytical Jacobians in future-node zero-rate order. The additive
  `DiscountZeroRate_v1` archive preserves representation and bump coordinates, and direct
  factories are available in core C++, public C++, Python (`DiscountZeroRate_New`), and
  Excel (`DISCOUNTZERORATE.NEW`). See `docs/yield-curves/construction.md`,
  `docs/yield-curves/jacobian-risk.md`, and `docs/public-api.md`.

- `curve`: Unified passive and AAD curve construction across piecewise-constant forwards,
  piecewise-linear forwards, and log-discount curves. Linear and natural-cubic interpolation
  now separate passive geometry from typed ordinates, all log-DF schemes share one boundary
  and extrapolation implementation, and both single and joint calibration can use AAD-derived
  analytic Jacobians for every implemented representation. Joint declarations may mix methods
  and base-layer any implemented forward representation over an actively calibrated discount
  curve while preserving declaration-order solver columns. Newly analytic PWC/PWL
  `APPROXIMATE` solves can select a different tolerance-satisfying curve than the historical
  bumped path because the underdetermined solver stops at `fitTolerance_`; callers requiring
  historical curve-level reproduction must select `BUMPED`. At that point, `ZERO_RATE` was
  deliberately outside the unified factory; the later entry above adds it without changing
  the other representation contracts.
  See `docs/methodology/interpolation.md`, `docs/yield-curves/log-discount.md`, and
  `docs/yield-curves/jacobian-risk.md`.

- `numerics`: Corrected three output-affecting quantitative contracts: rate-aware
  Dupire now prices a discounted spot call and includes the strike in
  $(r-q)K C_K$; the exact underdetermined solver uses the quadratic model's
  $k=(c-b)/(a-2b+c)$ backtrack fraction; and Bachelier pricing/implied volatility
  now supports all real forward/strike pairs with a finite, nonnegative
  price-unit bracket and translation-invariant tolerances.
  See `docs/models/dupire.md`, `docs/methodology/underdetermined_search.md`,
  and `docs/models/black-scholes.md`.
- `runtime`: Made Monte Carlo reject non-positive path counts at the public boundary,
  made the DAL thread pool lazy and configurable with `DAL_NUM_THREADS`, and added
  size-safe batching with thread-local active AAD models and propagated task failures.
  Stopping the pool waits for work already claimed by a worker or caller and cancels
  work still queued. Native valuation now excludes concurrent evaluation-date mutation
  while allowing date getters to progress; the Python valuation and date bindings
  release the GIL around synchronized native work. Python `DoubleMatrix_` now supports
  rectangular nested-list construction and mutable indexing, enabling non-flat Dupire
  surfaces through Python and the web gateway.
- `build`: Added relocatable `DAL::cpp` / `DAL::public` CMake packages and an
  installed-consumer check, including exported MSVC runtime metadata and a helper for
  matching consumer targets; added `core-dev`, `full-dev`, and portable `distribution`
  profiles; moved the automated Linux install into `build/stage`; and made native-CPU
  tuning opt-in through `DAL_ENABLE_NATIVE_ARCH`.
- `web`: Added persistent single-curve, staged-XCCY, and joint-XCCY calibration
  APIs and the Curve Lab workbench. Versioned run and instrument records plus
  reconstructible curve rows preserve inputs, execution evidence, fit and matrix
  diagnostics, FX forwards, and the effective inverse without persisting native
  handles. Base curves are referenced by ID and recursively expanded on read;
  quote-bump previews are calculated per GET request from the persisted effective
  inverse and are not stored. Completed results survive a database-backed restart;
  orphaned running calibrations become failed on startup. See `dal-web/README.md`
  (now at https://github.com/wegamekinglc/dal-web).
- `web`: Defined the backend as native-only and added startup preflight checks that
  preserve and validate the locally installed `dal` package before Uvicorn starts.

- `curve`: Added opt-out controls for exact-calibration diagnostic matrix construction:
  `CurveCalibrationOptions_::computeEffJacobianInverse_`,
  `CurveCalibrationOptions_::computeForwardJacobian_`, and
  `JointMultiCurveCalibrationOptions_::computeJacobianAtSolution_`. Defaults preserve the existing
  diagnostics surface, while performance-sensitive callers can run solve-only calibrations. See
  `docs/yield-curves/construction.md` and `docs/yield-curves/jacobian-risk.md`.

- `pde`: Implemented the `Rollback_`-based PDE framework: coefficient factories and callable
  adapters, endpoint-exact concentrating coordinate maps, grid materialization, node-location
  derivative operators, and `ThetaScheme_` with explicit `Prepare`/decomposition reuse. The old
  mesher/`FD1D_` stack was removed, and `european_fd` plus `pde_perf` now use the new framework.
  See `docs/methodology/pde/framework.md`. Breaking for direct `dal-cpp` PDE internals only; no
  `dal-public`/Python/Excel surface changed.

- `script`: The compiled (flat-stream) evaluator is now at strict capability
  parity with the tree-walk evaluators while `MCSimulation` keeps tree-walk as
  the default (`compiled=false`) in both specializations (`<double>` and
  `<AAD::Number_>`; `dal-cpp/dal/script/simulation.hpp`): fuzzy smoothing
  (call-spread/butterfly
  kernels shared via `dal-cpp/dal/script/visitor/smoothing.hpp`, dt-blend `FuzzyIf`,
  per-condition `eps` overrides, `maxNestedIfs`), const variables (live `ConstVar`
  opcode preserving const-var greeks), past events, and `NodeCollect_` all produce
  the same numbers (tol 1e-8) through either path. `ScriptProduct_::Compile(fuzzy)`
  is now `const` and returns a `ScriptCompiled_` artifact; `MCSimulation` compiles
  internally when requested, and `compiled` is `std::optional<bool>` (unset =
  tree-walk / `false`).
  Exposed through `ValueByMonteCarlo` (`dal-public/src/value.hpp`) and the Python
  `MonteCarlo_Value` binding as a backward-compatible `compiled` keyword. **Breaking
  API behavior**: (1) `AND`/`OR` are now eager in ALL evaluators — both
  operands always evaluate; scripts must not rely on short-circuit (condition
  expressions in this grammar are side-effect-free, so parseable scripts are
  unaffected); (2) the
  compiled evaluator remains opt-in with the same numbers and ~20-25% faster
  runtime in the benchmarked path; (3) `Compile()` signature/semantics changed
  from mutating member streams to a const artifact factory. See
  `docs/methodology/script_engine.md`.

- `random`: Sobol normal draws default to the fast Acklam inverse-CDF path;
  precise-CDF Newton correction is explicitly opt-in through `precise=true,
  polish=true` on the core, public C++, Python, and Excel constructors. Fast-CDF
  Newton polish (`precise=false, polish=true`) remains separately opt-in, and
  Sobol clones preserve sequence state and both policy flags. Pseudo-random
  normal draws retain their precise default. See `docs/methodology/monte-carlo/sampling.md`.

## 2026-06

- `curve`: Added a yield-curve Jacobian example demonstrating AAD-vs-bump agreement and the
  inverse-Jacobian IR-risk transform; documented the corrected units of
  `CurveCalibrationDiagnostics_::effJacobianInverse_` as
  `d(params)·tolerance_ / d(decimal-rate perturbation)` (the underdetermined solver scales
  residuals by `1/tolerance_` before forming the pseudoinverse, so consumers must divide by
  `tolerance_` when transforming a sensitivity vector: `r = gᵀ · effJacobianInverse_ / tolerance_`).
  See `docs/yield-curves/jacobian-risk.md` and the example at
  `dal-cpp/examples/yield_curve_jacobian/`. Non-breaking (new example + diagnostics-only test).
- `curve`: Exposed the calibration forward Jacobian on the public diagnostics struct as
  `CurveCalibrationDiagnostics_::jacobian_` (and the `CrossCurrencyCalibrationDiagnostics_` mirror,
  empty on the xccy path for now) — the unscaled analytic `d(modelRate)/d(logDF_free)` at the solved
  point, populated iff `jacobianMode_ = ANALYTIC && solveMode_ = EXACT` and eligible. The
  `yield_curve_jacobian` example now reads the AAD Jacobian from `result.diagnostics_.jacobian_`
  instead of the `TestOnly::AnalyticJacobianAt` helper, and a new test
  (`dal-cpp/tests/curve/test_forward_jacobian_diagnostics.cpp`) gates population and AAD-vs-bump
  agreement. Non-breaking (additive public field).
- `curve`: Added a joint multi-curve AAD analytic Jacobian for `CalibrateJointMultiCurve`
  (`dal-cpp/dal/curve/jointcalibration.cpp`). The `JointResidualFunction_::Gradient` override
  produces a backend-neutral dense Jacobian via a single-result reverse sweep over the joint
  stacked parameter vector, using three new tape primitives: `Tape::DiscountPWLF_<T_,B_>`
  (PWL-forward curve with templated base handle, `dal-cpp/dal/curve/ycpwlf.hpp`),
  `Tape::JointCurveBlock_<T_>` (multi-curve routing context, `dal-cpp/dal/curve/jointycctx.hpp`),
  and `Tape::JointRate_<T_>` (projection-capable rate base, `dal-cpp/dal/curve/jointrate.hpp`).
  Eligible for specs whose declarations are all `PIECEWISE_LINEAR_FWD` with vanilla instruments
  (`Deposit_`, `FRA_`, `Future_`, `Swap_` -- `OISSwap_` included) and `liborBasis_ == ACT_365F`;
  base-layered forward curves propagate OIS adjoints through a templated `Number_`-typed base
  handle. The `JointMultiCurveCalibrationOptions_` struct carries a `jacobianMode_` field defaulting
  to `ANALYTIC` (matching single-curve), and `JointMultiCurveCalibrationResult_` now carries
  `jacobianAtSolution_` (populated under `ANALYTIC && EXACT && eligible`). The shared
  `XCurveJacobian_` (`dal-cpp/dal/curve/curvejacobian.hpp`) serves both the single-curve and joint
  paths. All four AAD backends (native, Adept, XAD, CoDiPack) verified with 750/750 tests passing,
  including 4 new oracle tests at `dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp`.
  See `docs/yield-curves/construction.md` and `docs/methodology/aad.md`. Non-breaking (additive
  public surface; existing single-arg callers exercise the AAD path by default on eligible specs).

<!-- Add new qualifying changes below as dated sections, e.g. -->
<!-- ## 2026-06 -->
<!-- - `curve`: Added log-linear interpolation to the interpolation module (non-breaking). -->
