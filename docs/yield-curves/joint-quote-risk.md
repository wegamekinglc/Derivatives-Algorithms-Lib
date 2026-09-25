# Generic Joint Multi-Curve Quote DV01

Generic same-currency joint calibration supports quote-space risk for an EXACT
solve that explicitly retains its effective inverse. OIS discount and tenor
curves share one residual vector and one full matrix transform. A quote can
therefore affect several curves and the resulting trade PV simultaneously.

## Calibration and the selected solution

Set `JointMultiCurveCalibrationOptions_::computeEffJacobianInverse_ = true`
before calibration. Its default is `false`. `CalibrateJointMultiCurve` in core
C++, and `CalibrateJointMultiCurveBundle` in public C++, perform one solve and
retain passive `Matrix_<>` data from that solve. Provenance construction and
portfolio aggregation neither invert the public forward Jacobian nor calibrate
again. `computeJacobianAtSolution_` and the inverse request are independent.

With M native parameters, N residual quotes, residual tolerance t, and raw quote
vector q, the retained M by N matrix E satisfies

$$
\delta x = E\,\delta q/t,\qquad
D=g^{\mathsf T}E/t,\qquad V=10^{-4}D.
$$

D has units of price per decimal quote; V is price per +1 bp. The inverse
scaling token is `solver_scaled`. Residual AAD and trade-PV AAD use separate
recordings and `TapeGuard_`; neither E nor provenance stores active numbers.

For M>N, a nonlinear calibration has multiple possible exact solutions. A local
weighted pseudoinverse at the final solution does not generally differentiate
the solution selected by a fixed-initial-guess nonlinear solve. The explicitly
requested generic joint inverse therefore defines a fixed affine parameter
subspace:

$$
B=W^{-1}J(x_0)^{\mathsf T},\qquad x=x_0+Bz.
$$

The initial guess x0 and smoothing matrix W come from the spec. J is the raw
residual Jacobian, whose mathematical derivative is independent of the quote
values subtracted from model rates. The existing solver finds z once, starting
at zero, retains its scaled inverse Ez, and returns E=B Ez. The local unscaled
response is B[J(x)B]^-1. Native parameter ranges still contain all M coordinates.

This opt-in selects an `initial_jacobian_chart` solution and can return different
parameters from the default underdetermined solve. Recalibration comparisons
must retain the same guesses, settings, and inverse request on every shock.
Changing the warm start changes the selected solution. Square systems use the
existing `local_weighted` mapping. Calls without the inverse request retain
the existing solver behavior.

`ANALYTIC` uses the existing joint eligibility rules and reports the actual
mode in `jacobianModeUsed_`; ineligible requests can fall back to `BUMPED`.
The explicitly requested EXACT bumped path uses central native-parameter
differences with step 1e-6. Default bumped calls retain their existing solver
difference policy. The inverse is published only when the private at-solution
Jacobian verifies the complete residual response. A rank-deficient quote map
reports `not_available_for_mapping` and produces provenance reason
`QUOTE_RISK_EFFECTIVE_MAPPING_INVALID`. Other inverse availability values are
`available`, `not_requested`, and `not_available_for_mode`.

## Coordinates and immutable provenance

`parameterRanges_` and `residualRanges_` contain `{curveIndex_, offset_, size_}`
in declaration order. They partition their respective axes exactly once.
`residualInstrumentOrdinals_` maps each actual solver residual to its original
ordinal within the declaration. Repeated display names do not define identity:
block keys are `curve:0`, `curve:1`, and so on; quote keys append the zero-based
ordinal in that block's residual order.

```cpp
Dal::JointMultiCurveCalibrationOptions_ options;
options.computeEffJacobianInverse_ = true;
const auto calibrated = Dal::CalibrateJointMultiCurveBundle(spec, options);
// market components must use calibrated.discountCurves_/forwardCurves_ handles.
const auto provenance = Dal::BuildJointMultiCurveQuoteRiskProvenance(
    spec, calibrated, options, market,
    {"usd-joint", {{"curve:0", "discount"}, {"curve:1", "forward-3m"}}});
const auto risk = Dal::AggregateRatePortfolioQuoteRisk(trades, market, {provenance});
```

The factory validates a non-empty calibration ID, unique complete bindings,
ranges, original quote ordinals, matrix dimensions/scaling, finite inputs,
spec/result consistency, and acyclic bases. Each bound curve must be the actual
calibrated handle. A numerically equal clone cannot replace a layered base or
collateral/tenor route. The valuation timestamp must fall on the calibration
date; its exact time and immutable fixing snapshot enter the state identity.

The new kind is `JOINT_MULTI_CURVE`, with schemes
`dal.quote-risk-axis/2+jcs+sha256` and
`dal.quote-risk-state/2+jcs+sha256`. The records bind native coordinates, quote
order, inverse/mapping, spec/options/result, routing, valuation, and fixings.
Fingerprints contain no pointer addresses. The three existing factories and
the no-argument scheme accessors retain their v1 byte contracts.

Result/options members are appended and existing overloads remain available.
This preserves old aggregate-prefix initialization; it does not promise binary
ABI compatibility or unchanged structured-binding arity.

## Aggregation and failures

Each provenance is prepared once per aggregation call. Stale sources are
rejected before risk sweeps. A malformed live source is a provenance failure;
trades consuming that source are not priced through it. Other trades and
independent provenances continue.

Joint native coordinates follow each trade's actual consumed curve/base graph
by handle identity. For XCCY trades, the roots are the selected domestic and
foreign discount and forecast curves plus any basis curve. A selected forecast
need not have its own key in `market.curveComponents_`: its base path still
contributes to a bound calibrated component. Unused registered descendants do
not enter preparation, and aliases do not duplicate a native sweep.

Only the target's native parameters are independent variables in its sweep.
Intermediate curve parameters stay constant while their base response remains
active; the target's own base stays passive. Mixed PWC, PWLF, LogDF, and ZeroRate
layers retain their original geometry, including historical PWC/PWLF knots and
left/right values. Standalone node risk and v1 quote-risk sources keep their
fixed-base semantics. Their preparation caches are separate from joint
preparation, so mixing source kinds does not make results depend on source
order. Passive pricing is shared once per trade.

A joint PV gradient includes these base paths before the complete
g-transpose-E transform. Independent single-curve DV01s cannot be concatenated
to reproduce this coupled response.

For live joint risk, every necessary graph node must be an exact builtin
`Tape::DiscountPWC_<double>`, `Tape::DiscountPWLF_<double>`,
`Tape::DiscountLogDF_<double>`, or `Tape::DiscountZeroRate_<double>` instance.
Opaque nodes and subclasses, including an otherwise unchanged builtin
subclass or an opaque unit-discount leaf, are outside this eligibility domain.
Provenance factory acceptance alone does not establish trade-level graph
eligibility. An incomplete graph cannot establish a structural zero. Existing
family, routing, root-representation, passive-validation, and expired-XCCY
gates retain their priority; a subsequent graph or preparation failure reports
`AAD_EVALUATION_FAILED`.

If one consumed block fails, the whole `(trade, provenance)` gradient is
discarded, including slices from earlier successful sweeps. Exceptions,
non-finite results, and incorrect gradient widths produce one
`QUOTE_RISK_TRADE_PROVENANCE_INCOMPLETE` metadata entry with the failing bound
key and original node-risk reason. Bound blocks are processed in declaration
order. Successfully priced passive PV is retained once, and healthy trades and
independent provenances continue. Invalid v2 sources are also checked along
unregistered XCCY root/base paths before passive pricing.

A non-consumed parameter block remains exactly zero without an AAD
sweep; its quote buckets can still contain coupled risk through E. A trade
consuming no bound components contributes structural-zero buckets. Numerical
roundoff in the dense matrix is distinct from structural zeros in the native
gradient. Empty inputs and zero/negative PV follow the existing contracts.
Outputs remain grouped by actual PV currency under `UnconvertedByActualPvCcy`.

APPROXIMATE, ordinary staged multi-curve chain rules, FX/volatility Greeks, and
automatic currency conversion are outside this domain.

## Language surfaces

Public C++ includes the calibration facade through `curvespec.hpp` and quote
risk through `curvepricing.hpp`. Python exposes `JointCurveDeclaration_`,
`JointMultiCurveCalibrationSpec_`, `JointMultiCurveCalibrationOptions_`,
`CalibrateJointMultiCurveBundle`, and a read-only result with owning curve maps,
copied matrices, and immutable ranges. `BuildJointMultiCurveQuoteRiskProvenance`
is keyword-only. Native calibration, provenance construction, and aggregation
release the GIL. See the [Python example](../../dal-python/examples/010.generic_joint_quote_risk.py).

Excel has reachable `JOINTCURVEDECLARATION.NEW`,
`JOINTMULTICURVECALIBRATIONSPEC.NEW`, `CALIBRATE.JOINTMULTICURVE`,
`JOINTMULTICURVECALIBRATIONRESULT.GET.CURVE`, and
`JOINTMULTICURVEQUOTERISKPROVENANCE.NEW` functions. The dedicated result getter
publishes matrices, ranges, residual ordinals, diagnostics and inverse metadata.
The [worksheet recipe](../../dal-excel/examples/009.generic_joint_quote_risk.md)
uses the existing ten-column quote-risk spill. The legacy
`RATEQUOTERISKPROVENANCE.NEW` dispatcher retains its v1 generic-joint exclusion;
expanding that dispatcher requires a separately versioned contract.

## Numerical acceptance and performance evidence

The executable oracle covers 2/3 blocks, N=5/10/16, both modes, all four native
parameterizations, and layered/unlayered curves (96 configurations, 992 buckets).
Separate tests cover mixed parameterizations, reversed input order, signed
positions, full native parameter responses, and source/failure boundaries.
Unregistered XCCY base paths have full-recalibration and registration-invariance
controls. Historical mixed chains also have central native-coordinate bump
oracles that rebuild dependent layers without recalibration or quote mapping.
Failure tests cover declaration order, preparation-cache reuse, mixed v1/joint
source order, discarded partial slices, and tape recovery.
Each original quote is recalibrated and repriced at ±1e-6 and ±1e-4; calibration
failure, non-finite output, or loss of an eligible trade fails the test.

| N  | Derivative absolute limit | Relative limit | DV01 absolute limit |
|----|---------------------------|----------------|---------------------|
| 5  | 5e-6 P                    | 5e-6           | 5e-10 P             |
| 10 | 1e-4 P                    | 1e-4           | 1e-8 P              |
| 16 | 1e-3 P                    | 1e-3           | 1e-7 P              |

P is max(1, gross absolute PV at the base and four shocked states) for the
currency and quote. Derivative and DV01 must each pass their absolute or
relative limit; both comparisons are required. The unit identity additionally
requires |V-1e-4 D| ≤64 epsilon max(1e-4 P, |V|, 1e-4 |D|). These are acceptance
limits for this domain, not historical v1 measurements or permission to expand
the supported domain without renewed evidence.

`DAL_JOINT_QUOTE_RISK_EVIDENCE_FILE` captures raw prices, gross values, errors,
thresholds, axes and identities. The CI validator checks the complete coordinate
manifest and recomputes every numerical gate; it writes a source-SHA and file
digest manifest. `rate_risk_perf` measures 100/1,000 trades at each width against
equivalent joint node risk plus a dense transform, with a ≤20% steady-state
overhead target and passive operation counters. Set
`DAL_JOINT_QUOTE_RISK_BENCHMARK_FILE` to retain all interleaved samples.
The existing nine-executable paired regression gate retains its two rounds of
ten samples and 4% confirmation rule; new cases become comparable when a
baseline includes them.
