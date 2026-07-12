# ZERO_RATE Curve Parameterization Implementation Plan

> **For Codex:** Execute this plan task by task with the DAL implementer/tester/reviewer
> pipeline. Add the listed failing tests before each production change. Preserve unrelated
> behavior and commit each coherent green slice.

**Goal:** Implement a persistent continuously compounded ZERO_RATE curve representation,
using mapped log-DF ordinates and the shared interpolation framework, across core single and
joint calibration, AAD, base layering, archive, public C++, Python, Excel, and documentation.

**Architecture:** `Tape::DiscountZeroRate_<T_, B_>` stores an anchor, future dates, and typed
zero-rate ordinates. It maps node `z_i` to `logDF_i=-z_i*YearFrac(anchor,node_i)` and evaluates
those ordinates through `LogDfInterpolation_`. `CurveDefinition_` owns the internal anchor and
the central typed factory constructs passive, active, and active-base forms. A distinct
`DiscountZeroRate_v1` archive preserves zero-rate bump coordinates.

**Technology:** C++17, Google Test, DAL AAD backends, Machinist code generation, pybind11,
Python pytest, Excel generated wrappers, CMake/CTest.

**Controlling design:**
`docs/superpowers/specs/2026-07-12-zero-rate-parameterization-design.md`

---

## Task 1: Persistent ZERO_RATE Curve Kernel

**Files:**

- Create: `dal-cpp/dal/curve/yczerorate.hpp`
- Create: `dal-cpp/dal/curve/yczerorate.cpp`
- Create: `dal-cpp/tests/curve/test_zerorate.cpp`
- Reference: `dal-cpp/dal/curve/yclogdf.hpp`
- Reference: `dal-cpp/dal/curve/yclogdf.cpp`
- Reference: `dal-cpp/dal/curve/logdfinterp.hpp`

### Step 1: Add failing construction and valuation tests

Create `test_zerorate.cpp` with focused helpers for anchor, future knots, mapped log-DFs,
and an equivalent `NewDiscountLogDF` oracle.

Add failing tests for:

- one future LOG_LINEAR node and exact anchor/node discount factors
- ACT/365F and ACT/360 mapping
- zero and negative finite rates
- arbitrary `from/to`, same-date identity, and inverse identity
- all three `LogDfScheme_` values against the equivalent log-DF oracle at nodes, interior
  dates, a date before the anchor, and a date beyond the last node
- minimum future-node counts per scheme
- explicit ACT/365L rejection when no day-count context is available, plus monotonic
  ACT/ACT or BOND geometry
- empty arrays, mismatched arrays, non-monotonic dates, anchor-or-earlier dates, NaN/Inf,
  and invalid year-fraction geometry

Run the focused test build and record the expected compile failure because
`yczerorate.hpp` does not exist.

### Step 2: Add the templated curve declaration

Declare `Tape::DiscountZeroRate_<T_, B_>` deriving from
`CurveWithBase_<DiscountCurve_<T_>, B_>` and `FittableCurve_`.

Store:

- `anchorDate_`
- future-only `nodeDates_`
- `dayCount_`
- internal year fractions `[0,t_1,...,t_N]`
- typed future `zeroRates_`
- `scheme_`
- `std::unique_ptr<LogDfInterpolation_>`

Declare `LogDfAt`, `operator()`, `NX`, `ApplyDX`, `Write`, `Clone`, and accessors
`AnchorDate`, `NodeDates`, `NodeZeroRates`, `DayCount`, and `Scheme`.

Define `Write` in this slice as a clear unsupported-serialization failure so explicit
template instantiation links cleanly. Task 5 adds archive tests first, generates v1, and
replaces this temporary behavior with the canonical writer.

Add the passive alias and core `NewDiscountZeroRate` factory with explicit day count and
scheme plus optional base.

### Step 3: Implement validation and mapped interpolation

In the constructor:

- require at least one future node
- require date/rate size equality and strictly increasing future dates
- require every date after the anchor
- compute context-free anchor year fractions and require finite, positive, monotonic values
- validate finite rates only for `T_=double`
- build interpolation geometry on `[0,t_1,...,t_N]`

Implement `LogDfAt` by obtaining shared `LogDfInterpolation_::WeightsAt(yf)` and applying
the weights to an implicit mapped vector whose index zero is exactly zero and whose future
entry `i` is `-zeroRates_[i-1]*yearFractions_[i]`. Do not allocate or extract active values
through `double`.

Implement valuation with the same passive/active `exp` and multiplicative base pattern as
`DiscountLogDF_`.

### Step 4: Implement fittable and clone behavior

- `NX()` returns `zeroRates_.size()`.
- `ApplyDX` bumps `zeroRates_` directly in future-node order.
- `NodeZeroRates` uses `AAD::Value` only for read-only passive diagnostics.
- `Clone` preserves fields and applies normal base substitutions.

Add tests proving `ApplyDX` uses zero-rate, not log-DF, coordinates and an inverse bump
restores the curve.

### Step 5: Add direct active/base tests

Instantiate and test:

- `DiscountZeroRate_<double>` with and without a passive base
- `DiscountZeroRate_<AAD::Number_>` with a passive base
- `DiscountZeroRate_<AAD::Number_, DiscountCurve_<AAD::Number_>>` with an active base

Verify node derivative `d logDF_i/dz_i=-t_i`, off-node adjoints against central differences,
and non-zero sensitivities to both ZERO_RATE and active-base parameters.

### Step 6: Run and commit the green kernel slice

Run:

```bash
cmake --build build/Release-linux --target dal_cpp_tests -j$(nproc)
build/Release-linux/dal-cpp/dal_cpp_tests --gtest_filter='*ZeroRate*'
```

Then format only the touched C++ files, rerun the focused test, inspect `git diff --check`,
and commit:

```text
feat(curve): add persistent zero-rate curve
```

---

## Task 2: Central Definition, Layout, and Typed Factory

**Files:**

- Modify: `dal-cpp/dal/curve/curveparameterization.hpp`
- Modify: `dal-cpp/dal/curve/curveparameterization.cpp`
- Modify: `dal-cpp/tests/curve/test_curveparameterization.cpp`
- Modify: `dal-cpp/dal/curve/logdfscheme.hpp`
- Regenerate: affected core and Excel `MG_LogDfScheme_*` enum/help artifacts

### Step 1: Add failing definition/layout tests

Add tests asserting that N future ZERO_RATE knots:

- are accepted only when strictly after the anchor
- produce internal dates `[anchor, declared...]` exactly once
- retain the supplied anchor and day count in `CurveDefinition_`
- produce layout `{N+1,N,1,true}`
- preserve declared future-knot parameter order

Retain the existing LOG_DISCOUNT/PWC/PWL expectations verbatim.

### Step 2: Add failing passive typed-factory tests

For every log-DF scheme, construct the same ZERO_RATE curve through
`BuildDiscountCurveT<double>` and `NewDiscountZeroRate`. Assert matching dynamic type,
node/interior/tail values, layout count validation, and passive base behavior.

### Step 3: Add failing AAD typed-factory tests

Use `RegisterCurveParameters` and both active factory specializations. Assert primal parity,
the `-t_i` node chain factor, interpolation-weight propagation, and active-base derivatives.

### Step 4: Implement central support

- Add `anchorDate_` to `CurveDefinition_` without changing existing semantics.
- ZERO_RATE `MakeCurveDefinition` validates future-only knots and prepends anchor.
- ZERO_RATE layout returns `{storageNodes,storageNodes-1,1,true}`.
- Include `yczerorate.hpp` and dispatch all three typed instantiations to
  `DiscountZeroRate_<T_,B_>`, passing future internal dates (`nodeDates_[1:]`), active
  parameters, anchor, day count, scheme, and base.
- Replace only ZERO_RATE's “unimplemented” branches.

Update `LogDfScheme_` markup text to describe mapped log-DF interpolation for LOG_DISCOUNT
and ZERO_RATE without changing enum values or ordinals.

Run `dal_generate` and `dal_check_generated` in this task so the markup change and generated
outputs remain in the same green commit.

### Step 5: Verify and commit

Run:

```bash
cmake --build build/Release-linux --target dal_generate dal_check_generated -j$(nproc)
cmake --build build/Release-linux --target dal_cpp_tests -j$(nproc)
build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='CurveParameterizationTest.*:*ZeroRate*'
```

Format, run `git diff --check`, and commit:

```text
feat(curve): wire zero-rate typed factory
```

---

## Task 3: Single Calibration and Analytical Jacobian

**Files:**

- Modify: `dal-cpp/dal/curve/calibration.cpp`
- Modify: `dal-cpp/tests/curve/test_calibration.cpp`
- Modify: `dal-cpp/tests/curve/test_analytic_jacobian.cpp`
- Modify: `dal-cpp/tests/curve/test_curveparameterization.cpp` if shared layout helpers
  need calibration-specific coverage

### Step 1: Replace the rejection test with failing acceptance tests

Replace the existing ZERO_RATE “unimplemented” expectation with tests that:

- accept future-only ZERO_RATE knots
- reject a declared today/anchor knot with a ZERO_RATE-specific message
- validate scalar and per-node zero-rate guesses
- reject per-node guess lengths different from the layout parameter count
- preserve current LOG_DISCOUNT anchor validation

### Step 2: Add failing calibration behavior tests

Add deterministic ZERO_RATE cases for:

- EXACT calibration and repricing
- APPROXIMATE calibration and positive finite DFs
- staged/multi-curve use through the existing single-stage delegate
- scalar versus per-node starting guesses
- a passive base curve, interpreting calibrated values as spreads

Assert the returned dynamic type is `DiscountZeroRate_` and its stored rates are in future
knot order.

### Step 3: Add failing analytical Jacobian tests

Loop `LOG_LINEAR`, `LOG_CUBIC_NATURAL`, and `MIXED` with enough future knots. For each:

- solve with ANALYTIC and BUMPED modes
- compare solved curves and residuals
- compare the at-solution analytical Jacobian against central residual bumps in zero-rate
  coordinates
- validate effective inverse-Jacobian dimensions and column order
- confirm disabled diagnostics remain empty without changing convergence

Use existing suite tolerances unless measured numerical evidence requires a narrowly
documented adjustment.

### Step 4: Generalize validation, guesses, and smoothing dates

- Keep LOG_DISCOUNT as the only single-curve input that requires an explicit today knot.
- Build `CurveDefinition_` and `CurveParameterLayout_` before validating per-node length.
- Treat a supplied `initialGuessPerNode_` as an exact parameter vector for every supported
  representation; test existing forward behavior so the bug fix is intentional.
- Preserve LOG_DISCOUNT's default 2% transformed log-DF seed.
- Fill ZERO_RATE and forward representations from scalar `initialGuess_` when no per-node
  values are supplied.
- Derive smoothing/free-node dates from `definition.nodeDates_`, skipping its internal
  anchor when `pinnedAnchor_`, rather than dropping an entry from raw `knotDates`.
- Remove only the explicit ZERO_RATE analytical-eligibility rejection.

### Step 5: Verify and commit

Run:

```bash
cmake --build build/Release-linux --target dal_cpp_tests -j$(nproc)
build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='*Calibration*ZeroRate*:AnalyticJacobianTest.*ZeroRate*:*CurveParameterization*'
```

Then run all core curve calibration tests, format, inspect `git diff --check`, and commit:

```text
feat(curve): calibrate zero-rate curves with aad
```

---

## Task 4: Joint and Mixed Calibration

**Files:**

- Modify: `dal-cpp/dal/curve/jointcalibration.cpp`
- Modify: `dal-cpp/tests/curve/test_joint_calibration.cpp`
- Modify: `dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp`

### Step 1: Add failing homogeneous ZERO_RATE joint tests

Add a joint discount/forward calibration whose declarations both use ZERO_RATE. Verify
layout offsets, convergence, repricing, stored zero-rate coordinates, and declaration/knot
column order.

### Step 2: Add failing heterogeneous and base-layered tests

Cover at least:

- ZERO_RATE discount plus PWC/PWL or LOG_DISCOUNT forward
- non-ZERO discount plus base-layered ZERO_RATE forward

Assert cross-block discount sensitivities are non-zero where the active base is consumed.

### Step 3: Add failing joint Jacobian comparisons

For the homogeneous and heterogeneous cases, compare:

- analytical residual Jacobian versus central zero-rate/other-coordinate bumps
- analytical versus bumped solved parameters, DFs, and residuals
- block dimensions and column ordering

Retain the existing joint ACT/365F, instrument, projection, and routing eligibility gates.

### Step 4: Enable ZERO_RATE and verify

Remove only the explicit ZERO_RATE rejection in joint analytical eligibility. The central
typed factory should supply the remaining behavior without a parallel implementation.

Run:

```bash
cmake --build build/Release-linux --target dal_cpp_tests -j$(nproc)
build/Release-linux/dal-cpp/dal_cpp_tests \
  --gtest_filter='Joint*ZeroRate*:JointAnalyticJacobianTest.*ZeroRate*'
```

Then run the full joint suites, format, inspect `git diff --check`, and commit:

```text
feat(curve): support zero-rate joint calibration
```

---

## Task 5: Archive and Clone Persistence

**Files:**

- Modify: `dal-cpp/dal/curve/yczerorate.cpp`
- Generate: `dal-cpp/dal/auto/MG_DiscountZeroRate_object.hpp`
- Generate: `dal-cpp/dal/auto/MG_DiscountZeroRate_v1_Read.inc`
- Generate: `dal-cpp/dal/auto/MG_DiscountZeroRate_v1_Write.inc`
- Modify: `dal-cpp/tests/curve/test_ptirds_curve.cpp` or create a focused archive test file

### Step 1: Add failing archive and clone tests

Test clone and archive round-trips:

- without a base for every interpolation scheme
- with a serializable PWLF or log-DF base

Assert preserved dynamic type, name/currency, anchor, future dates, zero rates, day count,
scheme, base effect, `NX`, `ApplyDX`, node/interior/pre-anchor/tail values, and base
substitution on clone.

### Step 2: Add the v1 storable and generated artifacts

Add manual Machinist markup `storable DiscountZeroRate version 1` with name, ccy,
anchorDate, nodeDates, zeroRates, dayCount, scheme, and optional DiscountCurve base.

Run the normal core generation target. Do not hand-edit generated outputs.

### Step 3: Implement write/read

- Serialize only `<double, DiscountCurve_<double>>`.
- Convert stored typed rates with `AAD::Value` only in the passive writer.
- Reader `Build` reconstructs `DiscountZeroRate_` and validates the canonical fields.
- Keep all existing archive schemas unchanged.

### Step 4: Verify and commit

Run focused archive tests, the complete curve archive suite, and the generated-file drift
check. Format handwritten files only, inspect `git diff --check`, and commit:

```text
feat(curve): archive zero-rate curves
```

---

## Task 6: Public C++, Python, and Excel Surfaces

**Files:**

- Modify: `dal-public/src/curvedata.hpp`
- Modify: `dal-public/tests/test_curvedata.cpp` or nearest curve-data test
- Modify: `dal-public/tests/test_curvespec.cpp`
- Modify: `dal-python/src/bindings/curve.cpp`
- Modify: `dal-python/src/dal/api.py`
- Modify: `dal-python/tests/test_curve_protocol.py`
- Modify: `dal-python/tests/test_curve_calibration.py`
- Modify: `dal-excel/src/__curvedata.cpp`
- Modify: `dal-excel/src/__curvespec.cpp`
- Generate: `dal-excel/auto/MG_DiscountZeroRate_New_public.inc`
- Generate: `dal-excel/auto/MG_DiscountZeroRate_New_public.htm`
- Regenerate affected enum/help artifacts after the `LogDfScheme_` description change

### Step 1: Add failing public C++ tests

Test `DiscountZeroRateNew` with defaults, explicit ACT/360 and each scheme, optional base,
and single calibration selected through the public builder. Assert numerical parity with
the core factory and persistent type.

### Step 2: Implement the public wrapper

Add the required includes and `DiscountZeroRateNew` wrapper with user-facing defaults
`ACT_365F`, `LOG_LINEAR`, and empty base.

### Step 3: Add failing Python tests

Test:

- `DiscountZeroRate_New` construction with defaults and explicit options
- optional base multiplication
- ZERO_RATE single calibration and analytical diagnostics
- high-level `calibrate_curve` with an optional base curve
- invalid anchor/date/rate inputs translated to Python exceptions

### Step 4: Implement Python binding and high-level base support

Register curve enums before binding any default enum argument. Bind the direct factory with
Python names `anchor_date`, `node_dates`, `zero_rates`, `day_count`, `log_df_scheme`, and
`base`. Extend the existing high-level helper/settings path with optional `base_curve`
without changing existing positional behavior.

### Step 5: Add Excel markup and implementation

Add `DISCOUNTZERORATE.NEW` with required name/ccy/anchor/future dates/rates and optional
day count/scheme/base. Add an optional trailing calibration base curve to
`CALIBRATE.SINGLECURVE`, distinct from its pricing `discountCurve` input, and assign it to
`builder.baseCurve_`.

Run both Machinist passes and include generated files. Do not conflate the base curve with
the OIS pricing curve used for forward calibration.

### Step 6: Verify and commit

Run:

```bash
cmake --preset=full-dev
cmake --build build/full-dev --target dal_public_tests _dal -j$(nproc)
build/full-dev/dal-public/dal_public_tests --gtest_filter='*ZeroRate*'
ctest --test-dir build/full-dev --output-on-failure -R dal_python_pytest
```

Run `dal_generate` and `dal_check_generated`, plus the available Excel build contract
checks. Format handwritten files with include sorting preserved where required, inspect
generated drift and `git diff --check`, then commit:

```text
feat(api): expose zero-rate curves
```

---

## Task 7: Documentation and Full Regression

**Files:**

- Modify: `docs/methodology/yield_curve.md`
- Modify: `docs/methodology/yield_curve_jacobian.md`
- Modify: `docs/public-api.md`
- Modify: `dal-python/README.md`
- Modify: `CHANGELOG.md`
- Review: `docs/experimental/replicate-ptirds-single-currency-curve.md`
- Review: `docs/superpowers/plans/2026-07-12-unified-yield-curve-interpolation-aad.md`

### Step 1: Reconcile current-state documentation

Remove current claims that ZERO_RATE is unsupported. Document:

- continuous compounding and `logDF=-z*t`
- future-only knots and absent t=0 parameter
- configured day count
- all shared interpolation/extrapolation schemes
- base spread semantics
- scalar/per-node guess units
- `ApplyDX` and analytical Jacobian column units/order
- direct core/public/Python/Excel factories
- single/joint support boundaries and retained fallback gates

Keep historical planning documents historical unless they are presented as current API
documentation. Add a changelog entry for the new numerical capability.

### Step 2: Run documentation checks

Run:

```bash
python3 .github/scripts/check_docs.py
rg -n 'ZERO_RATE.*(unimplemented|unsupported)|unimplemented.*ZERO_RATE' \
  README.md docs dal-python/README.md CHANGELOG.md
```

Classify any remaining matches as intentionally historical or fix current-state text.

### Step 3: Run full verification

From a clean build directory tied to this isolated branch, run:

```bash
cmake --preset=Release-linux
cmake --build build/Release-linux -j$(nproc)
(cd build/Release-linux && ctest --output-on-failure)
cmake --preset=full-dev
cmake --build build/full-dev -j$(nproc)
ctest --test-dir build/full-dev --output-on-failure -R dal_python_pytest
python3 .github/scripts/check_docs.py
```

Run the targeted core suite under the explicit Linux AAD matrix:

```bash
for backend in aadet xad codipack adept; do
  case "$backend" in
    aadet) flags='-DDAL_USE_XAD_AAD=OFF -DDAL_USE_CODIPACK_AAD=OFF -DDAL_USE_ADEPT_AAD=OFF' ;;
    xad) flags='-DDAL_USE_XAD_AAD=ON -DDAL_USE_CODIPACK_AAD=OFF -DDAL_USE_ADEPT_AAD=OFF' ;;
    codipack) flags='-DDAL_USE_XAD_AAD=OFF -DDAL_USE_CODIPACK_AAD=ON -DDAL_USE_ADEPT_AAD=OFF' ;;
    adept) flags='-DDAL_USE_XAD_AAD=OFF -DDAL_USE_CODIPACK_AAD=OFF -DDAL_USE_ADEPT_AAD=ON' ;;
  esac
  cmake -S . -B "build/aad-$backend" -DCMAKE_BUILD_TYPE=Release \
    -DDAL_CPP_BUILD_TESTS=ON -DDAL_PUBLIC_BUILD_TESTS=OFF \
    -DDAL_CPP_BUILD_EXAMPLES=OFF -DDAL_BUILD_PUBLIC=OFF $flags
  cmake --build "build/aad-$backend" --target dal_cpp_tests -j$(nproc)
  "build/aad-$backend/dal-cpp/dal_cpp_tests" \
    --gtest_filter='*ZeroRate*:AnalyticJacobianTest.*ZeroRate*:JointAnalyticJacobianTest.*ZeroRate*'
done
```

Run Machinist drift checks and `git diff --check` after the matrix.

### Step 4: Simplification and review gates

Run DAL simplifier and reviewer passes over the complete branch. Resolve:

- duplicated log-DF mapping or interpolation logic
- unsafe anchor/date/rate indexing
- accidental active-to-double conversions
- generated-file hand edits or omissions
- public API/default inconsistencies
- missing error, base, archive, Python, or mixed-joint tests
- stale unsupported statements

Rerun all impacted checks after every review fix.

### Step 5: Commit final docs/review cleanup

Commit the green final slice:

```text
docs: document zero-rate curve support
```

---

## Task 8: Push, Pull Request, and Issue Closure

**Files:** none unless review or CI requires fixes.

### Step 1: Completion audit

Audit every requirement in the controlling design against current source, tests, generated
artifacts, docs, and command output. Treat missing direct evidence as incomplete.

### Step 2: Push and open the PR

Push `codex/zero-rate-parameterization` and open a PR describing:

- persistent representation and archive identity
- mapped log-DF shared interpolation semantics
- single/joint/AAD/base support
- public C++/Python/Excel exposure
- verification performed

### Step 3: Resolve PR feedback and CI

Inspect all review threads, requested changes, checks, and exact-head GitHub Actions results.
For each actionable item:

- reproduce or verify the concern
- add or adjust a failing test when behavior changes
- implement the narrow fix
- rerun impacted and regression checks
- commit and push
- resolve the corresponding review thread

Do not conclude from stale earlier workflow runs; verify the final pushed SHA.

### Step 4: Final merge-readiness audit

Confirm:

- all required checks on the final SHA are green
- no unresolved actionable review threads or requested changes remain
- generated files are current
- the PR diff contains no unrelated work
- the branch is mergeable and the PR accurately documents residual limitations

Only then report the PR ready for merge.
