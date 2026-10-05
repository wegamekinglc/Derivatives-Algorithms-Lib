//
// Created by Codex on 2026/10/5.
//

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <dal/concurrency/threadpool.hpp>
#include <dal/math/aad/native.hpp>
#include <dal/model/ivs.hpp>
#include <dal/platform/initall.hpp>

#include <dal-public/src/dupirerisk.hpp>
#include <dal-public/src/dupireriskinternal.hpp>
#include <dal-public/src/dupireriskrequest.hpp>
#include <dal-public/src/models.hpp>
#include <dal-public/src/script.hpp>

#include <script_test_observers.hpp>

namespace {
    template <class T_> class CustomArchive_ final : public T_ {
        int* writes_;

    public:
        template <class... A_> CustomArchive_(int* writes, A_&&... args) : T_(std::forward<A_>(args)...), writes_(writes) {}
        void Write(Dal::Archive::Store_&) const override {
            ++*writes_;
            THROW("unexpected custom model archive");
        }
    };

    class CustomSurface_ final : public Dal::LocalVolSurfaceData_ {
        int* writes_;

    public:
        CustomSurface_(const Dal::LocalVolSurfaceData_& source, int* writes)
            : LocalVolSurfaceData_(source.Name(), source.spots_, source.times_, source.vols_), writes_(writes) {}
        void Write(Dal::Archive::Store_&) const override {
            ++*writes_;
            THROW("unexpected custom surface archive");
        }
    };

    struct SingleWorker_ {
        Dal::ThreadPool_* pool_ = Dal::ThreadPool_::GetInstance();
        size_t threads_ = pool_->NumThreads();
        bool active_ = pool_->IsActive();
        SingleWorker_() { pool_->Start(1, true); }
        ~SingleWorker_() {
            pool_->Start(threads_, true);
            if (!active_)
                pool_->Stop();
        }
    };

    template <class F_> void AssertError(F_ action, const char* token) {
        try {
            action();
            FAIL() << "expected error for " << token;
        } catch (const Dal::Exception_& error) {
            ASSERT_NE(std::string(error.what()).find(token), std::string::npos) << error.what();
        }
    }

    class FlatIVS_ final : public Dal::AAD::IVS_ {
        double vol_;

    public:
        explicit FlatIVS_(double vol = 0.2) : IVS_(100.0, 0.05, 0.02), vol_(vol) {}
        [[nodiscard]] double ImpliedVol(double, double) const override { return vol_; }
    };

    auto Calibration() {
        const Dal::DupireRiskInputs_ inputs{{75.0, 105.0, 135.0}, {0.4, 1.2}, Dal::Matrix_<>(3, 2, 0.0), {60.0, 100.0, 140.0}, 10.0, {0.5, 1.0}, 0.5};
        return Dal::CalibrateDupireWithRisk(FlatIVS_(), inputs, "automatic");
    }

    auto Model(const Dal::DupireCalibrationSnapshot_& calibration) {
        Dal::HybridSettings_ settings;
        settings.domesticCurrency_ = "USD";
        settings.components_ = {Dal::NewHybridLocalVolEquityData("Z_LOCAL", "EQ[LOCAL]", "USD", "F_LOCAL", calibration.Spot(),
                                                                 calibration.DividendYield(), calibration.Surface(), 0.25),
                                Dal::NewHybridDeterministicRateData("00_RATE", "USD", calibration.Rate()),
                                Dal::NewHybridBSEquityData("A_OTHER", "EQ[OTHER]", "USD", "F_OTHER", 120.0, 0.25, 0.01)};
        Dal::Matrix_<> correlation(2, 2, 0.0);
        correlation(0, 0) = correlation(1, 1) = 1.0;
        settings.correlation_ = Dal::NewHybridConstantCorrelationData("correlation", {"F_LOCAL", "F_OTHER"}, correlation);
        return Dal::NewHybridModelData("unsorted", settings);
    }

    auto Product() {
        return Dal::NewScriptProduct("automatic", {Dal::Cell_("QUOTE"), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                                     {"0", "pay PAYS FIX(EQ[LOCAL]) * FIX(EQ[LOCAL]) / 100 + 0.1 * FIX(EQ[OTHER]) + 3 * QUOTE"});
    }

    struct MutableHybrid_ {
        std::shared_ptr<Dal::LocalVolSurfaceData_> surface_;
        std::shared_ptr<Dal::HybridLocalVolEquityData_> local_;
        std::shared_ptr<Dal::HybridConstantCorrelationData_> correlation_;
        std::shared_ptr<Dal::HybridModelData_> model_;

        explicit MutableHybrid_(const Dal::DupireCalibrationSnapshot_& calibration) {
            const auto original = Dal::handle_cast<Dal::HybridModelData_>(Model(calibration));
            const auto& surface = *calibration.Surface();
            surface_ = std::make_shared<Dal::LocalVolSurfaceData_>(surface.Name(), surface.spots_, surface.times_, surface.vols_);
            local_ = std::make_shared<Dal::HybridLocalVolEquityData_>("Z_LOCAL", "EQ[LOCAL]", "USD", "F_LOCAL", calibration.Spot(),
                                                                      calibration.DividendYield(), Dal::Handle_<Dal::LocalVolSurfaceData_>(surface_),
                                                                      0.25);
            const auto originalCorrelation = Dal::handle_cast<Dal::HybridConstantCorrelationData_>(original->correlation_);
            correlation_ = std::make_shared<Dal::HybridConstantCorrelationData_>(originalCorrelation->Name(), originalCorrelation->factorNames_,
                                                                                 originalCorrelation->correlations_);
            auto components = original->components_;
            components[0] = Dal::Handle_<Dal::HybridComponentData_>(local_);
            model_ = std::make_shared<Dal::HybridModelData_>(original->Name(), original->domesticCurrency_, components,
                                                             Dal::Handle_<Dal::HybridCorrelationData_>(correlation_));
        }
    };

    struct MutateCaller_ final : Dal::Detail::FixingReadObserver_ {
        MutableHybrid_* caller_ = nullptr;
        Dal::Handle_<Dal::ScriptProductData_>* product_ = nullptr;
        size_t calls_ = 0;
        void BeforeHistory(const Dal::String_&) override {
            ++calls_;
            caller_->surface_->vols_(0, 0) *= 2.0;
            caller_->local_->spot_ = 500.0;
            caller_->model_->components_.clear();
            *product_ = Dal::NewScriptProduct("changed", {Dal::Cell_(Dal::Date_(2027, 9, 12))}, {"pay PAYS 1000"});
        }
    };
} // namespace

TEST(DupireRiskRequestTest, TestPlanUsesActualUnsortedModelOrdinalsAndFullCombinedPayload) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    const auto product = Product();
    const size_t surfaceNodes = static_cast<size_t>(calibration.Surface()->vols_.Rows()) * calibration.Surface()->vols_.Cols();
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 257;
    request.quotes_.inputs_ = Dal::Vector_<Dal::String_>{"quote:3", "quote:0"};
    request.quotes_.reportFactors_ = Dal::Vector_<>{0.01, 0.01};
    request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
    const auto plan = Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
    ASSERT_EQ(plan.Component(), "Z_LOCAL");
    ASSERT_EQ(plan.NumPaths(), 257);
    ASSERT_EQ(plan.CompleteInputAxis().size(), surfaceNodes + 7);
    ASSERT_EQ(plan.RequiredInputAxis().size(), surfaceNodes);
    for (size_t ordinal = 0; ordinal < surfaceNodes; ++ordinal) {
        const auto& coordinate = plan.RequiredInputAxis()[ordinal];
        ASSERT_EQ(coordinate.id_, "model:" + Dal::String_(std::to_string(6 + ordinal)));
        ASSERT_EQ(coordinate.ordinal_, 6 + ordinal);
        ASSERT_EQ(coordinate.value_, calibration.Surface()->vols_(static_cast<int>(ordinal / calibration.Surface()->vols_.Cols()),
                                                                  static_cast<int>(ordinal % calibration.Surface()->vols_.Cols())));
    }
    ASSERT_EQ(plan.QuotePlan().SelectedOrdinals(), (Dal::Vector_<size_t>{3, 0}));
    ASSERT_EQ(plan.QuotePlan().NumericPayloadBytes(), 144);
    ASSERT_EQ(plan.NumericPayloadBytes(), Dal::Script::RiskResultPayloadBytes(1, surfaceNodes) + 144);
    ASSERT_TRUE(plan.SimulationSettings().enableAad_);
    ASSERT_EQ(plan.ValuationSettings().evaluationDate_, request.valuation_.evaluationDate_);
    ASSERT_TRUE(plan.DirectBindings().empty());
}

TEST(DupireRiskRequestTest, TestDirectBindingExtendsRequiredAxisAndCombinedBudget) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    const auto product = Product();
    const size_t nodes = static_cast<size_t>(calibration.Surface()->vols_.Rows()) * calibration.Surface()->vols_.Cols();
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 257;
    request.quotes_.inputs_ = Dal::Vector_<Dal::String_>{};
    request.directBindings_ = {{0, "quote:3"}};
    request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
    request.quotes_.numericPayloadBudgetBytes_ = Dal::Script::RiskResultPayloadBytes(1, nodes + 1) + 144;
    const auto plan = Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
    ASSERT_EQ(plan.RequiredInputAxis().size(), nodes + 1);
    ASSERT_EQ(plan.RequiredInputAxis().back().id_, "constant:0");
    ASSERT_EQ(plan.RequiredInputAxis().back().value_, 0.0);
    ASSERT_EQ(plan.DirectBindings().size(), 1);
    ASSERT_EQ(plan.DirectBindings()[0].quoteId_, "quote:3");
    ASSERT_EQ(plan.NumericPayloadBytes(), *request.quotes_.numericPayloadBudgetBytes_);
    ASSERT_TRUE(plan.QuotePlan().InputAxis().empty());
    --*request.quotes_.numericPayloadBudgetBytes_;
    ASSERT_THROW(static_cast<void>(Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request)), Dal::Exception_);
    ASSERT_EQ(plan.RequiredInputAxis().back().id_, "constant:0");
}

TEST(DupireRiskRequestTest, TestExternalDirectAcceptsQuoteOnlyIdentityAndRejectsChangedQuotesOrDoubleBinding) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    const auto otherBase = Dal::CalibrateDupireWithRisk(FlatIVS_(0.25), calibration.Inputs(), "other_base");
    ASSERT_FALSE(calibration.Matches(otherBase));
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 257;
    request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
    request.direct_ = Dal::NewCalibrationDirectQuoteAdjoints(Dal::NewCalibrationPullback(otherBase), Dal::Matrix_<>(3, 2, -1.25));
    const auto plan = Dal::PlanDupireScriptRisk(Product(), Model(calibration), calibration, "Z_LOCAL", request);
    ASSERT_TRUE(plan.DirectBindings().empty());
    ASSERT_EQ(plan.RequiredInputAxis().size(), plan.CompleteInputAxis().size() - 7);
    request.directBindings_ = {{0, "quote:3"}};
    ASSERT_THROW(static_cast<void>(Dal::PlanDupireScriptRisk(Product(), Model(calibration), calibration, "Z_LOCAL", request)), Dal::Exception_);
    request.directBindings_.clear();
    auto changed = calibration.Inputs();
    changed.quoteSpreads_(1, 1) = 0.001;
    const auto otherQuotes = Dal::CalibrateDupireWithRisk(FlatIVS_(), changed, "changed_quotes");
    request.direct_ = Dal::NewCalibrationDirectQuoteAdjoints(Dal::NewCalibrationPullback(otherQuotes), Dal::Matrix_<>(3, 2, -1.25));
    ASSERT_THROW(static_cast<void>(Dal::PlanDupireScriptRisk(Product(), Model(calibration), calibration, "Z_LOCAL", request)), Dal::Exception_);
    ASSERT_TRUE(plan.DirectBindings().empty());
}

TEST(DupireRiskRequestTest, TestExecutionMatchesManualNativeChainAndDirectPartialWithoutAnotherMean) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    const auto product = Product();
    for (const bool compiled : {false, true}) {
        Dal::DupireScriptRiskRequest_ request;
        request.numPaths_ = 257;
        request.directBindings_ = {{0, "quote:3"}};
        request.quotes_.inputs_ = Dal::Vector_<Dal::String_>{"quote:3", "quote:0"};
        request.quotes_.reportFactors_ = Dal::Vector_<>{0.01, 0.5};
        request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
        request.simulation_.compiled_ = compiled;
        const auto manual = Dal::ValueByMonteCarloWithRisk(product, model, 257, {}, request.valuation_, request.simulation_);
        Dal::Matrix_<> direct(3, 2, 0.0);
        direct(1, 1) = manual.Jacobian()(0, manual.Jacobian().Cols() - 1);
        const auto native = Dal::PullbackDupireScriptRisk(manual, calibration, "Z_LOCAL", Dal::DupireDirectQuoteAdjoints_{calibration, direct});
        const auto plan = Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
        const auto result = Dal::ValueByMonteCarloWithDupireRisk(plan);
        ASSERT_EQ(result.Component(), "Z_LOCAL");
        ASSERT_EQ(result.Method(), "NativeAADThenNativeAADCalibrationVJP");
        ASSERT_EQ(result.Valuation().Provenance().calibration_, "fixed");
        ASSERT_EQ(result.NumericPayloadBytes(), plan.NumericPayloadBytes());
        ASSERT_DOUBLE_EQ(result.Valuation().Values()[0], manual.Values()[0]);
        ASSERT_EQ(result.Valuation().InputAxis().size(), plan.RequiredInputAxis().size());
        for (size_t column = 0; column < result.Valuation().InputAxis().size(); ++column) {
            const auto& coordinate = result.Valuation().InputAxis()[column];
            const size_t nativeColumn = coordinate.family_ == "model" ? coordinate.ordinal_ : manual.CompleteInputAxis().size() - 1;
            ASSERT_DOUBLE_EQ(result.Valuation().Jacobian()(0, static_cast<int>(column)), manual.Jacobian()(0, static_cast<int>(nativeColumn)));
        }
        const auto& risk = result.QuoteRisk().QuoteRisk();
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 2; ++column) {
                ASSERT_DOUBLE_EQ(risk.CalibrationAdjoints()(row, column), native.QuoteRisk().CalibrationAdjoints()(row, column));
                ASSERT_DOUBLE_EQ(risk.DirectAdjoints()(row, column), native.QuoteRisk().DirectAdjoints()(row, column));
                ASSERT_DOUBLE_EQ(risk.TotalAdjoints()(row, column), native.QuoteRisk().TotalAdjoints()(row, column));
            }
        ASSERT_NEAR(risk.DirectAdjoints()(1, 1), 3.0 * std::exp(-0.05), 1e-10);
        ASSERT_DOUBLE_EQ(result.QuoteRisk().ReportedJacobian()(0, 0), 0.01 * risk.TotalAdjoints()(1, 1));
        ASSERT_DOUBLE_EQ(result.QuoteRisk().ReportedJacobian()(0, 1), 0.5 * risk.TotalAdjoints()(0, 0));
    }
}

TEST(DupireRiskRequestTest, TestCustomNestedSurfaceRejectsBeforeArchiveCallback) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    int writes = 0;
    auto model = Model(calibration);
    const auto hybrid = Dal::handle_cast<Dal::HybridModelData_>(model);
    const auto originalLocal = Dal::handle_cast<Dal::HybridLocalVolEquityData_>(hybrid->components_[0]);
    const auto local = std::make_shared<Dal::HybridLocalVolEquityData_>(originalLocal->Name(), originalLocal->index_, originalLocal->currency_,
                                                                        originalLocal->factor_, originalLocal->spot_, originalLocal->div_,
                                                                        originalLocal->surface_, originalLocal->maxStep_);
    local->surface_ = Dal::Handle_<Dal::LocalVolSurfaceData_>(new CustomSurface_(*calibration.Surface(), &writes));
    const auto data = std::make_shared<Dal::HybridModelData_>(hybrid->Name(), hybrid->domesticCurrency_, hybrid->components_, hybrid->correlation_);
    data->components_[0] = Dal::Handle_<Dal::HybridComponentData_>(local);
    model = Dal::Handle_<Dal::ModelData_>(data);
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 257;
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { static_cast<void>(Dal::PlanDupireScriptRisk(Product(), model, calibration, "Z_LOCAL", request)); },
                                        "unsupported Hybrid surface type"));
    ASSERT_EQ(writes, 0);
}

TEST(DupireRiskRequestTest, TestCombinedExtentArithmeticRejectsOverflowWithoutAllocating) {
    ASSERT_EQ(Dal::Detail::DupireRiskPayloadBytes(18, 1, 3, 2), 304);
    const size_t maximum = (std::numeric_limits<size_t>::max)();
    const size_t matrixLimit = static_cast<size_t>((std::numeric_limits<int>::max)());
    ASSERT_THROW(static_cast<void>(Dal::Detail::DupireRiskPayloadBytes(0, 0, 3, 2)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::Detail::DupireRiskPayloadBytes(maximum, 1, 3, 2)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::Detail::DupireRiskPayloadBytes(18, maximum, 3, 2)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::Detail::DupireRiskPayloadBytes(matrixLimit, 1, 3, 2)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::Detail::DupireRiskPayloadBytes(18, 0, maximum, 2)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::Detail::DupireRiskPayloadBytes(18, 0, 3, 0)), Dal::Exception_);
    ASSERT_THROW(static_cast<void>(Dal::Detail::DupireRiskPayloadBytes(18, 0, 1, maximum / (3 * sizeof(double)))), Dal::Exception_);
}

TEST(DupireRiskRequestTest, TestInvalidBindingsAndFullBudgetsRejectBeforeHistoryOrWorkers) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    const auto product = Dal::NewScriptProduct("two_quotes", {Dal::Cell_("Q1"), Dal::Cell_("Q2"), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                                               {"0", "0", "pay PAYS FIX(EQ[LOCAL]) + Q1 + 2 * Q2"});
    Dal::Script::TestSupport::RejectFixingReads_ history;
    Dal::Script::TestSupport::RejectSubmissions_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&history);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 32;
    request.simulation_.compiled_ = true;
    request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
    auto plan = [&] { static_cast<void>(Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request)); };
    request.directBindings_ = {{99, "quote:0"}};
    ASSERT_NO_FATAL_FAILURE(AssertError(plan, "unknown direct constant ordinal=99"));
    request.directBindings_ = {{0, "quote:99"}};
    ASSERT_NO_FATAL_FAILURE(AssertError(plan, "unknown direct quote ID"));
    request.directBindings_ = {{0, "quote:0"}, {0, "quote:1"}};
    ASSERT_NO_FATAL_FAILURE(AssertError(plan, "repeated direct constant"));
    request.directBindings_ = {{0, "quote:0"}, {1, "QUOTE:0"}};
    ASSERT_NO_FATAL_FAILURE(AssertError(plan, "repeated direct quote"));
    request.directBindings_.clear();
    const size_t bytes = Dal::Detail::DupireRiskPayloadBytes(18, 0, 3, 2);
    for (const auto& inputs :
         {std::optional<Dal::Vector_<Dal::String_>>(), std::optional<Dal::Vector_<Dal::String_>>(Dal::Vector_<Dal::String_>{"quote:0"}),
          std::optional<Dal::Vector_<Dal::String_>>(Dal::Vector_<Dal::String_>{})}) {
        request.quotes_.inputs_ = inputs;
        request.quotes_.numericPayloadBudgetBytes_ = bytes - 1;
        ASSERT_NO_FATAL_FAILURE(AssertError(plan, "CalibrationRiskBudgetExceeded"));
        request.quotes_.numericPayloadBudgetBytes_ = bytes;
        ASSERT_EQ(Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request).NumericPayloadBytes(), bytes);
    }
    request.quotes_.numericPayloadBudgetBytes_.reset();
    request.quotes_.inputs_ = Dal::Vector_<Dal::String_>{"quote:99"};
    ASSERT_NO_FATAL_FAILURE(AssertError(plan, "unknown input"));
    request.quotes_.inputs_.reset();
    request.numPaths_ = 0;
    ASSERT_NO_FATAL_FAILURE(AssertError(plan, "numPaths"));
    request.numPaths_ = 32;
    request.simulation_.enableAad_ = false;
    ASSERT_NO_FATAL_FAILURE(AssertError(plan, "native AAD"));
    request.simulation_.enableAad_ = true;
    request.simulation_.rsg_ = "invalid_rng";
    ASSERT_NO_FATAL_FAILURE(AssertError(plan, "rsg"));
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
    ASSERT_EQ(workers.calls_, 0);
}

TEST(DupireRiskRequestTest, TestPlanSealsNestedDataAndSurvivesCallerMutationAndDestruction) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    MutableHybrid_ caller(calibration);
    auto model = Dal::Handle_<Dal::ModelData_>(caller.model_);
    const std::weak_ptr<const Dal::ModelData_> lifetime = model;
    auto product = Product();
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 257;
    request.directBindings_ = {{0, "quote:3"}};
    request.quotes_.inputs_ = Dal::Vector_<Dal::String_>{"quote:3", "quote:0"};
    request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
    const auto manual = Dal::ValueByMonteCarloWithRisk(product, model, 257, {}, request.valuation_, request.simulation_);
    Dal::Matrix_<> direct(3, 2, 0.0);
    direct(1, 1) = manual.Jacobian()(0, manual.Jacobian().Cols() - 1);
    const auto expected = Dal::PullbackDupireScriptRisk(manual, calibration, "Z_LOCAL", Dal::DupireDirectQuoteAdjoints_{calibration, direct});
    const auto plan = Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
    caller.surface_->vols_(0, 0) *= 2.0;
    caller.surface_->spots_[0] *= 2.0;
    caller.local_->spot_ = 500.0;
    caller.correlation_->correlations_(0, 1) = caller.correlation_->correlations_(1, 0) = 0.7;
    caller.correlation_->factorNames_.clear();
    caller.model_->components_.clear();
    caller.model_->domesticCurrency_ = "changed";
    caller.model_.reset();
    model = {};
    ASSERT_TRUE(lifetime.expired());
    product = Dal::NewScriptProduct("changed", {Dal::Cell_(Dal::Date_(2027, 9, 12))}, {"pay PAYS 1000"});
    request.numPaths_ = 0;
    request.directBindings_ = {{99, "unknown"}};
    request.quotes_.inputs_->clear();
    request.simulation_.enableAad_ = false;
    request.valuation_.evaluationDate_ = Dal::Date_(2028, 9, 12);
    const auto result = Dal::ValueByMonteCarloWithDupireRisk(plan);
    ASSERT_DOUBLE_EQ(result.Valuation().Values()[0], manual.Values()[0]);
    ASSERT_EQ(result.Valuation().Provenance().execution_->modelSnapshotJson_, manual.Provenance().execution_->modelSnapshotJson_);
    ASSERT_EQ(result.QuoteRisk().Plan().SelectedOrdinals(), (Dal::Vector_<size_t>{3, 0}));
    const auto& actual = result.QuoteRisk().QuoteRisk().TotalAdjoints();
    const auto& reference = expected.QuoteRisk().TotalAdjoints();
    ASSERT_TRUE(std::equal(actual.begin(), actual.end(), reference.begin()));
}

TEST(DupireRiskRequestTest, TestPassivePlanPreservesUnrelatedNativeRecording) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    const auto product = Product();
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 32;
    Dal::AAD::RecordingScope_ recording;
    Dal::AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Dal::AAD::Number_ output = input * input;
    recording.FinishRecording();
    const auto nodes = Dal::AAD::Tape()->nodes_.Size();
    const auto plan = Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), nodes);
    ASSERT_TRUE(plan.ValuationSettings().evaluationDate_);
    request.directBindings_ = {{99, "quote:0"}};
    ASSERT_THROW(static_cast<void>(Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request)), Dal::Exception_);
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), nodes);
    Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
    recording.Reverse();
    ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
}

TEST(DupireRiskRequestTest, TestExternalDirectIsOwnedAndAddedOnceAfterNativeCalibration) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    const auto otherBase = Dal::CalibrateDupireWithRisk(FlatIVS_(0.25), calibration.Inputs());
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 257;
    request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
    request.direct_ = Dal::NewCalibrationDirectQuoteAdjoints(Dal::NewCalibrationPullback(otherBase), Dal::Matrix_<>(3, 2, -1.25));
    const auto model = Model(calibration);
    const auto product = Product();
    const auto manual = Dal::ValueByMonteCarloWithRisk(product, model, 257, {}, request.valuation_, request.simulation_);
    const auto expected =
        Dal::PullbackDupireScriptRisk(manual, calibration, "Z_LOCAL", Dal::DupireDirectQuoteAdjoints_{otherBase, Dal::Matrix_<>(3, 2, -1.25)});
    const auto plan = Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
    request.direct_.reset();
    const auto result = Dal::ValueByMonteCarloWithDupireRisk(plan);
    const auto& actual = result.QuoteRisk().QuoteRisk();
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 2; ++column) {
            ASSERT_EQ(actual.DirectAdjoints()(row, column), -1.25);
            ASSERT_DOUBLE_EQ(actual.TotalAdjoints()(row, column), expected.QuoteRisk().TotalAdjoints()(row, column));
        }
    auto detached = result.QuoteRisk().Jacobian();
    detached(0, 0) = 1000.0;
    ASSERT_NE(result.QuoteRisk().Jacobian()(0, 0), 1000.0);
}

TEST(DupireRiskRequestTest, TestEmptyQuoteSelectionPreservesSmoothedNativeEstimator) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    const auto product =
        Dal::NewScriptProduct("smooth", {Dal::Cell_(Dal::Date_(2026, 9, 12))}, {"IF FIX(EQ[LOCAL]) > 100 THEN pay PAYS 1 ELSE pay PAYS 0 END"});
    for (const bool compiled : {false, true}) {
        Dal::DupireScriptRiskRequest_ request;
        request.numPaths_ = 32;
        request.quotes_.inputs_ = Dal::Vector_<Dal::String_>{};
        request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
        request.simulation_.compiled_ = compiled;
        const auto native = Dal::ValueByMonteCarloWithRisk(product, model, 32, {}, request.valuation_, request.simulation_);
        const auto result = Dal::ValueByMonteCarloWithDupireRisk(Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request));
        ASSERT_EQ(result.Valuation().Provenance().method_, "NativeAAD");
        ASSERT_DOUBLE_EQ(result.Valuation().Values()[0], native.Values()[0]);
        ASSERT_EQ(result.Valuation().Jacobian().Cols(), 18);
        ASSERT_EQ(result.QuoteRisk().Jacobian().Cols(), 0);
        ASSERT_EQ(result.NumericPayloadBytes(), 296);
        auto passive = request.simulation_;
        passive.enableAad_ = false;
        ASSERT_NE(result.Valuation().Values()[0], Dal::ValueByMonteCarlo(product, model, 32, request.valuation_, passive).at("PV"));
    }
}

TEST(DupireRiskRequestTest, TestExpiredAndRetrainedPolicyMethodsRemainExplicit) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 32;
    request.valuation_.evaluationDate_ = Dal::Date_(2028, 9, 12);
    const auto expired = Dal::ValueByMonteCarloWithDupireRisk(Dal::PlanDupireScriptRisk(Product(), model, calibration, "Z_LOCAL", request));
    ASSERT_EQ(expired.Method(), "ExpiredThenNativeAADCalibrationVJP");
    ASSERT_EQ(expired.Valuation().Values()[0], 0.0);
    for (const double seed : expired.QuoteRisk().QuoteRisk().TotalAdjoints())
        ASSERT_EQ(seed, 0.0);
    Dal::Script::ScriptProductSettings_ productSettings;
    productSettings.defaultIndex_ = "EQ[LOCAL]";
    const auto product = Dal::NewScriptProduct("bermudan", {Dal::Cell_(Dal::Date_(2027, 3, 12)), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                                               {"EXERCISE MAX(100 - FIX(EQ[LOCAL]), 0)", "EXERCISE MAX(100 - FIX(EQ[LOCAL]), 0)"}, productSettings);
    request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
    request.simulation_.lsmcPolicyRiskMode_ = "RetrainedBump";
    request.simulation_.lsmcTrainingPaths_ = 64;
    const auto manual = Dal::ValueByMonteCarloWithRisk(product, model, 32, {}, request.valuation_, request.simulation_);
    const auto result = Dal::ValueByMonteCarloWithDupireRisk(Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request));
    ASSERT_EQ(result.Method(), "NativeAADWithRetrainedPolicySecantThenNativeAADCalibrationVJP");
    ASSERT_DOUBLE_EQ(result.Valuation().Values()[0], manual.Values()[0]);
    ASSERT_EQ(result.Valuation().Provenance().calibration_, "fixed");
}

TEST(DupireRiskRequestTest, TestGlobalFixingsResolveAtExecutionAndCallerMutationCannotReachWorkers) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    MutableHybrid_ caller(calibration);
    auto model = Dal::Handle_<Dal::ModelData_>(caller.model_);
    auto product = Dal::NewScriptProduct("history", {Dal::Cell_(Dal::Date_(2026, 9, 11)), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                                         {"x = FIX(EQ[LOCAL])", "pay PAYS x + FIX(EQ[LOCAL])"});
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 32;
    request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
    Dal::Script::TestSupport::StoreScriptTestFixing("EQ[LOCAL]", 80.0);
    const auto plan = Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
    Dal::Script::TestSupport::StoreScriptTestFixing("EQ[LOCAL]", 90.0);
    const auto expected = Dal::ValueByMonteCarloWithRisk(product, model, 32, {}, request.valuation_, request.simulation_);
    MutateCaller_ observer;
    observer.caller_ = &caller;
    observer.product_ = &product;
    const auto actual = [&] {
        const Dal::Detail::ScopedFixingReadObserver_ observe(&observer);
        return Dal::ValueByMonteCarloWithDupireRisk(plan);
    }();
    ASSERT_GT(observer.calls_, 0);
    ASSERT_DOUBLE_EQ(actual.Valuation().Values()[0], expected.Values()[0]);
    ASSERT_EQ(actual.Valuation().Provenance().execution_->observations_[0].value_, std::optional<double>(90.0));
    ASSERT_EQ(actual.Valuation().Provenance().execution_->productEvents_[1], "pay PAYS x + FIX(EQ[LOCAL])");
}

TEST(DupireRiskRequestTest, TestTargetCarrySurfaceAndBindingValuesRejectDuringPassivePlanning) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    MutableHybrid_ caller(calibration);
    const auto model = Dal::Handle_<Dal::ModelData_>(caller.model_);
    const auto product = Product();
    Dal::Script::TestSupport::RejectFixingReads_ history;
    Dal::Script::TestSupport::RejectSubmissions_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&history);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 32;
    auto plan = [&](const Dal::String_& component) { static_cast<void>(Dal::PlanDupireScriptRisk(product, model, calibration, component, request)); };
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { plan("unknown"); }, "named local-vol equity"));
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { plan("A_OTHER"); }, "named local-vol equity"));
    caller.local_->spot_ = 101.0;
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { plan("Z_LOCAL"); }, "matching flat deterministic carry"));
    caller.local_->spot_ = calibration.Spot();
    caller.local_->div_ = 0.03;
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { plan("Z_LOCAL"); }, "matching flat deterministic carry"));
    caller.local_->div_ = calibration.DividendYield();
    caller.surface_->vols_(0, 0) *= 1.1;
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { plan("Z_LOCAL"); }, "component surface values changed"));
    caller.surface_->vols_(0, 0) = calibration.Surface()->vols_(0, 0);
    caller.surface_->spots_[0] += 0.01;
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { plan("Z_LOCAL"); }, "component surface grids changed"));
    caller.surface_->spots_[0] = calibration.Surface()->spots_[0];
    const auto changed =
        Dal::NewScriptProduct("different_quote", {Dal::Cell_("Q"), Dal::Cell_(Dal::Date_(2027, 9, 12))}, {"0.001", "pay PAYS FIX(EQ[LOCAL]) + Q"});
    request.directBindings_ = {{0, "quote:0"}};
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { static_cast<void>(Dal::PlanDupireScriptRisk(changed, model, calibration, "Z_LOCAL", request)); },
                                        "direct constant/quote values disagree"));
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
    ASSERT_EQ(workers.calls_, 0);
}

TEST(DupireRiskRequestTest, TestCustomModelAndNonflatCarryRejectBeforeArchiveOrWorkers) {
    Dal::RegisterAll_::Init();
    const auto calibration = Calibration();
    const auto native = Dal::handle_cast<Dal::HybridModelData_>(Model(calibration));
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 32;
    int writes = 0;
    const Dal::Handle_<Dal::ModelData_> custom(
        new CustomArchive_<Dal::HybridModelData_>(&writes, native->Name(), native->domesticCurrency_, native->components_, native->correlation_));
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { static_cast<void>(Dal::PlanDupireScriptRisk(Product(), custom, calibration, "Z_LOCAL", request)); },
                                        "exact native Hybrid"));
    auto components = native->components_;
    components[1] = Dal::Handle_<Dal::HybridComponentData_>(
        new Dal::HybridLogDfRateData_("00_RATE", "USD", Dal::Vector_<>{0.0, 1.0}, Dal::Vector_<>{0.0, -0.05}));
    const Dal::Handle_<Dal::ModelData_> nonflat(
        new Dal::HybridModelData_(native->Name(), native->domesticCurrency_, components, native->correlation_));
    ASSERT_NO_FATAL_FAILURE(AssertError([&] { static_cast<void>(Dal::PlanDupireScriptRisk(Product(), nonflat, calibration, "Z_LOCAL", request)); },
                                        "unsupported Hybrid component type"));
    ASSERT_EQ(writes, 0);
}

TEST(DupireRiskRequestTest, TestExplicitFixingsAndPassiveGettersPreserveSnapshotAndTape) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    const auto product = Dal::NewScriptProduct("history", {Dal::Cell_(Dal::Date_(2026, 9, 11)), Dal::Cell_(Dal::Date_(2027, 9, 12))},
                                               {"x = FIX(EQ[LOCAL])", "pay PAYS x + FIX(EQ[LOCAL])"});
    Dal::MarketFixingSnapshot_::values_t values = {{"EQ[LOCAL]", {{Dal::DateTime_(Dal::Date_(2026, 9, 11), 0.0), 80.0}}}};
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 32;
    request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
    request.valuation_.fixings_ = Dal::Handle_<Dal::MarketFixingSnapshot_>(new Dal::MarketFixingSnapshot_(values));
    const auto plan = Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
    values.begin()->second.begin()->second = 90.0;
    request.valuation_.fixings_ = {};
    Dal::Script::TestSupport::StoreScriptTestFixing("EQ[LOCAL]", 90.0);
    const auto result = Dal::ValueByMonteCarloWithDupireRisk(plan);
    ASSERT_EQ(result.Valuation().Provenance().execution_->observations_[0].value_, std::optional<double>(80.0));
    Dal::Script::TestSupport::RejectFixingReads_ history;
    Dal::Script::TestSupport::RejectSubmissions_ workers;
    const Dal::Detail::ScopedFixingReadObserver_ observeHistory(&history);
    const Dal::Script::Detail::ScopedSimulationObserver_ observeWorkers(&workers);
    Dal::AAD::RecordingScope_ recording;
    Dal::AAD::Number_ input;
    recording.RegisterInput(input, 2.0);
    recording.StartRecording();
    Dal::AAD::Number_ output = input * input;
    recording.FinishRecording();
    const auto nodes = Dal::AAD::Tape()->nodes_.Size();
    ASSERT_EQ(result.QuoteRisk().Jacobian().Cols(), 6);
    ASSERT_EQ(result.QuoteRisk().CalibrationJacobian().Cols(), 6);
    ASSERT_EQ(result.QuoteRisk().DirectJacobian().Cols(), 6);
    ASSERT_EQ(result.QuoteRisk().ReportedJacobian().Cols(), 6);
    ASSERT_EQ(result.Valuation().ReportedJacobian().Cols(), 18);
    ASSERT_EQ(result.Method(), "NativeAADThenNativeAADCalibrationVJP");
    ASSERT_EQ(Dal::AAD::Tape()->nodes_.Size(), nodes);
    Dal::AAD::NativeOperations_::AddSeed(output, 1.0);
    recording.Reverse();
    ASSERT_EQ(Dal::AAD::NativeOperations_::ReadAdjoint(input), 4.0);
    recording.Close();
    ASSERT_EQ(history.historyCalls_, 0);
    ASSERT_EQ(history.fixingCalls_, 0);
    ASSERT_EQ(workers.calls_, 0);
}

TEST(DupireRiskRequestTest, TestFailedReportPublicationPreservesEarlierResultAndAllowsRecovery) {
    Dal::RegisterAll_::Init();
    const SingleWorker_ worker;
    const auto calibration = Calibration();
    const auto model = Model(calibration);
    const auto product = Product();
    Dal::DupireScriptRiskRequest_ request;
    request.numPaths_ = 32;
    request.valuation_.evaluationDate_ = Dal::Date_(2026, 9, 12);
    request.quotes_.inputs_ = Dal::Vector_<Dal::String_>{"quote:0"};
    const auto good = Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
    const auto before = Dal::ValueByMonteCarloWithDupireRisk(good);
    request.direct_ = Dal::NewCalibrationDirectQuoteAdjoints(Dal::NewCalibrationPullback(calibration),
                                                             Dal::Matrix_<>(3, 2, (std::numeric_limits<double>::max)() / 2.0));
    request.quotes_.reportFactors_ = Dal::Vector_<>{4.0};
    const auto bad = Dal::PlanDupireScriptRisk(product, model, calibration, "Z_LOCAL", request);
    ASSERT_THROW(static_cast<void>(Dal::ValueByMonteCarloWithDupireRisk(bad)), Dal::Exception_);
    const auto after = Dal::ValueByMonteCarloWithDupireRisk(good);
    ASSERT_DOUBLE_EQ(after.Valuation().Values()[0], before.Valuation().Values()[0]);
    ASSERT_DOUBLE_EQ(after.QuoteRisk().Jacobian()(0, 0), before.QuoteRisk().Jacobian()(0, 0));
}
