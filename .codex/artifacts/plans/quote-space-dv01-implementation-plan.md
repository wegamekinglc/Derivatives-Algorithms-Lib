# Calibration-Aware Quote-Space DV01 — Implementation Plan

> Status: approved plan; implementation has not started. This plan implements
> the frozen [quote-space DV01 design](../designs/quote-space-dv01-design.md). Each work
> item is an independent PR and rollback boundary. Later stages must not reopen
> the API-design and critic closure recorded in `DAL-171`.

## Delivery Rules

- Production implementation is out of scope for the design PR that introduces
  these documents.
- Every implementation stage is test-driven: add a focused failing contract,
  verify RED where a new public surface is missing, implement the minimum stage,
  then verify GREEN.
- Every stage branches from the latest merged predecessor, is independently
  reviewable, and has one GitHub issue and one corresponding Multica task.
- Multica tasks remain `backlog` until the parent workflow explicitly promotes
  their stage. Creating this plan does not start implementation.
- The source-of-truth success domains, fingerprints, tokens, units, failure
  atomicity, and numerical thresholds live in the design and this plan. An
  implementation issue may narrow its file scope but may not reinterpret the
  contract.
- New public behavior requires public C++, Python, and Excel coverage before the
  final closeout stage is accepted.
- Every code-changing stage follows the normal implementer → tester → reviewer
  → documentation-decision workflow. The assignee in the work-breakdown table
  is the suggested primary implementer, not permission to skip those gates.

## Dependency Graph

```text
QR-1 provenance foundation
  └─ QR-2 single-curve quote risk
       └─ QR-3 joint/staged XCCY domains
            └─ QR-4 public C++ + Python bindings
                 └─ QR-5 Excel bindings
                      └─ QR-6 acceptance/performance/docs
```

QR-5 starts only after QR-4 is merged because `dal-excel` consumes the public
facade and must not depend directly on `dal-cpp`. QR-6 starts only after both
QR-4 and QR-5 are merged.

## Work Breakdown

| Work item | Stage | Scope                                                            | Depends on | Suggested assignee |
|-----------|-------|------------------------------------------------------------------|------------|--------------------|
| QR-1      | 1     | Provenance DTOs, supported factories, fingerprints, validation   | none       | `dal-implementer`  |
| QR-2      | 2     | Single-curve aggregate engine and quote buckets                  | QR-1       | `dal-implementer`  |
| QR-3      | 3     | Joint-XCCY and staged-XCCY-basis domains, atomicity              | QR-2       | `dal-implementer`  |
| QR-4      | 4     | Public C++ and Python bindings/contracts                         | QR-3       | `dal-implementer`  |
| QR-5      | 5     | Excel handles, spills, generated registrations, portable tests   | QR-4       | `dal-implementer`  |
| QR-6      | 6     | Full numerical oracle, performance gate, docs/CHANGELOG closeout | QR-4, QR-5 | `dal-implementer`  |

## QR-1 — Provenance Foundation

### Scope

- Add immutable provenance/config/block/range/axis/state types in the curve-risk
  layer.
- Add the three explicit factory declarations:
  `BuildSingleCurveQuoteRiskProvenance`,
  `BuildJointXccyQuoteRiskProvenance`, and
  `BuildStagedXccyBasisQuoteRiskProvenance`.
- Freeze the single-curve core factory parameter as
  `const CurveCalibrationResult_&`. `dal-cpp` must not include or depend on the
  public-only `CalibrationResult_`.
- Implement shared validation for:
  - exact solve mode;
  - effective-inverse request and availability;
  - finite inverse/scaling/tolerance;
  - complete, non-overlapping, partitioning parameter/residual ranges;
  - non-empty calibration id;
  - complete, unique parameter-block bindings;
  - supported calibration kind.
- Implement JCS canonical records plus SHA-256 for:
  - `dal.quote-risk-axis/1+jcs+sha256`;
  - `dal.quote-risk-state/1+jcs+sha256`.
- Fingerprint normalized spec/options/quotes, solved free parameters and base DAG,
  market bindings, XCCY state, valuation/fixings, inverse/scaling/tolerance, and
  ranges.
- Detect base cycles and reject non-finite fingerprint inputs.
- Return read-only unavailable provenance for well-formed unsupported runtime
  state; throw for malformed or ambiguous input.

This stage does not add `AggregateRatePortfolioQuoteRisk` behavior or bindings.

### Primary files

- `dal-cpp/dal/curve/` — new quote-risk header/source and focused internals;
- existing calibration result/spec headers for read-only consumption only;
- `dal-cpp/tests/curve/` — provenance/fingerprint tests;
- `CMakeLists.txt` files needed to register the new compilation units/tests.

### Focused tests

- available single/joint/staged-basis shapes under ANALYTIC and BUMPED modes;
- exact/non-exact and requested/not-requested inverse availability;
- missing/empty inverse and malformed pairing;
- empty calibration id;
- duplicate binding, missing binding, overlapping/gapped ranges;
- identical axis across changed curve value, base curve, as-of, fixing, inverse,
  or tolerance: axis fingerprint unchanged where appropriate, state fingerprint
  changed;
- deterministic fingerprints across repeated construction and supported
  platforms;
- non-finite state and cyclic base DAG fail before hashing.
- a compile-time signature check accepts `CurveCalibrationResult_` at the core
  factory and proves the core header is self-contained without `dal-public`.

### Gate

- Focused provenance tests pass across all AAD backends where calibration
  fixtures are backend-dependent.
- No aggregate or binding surface exists yet.
- Existing calibration and node-risk tests remain unchanged and green.

### Rollback

Reverting QR-1 removes only unused provenance types/factories and their tests.
Existing calibration and risk behavior is unchanged.

## QR-2 — Single-Curve End-to-End Quote Risk

### Scope

- Add `RateQuoteRiskBucket_`, result/meta/provenance-failure shapes, and
  `AggregateRatePortfolioQuoteRisk`.
- Implement aggregate-call uniqueness validation for `calibrationId`.
- Recompute and compare bound component state fingerprints before any AAD sweep.
- Reuse the existing market-aware dependency plan and node-risk sweeper.
- Implement `(trade, provenance)` processing for a single parameter block:
  - structural zero when the trade does not depend on the block;
  - one node gradient when it does;
  - isolated `QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE` on node-risk failure.
- Aggregate by actual PV currency and apply
  `g^T * effectiveInverse / tolerance`.
- Emit deterministic buckets with unit-bearing `dPvDDecimalQuote` and `dv01`.
- Preserve `UnconvertedByActualPvCcy` and existing PV/meta semantics.

### Focused tests

- all supported curve parameterizations with 5-, 10-, and 16-quote single-curve
  fixtures;
- ANALYTIC and BUMPED effective inverses;
- duplicate display names with distinct ordinals;
- empty trades, empty provenances, zero PV, negative PV, and offsetting trades;
- duplicate calibration ids;
- stale provenance returns one mismatch row before node-sweep instrumentation
  advances;
- structural-zero non-dependency does not call the component sweep;
- one trade failure does not affect siblings;
- `dv01 == dPvDDecimalQuote * 1e-4` within the frozen machine-precision identity
  bound;
- deterministic bucket order and no cross-currency netting.

### Gate

- Single-curve numerical oracle passes the normative P0-4 contract below.
- No joint/staged-XCCY success is claimed.
- Existing node-risk batch numerical identity and failure-priority tests remain
  green.

### Rollback

Reverting QR-2 removes the aggregate entry point and quote-bucket types while
leaving the inert QR-1 provenance foundation intact.

## QR-3 — Joint-XCCY and Staged-Basis Domains

### Scope

- Implement full joint-XCCY provenance using all domestic, foreign, and basis
  parameter/residual ranges as one indivisible vector.
- Implement staged-XCCY basis-only provenance. Domestic/foreign curves remain
  state dependencies and do not emit quote buckets.
- Enforce the published parameter/residual range partitions and binding map.
- Generalize `(trade, provenance)` processing:
  - structural zeros for unconsumed blocks;
  - node gradients for every actually consumed block;
  - concatenation only after all consumed blocks succeed;
  - discard all sibling gradients on the first failure;
  - emit exactly one `QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE` row.
- Keep ordinary staged multi-curve and generic joint domains unavailable. Do
  not build partial inverses or invert a forward Jacobian.

### Focused tests

- joint-XCCY domestic/foreign/basis ranges cover the full axes exactly once;
- staged-XCCY emits basis quote buckets only;
- domestic/foreign state changes invalidate staged-basis provenance;
- each one-block failure after successful siblings produces no partial bucket;
- XCCY multi-currency results remain separated by actual PV currency;
- unused joint blocks are structural zero without a sweep;
- layered bases, FX spot, collateral, valuation time, and fixings participate in
  state mismatch detection;
- ordinary staged multi-curve and generic joint requests return their frozen
  unavailable results.

### Gate

- Joint-XCCY and staged-basis numerical oracles pass the P0-4 contract for 5-,
  10-, and 16-quote fixtures with ANALYTIC and BUMPED inverses.
- A fault-injection/instrumentation test proves sibling gradients are discarded.
- Existing XCCY calibration, pricing, fixing, and node-risk tests stay green.

### Rollback

Reverting QR-3 returns the available success domain to single curve. QR-1/QR-2
types and single-curve behavior remain intact.

## QR-4 — Public C++ and Python Surfaces

### Scope

- Add source-additive public facade passthroughs for all three provenance
  factories and aggregate quote risk.
- Add the single-curve public overload that accepts
  `const CalibrationResult_&` and adapts it into the QR-1 core validation path;
  do not duplicate provenance or fingerprint semantics in `dal-public`.
- Bind provenance configs, read-only provenance/result/meta/bucket types, enums,
  tokens, fingerprints, and units in Python.
- Make Python constructors/factories/aggregate arguments keyword-only.
- Release the GIL once around the complete native operation; perform no Python
  callback while released and marshal only after reacquiring it.
- Do not register builders for excluded staged/generic domains.
- Add user-facing Python examples for single curve and joint XCCY only after the
  executable contract is green.

### Focused tests

- public C++ headers compile and link all new entry points;
- the core overload accepts `CurveCalibrationResult_`, the public overload
  accepts `CalibrationResult_`, and both produce identical provenance for the
  same calibration result;
- Python signature snapshots pin names/order/keyword-only shape;
- provenance and results are read-only;
- C++ and Python produce identical fingerprints, ordering, tokens, availability,
  quote sensitivities, and DV01 values;
- a native barrier/heartbeat test proves the aggregate call releases the GIL;
- unsupported builders are absent, not silently emulated;
- Python object ownership survives deletion/GC of intermediate calibration
  results and markets where the provenance retains required immutable state.

### Gate

- `dal_public_tests` and focused Python tests pass on supported platforms.
- The generated/public surface does not change existing Python names or behavior.

### Rollback

Reverting QR-4 removes only public/Python entry points; the core feature remains
available to direct `DAL::cpp` consumers.

## QR-5 — Excel Surface

QR-5 is stage 5 and branches from the merged QR-4 result. Its public-facade
dependency is mandatory; it does not call `dal-cpp` directly or recreate the
QR-4 facade.

### Scope

- Add immutable repository-handle constructors for the three supported
  provenance kinds.
- Add the aggregate worksheet over trade, market, and provenance handles.
- Emit the fixed long-form spill:

  ```text
  calibration, axis_fingerprint, quote_key, quote_name, block, currency,
  quote_sensitivity, dv01, availability, reason
  ```

- Preserve deterministic row order and one row per bucket/failure record.
- Map valid excluded domains to the frozen unavailable tokens.
- Regenerate and commit Machinist output.
- Extend portable Excel tests so row shapes and economic values are testable on
  Linux; Windows retains XLL compile/registration ownership.

### Focused tests

- available single, joint-XCCY, and staged-basis handles;
- excluded staged/generic handles return stable availability;
- stale provenance, incomplete trade/provenance, empty results, duplicate ids;
- exact column count/order and cell types;
- quote sensitivity/DV01 ties to core results;
- mixed currencies stay in separate rows;
- registration, generated-source drift, and long identifier/help strings.

### Gate

- portable Excel contract tests pass locally;
- Windows Excel build, link, and registration checks pass in CI;
- `dal_check_generated` is clean.

### Rollback

Reverting QR-5 removes Excel handles, worksheets, generated registrations, and
portable tests. Core/public/Python behavior remains.

## QR-6 — Acceptance, Performance, and Documentation Closeout

QR-6 is stage 6 and starts only after both QR-4 and QR-5 are merged.

### Scope

- Consolidate the full cross-domain numerical oracle and preserve per-bucket
  evidence.
- Add single-curve, joint-XCCY, and staged-basis quote-risk benchmark cases to
  `rate_risk_perf` and the repository regression gate.
- Verify no production quote bump/recalibration occurs inside the aggregate
  path.
- Update `docs/public-api.md`, the yield-curve Jacobian methodology, Python and
  Excel user docs, `docs/README.md`, and `CHANGELOG.md` from “planned” to
  current supported behavior.
- Add runnable C++/Python/Excel examples that use the supported factories and
  explain unconverted currency policy and excluded domains.
- Run the full local validation matrix appropriate to the final head; CI owns
  the complete compiler × AAD-backend and Windows XLL matrices.

### Gate

- Every acceptance item below passes with no skipped bucket or percentile-based
  substitution.
- Performance cases are gated and within the recorded baseline.
- Docs and examples match the exact shipped API and token set.
- No implementation stage changes a frozen threshold or success domain.

### Rollback

Reverting QR-6 removes only final benchmarks, examples, and closeout docs where
possible. If an acceptance failure reveals a production defect, fix the owning
earlier stage instead of weakening QR-6 gates.

## Normative Numerical Acceptance Contract

All comparisons use `<=`; implementation stages may not choose new thresholds.

### Oracle, axis width, and scale

For each supported domain and each
`(provenance, actualPvCurrency, quote bucket i)`:

- `h = 1e-6` decimal quote and `b = 1e-4` (one basis point).
- Bump the original quote, fully recalibrate the frozen supported domain, and
  fully reprice the same eligible trade set. A forward Jacobian/map is not an
  oracle.
- Oracle non-convergence, a failed existing fit gate, non-finite output, or loss
  of an originally eligible trade is a test failure, not a skip.

Define:

```text
D_api = dPvDDecimalQuote[i]
D_fd  = (PV(q_i + h) - PV(q_i - h)) / (2h)
V_api = dv01[i]
V_fd  = (PV(q_i + b) - PV(q_i - b)) / 2
```

`N` is the quote/residual-axis width transformed by that provenance:

- single curve: effective-inverse column count;
- joint XCCY: total joint residual-range width;
- staged XCCY basis: basis residual-range width.

Trade count, parameter count, and display-name-deduplicated count are not `N`.

The per-bucket price scale is:

$$
P_i = \max\left(
1\text{ currency unit},
\sum_t |PV_t(q_i)|,
\sum_t |PV_t(q_i+h)|,
\sum_t |PV_t(q_i-h)|,
\sum_t |PV_t(q_i+b)|,
\sum_t |PV_t(q_i-b)|
\right).
$$

Every sum uses the same eligible trade set. Gross absolute PV is required;
netting cannot shrink the scale.

Define:

```text
e_D = |D_api - D_fd|     s_D = max(|D_api|, |D_fd|)
e_V = |V_api - V_fd|     s_V = max(|V_api|, |V_fd|)
```

If `s_D` or `s_V` is zero, its relative error is zero only when the matching
absolute error is zero; otherwise it is positive infinity.

### Frozen thresholds

The test suite must include actual `N = 5`, `N = 10`, and `N = 16` fixtures.
Any `N > 16` continues to use the `16+` thresholds and does not auto-relax.

| Ladder | Width          | Master worst baseline `B` | Derivative absolute | Derivative relative | DV01 absolute | DV01 relative |
|--------|----------------|---------------------------|---------------------|---------------------|---------------|---------------|
| 5      | `N <= 5`       | `7e-7`                    | `5e-6 * P_i`        | `5e-6`              | `5e-10 * P_i` | `5e-6`        |
| 10     | `6 <= N <= 10` | `1.8e-5`                  | `1e-4 * P_i`        | `1e-4`              | `1e-8 * P_i`  | `1e-4`        |
| 16+    | `N > 10`       | `1.3e-4`                  | `1e-3 * P_i`        | `1e-3`              | `1e-7 * P_i`  | `1e-3`        |

Thresholds are derived by:

```text
T = Round125Up(5 * B)
```

`Round125Up(x)` selects the least member of `{1, 2, 5} * 10^k` that is not
less than `x`. Therefore:

- `3.5e-6 -> 5e-6`;
- `9e-5 -> 1e-4`;
- `6.5e-4 -> 1e-3`.

DV01 absolute tolerance is exactly derivative absolute tolerance multiplied by
`b = 1e-4`; there is no second convention.

### Per-bucket pass/fail

Every bucket must satisfy all four conditions:

1. all API, oracle, and PV values are finite;
2. unit identity:

   ```text
   |V_api - b * D_api|
       <= 64 * epsilon * max(P_i * b, |V_api|, b * |D_api|)
   ```

   where `epsilon = std::numeric_limits<double>::epsilon()`;
3. derivative comparison: derivative absolute **or** relative threshold passes;
4. DV01 comparison: DV01 absolute **or** relative threshold passes.

The absolute/relative **OR** only handles a near-zero scale for the same
quantity. Derivative and DV01 checks are combined with **AND**.

The overall suite passes only when every quote bucket, currency, supported
domain, ANALYTIC/BUMPED inverse, and actual 5/10/16 ladder passes. Percentiles,
averages, worst-row-only summaries, and offsetting failures are forbidden. Save
for every bucket:

```text
N, P_i, API value, oracle value, absolute error, relative error, threshold
```

### Baseline evidence and threshold changes

The frozen baseline is commit
`7d8a0ffa9157f93aa5b2fa88f27da868981971e7`. The method
`D = g^T E / tolerance` and the 5/10/16 nonlinear recalibration observations
`7e-7`, `1.8e-5`, and `1.3e-4` are recorded in
`docs/methodology/yield_curve_jacobian.md`. Existing independent evidence is:

- `InverseJacobianRiskTest.*`: 2/2 passing at closure;
- the 10-quote `yield_curve_jacobian` example: all self-checks passing;
- observed 10-quote worst re-solve error `5.3e-7` and maximum AAD-vs-FD
  difference `9.2e-11`; the threshold deliberately retains the more conservative
  methodology baseline.

Thresholds never self-learn at runtime or during an implementation PR. A new
baseline review is required if any of these change:

- solver residual scaling or tolerance;
- curve parameterization/interpolation;
- supported success domains;
- oracle bump;
- axis-width bands;
- required fixtures add `N > 16`;
- any valid observation exceeds the current `T / 5`.

Changing a threshold requires preserved commit/fixture/axis/platform/compiler/
AAD-backend evidence plus every per-bucket raw row. With an approved new worst
baseline `B_new`, the only update rule is:

```text
Round125Up(5 * max(B_old, B_new))
```

The change requires a new API-note/design revision and critic approval. If a
result exceeds the current threshold before that review, it fails.

## Cross-Cutting Functional Acceptance

- All three supported domains succeed with ANALYTIC and BUMPED effective
  inverses.
- Ordinary staged multi-curve and generic joint requests are stably unavailable
  and emit no bucket.
- Axis and state fingerprints are deterministic and detect every frozen stale
  state dimension.
- Duplicate/empty calibration ids, malformed ranges, duplicate bindings,
  non-finite state, and cyclic bases fail as specified.
- `(trade, provenance)` is atomic: a consumed-block failure emits one meta row
  and no partial gradient/bucket.
- Structural-zero blocks are the exact published width and run no sweep.
- Actual PV currencies never net or convert; policy remains
  `UnconvertedByActualPvCcy`.
- Empty inputs, zero/negative PV, duplicate display names, layered bases, joint
  ranges, valuation/fixing changes, and partial trade failure are covered.
- C++/Python/Excel expose identical units, axis identity, bucket ordering,
  tokens, availability, and economic values.

## Verification Commands

Use the actual build directory selected by the implementing task. The expected
focused and repository checks are:

```bash
./build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='*QuoteRisk*:*InverseJacobianRisk*'
./build/Release-linux/dal-public/dal_public_tests
python3 -m pytest dal-python/tests -q -k 'quote_risk or curve_pricing'
./build/Release-linux/dal-excel/dal_excel_portable_tests
cmake --build build/Release-linux --target dal_check_generated
python3 .github/scripts/check_docs.py
./build/Release-linux/dal-cpp/benchmarks/rate_risk_perf/rate_risk_perf
ctest --test-dir build/Release-linux --output-on-failure
git diff --check
```

The final implementation head must also pass the repository's PR-tier full
compiler × AAD-backend matrix, Windows Python/Excel build and registration,
coverage, warning-clean, sanitizers, and benchmark regression gates.

## Final Completion Criteria

The feature is complete only after QR-1 through QR-6 are merged in dependency
order, every normative numerical row passes, all bindings are aligned, the
performance gate is active, current-state docs replace planned wording, and the
final independent review has no unresolved finding. Merge of this design-only
PR does not satisfy any implementation item.
