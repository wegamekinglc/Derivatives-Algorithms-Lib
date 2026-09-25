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

## 2026-09-25

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
  output. See the [Excel FIX guide](docs/excel-script-settings.md).

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
  [prepared pricing contract](docs/methodology/rate_node_risk.md#repeated-pricing-with-prepared-trades).

## 2026-09-11

- **Joint quote-risk base graphs and eligibility** — joint risk follows actual
  consumed base paths, including unregistered XCCY forecast curves, with
  historical native coordinates preserved. Necessary nodes must be exact
  builtin curve types. This intentionally makes opaque unit-discount leaves
  and builtin subclasses ineligible even when their previous numerical results
  were correct; standalone node risk and v1 quote-risk semantics are unchanged.
  See the [graph contract](docs/methodology/generic_joint_quote_risk.md#aggregation-and-failures).

## 2026-09-08

- **Generic joint quote-space DV01** — exact same-currency joint calibration can
  retain a full coupled effective inverse and expose immutable v2 quote-risk
  provenance through C++, Python, and Excel. Explicit inverse requests define
  a fixed initial-Jacobian subspace for underdetermined systems; default solves
  and existing v1 domains retain their behavior. See the
  [mapping and units contract](docs/methodology/generic_joint_quote_risk.md).

## Existing methodology and capabilities

These are documented today and represent the current documented surface; they are listed
here as the baseline rather than dated releases:

- **Automatic Adjoint Differentiation (AAD)** — reverse-mode AD for risk sensitivities, with
  Adept/XAD/CoDiPack backends. See `docs/methodology/aad.md`.
- **Yield Curve Construction** — discount-factor / forward-rate parameterised curves
  calibrated to market instruments. See `docs/methodology/yield_curve.md`.
- **Underdetermined Search** — constrained least-change solver for over-parameterised
  nonlinear calibration. See `docs/methodology/underdetermined_search.md`.
- **Cross-Currency Pricing and Calibration** — fixed, resettable, and mark-to-market
  swap pricing with immutable timestamped rate/FX fixing snapshots, staged basis
  fitting, simultaneous domestic/foreign/basis calibration, and named joint
  parameter/residual ranges. See `docs/methodology/xccy_calibration.md`.
- **Interpolation** — linear, log-linear, cubic-spline, and mixed 1D interpolators plus
  bilinear 2D interpolation. See `docs/methodology/interpolation.md`.
- **Log-Discount Curve** — node log-discount-factor parameterisation with `LogDfScheme_`
  interpolation schemes and scalar-generic passive/AAD evaluation. See
  `docs/methodology/log_discount_curve.md`.
- **Yield-Curve Jacobian and Inverse-Jacobian Risk** — AAD forward Jacobians for every
  implemented curve representation subject to the normal eligibility gates, plus the
  inverse-Jacobian IR-risk transform and its `effJacobianInverse_` unit convention. See
  `docs/methodology/yield_curve_jacobian.md`.
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
  `docs/methodology/yield_curve_jacobian.md`.

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
  `docs/methodology/xccy_calibration.md`,
  `docs/methodology/yield_curve_jacobian.md`, and `docs/public-api.md`.

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
  and builder remain compatible. See `docs/methodology/xccy_calibration.md`,
  `docs/methodology/yield_curve_jacobian.md`, and `docs/public-api.md`.

- `curve`: Added persistent continuously compounded `ZERO_RATE` curves. Future-node
  rates map to `logDF = -z * YearFrac(anchor,node)` and reuse all shared log-DF
  interpolation/extrapolation schemes; the anchor has no free zero-rate parameter.
  Single, staged, and joint calibration support ZERO_RATE with passive or active base
  layering and AAD analytical Jacobians in future-node zero-rate order. The additive
  `DiscountZeroRate_v1` archive preserves representation and bump coordinates, and direct
  factories are available in core C++, public C++, Python (`DiscountZeroRate_New`), and
  Excel (`DISCOUNTZERORATE.NEW`). See `docs/methodology/yield_curve.md`,
  `docs/methodology/yield_curve_jacobian.md`, and `docs/public-api.md`.

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
  See `docs/methodology/interpolation.md`, `docs/methodology/log_discount_curve.md`, and
  `docs/methodology/yield_curve_jacobian.md`.

- `numerics`: Corrected three output-affecting quantitative contracts: rate-aware
  Dupire now prices a discounted spot call and includes the strike in
  $(r-q)K C_K$; the exact underdetermined solver uses the quadratic model's
  $k=(c-b)/(a-2b+c)$ backtrack fraction; and Bachelier pricing/implied volatility
  now supports all real forward/strike pairs with a finite, nonnegative
  price-unit bracket and translation-invariant tolerances.
  See `docs/methodology/dupire.md`, `docs/methodology/underdetermined_search.md`,
  and `docs/methodology/black_scholes.md`.
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
  `docs/methodology/yield_curve.md` and `docs/methodology/yield_curve_jacobian.md`.

- `pde`: Implemented the `Rollback_`-based PDE framework: coefficient factories and callable
  adapters, endpoint-exact concentrating coordinate maps, grid materialization, node-location
  derivative operators, and `ThetaScheme_` with explicit `Prepare`/decomposition reuse. The old
  mesher/`FD1D_` stack was removed, and `european_fd` plus `pde_perf` now use the new framework.
  See `docs/methodology/pde.md`. Breaking for direct `dal-cpp` PDE internals only; no
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
  normal draws retain their precise default. See `docs/methodology/random.md`.

## 2026-06

- `curve`: Added a yield-curve Jacobian example demonstrating AAD-vs-bump agreement and the
  inverse-Jacobian IR-risk transform; documented the corrected units of
  `CurveCalibrationDiagnostics_::effJacobianInverse_` as
  `d(params)·tolerance_ / d(decimal-rate perturbation)` (the underdetermined solver scales
  residuals by `1/tolerance_` before forming the pseudoinverse, so consumers must divide by
  `tolerance_` when transforming a sensitivity vector: `r = gᵀ · effJacobianInverse_ / tolerance_`).
  See `docs/methodology/yield_curve_jacobian.md` and the example at
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
  See `docs/methodology/yield_curve.md` and `docs/methodology/aad.md`. Non-breaking (additive
  public surface; existing single-arg callers exercise the AAD path by default on eligible specs).

<!-- Add new qualifying changes below as dated sections, e.g. -->
<!-- ## 2026-06 -->
<!-- - `curve`: Added log-linear interpolation to the interpolation module (non-breaking). -->
