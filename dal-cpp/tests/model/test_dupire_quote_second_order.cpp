#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cfenv>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

#include <dal/model/detail/dupirequoteplan.hpp>
#include <dal/model/dupirequotesecondorder.hpp>
#include <dal/model/ivs.hpp>

#include "../../benchmarks/tape_perf/dupirereference.hpp"

using namespace Dal;
using namespace Dal::AAD;

namespace {
    using Reason = SecondOrderErrorReason_::Value_;
    using Stage = SecondOrderStage_::Value_;
    namespace Reference = TapePerfDupire::Reference;

    class FlatBase_ final : public IVS_ {
        bool changed_;

    public:
        explicit FlatBase_(bool changed = false) : IVS_(100.0, 0.05, 0.02), changed_(changed) {}
        double ImpliedVol(double strike, double time) const override {
            return changed_ && strike == 80.0 && time == 0.5 ? std::nextafter(0.20, 1.0) : 0.20;
        }
    };

    DupireCalibrationSnapshot_ Snapshot(bool subtractSpread = false, bool changedBase = false, double middleSpread = 0.0) {
        Matrix_<> spreads(3, 2, 0.0);
        spreads(0, 0) = subtractSpread ? 0.208 - 0.20 : 0.008;
        spreads(0, 1) = 0.012;
        spreads(1, 0) = middleSpread;
        spreads(1, 1) = 0.004;
        spreads(2, 0) = 0.006;
        spreads(2, 1) = 0.009;
        return CalibrateDupireWithRisk(
            FlatBase_(changedBase),
            {{75.0, 105.0, 135.0}, {0.4, 1.2}, spreads, {60.0, 70.0, 80.0, 90.0, 100.0, 110.0, 120.0, 130.0, 140.0}, 10.0, {0.5, 1.0}, 0.5});
    }

    DupireEuropeanCall_ Call() { return {105.0, 1.0, "EQ", Ccy_("USD")}; }

    class RestoreFloatingEnvironment_ {
        std::fenv_t environment_{};
        const int status_ = std::fegetenv(&environment_);
#if defined(__x86_64__) || defined(_M_X64)
        const unsigned mxcsr_ = _mm_getcsr();
#endif

    public:
        ~RestoreFloatingEnvironment_() {
            if (Saved())
                (void)std::fesetenv(&environment_);
#if defined(__x86_64__) || defined(_M_X64)
            _mm_setcsr(mxcsr_);
#endif
        }

        [[nodiscard]] bool Saved() const { return status_ == 0; }
    };

    template <class F_> void CheckValidation(F_&& fn, Reason reason, std::string_view location) {
        try {
            fn();
            FAIL() << "expected typed validation failure";
        } catch (const SecondOrderError_& error) {
            EXPECT_EQ(error.Reason().Switch(), reason);
            EXPECT_EQ(error.Stage().Switch(), Stage::VALIDATION);
            EXPECT_FALSE(error.DirectionIndex());
            EXPECT_NE(std::string(error.what()).find(location), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(DupireQuoteSecondOrderTest, CanonicalizesQuoteZeroButRetainsDirectionZero) {
    const auto positive = Snapshot(false, false, 0.0);
    const auto negative = Snapshot(false, false, -0.0);
    const auto positivePlan = PrepareDupireEuropeanCallQuoteRisk(positive, Call(), {0.8, 81, 64});
    const auto negativePlan = PrepareDupireEuropeanCallQuoteRisk(negative, Call(), {0.8, 81, 64});
    for (int rows : {1, 3}) {
        Matrix_<> directions(rows, 6, 0.0);
        directions(0, 3) = 1.0;
        directions(0, 2) = -0.0;
        const auto before = EvaluateDupireQuoteSecondOrder(positivePlan, positivePlan.InputAxis(), directions);
        const auto after = EvaluateDupireQuoteSecondOrder(negativePlan, negativePlan.InputAxis(), directions);
        const auto& a = before.Risk();
        const auto& b = after.Risk();
        EXPECT_EQ(a.Value(), b.Value());
        EXPECT_EQ(a.Gradient(), b.Gradient());
        EXPECT_EQ(a.DirectionalDerivatives(), b.DirectionalDerivatives());
        EXPECT_TRUE(std::equal(a.HessianProducts().begin(), a.HessianProducts().end(), b.HessianProducts().begin()));
        for (const auto* risk : {&a, &b}) {
            const auto& backendPoint = risk->Pointwise() ? risk->Pointwise()->Point() : risk->Shared()->Point();
            EXPECT_EQ(risk->Point()[2], 0.0);
            EXPECT_FALSE(std::signbit(risk->Point()[2]));
            EXPECT_EQ(backendPoint[2], 0.0);
            EXPECT_FALSE(std::signbit(backendPoint[2]));
            EXPECT_TRUE(std::signbit(risk->Directions()(0, 2)));
        }
    }
    EXPECT_FALSE(std::signbit(positive.Inputs().quoteSpreads_(1, 0)));
    EXPECT_TRUE(std::signbit(negative.Inputs().quoteSpreads_(1, 0)));
}

TEST(DupireQuoteSecondOrderTest, RejectsSubnormalSpreadsBeforeRecordingWithOrWithoutDaz) {
    const RestoreFloatingEnvironment_ restore;
    ASSERT_TRUE(restore.Saved());
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
#if defined(__x86_64__) || defined(_M_X64)
    constexpr unsigned DAZ = 1u << 6;
    _mm_setcsr(_mm_getcsr() & ~DAZ);
#endif
    constexpr double smallest = std::numeric_limits<double>::denorm_min();
    const std::array<double, 2> neighbors{smallest, -smallest};
    const std::array<DupireCalibrationSnapshot_, 2> snapshots{Snapshot(false, false, neighbors[0]), Snapshot(false, false, neighbors[1])};
    for (size_t i = 0; i < snapshots.size(); ++i)
        ASSERT_EQ(std::memcmp(&snapshots[i].Inputs().quoteSpreads_.Data()[2], &neighbors[i], sizeof(double)), 0);

    const auto reject = [&] {
        const auto nativeBefore = SecondOrderDetail::LastSecondOrderLogicalPeak();
        const auto sharedBefore = SharedSecondOrderDetail::LastLogicalPeak();
        for (const auto& snapshot : snapshots)
            CheckValidation([&] { (void)PrepareDupireEuropeanCallQuoteRisk(snapshot, Call(), {0.8, 81, 64}); }, Reason::OUTSIDE_DOMAIN, "quote:2");
        EXPECT_EQ(SecondOrderDetail::LastSecondOrderLogicalPeak(), nativeBefore);
        EXPECT_EQ(SharedSecondOrderDetail::LastLogicalPeak(), sharedBefore);
        EXPECT_EQ(std::fegetround(), FE_TONEAREST);
    };
    reject();
#if defined(__x86_64__) || defined(_M_X64)
    const unsigned beforeDaz = _mm_getcsr();
    {
        SCOPED_TRACE("MXCSR.DAZ enabled");
        const RestoreFloatingEnvironment_ restoreDaz;
        ASSERT_TRUE(restoreDaz.Saved());
        _mm_setcsr(beforeDaz | DAZ);
        ASSERT_NE(_mm_getcsr() & DAZ, 0U);
        reject();
        EXPECT_EQ(_mm_getcsr(), beforeDaz | DAZ);
    }
    EXPECT_EQ(_mm_getcsr(), beforeDaz);
#endif
}

TEST(DupireQuoteSecondOrderTest, RejectsOtherRoundingModesBeforeRecordingAndKeepsThePlan) {
    const RestoreFloatingEnvironment_ restore;
    ASSERT_TRUE(restore.Saved());
    const auto snapshot = Snapshot();
    const auto prepare = [&] { return PrepareDupireEuropeanCallQuoteRisk(snapshot, Call(), {0.8, 81, 64}); };
    const auto plan = prepare();
    const auto nativeBefore = SecondOrderDetail::LastSecondOrderLogicalPeak();
    const auto sharedBefore = SharedSecondOrderDetail::LastLogicalPeak();
    for (const auto& [mode, name] :
         {std::pair{FE_DOWNWARD, "FE_DOWNWARD"}, std::pair{FE_UPWARD, "FE_UPWARD"}, std::pair{FE_TOWARDZERO, "FE_TOWARDZERO"}}) {
        ASSERT_EQ(std::fesetround(mode), 0);
        CheckValidation([&] { (void)prepare(); }, Reason::UNSUPPORTED_OPERATION,
                        std::string("PrepareDupireEuropeanCallQuoteRisk requires FE_TONEAREST, current mode is ") + name);
        EXPECT_EQ(std::fegetround(), mode);
        for (int rows : {0, 1, 3}) {
            const Matrix_<> direction(rows, 6, 1.0);
            CheckValidation([&] { (void)EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), direction, {0, 0}); }, Reason::UNSUPPORTED_OPERATION,
                            std::string("EvaluateDupireQuoteSecondOrder requires FE_TONEAREST, current mode is ") + name);
            EXPECT_EQ(std::fegetround(), mode);
        }
        EXPECT_EQ(SecondOrderDetail::LastSecondOrderLogicalPeak(), nativeBefore);
        EXPECT_EQ(SharedSecondOrderDetail::LastLogicalPeak(), sharedBefore);
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
    for (int rows : {0, 1, 3}) {
        const auto risk = EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), Matrix_<>(rows, 6, 1.0));
        EXPECT_EQ(risk.Risk().Value(), DupireSecondOrderDetail::PlanAccess_::Margins(plan).price_);
        EXPECT_EQ(std::fegetround(), FE_TONEAREST);
    }
}

TEST(DupireQuoteSecondOrderTest, PublicEntryReturnsOwningPointwiseAndSharedGreeks) {
    const auto run = [](int rows) {
        const auto plan = PrepareDupireEuropeanCallQuoteRisk(Snapshot(), Call(), {0.8, 81, 64});
        auto axis = plan.InputAxis();
        Matrix_<> directions(rows, 6, 0.0);
        directions(0, 3) = 1.0;
        if (rows == 3) {
            directions(1, 2) = 1.0;
            const std::array<double, 6> mixed{0.5, -0.25, -0.5, 1.0, 0.25, -0.5};
            std::copy(mixed.begin(), mixed.end(), directions.Row(2).begin());
        }
        auto result = EvaluateDupireQuoteSecondOrder(plan, axis, directions);
        directions.Fill(0.0);
        axis.clear();
        return result;
    };
    for (int rows : {1, 3}) {
        const auto result = run(rows);
        const auto& risk = result.Risk();
        EXPECT_NEAR(risk.Value(), Reference::PRICE, 2e-7);
        EXPECT_EQ(result.Call().underlyingId_, "EQ");
        EXPECT_EQ(result.QuoteUnit(), "decimal-vol");
        EXPECT_EQ(result.CalibrationAlgorithm(), "DupireCentral1e-4FixedBaseBand/v1");
        ASSERT_EQ(result.InputAxis().size(), 6U);
        EXPECT_EQ(result.InputAxis()[3].id_, "quote:3");
        EXPECT_EQ(result.InputAxis()[3].strike_, 105.0);
        EXPECT_EQ(result.InputAxis()[3].maturity_, 1.2);
        EXPECT_EQ(risk.Point()[0], 0.008);
        ASSERT_EQ(risk.HessianProducts().Rows(), rows);
        EXPECT_EQ(risk.Directions()(0, 3), 1.0);
        for (int i = 0; i < 6; ++i)
            EXPECT_NEAR(risk.Gradient()[i], Reference::VEGA[i], 2e-6);
        for (int row = 0; row < rows; ++row) {
            double directional = 0.0;
            for (int col = 0; col < 6; ++col) {
                directional += Reference::VEGA[col] * risk.Directions()(row, col);
                EXPECT_NEAR(risk.HessianProducts()(row, col), Reference::PRODUCTS[row][col], 1e-5);
            }
            EXPECT_NEAR(risk.DirectionalDerivatives()[row], directional, 2e-6);
        }
        if (rows == 1) {
            ASSERT_TRUE(risk.Pointwise());
            EXPECT_EQ(risk.Pointwise()->Execution().recordings_, 1U);
            EXPECT_EQ(risk.Pointwise()->Execution().reverseSweeps_, 1U);
            EXPECT_EQ(risk.Pointwise()->Execution().safePathActivations_, 0U);
        } else {
            ASSERT_TRUE(risk.Shared());
            EXPECT_EQ(risk.Shared()->Execution().sharedRecordings_, 1U);
            EXPECT_EQ(risk.Shared()->Execution().sharedReverseSweeps_, 1U);
            EXPECT_EQ(risk.Shared()->Execution().replayDirections_, 0U);
        }
    }
}

TEST(DupireQuoteSecondOrderTest, RejectsNonregisteredInputsAndMisidentifiedAxesBeforeRecording) {
    const auto calibration = Snapshot();
    const auto call = Call();
    CheckValidation([&] { (void)PrepareDupireEuropeanCallQuoteRisk(Snapshot(true), call, {0.8, 81, 64}); }, Reason::OUTSIDE_DOMAIN, "quote:0");
    CheckValidation([&] { (void)PrepareDupireEuropeanCallQuoteRisk(Snapshot(false, true), call, {0.8, 81, 64}); }, Reason::OUTSIDE_DOMAIN, "80");
    CheckValidation([&] { (void)PrepareDupireEuropeanCallQuoteRisk(calibration, call, {0.1, 81, 64}); }, Reason::OUTSIDE_DOMAIN, "logSpotHalfWidth");
    CheckValidation([&] { (void)PrepareDupireEuropeanCallQuoteRisk(calibration, call, {0.8, 80, 64}); }, Reason::INVALID_INPUT, "spaceNodes");
    const auto plan = PrepareDupireEuropeanCallQuoteRisk(calibration, call, {0.8, 81, 64});
    Matrix_<> directions(0, 6);
    auto axis = plan.InputAxis();
    std::swap(axis[1], axis[2]);
    CheckValidation([&] { (void)EvaluateDupireQuoteSecondOrder(plan, axis, directions, {0, 0}); }, Reason::INVALID_INPUT, "column 1");
    axis = plan.InputAxis();
    axis[3].maturity_ = 1.25;
    CheckValidation([&] { (void)EvaluateDupireQuoteSecondOrder(plan, axis, directions, {0, 0}); }, Reason::INVALID_INPUT, "column 3");
    const auto empty = EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), directions);
    ASSERT_TRUE(empty.Risk().Pointwise());
    EXPECT_EQ(empty.Risk().HessianProducts().Rows(), 0);
    EXPECT_EQ(empty.Risk().HessianProducts().Cols(), 6);
    axis = plan.InputAxis();
    axis[1].id_ = axis[0].id_;
    CheckValidation([&] { (void)EvaluateDupireQuoteSecondOrder(plan, axis, directions); }, Reason::INVALID_INPUT, "column 1");
    axis = plan.InputAxis();
    axis[0].id_ = "QUOTE:0";
    CheckValidation([&] { (void)EvaluateDupireQuoteSecondOrder(plan, axis, directions); }, Reason::INVALID_INPUT, "column 0");
}

TEST(DupireQuoteSecondOrderTest, PrivateQualificationRejectsBadBoundaryAndUnresolvedMargins) {
    using namespace DupireSecondOrderDetail;
    const double epsilon = std::numeric_limits<double>::epsilon();
    CheckValidation([] { (void)Kernel_(120.0, 0.1); }, Reason::OUTSIDE_DOMAIN, "right discounted boundary margin is nonpositive at PDE time step 0");
    double minimum = 1.0;
    CheckValidation([&] { CertifyCalibration({epsilon, 2.0 * epsilon}, "Dupire numerator", 2, 1, minimum); }, Reason::NUMERICAL_FAILURE,
                    "calibration strike index 2, maturity index 1");
    Margins_ margins;
    CheckValidation([&] { CertifyRow({-1.0, epsilon}, {std::nextafter(2.0, 3.0)}, {-1.0, epsilon}, 4, 7, margins); }, Reason::NUMERICAL_FAILURE,
                    "PDE step 4, space index 7");
}

TEST(DupireQuoteSecondOrderTest, IncludesBothCalibrationAndPricingCurvature) {
    using namespace DupireSecondOrderDetail;
    const auto plan = PrepareDupireEuropeanCallQuoteRisk(Snapshot(), Call(), {0.8, 81, 64});
    const auto& kernel = PlanAccess_::Kernel(plan);
    const auto& quotes = PlanAccess_::Point(plan);
    const auto local = kernel.Calibrate(quotes);
    const int strikes = static_cast<int>(LOCAL_STRIKES.size()), times = static_cast<int>(LOCAL_TIMES.size());
    Vector_<> flatVols(strikes * times);
    Matrix_<> jacobian(strikes * times, quotes.size());
    const Matrix_<> noDirections(0, static_cast<int>(quotes.size()));
    for (int t = 0; t < times; ++t)
        for (int k = 0; k < strikes; ++k) {
            const int cell = t * strikes + k;
            flatVols[cell] = local[t][k];
            const auto calibrated = EvaluateSecondOrder([&](const auto& x) { return kernel.Calibrate(x)[t][k]; }, quotes, noDirections);
            std::copy(calibrated.Gradient().begin(), calibrated.Gradient().end(), jacobian.Row(cell).begin());
        }

    const auto pricing = [&](const Vector_<SecondOrderNumber_>& vols) {
        Surface_<SecondOrderNumber_> grid;
        for (int t = 0; t < times; ++t)
            for (int k = 0; k < strikes; ++k)
                grid[t][k] = vols[t * strikes + k];
        return kernel.Rollback(grid);
    };
    const auto priceVega = EvaluateSecondOrder(pricing, flatVols, Matrix_<>(0, flatVols.size())).Gradient();
    Matrix_<> direction(1, quotes.size(), 0.0);
    direction(0, 3) = 1.0;
    const auto fixedCalibration = [&](const Vector_<SecondOrderNumber_>& x) {
        Vector_<SecondOrderNumber_> linearVols(flatVols.begin(), flatVols.end());
        for (int cell = 0; cell < jacobian.Rows(); ++cell)
            for (int col = 0; col < jacobian.Cols(); ++col)
                linearVols[cell] += jacobian(cell, col) * (x[col] - quotes[col]);
        return pricing(linearVols);
    };
    const auto fixedPricing = [&](const Vector_<SecondOrderNumber_>& x) {
        const auto calibrated = kernel.Calibrate(x);
        SecondOrderNumber_ linearPrice(0.0);
        for (int t = 0; t < times; ++t)
            for (int k = 0; k < strikes; ++k)
                linearPrice += priceVega[t * strikes + k] * (calibrated[t][k] - local[t][k]);
        return linearPrice;
    };
    const auto fromPricing = EvaluateSecondOrder(fixedCalibration, quotes, direction);
    const auto fromCalibration = EvaluateSecondOrder(fixedPricing, quotes, direction);
    const auto full = EvaluateDupireQuoteSecondOrder(plan, plan.InputAxis(), direction);
    EXPECT_GT(std::abs(fromPricing.HessianProducts()(0, 3)), 1.0);
    EXPECT_GT(std::abs(fromCalibration.HessianProducts()(0, 3)), 1.0);
    for (int col = 0; col < direction.Cols(); ++col)
        EXPECT_NEAR(full.Risk().HessianProducts()(0, col), fromPricing.HessianProducts()(0, col) + fromCalibration.HessianProducts()(0, col), 1e-5);
}
