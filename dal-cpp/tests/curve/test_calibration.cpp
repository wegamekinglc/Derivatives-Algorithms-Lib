//
// Created by GitHub Copilot on 2026/5/17.
//

#include <gtest/gtest.h>

#include <limits>
#include <memory>

#include <dal/curve/calibration.hpp>
#include <dal/curve/curveblock.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/curve/yczerorate.hpp>
#include <dal/platform/platform.hpp>
#include <dal/storage/archive.hpp>

using namespace Dal;

namespace {
    class ConstantDiscountCurve_ : public DiscountCurve_ {
        double value_;

    public:
        ConstantDiscountCurve_(const String_& name, const String_& ccy, double value) : DiscountCurve_(name, ccy), value_(value) {}

        double operator()(const Date_&, const Date_&) const override { return value_; }

        void Poll(Vector_<const YCComponent_*>* all) const override { all->push_back(this); }

        void Poll(std::map<const YCComponent_*, Handle_<YCComponent_>>*) const override {}

        [[nodiscard]] std::unique_ptr<YCComponent_> Clone(const String_& newName, const YCComponent_::substitutions_t&) const override {
            return std::make_unique<ConstantDiscountCurve_>(newName, ccy_.String(), value_);
        }

        void Write(Archive::Store_&) const override {}
    };

    Vector_<Handle_<YCInstrument_>> MakeCalibrationInstruments(const Date_& today) {
        const DayBasis_ basis("ACT_365F");
        return {
            Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, 3), 0.0125, basis)),
            Handle_<YCInstrument_>(new STIR_(today, Date::AddMonths(today, 3), Date::AddMonths(today, 6), 0.0135, basis)),
            Handle_<YCInstrument_>(new Swap_(today, Date::AddMonths(today, 12), 0.0150, 6, basis)),
        };
    }

    CurveCalibrationSpec_ MakeValidSpec() {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2024, 1, 15);
        spec.ccy_ = "USD";
        spec.instruments_ = MakeCalibrationInstruments(spec.today_);
        spec.knotDates_ = {Date::AddMonths(spec.today_, 3), Date::AddMonths(spec.today_, 6), Date::AddMonths(spec.today_, 12)};
        return spec;
    }

    CurveCalibrationSpec_ MakeZeroRateCalibrationSpec(LogDfScheme_ scheme = LogDfScheme_::Value_::LOG_LINEAR,
                                                      CurveSolveMode_ solveMode = CurveSolveMode_::Value_::EXACT,
                                                      const Handle_<DiscountCurve_>& base = Handle_<DiscountCurve_>()) {
        CurveCalibrationSpec_ spec;
        spec.today_ = Date_(2024, 1, 15);
        spec.ccy_ = "USD";
        spec.curveName_ = "zero_rate_calibrated";
        spec.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
        spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
        spec.solveMode_ = solveMode;
        spec.liborBasis_ = DayBasis_("ACT_365F");
        spec.tolerance_ = 1.0e-11;
        spec.fitTolerance_ = 1.0e-9;
        spec.smoothingWeight_ = 1.0;
        spec.initialGuess_ = 0.01;
        spec.logDfScheme_ = scheme;
        spec.baseCurve_ = base;
        spec.knotDates_ = {
            Date::AddMonths(spec.today_, 3),
            Date::AddMonths(spec.today_, 6),
            Date::AddMonths(spec.today_, 12),
            Date::AddMonths(spec.today_, 24),
        };

        const Vector_<> targetRates = {0.011, 0.014, 0.017, 0.021};
        const Handle_<DiscountCurve_> target(
            NewDiscountZeroRate("zero_rate_market", spec.ccy_, spec.today_, spec.knotDates_, targetRates, spec.liborBasis_, scheme, base));
        const CurveBlock_ market("zero_rate_market", spec.ccy_, {{spec.targetCollateral_, target}}, {}, spec.liborBasis_);
        Handle_<YieldCurve_> empty;
        for (const auto& maturity : spec.knotDates_) {
            const Handle_<YCInstrument_> prototype(new Deposit_(spec.today_, maturity, 0.0, spec.liborBasis_));
            const double marketRate = (*prototype->Precompute(empty))(market);
            spec.instruments_.push_back(Handle_<YCInstrument_>(new Deposit_(spec.today_, maturity, marketRate, spec.liborBasis_)));
        }
        return spec;
    }

    void AssertZeroRateCalibration(const CurveCalibrationSpec_& spec, const CurveCalibrationResult_& result, double tolerance) {
        ASSERT_NE(result.curve_, nullptr);
        const auto* zeroRate = dynamic_cast<const DiscountZeroRate_*>(result.curve_.get());
        ASSERT_NE(zeroRate, nullptr);
        ASSERT_EQ(zeroRate->NodeDates(), spec.knotDates_);
        ASSERT_EQ(zeroRate->NodeZeroRates().size(), spec.knotDates_.size());
        ASSERT_LT(result.diagnostics_.maxAbsResidual_, tolerance);
        for (const auto& knot : spec.knotDates_) {
            const double df = (*result.curve_)(spec.today_, knot);
            ASSERT_TRUE(std::isfinite(df));
            ASSERT_GT(df, 0.0);
        }
    }
} // namespace

TEST(CalibrationTest, TestBuildCurveCalibrationWeightsExpandsParametersPerKnot) {
    const Vector_<Date_> knotDates = {Date_(2024, 4, 15), Date_(2024, 7, 15), Date_(2025, 1, 15)};

    std::unique_ptr<Sparse::TriDiagonal_> weights(BuildCurveCalibrationWeights(knotDates, 2, 0.5));

    ASSERT_EQ(weights->Size(), 6);
    ASSERT_TRUE(weights->IsSymmetric());
}

TEST(CalibrationTest, TestBuildCurveCalibrationKnotsPolicies) {
    const Date_ today(2024, 1, 15);
    const Vector_<Handle_<YCInstrument_>> instruments = MakeCalibrationInstruments(today);
    const Vector_<Date_> inputKnots = {
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 18),
    };

    const Vector_<Date_> inputOnly = BuildCurveCalibrationKnots(today, instruments, inputKnots, CurveKnotPolicy_::Value_::INPUT);
    ASSERT_EQ(inputOnly.size(), 3);
    ASSERT_EQ(inputOnly[0], Date::AddMonths(today, 6));
    ASSERT_EQ(inputOnly[1], Date::AddMonths(today, 12));
    ASSERT_EQ(inputOnly[2], Date::AddMonths(today, 18));

    const Vector_<Date_> instrumentOnly = BuildCurveCalibrationKnots(today, instruments, {}, CurveKnotPolicy_::Value_::INSTRUMENTS);
    ASSERT_EQ(instrumentOnly.size(), 3);
    ASSERT_EQ(instrumentOnly[0], Date::AddMonths(today, 3));
    ASSERT_EQ(instrumentOnly[1], Date::AddMonths(today, 6));
    ASSERT_EQ(instrumentOnly[2], Date::AddMonths(today, 12));

    const Vector_<Date_> augmented = BuildCurveCalibrationKnots(today, instruments, inputKnots, CurveKnotPolicy_::Value_::AUGMENTED);
    ASSERT_EQ(augmented.size(), 4);
    ASSERT_EQ(augmented[0], Date::AddMonths(today, 3));
    ASSERT_EQ(augmented[1], Date::AddMonths(today, 6));
    ASSERT_EQ(augmented[2], Date::AddMonths(today, 12));
    ASSERT_EQ(augmented[3], Date::AddMonths(today, 18));
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecAcceptsInstrumentDerivedKnots) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.knotDates_.clear();
    spec.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;

    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecRejectsInvalidConfigurations) {
    {
        CurveCalibrationSpec_ spec = MakeValidSpec();
        spec.instruments_.clear();
        ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
    }

    {
        CurveCalibrationSpec_ spec = MakeValidSpec();
        spec.knotDates_[0] = spec.today_;
        ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
    }

    {
        CurveCalibrationSpec_ spec = MakeValidSpec();
        spec.knotDates_.back() = Date::AddMonths(spec.today_, 6);
        ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
    }

    {
        CurveCalibrationSpec_ spec = MakeValidSpec();
        spec.initialGuess_ = std::numeric_limits<double>::quiet_NaN();
        ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
    }
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecAcceptsFutureOnlyZeroRateKnots) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    spec.initialGuess_ = 0.02;

    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecRejectsZeroRateAnchorKnotWithSpecificMessage) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    spec.knotDates_ = Vector::Join(Vector_<Date_>{spec.today_}, spec.knotDates_);

    try {
        ValidateCurveCalibrationSpec(spec);
        FAIL() << "Expected ZERO_RATE anchor knot to be rejected";
    } catch (const Dal::Exception_& e) {
        ASSERT_TRUE(std::string(e.what()).find("ZERO_RATE calibration requires every knot to be strictly after today") != std::string::npos)
            << e.what();
    }
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecChecksZeroRateGuessAgainstParameterCount) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
    spec.initialGuessPerNode_ = {0.01, 0.02, 0.03};
    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));

    spec.initialGuessPerNode_.pop_back();
    ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);

    spec.initialGuessPerNode_ = {0.01, 0.02, std::numeric_limits<double>::infinity()};
    ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecChecksForwardGuessAgainstParameterCount) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
    spec.initialGuessPerNode_ = {0.01, 0.02, 0.03};
    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));

    spec.initialGuessPerNode_.pop_back();
    ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);

    spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD;
    spec.initialGuessPerNode_ = {0.01, 0.02, 0.03, 0.04, 0.05, 0.06};
    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));

    spec.initialGuessPerNode_.pop_back();
    ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
}

TEST(CalibrationTest, TestValidateCurveCalibrationSpecPreservesLogDiscountAnchorContract) {
    CurveCalibrationSpec_ spec = MakeValidSpec();
    spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);

    spec.knotDates_ = Vector::Join(Vector_<Date_>{spec.today_}, spec.knotDates_);
    spec.initialGuessPerNode_ = {0.01, 0.02, 0.03};
    ASSERT_NO_THROW(ValidateCurveCalibrationSpec(spec));
}

TEST(CalibrationTest, TestResolveInitialGuessMapsLogDiscountScalarByNodeDate) {
    CurveCalibrationSpec_ spec;
    spec.today_ = Date_(2026, 1, 1);
    spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    spec.logDfScheme_ = LogDfScheme_::Value_::LOG_LINEAR;
    spec.liborBasis_ = DayBasis_("ACT_365F");
    spec.initialGuess_ = 0.04;
    spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
    spec.knotDates_ = {
        spec.today_,
        spec.today_.AddDays(365),
        spec.today_.AddDays(730),
    };

    const Vector_<> scalar = ResolveCurveCalibrationInitialGuess(spec);

    ASSERT_EQ(scalar.size(), static_cast<size_t>(2));
    EXPECT_NEAR(scalar[0], -0.04, 1.0e-14);
    EXPECT_NEAR(scalar[1], -0.08, 1.0e-14);

    spec.initialGuessPerNode_ = {-0.03, -0.07};
    EXPECT_EQ(ResolveCurveCalibrationInitialGuess(spec), spec.initialGuessPerNode_);
}

TEST(CalibrationTest, TestValidatePositiveDiscountFactorsRejectsInvalidCurves) {
    const Date_ today(2024, 1, 15);
    const Vector_<Date_> checkDates = {Date::AddMonths(today, 3), Date::AddMonths(today, 6)};

    const ConstantDiscountCurve_ zeroDf("zero", "USD", 0.0);
    ASSERT_THROW(ValidatePositiveDiscountFactors(zeroDf, today, checkDates), Dal::Exception_);

    const ConstantDiscountCurve_ nanDf("nan", "USD", std::numeric_limits<double>::quiet_NaN());
    ASSERT_THROW(ValidatePositiveDiscountFactors(nanDf, today, checkDates), Dal::Exception_);
}

TEST(CalibrationTest, TestZeroRateExactCalibrationRepricesAndReturnsPersistentType) {
    const CurveCalibrationSpec_ spec = MakeZeroRateCalibrationSpec();
    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec);

    AssertZeroRateCalibration(spec, result, 1.0e-8);
    const Vector_<> expected = {0.011, 0.014, 0.017, 0.021};
    const Vector_<> actual = dynamic_cast<const DiscountZeroRate_&>(*result.curve_).NodeZeroRates();
    for (int i = 0; i < static_cast<int>(expected.size()); ++i)
        ASSERT_NEAR(actual[i], expected[i], 1.0e-8) << "node=" << i;
}

TEST(CalibrationTest, TestZeroRateApproximateCalibrationReturnsPositiveFiniteDiscountFactors) {
    CurveCalibrationSpec_ spec = MakeZeroRateCalibrationSpec(LogDfScheme_::Value_::LOG_LINEAR, CurveSolveMode_::Value_::APPROXIMATE);
    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec);

    AssertZeroRateCalibration(spec, result, spec.fitTolerance_);
    ASSERT_TRUE(result.diagnostics_.usedApproximateFit_);
}

TEST(CalibrationTest, TestZeroRateScalarAndPerNodeGuessesConvergeToSameCurve) {
    CurveCalibrationSpec_ scalarSpec = MakeZeroRateCalibrationSpec();
    scalarSpec.initialGuess_ = 0.03;
    CurveCalibrationSpec_ perNodeSpec = scalarSpec;
    perNodeSpec.initialGuessPerNode_ = {0.005, 0.010, 0.015, 0.020};

    const CurveCalibrationResult_ scalarResult = CalibrateYieldCurve(scalarSpec);
    const CurveCalibrationResult_ perNodeResult = CalibrateYieldCurve(perNodeSpec);
    AssertZeroRateCalibration(scalarSpec, scalarResult, 1.0e-8);
    AssertZeroRateCalibration(perNodeSpec, perNodeResult, 1.0e-8);
    for (const auto& knot : scalarSpec.knotDates_)
        ASSERT_NEAR((*scalarResult.curve_)(scalarSpec.today_, knot), (*perNodeResult.curve_)(perNodeSpec.today_, knot), 1.0e-9);
}

TEST(CalibrationTest, TestZeroRateCalibrationTreatsParametersAsSpreadsOverBase) {
    const Date_ today(2024, 1, 15);
    const Vector_<Date_> baseKnots = {
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 12),
        Date::AddMonths(today, 24),
    };
    const Handle_<DiscountCurve_> base(NewDiscountZeroRate("base", "USD", today, baseKnots, Vector_<>(baseKnots.size(), 0.0075),
                                                           DayBasis_("ACT_365F"), LogDfScheme_::Value_::LOG_LINEAR));
    const CurveCalibrationSpec_ spec = MakeZeroRateCalibrationSpec(LogDfScheme_::Value_::LOG_LINEAR, CurveSolveMode_::Value_::EXACT, base);
    const CurveCalibrationResult_ result = CalibrateYieldCurve(spec);

    AssertZeroRateCalibration(spec, result, 1.0e-8);
    const Vector_<> expectedSpreads = {0.011, 0.014, 0.017, 0.021};
    const Vector_<> actualSpreads = dynamic_cast<const DiscountZeroRate_&>(*result.curve_).NodeZeroRates();
    for (int i = 0; i < static_cast<int>(expectedSpreads.size()); ++i)
        ASSERT_NEAR(actualSpreads[i], expectedSpreads[i], 1.0e-8) << "node=" << i;
}

TEST(CalibrationTest, TestZeroRateStageUsesSingleCurveDelegate) {
    const CurveCalibrationSpec_ stage = MakeZeroRateCalibrationSpec();
    MultiCurveCalibrationSpec_ multi;
    multi.name_ = "zero_rate_bundle";
    multi.ccy_ = stage.ccy_;
    multi.liborBasis_ = stage.liborBasis_;
    multi.stages_ = {stage};

    const MultiCurveCalibrationResult_ result = CalibrateMultiCurve(multi);
    ASSERT_EQ(result.discountCurves_.size(), 1u);
    const auto found = result.discountCurves_.find(stage.targetCollateral_);
    ASSERT_NE(found, result.discountCurves_.end());
    ASSERT_NE(dynamic_cast<const DiscountZeroRate_*>(found->second.get()), nullptr);
    ASSERT_EQ(result.diagnostics_.size(), 1u);
    ASSERT_LT(result.diagnostics_[0].maxAbsResidual_, 1.0e-8);
}
