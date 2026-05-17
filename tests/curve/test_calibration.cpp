//
// Created by GitHub Copilot on 2026/5/17.
//

#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <dal/platform/platform.hpp>
#include <dal/curve/calibration.hpp>
#include <dal/curve/discount.hpp>
#include <dal/curve/ycinstrument.hpp>
#include <dal/storage/archive.hpp>

using namespace Dal;

namespace {
    class ConstantDiscountCurve_ : public DiscountCurve_ {
        double value_;
    public:
        ConstantDiscountCurve_(const String_& name, const String_& ccy, double value)
            : DiscountCurve_(name, ccy), value_(value) {}

        double operator()(const Date_&, const Date_&) const override { return value_; }

        void Poll(Vector_<const YCComponent_*>* all) const override { all->push_back(this); }

        void Poll(std::map<const YCComponent_*, Handle_<YCComponent_>>*) const override {}

        [[nodiscard]] ConstantDiscountCurve_* Clone(const String_& new_name,
                                                    const YCComponent_::substitutions_t&) const override {
            return new ConstantDiscountCurve_(new_name, ccy_.String(), value_);
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
        spec.parameterization_ = CurveParameterization_::Value_::ZERO_RATE;
        ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
    }

    {
        CurveCalibrationSpec_ spec = MakeValidSpec();
        spec.initialGuess_ = std::numeric_limits<double>::quiet_NaN();
        ASSERT_THROW(ValidateCurveCalibrationSpec(spec), Dal::Exception_);
    }
}

TEST(CalibrationTest, TestValidatePositiveDiscountFactorsRejectsInvalidCurves) {
    const Date_ today(2024, 1, 15);
    const Vector_<Date_> checkDates = {Date::AddMonths(today, 3), Date::AddMonths(today, 6)};

    const ConstantDiscountCurve_ zeroDf("zero", "USD", 0.0);
    ASSERT_THROW(ValidatePositiveDiscountFactors(zeroDf, today, checkDates), Dal::Exception_);

    const ConstantDiscountCurve_ nanDf("nan", "USD", std::numeric_limits<double>::quiet_NaN());
    ASSERT_THROW(ValidatePositiveDiscountFactors(nanDf, today, checkDates), Dal::Exception_);
}
