# Joint Multi-Curve AAD Analytic Jacobian (Phase B) - API Note

> Status: **REVISED (third pass) — 2026-06-20.** This pass DROPS the
> second-pass CP4 / critique-B5 decision (reject `OISSwap_`, switch the
> example to vanilla `Swap_`, defer `Tape::OisSwapRate_<T_>` to Phase B+1).
> The lead re-verified `dal-cpp/dal/curve/ycinstrument.cpp`: the library has
> NO geometric overnight compounding — both the AAD path
> (`Tape::SwapRate_<T_>::operator()`, `ycinstrument.cpp:347-368`) and the
> double path (`ForwardRate`, `ycinstrument.cpp:53-59`) price OIS with the
> IDENTICAL simple-rate arithmetic formula `(1/DF - 1) / basis`, accumulated
> per period as `fixing * dcf * DF`. AAD and bumped therefore evaluate the
> SAME function on OIS rows, so the AAD-vs-bumped oracle (spec AC1) PASSES for
> OIS. The OIS overnight index has `useProjectionCurve_ == false`
> (`ycinstrument.cpp:44`) -> `forecast == discount == OIS`, so the inherited
> single-curve `Swap_::PrecomputeT<T_>` is BOTH routing-compatible AND
> gradient-correct for OIS. **OIS is restored to full ANALYTIC; the example's
> OIS swaps stay `OISSwap_`.** One decision remains LOCKED from the second
> pass:
>
> - **CP3 (B4):** the IBOR(3M) PROJECTION slice — where
>   `forecast(3M) != discount(OIS)` — is priced through a NEW
>   `Tape::JointRate_<T_>` base (sibling of Phase A's `Tape::Rate_<T_>`, which
>   is bound to `YCCtx_<T_>` and untouched) whose `operator()` takes a
>   `const JointCurveBlock_<T_>&`. A joint-local `ProjectionRateAt<T_>(d, i)`
>   dispatch builds the projection-capable subclasses directly from instrument
>   schedules — NOT through `Swap_::PrecomputeT<T_>()`. The OIS-discount slice
>   (`forecast == discount`) does NOT need projection and rides the inherited
>   `Swap_::PrecomputeT<T_>` machinery, `OISSwap_` included (`OISSwap_ : public
>   Swap_`, `ycinstrument.hpp:155`).
>
> Option B (`PIECEWISE_LINEAR_FWD` via `Tape::DiscountPWLF_<T_>` with a templated
> base handle) and the default `jacobianMode_` = `ANALYTIC` remain LOCKED from
> the first pass. This is an API NOTE — no implementation. Pair with the full
> design at `.codex/artifacts/designs/joint-aad-gradient.md`. This note locks the
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
    //               resolves + liborBasis_ == ACT_365F + vanilla Deposit/FRA/Future/Swap
    //               (OISSwap_ rides Swap_::PrecomputeT<T_> -- it inherits Swap_ and its
    //               overnight index forecasts off the discount curve, so forecast ==
    //               discount and the inherited templated rate is both routing-compatible
    //               and gradient-correct) + tradeDate == knot 0); otherwise falls back
    //               to BUMPED with a NOTICE (at most once per CalibrateJointMultiCurve
    //               call; never throws). The IBOR(3M) projection slice (forecast !=
    //               discount) is priced through a NEW Tape::JointRate_<T_> base (CP3)
    //               reading a JointCurveBlock_<T_> routing context; the OIS-discount
    //               slice (forecast == discount) rides the inherited
    //               Swap_::PrecomputeT<T_>. See .codex/artifacts/designs/joint-aad-gradient.md.
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

| Input violation                                                                                                                                                                                        | NOTICE text                                                                                                                                                                                                                                                                                                   |
|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `ANALYTIC` + a declaration's parameterization != PIECEWISE_LINEAR_FWD                                                                                                                                  | `"Joint AAD Jacobian requires CurveParameterization_::PIECEWISE_LINEAR_FWD on every declaration; declaration N has X; falling back to bumped"`                                                                                                                                                                |
| `ANALYTIC` + a declaration contains an out-of-scope instrument type (`BasisSwap_`/`STIR_` only — `OISSwap_` IS eligible: it inherits `Swap_` and its overnight index forecasts off the discount curve) | `"Joint AAD Jacobian has no templated rate for instrument 'name' (type) in declaration N; falling back to bumped"`                                                                                                                                                                                            |
| `ANALYTIC` + a declaration's `liborBasis_ != ACT_365F` (S7)                                                                                                                                            | `"Joint AAD Jacobian requires liborBasis_ == ACT_365F (the DAYS_PER_YEAR denominator the templated PWL-forward curve assumes); declaration N has X; falling back to bumped"`                                                                                                                                  |
| `ANALYTIC` + a discount-declaration instrument projects (`useProjectionCurve_ == true`)                                                                                                                | `"Joint AAD Jacobian requires discount-declaration instruments to forecast off the discount curve; instrument 'name' in declaration N projects, falling back to bumped"`                                                                                                                                      |
| `ANALYTIC` + a base-layered forward's base collateral is not an eligible discount declaration                                                                                                          | `"Joint AAD Jacobian requires base-layered forward declaration N's base collateral to be an eligible discount declaration; falling back to bumped"` (**defensive, unreachable** — the validator throws first on missing collateral; clause (c) fires first on non-PWL discount. Retained for symmetry — S10.) |

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
  `{Deposit_, FRA_, Future_, Swap_}`, which INCLUDES `OISSwap_` (`OISSwap_ :
  public Swap_`, `ycinstrument.hpp:155`). `OISSwap_` rides the inherited
  single-curve `Swap_::PrecomputeT<T_>`: its overnight index has
  `useProjectionCurve_ == false` (`ycinstrument.cpp:44`), so it forecasts off
  the discount curve (`forecast == discount == OIS`), and the inherited
  `Tape::SwapRate_<T_>::operator()` (`ycinstrument.cpp:347-368`) prices each
  float period with the same simple-rate `ForwardRate` arithmetic the double
  path uses (`ForwardRate`, `ycinstrument.cpp:53-59`) — so the JACOBIAN is
  correct (AAD and bumped evaluate the identical function; AC1 passes for OIS).
  `BasisSwap_` and `STIR_` are rejected (no Phase A templated rate). The IBOR(3M)
  projection slice — where `forecast(3M) != discount(OIS)` — is priced through a
  NEW `Tape::JointRate_<T_>` base (CP3, critique B4) reading a
  `JointCurveBlock_<T_>` routing context — NOT through Phase A's
  `Tape::Rate_<T_>` virtual (which is bound to `YCCtx_<T_>` and reads a single
  curve). The OIS-discount slice (`forecast == discount`) needs no projection and
  rides the inherited `Swap_::PrecomputeT<T_>`;
- (f) every forward-declaration instrument has `useProjectionCurve_ == true`
  (structurally guaranteed by `ValidateAndBuildSlots` at
  `dal-cpp/dal/curve/jointcalibration.cpp:225-231`);
- (g) every discount-declaration and baseless-forward-declaration instrument has
  `useProjectionCurve_ == false`;
- (h) every base-layered forward declaration's `targetCollateral_` is produced
  by a discount declaration in the same spec whose parameterization is also
  PIECEWISE_LINEAR_FWD (defensive / unreachable per S10);
- (i) every instrument's `TradeDate() == its declaration's knotDates_.front()`
  (which under PWL_FWD is `> today_` per the joint validator at
  `jointcalibration.cpp:203-204` — PWL_FWD has NO today-pinned anchor, so the
  rule is `tradeDate == knot 0`, NOT `tradeDate == today_`);
- (j) every declaration's `liborBasis_ == ACT_365F` (the joint default at
  `jointcalibration.hpp:60`). The templated `Tape::DiscountPWLF_<T_>` routes its
  forward-to-log-DF denominator through the `DAYS_PER_YEAR = 365.0` constant
  (`ycconst.cpp:16`); a spec whose `liborBasis_ != ACT_365F` is rejected (S7).

## Example

The example program at
`dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp`
needs NO instrument-type edit under Option B: its OIS slice stays `OISSwap_`,
which rides the inherited `Swap_::PrecomputeT<T_>` on the ANALYTIC path (its
overnight index forecasts off the discount curve, so `forecast == discount`).
Its declarations already inherit `PIECEWISE_LINEAR_FWD` from the
`JointCurveDeclaration_` default (`jointcalibration.hpp:47`) and its knot ladder
already starts strictly after `today_` (satisfying the validator). The full
instrument set — OIS deposits, OIS swaps (`OISSwap_`), FRAs, and vanilla swaps
— runs ANALYTIC natively; the OIS-discount slice rides `Swap_::PrecomputeT<T_>`
and the IBOR(3M) projection slice rides the new `Tape::JointRate_<T_>` (CP3).
The existing `tolerance_ = 1.0e-10` / `fitTolerance_ = 1.0e-10` edits are
PRESERVED (do not touch — spec hard constraint). (Restores the first-pass "needs
NO parameterization edit" claim — the second-pass CP4 OIS-slice switch is
retired; no example edit is required.)

Under the LOCKED default (`ANALYTIC`), the existing single-arg call exercises
the AAD path natively on the edited spec. The example may optionally add an
explicit A/B comparison via the two-arg overload for clarity:

~~~cpp
// dal-cpp/examples/joint_multi_curve_calibration/joint_multi_curve_calibration.cpp
// (illustrative excerpt -- under Option B the declarations stay
// PIECEWISE_LINEAR_FWD and the OIS slice stays OISSwap_ (it rides the inherited
// Swap_::PrecomputeT<T_> on the ANALYTIC path); under the LOCKED default the
// single-arg call already engages ANALYTIC. The two-arg overload is optional,
// for an explicit A/B.)

int main() {
    RegisterAll_::Init();
    const Date_ today(2024, 1, 15);
    const Ccy_ ccy("USD");
    const MarketSet_ market = BuildMarket(today, ccy);

    const JointMultiCurveCalibrationSpec_ jointSpec = BuildJointSpec(today, ccy, market);
    // BuildJointSpec: declarations inherit PIECEWISE_LINEAR_FWD, knots start
    // strictly after today_. The OIS-discount slice uses OISSwap_ (which rides
    // Swap_::PrecomputeT<T_> on the ANALYTIC path -- forecast == discount).

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
  surface is the same under either option; Option B avoids the validator fork
  that Option A would have required. Option B needs NO example instrument-type
  edit (the OIS slice stays `OISSwap_`).
- **OQ-B (design, RESOLVED).** `OISSwap_` coverage. First-pass resolution:
  "`OISSwap_` rides `Swap_::PrecomputeT<T_>()`". The second pass SUPERSEDED
  this with CP4 (reject `OISSwap_` on a geometric-compounding concern); the
  third pass RETIRES CP4 and RESTORES the first-pass resolution after the lead
  re-verified `ycinstrument.cpp`: the library has NO geometric overnight
  compounding — the AAD path (`Tape::SwapRate_<T_>::operator()`,
  `ycinstrument.cpp:347-368`) and the double path (`ForwardRate`,
  `ycinstrument.cpp:53-59`) use the IDENTICAL simple-rate arithmetic, and the
  OIS overnight index forecasts off the discount curve (`useProjectionCurve_ ==
  false`, `ycinstrument.cpp:44`), so `forecast == discount == OIS` and the
  inherited `Swap_::PrecomputeT<T_>` is both routing-compatible and
  gradient-correct. `OISSwap_` is ELIGIBLE for ANALYTIC; the example's OIS slice
  stays `OISSwap_`.
- **OQ-C (design, RESOLVED).** Joint default — `BUMPED` (the earlier draft's
  recommendation) or `ANALYTIC` (matching single-curve). **RESOLVED in favour
  of ANALYTIC by the user on 2026-06-20.** The default matches single-curve;
  the mitigation is the oracle test (spec AC1) and the four-backend build
  matrix (spec AC6).
- **OQ-D (RESOLVED — CP3 only; CP4 retired).** The second-pass re-critique's
  two blockers. **CP3 (B4, LOCKED):** introduce `Tape::JointRate_<T_>` +
  `ProjectionRateAt<T_>` (Phase A's `Tape::Rate_<T_>` untouched) for the
  IBOR(3M) projection slice. **CP4 (B5, RETIRED this pass):** the `OISSwap_`
  rejection rested on a geometric-compounding concern that does not exist in
  this codebase; it is dropped, and with it the `Tape::OisSwapRate_<T_>` Phase
  B+1 deferral. See design §4 (CP3), §2.4 (instrument coverage).

## Hand-off

- **API note path (absolute):**
  `.codex/artifacts/api-notes/joint-aad-gradient.md`
- **Design doc path (absolute):**
  `.codex/artifacts/designs/joint-aad-gradient.md`
- **Blocking dependencies:** NONE — Option B is LOCKED; OQ-1 (parameterization)
  is RESOLVED; OQ-B (`OISSwap_`) is RESOLVED (`OISSwap_` eligible, rides
  `Swap_::PrecomputeT<T_>`); OQ-C (default) is LOCKED to ANALYTIC; OQ-D is
  RESOLVED (CP3 LOCKED for the IBOR projection slice; CP4 RETIRED this pass).
- **No scope-change flag this pass.** Phase B's ANALYTIC path covers the full
  vanilla instrument set `{Deposit, FRA, Future, Swap}` INCLUDING `OISSwap_`.
  The second-pass CP4 scope reduction (reject `OISSwap_`, switch the example to
  vanilla `Swap_`, defer `Tape::OisSwapRate_<T_>`) is RETIRED — it rested on a
  geometric-compounding concern that the lead verified does not exist in
  `ycinstrument.cpp` (AAD and double paths share identical simple-rate
  arithmetic; `ycinstrument.cpp:347-368` vs `:53-59`). The example's OIS slice
  stays `OISSwap_` and runs ANALYTIC. No user objection is required.
- **Surface is LOCKED.** Next agent: `dal-critic` for an adversarial pass on
  the spec + design + this note, stressing the Option-B risk surface (four-branch
  `IntegralTo` on `T_`, templated base handle across four backends,
  2-params/knot column map, the new `JointRate_<T_>` dispatch — design §14) and
  the OIS-rides-`Swap_::PrecomputeT<T_>` eligibility; then `dal-implementer`.
