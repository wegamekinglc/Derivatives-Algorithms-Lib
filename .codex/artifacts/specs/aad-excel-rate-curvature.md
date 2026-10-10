# Excel native rate-trade quote curvature

Status: active implementation after merged #536. Source contracts are
`dal-public/src/ratecurvature.hpp`, the native rate-trade methodology and the
existing Excel calibration/trade handles. No native numerical change is planned.

## Problem and goal

Excel can calibrate rates and price native trades but cannot invoke the accepted
full-recalibration quote-gradient secants. Add owning worksheet snapshots,
portfolio settings and results, using the common finite-step request from #536.
Generic equation callbacks remain C++ only. This is a finite-step estimate;
native `higherOrder_` remains false.

## Requirements

1. `RateCalibration_New` accepts the existing single-curve, joint multi-curve,
   staged-XCCY and joint-XCCY calibration **result** handles. It captures their
   retained final specification through the matching native `NewRateCalibration`.
   This performs a new solve with native default options; old solved parameters
   and options are not replayed. EXACT, square-system and native dependency
   admission remain native requirements. Other handles, including legacy staged
   multi-curve results without a retained full specification, reject explicitly.
2. Snapshots expose copied point/parameters and an existing calibration-risk
   plan for complete quote metadata. `RateCalibration_Recalibrate` accepts one
   finite raw quote per axis coordinate and preserves native axis identity.
3. Optional portfolio settings copy a finite row/column weight vector and a
   fixing snapshot. Blank weights mean unit weights; signed and zero weights
   remain valid. A present fixing wrapper must contain a nonnull snapshot;
   the settings own a copy. The fixing query returns a detached three-column
   record table: first row `explicit_snapshot`, Boolean presence, blank; remaining
   rows index, fixing time and value. This preserves absent versus explicit empty
   history without an invalid null Excel output handle.
   Trade-count, history agreement and actual-PV-currency
   admission remain delegated to the native closed portfolio evaluator.
4. `RateTradeQuoteCurvatureResult_New` takes a nonempty vector of typed native
   trade handles, a snapshot, a common bump request and optional settings.
   Reject null/wrong trade rows with their one-based row. Evaluate exactly once
   through `EvaluateRateTradeQuoteCurvature`; validate every trade even at zero
   weight. Preserve duplicate rows and signed portfolio weights.
5. Results expose value, actual PV currency, point, full raw gradient,
   directions, steps, Hessian products, logical shape, base snapshot and all
   seven native execution fields. Metadata is available from the base snapshot.
   Empty products retain 0-by-Q internally and spill one blank cell; shape
   remains authoritative. Getters copy data and remain passive. Rate quote
   metadata retains the existing blank value column; query Point for values.
6. Accept only actual finite numeric cells for quote/weight inputs. Raw XLL
   guards reject bool/text/date/blank/error/nonfinite elements before coercion,
   normalize Excel integers, and identify the function, field and location.
   Only a wholly blank optional weights range means absent weights. Names
   reject embedded NUL before conversion. Numeric vectors must be row/column.
7. All output replacement is atomic on input, solve, preparation and budget
   failure. Active caller recordings reject at native financial entry points;
   passive constructors/getters preserve graph, seeds and adjoint mode. New
   handles reject archive serialization explicitly.
8. Preserve the common numeric budget, `8*(1+2Q+2MQ+M)` bytes, and supported
   caller-thread recording-cap semantics. Unlike Dupire, a provided recording
   cap is admitted and enforced by the native rate evaluator. Numeric budgets
   exclude solver, preparation, handle and cell storage. Report native peak
   capacity and cleanup reserve separately, without an RSS claim.

## Acceptance

- Focused RED/GREEN typed tests cover all four factory routes, detached input
  ownership, replay, complete axes, settings, signed analytic deposit Gamma,
  multiple/empty directions, separate caps, invalid rows, atomic recovery and
  caller-state preservation. Existing native tests are reused by provenance;
  do not repeat their full financial matrix for a worksheet adapter.
- Windows raw tests execute every new export, strict cell/NUL/handle admission,
  integer normalization, blank weights, empty products and budget failures.
  Registration checks cover exact names, argument order/types and help.
- Official Machinist generation/drift, actual CMake binding boundary, strict
  OFF/combined compilation and documentation checks pass.
- Select two complete rate requests at small/large quote counts. Retain
  calibrated paired observations and unchanged native/public archive identity;
  native-versus-owning-Excel costs are informational.
- Inspect exact-head CI, Codacy, complete reviews and actual runtime logs;
  audit twice, merge by SHA guard and verify the accepted tree before proceeding.

Open questions: none. No new solver, callback binding, dense Hessian, currency
conversion or unsupported calibration family is introduced.
