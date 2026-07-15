# Unified Yield-Curve Interpolation and AAD Jacobian Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every implemented yield-curve parameterization one scalar-templated evaluation path and make its AAD-derived analytic calibration Jacobian available in single and joint calibration.

**Architecture:** Separate passive interpolation geometry from scalar-valued curve state. Shared weight and integration kernels operate on `double` knot locations and either `double` or `Dal::AAD::Number_` ordinates; one internal curve definition/layout and typed factory then serves both calibration engines. Existing factories, enums, parameter order, serialization, and bumped-Jacobian fallback remain compatible.

**Tech Stack:** C++17, DAL `Vector_`/`Matrix_`/`Handle_`, DAL AAD facade with native/Adept/XAD/CoDiPack backends, Google Test, CMake.

## Global Constraints

- Cover `PIECEWISE_CONSTANT_FWD`, `PIECEWISE_LINEAR_FWD`, and `LOG_DISCOUNT` with `LOG_LINEAR`, `LOG_CUBIC_NATURAL`, and `MIXED`.
- Keep reserved, unimplemented `ZERO_RATE` out of scope and preserve its current rejection.
- Keep knot dates, year fractions, segment choices, boundaries, and mixed cutoffs passive; derivatives are with respect to stored curve ordinates only.
- Preserve `CurveCalibrationSpec_`, `JointMultiCurveCalibrationSpec_`, `CurveCalibrationOptions_`, public factories, enum spellings, bindings, diagnostic matrix layouts, and archive wire formats.
- Preserve existing numerical behavior, including PWLF tight-parity checks and log-DF secant extrapolation; do not accept tolerance drift until an existing test proves that tolerance is already the contract.
- Preserve solver column order: log-DF excludes its pinned anchor, PWC has one column per declared knot, and PWL interleaves left/right columns per declared knot.
- `ANALYTIC` remains best-effort. Unsupported instruments and configurations continue to return `nullptr` from `Gradient` and use the bumped solver path.
- Follow `.clang-format`, `.codex/skills/dal-agent-team/references/shared-rules.md`, and the existing AAD register/record/propagate/zero ordering for every backend.

---

## Target File Structure

- `dal-cpp/dal/math/interp/interpweights.hpp/.cpp`: passive interpolation geometry and scalar-templated weighted evaluation for linear and natural-cubic interpolation.
- `dal-cpp/dal/curve/logdfinterp.hpp/.cpp`: curve-specific log-DF scheme dispatch, mixed cutoff handling, and secant extrapolation policy.
- `dal-cpp/dal/curve/ycconst.hpp/.cpp`: public factory plus `Tape::DiscountPWC_<T_, B_>`.
- `dal-cpp/dal/curve/ycpwlf.hpp/.cpp`: existing typed PWL curve, retaining its compatibility-preserving double delegation.
- `dal-cpp/dal/curve/yclogdf.hpp/.cpp`: typed log-DF curve with typed or passive base and the shared log-DF interpolation policy.
- `dal-cpp/dal/curve/curveparameterization.hpp/.cpp`: complete node dates, solver layout, flat parameter registration, and the common typed curve factory.
- `dal-cpp/dal/curve/aadjacobian.hpp/.cpp`: backend-neutral reverse-sweep harvesting shared by single and joint calibration.
- Existing interpolation and curve test files remain focused; add `test_interpweights.cpp`, `test_logdfinterp.cpp`, and `test_curveparameterization.cpp` for the new internal contracts.

### Task 1: Add Passive Interpolation Geometry and Typed Evaluation

**Files:**
- Create: `dal-cpp/dal/math/interp/interpweights.hpp`
- Create: `dal-cpp/dal/math/interp/interpweights.cpp`
- Create: `dal-cpp/tests/math/interp/test_interpweights.cpp`

**Interfaces:**
- Produces `InterpWeights_`, `ApplyInterpWeights<T_>`, `LinearWeightGeometry_`, and `NaturalCubicWeightGeometry_`.
- Abscissae and returned weights are passive `double`; ordinates and results use `T_`.
- `NaturalCubicWeightGeometry_` supports natural boundaries only because that is the curve contract; general `Interp::NewCubic` boundary orders remain unchanged.

- [ ] **Step 1: Write failing linear-weight tests**

Add tests that pin knot reproduction, interpolation, left/right clamping, and active-scalar derivatives:

```cpp
TEST(InterpWeightsTest, TestLinearWeightsReproduceLegacyValues) {
    const Vector_<> x{0.0, 1.0, 3.0};
    const Vector_<> y{2.0, 4.0, 10.0};
    const Interp::LinearWeightGeometry_ geometry(x);
    ASSERT_DOUBLE_EQ(Interp::ApplyInterpWeights(y, geometry.At(-1.0)), 2.0);
    ASSERT_DOUBLE_EQ(Interp::ApplyInterpWeights(y, geometry.At(0.0)), 2.0);
    ASSERT_DOUBLE_EQ(Interp::ApplyInterpWeights(y, geometry.At(2.0)), 7.0);
    ASSERT_DOUBLE_EQ(Interp::ApplyInterpWeights(y, geometry.At(4.0)), 10.0);
}

TEST(InterpWeightsTest, TestLinearWeightsPropagateAadOrdinateDerivatives) {
    auto* tape = AAD::Tape();
    AAD::Clear(tape);
    Vector_<AAD::Number_> y(3);
    for (int i = 0; i < 3; ++i)
        AAD::RegisterIndependent(y[i], 2.0 + static_cast<double>(i));
    AAD::NewRecording(*tape);
    const Interp::LinearWeightGeometry_ geometry(Vector_<>{0.0, 1.0, 3.0});
    AAD::Number_ result = Interp::ApplyInterpWeights(y, geometry.At(2.0));
    AAD::Adjoint(result) = 1.0;
    AAD::PropagateToStart(*tape);
    ASSERT_DOUBLE_EQ(AAD::Value(AAD::Adjoint(y[0])), 0.0);
    ASSERT_DOUBLE_EQ(AAD::Value(AAD::Adjoint(y[1])), 0.5);
    ASSERT_DOUBLE_EQ(AAD::Value(AAD::Adjoint(y[2])), 0.5);
    AAD::Clear(tape);
}
```

- [ ] **Step 2: Run the new tests and confirm the missing API failure**

Run:

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter=InterpWeightsTest.*
```

Expected: compilation fails because `interpweights.hpp` and its types do not exist.

- [ ] **Step 3: Implement the linear geometry contract**

Create this public internal shape:

```cpp
namespace Dal::Interp {
    using InterpWeights_ = Vector_<std::pair<int, double>>;

    template <class T_>
    T_ ApplyInterpWeights(const Vector_<T_>& values, const InterpWeights_& weights) {
        T_ retval(0.0);
        for (const auto& [index, weight] : weights) {
            REQUIRE(index >= 0 && index < static_cast<int>(values.size()),
                    "ApplyInterpWeights: weight index is outside the ordinate vector");
            retval += weight * values[index];
        }
        return retval;
    }

    class LinearWeightGeometry_ {
        Vector_<> x_;

    public:
        explicit LinearWeightGeometry_(const Vector_<>& x);
        [[nodiscard]] InterpWeights_ At(double x) const;
    };
}
```

Validate non-empty, strictly increasing abscissae. `At` returns one unit weight outside the grid and at exact knots, otherwise the two standard barycentric weights.

- [ ] **Step 4: Write failing natural-cubic geometry tests**

Add tests for exact knot reproduction, partition of unity, natural end conditions, dense/global sensitivity, and agreement with `Interp::NewCubic` at interior and exterior points. Use at least five non-uniform knots and query `x.front() - 0.25`, every knot, one point in every segment, and `x.back() + 0.25`.

- [ ] **Step 5: Implement natural-cubic weights once**

Add this interface:

```cpp
class NaturalCubicWeightGeometry_ {
    Vector_<> x_;
    Vector_<Vector_<>> secondDerivativeWeights_;

public:
    explicit NaturalCubicWeightGeometry_(const Vector_<>& x);
    [[nodiscard]] InterpWeights_ At(double x) const;
};
```

Move the tridiagonal coefficient construction now duplicated in `yclogdf.cpp` into `interpweights.cpp`. Build the `n x n` map once from passive abscissae. For a query, use the same segment clamp and Numerical Recipes spline formula as `Cubic1_::operator()` so natural-cubic values remain bit-compatible. Return all nonzero storage-node weights; retain tiny computed weights rather than thresholding them.

- [ ] **Step 6: Run interpolation tests**

Run:

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter='InterpWeightsTest.*:InterpTest.*:InterpMixedTest.*'
```

Expected: all selected tests pass.

- [ ] **Step 7: Commit the interpolation geometry**

```bash
git add dal-cpp/dal/math/interp/interpweights.hpp dal-cpp/dal/math/interp/interpweights.cpp dal-cpp/tests/math/interp/test_interpweights.cpp
git commit -m "Add typed interpolation weight geometry"
```

### Task 2: Make Log-DF Interpolation One Source of Truth

**Files:**
- Create: `dal-cpp/dal/curve/logdfinterp.hpp`
- Create: `dal-cpp/dal/curve/logdfinterp.cpp`
- Create: `dal-cpp/tests/curve/test_logdfinterp.cpp`
- Modify: `dal-cpp/dal/curve/yclogdf.hpp`
- Modify: `dal-cpp/dal/curve/yclogdf.cpp`

**Interfaces:**
- Consumes `LinearWeightGeometry_`, `NaturalCubicWeightGeometry_`, and `ApplyInterpWeights<T_>`.
- Produces `LogDfInterpolation_::WeightsAt(double)` and `Evaluate<T_>(values, yf)` for all three `LogDfScheme_` values.
- Owns the log-DF curve’s secant-right-extrapolation policy instead of changing general math interpolator behavior.

- [ ] **Step 1: Write the scheme and boundary contract tests**

For each scheme, compare `LogDfInterpolation_::Evaluate<double>` with the current `DiscountLogDF_` at every knot and representative interior points. Pin these cases separately:

- `yf < 0`: reproduce the current public `double` behavior—log-linear and mixed clamp to the anchor, while natural cubic extends its first-segment polynomial. This intentionally fixes the old AAD-only cubic mismatch without changing public `double` values.
- `yf == 0` and `yf == yf.back()`: reproduce exact stored nodes.
- `yf > yf.back()`: use the last two node secant for every scheme.
- `MIXED`: use linear weights through the computed cutoff and natural-cubic weights strictly after it.
- Minimum valid sizes: two nodes for log-linear, three for natural cubic, and the existing mixed tail requirement.

- [ ] **Step 2: Run the tests and confirm the missing type failure**

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter=LogDfInterpolationTest.*
```

Expected: compilation fails because `LogDfInterpolation_` does not exist.

- [ ] **Step 3: Implement the policy object**

Use this exact contract:

```cpp
namespace Dal {
    class LogDfInterpolation_ {
        Vector_<> yf_;
        LogDfScheme_ scheme_;
        Interp::LinearWeightGeometry_ linear_;
        std::unique_ptr<Interp::NaturalCubicWeightGeometry_> cubic_;
        int mixedCutoffIndex_ = -1;

        [[nodiscard]] Interp::InterpWeights_ SecantExtrapolation(double yf) const;

    public:
        LogDfInterpolation_(const Vector_<>& yf, LogDfScheme_ scheme);
        [[nodiscard]] Interp::InterpWeights_ WeightsAt(double yf) const;

        template <class T_>
        T_ Evaluate(const Vector_<T_>& values, double yf) const {
            REQUIRE(values.size() == yf_.size(),
                    "LogDfInterpolation_: ordinate count must equal year-fraction count");
            return Interp::ApplyInterpWeights(values, WeightsAt(yf));
        }
    };
}
```

For mixed curves, construct cubic geometry on the tail and translate its local indices back to storage indices. Do not rebuild geometry when ordinate values change.

- [ ] **Step 4: Replace the split `DiscountLogDF_` evaluators**

In `DiscountLogDF_`:

- Replace `interp_`, `fppCoef_`, `mixedCutoffIndex_`, and `mixedCutoffYf_` with `LogDfInterpolation_ interpolation_`.
- Delete `RebuildInterp`, `RebuildBasisAux`, `CubicBasisAt`, `CubicExtrapWeights`, and `StorageBasisWeightsAt`.
- Make `LogDfAt` unconditionally call `interpolation_.Evaluate(logDF_, yf)` for both scalar types.
- Make `ApplyDX` update only `logDF_`; passive geometry does not need rebuilding.
- Retain `AAD::Value` extraction only in accessors, validation, and serialization.

The resulting evaluation is:

```cpp
template <class T_, class B_>
T_ DiscountLogDF_<T_, B_>::operator()(const Date_& from, const Date_& to) const {
    const double yfFrom = dayCount_(nodeDates_.front(), from, nullptr);
    const double yfTo = dayCount_(nodeDates_.front(), to, nullptr);
    const T_ logDf = interpolation_.Evaluate(logDF_, yfTo) - interpolation_.Evaluate(logDF_, yfFrom);
    if (this->base_) {
        const auto baseFactor = (*this->base_)(from, to);
        if constexpr (std::is_same_v<T_, double>)
            return std::exp(logDf) * baseFactor;
        else
            return AAD::exp(logDf) * baseFactor;
    }
    if constexpr (std::is_same_v<T_, double>)
        return std::exp(logDf);
    else
        return AAD::exp(logDf);
}
```

- [ ] **Step 5: Run log-DF and analytic-Jacobian regressions**

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter='LogDfInterpolationTest.*:AnalyticJacobianTest.*:CurveJacobianModeFlagTest.*'
```

Expected: all selected tests pass, including central-difference comparisons for log-linear, natural-cubic, and mixed curves.

- [ ] **Step 6: Commit the unified log-DF path**

```bash
git add dal-cpp/dal/curve/logdfinterp.hpp dal-cpp/dal/curve/logdfinterp.cpp dal-cpp/dal/curve/yclogdf.hpp dal-cpp/dal/curve/yclogdf.cpp dal-cpp/tests/curve/test_logdfinterp.cpp
git commit -m "Unify log discount interpolation paths"
```

### Task 3: Promote PWC to the Typed Curve Model

**Files:**
- Modify: `dal-cpp/dal/curve/ycconst.hpp`
- Modify: `dal-cpp/dal/curve/ycconst.cpp`
- Modify: `dal-cpp/tests/curve/test_piecewiseconstant.cpp`

**Interfaces:**
- Produces `Tape::DiscountPWC_<T_, B_>` with the same base-type pattern as PWLF.
- Keeps `NewDiscountPWC` unchanged and keeps PWC persistence explicitly unsupported.

- [ ] **Step 1: Add typed PWC parity and AAD tests**

Cover factory-versus-direct construction, `NX`, `ApplyDX`, before/at/between/after knots, passive base multiplication, active base multiplication, and AAD gradients against central differences. The direct type must be constructible as:

```cpp
Tape::DiscountPWC_<double> primal(name, ccy, knotDates, fRight);
Tape::DiscountPWC_<AAD::Number_> active(name, ccy, knotDates, fRightT);
Tape::DiscountPWC_<AAD::Number_, Tape::DiscountCurve_<AAD::Number_>> layered(
    name, ccy, knotDates, fRightT, activeBase);
```

- [ ] **Step 2: Confirm the direct typed type is absent**

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
```

Expected: compilation fails because `Tape::DiscountPWC_` is not declared.

- [ ] **Step 3: Move PWC into `namespace Tape` and template it**

Declare:

```cpp
namespace Dal::Tape {
    template <class T_, class B_ = DiscountCurve_<double>>
    class DiscountPWC_ : public CurveWithBase_<DiscountCurve_<T_>, B_>, public FittableCurve_ {
        Vector_<Date_> knotDates_;
        Vector_<T_> fRightT_;
        Vector_<T_> sofarT_;

        void UpdateT();
        [[nodiscard]] T_ IntegralTo(const Date_& date) const;

    public:
        DiscountPWC_(const String_& name,
                     const String_& ccy,
                     const Vector_<Date_>& knotDates,
                     const Vector_<T_>& fRightT,
                     const Handle_<B_>& base = Handle_<B_>());
        T_ operator()(const Date_& from, const Date_& to) const override;
        [[nodiscard]] int NX() const override;
        void ApplyDX(Vector_<>::const_iterator dx, double leverage) override;
        void Write(Archive::Store_& dst) const override;
        [[nodiscard]] DiscountPWC_* Clone(const String_& newName,
                                          const YCComponent_::substitutions_t& baseChanges) const override;
    };
}
```

For `double`, delegate integral-cache construction and evaluation to `PiecewiseConstant_` to preserve ordering. For active scalars, accumulate the same rectangle integrals with passive day counts and call `AAD::exp`. Explicitly instantiate the same three scalar/base combinations as PWLF.

- [ ] **Step 4: Keep the public factory as a compatibility adapter**

```cpp
DiscountCurve_* NewDiscountPWC(const String_& name,
                               const String_& ccy,
                               const PiecewiseConstant_& fwds,
                               const Handle_<DiscountCurve_>& base) {
    return new Tape::DiscountPWC_<double>(name, ccy, fwds.knotDates_, fwds.fRight_, base);
}
```

- [ ] **Step 5: Run PWC/PWL curve tests**

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter='PiecewiseConstantTest.*:PiecewiseLinearTest.*:JointAnalyticJacobianTest.TestPwl*'
```

Expected: all selected tests pass and the unsupported-persistence exception remains unchanged.

- [ ] **Step 6: Commit typed PWC support**

```bash
git add dal-cpp/dal/curve/ycconst.hpp dal-cpp/dal/curve/ycconst.cpp dal-cpp/tests/curve/test_piecewiseconstant.cpp
git commit -m "Add typed piecewise constant curves"
```

### Task 4: Add One Curve Definition, Layout, and Typed Factory

**Files:**
- Create: `dal-cpp/dal/curve/curveparameterization.hpp`
- Create: `dal-cpp/dal/curve/curveparameterization.cpp`
- Create: `dal-cpp/tests/curve/test_curveparameterization.cpp`
- Modify: `dal-cpp/dal/curve/yclogdf.hpp`
- Modify: `dal-cpp/dal/curve/yclogdf.cpp`

**Interfaces:**
- Produces `CurveDefinition_`, `CurveParameterLayout_`, `MakeCurveDefinition`, `RegisterCurveParameters`, and `BuildDiscountCurveT<T_, B_>`.
- Gives both calibration engines the same complete storage dates and flat solver-column order.

- [ ] **Step 1: Write layout tests for every implemented parameterization**

Pin these exact layouts:

```text
LOG_DISCOUNT with N declared maturity knots -> N+1 storage nodes, N solver columns, anchor at column -1
PIECEWISE_CONSTANT_FWD with N knots         -> N storage nodes, N solver columns
PIECEWISE_LINEAR_FWD with N knots           -> N storage nodes, 2N solver columns, continuing left/right in knot order
```

Assert that `ZERO_RATE` throws and that a joint log-DF definition prepends `today` exactly once.

- [ ] **Step 2: Add typed factory parity tests**

For every implemented representation/scheme, compare the public factory with `BuildDiscountCurveT<double>` at dates before, on, between, and after knots. Assert `NX()` equals `layout.parameterCount_` and `ApplyDX` consumes parameters in the layout order.

- [ ] **Step 3: Implement the definition and layout**

Use these exact public internal types:

```cpp
struct CurveDefinition_ {
    String_ name_;
    String_ ccy_;
    CurveParameterization_ parameterization_;
    LogDfScheme_ logDfScheme_;
    Vector_<Date_> nodeDates_;
    DayBasis_ dayCount_;
};

struct CurveParameterLayout_ {
    int storageNodeCount_ = 0;
    int parameterCount_ = 0;
    int paramsPerDeclaredKnot_ = 0;
    bool pinnedAnchor_ = false;
};

CurveDefinition_ MakeCurveDefinition(const String_& name,
                                     const String_& ccy,
                                     CurveParameterization_ parameterization,
                                     LogDfScheme_ logDfScheme,
                                     const Vector_<Date_>& declaredKnots,
                                     const Date_& anchor,
                                     const DayBasis_& dayCount);

CurveParameterLayout_ BuildCurveParameterLayout(const CurveDefinition_& definition);
```

`MakeCurveDefinition` prepends `anchor` only for log-DF when the supplied vector does not already start at the anchor. Reject a declared log-DF maturity at or before the anchor, and require strict date ordering for all representations.

- [ ] **Step 4: Implement flat registration and typed construction**

Expose:

```cpp
Vector_<AAD::Number_> RegisterCurveParameters(const Vector_<>& x);

template <class T_, class B_ = Tape::DiscountCurve_<double>>
std::shared_ptr<Tape::DiscountCurve_<T_>> BuildDiscountCurveT(
    const CurveDefinition_& definition,
    const Vector_<T_>& parameters,
    const Handle_<B_>& base = Handle_<B_>());
```

The factory validates `parameters.size()` against the layout, then:

- prepends scalar zero for log-DF and builds `DiscountLogDF_<T_, B_>`;
- passes the flat vector to `DiscountPWC_<T_, B_>`;
- deinterleaves PWL into left/right vectors and builds `DiscountPWLF_<T_, B_>`;
- throws the existing reserved-parameterization error for `ZERO_RATE`.

Generalize the log-DF declaration to:

```cpp
template <class T_, class B_ = DiscountCurve_<double>>
class DiscountLogDF_ : public CurveWithBase_<DiscountCurve_<T_>, B_>, public FittableCurve_;
```

Explicitly instantiate passive-base and active-base variants needed by single and joint calibration.

- [ ] **Step 5: Run factory and curve tests**

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter='CurveParameterizationTest.*:PiecewiseConstantTest.*:JointAnalyticJacobianTest.TestPwl*'
```

Expected: all selected tests pass with unchanged solver layout and curve values.

- [ ] **Step 6: Commit the common factory**

```bash
git add dal-cpp/dal/curve/curveparameterization.hpp dal-cpp/dal/curve/curveparameterization.cpp dal-cpp/dal/curve/yclogdf.hpp dal-cpp/dal/curve/yclogdf.cpp dal-cpp/tests/curve/test_curveparameterization.cpp
git commit -m "Centralize typed curve construction"
```

### Task 5: Share Backend-Neutral Jacobian Harvesting

**Files:**
- Create: `dal-cpp/dal/curve/aadjacobian.hpp`
- Create: `dal-cpp/dal/curve/aadjacobian.cpp`
- Modify: `dal-cpp/tests/curve/test_analytic_jacobian.cpp`
- Modify: `dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp`

**Interfaces:**
- Produces `HarvestCurveJacobian`, which accepts flat independent leaves, residuals, and optional per-row safe widths.
- Preserves native leaf clearing and full backend clearing for Adept/XAD/CoDiPack.

- [ ] **Step 1: Add direct harvester tests**

Record two asymmetric residuals over three independent variables. Assert the exact `2 x 3` matrix, repeat the harvest after rewinding, and verify that the second row contains no first-row adjoint residue. Add an exception-scope test proving `TapeGuard_` leaves the next recording clean.

- [ ] **Step 2: Implement the shared reverse sweep**

Declare:

```cpp
Matrix_<> HarvestCurveJacobian(AAD::Tape_& tape,
                               Vector_<AAD::Number_>& independents,
                               Vector_<AAD::Number_>& residuals,
                               const Vector_<int>& rowWidths = Vector_<int>());
```

Require every supplied width to be between zero and the number of columns. An empty width vector means full width for every row. For each residual:

1. Clear all backend adjoints before propagation for Adept/XAD/CoDiPack.
2. Seed the residual adjoint with one and propagate to the start.
3. Copy the requested prefix into the dense matrix.
4. Clear every native independent leaf, including leaves outside a shortened harvested prefix.

- [ ] **Step 3: Run tape and Jacobian tests**

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter='AnalyticJacobianTest.*:JointAnalyticJacobianTest.*:TapeTest.*'
```

Expected: all selected tests pass before callers are migrated.

- [ ] **Step 4: Commit the harvester**

```bash
git add dal-cpp/dal/curve/aadjacobian.hpp dal-cpp/dal/curve/aadjacobian.cpp dal-cpp/tests/curve/test_analytic_jacobian.cpp dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp
git commit -m "Share AAD curve Jacobian harvesting"
```

### Task 6: Enable Every Curve Method in Single Calibration

**Files:**
- Modify: `dal-cpp/dal/curve/calibration.cpp`
- Modify: `dal-cpp/tests/curve/test_analytic_jacobian.cpp`
- Modify: `dal-cpp/tests/curve/test_curve_jacobian_mode_flag.cpp`
- Modify: `dal-cpp/tests/curve/test_row_width.cpp`

**Interfaces:**
- Consumes the common curve definition/layout/factory, flat active parameter vector, and Jacobian harvester.
- Keeps the current instrument, target, projection, and trade-date eligibility restrictions.

- [ ] **Step 1: Replace the old fallback test with a support matrix**

Parameterize analytic-versus-central-difference tests over:

```text
PIECEWISE_CONSTANT_FWD
PIECEWISE_LINEAR_FWD
LOG_DISCOUNT / LOG_LINEAR
LOG_DISCOUNT / LOG_CUBIC_NATURAL
LOG_DISCOUNT / MIXED
```

For each case require a non-empty diagnostic Jacobian, matching dimensions and column order, central-difference agreement at the same solved parameters, residual convergence, and analytic-versus-bumped curve agreement. Keep separate fallback tests for forecast targets, projection instruments, trade-date mismatch, and `ZERO_RATE`.

- [ ] **Step 2: Run the new support tests and confirm PWC/PWL fail**

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter='AnalyticJacobianTest.TestMatchesCentralDifference*'
```

Expected: PWC and PWL cases fail because current eligibility only admits log-DF.

- [ ] **Step 3: Replace local construction switches**

Build one `CurveDefinition_` in `YieldCurveCalibrationFunc_`. Make primal `F` call `BuildDiscountCurveT<double>` and analytic `Gradient` call the same factory with flat registered `AAD::Number_` parameters. Remove local `ParamsPerKnot`, `BuildDiscountCurve`, and log-DF-only `BuildDiscountCurveT` after all callers use the shared implementation.

- [ ] **Step 4: Generalize eligibility without broadening instrument scope**

Replace the hard-coded log-DF check with a capability check that rejects only `ZERO_RATE`. Retain these current gates verbatim in behavior:

- discount-target calibration only;
- templated deposit/FRA/future/swap rate only;
- forecast equals discount;
- instrument trade date equals the curve anchor.

Update notices so they identify the unsupported capability instead of claiming log-DF is required.

- [ ] **Step 5: Harvest flat parameters safely**

Pass the flat registered parameter vector to `HarvestCurveJacobian`. Use full width for PWC, PWL, natural cubic, and mixed. Allow the existing maturity prefix optimization only for log-linear after `test_row_width.cpp` proves it against a full harvest.

- [ ] **Step 6: Run the single-calibration regression matrix**

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter='AnalyticJacobianTest.*:CurveJacobianModeFlagTest.*:CurveCalibrationTest.*:RowWidthTest.*'
```

Expected: all selected tests pass; every implemented curve method engages analytic mode for otherwise eligible single-curve specs.

- [ ] **Step 7: Commit single-calibration support**

```bash
git add dal-cpp/dal/curve/calibration.cpp dal-cpp/tests/curve/test_analytic_jacobian.cpp dal-cpp/tests/curve/test_curve_jacobian_mode_flag.cpp dal-cpp/tests/curve/test_row_width.cpp
git commit -m "Enable analytic Jacobians for all single curves"
```

### Task 7: Enable Mixed Curve Methods in Joint Calibration

**Files:**
- Modify: `dal-cpp/dal/curve/jointcalibration.cpp`
- Modify: `dal-cpp/dal/curve/jointcalibration.hpp`
- Modify: `dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp`
- Modify: `dal-cpp/tests/curve/test_joint_calibration.cpp`

**Interfaces:**
- Consumes the same definition/layout/factory and flat parameter vectors as single calibration.
- Supports PWC, PWL, and all log-DF schemes for discount and forward declarations, including active base layering.

- [ ] **Step 1: Add joint support-matrix tests**

Add otherwise-identical two-curve exact calibrations for homogeneous PWC, homogeneous PWL, and each homogeneous log-DF scheme. Add one mixed case with log-DF discount plus base-layered PWC forward, and one with PWC discount plus base-layered PWL forward. For each, verify:

- analytic diagnostics are populated;
- AAD matches full central differences, including cross-curve blocks;
- analytic and bumped solutions agree;
- flat Jacobian columns follow declaration order and each declaration’s layout;
- `computeForwardJacobian_ = false` still avoids the at-solution byproduct.

- [ ] **Step 2: Run joint tests and confirm unsupported declarations fail**

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter='JointAnalyticJacobianTest.Test*Parameterization*'
```

Expected: non-PWL cases fail under the current PWL-only typed builder and eligibility check.

- [ ] **Step 3: Store complete definitions in curve slots**

Replace `paramsPerKnot` arithmetic with `CurveDefinition_ definition` and `CurveParameterLayout_ layout` in each internal slot. For joint log-DF declarations, prepend `spec.today_` as the pinned storage anchor while leaving `decl.knotDates_` and the public spec unchanged. Derive smoothing expansion and offsets from `layout.parameterCount_` and stable column order.

- [ ] **Step 4: Replace PWL-pair registration with flat registration**

Change `RegisterTapeParameters` to return one flat `Vector_<AAD::Number_>` per declaration. Build discount declarations first, then forward declarations, using `BuildDiscountCurveT` with either an empty base or the active discount curve selected by `targetCollateral_`.

- [ ] **Step 5: Generalize joint validation and eligibility**

Permit base layering for all implemented typed representations. Reject only unimplemented `ZERO_RATE` at the curve-method gate. Retain the current ACT/365F, supported-instrument, projection-routing, and discount-declaration forecasting checks.

- [ ] **Step 6: Use the common harvester**

Flatten declaration leaves in global solver-column order and harvest full width. Do not use maturity-prefix truncation for joint curves until a later optimization proves support across cross-curve base dependencies.

- [ ] **Step 7: Run joint and staged regressions**

```bash
cmake --build build --target dal_cpp_tests -j$(nproc)
bin/dal_cpp_tests --gtest_filter='JointAnalyticJacobianTest.*:JointCalibrationTest.*:CurveBlockTest.*'
```

Expected: all selected tests pass, including active cross-curve derivatives and existing PWL exact-parity assertions.

- [ ] **Step 8: Commit joint-calibration support**

```bash
git add dal-cpp/dal/curve/jointcalibration.cpp dal-cpp/dal/curve/jointcalibration.hpp dal-cpp/tests/curve/test_joint_analytic_jacobian.cpp dal-cpp/tests/curve/test_joint_calibration.cpp
git commit -m "Enable mixed-parameter joint AAD calibration"
```

### Task 8: Reconcile Documentation, Performance, and Full Verification

**Files:**
- Modify: `dal-cpp/dal/curve/calibration.hpp`
- Modify: `docs/methodology/interpolation.md`
- Modify: `docs/methodology/log_discount_curve.md`
- Modify: `docs/methodology/yield_curve_jacobian.md`
- Modify: `dal-cpp/benchmarks/curve_calibration_perf/curve_calibration_perf.cpp`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Documents the implemented analytic support matrix and stable solver column layouts.
- Adds benchmark coverage without changing runtime defaults or public types.

- [ ] **Step 1: Update current-state methodology**

Document:

- passive geometry plus typed ordinate evaluation;
- natural-cubic global sensitivity and mixed cutoff behavior;
- log-DF secant extrapolation;
- PWC/PWL integration and base composition;
- pinned log-DF anchor versus declared free knots;
- column ordering for every representation;
- remaining instrument/configuration fallback gates;
- backend-neutral tape lifecycle and full-width harvesting policy.

Update `CurveJacobianMode` markup to remove the obsolete log-DF-only claim. Regenerate core and Excel Machinist outputs because enum markup changed.

- [ ] **Step 2: Add calibration benchmark cases**

Benchmark analytic and bumped construction/evaluation for PWC, PWL, and every log-DF scheme using the same instrument set and knot count. Report each representation separately; do not introduce a hard performance gate until baseline CI variance is measured.

- [ ] **Step 3: Run generated-file checks and format validation**

```bash
export MACHINIST_TEMPLATE_DIR=$PWD/dal-cpp/externals/machinist/template/
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-cpp/dal
./dal-cpp/externals/machinist/bin/Machinist -c dal-cpp/config/dal.ifc -l dal-cpp/config/dal.mgl -d ./dal-excel
cmake --build build --target dal_check_generated -j$(nproc)
```

Expected: regeneration succeeds and `dal_check_generated` reports no drift after generated outputs are staged.

- [ ] **Step 4: Run the full native build and test suite**

```bash
bash ./build_linux.sh
(cd build && ctest --output-on-failure)
```

Expected: both commands exit zero and CTest reports zero failed tests.

- [ ] **Step 5: Run supported alternate AAD configurations**

Run the repository’s CI-equivalent Adept, XAD, and CoDiPack build presets or scripts. For each available backend, run:

```bash
bin/dal_cpp_tests --gtest_filter='InterpWeightsTest.*:AnalyticJacobianTest.*:JointAnalyticJacobianTest.*'
```

Expected: all selected tests pass. If a backend dependency is unavailable locally, record the exact missing dependency in the PR and require its existing CI job before merge.

- [ ] **Step 6: Run the curve calibration benchmark**

```bash
bin/curve_calibration_perf
```

Expected: every new benchmark case completes and reports timings; investigate any analytic path slower than bumped for the same representation before merge.

- [ ] **Step 7: Commit documentation and verification coverage**

```bash
git add dal-cpp/dal/curve/calibration.hpp dal-cpp/dal/auto dal-excel/auto docs/methodology/interpolation.md docs/methodology/log_discount_curve.md docs/methodology/yield_curve_jacobian.md dal-cpp/benchmarks/curve_calibration_perf/curve_calibration_perf.cpp CHANGELOG.md
git commit -m "Document unified curve analytic Jacobians"
```

## Final Acceptance Checklist

- [ ] Both single and joint calibration engage `ANALYTIC` for PWC, PWL, log-linear, natural-cubic log-DF, and mixed log-DF when all non-curve eligibility gates pass.
- [ ] Every new analytic Jacobian agrees with two-sided central differences at the same parameter vector and preserves documented matrix dimensions and column order.
- [ ] PWC, PWL, and log-DF `double` results preserve existing factory, boundary, extrapolation, base, `ApplyDX`, and persistence behavior.
- [ ] The double and AAD paths use the same interpolation or integration definition; no curve keeps a separate hand-maintained AAD formula.
- [ ] Joint base-layered tests contain nonzero, correct cross-curve sensitivity blocks.
- [ ] Unsupported instruments/configurations and `ZERO_RATE` still fall back or reject exactly as documented.
- [ ] Native, Adept, XAD, and CoDiPack tape cleanup semantics are covered wherever their dependencies are available.
- [ ] Full CTest, generated-file checks, documentation, and benchmark smoke tests pass before merge.
