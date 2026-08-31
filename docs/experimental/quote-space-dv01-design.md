# Calibration-Aware Quote-Space DV01 — Design

> Status: approved design; implementation has not started. This document extracts
> the P0 contract approved in Multica issue `DAL-171` after API-design and critic
> closure. The companion
> [implementation plan](quote-space-dv01-implementation-plan.md) defines staged,
> independently reviewable delivery. The code baseline used to close the design is
> `7d8a0ffa9157f93aa5b2fa88f27da868981971e7`.

## Source and Problem Statement

DAL already has both mathematical halves of calibration-aware quote risk:

- `AggregateRatePortfolioNodeRisk` produces portfolio PV gradients with respect
  to native curve parameters, grouped without FX conversion by actual PV
  currency.
- exact curve calibration can retain the solver-scaled effective inverse
  Jacobian that maps calibration-quote perturbations to curve-parameter
  perturbations.

The supported public API does not join those halves. The only current example
of the transform is the local `TransformToQuoteRisk` helper in
`dal-cpp/examples/yield_curve_jacobian/yield_curve_jacobian.cpp`. A consumer
must currently align parameter and residual axes itself and remember the
solver-tolerance correction:

$$
q = g^{\mathsf T} E / \tau,
\qquad
\mathrm{DV01}=10^{-4}q,
$$

where $g$ is a PV gradient over free curve parameters, $E$ is the retained
effective inverse, and $\tau$ is the calibration tolerance. Omitting the
division by $\tau$ gives a finite, plausible-looking but dimensionally wrong
answer. Guessing axis correspondence is similarly dangerous, especially for
layered and joint cross-currency calibrations.

This feature makes provenance, axis identity, state binding, failure isolation,
units, and the transform itself one supported contract across C++, Python, and
Excel.

## Goals

1. Expose quote-space portfolio sensitivity and DV01 for three explicitly
   supported exact-calibration domains:
   - single-curve calibration;
   - full joint-XCCY calibration;
   - staged-XCCY basis calibration, for the basis block only.
2. Make stale or mismatched calibration provenance detectable before any AAD
   sweep or matrix transform.
3. Preserve the existing `UnconvertedByActualPvCcy` aggregation policy and
   never combine PV gradients from different currencies.
4. Isolate a valid trade-level failure to one `(trade, provenance)` pair while
   allowing other trades, currencies, and provenances to continue.
5. Carry one source-additive contract through DAL core/public C++, Python, and
   Excel without changing existing pricing, node-risk, token, or ordering
   behavior.
6. Gate the economic result against independent full recalibration and
   repricing, with numerical thresholds frozen before implementation.

## Non-Goals

- Ordinary staged multi-curve quote risk and generic joint-multi-curve quote
  risk are not supported in v1.
- Approximate-fit calibration is not supported in v1.
- The feature does not reconstruct an inverse from a forward Jacobian and does
  not use a forward Jacobian as an availability signal.
- There is no reporting-currency conversion, FX delta, volatility risk,
  nonlinear scenario/P&L explain, or shock orchestration.
- Existing `RatePortfolioNodeRisk_`, curve handles, and calibration result
  types do not acquire implicit mutable provenance.
- This design does not add a serialization schema; that remains a separate
  feature.
- Parallel quote-risk aggregation is outside v1. Existing thread-local AAD tape
  ownership remains unchanged.

## Compatibility Contract

All new types and entry points are source-additive. Existing behavior remains
unchanged:

- the seven-family `RateInstrumentType_` registry;
- `PriceRateTrade(s)`, `RateTradeNodeSensitivities`,
  `RateTradeNodeSensitivitiesBatch`, and `AggregateRatePortfolioNodeRisk`;
- node-risk failure tokens and their priority;
- trade-major and component-key ordering;
- the `UnconvertedByActualPvCcy` policy;
- Python private archive/Bag helpers;
- all existing calibration options and result fields.

No public caller may construct an available provenance with an empty inverse or
otherwise bypass a supported factory.

## Supported Domains and Availability

### Explicit factories

API names deliberately encode the supported domains:

```cpp
struct RateQuoteRiskProvenanceConfig_ {
    String_ calibrationId_;
    std::map<String_, String_> componentKeyByParameterBlock_;
};

RateQuoteRiskProvenance_ BuildSingleCurveQuoteRiskProvenance(
    const CurveCalibrationSpec_& spec,
    const CalibrationResult_& result,
    const CurveCalibrationOptions_& options,
    const RatePricingMarket_& boundMarket,
    const RateQuoteRiskProvenanceConfig_& config);

RateQuoteRiskProvenance_ BuildJointXccyQuoteRiskProvenance(
    const JointXccyCalibrationSpec_& spec,
    const JointXccyCalibrationResult_& result,
    const JointXccyCalibrationOptions_& options,
    const RatePricingMarket_& boundMarket,
    const RateQuoteRiskProvenanceConfig_& config);

RateQuoteRiskProvenance_ BuildStagedXccyBasisQuoteRiskProvenance(
    const CrossCurrencyCalibrationSpec_& spec,
    const CrossCurrencyCalibrationResult_& result,
    const CrossCurrencyCalibrationOptions_& options,
    const RatePricingMarket_& boundMarket,
    const RateQuoteRiskProvenanceConfig_& config);
```

Every available provenance requires:

- `solveMode == EXACT`;
- `computeEffJacobianInverse == true`;
- an available, finite effective inverse with the published shape;
- a non-empty, aggregate-call-unique `calibrationId`;
- a complete parameter-block-to-market-component binding;
- structurally consistent spec, result, options, ranges, and bound market.

`jacobianMode == ANALYTIC` and `jacobianMode == BUMPED` are both supported. The
effective inverse is the contract; the forward Jacobian is irrelevant to
availability.

The domains are:

- **Single curve:** one complete parameter block and every quote row in that
  solve.
- **Joint XCCY:** domestic, foreign, and basis parameter/residual ranges form
  one indivisible provenance.
- **Staged XCCY basis:** only the basis parameter block and basis quote rows are
  exposed. Domestic and foreign curves remain fingerprinted state dependencies
  and do not produce quote buckets.

Ordinary `MultiCurveCalibrationResult_` and generic
`JointMultiCurveCalibrationResult_` have no C++ provenance factory and no
Python builder. Excel/dynamic dispatch reports a stable unavailable state
instead of constructing a partial inverse.

### Availability and structural failures

Supported factories return a read-only unavailable provenance for a well-formed
but unsupported runtime state:

- `QUOTE_RISK_INVERSE_NOT_REQUESTED`;
- `QUOTE_RISK_NOT_AVAILABLE_FOR_SOLVE_MODE`;
- `QUOTE_RISK_EFFECTIVE_INVERSE_UNAVAILABLE`;
- `QUOTE_RISK_NOT_AVAILABLE_FOR_STAGED_CHAIN_RULE` on the Excel/dynamic staged
  exclusion path.

Malformed input remains exceptional:

- `QUOTE_RISK_CALIBRATION_ID_EMPTY` for an empty id;
- `QUOTE_RISK_DUPLICATE_CALIBRATION_ID` when an aggregate call repeats an id;
- invalid ranges, duplicate bindings, non-partitioning axes, mismatched
  spec/result/options, non-finite state, and cyclic base graphs.

The distinction is intentional: a valid but unavailable calibration is reportable
data; corrupt or ambiguous provenance is a programming/configuration error.

## Provenance Identity

Axis compatibility and market-state freshness are separate identities. Both use
lowercase `sha256:<64-hex>` values. Their canonical records are encoded with RFC
8785 JSON Canonicalization Scheme (JCS) before SHA-256. Fingerprinting rejects
non-finite numbers.

### Axis fingerprint

`axisFingerprintScheme = dal.quote-risk-axis/1+jcs+sha256` covers:

- calibration kind;
- every parameter-block key, offset, size, and parameter coordinate;
- every residual block;
- quote ordinal, display name, and unit.

A stable quote-bucket key is:

```text
{calibrationId, axisFingerprint, residualBlock, ordinal}
```

It is stable only within one immutable axis version. Adding, removing, or
reordering a quote changes the fingerprint even if display names are reused.

### Calibration-state fingerprint

`calibrationStateFingerprintScheme = dal.quote-risk-state/1+jcs+sha256` stores
one global and one per-bound-component fingerprint. The canonical record covers:

- normalized calibration spec and solver/options;
- instrument quotes and order;
- solved curve definition and free parameters;
- the complete recursive base-curve dependency DAG;
- component-key-to-curve binding;
- XCCY domestic/foreign blocks, basis curve, FX spot, and collateral state;
- valuation time and the full immutable fixing snapshot;
- effective inverse, scaling, tolerance, and published ranges.

References are sorted by stable ID after cycle validation. Equal axes do not
imply equal state: a changed curve value, base curve, as-of, fixing, inverse, or
tolerance must change the state fingerprint.

At aggregate entry, DAL recomputes every bound component fingerprint before any
sweep. A mismatch produces one `QUOTE_RISK_CALIBRATION_STATE_MISMATCH` row with
the offending component and expected/actual fingerprints. That provenance emits
no bucket; other provenances continue.

## Aggregate API and Output

The v1 entry point has exactly three required arguments:

```cpp
RatePortfolioQuoteRisk_ AggregateRatePortfolioQuoteRisk(
    const Vector_<RateTradeDefinition_>& trades,
    const RatePricingMarket_& market,
    const Vector_<RateQuoteRiskProvenance_>& provenances);
```

The result retains:

- `policy = UnconvertedByActualPvCcy`;
- PV totals grouped by actual PV currency;
- deterministic quote buckets;
- per-trade metadata;
- provenance-level availability/failure rows.

Each `RateQuoteRiskBucket_` contains:

- calibration id;
- axis fingerprint;
- quote key and display name;
- residual block and quote ordinal;
- actual PV currency;
- `dPvDDecimalQuote` in currency amount per decimal quote;
- `dv01` in currency amount per one basis point.

The fixed unit identity is:

```text
dv01 = dPvDDecimalQuote * 1e-4
```

No field is named simply `sensitivity` or `risk` without a unit-bearing contract.

## Trade/Provenance Atomicity

The failure-isolation unit is `(trade, provenance)`, not `(trade, component)`
and not the entire portfolio.

For each trade:

1. Build the market-aware cashflow plan and determine the curve blocks actually
   consumed by that trade.
2. For a provenance parameter block the trade does not consume, append a
   structural zero vector of the published block width without running a node
   sweep.
3. Run node-risk sweeps for every actually consumed block.
4. Only after every consumed block succeeds, concatenate the gradients in the
   provenance's published range order.
5. If any consumed block fails, discard every sibling gradient already computed
   for that `(trade, provenance)` pair. Emit one
   `QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE` meta row containing calibration id,
   instrument id, first failing component, and the original node-risk token.
6. Other trades, currencies, and provenances continue.

Complete gradients are accumulated by `(provenance, actualPvCurrency)` and then
transformed as `g^T * effectiveInverse / tolerance`. This prevents partial joint
vectors and cross-currency netting. Empty trades return a successful empty
bucket/meta/PV result.

## Binding Contracts

### Public C++

The three provenance factories and aggregate entry point are exposed through
the public facade as direct source-additive passthroughs. Provenance and result
objects are immutable after construction.

### Python

- Factory and aggregate inputs are keyword-only.
- Provenance, fingerprints, ranges, buckets, and result fields are read-only.
- The complete native operation releases the GIL once; marshalling happens
  after reacquiring it.
- Python does not expose unsupported staged/generic builders.
- Python-visible availability values, tokens, ordering, units, and fingerprints
  match C++ exactly.

### Excel

Provenance is an immutable repository handle. The aggregate worksheet accepts
trade, market, and provenance handles. Its long-form spill columns are fixed:

```text
calibration, axis_fingerprint, quote_key, quote_name, block, currency,
quote_sensitivity, dv01, availability, reason
```

Dynamic unsupported-domain requests return the stable unavailable token rather
than an empty successful matrix. Machinist-generated registration remains the
source of truth.

## AAD, State, and Concurrency

- Node gradients are produced by the existing rate-risk sweep and its
  `TapeGuard_` discipline. The residual/calibration tape is never reused to
  produce portfolio PV gradients.
- The quote transform itself is passive matrix arithmetic over already
  harvested gradients and the retained effective inverse.
- v1 remains serial on one thread-local tape. It does not dispatch sweeps onto
  the process-wide thread pool.
- A factory may consume an inverse generated by either the analytic or bumped
  calibration mode. It must not recompute or invert a forward Jacobian.
- State fingerprints are recomputed before any sweep, so a stale market fails
  without recording AAD nodes.

## Edge Cases

- Empty trade lists succeed with empty buckets, metadata, and PV totals.
- Empty provenance lists succeed with no quote buckets.
- Empty or duplicate calibration ids are rejected as specified above.
- Duplicate quote display names are allowed; ordinal and block, not the display
  string, determine identity.
- Zero and negative PV are valid. Numerical tests scale with gross absolute PV,
  not net PV.
- A trade that consumes none of a provenance's blocks contributes a structural
  zero, not an availability failure.
- Non-finite curve state, inverse entries, tolerance, PV, gradients, or output
  is a hard failure.
- A provenance state mismatch is isolated to that provenance; malformed ranges
  and non-partitioning axes throw.
- A joint-XCCY block failure cannot leave domestic-only, foreign-only, or
  basis-only quote buckets.

## Performance Contract

With provenance already built, quote transformation must:

- perform no calibration or quote bump;
- prepare each provenance once;
- reuse the existing node-risk batch hoisting behavior;
- use ordinary matrix multiplication with complexity proportional to the
  published parameter and residual dimensions;
- avoid work for structural-zero blocks;
- join `rate_risk_perf` with single-curve, joint-XCCY, and staged-basis cases.

Performance regression limits use the repository's existing benchmark gate.
The numerical oracle performs full recalibration only in tests and is not part
of production execution.

## Risks and Countermeasures

1. **Missing solver-scale correction.** Centralize the transform and pin both
   derivative and DV01 units against full-recalibration oracles.
2. **Axis drift.** Carry published ranges plus an axis fingerprint; never align
   by display name.
3. **Stale market state.** Bind the complete calibration and valuation state in
   a second fingerprint and verify it before recording.
4. **Partial joint vectors.** Use `(trade, provenance)` atomicity and discard
   sibling gradients on the first consumed-block failure.
5. **Cross-currency netting.** Aggregate by actual PV currency and preserve the
   existing unconverted policy.
6. **Unsupported chain rules.** Encode supported domains in factory names and
   expose stable unavailable results for valid excluded domains.
7. **False validation through the forward map.** Numerical acceptance always
   bumps original quotes, fully recalibrates, and fully reprices.
8. **Backend/tape divergence.** Reuse the existing node-risk sweep discipline
   and require the full compiler/AAD-backend PR matrix.

## Acceptance Summary

Implementation is complete only when:

- all three supported domains pass with analytic and bumped effective inverses;
- ordinary staged multi-curve and generic joint domains are stably unavailable;
- stale same-axis variants of curve values, bases, as-of, fixings, inverse, and
  tolerance are rejected before sweeping;
- a single failing joint dependency emits no partial quote bucket;
- C++/Python/Excel agree on buckets, units, fingerprints, tokens, ordering, and
  availability;
- the independent full-recalibration oracle passes every bucket/currency/domain
  in the required 5-, 10-, and 16-quote ladder fixtures;
- performance cases are wired into the repository regression gate;
- current pricing, calibration, node-risk, Python, Excel, generated-source, and
  documentation checks remain green.

The exact numerical oracle, thresholds, staged PR boundaries, test commands,
and rollback conditions are normative in the companion
[implementation plan](quote-space-dv01-implementation-plan.md).

## Open Questions

None for v1. Changes to success domains, fingerprint records, failure atomicity,
units, numerical thresholds, or threshold-update rules require a new reviewed
design revision; an implementation PR cannot change them opportunistically.
