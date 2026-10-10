# Native rate-trade quote curvature

Status: active. Builds on merged #523 and #524; this increment is a separate PR.

## Problem and goals

`EvaluateRateQuoteCurvature` already performs complete native recalibration and
quote pullback, but callers must build their own native scalar objective. Connect
the seven existing financial trade families to that driver without reproducing
pricing formulas or computing a trade-by-parameter Jacobian for a scalar request.

The financial objective is `V(q) = sum_i w_i PV_i(p(q), fixed context)`. Report
its value, raw-quote gradient and requested central differences of that gradient.
These products include calibration curvature. Weights, contracts, FX spot,
curve geometry and resolved fixings remain fixed across the request. Native
backend higher-order capability remains false.

## Requirements

1. Add a typed C++ public adapter accepting trades, an owning rate-calibration
   snapshot, the existing bump request, and optional settings. Support deposit,
   FRA, future, OIS, IRS, basis swap and XCCY terms through existing native pricing.
2. Support all four admitted square EXACT snapshot families: single curve,
   same-currency joint, staged XCCY basis and joint XCCY. Keep their quote and
   free-parameter axes, inverse validation and replay rules unchanged.
3. Retain the solved native pricing market privately in each snapshot. Every
   returned and recalibrated snapshot owns its calibrated curves and fixed graph;
   expose no mutable graph. Bind input coordinates through provenance's block-to-
   component map in its exact global free-parameter order.
4. Reuse core native active-graph reconstruction, including layered bases and
   XCCY tenor/collateral routing. Register staged fixed roots for internal pricing
   after provenance capture so they neither add risk coordinates nor change old
   provenance fingerprints. These roots are fixed dependencies.
5. Sum weighted trade PVs before reverse propagation. Retain the existing
   `1 + 2M` calibration/gradient/objective-reverse counts for M requested directions,
   independent of trade count. Do not form an intermediate per-trade Jacobian.
6. Empty weights mean unit weights. Explicit weights must match trade count and
   be finite; negative and zero weights are valid. Require a nonempty portfolio.
   Validate every supplied trade, including zero-weight rows, and preserve row/
   instrument context on failures. Duplicate instrument IDs are permitted.
7. Require one specified actual PV currency across all rows, including zero-
   weight rows. Use the XCCY contract's domestic currency for XCCY, not the market
   result label. Reject mixed currencies; perform no implicit FX conversion.
   Non-XCCY consumed curves must match the trade's actual currency. Return the
   currency explicitly alongside the ordinary curvature result.
8. Freeze request-owned trades and native pricing geometry before recording.
   Copy mutable market routing before capturing the internal objective; retain
   passive curve definitions, base closure and constants, never AAD numbers or
   tape nodes. Const evaluation must have no hidden mutable cache.
9. Supplement calibration fixings with trade fixings once before bumped replay.
   With an explicit fixing snapshot, use it plus the saved calibration fixings,
   without global fallback. With no explicit snapshot, capture only missing
   required historical trade observations from globals once. Overlapping saved
   values must agree; use native FX reciprocal validation. Missing observations
   reject with trade context. Never alter the calibration source's saved fixings.
   Same-time optional observations follow the native snapshot pricing rules.
10. Reject active outer recording and invalid numeric requests before preparation
    or global fixing reads. Preserve scalar/wide mode, tape cleanup, recording
    capacity and payload budget rules on success and failure. Numeric payload
    retains its existing meaning and excludes owning pricing/preparation data.
11. Leave the existing generic objective entry point and native calibrators
    compatible. Do not promote rectangular/approximate calibration, FX quote
    axes, simulation policies, Python/Excel bindings or native mixed mode here.

## API and ownership

The [API note](../api-notes/aad-rate-trade-quote-curvature.md) defines the public
surface. A small core internal objective builder owns trades, prepared cashflows,
fixed pricing context and the native curve preparation. Its callback consumes
native parameters plus a declared unused tail; the public adapter supplies the
raw-quote tail required by the existing calibration driver. The resulting direct
raw-quote derivative is zero for these fixed financial contracts.

## Acceptance

- First demonstrate a failing financial public request against the missing
  adapter, then implement and retain RED/GREEN evidence.
- Compare value, gradient and curvature with independently requoted original
  native calibrators followed by passive `PriceRateTrade`/`PriceRateTrades`.
  Cover all seven families, all four snapshot families, layered coupling,
  cross-block directions, three outer steps and independent nested-step checks.
  References must not invoke the new objective builder or calibration pullback.
- Verify weighted linearity, currency labels/admission, duplicate IDs, empty and
  malformed inputs, fixing isolation/conflicts/missing data, caller mutation,
  zero directions, finite outputs, scalar/wide restoration, budget failure and
  recovery, and preservation of active outer recording.
- Run affected existing public rate-curvature and core parameter-Jacobian cases;
  add pricing cases only for a specific shared-helper change. Rebuild changed
  objects and affected test executables with captured dependency/config identity.
- Run strict OFF/combined diagnostics probes and installed public consumption.
- Select the smallest performance set covering changed public entry points,
  fixed/free XCCY graph boundaries and affected old callers. Preserve calibrated
  paired sampling and the sustained 4% old-caller gate. Exclude unrelated tape,
  Monte Carlo, Dupire, PDE and linear-algebra matrices with recorded reasons.
- Before merge, inspect all current-head CI, Codacy annotations, complete review
  bodies, issue comments and inline threads. Verify new tests actually execute in
  applicable runtime profiles, repeat exact-head merge audits and prove tested
  merge-preview / final merged-tree identity.

No unresolved design question requires user input. Additional trade types or
currency conversion require separately specified work.
