# P1 Staged XCCY Sensitivity Implementation Plan

## Baseline and scope

- Base: `master` at `1d90f66036bbc84a322eb26ffbca2bbe1abba89b`.
- Deliver only the reviewed P1 implementation across core, public C++, Python, and Excel.
- Preserve staged matrix ownership in `CrossCurrencyCalibrationDiagnostics_`, joint matrix ownership on `JointXccyCalibrationResult_`, all existing axes and solver scaling, and every legacy one-argument/default entry point.
- Defer current-state documentation prose to the later doc-writer stage. Excel Machinist function metadata changes with the implementation because it defines the worksheet contract.

## Design

1. Extend staged diagnostics in place with:
   - `parameterKnotDates_` in `spec.knotDates_` order;
   - `residualTolerance_`;
   - `jacobianScaling_ = "unscaled"`;
   - `effJacobianInverseScaling_ = "solver_scaled"`;
   - independent `jacobianAvailability_` and `effJacobianInverseAvailability_` values from `available`, `not_requested`, and `not_available_for_mode`.
2. Derive availability from the requested compute flag first, then solve/matrix mode. Keep empty matrices as the numeric carrier for unavailable data; never infer the reason from emptiness and never recompute in a getter.
3. Validate every market quote, positive solver scalar, initial guess, residual, and returned matrix entry as finite before exposing the result. Preserve the existing rejection rules for empty instruments and non-increasing knots.
4. Add public facade overload/accessors:
   - `CalibrateXccyMarket(spec, options)`;
   - staged diagnostics/Jacobian/effective-inverse read-only accessors;
   - `JointXccyResultEffJacobianInverse`.
5. Bind staged options and diagnostics aliases in Python. Keep matrices under `result.diagnostics` and reserve `jacobian_at_solution` for joint results.
6. Parse staged Excel `jacobianMode`, `computeForwardJacobian`, and `computeEffJacobianInverse` settings with strict types for these new keys only. Add staged selectors for matrices, axes, availability, tolerance, and scaling, plus the joint effective-inverse selector.

The effective inverse remains the solver-scaled pseudoinverse. A raw decimal quote bump is mapped only as:

```text
dx = eff_jacobian_inverse * dq / residual_tolerance
```

Instrument names are labels and may repeat; integer row order is authoritative. Parameter columns follow the PWC knot/right-forward order.

## TDD stages

1. Core RED:
   - add focused tests for axes, shapes, duplicate labels, metadata, the full availability truth table, default-overload parity, finite-input rejection, central-difference parity, and scaled inverse re-solve predictions;
   - include small-bump negative scaling evidence and fixed 5/10/16-instrument 1bp ladders.
2. Core GREEN:
   - implement diagnostics metadata, availability derivation, and finite checks with the minimum calibration changes;
   - rerun the focused core filter and refactor only while green.
3. Public/Python RED then GREEN:
   - add facade overload/accessor tests and Python overload/legacy/snake-case cross-surface tests;
   - implement facade and bindings without moving diagnostics data.
4. Excel RED then GREEN:
   - add portable Excel tests for strict new-setting types, settings propagation, all staged selectors, joint inverse, and unknown-key/view messages;
   - implement adapters, update Machinist markup, regenerate checked-in public stubs, and rerun the focused Excel target.
5. Verification:
   - run formatting and generated-file drift checks;
   - run targeted C++, public, Python, and portable Excel tests;
   - run `bash ./build_linux.sh` and `ctest --test-dir build/Release-linux --output-on-failure`;
   - record Windows Excel and sanitizer/AAD-backend coverage as CI-only residual risk unless available locally.

## Delivery

- Use branch `feature/dal-6-staged-xccy-sensitivity`.
- Commit one focused implementation unit with an imperative subject.
- Push and open a `master` PR whose title or branch routes to `DAL-6` without close intent.
- Report the exact branch, commit, PR, RED/GREEN commands and outcomes, full local verification, and remaining platform/CI risks.
