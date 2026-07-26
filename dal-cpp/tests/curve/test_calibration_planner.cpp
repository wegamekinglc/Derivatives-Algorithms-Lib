#include <gtest/gtest.h>

#include <dal/curve/calibration.hpp>
#include <dal/curve/ycinstrument.hpp>

using namespace Dal;

namespace {
    Handle_<YCInstrument_> Deposit(const Date_& today, int months) {
        return Handle_<YCInstrument_>(new Deposit_(today, Date::AddMonths(today, months), 0.02, DayBasis_("ACT_365F")));
    }
} // namespace

TEST(CalibrationPlannerTest, TestAugmentedPlanPreservesTraversalOriginsAndRawParameterOrder) {
    const Date_ today(2026, 1, 15);
    const Vector_<Handle_<YCInstrument_>> instruments{Deposit(today, 3), Deposit(today, 6)};
    const Vector_<Date_> submitted{
        Date::AddMonths(today, 6),
        Date::AddMonths(today, 1),
        Date::AddMonths(today, 6),
    };

    const auto plan = PlanCurveCalibrationKnots(today, instruments, submitted, CurveKnotPolicy_::Value_::AUGMENTED,
                                                CurveParameterization_::Value_::PIECEWISE_LINEAR_FWD);

    ASSERT_EQ(plan.plannerVersion_, 1);
    ASSERT_EQ(plan.requestedPolicy_, CurveKnotPolicy_::Value_::AUGMENTED);
    ASSERT_EQ(plan.executionPolicy_, CurveKnotPolicy_::Value_::INPUT);
    ASSERT_EQ(plan.submittedKnotDates_, submitted);
    ASSERT_EQ(plan.candidateTrace_.size(), 7);
    EXPECT_EQ(plan.candidateTrace_[0].disposition_, CurveKnotCandidateDisposition_::Value_::ADDED);
    EXPECT_EQ(plan.candidateTrace_[2].disposition_, CurveKnotCandidateDisposition_::Value_::DUPLICATE);
    EXPECT_EQ(plan.candidateTrace_[3].disposition_, CurveKnotCandidateDisposition_::Value_::FILTERED_NOT_AFTER_TODAY);
    EXPECT_EQ(plan.candidateTrace_[3].origin_.kind_, CurveKnotOriginKind_::Value_::INSTRUMENT_START);
    EXPECT_EQ(plan.candidateTrace_[4].origin_.instrumentInputIndex_, 0);

    ASSERT_EQ(plan.resolvedDeclaredNodes_.size(), 3);
    EXPECT_EQ(plan.resolvedDeclaredNodes_[0].date_, Date::AddMonths(today, 1));
    EXPECT_EQ(plan.resolvedDeclaredNodes_[1].date_, Date::AddMonths(today, 3));
    EXPECT_EQ(plan.resolvedDeclaredNodes_[2].date_, Date::AddMonths(today, 6));
    ASSERT_EQ(plan.resolvedDeclaredNodes_[2].origins_.size(), 3);
    EXPECT_EQ(plan.resolvedDeclaredNodes_[2].origins_[0].inputKnotIndex_, 0);
    EXPECT_EQ(plan.resolvedDeclaredNodes_[2].origins_[1].inputKnotIndex_, 2);
    EXPECT_EQ(plan.resolvedDeclaredNodes_[2].origins_[2].instrumentInputIndex_, 1);

    ASSERT_EQ(plan.freeParameters_.size(), 6);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(plan.freeParameters_[2 * i].date_, plan.resolvedDeclaredNodes_[i].date_);
        EXPECT_EQ(plan.freeParameters_[2 * i].component_, CurveFreeParameterComponent_::Value_::LEFT_FORWARD);
        EXPECT_EQ(plan.freeParameters_[2 * i + 1].component_, CurveFreeParameterComponent_::Value_::RIGHT_FORWARD);
    }
    EXPECT_EQ(plan.counts_.submittedKnots_, 3);
    EXPECT_EQ(plan.counts_.instrumentCandidates_, 4);
    EXPECT_EQ(plan.counts_.resolvedDeclaredNodes_, 3);
    EXPECT_EQ(plan.counts_.storageNodes_, 3);
    EXPECT_EQ(plan.counts_.freeParameters_, 6);
}

TEST(CalibrationPlannerTest, TestZeroRatePlanAddsSyntheticStorageAnchorOnly) {
    const Date_ today(2026, 1, 15);
    const Vector_<Date_> submitted{
        Date::AddMonths(today, 3),
        Date::AddMonths(today, 6),
    };

    const auto plan = PlanCurveCalibrationKnots(today, {}, submitted, CurveKnotPolicy_::Value_::INPUT, CurveParameterization_::Value_::ZERO_RATE);

    ASSERT_TRUE(plan.anchorAdded_);
    ASSERT_EQ(plan.resolvedDeclaredNodes_.size(), 2);
    ASSERT_EQ(plan.storageNodes_.size(), 3);
    EXPECT_EQ(plan.storageNodes_.front().date_, today);
    ASSERT_EQ(plan.storageNodes_.front().origins_.size(), 1);
    EXPECT_EQ(plan.storageNodes_.front().origins_.front().kind_, CurveKnotOriginKind_::Value_::SYNTHETIC_ANCHOR);
    ASSERT_EQ(plan.freeParameters_.size(), 2);
    EXPECT_EQ(plan.freeParameters_[0].component_, CurveFreeParameterComponent_::Value_::ZERO_RATE);
}

TEST(CalibrationPlannerTest, TestExecutionIdentityUsesActualInputDefinitionAndLayout) {
    CurveCalibrationSpec_ spec;
    spec.today_ = Date_(2026, 1, 15);
    spec.ccy_ = "USD";
    spec.curveName_ = "identity";
    spec.knotPolicy_ = CurveKnotPolicy_::Value_::INPUT;
    spec.parameterization_ = CurveParameterization_::Value_::LOG_DISCOUNT;
    spec.logDfScheme_ = LogDfScheme_::Value_::MIXED;
    spec.knotDates_ = {
        spec.today_,
        Date::AddMonths(spec.today_, 3),
        Date::AddMonths(spec.today_, 6),
        Date::AddMonths(spec.today_, 12),
    };

    const auto identity = InspectCurveCalibrationExecutionIdentity(spec);

    EXPECT_EQ(identity.identityVersion_, 1);
    EXPECT_EQ(identity.executionPolicy_, CurveKnotPolicy_::Value_::INPUT);
    EXPECT_EQ(identity.today_, spec.today_);
    EXPECT_EQ(identity.parameterization_, CurveParameterization_::Value_::LOG_DISCOUNT);
    ASSERT_TRUE(identity.logDfScheme_.has_value());
    EXPECT_EQ(*identity.logDfScheme_, LogDfScheme_::Value_::MIXED);
    EXPECT_EQ(identity.resolvedDeclaredDates_, spec.knotDates_);
    EXPECT_EQ(identity.storageDates_, spec.knotDates_);
    ASSERT_EQ(identity.freeParameters_.size(), 3);
    EXPECT_EQ(identity.freeParameters_.front().date_, spec.knotDates_[1]);
    EXPECT_EQ(identity.freeParameters_.front().component_, CurveFreeParameterComponent_::Value_::LOG_DISCOUNT_FACTOR);
    EXPECT_EQ(identity.counts_.resolvedDeclaredNodes_, 4);
    EXPECT_EQ(identity.counts_.storageNodes_, 4);
    EXPECT_EQ(identity.counts_.freeParameters_, 3);
}

TEST(CalibrationPlannerTest, TestExecutionIdentityRejectsNonInputSpec) {
    CurveCalibrationSpec_ spec;
    spec.today_ = Date_(2026, 1, 15);
    spec.ccy_ = "USD";
    spec.knotPolicy_ = CurveKnotPolicy_::Value_::INSTRUMENTS;
    spec.parameterization_ = CurveParameterization_::Value_::PIECEWISE_CONSTANT_FWD;
    spec.knotDates_ = {Date::AddMonths(spec.today_, 3)};

    EXPECT_THROW(static_cast<void>(InspectCurveCalibrationExecutionIdentity(spec)), Exception_);
}

TEST(CalibrationPlannerTest, TestStructuredAnalyticEligibilityUsesStableReason) {
    CurveCalibrationSpec_ spec;
    spec.today_ = Date_(2026, 1, 15);
    spec.calibrateDiscountCurve_ = false;

    const AnalyticEligibilityReport_ report = ValidateSingleCurveAnalyticEligibility(spec);

    ASSERT_FALSE(report.eligible_);
    ASSERT_EQ(report.issues_.size(), 1);
    EXPECT_EQ(report.issues_[0].reason_.Switch(), AnalyticIneligibilityReason_::Value_::DISCOUNT_TARGET_REQUIRED);
    EXPECT_EQ(report.issues_[0].group_, String_("single"));
    EXPECT_EQ(report.issues_[0].declarationIndex_, -1);
    EXPECT_EQ(report.issues_[0].instrumentIndex_, -1);
}
