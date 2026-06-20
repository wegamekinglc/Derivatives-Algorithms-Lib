# Joint Multi-Curve AAD Analytic Jacobian (Phase B) - API Note

> Status: **REVISED — Option B LOCKED by the user on 2026-06-20.**
> Parameterization is `PIECEWISE_LINEAR_FWD` (native to the joint module); the
> AAD path authors a new templated `Tape::DiscountPWLF_<T_>` with a templated
> base handle. Default `jacobianMode_` = `ANALYTIC`, matching single-curve.
> `OISSwap_` rides `Swap_::PrecomputeT<T_>()` (spec OQ-1 RESOLVED) — no new
> OIS templated rate is authored. Pair with the full design at
> `.claude/designs/joint-aad-gradient.md`. This note locks the
> developer-facing surface: public headers, the options struct, the
> `Gradient` signature, NOTICE text, and the example-program shape.
> Implementation goes to `dal-implementer` after `dal-critic` signs off.

## Audiences

- **C++ quants:** the new `JointMultiCurveCalibrationOptions_` struct and the
  two-arg `CalibrateJointMultiCurve` overload are the only public-surface
  additions. The mode is per-call, defaults to `ANALYTIC` (matching the
  single-curve `CurveCalibrationOptions_` default), and composes with the
  existing `solveMode_`. No changes to `JointCurveDeclaration_` or
  `JointMultiCurveCalibrationSpec_`. Existing callers exercise the AAD path
  by default after the upgrade; on ineligible specs they get a one-time
  NOTICE and the byte-for-byte bumped result.
- **Excel users:** n/a — no binding is in scope (spec NG3). The
  `CurveJacobianMode_` enum is reused unchanged from single-curve, so a
  future binding inherits it for free.
- **Python users:** n/a — no binding is in scope (spec NG3).

## Surface Today

```cpp
// dal-cpp/dal/curve/jointcalibration.hpp (current, unchanged)
struct JointCurveDeclaration_ { /* 11 fields, parameterization_ defaults to PIECEWISE_LINEAR_FWD */ };
struct JointMultiCurveCalibrationSpec_ { /* 10 fields, solveMode_ defaults to EXACT */ };
struct JointCurveCalibrationDiagnostics_ { /* per-curve diagnostics, no jacobian_ field */ };
struct JointMultiCurveCalibrationResult_ { /* discountCurves_, forwardCurves_, diagnostics_, solverEvaluations_ */ };

JointMultiCurveCalibrationResult_ CalibrateJointMultiCurve(const JointMultiCurveCalibrationSpec_& spec);
```

```cpp
// dal-cpp/dal/curve/jointcalibration.cpp (current, the nullptr to replace)
// JointResidualFunction_::Gradient returns nullptr -- the solver dense-bumps.
[[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>&, const Vector_<>&) const override { return nullptr; }
```

The single-curve `CurveJacobianMode_` enum (`dal-cpp/dal/curve/calibration.hpp:38-51`)
already exists with values `{BUMPED, ANALYTIC}`, Machinist-generated, `switchable`.
Phase B reuses it verbatim — **no Machinist regen required for the flag**
(spec NG7, AC10).

## Proposed Surface

### Public header additions (`dal-cpp/dal/curve/jointcalibration.hpp`)

~~~cpp
// Solver-side options for joint multi-curve calibration. NOT serialized with the
// spec: the spec describes WHAT to calibrate (declarations, knots, instruments);
// the options describe HOW to solve (Jacobian construction). A default-constructed
// JointMultiCurveCalibrationOptions_ engages the AAD path on eligible specs
// (matching the single-curve CurveCalibrationOptions_ default); on an ineligible
// spec it emits a one-time NOTICE and falls back to the byte-for-byte bumped path.
struct JointMultiCurveCalibrationOptions_ {
    // Jacobian construction for the joint calibration solver.
    //   BUMPED   -- finite-difference bumping of every free parameter. Always
    //               available; byte-for-byte identical to the pre-Phase-B path.
    //   ANALYTIC -- AAD-derived dense Jacobian over the joint stacked parameter
    //               vector (default). Engages only when EligibleForAnalyticJacobian()
    //               is true (every declaration PIECEWISE_LINEAR_FWD + base collateral
    //               resolves + vanilla Deposit/FRA/Future/Swap, with OISSwap_ riding
    //               Swap_ + tradeDate == knot 0); otherwise falls back to BUMPED with
    //               a NOTICE (at most once per CalibrateJointMultiCurve call; never
    //               throws). See .claude/designs/joint-aad-gradient.md.
    //
    // DEFAULT IS ANALYTIC, matching single-curve CurveCalibrationOptions_
    // (dal-cpp/dal/curve/calibration.hpp:102). Every existing joint caller
    // exercises the new Tape::DiscountPWLF_<T_> + Tape::JointCurveBlock_<T_>
    // machinery on eligible specs after the upgrade; the mitigation is the
    // AAD-vs-bumped oracle test (spec AC1) and the four-backend build matrix
    // (spec AC6).
    CurveJacobianMode_ jacobianMode_ = CurveJacobianMode_::Value_::ANALYTIC;
};

// Existing single-arg overload -- UNCHANGED. Delegates to the two-arg form with
// default-constructed options (-> ANALYTIC), so existing callers exercise the
// AAD path by default on eligible specs.
JointMultiCurveCalibrationResult_ CalibrateJointMultiCurve(const JointMultiCurveCalibrationSpec_& spec);

// NEW two-arg overload.
JointMultiCurveCalibrationResult_ CalibrateJointMultiCurve(const JointMultiCurveCalibrationSpec_& spec,
                                                            const JointMultiCurveCalibrationOptions_& options);
~~~

### `Gradient` signature (unchanged from the underlying interface)

~~~cpp
// dal-cpp/dal/curve/jointcalibration.cpp -- JointResidualFunction_ member
// Signature is unchanged (mirrors Phase A at calibration.cpp:368).
[[nodiscard]] Underdetermined::Jacobian_* Gradient(const Vector_<>& x, const Vector_<>& f) const override;
~~~

The returned `Underdetermined::Jacobian_*` is a `new`-ed `XCurveJacobian_`
(factored into the shared header `dal-cpp/dal/curve/curvejacobian.hpp` per
design §8); the caller (the solver) owns it. Non-null iff
`jacobianMode_ == ANALYTIC` AND the cached joint-eligibility verdict is
`Eligible`; otherwise `nullptr` and the solver dense-bumps.

## Why This Shape

- **New `JointMultiCurveCalibrationOptions_`, not a field on the spec.**
  Phase A's H2 resolution
  (`.claude/designs/aad-analytic-jacobian-naming-and-flag.md:376-401`)
  established the pattern: the mode is a per-solve choice, not a property of
  the curve set, and must NOT serialize with the spec. The joint path
  inherits the same reasoning. A per-declaration override on
  `JointCurveDeclaration_` would allow mixed modes (declaration 0 analytic,
  declaration 1 bumped), which is incoherent — the joint AAD path is all-or-
  nothing.
- **Reusing `CurveJacobianMode_` (no new enum).** The single-curve enum is
  two-valued and Machinist-generated; the joint path needs the same two
  values. A separate `JointCurveJacobianMode_` would violate "one way to do
  it" and force a Machinist regen + Java reconciliation step (the B3
  landmine from `redesign.md`). Reuse is free.
- **Default `ANALYTIC` (matching single-curve).** The earlier Option-A draft
  defaulted to `BUMPED` on the grounds that Phase B ships new templated
  machinery. The user LOCKED the default to `ANALYTIC` to match single-curve
  (`dal-cpp/dal/curve/calibration.hpp:102`), accepting that every existing
  joint caller exercises the new templated machinery on eligible specs after
  the upgrade. The mitigation is the oracle test (spec AC1) and the
  four-backend build matrix (spec AC6): if the new machinery is correct on
  all four backends, defaulting to ANALYTIC is safe; if it is not, the
  oracle test fails loud before merge.
- **Composition with `solveMode_`.** ANALYTIC engages for both EXACT and
  APPROXIMATE; the Jacobian is well-defined in both. The at-solution forward-
  Jacobian capture is EXACT-only (deferred anyway — design §9.4).
- **Two-arg overload delegates; single-arg overload preserved.** Existing
  callers are undisturbed (spec AC8). "One way to do it": the two-arg form
  is the real entry point, the single-arg form is a convenience overload
  that pins the default.
- **`XCurveJacobian_` factored into a shared header.** The subclass is
  structurally identical to what the joint path needs and is already in
  `Dal::` (not anonymous) for exactly this reason. Factoring it (design §8)
  avoids a second copy that must track the first.

## Error Cases (NOTICE contract — `ANALYTIC` never throws)

Every fall-through emits a NOTICE naming the declaration index (and instrument
name where applicable) and the failing condition. Frequency contract: each
NOTICE fires **at most once per `CalibrateJointMultiCurve` call** (the
eligibility verdict is evaluated once and cached on
`JointResidualFunction_`, mirroring Phase A H1 at
`dal-cpp/dal/curve/calibration.cpp:282-307, 396-409`). Ineligibility routes
through NOTICE + `nullptr` (solver dense-bumps); no `REQUIRE` or `THROW`.

| Input violation | NOTICE text |
|------------------|-------------|
| `ANALYTIC` + a declaration's parameterization != PIECEWISE_LINEAR_FWD | `"Joint AAD Jacobian requires CurveParameterization_::PIECEWISE_LINEAR_FWD on every declaration; declaration N has X; falling back to bumped"` |
| `ANALYTIC` + a declaration contains an out-of-scope instrument type (BasisSwap/STIR) | `"Joint AAD Jacobian has no templated rate for instrument 'name' (type) in declaration N; falling back to bumped"` |
| `ANALYTIC` + a discount-declaration instrument projects (`useProjectionCurve_ == true`) | `"Joint AAD Jacobian requires discount-declaration instruments to forecast off the discount curve; instrument 'name' in declaration N projects, falling back to bumped"` |
| `ANALYTIC` + a base-layered forward's base collateral is not an eligible discount declaration | `"Joint AAD Jacobian requires base-layered forward declaration N's base collateral to be an eligible discount declaration; falling back to bumped"` (defensive — structurally guaranteed by ValidateAndBuildSlots) |
| `ANALYTIC` + an instrument's `TradeDate() != declaration.knotDates_.front()` | `"Joint AAD Jacobian requires every instrument to trade at its declaration's knot 0; instrument 'name' in declaration N does not, falling back to bumped"` |

The NOTICE text uses "Joint AAD Jacobian" (not "ANALYTIC Jacobian") to
distinguish joint NOTICEs from single-curve NOTICEs in a quant's log. Cosmetic;
reviewer's call.

## Eligibility Contract (headline)

`EligibleForAnalyticJacobian()` is a per-SPEC predicate (one verdict for the
whole joint system, not per-declaration), because the joint AAD path is all-or-
nothing — a single ineligible declaration disables the joint AAD path (mirrors
Phase A's "no mixed-row mode" at
`.claude/designs/aad-analytic-jacobian-selector-api.md:71-77`). Returns `true`
iff ALL of:

- (a) `jacobianMode_ == ANALYTIC` (default under Option B);
- (b) NO compile-time gate (matching shipped Phase A); the four-backend test
  mandate (spec AC6) is the verification;
- (c) every declaration's `parameterization_ == PIECEWISE_LINEAR_FWD`;
- (d) `solveMode_` consistency: ANALYTIC engages for both EXACT and APPROXIMATE
  (the at-solution forward J capture is EXACT-only and deferred — design §9.4);
- (e) every instrument in every declaration is one of
  `{Deposit_, FRA_, Future_, Swap_}`, with `OISSwap_` riding `Swap_`
  (`OISSwap_ : public Swap_` at `ycinstrument.hpp:155`; `PhaseARateAt<T_>`
  dispatches it via `dynamic_cast<const Swap_*>` at `calibration.cpp:507` →
  `Swap_::PrecomputeT<T_>()` at `ycinstrument.cpp:497`);
- (f) every forward-declaration instrument has `useProjectionCurve_ == true`
  (structurally guaranteed by `ValidateAndBuildSlots` at
  `dal-cpp/dal/curve/jointcalibration.cpp:225-231`);
- (g) every discount-declaration and baseless-forward-declaration instrument has
  `useProjectionCurve_ == false`;
- (h) every base-layered forward declaration's `targetCollateral_` is produced
  by a discount declaration in the same spec whose parameterization is also
  PIECEWISE_LINEAR_FWD;
- (i) every instrument's `TradeDate() == its declaration's knotDates_.front()`
  (which under PWL_FWD is `> today_` per the joint validator at
  `jointcalibration.cpp:203-204` — PWL_FWD has NO today-pinned anchor, so the
  rule is `tradeDate == knot 0`, NOT `tradeDate == today_`).

## Example

The example program at
`dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp`
needs NO parameterization edit under Option B — its declarations already
inherit `PIECEWISE_LINEAR_FWD` from the `JointCurveDeclaration_` default
(`jointcalibration.hpp:47`), its knot ladder already starts strictly after
`today_` (satisfying the validator), and its OIS slice is `OISSwap_` (which
rides `Swap_::PrecomputeT<T_>()`). The existing `tolerance_ = 1.0e-10` /
`fitTolerance_ = 1.0e-10` edits are PRESERVED (do not touch — spec hard
constraint).

Under the LOCKED default (`ANALYTIC`), the existing single-arg call exercises
the AAD path natively. The example may optionally add an explicit A/B
comparison via the two-arg overload for clarity:

~~~cpp
// dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp
// (illustrative excerpt -- under Option B the declarations stay
// PIECEWISE_LINEAR_FWD; under the LOCKED default the single-arg call already
// engages ANALYTIC. The two-arg overload is optional, for an explicit A/B.)

int main() {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const MarketSet_ market = BuildMarket(today, ccy);

    const JointMultiCurveCalibrationSpec_ jointSpec = BuildJointSpec(today, ccy, market);
    // BuildJointSpec is UNCHANGED -- declarations inherit PIECEWISE_LINEAR_FWD,
    // knots start strictly after today_, OIS slice is OISSwap_ (rides Swap_).

    // Under the LOCKED default (ANALYTIC), this single-arg call engages the
    // AAD path on the eligible joint spec.
    const JointMultiCurveCalibrationResult_ rAnalytic = CalibrateJointMultiCurve(jointSpec);

    // Optional explicit A/B: bumped vs analytic. Both calibrated curves agree
    // within solver tolerance; the analytic run uses fewer solver evaluations.
    JointMultiCurveCalibrationOptions_ optBumped;
    optBumped.jacobianMode_ = CurveJacobianMode_::Value_::BUMPED;
    const JointMultiCurveCalibrationResult_ rBumped = CalibrateJointMultiCurve(jointSpec, optBumped);

    // ... existing DF comparison + self-checks ...
}
~~~

## Open Questions

- **OQ-1 (spec, RESOLVED).** Parameterization scope — Option A
  (LOG_DISCOUNT-only) vs Option B (extend AAD to PIECEWISE_LINEAR_FWD).
  **RESOLVED in favour of Option B by the user on 2026-06-20.** The API
  surface is the same under either option; Option B avoids the example edit
  and the validator fork that Option A would have required.
- **OQ-B (design, RESOLVED).** `OISSwap_` coverage. **RESOLVED: `OISSwap_`
  rides `Swap_::PrecomputeT<T_>()`** (`OISSwap_ : public Swap_`; Phase A's
  `PhaseARateAt<T_>` dispatches it via `dynamic_cast<const Swap_*>`). No new
  `Tape::OisSwapRate_<T_>` is authored; the example's OIS slice exercises the
  joint AAD path natively.
- **OQ-C (design, RESOLVED).** Joint default — `BUMPED` (the earlier draft's
  recommendation) or `ANALYTIC` (matching single-curve). **RESOLVED in favour
  of ANALYTIC by the user on 2026-06-20.** The default matches single-curve;
  the mitigation is the oracle test (spec AC1) and the four-backend build
  matrix (spec AC6).

## Hand-off

- **API note path (absolute):**
  `/home/wegamekinglc/dev/github/my-claude/workspace/Derivatives-Algorithms-Lib/.claude/api-notes/joint-aad-gradient.md`
- **Design doc path (absolute):**
  `/home/wegamekinglc/dev/github/my-claude/workspace/Derivatives-Algorithms-Lib/.claude/designs/joint-aad-gradient.md`
- **Blocking dependencies:** NONE — Option B is LOCKED; OQ-1 (parameterization)
  and OQ-B (`OISSwap_`) are RESOLVED; OQ-C (default) is LOCKED to ANALYTIC.
- **Surface is LOCKED.** Next agent: `dal-critic` for an adversarial pass on
  the spec + design + this note, stressing the Option-B risk surface (templated
  PWL-forward integration on `T_`, templated base handle across four backends,
  2-params/knot column map — design §14); then `dal-implementer`.
