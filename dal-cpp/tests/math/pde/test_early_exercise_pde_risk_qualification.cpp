#include <gtest/gtest.h>

#include <array>
#include <cmath>

#include <dal/math/pde/earlyexercisepderisk.hpp>
#include <dal/math/pde/earlyexercisepderiskinternal.hpp>
#include <dal/platform/platform.hpp>

#include <pde-risk-qualification/reference.hpp>

using Dal::Date_;
using Dal::EarlyExerciseStyle_;
using Dal::OptionType_;
using Dal::PDE::EarlyExercisePdeRiskRequest_;
using Dal::PDE::EvaluateEarlyExercisePdeRisk;

namespace {
    EarlyExercisePdeRiskRequest_ QualificationRequest(int days = 365) {
        const Date_ evaluation(2026, 10, 18), maturity = evaluation.AddDays(days);
        EarlyExercisePdeRiskRequest_ r;
        r.valuation_.contract_ = {evaluation, maturity, 100.0, OptionType_("PUT"), EarlyExerciseStyle_("BERMUDAN"), {maturity}};
        r.valuation_.market_ = {100.0, .2, {{maturity, .05, .03}}};
        auto& s = r.valuation_.settings_;
        s.gridPoints_ = 1601;
        s.upper_ = 400;
        s.ordinarySteps_ = 8000;
        s.maxActualSteps_ = 200000;
        s.maxWorkUnits_ = 1000000000;
        s.numericPayloadBudgetBytes_ = s.workspaceBudgetBytes_ = 1073741824;
        r.risk_.maxWorkUnits_ = 10000000000ULL;
        r.risk_.numericPayloadBudgetBytes_ = r.risk_.workspaceBudgetBytes_ = 2147483648ULL;
        return r;
    }
    void AssertAnalytic(const Dal::PDE::EarlyExercisePdeRiskResult_& result, const PdeRiskReference::Input_& input) {
        ASSERT_TRUE(result.IsComplete()) << result.Status().String() << "; base=" << result.BaseResult().Status().String();
        const auto reference = PdeRiskReference::Analytic(input);
        ASSERT_NEAR(result.Price() / 100.0, reference[0] / 100.0, 1e-4);
        const std::array<double, 5> factor{1, 100, .01, .01, .01}, tolerance{2e-3, 2e-2, 3e-3, 3e-3, 3e-3};
        ASSERT_EQ(result.Risks().size(), factor.size());
        for (size_t i = 0; i < factor.size(); ++i) {
            const auto& risk = result.Risks()[i];
            ASSERT_TRUE(risk.value_) << risk.riskId_ << " " << risk.status_.String();
            ASSERT_NEAR(*risk.value_ * factor[i], reference[i + 1] * factor[i], tolerance[i]) << risk.riskId_;
        }
    }
} // namespace

TEST(EarlyExercisePdeRiskQualificationTest, TestAnalyticReferencePutCallParityAndScalarDifferences) {
    PdeRiskReference::Input_ r;
    r.carry_ = {{137, .02, .01}, {365, .06, .03}};
    for (bool call : {false, true}) {
        r.call_ = call;
        const auto reference = PdeRiskReference::Analytic(r);
        auto low = r, high = r;
        const double h = .001;
        low.spot_ -= h;
        high.spot_ += h;
        const double minus = PdeRiskReference::Analytic(low)[0], plus = PdeRiskReference::Analytic(high)[0];
        ASSERT_NEAR((plus - minus) / (2 * h), reference[1], 5e-4);
        ASSERT_NEAR(100 * (plus - 2 * reference[0] + minus) / (h * h), 100 * reference[2], 5e-3);
        for (size_t i = 0; i < r.carry_.size(); ++i) {
            low = high = r;
            low.carry_[i].rate_ -= 1e-5;
            high.carry_[i].rate_ += 1e-5;
            ASSERT_NEAR((PdeRiskReference::Analytic(high)[0] - PdeRiskReference::Analytic(low)[0]) / .002, reference[4 + 2 * i] / 100, 7.5e-4);
        }
    }
    r.call_ = true;
    const double call = PdeRiskReference::Analytic(r)[0];
    r.call_ = false;
    const double put = PdeRiskReference::Analytic(r)[0];
    const auto integral = PdeRiskReference::Integral(r, 0, 365);
    ASSERT_NEAR(call - put, r.spot_ * std::exp(-integral.second) - r.strike_ * std::exp(-integral.first), 1e-12);
}

TEST(EarlyExercisePdeRiskQualificationTest, TestFrozenN1SmoothEuropeanAllRisksAgainstIndependentAnalytic) {
    const auto r = QualificationRequest();
    ASSERT_NO_FATAL_FAILURE(AssertAnalytic(EvaluateEarlyExercisePdeRisk(r), {}));
}

TEST(EarlyExercisePdeRiskQualificationTest, TestIndependentReferenceEuropeanPriceControls) {
    const PdeRiskReference::Input_ r;
    const double exact = PdeRiskReference::Analytic(r)[0];
    const double even = PdeRiskReference::Tree(r, 4096), odd = PdeRiskReference::Tree(r, 4097);
    ASSERT_LT(std::abs(even - odd) / 100, 2.5e-5);
    ASSERT_LT(std::abs(.5 * (even + odd) - exact) / 100, 2.5e-5);
    const auto log = PdeRiskReference::LogLcp(r, 0);
    ASSERT_LT(std::abs(log.price_ - exact) / 100, 2.5e-5);
    ASSERT_EQ(log.steps_, 365 * 16);
}

TEST(EarlyExercisePdeRiskQualificationTest, TestFrozenN5bShortExpiryRequiresAvailableGamma) {
    auto r = QualificationRequest(2);
    r.valuation_.settings_.gridPoints_ = 16001;
    r.valuation_.contract_.evaluationDate_ = Date_(2028, 2, 28);
    r.valuation_.contract_.maturityDate_ = Date_(2028, 3, 1);
    r.valuation_.contract_.exerciseDates_ = {Date_(2028, 3, 1)};
    r.valuation_.market_.carry_[0].endDate_ = Date_(2028, 3, 1);
    PdeRiskReference::Input_ input;
    input.days_ = 2;
    input.carry_[0].end_ = 2;
    input.exercise_ = {2};
    ASSERT_NO_FATAL_FAILURE(AssertAnalytic(EvaluateEarlyExercisePdeRisk(r), input));
}

TEST(EarlyExercisePdeRiskQualificationTest, TestFrozenN7FixedSpacingDomainRefinementRetainsAllRisks) {
    auto r = QualificationRequest();
    r.valuation_.settings_.gridPoints_ = 3201;
    r.valuation_.settings_.upper_ = 800;
    ASSERT_NO_FATAL_FAILURE(AssertAnalytic(EvaluateEarlyExercisePdeRisk(r), {}));
}

TEST(EarlyExercisePdeRiskQualificationTest, TestFrozenN11SubCellBumpsRejectBeforeAnyRollback) {
    for (double spot : {100.0, 100.125}) {
        auto r = QualificationRequest();
        r.valuation_.market_.spot_ = spot;
        r.risk_.spotBump_ = .125;
        size_t started = 0;
        Dal::PDE::EarlyExercisePdeRiskInternal::TestControl_ control;
        control.rollbacksStarted_ = &started;
        ASSERT_THROW(static_cast<void>(Dal::PDE::EarlyExercisePdeRiskInternal::Evaluate(r, control)), Dal::Exception_);
        ASSERT_EQ(started, 0);
    }
}

TEST(EarlyExercisePdeRiskQualificationTest, TestFrozenN12SelectorsPreserveCanonicalOrderAndSharedSpotScenarios) {
    auto r = QualificationRequest();
    r.valuation_.settings_.gridPoints_ = 41;
    r.valuation_.settings_.ordinarySteps_ = 2;
    const auto evaluation = r.valuation_.contract_.evaluationDate_, maturity = r.valuation_.contract_.maturityDate_;
    r.valuation_.market_.carry_ = {{evaluation.AddDays(137), .02, .01}, {maturity, .06, .03}};
    for (int mask = 1; mask < 16; ++mask) {
        r.risk_.delta_ = mask & 1;
        r.risk_.gamma_ = mask & 2;
        r.risk_.vega_ = mask & 4;
        r.risk_.carryRisk_ = mask & 8;
        const auto result = EvaluateEarlyExercisePdeRisk(r);
        Dal::Vector_<Dal::String_> expected;
        if (mask & 1)
            expected.push_back("Delta");
        if (mask & 2)
            expected.push_back("Gamma");
        if (mask & 4)
            expected.push_back("Vega");
        if (mask & 8)
            for (const char* id : {"Rate[0]", "DividendYield[0]", "Rate[1]", "DividendYield[1]"})
                expected.push_back(id);
        ASSERT_EQ(result.Risks().size(), expected.size());
        for (size_t i = 0; i < expected.size(); ++i)
            ASSERT_EQ(result.Risks()[i].riskId_, expected[i]);
        const size_t axes = bool(mask & 3) + bool(mask & 4) + 4 * bool(mask & 8);
        ASSERT_EQ(result.Scenarios().size(), 1 + 4 * axes);
    }
    r.risk_.delta_ = r.risk_.gamma_ = r.risk_.vega_ = r.risk_.carryRisk_ = false;
    ASSERT_THROW(static_cast<void>(EvaluateEarlyExercisePdeRisk(r)), Dal::Exception_);
    r.risk_.carryRisk_ = true;
    r.risk_.rateBumps_ = {.0001};
    ASSERT_THROW(static_cast<void>(EvaluateEarlyExercisePdeRisk(r)), Dal::Exception_);
    r.risk_.rateBumps_.clear();
    r.risk_.dividendYieldBumps_ = {.0001};
    ASSERT_THROW(static_cast<void>(EvaluateEarlyExercisePdeRisk(r)), Dal::Exception_);
}
