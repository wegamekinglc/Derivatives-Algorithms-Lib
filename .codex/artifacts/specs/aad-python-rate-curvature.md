# Python native rate-trade quote curvature

Source: the native-only AAD roadmap and accepted `ratecurvature.hpp` financial
adapter. This increment follows merged #529 (`4df4a0b3`); it is a separate PR.

## Problem and scope

Python can construct native rate calibration specifications and trade definitions,
but cannot capture the native immutable calibration snapshot or request its
financial quote-space Hessian products. Add a passive projection of the existing
four-family adapter. Preserve native algorithms, first-order interfaces and
capability flags. Do not expose an active scalar, arbitrary Python objective,
generic native callback or the experimental smooth mixed-mode prototype.

## Requirements

1. `RateCalibration_New(spec)` accepts each existing native single-curve, joint
   multi-curve, staged-XCCY and joint-XCCY spec. It copies inputs before GIL
   release and calls the matching `NewRateCalibration` overload. Native exact,
   square calibration and supported-curve restrictions remain authoritative.
2. Expose a copied, immutable `RateCalibrationSnapshot_` with full raw `point`,
   solved `parameters` and native `provenance`. Preserve quote order, distinct
   declaration keys, units, fingerprints and staged fixed-root exclusion.
3. `RateCalibration_Recalibrate(calibration, quotes)` accepts a native snapshot
   and copied list/tuple of strict finite real quotes. Reject bool/enums and
   implicit numeric coercions. Native replay validates count and axis identity.
4. `RateTradeQuoteCurvatureSettings_(*, weights=None, fixings=None)` owns strict
   copied finite weights and a typed immutable fixing-snapshot handle. Empty
   weights retain unit weighting; signed/zero values are supported. Native
   execution checks trade count and validates every trade, including zero weights.
5. `RateTradeQuoteCurvature(trades, calibration, bumps, *, settings=None)` copies
   a list/tuple of exact native trade values, snapshot, #529 bump request and
   settings before GIL release. No borrowed Python or mutable list state survives.
6. Return passive `RateTradeQuoteCurvatureResult_` with actual `currency` and a
   copied nested `curvature`. Expose value, gradient, point, directions, steps,
   Hessian products, base calibration and execution. Getters/copy/deepcopy cannot
   mutate internal results; immutable snapshot internals may be shared safely.
7. Keep method `BumpOverRecalibratedNativeRateAAD`, full raw quote coordinates,
   `1+2M` gradients/calibrations/objective reverses, numeric payload, tape peak
   and cleanup reserve. Finite-step products are not exact native higher order.
8. Reuse the existing bump type, optional-zero numeric/recording budgets and
   native validation. Empty directions keep a `0 x N` product and full gradient;
   malformed dimensions, zero rows, invalid steps and unsupported requests reject.
9. Preserve sealed valuation/fixing inputs, explicit additional trade history,
   saved-fixing conflict checks, currency/component admission and recovery.
10. Errors identify the public function/type and offending field. Conversion
    rejects wrong native objects without calling arbitrary Python objectives.

## Acceptance

- RED: the merged installed baseline lacks `RateCalibration_New`.
- Single-curve off-knot deposit: independent analytic PV/quote gradient and
  finite-step analytic-gradient HVP; signed/mixed rows and convergence toward
  the smooth analytic Hessian with suitable steps.
- Small examples execute all four calibration alternatives. Check native quote
  axes/provenance, native base-only replay and independently bumped gradients.
- Weighted linearity, duplicate IDs, empty directions, exact/one-byte-short/zero
  numeric caps, recording cap propagation, currency/component failures and valid
  recovery. Keep invalid zero-weight trade rejection.
- Strict type/sequence/number errors, copied inputs/getters/nested values,
  copy/deepcopy, garbage collection, explicit historical fixings, GIL release
  and independent calling threads. Use stable financial scales for tolerances.
- New tests plus directly affected existing shared-bump/calibration/pricing
  consumers; no full native or parameter matrix repeats locally.
- Reuse #529 native libraries only with exact source/header/configuration and
  executable identity. Rebuild changed binding/module units and install them.
- Select only new complete boundary costs before measuring, with calibrated
  loops and two rounds of ten alternating interleaved pairs. No native speedup
  claim and no old unrelated timing repeats.
- Fresh exact-head CI/Codacy, actual Python cases in extended/MSVC profiles,
  complete reviews, two final audits and guarded merge.

Open questions: none required for this projection.
