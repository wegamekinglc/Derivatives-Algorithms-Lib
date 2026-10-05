//
// Created by Codex on 2026/10/5.
//

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>

#include <dal/math/aad/native.hpp>
#include <dal/math/aad/recording.hpp>
#include <dal/model/ivs.hpp>
#include <dal/platform/platform.hpp>

#include <dal-public/src/calibrationriskrequest.hpp>

#include <dal-cpp/benchmarks/rate_risk_perf/quoteriskbenchfixtures.hpp>

#include "jointxccyquoteriskfixtures.hpp"

namespace {
    class FlatIVS_ final : public Dal::AAD::IVS_ {
        double volatility_;

    public:
        explicit FlatIVS_(double volatility = 0.2) : IVS_(100.0, 0.05, 0.02), volatility_(volatility) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return volatility_; }
    };

    auto DupireSource(double volatility = 0.2, double spread = 0.0) {
        const Dal::DupireRiskInputs_ inputs{{75.0, 105.0, 135.0}, {0.4, 1.2}, Dal::Matrix_<>(3, 2, spread), {60.0, 100.0, 140.0}, 10.0,
                                            {0.5, 1.0},           0.5};
        return Dal::NewCalibrationPullback(Dal::CalibrateDupireWithRisk(FlatIVS_(volatility), inputs, "request"));
    }

    template <class M_, class F_> Dal::CalibrationPullback_ CapturedSource(const M_& materials, F_ build) {
        auto config = materials.config_;
        config.retainCalibrationRecord_ = true;
        return Dal::NewCalibrationPullback(build(materials.spec_, *materials.calibration_, materials.options_, materials.market_, config));
    }

    auto JointSource(Dal::CurveJacobianMode_ mode) {
        const auto spec = JointQuoteRiskFixtures::Spec(8, 2, Dal::CurveParameterization_::Value_::LOG_DISCOUNT, true);
        const auto options = JointXccyQuoteRiskFixtures::Options(mode);
        const auto calibrated = Dal::CalibrateJointMultiCurve(spec, options);
        const auto market = JointQuoteRiskFixtures::Market(spec, calibrated);
        auto config = JointQuoteRiskFixtures::Config(2);
        config.retainCalibrationRecord_ = true;
        return Dal::NewCalibrationPullback(Dal::BuildJointMultiCurveQuoteRiskProvenance(spec, calibrated, options, market, config));
    }

    template <class F_> void AssertError(F_ action, const std::string& reason) {
        try {
            action();
            FAIL() << "Expected " << reason;
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(reason), std::string::npos) << error.what();
        }
    }
} // namespace

TEST(CalibrationRiskRequestTest, TestPlanRetainsFullPayloadForSelectedDupireQuotes) {
    const auto source = DupireSource();
    Dal::CalibrationRiskRequest_ request;
    request.inputs_ = Dal::Vector_<Dal::String_>{"quote:3", "quote:0"};
    request.reportFactors_ = Dal::Vector_<>{0.01, 0.5};
    const auto plan = Dal::PlanCalibrationRiskRequest(source, request);
    ASSERT_TRUE(plan.Calibration().Matches(source));
    ASSERT_EQ(plan.CompleteInputAxis().size(), 6);
    ASSERT_EQ(plan.InputAxis().size(), 2);
    ASSERT_EQ(plan.SelectedOrdinals(), (Dal::Vector_<size_t>{3, 0}));
    ASSERT_EQ(plan.NumericPayloadBytes(), 3 * 6 * sizeof(double));
    for (size_t ordinal = 0; ordinal < 6; ++ordinal) {
        const auto& coordinate = plan.CompleteInputAxis()[ordinal];
        ASSERT_EQ(coordinate.id_, "quote:" + Dal::String_(std::to_string(ordinal)));
        ASSERT_EQ(coordinate.ordinal_, ordinal);
        ASSERT_EQ(coordinate.row_, static_cast<int>(ordinal / 2));
        ASSERT_EQ(coordinate.column_, static_cast<int>(ordinal % 2));
        ASSERT_EQ(coordinate.nativeUnit_, "decimal-vol");
        ASSERT_EQ(coordinate.reportScale_, 1.0);
        ASSERT_EQ(coordinate.value_, 0.0);
        ASSERT_EQ(coordinate.strike_, (Dal::Vector_<>{75.0, 105.0, 135.0})[ordinal / 2]);
        ASSERT_EQ(coordinate.maturity_, (Dal::Vector_<>{0.4, 1.2})[ordinal % 2]);
        ASSERT_FALSE(coordinate.blockKey_);
        ASSERT_FALSE(coordinate.blockOrdinal_);
    }
    ASSERT_EQ(plan.InputAxis()[0].reportScale_, 0.01);
    ASSERT_EQ(plan.InputAxis()[1].reportScale_, 0.5);
    request.inputs_->clear();
    (*request.reportFactors_)[0] = 7.0;
    ASSERT_EQ(plan.InputAxis().size(), 2);
    ASSERT_EQ(plan.InputAxis()[0].id_, "quote:3");
    ASSERT_EQ(plan.InputAxis()[0].reportScale_, 0.01);
}

TEST(CalibrationRiskRequestTest, TestBudgetCountsFullContributionsForAllSubsetAndEmptyPlans) {
    const auto source = DupireSource();
    const size_t fullBytes = 3 * 6 * sizeof(double);
    ASSERT_EQ(Dal::CalibrationRiskPayloadBytes(3, 2), fullBytes);
    const std::array<std::optional<Dal::Vector_<Dal::String_>>, 3> selections{std::nullopt, Dal::Vector_<Dal::String_>{"quote:3"},
                                                                              Dal::Vector_<Dal::String_>{}};
    Dal::AAD::RecordingScope_ recording;
    Dal::AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Dal::AAD::Number_ output = input * input;
    const auto nodeCount = Dal::AAD::Tape()->nodes_.Size();
    for (const auto& selection : selections) {
        Dal::CalibrationRiskRequest_ request;
        request.inputs_ = selection;
        request.numericPayloadBudgetBytes_ = fullBytes;
        ASSERT_EQ(Dal::PlanCalibrationRiskRequest(source, request).NumericPayloadBytes(), fullBytes);
        request.numericPayloadBudgetBytes_ = fullBytes - 1;
        ASSERT_NO_FATAL_FAILURE(
            AssertError([&] { static_cast<void>(Dal::PlanCalibrationRiskRequest(source, request)); }, "CalibrationRiskBudgetExceeded"));
        ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), nodeCount);
    }
    recording.FinishRecording();
    Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
    recording.Reverse();
    ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
}

TEST(CalibrationRiskRequestTest, TestPayloadArithmeticRejectsOverflowWithoutAllocating) {
    const size_t maximum = (std::numeric_limits<size_t>::max)();
    ASSERT_THROW(static_cast<void>(Dal::CalibrationRiskPayloadBytes(0, 1)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::CalibrationRiskPayloadBytes(1, 0)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::CalibrationRiskPayloadBytes(maximum, 1)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::CalibrationRiskPayloadBytes(1, maximum)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::CalibrationRiskPayloadBytes(maximum / 24, 2)), Dal::Exception_);
    ASSERT_EQ(Dal::CalibrationRiskPayloadBytes(maximum / 24, 1), maximum / 24 * 24);
}

TEST(CalibrationRiskRequestTest, TestInvalidIdsAndFactorsFailDuringPassivePlanning) {
    const auto source = DupireSource();
    Dal::CalibrationRiskRequest_ request;
    for (const auto& ids : Dal::Vector_<Dal::Vector_<Dal::String_>>{
             {"quote:6"}, {"model:0"}, {"quote:0", "QUOTE:0"}, {Dal::String_(std::string("quote:0\0extra", 13))}}) {
        request.inputs_ = ids;
        ASSERT_THROW(static_cast<void>(Dal::PlanCalibrationRiskRequest(source, request)), Dal::Exception_);
    }
    request.inputs_ = Dal::Vector_<Dal::String_>{"quote:0"};
    request.reportFactors_ = Dal::Vector_<>{1.0, 1.0};
    ASSERT_THROW(static_cast<void>(Dal::PlanCalibrationRiskRequest(source, request)), Dal::Exception_);
    for (const double factor :
         {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()}) {
        request.reportFactors_ = Dal::Vector_<>{factor};
        ASSERT_THROW(static_cast<void>(Dal::PlanCalibrationRiskRequest(source, request)), Dal::Exception_);
    }
    request.reportFactors_ = Dal::Vector_<>{1.0};
    ASSERT_EQ(Dal::PlanCalibrationRiskRequest(source, request).InputAxis().size(), 1);
}

TEST(CalibrationRiskRequestTest, TestRawAndReportedProjectionsKeepNativeContributionsAndOrder) {
    const auto source = DupireSource();
    const auto parameters = Dal::NewCalibrationParameterAdjoints(source, Dal::Matrix_<>(source.ParameterRows(), source.ParameterCols(), -0.3));
    const auto direct = Dal::NewCalibrationDirectQuoteAdjoints(source, Dal::Matrix_<>(3, 2, -1.25));
    const auto native = Dal::PullbackCalibration(source, parameters, direct);
    Dal::CalibrationRiskRequest_ request;
    request.inputs_ = Dal::Vector_<Dal::String_>{"quote:3", "quote:0"};
    request.reportFactors_ = Dal::Vector_<>{0.01, 0.5};
    const auto plan = Dal::PlanCalibrationRiskRequest(source, request);
    const auto result = Dal::PullbackCalibrationWithRisk(plan, parameters, direct);
    ASSERT_EQ(result.Plan().SelectedOrdinals(), plan.SelectedOrdinals());
    ASSERT_EQ(result.QuoteRisk().Method(), native.Method());
    ASSERT_EQ(result.QuoteRisk().Boundary(), native.Boundary());
    ASSERT_EQ(result.QuoteRisk().Unit(), native.Unit());
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 2; ++column) {
            ASSERT_EQ(result.QuoteRisk().CalibrationAdjoints()(row, column), native.CalibrationAdjoints()(row, column));
            ASSERT_EQ(result.QuoteRisk().DirectAdjoints()(row, column), native.DirectAdjoints()(row, column));
            ASSERT_EQ(result.QuoteRisk().TotalAdjoints()(row, column), native.TotalAdjoints()(row, column));
        }
    const auto raw = result.Jacobian();
    const auto calibration = result.CalibrationJacobian();
    const auto directProjection = result.DirectJacobian();
    const auto reported = result.ReportedJacobian();
    ASSERT_EQ(raw.Rows(), 1);
    ASSERT_EQ(raw.Cols(), 2);
    for (int column = 0; column < 2; ++column) {
        const auto& coordinate = plan.InputAxis()[static_cast<size_t>(column)];
        ASSERT_EQ(raw(0, column), native.TotalAdjoints()(coordinate.row_, coordinate.column_));
        ASSERT_EQ(calibration(0, column), native.CalibrationAdjoints()(coordinate.row_, coordinate.column_));
        ASSERT_EQ(directProjection(0, column), -1.25);
        ASSERT_EQ(reported(0, column), raw(0, column) * coordinate.reportScale_);
    }
    auto detached = result.ReportedJacobian();
    detached(0, 0) = 99.0;
    ASSERT_EQ(result.ReportedJacobian()(0, 0), reported(0, 0));
    ASSERT_EQ(result.Jacobian()(0, 0), raw(0, 0));
}

TEST(CalibrationRiskRequestTest, TestAllFourCurveProvidersPreserveNativeAxesAndDv01Projection) {
    using namespace Dal::RateRiskPerf;
    for (const auto mode : {Dal::CurveJacobianMode_::Value_::ANALYTIC, Dal::CurveJacobianMode_::Value_::BUMPED}) {
        const std::array<Dal::CalibrationPullback_, 4> sources{
            CapturedSource(MakeSingleCurveProvenanceMaterials(8, mode), Dal::BuildSingleCurveQuoteRiskProvenance),
            CapturedSource(MakeJointXccyProvenanceMaterials(8, mode), Dal::BuildJointXccyQuoteRiskProvenance),
            CapturedSource(MakeStagedXccyProvenanceMaterials(8, mode), Dal::BuildStagedXccyBasisQuoteRiskProvenance), JointSource(mode)};
        for (const auto& source : sources) {
            const auto& provenance = std::get<Dal::RateQuoteRiskProvenance_>(source.Source());
            const auto complete = Dal::PlanCalibrationRiskRequest(source);
            ASSERT_EQ(complete.InputAxis().size(), provenance.Axis().quotes_.size());
            for (size_t ordinal = 0; ordinal < complete.InputAxis().size(); ++ordinal) {
                const auto& coordinate = complete.InputAxis()[ordinal];
                const auto& native = provenance.Axis().quotes_[ordinal];
                ASSERT_EQ(coordinate.ordinal_, ordinal);
                ASSERT_EQ(coordinate.row_, native.globalOrdinal_);
                ASSERT_EQ(coordinate.column_, 0);
                ASSERT_EQ(coordinate.blockKey_, native.blockKey_);
                ASSERT_EQ(coordinate.blockOrdinal_, native.blockOrdinal_);
                ASSERT_EQ(coordinate.label_, native.displayName_);
                ASSERT_EQ(coordinate.nativeUnit_, native.unit_);
                ASSERT_FALSE(coordinate.value_);
                ASSERT_FALSE(coordinate.strike_);
                ASSERT_FALSE(coordinate.maturity_);
            }
            const size_t last = complete.InputAxis().size() - 1;
            Dal::CalibrationRiskRequest_ request;
            request.inputs_ = Dal::Vector_<Dal::String_>{complete.InputAxis()[last].id_, "quote:0"};
            request.reportFactors_ = Dal::Vector_<>{1e-4, 1e-4};
            const auto plan = Dal::PlanCalibrationRiskRequest(source, request);
            Dal::Matrix_<> gradient(source.ParameterRows(), 1);
            for (int row = 0; row < gradient.Rows(); ++row)
                gradient(row, 0) = row % 2 == 0 ? 0.3 : -0.7;
            const auto parameters = Dal::NewCalibrationParameterAdjoints(source, gradient);
            const auto direct = Dal::NewCalibrationDirectQuoteAdjoints(source, Dal::Matrix_<>(source.QuoteRows(), 1, -1.25));
            const auto result = Dal::PullbackCalibrationWithRisk(plan, parameters, direct);
            const auto raw = result.Jacobian();
            const auto reported = result.ReportedJacobian();
            for (int column = 0; column < 2; ++column) {
                const auto ordinal = static_cast<int>(plan.SelectedOrdinals()[static_cast<size_t>(column)]);
                double expected = 0.0;
                for (int row = 0; row < gradient.Rows(); ++row)
                    expected += gradient(row, 0) * provenance.EffectiveInverse()(row, ordinal);
                expected /= provenance.Tolerance();
                ASSERT_NEAR(result.CalibrationJacobian()(0, column), expected, 1e-10 * std::max(1.0, std::abs(expected)));
                ASSERT_EQ(result.DirectJacobian()(0, column), -1.25);
                ASSERT_NEAR(raw(0, column), expected - 1.25, 1e-10 * std::max(1.0, std::abs(expected)));
                ASSERT_EQ(reported(0, column), raw(0, column) * 1e-4);
            }
            ASSERT_EQ(plan.NumericPayloadBytes(), 3 * provenance.Axis().quotes_.size() * sizeof(double));
        }
    }
}

TEST(CalibrationRiskRequestTest, TestEmptySelectionStillPerformsNativeDupirePullback) {
    const auto source = DupireSource();
    const auto parameters = Dal::NewCalibrationParameterAdjoints(source, Dal::Matrix_<>(source.ParameterRows(), source.ParameterCols(), -0.3));
    Dal::CalibrationRiskRequest_ request;
    request.inputs_ = Dal::Vector_<Dal::String_>();
    const auto plan = Dal::PlanCalibrationRiskRequest(source, request);
    {
        Dal::AAD::RecordingScope_ recording;
        Dal::AAD::Number_ input;
        recording.RegisterInput(input, 2.0);
        recording.StartRecording();
        Dal::AAD::Number_ output = input * input;
        recording.FinishRecording();
        const auto nodeCount = Dal::AAD::Tape()->nodes_.Size();
        ASSERT_NO_FATAL_FAILURE(AssertError([&] { static_cast<void>(Dal::PullbackCalibrationWithRisk(plan, parameters)); }, "independent scope"));
        ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), nodeCount);
        Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
        recording.Reverse();
        ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
        recording.Close();
    }
    const auto result = Dal::PullbackCalibrationWithRisk(plan, parameters);
    const auto native = Dal::PullbackCalibration(source, parameters);
    ASSERT_EQ(result.Jacobian().Rows(), 1);
    ASSERT_EQ(result.Jacobian().Cols(), 0);
    ASSERT_EQ(result.ReportedJacobian().Cols(), 0);
    ASSERT_EQ(result.QuoteRisk().Method(), native.Method());
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 2; ++column) {
            ASSERT_EQ(result.QuoteRisk().CalibrationAdjoints()(row, column), native.CalibrationAdjoints()(row, column));
            ASSERT_EQ(result.QuoteRisk().DirectAdjoints()(row, column), 0.0);
        }
}

TEST(CalibrationRiskRequestTest, TestParameterSourcesAndDirectQuoteIdentityStayDistinct) {
    const auto source = DupireSource();
    const auto other = DupireSource(0.21);
    const auto plan = Dal::PlanCalibrationRiskRequest(source);
    const auto otherPlan = Dal::PlanCalibrationRiskRequest(other);
    ASSERT_FALSE(source.Matches(other));
    ASSERT_EQ(plan.InputAxis()[0].id_, otherPlan.InputAxis()[0].id_);
    ASSERT_EQ(plan.InputAxis()[0].strike_, otherPlan.InputAxis()[0].strike_);
    const auto parameters = Dal::NewCalibrationParameterAdjoints(source, Dal::Matrix_<>(source.ParameterRows(), source.ParameterCols(), 0.0));
    const auto otherParameters = Dal::NewCalibrationParameterAdjoints(other, Dal::Matrix_<>(other.ParameterRows(), other.ParameterCols(), 0.0));
    const auto otherDirect = Dal::NewCalibrationDirectQuoteAdjoints(other, Dal::Matrix_<>(3, 2, -1.25));
    const auto changedQuotes = DupireSource(0.2, 0.001);
    const auto wrongDirect = Dal::NewCalibrationDirectQuoteAdjoints(changedQuotes, Dal::Matrix_<>(3, 2, -1.25));
    ASSERT_THROW(static_cast<void>(Dal::PullbackCalibrationWithRisk(plan, otherParameters)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::PullbackCalibrationWithRisk(plan, parameters, wrongDirect)), Dal::Exception_);
    ASSERT_EQ(Dal::PullbackCalibrationWithRisk(plan, parameters, otherDirect).Jacobian()(0, 0), -1.25);
    ASSERT_EQ(Dal::PullbackCalibrationWithRisk(plan, parameters).Jacobian()(0, 0), 0.0);
}

TEST(CalibrationRiskRequestTest, TestReportedCancellationCannotHideContributionOverflow) {
    const auto source = DupireSource();
    const auto parameters = Dal::NewCalibrationParameterAdjoints(source, Dal::Matrix_<>(source.ParameterRows(), source.ParameterCols(), 100.0));
    const auto native = Dal::PullbackCalibration(source, parameters);
    auto directMatrix = native.CalibrationAdjoints();
    int selected = -1;
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 2; ++column) {
            const double value = directMatrix(row, column);
            directMatrix(row, column) = -value;
            if (std::abs(value) > 1.0)
                selected = row * 2 + column;
        }
    ASSERT_GE(selected, 0);
    const auto direct = Dal::NewCalibrationDirectQuoteAdjoints(source, directMatrix);
    Dal::CalibrationRiskRequest_ request;
    request.inputs_ = Dal::Vector_<Dal::String_>{"quote:" + Dal::String::FromInt(selected)};
    const auto goodPlan = Dal::PlanCalibrationRiskRequest(source, request);
    const auto good = Dal::PullbackCalibrationWithRisk(goodPlan, parameters, direct);
    ASSERT_EQ(good.Jacobian()(0, 0), 0.0);
    request.reportFactors_ = Dal::Vector_<>{(std::numeric_limits<double>::max)()};
    const auto excessive = Dal::PlanCalibrationRiskRequest(source, request);
    ASSERT_NO_FATAL_FAILURE(
        AssertError([&] { static_cast<void>(Dal::PullbackCalibrationWithRisk(excessive, parameters, direct)); }, "non-finite reported contribution"));
    ASSERT_EQ(good.ReportedJacobian()(0, 0), 0.0);
    ASSERT_EQ(Dal::PullbackCalibrationWithRisk(goodPlan, parameters, direct).Jacobian()(0, 0), 0.0);
}

TEST(CalibrationRiskRequestTest, TestDetachedOwnedResultGettersArePassiveDuringUnrelatedRecording) {
    const auto result = [] {
        const auto source = DupireSource();
        const auto plan = Dal::PlanCalibrationRiskRequest(source);
        Dal::Matrix_<> matrix(source.ParameterRows(), source.ParameterCols(), -0.3);
        const auto parameters = Dal::NewCalibrationParameterAdjoints(source, matrix);
        for (int row = 0; row < matrix.Rows(); ++row)
            for (int column = 0; column < matrix.Cols(); ++column)
                matrix(row, column) = 0.0;
        return Dal::PullbackCalibrationWithRisk(plan, parameters);
    }();
    const auto expected = result.Jacobian();
    Dal::AAD::RecordingScope_ recording;
    Dal::AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Dal::AAD::Number_ output = input * input;
    const auto nodeCount = Dal::AAD::Tape()->nodes_.Size();
    const auto raw = result.Jacobian();
    const auto reported = result.ReportedJacobian();
    const auto calibration = result.CalibrationJacobian();
    const auto direct = result.DirectJacobian();
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), nodeCount);
    for (int column = 0; column < raw.Cols(); ++column) {
        ASSERT_EQ(raw(0, column), expected(0, column));
        ASSERT_EQ(reported(0, column), raw(0, column));
        ASSERT_EQ(calibration(0, column), raw(0, column));
        ASSERT_EQ(direct(0, column), 0.0);
    }
    recording.FinishRecording();
    Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
    recording.Reverse();
    ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
}
