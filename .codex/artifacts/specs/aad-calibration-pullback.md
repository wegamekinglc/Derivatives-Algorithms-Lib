# F01 common passive calibration pullback

Status: active contract. Native opt-in record capture and the shared C++
operation/result are accepted, including all 35 exact `12b3d7d` CI checks.
Python capture/common projections pass full local and incremental performance
verification; their own publication-head CI remains required. Excel and complete
F01 request/budget integration remain open.
This contract completes the common curve-adaptation portion of F01 without
closing the remaining binding, request-integration or performance requirements.

## Source and problem

The controlling [detailed plan](https://github.com/wegamekinglc/Derivatives-Algorithms-Lib/blob/de5dd8b20089223e6938dfd8100d463aa6d4a169/.codex/artifacts/plans/aad-improvement-plan.md)
requires a common calibration pullback and result structure, including an
adapter for existing curve quote risk. The
[Dupire contract](aad-dupire-pullback.md),
[structured risk contract](aad-risk-results.md), and current
[curve inverse](../../../docs/yield-curves/jacobian-risk.md) and
[joint risk](../../../docs/yield-curves/joint-quote-risk.md) methods control
the mathematical meaning. The native-only amendment remains in force.

Dupire already provides a checked scalar calibration VJP and separate direct
quote contributions. Curve aggregation already applies a retained effective
inverse with its original tolerance scaling. The two operations have different
result types and do not provide a common passive entry point.

Curve provenance owns axes, bindings, state fingerprints, tolerance and inverse.
Its builders hash a complete canonical state record and discard that record.
A shared snapshot must support full content comparison after a fingerprint
match. Retaining this record eagerly for every legacy request would add memory
and storage work to an existing path, so capture must be explicit.

## Goals and boundaries

Provide a single passive pullback operation and common result with separate
calibration, direct and total raw quote derivatives. Preserve canonical domain
coordinates, units, snapshot content, method and boundary. Support all four
currently available curve provenance kinds, plus the accepted Dupire boundary.

Keep the current curve inverse, selected solution, fixed-base versus joint
coordinates, numerical method, aggregation and currency semantics. The new
boundary performs neither a curve solve nor a fresh inverse construction.
It is not the F03 implicit-solver implementation. APPROXIMATE, unsupported or
unavailable mappings retain their existing reasons; no automatic fallback is
introduced. Stochastic-rate joint Dupire, moving strikes and second order
remain later requirements.

## Functional requirements

1. Append `retainCalibrationRecord_ = false` to
   `RateQuoteRiskProvenanceConfig_`. Existing factory signatures, default
   behavior and aggregate-prefix initialization remain. Add a const
   `CalibrationRecord()` getter returning byte-preserving `std::string` data;
   default provenance returns an empty record.
2. With capture enabled, retain the exact canonical bytes used for the existing
   state fingerprint. Include spec, options, result, numeric quotes/curves,
   inverse/scaling, ranges, bindings, market timestamp/fixings and joint routing
   already represented in that record. Canonicalize once. The storage preference
   does not enter the mathematical state or change v1/v2 fingerprint bytes.
   Captured records are immutable and share ownership with provenance.
3. Introduce a passive `CalibrationPullback_` in public C++ with constructors
   from `DupireCalibrationSnapshot_` and `RateQuoteRiskProvenance_`. It owns its
   typed source through immutable storage. A curve source requires both an
   available mapping and a retained record. Do not silently manufacture missing
   content from a fingerprint or request another calibration.
4. Represent parameter and direct quote seeds as owning passive values bound
   to a `CalibrationPullback_`. Use one implementation for the two seed roles.
   Dupire matrices retain spot/time or strike/maturity orientation. Curve
   matrices are M-by-one or N-by-one columns in original global ordinal order.
   Reject wrong dimensions, non-finite values, unsupported types and source
   mismatches before publication. Zero and negative seeds are valid.
5. A parameter seed must match the complete source point. For curves compare
   domain, ID, schemes, complete axes/bindings and retained canonical state,
   using fingerprints only as a quick rejection. Use case-sensitive byte
   equality for canonical records, not DAL's case-insensitive String_ equality.
   For Dupire preserve the existing content comparison. Same dimensions or
   display labels alone never establish identity.
6. Preserve Dupire's existing direct-quote identity rule: equal ordered quote
   axes and quote values may have different fixed bases. Curve direct seeds
   require the same complete captured provenance; this initial curve boundary
   does not infer a separate quote-only identity from display names.
7. `PullbackCalibration(source, parameters, direct = {})` returns a common
   immutable `CalibrationQuoteRisk_` with `Calibration()`,
   `CalibrationAdjoints()`, `DirectAdjoints()`, `TotalAdjoints()`, `Method()`,
   `Unit()` and `Boundary()`. It retains the canonical typed source for axes
   and provenance. Numeric contributions have the domain's quote matrix shape.
   Add the direct PV derivative once, after calibration mapping.
8. Delegate Dupire to its existing checked VJP, including additive alias seeds,
   contracted-arithmetic replay and lifecycle rules. Do not add an outer
   recording around a provider that owns its own independent recording.
9. For curves, compute the original `g-transpose-E/tolerance` mapping in its
   original arithmetic order. Reuse the mapping primitive with legacy
   aggregation through a shared implementation; keep validation and common
   metadata allocation outside the legacy hot path. A consumer callback can
   preserve one output loop and the original allocation count.
10. Curve method is `RetainedCurveEffectiveInverse`, unit is `DECIMAL_QUOTE`,
    and boundary is `FrozenCalibrationEffectiveInverse`. These labels do not
    claim that a requested ANALYTIC mode was used, or that a retained weighted
    linearization is the exact derivative of every nonlinear selected solution.
    Original mode/mapping details remain in captured provenance. Dupire retains
    its accepted method, `decimal-vol` and fixed-input boundary labels.
11. Curve mapping and all getters are passive: no recording/reset, worker
    submission, fixing lookup, repricing, calibration or inverse construction.
    They may run while an unrelated native recording is active without changing
    it. Dupire continues to reject unsupported independent nesting. Neither
    source, seed nor result retains active numbers or borrowed inputs.
12. Extend C++/Python/Excel with strict input validation and detached mutable
    numeric projections. Python exposes record capture through its existing
    config, and common operation factories with required arguments first.
    Excel appends a default-false capture option to the native provenance
    factories while preserving old argument prefixes and test-export overloads.
    Its legacy dispatcher retains the existing generic-joint exclusion.
    Common handles reject archive serialization explicitly. Getters do no work
    beyond projection, and failed factories retain prior output handles.

## Resources, compatibility and failure

Default curve construction does not retain canonical storage. Record capture
adds storage proportional to the canonical source, including any retained
inverse, only when explicitly requested. The shared result adds three quote
matrices; it does not build a new dense calibration Jacobian. Dupire remains a
single VJP. Capacity/dimension arithmetic must reject overflow before allocation.

Keep public history in the changelog and publish current-state docs only after
the entry points exist. Preserve every existing tolerance, path count, quote
ordering, selected-solution option and benchmark policy. Added config members
preserve aggregate prefixes; do not promise unchanged structured-binding arity
or binary layout for configuration aggregates.

Unavailable curve sources report their original reason. Missing captured
content reports `QUOTE_RISK_CALIBRATION_RECORD_NOT_RETAINED`. Other failures
identify `InvalidCalibrationPullback` or `CalibrationSnapshotMismatch` and the
offending source, seed role or matrix coordinate. Never publish partial results
or accept non-finite accumulated outputs. A failure must not poison the next
independent operation.

## Acceptance

- Establish missing-interface RED before record capture, common boundary and
  each language projection. Keep all failed evidence.
- Default/captured records produce identical axes, state fingerprints, inverse,
  tolerance, prices and legacy quote buckets for SINGLE_CURVE, JOINT_XCCY,
  STAGED_XCCY_BASIS and JOINT_MULTI_CURVE. Verify actual canonical content,
  including quote and parameter values, fixes and v2 routing. Keep records after
  source input mutation/destruction. Old defaults retain no record.
- Test same-size reordered axes, changed quotes/parameters/settings, different
  bindings/IDs and case-only canonical content differences. Reject mixed domains,
  empty/wrong shapes, NaN/infinity and unavailable mappings. Test zero/negative
  seeds, direct-only, nonzero direct plus calibration and overflow.
- Compare the common Dupire boundary bitwise to accepted typed VJP traces and
  maintain every existing flat/Merton, tree/compiled complete-chain oracle.
- Compare common curve results with actual legacy aggregation on the same
  priced gradients, including mixed representations, coupled blocks, layered
  bases, reversed report order, signed portfolios and actual PV currencies.
  Raw decimal sensitivities and DV01 must retain their existing scaling.
- Independently bump each original curve quote, recalibrate with identical
  guesses/inverse request/settings and reprice. Retain existing ±1e-6 and
  ±1e-4 oracles and their published N=5/10/16 limits. New directional calibration
  checks use declared 2e-6/1e-6/5e-7 steps; require two adjacent passes at
  abs/rel 1e-6 for the specified smooth square small cases. Do not apply a
  square-system reference to a different underdetermined solution chart.
- Prove no calibration, fixing, worker or tape work for curve mapping/getters.
  Preserve an unrelated active native graph and its analytic derivative.
  Test success/failure/success and repeat operations with independent seeds.
- C++ installed consumers, Python joint/standalone and Excel portable/Windows
  exercise complete shared chains, strict types, copied values, output retention
  and serialization errors. Verify common coordinate/method/unit/provenance
  parity. Keep new Excel native declarations ahead of SDK input headers.
- Run fresh OFF/combined full suites and appropriate fully instrumented
  ASan/UBSan, formatting, CCN-eight, docs, generation and installed checks.
  Inspect exact publication-head CI; earlier green heads are insufficient.
- Preserve and run the nine-target gate and changed existing curve construction
  and aggregation workloads with the unchanged two-round/ten-process/4% policy.
  Separately report opt-in record/shared-entry cost. P01's inconclusive production
  verdict remains open until its own controlled evidence resolves it.

## Delivery and open requirements

Deliver opt-in record capture first, then shared native/public operations,
then Python and Excel, retaining a separate acceptance record for each increment.
Follow the [API decision](../api-notes/aad-calibration-pullback.md) and
[critique](../critiques/aad-calibration-pullback.md). Market-coordinate request
planning, numeric payload budgets and compatibility projections remain part of
the full D04/F01/F02 integration audit; this explicit boundary alone does not
close them. No user clarification is currently needed.
