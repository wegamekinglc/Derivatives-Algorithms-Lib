//
// Created by Codex on 2026/10/10.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>

#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/model/ivs.hpp>
#include <dal/platform/initall.hpp>

#include <dal-public/src/dupirecurvature.hpp>
#include <dal-public/src/dupirerisk.hpp>
#include <dal-public/src/dupireriskrequest.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>
#include <dal-public/test-support/dupirecurvaturefixtures.hpp>

#include <script_test_observers.hpp>

namespace Fixture = Dal::Script::TestSupport::DupireCurvature;
using Fixture::Calibration;
using Fixture::MixedProduct;
using Fixture::Model;
using Fixture::Product;
using Fixture::RiskRequest;
using Fixture::SingleWorker_;

namespace {
    auto CurvatureRequest() {
        Dal::DupireScriptCurvatureRequest_ request;
        request.risk_ = RiskRequest();
        request.bumps_.directions_ = Dal::Matrix_<>(1, 6, 0.0);
        request.bumps_.directions_(0, 3) = 1.0;
        request.bumps_.steps_ = {2e-4};
        return request;
    }

    auto DirectProduct() {
        return Dal::NewScriptProduct("direct", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2027, 9, 12))}, {"0.001", "pay PAYS QUOTE * QUOTE"});
    }

    struct RejectSubmission_ final : Dal::Script::Detail::SimulationObserver_ {
        size_t calls_ = 0;
        size_t failAt_;
        explicit RejectSubmission_(size_t failAt) : failAt_(failAt) {}
        void AfterSubmission() override {
            if (++calls_ == failAt_)
                THROW("injected curvature submission failure");
        }
    };

    Dal::Vector_<> FrozenMapGradient(const Dal::DupireCalibrationSnapshot_& original, const Dal::Matrix_<>& spreads) {
        const auto fresh = Dal::RecalibrateDupireWithRisk(original, spreads);
        const auto request = RiskRequest();
        const auto valuation =
            Dal::ValueByMonteCarloWithRisk(MixedProduct(spreads(1, 1)), Model(fresh), request.numPaths_, {}, request.valuation_, request.simulation_);
        const auto parameters = Dal::ExtractDupireParameterAdjoints(valuation, fresh, "Z_LOCAL");
        Dal::Matrix_<> direct(3, 2, 0.0);
        direct(1, 1) = valuation.Jacobian()(0, valuation.Jacobian().Cols() - 1);
        const auto frozen =
            Dal::PullbackDupireCalibration(original, {original, parameters.adjoints_}, Dal::DupireDirectQuoteAdjoints_{original, direct});
        return Dal::Vector_<>(frozen.TotalAdjoints().begin(), frozen.TotalAdjoints().end());
    }

    // Generated from passive long-double call stencils at the declared steps.
    constexpr double PASSIVE_REFERENCE[6][6] = {
        {-18.881846930440815, 13.131843035196766, -101.33567793779719, -66.193071823761329, 56.462213482433299, 36.90224015251431},
        {-18.882032648548375, 13.131846987590734, -101.3351012879582, -66.19318817513431, 56.462033715121152, 36.902195876820088},
        {-18.882190389035713, 13.131306175750979, -101.33536676448784, -66.193831749217225, 56.461965414200677, 36.902146938189162},
        {-33.040425417851793, 9.8206635268383025, -68.720003731925772, -63.958467366731497, 27.253390966208713, 32.860831566949855},
        {-33.040455083011011, 9.8204436582705057, -68.719594015220764, -63.95814198256744, 27.253409573546605, 32.860774279441785},
        {-33.040574543008461, 9.8203829068665982, -68.719190693400378, -63.95892917510082, 27.253386747361219, 32.860897469788597}};
} // namespace

TEST(DupireScriptCurvatureTest, TestRebuildUpdatesDirectConstantAndPreservesOtherModelCoordinates) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    const auto plan = Dal::PlanDupireScriptRisk(Product(), Model(calibration), calibration, "Z_LOCAL", RiskRequest());
    auto spreads = calibration.Inputs().quoteSpreads_;
    spreads(1, 1) = 0.00123456789123456;
    const auto rebuilt = Dal::RecalibrateDupireScriptRisk(plan, spreads);
    const auto& before = plan.CompleteInputAxis();
    const auto& after = rebuilt.CompleteInputAxis();
    ASSERT_EQ(before.size(), after.size());
    ASSERT_DOUBLE_EQ(before.back().value_, 0.001);
    ASSERT_DOUBLE_EQ(after.back().value_, spreads(1, 1));
    ASSERT_EQ(rebuilt.DirectBindings()[0].constantOrdinal_, 0);
    const auto fresh = Dal::RecalibrateDupireWithRisk(calibration, spreads);
    const auto expected =
        Dal::PlanDupireScriptRisk(Dal::NewScriptProduct("curvature", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                                                        {"0.00123456789123456", "pay PAYS FIX(EQ[LOCAL]) + 0.1 * FIX(EQ[OTHER]) + QUOTE * QUOTE"}),
                                  Model(fresh), fresh, "Z_LOCAL", RiskRequest());
    for (size_t column = 0; column < after.size(); ++column) {
        ASSERT_EQ(after[column].id_, before[column].id_);
        ASSERT_DOUBLE_EQ(after[column].value_, expected.CompleteInputAxis()[column].value_);
    }
}

TEST(DupireScriptCurvatureTest, TestRebuildFreezesGlobalHistoryForLaterQuotePoints) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    const auto product =
        Dal::NewScriptProduct("history", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2026, 9, 11)), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                              {"0.001", "past = FIX(EQ[CURVATURE_HISTORY])", "pay PAYS past + QUOTE * QUOTE"});
    Dal::Script::TestSupport::StoreScriptTestFixing("EQ[CURVATURE_HISTORY]", 80.0);
    const auto original = Dal::PlanDupireScriptRisk(product, Model(calibration), calibration, "Z_LOCAL", RiskRequest());
    Dal::Script::TestSupport::FixingReadCounter_ reads;
    const Dal::Detail::ScopedFixingReadObserver_ observe(&reads);
    const auto frozen = Dal::RecalibrateDupireScriptRisk(original, calibration.Inputs().quoteSpreads_);
    ASSERT_TRUE(frozen.ValuationSettings().fixings_);
    ASSERT_EQ(reads.histories_, 1);
    Dal::Script::TestSupport::StoreScriptTestFixing("EQ[CURVATURE_HISTORY]", 90.0);
    auto spreads = calibration.Inputs().quoteSpreads_;
    spreads(1, 1) += 0.0001;
    const auto next = Dal::RecalibrateDupireScriptRisk(frozen, spreads);
    ASSERT_EQ(reads.histories_, 1);
    const auto fixing = next.ValuationSettings().fixings_->Find("EQ[CURVATURE_HISTORY]", Dal::DateTime_(Dal::Date_(2026, 9, 11), 0.0));
    ASSERT_TRUE(fixing);
    ASSERT_DOUBLE_EQ(*fixing, 80.0);
}

TEST(DupireScriptCurvatureTest, TestDirectQuadraticHasAnalyticRawGamma) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    const auto product =
        Dal::NewScriptProduct("direct", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2027, 9, 12))}, {"0.001", "pay PAYS QUOTE * QUOTE"});
    Dal::DupireScriptCurvatureRequest_ request;
    request.risk_ = RiskRequest();
    request.bumps_.directions_ = Dal::Matrix_<>(1, 6, 0.0);
    request.bumps_.directions_(0, 3) = 1.0;
    request.bumps_.steps_ = {0.0001};
    for (const bool compiled : {false, true}) {
        request.risk_.simulation_.compiled_ = compiled;
        const auto plan = Dal::PlanDupireScriptCurvature(product, Model(calibration), calibration, "Z_LOCAL", request);
        const auto result = Dal::ValueByMonteCarloWithDupireCurvature(plan);
        ASSERT_EQ(result.HessianProducts().Rows(), 1);
        ASSERT_EQ(result.HessianProducts().Cols(), 6);
        ASSERT_EQ(result.InputAxis().size(), 6);
        ASSERT_EQ(result.Execution().quoteGradientEvaluations_, 3);
        ASSERT_EQ(result.Execution().pathsPerEvaluation_, 17);
        ASSERT_NEAR(result.Base().Valuation().Values()[0], 1e-6 * std::exp(-0.05), 1e-14);
        for (int column = 0; column < 6; ++column)
            ASSERT_NEAR(result.HessianProducts()(0, column), column == 3 ? 2.0 * std::exp(-0.05) : 0.0, 1e-10);
    }
}

TEST(DupireScriptCurvatureTest, TestFlatAndNonflatMixedCurvatureMatchesIndependentPassivePrices) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const Dal::Vector_<> direction{0.3, -0.2, 1.0, 0.4, -0.1, 0.2};
    for (const bool nonflat : {false, true}) {
        const auto calibration =
            nonflat ? Dal::CalibrateDupireWithRisk(Dal::AAD::MertonIVS_(100.0, 0.2, 0.08, -0.1, 0.15), Calibration().Inputs()) : Calibration();
        for (int stepIndex = 0; stepIndex < 3; ++stepIndex) {
            const double step = std::array<double, 3>{4e-4, 2e-4, 1e-4}[stepIndex];
            Dal::DupireScriptCurvatureRequest_ request;
            request.risk_ = RiskRequest();
            request.bumps_.directions_ = Dal::Matrix_<>(1, 6);
            std::copy(direction.begin(), direction.end(), request.bumps_.directions_.Data());
            request.bumps_.steps_ = {step};
            const auto plan = Dal::PlanDupireScriptCurvature(MixedProduct(0.001), Model(calibration), calibration, "Z_LOCAL", request);
            const auto actual = Dal::ValueByMonteCarloWithDupireCurvature(plan);
            const auto& independent = PASSIVE_REFERENCE[(nonflat ? 3 : 0) + stepIndex];
            for (int column = 0; column < 6; ++column) {
                std::cout << std::setprecision(17) << "oracle " << nonflat << ' ' << step << ' ' << column << ' '
                          << actual.HessianProducts()(0, column) << ' ' << independent[column] << '\n';
                ASSERT_NEAR(actual.HessianProducts()(0, column), independent[column], 0.15 + 0.02 * std::abs(independent[column]));
            }
        }
    }
}

TEST(DupireScriptCurvatureTest, TestSelectedReportedBaseKeepsFullRawSignedProducts) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    auto request = CurvatureRequest();
    request.bumps_.directions_ = Dal::Matrix_<>(3, 6, 0.0);
    request.bumps_.directions_(0, 3) = 1.0;
    request.bumps_.directions_(1, 3) = -2.0;
    request.bumps_.directions_(2, 0) = 1.0;
    request.bumps_.steps_ = {2e-4, 1e-4, 2e-4};
    const auto full = Dal::ValueByMonteCarloWithDupireCurvature(
        Dal::PlanDupireScriptCurvature(DirectProduct(), Model(calibration), calibration, "Z_LOCAL", request));
    request.risk_.quotes_.inputs_ = Dal::Vector_<Dal::String_>{"quote:3", "quote:0"};
    request.risk_.quotes_.reportFactors_ = Dal::Vector_<>{0.01, 0.5};
    const auto selected = Dal::ValueByMonteCarloWithDupireCurvature(
        Dal::PlanDupireScriptCurvature(DirectProduct(), Model(calibration), calibration, "Z_LOCAL", request));
    ASSERT_EQ(selected.Base().QuoteRisk().Plan().InputAxis().size(), 2);
    ASSERT_EQ(selected.InputAxis().size(), 6);
    ASSERT_EQ(selected.Execution().quoteGradientEvaluations_, 7);
    ASSERT_DOUBLE_EQ(selected.Base().QuoteRisk().ReportedJacobian()(0, 0), 0.01 * selected.Gradient()[3]);
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 6; ++column)
            ASSERT_DOUBLE_EQ(selected.HessianProducts()(row, column), full.HessianProducts()(row, column));
    ASSERT_NEAR(selected.HessianProducts()(1, 3), -2.0 * selected.HessianProducts()(0, 3), 1e-10);
    ASSERT_DOUBLE_EQ(selected.HessianProducts()(2, 3), 0.0);
}

TEST(DupireScriptCurvatureTest, TestMalformedBumpsAndUnsupportedLimitsRejectBeforeHistoryAndWorkers) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    const auto product =
        Dal::NewScriptProduct("history", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2026, 9, 11)), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                              {"0.001", "past = FIX(EQ[CURVATURE_ADMISSION])", "pay PAYS past + QUOTE * QUOTE"});
    Dal::Script::TestSupport::RejectFixingReads_ history;
    Dal::Script::TestSupport::RejectSubmissions_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&history);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    const auto valid = CurvatureRequest();
    auto check = [&](const Dal::DupireScriptCurvatureRequest_& request) {
        ASSERT_THROW(static_cast<void>(Dal::PlanDupireScriptCurvature(product, model, calibration, "Z_LOCAL", request)), Dal::Exception_);
        ASSERT_EQ(history.historyCalls_, 0);
        ASSERT_EQ(history.fixingCalls_, 0);
        ASSERT_EQ(workers.calls_, 0);
    };
    auto invalid = valid;
    invalid.bumps_.directions_ = Dal::Matrix_<>(1, 5, 1.0);
    ASSERT_NO_FATAL_FAILURE(check(invalid));
    invalid = valid;
    invalid.bumps_.steps_.clear();
    ASSERT_NO_FATAL_FAILURE(check(invalid));
    invalid = valid;
    invalid.bumps_.directions_(0, 3) = 0.0;
    ASSERT_NO_FATAL_FAILURE(check(invalid));
    for (const double bad :
         {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::denorm_min()}) {
        invalid = valid;
        invalid.bumps_.steps_[0] = bad;
        ASSERT_NO_FATAL_FAILURE(check(invalid));
    }
    invalid = valid;
    invalid.bumps_.directions_(0, 3) = std::numeric_limits<double>::quiet_NaN();
    ASSERT_NO_FATAL_FAILURE(check(invalid));
    invalid = valid;
    invalid.bumps_.steps_[0] = 1.0;
    ASSERT_NO_FATAL_FAILURE(check(invalid));
    invalid = valid;
    invalid.bumps_.recordingCapacityBudgetBytes_ = 0;
    ASSERT_NO_FATAL_FAILURE(check(invalid));
    invalid = valid;
    invalid.bumps_.numericPayloadBudgetBytes_ = 0;
    ASSERT_NO_FATAL_FAILURE(check(invalid));
    invalid = valid;
    invalid.risk_.direct_ = Dal::NewCalibrationDirectQuoteAdjoints(Dal::NewCalibrationPullback(calibration), Dal::Matrix_<>(3, 2, 0.0));
    ASSERT_NO_FATAL_FAILURE(check(invalid));
}

TEST(DupireScriptCurvatureTest, TestCombinedBudgetAndEmptyRowsRetainSealedOwningResults) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    auto request = CurvatureRequest();
    request.bumps_.directions_ = Dal::Matrix_<>(0, 6);
    request.bumps_.steps_.clear();
    const auto firstOrder = Dal::PlanDupireScriptRisk(DirectProduct(), Model(calibration), calibration, "Z_LOCAL", request.risk_);
    const size_t bytes = Dal::AAD::BumpOverAADPayloadBytes(6, 0) + firstOrder.NumericPayloadBytes();
    request.bumps_.numericPayloadBudgetBytes_ = bytes - 1;
    ASSERT_THROW(static_cast<void>(Dal::PlanDupireScriptCurvature(DirectProduct(), Model(calibration), calibration, "Z_LOCAL", request)),
                 Dal::Exception_);
    request.bumps_.numericPayloadBudgetBytes_ = bytes;
    auto product = DirectProduct();
    const auto model = Model(calibration);
    const auto hybrid = Dal::handle_cast<Dal::HybridModelData_>(model);
    const auto mutableModel =
        std::make_shared<Dal::HybridModelData_>(hybrid->Name(), hybrid->domesticCurrency_, hybrid->components_, hybrid->correlation_);
    const auto plan = Dal::PlanDupireScriptCurvature(product, Dal::Handle_<Dal::ModelData_>(mutableModel), calibration, "Z_LOCAL", request);
    ASSERT_EQ(plan.NumericPayloadBytes(), bytes);
    request.risk_.numPaths_ = 0;
    request.risk_.directBindings_.clear();
    product = Dal::NewScriptProduct("changed", {Dal::Cell_(Dal::Date_(2027, 9, 12))}, {"pay PAYS 1000"});
    mutableModel->components_.clear();
    const auto result = Dal::ValueByMonteCarloWithDupireCurvature(plan);
    ASSERT_EQ(result.Execution().numericPayloadBytes_, bytes);
    ASSERT_EQ(result.Execution().quoteGradientEvaluations_, 1);
    ASSERT_EQ(result.HessianProducts().Rows(), 0);
    ASSERT_EQ(result.HessianProducts().Cols(), 6);
    ASSERT_EQ(result.Gradient().size(), 6);
    ASSERT_NEAR(result.Gradient()[3], 0.002 * std::exp(-0.05), 1e-12);
    const auto retained = [&] {
        auto localRequest = CurvatureRequest();
        const auto localCalibration = Calibration();
        return Dal::ValueByMonteCarloWithDupireCurvature(
            Dal::PlanDupireScriptCurvature(DirectProduct(), Model(localCalibration), localCalibration, "Z_LOCAL", localRequest));
    }();
    ASSERT_NEAR(retained.HessianProducts()(0, 3), 2.0 * std::exp(-0.05), 1e-10);
    ASSERT_DOUBLE_EQ(result.Point()[3], 0.001);
}

TEST(DupireScriptCurvatureTest, TestWorkerFailureAtEveryGradientRestoresWideModeAndRecovers) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto mode = Dal::AAD::SetNumResultsForAAD(true, 4);
    const auto calibration = Calibration();
    const auto plan = Dal::PlanDupireScriptCurvature(DirectProduct(), Model(calibration), calibration, "Z_LOCAL", CurvatureRequest());
    for (const size_t phase : {1u, 2u, 3u}) {
        {
            RejectSubmission_ rejected(phase);
            const Dal::Script::Detail::ScopedSimulationObserver_ observe(&rejected);
            try {
                static_cast<void>(Dal::ValueByMonteCarloWithDupireCurvature(plan));
                FAIL() << "expected injected worker failure";
            } catch (const Dal::Exception_& error) {
                const std::string message(error.what());
                ASSERT_NE(message.find("DupireScriptCurvature: execution"), std::string::npos);
                ASSERT_NE(message.find(phase == 1 ? "base" : phase == 2 ? "plus" : "minus"), std::string::npos);
            }
        }
        ASSERT_TRUE(Dal::AAD::Tape()->multi_);
        ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 4);
        const auto recovered = Dal::ValueByMonteCarloWithDupireCurvature(plan);
        ASSERT_NEAR(recovered.HessianProducts()(0, 3), 2.0 * std::exp(-0.05), 1e-10);
        ASSERT_TRUE(Dal::AAD::Tape()->multi_);
        ASSERT_EQ(Dal::AAD::Tape()->numAdj_, 4);
    }
}

TEST(DupireScriptCurvatureTest, TestExerciseAndNestedRequestsRejectWithoutDamagingOuterGraph) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    const auto request = CurvatureRequest();
    Dal::Script::ScriptProductSettings_ settings;
    settings.defaultIndex_ = "EQ[LOCAL]";
    const auto exercise = Dal::NewScriptProduct("exercise", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                                                {"0.001", "EXERCISE MAX(100 - FIX(EQ[LOCAL]) + QUOTE, 0)"}, settings);
    {
        Dal::Script::TestSupport::RejectSubmissions_ workers;
        const Dal::Script::Detail::ScopedSimulationObserver_ observe(&workers);
        try {
            static_cast<void>(Dal::PlanDupireScriptCurvature(exercise, model, calibration, "Z_LOCAL", request));
            FAIL() << "expected exercise-policy rejection";
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find("exercise-policy"), std::string::npos) << error.what();
        }
        ASSERT_EQ(workers.calls_, 0);
    }
    const auto plan = Dal::PlanDupireScriptCurvature(DirectProduct(), model, calibration, "Z_LOCAL", request);
    Dal::AAD::RecordingScope_ outer;
    Dal::AAD::Number_ x;
    outer.RegisterInput(x, 2.0);
    outer.StartRecording();
    const Dal::AAD::Number_ square = x * x;
    ASSERT_THROW(static_cast<void>(Dal::PlanDupireScriptCurvature(DirectProduct(), model, calibration, "Z_LOCAL", request)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::ValueByMonteCarloWithDupireCurvature(plan)), Dal::Exception_);
    outer.FinishRecording();
    Dal::AAD::Adjoint(square) = 1.0;
    outer.Reverse();
    ASSERT_DOUBLE_EQ(Dal::AAD::Adjoint(x), 4.0);
    outer.Close();
}

TEST(DupireScriptCurvatureTest, TestFrozenCalibrationMapCannotReproduceFullQuoteCurvature) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Dal::CalibrateDupireWithRisk(Dal::AAD::MertonIVS_(100.0, 0.2, 0.08, -0.1, 0.15), Calibration().Inputs());
    const Dal::Vector_<> direction{0.3, -0.2, 1.0, 0.4, -0.1, 0.2};
    auto request = CurvatureRequest();
    std::copy(direction.begin(), direction.end(), request.bumps_.directions_.Data());
    const auto actual = Dal::ValueByMonteCarloWithDupireCurvature(
        Dal::PlanDupireScriptCurvature(MixedProduct(0.001), Model(calibration), calibration, "Z_LOCAL", request));
    auto plus = calibration.Inputs().quoteSpreads_, minus = plus;
    for (size_t column = 0; column < direction.size(); ++column) {
        plus.Data()[column] += 2e-4 * direction[column];
        minus.Data()[column] -= 2e-4 * direction[column];
    }
    const auto up = FrozenMapGradient(calibration, plus), down = FrozenMapGradient(calibration, minus);
    double error = 0.0;
    for (int column = 0; column < 6; ++column)
        error = std::max(error, std::abs(actual.HessianProducts()(0, column) - (up[column] - down[column]) / 4e-4));
    std::cout << "frozen-map max-error " << error << '\n';
    ASSERT_GT(error, 1.0);
}

TEST(DupireScriptCurvatureTest, TestFullyExpiredContractReturnsZeroWithoutHistoryOrWorkers) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    auto request = CurvatureRequest();
    request.risk_.valuation_.evaluationDate_ = Dal::Date_(2030, 1, 1);
    Dal::Script::TestSupport::RejectFixingReads_ history;
    Dal::Script::TestSupport::RejectSubmissions_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&history);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    const auto result = Dal::ValueByMonteCarloWithDupireCurvature(
        Dal::PlanDupireScriptCurvature(DirectProduct(), Model(calibration), calibration, "Z_LOCAL", request));
    ASSERT_DOUBLE_EQ(result.Base().Valuation().Values()[0], 0.0);
    for (const double product : result.HessianProducts())
        ASSERT_DOUBLE_EQ(product, 0.0);
    for (const double derivative : result.Gradient())
        ASSERT_DOUBLE_EQ(derivative, 0.0);
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
    ASSERT_EQ(workers.calls_, 0);
}
